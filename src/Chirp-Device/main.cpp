/**
 * Chirp-Device firmware
 *
 * Drop-in replacement for Clap-Device for distance calibration.
 * On receiving CMD_START_DISTANCE_CALIBRATION from the master:
 *   1. Broadcasts MSG_CLAP(true) as the timing reference.
 *   2. Plays the chirp via I2S → PCM5102A → boombox line-in.
 *   3. Broadcasts MSG_CLAP(false) when done.
 *
 * The master compensates for the fixed I2S startup latency via
 * clapDeviceDelay, measured automatically by runClapSync.
 *
 * I2S pins (PCM5102A):
 *   BCLK → GPIO 6   WS → GPIO 7   DOUT → GPIO 16
 */

#include <Arduino.h>
#include <MyDefines.h>
#include <esp_log.h>
#include "esp_now.h"
#include "WiFi.h"
#include "driver/i2s_std.h"
#include <math.h>

// ---------------------------------------------------------------------------
// Test mode — plays chirp every 5 seconds, no ESP-NOW needed
// ---------------------------------------------------------------------------
#define TEST_AUDIO true

// ---------------------------------------------------------------------------
// I2S config
// ---------------------------------------------------------------------------
#define I2S_SAMPLE_RATE  44100
#define I2S_BCLK_PIN     GPIO_NUM_4
#define I2S_WS_PIN       GPIO_NUM_5
#define I2S_DOUT_PIN     GPIO_NUM_17

// ---------------------------------------------------------------------------
// Chirp config — 8 frequency steps × 3 ms, 1–2 kHz
// ---------------------------------------------------------------------------
#define CHIRP_STEPS       8
#define CHIRP_STEP_MS     3
#define SAMPLES_PER_STEP  (I2S_SAMPLE_RATE * CHIRP_STEP_MS / 1000)   // 132
#define CHIRP_SAMPLES     (CHIRP_STEPS * SAMPLES_PER_STEP)            // 1056
#define CHIRP_FRAMES      (CHIRP_SAMPLES * 2)                         // stereo
#define CHIRP_AMPLITUDE   32000

static const int CHIRP_FREQS[CHIRP_STEPS] = {1000, 1700, 1200, 2000, 1500, 1100, 1800, 1300};

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static i2s_chan_handle_t s_txChan = nullptr;
static int16_t           s_chirp[CHIRP_FRAMES];

static QueueHandle_t s_recvQueue;
static uint8_t       s_myAddress[6];
static uint8_t       s_hostAddress[6] = {0x34, 0x85, 0x18, 0x8f, 0xbf, 0xb8};

static TaskHandle_t s_chirpTaskHandle = nullptr;

// ---------------------------------------------------------------------------
// Timer sync — same algorithm as client handleTimer
// ---------------------------------------------------------------------------
static unsigned long long s_lastReceiveTime  = 0;
static long long          s_offsetSum        = 0;
static int                s_offsetCount      = 0;
static int                s_offsetMultiplier = 1;
static int                s_delayAverage     = 0;
static int                s_delayCounter     = 0;
static volatile long long s_timeOffset       = 0;
static volatile bool      s_timerSynced      = false;

static inline unsigned long long s_timeDiffAbs(unsigned long long a, unsigned long long b) {
    return a > b ? a - b : b - a;
}

static inline long long syncedTime() {
    return (long long)esp_timer_get_time() + s_timeOffset;
}

