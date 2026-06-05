/**
 * Chirp-Device firmware
 *
 * I2S pins (PCM5102A):
 *   BCLK → GPIO 38   WS → GPIO 39   DOUT → GPIO 40
 *
 * TEST_AUDIO true  → plays warning chirp + timing chirp every 5s, no ESP-NOW
 * TEST_AUDIO false → production mode, triggered by ESP-NOW from master
 */

#include <Arduino.h>
#include <MyDefines.h>
#include <esp_log.h>
#include "esp_now.h"
#include "WiFi.h"
#include <driver/i2s_std.h>
#include <math.h>

#define TEST_AUDIO true

#define I2S_BCK_PIN   38
#define I2S_WS_PIN    39
#define I2S_DATA_PIN  40
#define SAMPLE_RATE   44100
#define AMPLITUDE     20000
#define TABLE_SIZE    256
#define BUF_FRAMES    256

#define CHIRP_STEPS    8
#define CHIRP_STEP_MS  50
#define WARN_STEP_MS   200

static const int CHIRP_FREQS[CHIRP_STEPS] = {1000,1700,1200,2000,1500,1100,1800,1300};

static i2s_chan_handle_t s_tx = NULL;
static int16_t  s_sineTable[TABLE_SIZE];
static int16_t  s_buf[BUF_FRAMES * 2];
static uint32_t s_phase    = 0;
static uint32_t s_phaseInc = 0;

// Timer sync
static unsigned long long s_lastReceiveTime  = 0;
static long long          s_offsetSum        = 0;
static int                s_offsetCount      = 0;
static int                s_offsetMultiplier = 1;
static int                s_delayAverage     = 0;
static int                s_delayCounter     = 0;
static volatile long long s_timeOffset       = 0;
static volatile bool      s_timerSynced      = false;
static uint8_t            s_myAddress[6];
static uint8_t            s_hostAddress[6]   = {};
static bool               s_hostAddressLearned = false;
static QueueHandle_t      s_recvQueue;

static inline long long syncedTime() {
    return (long long)esp_timer_get_time() + s_timeOffset;
}
static inline unsigned long long timeDiffAbs(unsigned long long a, unsigned long long b) {
    return a > b ? a - b : b - a;
}

static uint32_t freqToPhaseInc(int hz) {
    return (uint32_t)((float)TABLE_SIZE * hz * 256.0f / SAMPLE_RATE);
}

// Write exactly durationMs of audio at the current s_phaseInc, streaming via loop-compatible writes
static void streamMs(int hz, int durationMs) {
    s_phaseInc = (hz > 0) ? freqToPhaseInc(hz) : 0;
    int totalFrames = SAMPLE_RATE * durationMs / 1000;
    int done = 0;
    while (done < totalFrames) {
        for (int i = 0; i < BUF_FRAMES; i++) {
            int16_t s = 0;
            if (done + i < totalFrames && s_phaseInc > 0)
                s = s_sineTable[(s_phase >> 8) & (TABLE_SIZE - 1)];
            s_phase += s_phaseInc;
            s_buf[i * 2]     = s;
            s_buf[i * 2 + 1] = s;
        }
        size_t written = 0;
        i2s_channel_write(s_tx, s_buf, sizeof(s_buf), &written, portMAX_DELAY);
        done += BUF_FRAMES;
    }
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

static void playWarnChirp() {
    for (int i = 0; i < CHIRP_STEPS; i++)
        streamMs(CHIRP_FREQS[i], WARN_STEP_MS);
    streamMs(0, 1000);
}

static void playChirp() {
    playWarnChirp();
    broadcastClapMsg(true);
    streamMs(0, 100);  // wait for receiver to get ready
    for (int i = 0; i < CHIRP_STEPS; i++)
        streamMs(CHIRP_FREQS[i], CHIRP_STEP_MS);
    streamMs(0, 20);
    broadcastClapMsg(false);
    ESP_LOGI("CHIRP", "Done");
}

static void i2sInit() {
    for (int i = 0; i < TABLE_SIZE; i++)
        s_sineTable[i] = (int16_t)(AMPLITUDE * sinf(2.0f * M_PI * i / TABLE_SIZE));

    i2s_chan_config_t ch = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ch.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&ch, &s_tx, NULL));

    i2s_std_config_t cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)I2S_BCK_PIN,
            .ws   = (gpio_num_t)I2S_WS_PIN,
            .dout = (gpio_num_t)I2S_DATA_PIN,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { false, false, false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));
}

