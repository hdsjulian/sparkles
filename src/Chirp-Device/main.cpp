// Chirp-Device: drop-in replacement for the Clap-Device for distance calibration.
// On CMD_START/CONTINUE_DISTANCE_CALIBRATION it broadcasts MSG_CLAP(true) with the
// synced emission timestamp, plays the chirp via I2S -> PCM5102A -> boombox line-in,
// then broadcasts MSG_CLAP(false).
// I2S pins (PCM5102A): BCLK GPIO6, WS GPIO7, DOUT GPIO16

#include <Arduino.h>
#include <MyDefines.h>
#include <esp_log.h>
#include "esp_now.h"
#include "WiFi.h"
#include "driver/i2s_std.h"
#include <math.h>

#define I2S_SAMPLE_RATE  44100
#define I2S_BCLK_PIN     GPIO_NUM_6
#define I2S_WS_PIN       GPIO_NUM_7
#define I2S_DOUT_PIN     GPIO_NUM_16

#define SAMPLES_PER_STEP  (I2S_SAMPLE_RATE * CHIRP_STEP_MS / 1000)   // 132
#define CHIRP_SAMPLES     (CHIRP_STEPS * SAMPLES_PER_STEP)            // 1056
#define CHIRP_FRAMES      (CHIRP_SAMPLES * 2)                         // stereo
#define CHIRP_AMPLITUDE   26000

i2s_chan_handle_t txChan = nullptr;
int16_t chirpBuffer[CHIRP_FRAMES];

QueueHandle_t recvQueue;
uint8_t myAddress[6];
uint8_t hostAddress[6] = {0x34, 0x85, 0x18, 0x8f, 0xbf, 0xb8}; // fallback, learned from first MSG_TIMER
TaskHandle_t chirpTaskHandle = nullptr;
volatile bool engaged = false; // master has contacted us, stop announcing
bool hostLearned = false;

void addPeer(const uint8_t *addr);

// Timer sync, same algorithm as the client handleTimer: each packet carries the
// measured TX latency of the previous one (paired by counter), the burst's median wins.
unsigned long long lastReceiveTime = 0;
int delayAverage = 0;
int delayCounter = 0;
volatile long long timeOffset = 0;
volatile bool timerSynced = false;
long long pendingRaw = 0;
uint16_t pendingCounter = 0;
bool pendingValid = false;
long long samples[TIMER_ARRAY_COUNT];
int sampleCount = 0;

long long syncedTime() {
    return (long long)esp_timer_get_time() + timeOffset;
}

void handleTimer(const message_data &msg) {
    const message_timer &timerMessage = msg.payload.timer;

    unsigned long long sinceLast = timerMessage.receiveTime - lastReceiveTime;
    if (timerMessage.reset || sinceLast > 1000000ULL) {
        // new burst, drop stale state
        sampleCount = 0;
        pendingValid = false;
        delayCounter = 0;
        delayAverage = 0;
    }

    if (pendingValid && timerMessage.counter == (uint16_t)(pendingCounter + 1)
        && timerMessage.lastDelay > 0 && timerMessage.lastDelay < 6000) {
        if (sampleCount < TIMER_ARRAY_COUNT) {
            samples[sampleCount++] = pendingRaw + (long long)timerMessage.lastDelay;
        }
        delayAverage = (delayAverage * delayCounter + timerMessage.lastDelay) / (delayCounter + 1);
        delayCounter++;
    }
    pendingRaw = (long long)timerMessage.sendTime - (long long)timerMessage.receiveTime;
    pendingCounter = timerMessage.counter;
    pendingValid = true;
    lastReceiveTime = timerMessage.receiveTime;

    if (sampleCount >= TIMER_ARRAY_COUNT) {
        long long sorted[TIMER_ARRAY_COUNT];
        memcpy(sorted, samples, sizeof(sorted));
        for (int i = 1; i < TIMER_ARRAY_COUNT; i++) {
            long long v = sorted[i];
            int j = i - 1;
            while (j >= 0 && sorted[j] > v) { sorted[j + 1] = sorted[j]; j--; }
            sorted[j + 1] = v;
        }
        timeOffset = (sorted[TIMER_ARRAY_COUNT / 2 - 1] + sorted[TIMER_ARRAY_COUNT / 2]) / 2;
        ESP_LOGI("CHIRP", "Timer synced. Offset %lld, delay average %d", (long long)timeOffset, delayAverage);

        if (!timerSynced) {
            timerSynced = true;
            message_data gotTimerMessage;
            gotTimerMessage.messageType = MSG_GOT_TIMER;
            memcpy(gotTimerMessage.targetAddress, hostAddress, 6);
            memcpy(gotTimerMessage.senderAddress, myAddress, 6);
            gotTimerMessage.payload.gotTimer.delayAverage = delayAverage;
            gotTimerMessage.payload.gotTimer.batteryPercentage = 0.0f;
            gotTimerMessage.payload.gotTimer.offset = timeOffset;
            gotTimerMessage.payload.gotTimer.perceivedTime = syncedTime();
            gotTimerMessage.payload.gotTimer.sendTime = (unsigned long long)esp_timer_get_time();
            esp_now_send(hostAddress, (uint8_t *)&gotTimerMessage, sizeof(gotTimerMessage));
        }
        sampleCount = 0;
        pendingValid = false;
        delayCounter = 0;
        delayAverage = 0;
    }
}

