// Chirp-Device: drop-in replacement for the Clap-Device for distance calibration.
// On CMD_START/CONTINUE_DISTANCE_CALIBRATION it plays a burst of CHIRP_BURST_COUNT
// chirps via I2S -> PCM5102A -> boombox line-in, announcing each one with MSG_CLAP
// carrying its synced emission timestamp and its index in the burst.
// I2S pins (PCM5102A): BCLK GPIO38, WS GPIO39, DOUT GPIO40

#include <Arduino.h>
#include <MyDefines.h>
#include <esp_log.h>
#include "esp_now.h"
#include "WiFi.h"
#include "driver/i2s_std.h"
#include <math.h>

#define I2S_SAMPLE_RATE  44100
#define I2S_BCLK_PIN     GPIO_NUM_38
#define I2S_WS_PIN       GPIO_NUM_39
#define I2S_DOUT_PIN     GPIO_NUM_40

#define SAMPLES_PER_STEP  (I2S_SAMPLE_RATE * CHIRP_STEP_MS / 1000)   // 132
#define CHIRP_SAMPLES     (CHIRP_STEPS * SAMPLES_PER_STEP)            // 1056
#define CHIRP_FRAMES      (CHIRP_SAMPLES * 2)                         // stereo
#define CHIRP_AMPLITUDE   26000
// Silence played before the chirp so the PCM5102A's PLL can lock onto BCLK
// before there is anything to hear. The peripheral consumes at exactly the
// sample rate from the moment it is enabled, so frame k leaves at
// enable + k/I2S_SAMPLE_RATE — which means a lead-in of a known length costs
// nothing in timing accuracy, it just moves the chirp to a later known frame.
// Long enough for the DAC's PLL to lock, short enough not to eat the client's
// search window: the client records a fixed window from its own schedule, so
// every millisecond of lead-in is a millisecond of detection range given up.
// 100 ms cost half the range (67 m -> 33 m); 40 ms costs a fifth.
#define CHIRP_LEAD_IN_MS  40
#define LEAD_IN_SAMPLES   (I2S_SAMPLE_RATE * CHIRP_LEAD_IN_MS / 1000)   // 4410
#define LEAD_IN_INTS      (LEAD_IN_SAMPLES * 2)                         // stereo
// i2s_channel_write returns once the last bytes are queued, not played, so up to
// a full DMA ring — 6 x 240 frames, 33 ms — can still be unplayed. The chirp is
// at the very end of the buffer, so disabling the channel any sooner than this
// truncates its tail.
#define CHIRP_DRAIN_MS    50
// The devkit's BOOT button. A strapping pin at reset, an ordinary input after —
// which is all the chirp needs to be testable with nothing but a usb cable, no
// master, no mesh, no browser.
#define TEST_CHIRP_BUTTON GPIO_NUM_0

i2s_chan_handle_t txChan = nullptr;
int16_t chirpBuffer[CHIRP_FRAMES];
// lead-in silence and the chirp as one contiguous write: a gap between two
// writes could underrun, and auto_clear would then insert its own zeros and
// shift the chirp off the frame it was timestamped for
int16_t chirpWithLeadIn[LEAD_IN_INTS + CHIRP_FRAMES];

QueueHandle_t recvQueue;
uint8_t myAddress[6];
uint8_t hostAddress[6] = {0x34, 0x85, 0x18, 0x8f, 0xbf, 0xb8}; // fallback, learned from first MSG_TIMER
TaskHandle_t chirpTaskHandle = nullptr;
volatile bool engaged = false; // master has contacted us, stop announcing
bool hostLearned = false;

