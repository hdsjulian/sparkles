/**
 * ChirpTest-Device
 *
 * Syncs clock with master (same MSG_TIMER algorithm as client devices).
 * On MSG_CLAP(true) from Chirp-Device: records mic, cross-correlates
 * against chirp template, computes distance using synced timestamps.
 *
 * Distance = (detection_time_synced - clapTime - I2S_LATENCY_US) * SOUND_SPEED
 */

#include <Arduino.h>
#include <esp_log.h>
#include "esp_now.h"
#include "WiFi.h"
#include <MyDefines.h>
#include <math.h>

#define MIC_PIN           AUDIO_PIN
#define CHIRP_SAMPLE_RATE 10000
#define CHIRP_STEP_MS     50
#define CHIRP_STEPS       8
#define SAMPLES_PER_STEP  (CHIRP_SAMPLE_RATE * CHIRP_STEP_MS / 1000)
#define CHIRP_SAMPLES     (CHIRP_STEPS * SAMPLES_PER_STEP)
#define RECORD_MS         2000
#define RECORD_SAMPLES    (CHIRP_SAMPLE_RATE * RECORD_MS / 1000)
#define SOUND_SPEED       343.0f
#define MIN_CORR          0.2f

// I2S DMA latency on the Chirp-Device between broadcastClapMsg and actual audio output.
// Calibrate: set to 0, measure at known distance, adjust until correct.
#define I2S_LATENCY_US    12000LL   // ~12ms, tune this

static const int CHIRP_FREQS[CHIRP_STEPS] = {1000,1700,1200,2000,1500,1100,1800,1300};

static int16_t s_recBuf[RECORD_SAMPLES];
static float   s_tmpl[CHIRP_SAMPLES];
static float   s_tmplEnergy = 0;

// Timer sync state (same algorithm as client device)
static long long          s_timeOffset       = 0;
static long long          s_offsetSum        = 0;
static int                s_offsetCount      = 0;
static int                s_offsetMultiplier = 1;
static int                s_delayAverage     = 0;
static int                s_delayCounter     = 0;
static unsigned long long s_lastReceiveTime  = 0;
static volatile bool      s_timerSynced      = false;
static uint8_t            s_myAddress[6];
static uint8_t            s_hostAddress[6]   = {};
static bool               s_hostAddressLearned = false;

static inline long long syncedTime() {
    return (long long)esp_timer_get_time() + s_timeOffset;
}
static inline unsigned long long timeDiffAbs(unsigned long long a, unsigned long long b) {
    return a > b ? a - b : b - a;
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
                for (int i = 0; i < 3; i++) {
                    digitalWrite(LEDPINBLUE1, HIGH);
                    vTaskDelay(pdMS_TO_TICKS(300));
                    digitalWrite(LEDPINBLUE1, LOW);
                    vTaskDelay(pdMS_TO_TICKS(300));
                }
                message_data gotTimer = {};
                gotTimer.messageType = MSG_GOT_TIMER;
                memcpy(gotTimer.targetAddress, s_hostAddress, 6);
                memcpy(gotTimer.senderAddress, s_myAddress, 6);
                gotTimer.payload.gotTimer.delayAverage      = s_delayAverage;
                gotTimer.payload.gotTimer.batteryPercentage = 0.0f;
                gotTimer.payload.gotTimer.offset            = s_timeOffset;
                gotTimer.payload.gotTimer.perceivedTime     = (unsigned long long)syncedTime();
                gotTimer.payload.gotTimer.sendTime          = (unsigned long long)esp_timer_get_time();
                esp_now_send(s_hostAddress, (uint8_t *)&gotTimer, sizeof(gotTimer));
            }
            s_offsetSum = s_offsetCount = s_delayAverage = s_delayCounter = 0;
        }
    }
    s_lastReceiveTime = t.receiveTime;
}

// ClapEvent carries the clapTime from MSG_CLAP (already in synced master time)
struct ClapEvent {
    unsigned long long clapTime;   // from msg.payload.clap.clapTime (master synced)
    uint8_t            senderMac[6];
    unsigned long long recvTimeUs; // local esp_timer_get_time() at receipt
};

static QueueHandle_t s_queue;

static void buildTemplate() {
    for (int step = 0; step < CHIRP_STEPS; step++) {
        float freq = (float)CHIRP_FREQS[step];
        for (int s = 0; s < SAMPLES_PER_STEP; s++) {
            int idx = step * SAMPLES_PER_STEP + s;
            s_tmpl[idx] = sinf(2.0f * M_PI * freq * idx / CHIRP_SAMPLE_RATE);
        }
    }
    for (int k = 0; k < CHIRP_SAMPLES; k++)
        s_tmplEnergy += s_tmpl[k] * s_tmpl[k];
}