void buildChirp() {
    int out = 0;
    for (int step = 0; step < CHIRP_STEPS; step++) {
        float freq = (float)chirpFrequencies[step];
        for (int s = 0; s < SAMPLES_PER_STEP; s++) {
            int totalSample = step * SAMPLES_PER_STEP + s;
            int16_t val = (int16_t)(CHIRP_AMPLITUDE * sinf(2.0f * M_PI * freq * totalSample / I2S_SAMPLE_RATE));
            chirpBuffer[out++] = val; // L
            chirpBuffer[out++] = val; // R
        }
    }
}

void i2sInit() {
    i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chanCfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chanCfg, &txChan, nullptr));

    i2s_std_config_t stdCfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_PIN,
            .ws   = I2S_WS_PIN,
            .dout = I2S_DOUT_PIN,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { false, false, false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(txChan, &stdCfg));
    ESP_ERROR_CHECK(i2s_channel_enable(txChan));
}

void broadcastClapMessage(bool happened) {
    message_data clapMessage;
    clapMessage.messageType = MSG_CLAP;
    memcpy(clapMessage.targetAddress, broadcastAddress, 6);
    memcpy(clapMessage.senderAddress, myAddress, 6);
    clapMessage.payload.clap.clapTime = (unsigned long long)syncedTime();
    clapMessage.payload.clap.clapHappened = happened;
    esp_now_send(broadcastAddress, (uint8_t *)&clapMessage, sizeof(clapMessage));
}

void playChirp() {
    size_t written = 0;
    broadcastClapMessage(true);
    i2s_channel_write(txChan, chirpBuffer, sizeof(chirpBuffer), &written, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(CHIRP_STEPS * CHIRP_STEP_MS + 20));
    broadcastClapMessage(false);
    ESP_LOGI("CHIRP", "Done");
}

// Map the 32-bit hardware MAC-time RX stamp into the esp_timer domain. The smallest
// observed span over a burst is the clock-domain offset, jitter only ever adds delay.
unsigned long long timerRxTimestamp(const esp_now_recv_info *info, unsigned long long cbTime) {
    if (info == nullptr || info->rx_ctrl == nullptr) return cbTime;
    int32_t span = (int32_t)((uint32_t)cbTime - info->rx_ctrl->timestamp);
    static int32_t minSpan = INT32_MAX;
    static unsigned long long lastCbTime = 0;
    if (cbTime - lastCbTime > 1000000ULL) minSpan = INT32_MAX; // stale after a 1 s gap
    lastCbTime = cbTime;
    if (span < minSpan) minSpan = span;
    return cbTime - (unsigned long long)(span - minSpan);
}