static void addPeer(const uint8_t *addr);

// ESP-NOW
static void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
    ESP_LOGI("CHIRP", "ESP-NOW recv: type=%d len=%d from %02x:%02x:%02x:%02x:%02x:%02x",
             len > 0 ? data[0] : -1, len,
             info->src_addr[0], info->src_addr[1], info->src_addr[2],
             info->src_addr[3], info->src_addr[4], info->src_addr[5]);
    if (!s_hostAddressLearned) {
        memcpy(s_hostAddress, info->src_addr, 6);
        s_hostAddressLearned = true;
        addPeer(s_hostAddress);
        WiFi.macAddress(s_myAddress);
        ESP_LOGI("CHIRP", "Learned master, my MAC %02x:%02x:%02x:%02x:%02x:%02x",
                 s_myAddress[0], s_myAddress[1], s_myAddress[2],
                 s_myAddress[3], s_myAddress[4], s_myAddress[5]);
        message_data ann = {};
        ann.messageType = MSG_ADDRESS;
        memcpy(ann.targetAddress, broadcastAddress, 6);
        memcpy(ann.senderAddress, s_myAddress, 6);
        memcpy(ann.payload.address.address, s_myAddress, 6);
        ann.payload.address.version = VERSION;
        esp_now_send(broadcastAddress, (uint8_t *)&ann, sizeof(ann));
    }
    if (len < (int)sizeof(message_timer) + 1) return;
    message_data msg;
    memcpy(&msg, data, sizeof(msg));
    if (msg.messageType == MSG_TIMER)
        msg.payload.timer.receiveTime = (uint64_t)esp_timer_get_time();
    xQueueSend(s_recvQueue, &msg, 0);
}
static void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    ESP_LOGI("CHIRP", "Send %s to %02x:%02x:%02x:%02x:%02x:%02x",
             status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void handleTimer(const message_data &msg) {
    const message_timer &t = msg.payload.timer;
    unsigned long long timeDiff    = t.receiveTime - s_lastReceiveTime;
    unsigned long long timerFreqUs = TIMER_FREQUENCY * 1000ULL;
    if (s_lastReceiveTime == 0)
        ESP_LOGI("CHIRP", "First MSG_TIMER received");
    if (timeDiffAbs(timeDiff, timerFreqUs) >= 2500 || t.lastDelay >= 6000)
        ESP_LOGI("CHIRP", "MSG_TIMER rejected: timeDiff=%llu (exp ~%llu) lastDelay=%d", timeDiff, timerFreqUs, t.lastDelay);
    if (timeDiffAbs(timeDiff, timerFreqUs) < 2500 && t.lastDelay < 6000) {
        unsigned long long offset;
        if (t.receiveTime < t.sendTime) { offset = t.sendTime - t.receiveTime; s_offsetMultiplier = 1; }
        else                            { offset = t.receiveTime - t.sendTime; s_offsetMultiplier = -1; }
        s_offsetSum += (long long)offset;
        s_offsetCount++;
        s_timeOffset = s_offsetMultiplier * (s_offsetSum / s_offsetCount);
        if (s_delayCounter < TIMER_ARRAY_COUNT) {
            s_delayAverage = (s_delayAverage * s_delayCounter + (int)t.lastDelay) / (s_delayCounter + 1);
            s_delayCounter++;
        } else {
            s_timeOffset = s_offsetMultiplier * (s_offsetSum / s_offsetCount + (long long)s_delayAverage / 2);
            if (!s_timerSynced) {
                s_timerSynced = true;
                ESP_LOGI("CHIRP", "Timer synced — offset %lld µs, delay avg %d µs", s_timeOffset, s_delayAverage);
                // play confirmation tone: three short beeps
                for (int i = 0; i < 3; i++) {
                    streamMs(1000, 100);
                    streamMs(0, 100);
                }
                message_data gotTimer = {};
                gotTimer.messageType = MSG_GOT_TIMER;
                memcpy(gotTimer.targetAddress, s_hostAddress, 6);
                memcpy(gotTimer.senderAddress, s_myAddress, 6);
                gotTimer.payload.gotTimer.delayAverage      = s_delayAverage;
                gotTimer.payload.gotTimer.batteryPercentage = 0.0f;
                gotTimer.payload.gotTimer.offset            = s_timeOffset;
                gotTimer.payload.gotTimer.perceivedTime     = syncedTime();
                gotTimer.payload.gotTimer.sendTime          = (unsigned long long)esp_timer_get_time();
                esp_now_send(s_hostAddress, (uint8_t *)&gotTimer, sizeof(gotTimer));
            }
            s_offsetSum = s_offsetCount = s_delayAverage = s_delayCounter = 0;
        }
    }
    s_lastReceiveTime = t.receiveTime;
}

static void handleReceiveTask(void *) {
    message_data msg;
    while (true) {
        if (xQueueReceive(s_recvQueue, &msg, portMAX_DELAY) != pdTRUE) continue;
        if (msg.messageType == MSG_TIMER) { handleTimer(msg); continue; }
        if (msg.messageType == MSG_CLAP) {
            if (msg.payload.clap.clapHappened)
                ESP_LOGI("DIST", "%.0f cm", (float)msg.payload.clap.clapTime);
            else
                ESP_LOGI("DIST", "no detection");
            continue;
        }
        if (msg.messageType != MSG_COMMAND) continue;
        switch (msg.payload.command.commandType) {
            case CMD_START_DISTANCE_CALIBRATION:
            case CMD_CONTINUE_DISTANCE_CALIBRATION:
                playChirp();
                break;
            case CMD_CANCEL_CALIBRATION:
            case CMD_END_CALIBRATION:
                break;
            case CMD_REANNOUNCE: {
                message_data ann = {};
                ann.messageType = MSG_ADDRESS;
                memcpy(ann.targetAddress, broadcastAddress, 6);
                memcpy(ann.senderAddress, s_myAddress, 6);
                memcpy(ann.payload.address.address, s_myAddress, 6);
                ann.payload.address.version = VERSION;
                esp_now_send(broadcastAddress, (uint8_t *)&ann, sizeof(ann));
                ESP_LOGI("CHIRP", "Re-announced to broadcast");
                break;
            }
            default: break;
        }
    }
}

static void addPeer(const uint8_t *addr) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, addr, 6);
    peer.channel = 0; peer.encrypt = false;
    if (esp_now_get_peer(peer.peer_addr, &peer) != ESP_OK)
        esp_now_add_peer(&peer);
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    i2sInit();

    WiFi.mode(WIFI_STA);
    s_recvQueue = xQueueCreate(8, sizeof(message_data));
    ESP_ERROR_CHECK(esp_now_init());
    WiFi.macAddress(s_myAddress);
    esp_now_register_recv_cb(onDataRecv);
    esp_now_register_send_cb(onDataSent);
    addPeer(broadcastAddress);
    xTaskCreate(handleReceiveTask, "handleRecv", 4096, nullptr, 5, nullptr);

    xTaskCreate([](void*) {
        vTaskDelay(pdMS_TO_TICKS(500));
        WiFi.macAddress(s_myAddress);
        message_data ann = {};
        ann.messageType = MSG_ADDRESS;
        memcpy(ann.targetAddress, broadcastAddress, 6);
        memcpy(ann.senderAddress, s_myAddress, 6);
        memcpy(ann.payload.address.address, s_myAddress, 6);
        ann.payload.address.version = VERSION;
        esp_now_send(broadcastAddress, (uint8_t *)&ann, sizeof(ann));
        ESP_LOGI("CHIRP", "Announced — MAC %02x:%02x:%02x:%02x:%02x:%02x",
                 s_myAddress[0], s_myAddress[1], s_myAddress[2],
                 s_myAddress[3], s_myAddress[4], s_myAddress[5]);
        vTaskDelete(nullptr);
    }, "announce", 4096, nullptr, 5, nullptr);
}

void loop() {
#if TEST_AUDIO
    if (!s_timerSynced) {
        streamMs(0, 1000);
        return;
    }
    static int chirpCount = 0;
    ESP_LOGI("CHIRP", "Playing chirp #%d — offset=%lld µs", ++chirpCount, s_timeOffset);
    playChirp();
    ESP_LOGI("CHIRP", "Chirp #%d done", chirpCount);
    streamMs(0, 20000);
#else
    // production: all work happens in handleReceiveTask
    vTaskDelay(pdMS_TO_TICKS(10000));
#endif
}