static void handleTimer(const message_data &msg) {
    const message_timer &t = msg.payload.timer;
    unsigned long long timeDiff    = t.receiveTime - s_lastReceiveTime;
    unsigned long long timerFreqUs = TIMER_FREQUENCY * 1000ULL;

    if (s_timeDiffAbs(timeDiff, timerFreqUs) < 2500 && t.lastDelay < 6000) {
        unsigned long long offset;
        if (t.receiveTime < t.sendTime) {
            offset             = t.sendTime - t.receiveTime;
            s_offsetMultiplier = 1;
        } else {
            offset             = t.receiveTime - t.sendTime;
            s_offsetMultiplier = -1;
        }
        s_offsetSum += (long long)offset;
        s_offsetCount++;
        s_timeOffset = s_offsetMultiplier * (s_offsetSum / s_offsetCount);

        if (s_delayCounter < TIMER_ARRAY_COUNT) {
            s_delayAverage = (s_delayAverage * s_delayCounter + (int)t.lastDelay)
                             / (s_delayCounter + 1);
            s_delayCounter++;
        } else {
            s_timeOffset = s_offsetMultiplier *
                (s_offsetSum / s_offsetCount + (long long)s_delayAverage / 2);
            if (!s_timerSynced) {
                s_timerSynced = true;
                ESP_LOGI("CHIRP", "Timer synced — offset %lld µs, delay avg %d µs",
                         s_timeOffset, s_delayAverage);

                message_data gotTimer = {};
                gotTimer.messageType = MSG_GOT_TIMER;
                memcpy(gotTimer.targetAddress, s_hostAddress, 6);
                memcpy(gotTimer.senderAddress, s_myAddress, 6);
                gotTimer.payload.gotTimer.delayAverage    = s_delayAverage;
                gotTimer.payload.gotTimer.batteryPercentage = 0.0f;
                gotTimer.payload.gotTimer.offset          = s_timeOffset;
                gotTimer.payload.gotTimer.perceivedTime   = syncedTime();
                gotTimer.payload.gotTimer.sendTime        = (unsigned long long)esp_timer_get_time();
                esp_now_send(s_hostAddress, (uint8_t *)&gotTimer, sizeof(gotTimer));
            }
            s_offsetSum    = 0;
            s_offsetCount  = 0;
            s_delayAverage = 0;
            s_delayCounter = 0;
        }
    }
    s_lastReceiveTime = t.receiveTime;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void buildChirp() {
    int out = 0;
    for (int step = 0; step < CHIRP_STEPS; step++) {
        float freq = (float)CHIRP_FREQS[step];
        for (int s = 0; s < SAMPLES_PER_STEP; s++) {
            int totalSample = step * SAMPLES_PER_STEP + s;
            int16_t val = (int16_t)(CHIRP_AMPLITUDE *
                sinf(2.0f * M_PI * freq * totalSample / I2S_SAMPLE_RATE));
            s_chirp[out++] = val; // L
            s_chirp[out++] = val; // R
        }
    }
}

static void i2sInit() {
    i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chanCfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chanCfg, &s_txChan, nullptr));

    i2s_std_config_t stdCfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                         I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_PIN,
            .ws   = I2S_WS_PIN,
            .dout = I2S_DOUT_PIN,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { false, false, false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_txChan, &stdCfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_txChan));
}

static void broadcastClapMsg(bool happened) {
    message_data msg = {};
    msg.messageType               = MSG_CLAP;
    memcpy(msg.targetAddress, broadcastAddress, 6);
    memcpy(msg.senderAddress, s_myAddress, 6);
    msg.payload.clap.clapTime     = (unsigned long long)syncedTime();
    msg.payload.clap.clapHappened = happened;
    esp_now_send(broadcastAddress, (uint8_t *)&msg, sizeof(msg));
}

static void playChirp() {
    size_t written = 0;
    broadcastClapMsg(true);
    i2s_channel_write(s_txChan, s_chirp, sizeof(s_chirp), &written, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(CHIRP_STEPS * CHIRP_STEP_MS + 20));
    broadcastClapMsg(false);
    ESP_LOGI("CHIRP", "Done");
}

// ---------------------------------------------------------------------------
// ESP-NOW
// ---------------------------------------------------------------------------
static void IRAM_ATTR onDataRecv(const esp_now_recv_info *,
                                  const uint8_t *data, int len) {
    if (len != sizeof(message_data)) return;
    message_data msg;
    memcpy(&msg, data, sizeof(msg));
    if (msg.messageType == MSG_TIMER)
        msg.payload.timer.receiveTime = (uint64_t)esp_timer_get_time();
    xQueueSendFromISR(s_recvQueue, &msg, nullptr);
}