void IRAM_ATTR onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
    unsigned long long cbTime = esp_timer_get_time();
    // Master sends 80-byte ESPNOW_CLIENT_COMPAT_SIZE frames, accept anything up to full size
    if (len < 1 || len > (int)sizeof(message_data)) return;
    message_data msg;
    memcpy(&msg, data, len);
    // timer bursts bypass the master's send queue and leave senderAddress empty
    if (info != nullptr) {
        memcpy(msg.senderAddress, info->src_addr, 6);
    }
    if (msg.messageType == MSG_TIMER) {
        msg.payload.timer.receiveTime = timerRxTimestamp(info, cbTime);
    }
    xQueueSendFromISR(recvQueue, &msg, nullptr);
}

void onDataSent(const uint8_t *, esp_now_send_status_t) {}

void chirpTask(void *) {
    playChirp();
    chirpTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

void handleReceiveTask(void *) {
    message_data msg;
    while (true) {
        if (xQueueReceive(recvQueue, &msg, portMAX_DELAY) != pdTRUE) continue;

        if (msg.messageType == MSG_TIMER) {
            engaged = true;
            if (!hostLearned) {
                memcpy(hostAddress, msg.senderAddress, 6);
                hostLearned = true;
                addPeer(hostAddress);
                ESP_LOGI("CHIRP", "Learned master %02x:%02x:%02x:%02x:%02x:%02x",
                         hostAddress[0], hostAddress[1], hostAddress[2],
                         hostAddress[3], hostAddress[4], hostAddress[5]);
            }
            handleTimer(msg);
            continue;
        }

        if (msg.messageType != MSG_COMMAND) continue;
        engaged = true;
        switch (msg.payload.command.commandType) {
            case CMD_START_DISTANCE_CALIBRATION:
            case CMD_CONTINUE_DISTANCE_CALIBRATION:
                if (chirpTaskHandle == nullptr) {
                    xTaskCreatePinnedToCore(chirpTask, "chirpTask", 8192, nullptr, 10, &chirpTaskHandle, 1);
                }
                break;
            case CMD_CANCEL_CALIBRATION:
            case CMD_END_CALIBRATION:
                if (chirpTaskHandle != nullptr) {
                    vTaskDelete(chirpTaskHandle);
                    chirpTaskHandle = nullptr;
                }
                break;
            default:
                break;
        }
    }
}

void addPeer(const uint8_t *addr) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, addr, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_get_peer(peerInfo.peer_addr, &peerInfo) != ESP_OK) {
        esp_now_add_peer(&peerInfo);
    }
}

// Broadcast so any master finds us, repeated from loop() until the master engages us
void announceAddress() {
    message_data addressMessage;
    addressMessage.messageType = MSG_SOUND_DEVICE;
    memcpy(addressMessage.targetAddress, broadcastAddress, 6);
    memcpy(addressMessage.senderAddress, myAddress, 6);
    memcpy(addressMessage.payload.address.address, myAddress, 6);
    addressMessage.payload.address.version = VERSION;
    esp_now_send(broadcastAddress, (uint8_t *)&addressMessage, sizeof(addressMessage));
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // modem sleep adds RX latency and skews hardware RX timestamps
    WiFi.macAddress(myAddress);

    buildChirp();
    i2sInit();

    recvQueue = xQueueCreate(8, sizeof(message_data));

    ESP_ERROR_CHECK(esp_now_init());
    esp_now_register_recv_cb(onDataRecv);
    esp_now_register_send_cb(onDataSent);
    addPeer(hostAddress);
    addPeer(broadcastAddress);

    xTaskCreate(handleReceiveTask, "handleRecv", 4096, nullptr, 5, nullptr);

    delay(500);
    announceAddress();

    ESP_LOGI("CHIRP", "Ready. My address %02x:%02x:%02x:%02x:%02x:%02x",
             myAddress[0], myAddress[1], myAddress[2], myAddress[3], myAddress[4], myAddress[5]);
}

void loop() {
    if (!engaged) {
        announceAddress();
        vTaskDelay(pdMS_TO_TICKS(2000));
    } else {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