void addPeer(const uint8_t *addr);
void startTestChirp(const char *source);
void startTestTone();
void startTestBeeps();

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

        // Report every completed sync, not just the first. The master's clock is
        // microseconds since its own boot, so a master reboot invalidates this
        // offset entirely — and re-syncing needs ten correctly paired packets,
        // which can silently fail. Reporting once meant the master could never
        // tell a fresh offset from one belonging to a previous master
        // generation, and a stale one puts every emission stamp out by the old
        // master's whole uptime.
        {
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

// chirpBuffer preceded by CHIRP_LEAD_IN_MS of digital silence
void buildChirpWithLeadIn() {
    memset(chirpWithLeadIn, 0, LEAD_IN_INTS * sizeof(int16_t));
    memcpy(chirpWithLeadIn + LEAD_IN_INTS, chirpBuffer, sizeof(chirpBuffer));
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
        // MSB (left-justified), not Philips: this board's PCM5102A has FMT tied
        // high. Tested — Philips gives silence on it, MSB gives sound. So the
        // format was never the reason the chirp sounds wrong; buildChirp is.
        // MSB (left-justified): this board's PCM5102A has FMT tied high. Tested
        // both ways — Philips is silent on it.
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
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
    // Left in READY on purpose, not enabled. An enabled channel clocks silence
    // through a 6 x 240 frame ring — 33 ms at 44.1 kHz — and a write lands
    // wherever the DMA happens to be, so the sound leaves an unknown time after
    // we stamp it. Each chirp preloads the ring while stopped instead, and
    // starts on the enable, which we can timestamp.
}

void broadcastClapMessage(bool happened, uint8_t chirpIndex, long long emitTime) {
    message_data clapMessage;
    clapMessage.messageType = MSG_CLAP;
    memcpy(clapMessage.targetAddress, broadcastAddress, 6);
    memcpy(clapMessage.senderAddress, myAddress, 6);
    clapMessage.payload.clap.clapTime = (unsigned long long)emitTime;
    clapMessage.payload.clap.clapHappened = happened;
    clapMessage.payload.clap.chirpIndex = chirpIndex;
    esp_now_send(broadcastAddress, (uint8_t *)&clapMessage, sizeof(clapMessage));
}

// One calibration = CHIRP_BURST_COUNT chirps on a fixed schedule. Every chirp is
// announced with its own emission timestamp and index; the clients count the same
// schedule off the same calibration broadcast and report one measurement per index.
// The I2S channel is enabled once, in setup(), and never disabled. The
// PCM5102A soft-un-mutes and settles its charge pump when its clock starts, and
// that takes seconds — so anything that stops BCLK costs the first few seconds
// of audio. Disabling between chirps (1b08d21) made every chirp play into a
// muting DAC; enabling once per burst only moved the problem, leaving the first
// seven chirps of a ten-chirp burst inaudible. A clock that never stops is how
// this worked before the burst change, and it is the only arrangement where
// every chirp comes out at the same level.
//
// Timing is exact for free: the peripheral consumes at precisely
// I2S_SAMPLE_RATE from the single enable, so frame k leaves at
// enable + k/I2S_SAMPLE_RATE. audioFrames counts every frame ever written, so
// any chirp's emission time is arithmetic on a counter that never resets.
static const int PUMP_CHUNK_FRAMES = 441;                 // 10 ms at 44.1 kHz
// Not zeros, and not a token 1 LSB either. The DAC needs ramp-up: after an idle
// stretch the first chirps of a burst simply do not come out, which is why a
// first burst gave 4 and a second burst 3 s later gave 10. Feeding true silence
// lets it fall asleep between bursts; feeding +-1 LSB was not enough to hold it
// awake (7-8 of 10). +-64 is -54 dBFS, and alternating every frame puts it at
// half the sample rate — 22 kHz, above hearing — so it keeps the DAC fully awake
// without being audible.
#define PUMP_DITHER_LSB 64
static int16_t pumpSilence[PUMP_CHUNK_FRAMES * 2];

static void buildPumpDither() {
    for (int i = 0; i < PUMP_CHUNK_FRAMES; i++) {
        int16_t v = (i & 1) ? PUMP_DITHER_LSB : -PUMP_DITHER_LSB;
        pumpSilence[2 * i]     = v;
        pumpSilence[2 * i + 1] = v;
    }
}

volatile uint64_t audioFrames = 0;      // frames written since the single enable
long long audioEnableTime = 0;          // syncedTime() at that enable

// a burst request, consumed by the pump
volatile int      burstRemaining = 0;   // chirps still to emit
volatile bool     burstAnnounce  = false;
volatile uint8_t  burstNextIndex = 0;
volatile uint64_t burstNextFrame = 0;   // frame at which the next chirp starts

void requestChirps(int count, bool announce) {
    const int framesPerPeriod = I2S_SAMPLE_RATE * CHIRP_BURST_PERIOD_MS / 1000;
    // start far enough ahead that the pump has not already written past it
    burstNextFrame = audioFrames + framesPerPeriod;
    burstNextIndex = 0;
    burstAnnounce  = announce;
    burstRemaining = count;
    (void)framesPerPeriod;
}

void audioPumpTask(void *) {
    const int framesPerPeriod = I2S_SAMPLE_RATE * CHIRP_BURST_PERIOD_MS / 1000;
    buildPumpDither();
    i2s_channel_disable(txChan);                 // ensure READY
    ESP_ERROR_CHECK(i2s_channel_enable(txChan)); // the only enable, ever
    audioEnableTime = syncedTime();
    audioFrames = 0;
    ESP_LOGI("CHIRP", "audio pump running, clock is up and stays up");

    while (true) {
        size_t w = 0;
        if (burstRemaining > 0 && audioFrames >= burstNextFrame) {
            // this frame is the first sample of the chirp, so it leaves the DAC
            // at enable + audioFrames/rate — announced just before it is queued
            long long emitTime = audioEnableTime +
                (long long)((audioFrames * 1000000ULL) / (uint64_t)I2S_SAMPLE_RATE);
            uint8_t idx = burstNextIndex;
            if (burstAnnounce) broadcastClapMessage(true, idx, emitTime);
            i2s_channel_write(txChan, chirpBuffer, sizeof(chirpBuffer), &w, portMAX_DELAY);
            audioFrames += CHIRP_SAMPLES;
            burstNextIndex = idx + 1;
            burstNextFrame = burstNextFrame + framesPerPeriod;
            if (--burstRemaining == 0) ESP_LOGI("CHIRP", "Burst done");
            ESP_LOGI("CHIRP", "Chirp %u at frame %llu (+%llu ms)", (unsigned)(idx + 1),
                     (unsigned long long)(audioFrames - CHIRP_SAMPLES),
                     (unsigned long long)((audioFrames - CHIRP_SAMPLES) * 1000ULL / I2S_SAMPLE_RATE));
            continue;
        }
        // silence, but never past the start of the next chirp
        int n = PUMP_CHUNK_FRAMES;
        if (burstRemaining > 0) {
            uint64_t until = burstNextFrame - audioFrames;
            if (until < (uint64_t)n) n = (int)until;
        }
        if (n <= 0) n = 1;
        i2s_channel_write(txChan, pumpSilence, n * 2 * sizeof(int16_t), &w, portMAX_DELAY);
        audioFrames += n;
    }
}

void playChirpBurst() {
    requestChirps(CHIRP_BURST_COUNT, true);
}

// Just the sound. No timestamp, no MSG_CLAP, no slot — nothing downstream hears
// about it, so it can be fired from anywhere at any time without touching a
// calibration in progress. This is the one for listening to the chirp itself.
void playSingleChirp() {
    requestChirps(1, false);   // no timestamp, no MSG_CLAP — just the sound
    ESP_LOGI("CHIRP", "Test chirp queued (%d steps x %d ms, %d Hz to %d Hz)",
             CHIRP_STEPS, CHIRP_STEP_MS, chirpFrequencies[0],
             chirpFrequencies[CHIRP_STEPS - 1]);
}

void playToneSequence(const int *freqs, int count, int msPer, int fadeMs) {
    i2s_channel_disable(txChan);                 // ensure READY
    ESP_ERROR_CHECK(i2s_channel_enable(txChan)); // clock starts here
    // same run-up the chirp gets, for the same reason: the DAC recovers its
    // clock from BCLK and needs it running before there is anything to hear
    {
        size_t w = 0;
        i2s_channel_write(txChan, chirpWithLeadIn, LEAD_IN_INTS * sizeof(int16_t),
                          &w, portMAX_DELAY);
    }

    const int framesPerChunk = 256;
    static int16_t chunk[framesPerChunk * 2];

    for (int t = 0; t < count; t++) {
        const int totalFrames = I2S_SAMPLE_RATE * msPer / 1000;
        int fadeFrames = I2S_SAMPLE_RATE * fadeMs / 1000;
        if (fadeFrames * 2 > totalFrames) fadeFrames = totalFrames / 2;

        float phase = 0.0f;
        const float inc = 2.0f * M_PI * (float)freqs[t] / (float)I2S_SAMPLE_RATE;
        int done = 0;
        while (done < totalFrames) {
            int n = (totalFrames - done) < framesPerChunk ? (totalFrames - done) : framesPerChunk;
            for (int i = 0; i < n; i++) {
                const int idx = done + i;
                float env = 1.0f;
                if (fadeFrames > 0) {
                    if (idx < fadeFrames) {
                        env = 0.5f * (1.0f - cosf(M_PI * (float)idx / (float)fadeFrames));
                    } else if (idx >= totalFrames - fadeFrames) {
                        env = 0.5f * (1.0f - cosf(M_PI * (float)(totalFrames - 1 - idx) / (float)fadeFrames));
                    }
                }
                int16_t v = (int16_t)(CHIRP_AMPLITUDE * env * sinf(phase));
                phase += inc;
                if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
                chunk[2 * i]     = v;
                chunk[2 * i + 1] = v;
            }
            size_t written = 0;
            i2s_channel_write(txChan, chunk, n * 2 * sizeof(int16_t), &written, portMAX_DELAY);
            done += n;
        }
        ESP_LOGI("CHIRP", "  tone %d/%d: %d Hz for %d ms", t + 1, count, freqs[t], msPer);
    }
    vTaskDelay(pdMS_TO_TICKS(CHIRP_DRAIN_MS));
    i2s_channel_disable(txChan);
}

void playTestTone(int freqHz, int ms) {
    playToneSequence(&freqHz, 1, ms, 5);
    ESP_LOGI("CHIRP", "Test tone done: %d Hz for %d ms at amplitude %d", freqHz, ms, CHIRP_AMPLITUDE);
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
    playChirpBurst();
    chirpTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

void testChirpTask(void *) {
    playSingleChirp();
    chirpTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

void testToneTask(void *) {
    ESP_LOGI("CHIRP", "Test tone: 1 kHz, 3 s — if this is silent, the audio path is at fault");
    playTestTone(1000, 3000);
    chirpTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

// Deliberately not the real chirp: this does not touch chirpFrequencies,
// CHIRP_STEPS or CHIRP_STEP_MS, which the client's correlation template is built
// from, so it cannot affect detection.
void testBeepTask(void *) {
    static const int beeps[3] = {440, 220, 660};
    ESP_LOGI("CHIRP", "Beep boop beep: 440 / 220 / 660 Hz, 300 ms each");
    playToneSequence(beeps, 3, 300, 5);
    ESP_LOGI("CHIRP", "Beep sequence done");
    chirpTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

void startTestBeeps() {
    if (chirpTaskHandle != nullptr) {
        eTaskState st = eTaskGetState(chirpTaskHandle);
        if (st != eDeleted && st != eInvalid) {
            ESP_LOGW("CHIRP", "Busy, ignoring beep sequence");
            return;
        }
        chirpTaskHandle = nullptr;
    }
    xTaskCreatePinnedToCore(testBeepTask, "testBeeps", 8192, nullptr, 10, &chirpTaskHandle, 1);
}

void startTestTone() {
    if (chirpTaskHandle != nullptr) {
        eTaskState st = eTaskGetState(chirpTaskHandle);
        if (st != eDeleted && st != eInvalid) {
            ESP_LOGW("CHIRP", "Busy, ignoring test tone");
            return;
        }
        chirpTaskHandle = nullptr;
    }
    xTaskCreatePinnedToCore(testToneTask, "testTone", 8192, nullptr, 10, &chirpTaskHandle, 1);
}

// shares chirpTaskHandle with the burst on purpose: both drive the one I2S
// channel, so a test chirp must not start on top of a calibration. A burst that
// was killed mid-flight can leave the handle pointing at a task that no longer
// exists, which would refuse every test chirp from then on — so check the task
// really is alive rather than merely that the pointer is set.
void startTestChirp(const char *source) {
    if (chirpTaskHandle != nullptr) {
        eTaskState state = eTaskGetState(chirpTaskHandle);
        if (state != eDeleted && state != eInvalid) {
            ESP_LOGW("CHIRP", "Busy chirping already, ignoring test chirp from %s", source);
            return;
        }
        ESP_LOGW("CHIRP", "Clearing a stale chirp task handle");
        chirpTaskHandle = nullptr;
    }
    ESP_LOGI("CHIRP", "Test chirp from %s", source);
    BaseType_t r = xTaskCreatePinnedToCore(testChirpTask, "testChirp", 8192, nullptr,
                                           10, &chirpTaskHandle, 1);
    if (r != pdPASS) {
        ESP_LOGE("CHIRP", "Could not start the chirp task (%d)", (int)r);
        chirpTaskHandle = nullptr;
    }
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
            case CMD_REANNOUNCE:
                // was unhandled, so a fleet-wide reannounce skipped the one
                // device whose address the master cannot guess
                ESP_LOGI("CHIRP", "CMD_REANNOUNCE, announcing again");
                engaged = false;
                break;
            case CMD_TEST_CHIRP:
                startTestChirp("master");
                break;
            case CMD_RESET_SYSTEM:
                // Was ignored entirely: the chirp device is not in the address
                // list, so neither the fleet reboot nor a broadcast reset ever
                // reached it — the only way to restart it was to pull the usb.
                ESP_LOGI("CHIRP", "CMD_RESET_SYSTEM, restarting");
                vTaskDelay(pdMS_TO_TICKS(200));
                ESP.restart();
                break;
            case CMD_CANCEL_CALIBRATION:
            case CMD_END_CALIBRATION:
                burstRemaining = 0;   // the burst is pump state, not a task
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
uint32_t announceCount = 0;
void announceAddress() {
    announceCount++;
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
    buildChirpWithLeadIn();
    i2sInit();

    pinMode(TEST_CHIRP_BUTTON, INPUT_PULLUP);

    recvQueue = xQueueCreate(8, sizeof(message_data));

    ESP_ERROR_CHECK(esp_now_init());
    esp_now_register_recv_cb(onDataRecv);
    esp_now_register_send_cb(onDataSent);
    addPeer(hostAddress);
    addPeer(broadcastAddress);

    xTaskCreate(handleReceiveTask, "handleRecv", 4096, nullptr, 5, nullptr);
    // owns the I2S channel for the board's lifetime and keeps inaudible dither
    // flowing, so the DAC never sleeps and never has to ramp up mid-burst
    xTaskCreatePinnedToCore(audioPumpTask, "audioPump", 4096, nullptr, 8, nullptr, 1);
    // owns the I2S channel for the lifetime of the board: the DAC un-mutes and
    // settles over seconds when its clock starts, so the clock must never stop
    xTaskCreatePinnedToCore(audioPumpTask, "audioPump", 4096, nullptr, 8, nullptr, 1);

    delay(500);
    announceAddress();

    ESP_LOGI("CHIRP", "Ready. My address %02x:%02x:%02x:%02x:%02x:%02x",
             myAddress[0], myAddress[1], myAddress[2], myAddress[3], myAddress[4], myAddress[5]);
    ESP_LOGI("CHIRP", "BOOT or any serial char = one test chirp");
}

// Polls fast enough for a button to feel connected to the sound it makes, and
// keeps the announce on its own clock rather than the loop's — before this the
// loop slept for whole seconds at a time, which no button survives.
void loop() {
    static unsigned long lastAnnounce = 0;
    static bool wasPressed = false;

    bool pressed = (digitalRead(TEST_CHIRP_BUTTON) == LOW);
    if (pressed && !wasPressed) startTestChirp("BOOT button");
    wasPressed = pressed;

    // anything at all on serial chirps once; 'c' is just the shortest thing to type
    while (Serial.available() > 0) {
        int c = Serial.read();
        if (c == '\n' || c == '\r') continue;
        // 't'/'b' tone helpers are gone: the pump owns the channel, and the
        // client's energy metric replaced what they were for
        startTestChirp("serial");
        while (Serial.available() > 0) Serial.read();   // one per line, not per byte
    }

    // Announce even once engaged, just rarely. clapDeviceAddress on the master
    // defaults to a hardcoded address that is not ours, and it only learns the
    // real one from MSG_SOUND_DEVICE — so a device that fell silent after its
    // first contact stayed invisible to every master that rebooted later. The
    // calibration command is a broadcast, so it still chirped; only the unicast
    // timer sync was lost, which left its emission timestamps unsynced and every
    // client unable to place the chirp. Cheap insurance at 0.1 Hz.
    unsigned long interval = engaged ? 10000UL : 2000UL;
    if (millis() - lastAnnounce >= interval) {
        lastAnnounce = millis();
        announceAddress();
        // Everything the emitter side of the clock handshake depends on. The
        // master refuses a burst when it cannot confirm this clock, and with the
        // device silent at idle there was no way to tell whether it was failing
        // to announce, failing to be engaged, or failing to sync.
        ESP_LOGI("CHIRP", "announces=%lu engaged=%d timerSynced=%d offset=%lld samples=%d delayAvg=%d",
                 (unsigned long)announceCount, (int)engaged, (int)timerSynced,
                 (long long)timeOffset, sampleCount, delayAverage);
    }

    vTaskDelay(pdMS_TO_TICKS(20));
}