static void onDataSent(const uint8_t *, esp_now_send_status_t) {}

// ---------------------------------------------------------------------------
// Tasks
// ---------------------------------------------------------------------------
static void chirpTask(void *) {
    playChirp();
    s_chirpTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

static void handleReceiveTask(void *) {
    message_data msg;
    while (true) {
        if (xQueueReceive(s_recvQueue, &msg, portMAX_DELAY) != pdTRUE) continue;

        if (msg.messageType == MSG_TIMER) {
            handleTimer(msg);
            continue;
        }

        if (msg.messageType != MSG_COMMAND) continue;
        switch (msg.payload.command.commandType) {
            case CMD_START_DISTANCE_CALIBRATION:
            case CMD_CONTINUE_DISTANCE_CALIBRATION:
                if (s_chirpTaskHandle == nullptr) {
                    ESP_LOGI("CHIRP", "Trigger received");
                    xTaskCreatePinnedToCore(chirpTask, "chirpTask", 8192,
                                            nullptr, 10, &s_chirpTaskHandle, 1);
                }
                break;
            case CMD_CANCEL_CALIBRATION:
            case CMD_END_CALIBRATION:
                if (s_chirpTaskHandle != nullptr) {
                    vTaskDelete(s_chirpTaskHandle);
                    s_chirpTaskHandle = nullptr;
                }
                break;
            default:
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// Setup / loop
// ---------------------------------------------------------------------------
static void addPeer(const uint8_t *addr) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, addr, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_get_peer(peer.peer_addr, &peer) != ESP_OK)
        esp_now_add_peer(&peer);
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.macAddress(s_myAddress);

    buildChirp();
    i2sInit();

    s_recvQueue = xQueueCreate(8, sizeof(message_data));

    ESP_ERROR_CHECK(esp_now_init());
    esp_now_register_recv_cb(onDataRecv);
    esp_now_register_send_cb(onDataSent);
    addPeer(s_hostAddress);
    addPeer(broadcastAddress);

    xTaskCreate(handleReceiveTask, "handleRecv", 4096, nullptr, 5, nullptr);

    // Announce to master
    message_data ann = {};
    ann.messageType = MSG_ADDRESS;
    memcpy(ann.targetAddress, s_hostAddress, 6);
    memcpy(ann.senderAddress, s_myAddress, 6);
    memcpy(ann.payload.address.address, s_myAddress, 6);
    ann.payload.address.version = VERSION;
    delay(500);
    esp_now_send(s_hostAddress, (uint8_t *)&ann, sizeof(ann));

    ESP_LOGI("CHIRP", "Ready — MAC %02x:%02x:%02x:%02x:%02x:%02x",
             s_myAddress[0], s_myAddress[1], s_myAddress[2],
             s_myAddress[3], s_myAddress[4], s_myAddress[5]);
}

void loop() {
#if TEST_AUDIO
    // 1 kHz sine, 512 frames — fills DMA continuously with no gap
    static int16_t sineBuf[512 * 2];
    static bool built = false;
    if (!built) {
        for (int i = 0; i < 512; i++) {
            int16_t v = (int16_t)(CHIRP_AMPLITUDE * sinf(2.0f * M_PI * 1000.0f * i / I2S_SAMPLE_RATE));
            sineBuf[i * 2]     = v;
            sineBuf[i * 2 + 1] = v;
        }
        built = true;
        ESP_LOGI("CHIRP", "sine[0]=%d [11]=%d [22]=%d [44]=%d",
                 sineBuf[0], sineBuf[22], sineBuf[44], sineBuf[88]);
    }
    size_t written = 0;
    i2s_channel_write(s_txChan, sineBuf, sizeof(sineBuf), &written, pdMS_TO_TICKS(100));
    static uint32_t lastBeat = 0;
    uint32_t now = millis();
    if (now - lastBeat >= 20000) {
        ESP_LOGI("CHIRP", "heartbeat — written=%d", (int)written);
        lastBeat = now;
    }
#else
    vTaskDelay(pdMS_TO_TICKS(10000));
#endif
}