static void replyToChirpDevice(const uint8_t *mac, bool detected, float distCm) {
    message_data reply = {};
    reply.messageType = MSG_CLAP;
    memcpy(reply.targetAddress, mac, 6);
    memcpy(reply.senderAddress, s_myAddress, 6);
    reply.payload.clap.clapHappened = detected;
    reply.payload.clap.clapTime     = detected ? (unsigned long long)distCm : 0;

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0; peer.encrypt = false;
    if (esp_now_get_peer(mac, &peer) != ESP_OK)
        esp_now_add_peer(&peer);
    esp_now_send(mac, (uint8_t *)&reply, sizeof(reply));
}

static void recordAndAnalyze(const ClapEvent &ev) {
    // record
    unsigned long long recStart = esp_timer_get_time();
    unsigned long long nextSample = recStart;
    const unsigned long long intervalUs = 1000000ULL / CHIRP_SAMPLE_RATE;
    for (int i = 0; i < RECORD_SAMPLES; i++) {
        while (esp_timer_get_time() < nextSample) {}
        s_recBuf[i] = (int16_t)(analogRead(MIC_PIN) - 2048);
        nextSample += intervalUs;
    }
    float actualSampleRate = (float)RECORD_SAMPLES / ((esp_timer_get_time() - recStart) / 1e6f);

    // remove DC bias
    long dcSum = 0;
    for (int i = 0; i < RECORD_SAMPLES; i++) dcSum += s_recBuf[i];
    int16_t mean = (int16_t)(dcSum / RECORD_SAMPLES);
    for (int i = 0; i < RECORD_SAMPLES; i++) s_recBuf[i] -= mean;

    // per-step correlation: find each step's arrival sample
    float starts[CHIRP_STEPS] = {};
    int validCount = 0;
    for (int step = 0; step < CHIRP_STEPS; step++) {
        float stepTmpl[SAMPLES_PER_STEP];
        float stepEnergy = 0;
        for (int s = 0; s < SAMPLES_PER_STEP; s++) {
            stepTmpl[s] = sinf(2.0f * M_PI * CHIRP_FREQS[step] * s / CHIRP_SAMPLE_RATE);
            stepEnergy += stepTmpl[s] * stepTmpl[s];
        }
        float stepBestCorr = 0;
        int   stepBestDelay = 0;
        for (int d = 0; d <= RECORD_SAMPLES - SAMPLES_PER_STEP; d++) {
            float corr = 0, sigE = 0;
            for (int k = 0; k < SAMPLES_PER_STEP; k++) {
                float s = (float)s_recBuf[d + k];
                corr += s * stepTmpl[k]; sigE += s * s;
            }
            float nc = (sigE > 0) ? corr / sqrtf(stepEnergy * sigE) : 0;
            if (nc > stepBestCorr) { stepBestCorr = nc; stepBestDelay = d; }
        }
        if (stepBestCorr >= MIN_CORR && step >= 1 && step <= 6) {
            starts[validCount++] = (float)stepBestDelay - (float)(step * SAMPLES_PER_STEP);
        }
    }

    if (validCount < 2) {
        ESP_LOGI("CHIRP", "no detection (valid=%d)", validCount);
        replyToChirpDevice(ev.senderMac, false, 0);
        return;
    }

    // median chirp start in samples
    for (int i = 0; i < validCount - 1; i++)
        for (int j = i + 1; j < validCount; j++)
            if (starts[j] < starts[i]) { float t = starts[i]; starts[i] = starts[j]; starts[j] = t; }
    float medianStartSamples = starts[validCount / 2];

    // convert to absolute time: recStart + offset_in_recording
    long long detectionUs = (long long)recStart + (long long)(medianStartSamples / actualSampleRate * 1e6f);

    // apply timer sync to get detection time in master timebase
    long long detectionSynced = detectionUs + s_timeOffset;

    // distance = (detection_synced - clapTime - I2S_latency) * speed_of_sound
    long long propagationUs = detectionSynced - (long long)ev.clapTime - I2S_LATENCY_US;
    float distCm = propagationUs > 0 ? ((float)propagationUs / 1e6f * SOUND_SPEED * 100.0f) : 0.0f;

    ESP_LOGI("CHIRP", "propag=%lld µs (%.1f ms), dist=%.1f cm, synced=%s",
             propagationUs, propagationUs / 1000.0f, distCm, s_timerSynced ? "yes" : "NO");

    static float readings[5];
    static int   distCount = 0;
    readings[distCount++] = distCm;
    if (distCount >= 5) {
        float sorted[5]; memcpy(sorted, readings, sizeof(sorted));
        for (int i = 0; i < 4; i++)
            for (int j = i+1; j < 5; j++)
                if (sorted[j] < sorted[i]) { float t = sorted[i]; sorted[i] = sorted[j]; sorted[j] = t; }
        ESP_LOGI("CHIRP", "=== MEDIAN: %.1f cm ===", sorted[2]);
        replyToChirpDevice(ev.senderMac, true, sorted[2]);
        distCount = 0;
    } else {
        replyToChirpDevice(ev.senderMac, false, 0);
    }
}

static void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
    if (len < (int)sizeof(message_timer) + 1) return;
    message_data msg;
    memcpy(&msg, data, sizeof(msg));

    if (!s_hostAddressLearned) {
        memcpy(s_hostAddress, info->src_addr, 6);
        s_hostAddressLearned = true;
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, s_hostAddress, 6);
        peer.channel = 0; peer.encrypt = false;
        if (esp_now_get_peer(s_hostAddress, &peer) != ESP_OK)
            esp_now_add_peer(&peer);
        ESP_LOGI("CHIRP", "Learned master MAC %02x:%02x:%02x:%02x:%02x:%02x",
                 s_hostAddress[0], s_hostAddress[1], s_hostAddress[2],
                 s_hostAddress[3], s_hostAddress[4], s_hostAddress[5]);
        // announce now that we know master
        message_data ann = {};
        ann.messageType = MSG_ADDRESS;
        memcpy(ann.targetAddress, s_hostAddress, 6);
        memcpy(ann.senderAddress, s_myAddress, 6);
        memcpy(ann.payload.address.address, s_myAddress, 6);
        ann.payload.address.version = VERSION;
        esp_now_send(s_hostAddress, (uint8_t *)&ann, sizeof(ann));
        ESP_LOGI("CHIRP", "Announced to master");
    }
    if (msg.messageType == MSG_TIMER) {
        message_data m = msg;
        m.payload.timer.receiveTime = esp_timer_get_time();
        handleTimer(m);
        return;
    }
    if (msg.messageType == MSG_COMMAND &&
        msg.payload.command.commandType == CMD_REANNOUNCE) {
        message_data ann = {};
        ann.messageType = MSG_ADDRESS;
        memcpy(ann.targetAddress, broadcastAddress, 6);
        memcpy(ann.senderAddress, s_myAddress, 6);
        memcpy(ann.payload.address.address, s_myAddress, 6);
        ann.payload.address.version = VERSION;
        esp_now_send(broadcastAddress, (uint8_t *)&ann, sizeof(ann));
        ESP_LOGI("CHIRP", "Re-announced to broadcast");
        return;
    }
    if (msg.messageType == MSG_CLAP && msg.payload.clap.clapHappened) {
        ClapEvent ev;
        ev.clapTime   = msg.payload.clap.clapTime;
        ev.recvTimeUs = esp_timer_get_time();
        memcpy(ev.senderMac, info->src_addr, 6);
        xQueueSend(s_queue, &ev, 0);
    }
}

static void addPeer(const uint8_t *addr) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, addr, 6);
    peer.channel = 0; peer.encrypt = false;
    if (esp_now_get_peer(addr, &peer) != ESP_OK)
        esp_now_add_peer(&peer);
}

void setup() {
    Serial.begin(115200);
    delay(500);

    pinMode(MIC_PIN, INPUT);
    pinMode(LEDPINBLUE1, OUTPUT);
    buildTemplate();

    WiFi.mode(WIFI_AP_STA);
    s_queue = xQueueCreate(4, sizeof(ClapEvent));

    ESP_ERROR_CHECK(esp_now_init());
    esp_now_register_recv_cb(onDataRecv);
    addPeer(broadcastAddress);

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
    ClapEvent ev;
    if (xQueueReceive(s_queue, &ev, pdMS_TO_TICKS(5000)) == pdTRUE)
        recordAndAnalyze(ev);
    else {
        ESP_LOGI("CHIRP", "heartbeat — synced=%s offset=%lld µs", s_timerSynced ? "yes" : "no", s_timeOffset);
        if (!s_timerSynced && s_hostAddressLearned) {
            // re-announce to trigger master timer sync retry
            message_data ann = {};
            ann.messageType = MSG_ADDRESS;
            memcpy(ann.targetAddress, broadcastAddress, 6);
            memcpy(ann.senderAddress, s_myAddress, 6);
            memcpy(ann.payload.address.address, s_myAddress, 6);
            ann.payload.address.version = VERSION;
            esp_now_send(broadcastAddress, (uint8_t *)&ann, sizeof(ann));
            ESP_LOGI("CHIRP", "Re-announced to trigger timer sync");
        }
    }
}
