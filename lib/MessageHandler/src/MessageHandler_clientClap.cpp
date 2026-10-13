
#include "MessageHandler.h"
#if (DEVICE_MODE == CLIENT)
#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"
#include <PeakDetection.h>
#include <math.h>
#include <algorithm>

// Chirp correlation template. Step count, length and frequencies come from
// MyDefines.h so they always match what the Chirp-Device emits.
static const int   CHIRP_SAMPLE_RATE  = 10000;           // Hz
static const int   SAMPLES_PER_STEP   = CHIRP_SAMPLE_RATE * CHIRP_STEP_MS / 1000; // 30
static const int   CHIRP_SAMPLES      = CHIRP_STEPS * SAMPLES_PER_STEP;           // 240
// Wide enough that the emitter's lead-in and the schedule skew between the two
// boards come out of headroom rather than out of range. Must still leave the
// correlation time to finish inside CHIRP_BURST_PERIOD_MS.
static const int   RECORD_MS          = 300;
static const int   RECORD_SAMPLES     = CHIRP_SAMPLE_RATE * RECORD_MS / 1000;     // 3000
static const int   MAX_LAG            = RECORD_SAMPLES - CHIRP_SAMPLES;           // last window that fits
static const float SOUND_SPEED        = 343.0f;          // m/s
static const int   EMISSION_MARGIN    = 20;              // 2 ms of slack on the emission stamp
// A window holds a chirp when its strongest match beats the median lag by this
// much. The median lag holds noise or a faint tail: 12x at 4 m, 37x at 1 m,
// where noise alone gets to about 3.5x over this many lags.
static const float NOISE_GATE_RATIO   = 6.0f;
// The earliest match peak above this x the strongest wins. The chirp's own side
// peaks reach 0.41 of the peak they belong to, and at 4 m an echo came in twice
// as strong as the direct sound, which then sat at 0.45-0.65 of it.
static const float FIRST_PEAK_RATIO   = 0.5f;

static void buildChirpTemplate(float* tmpl) {
    for (int step = 0; step < CHIRP_STEPS; step++) {
        float freq = (float)chirpFrequencies[step];
        for (int s = 0; s < SAMPLES_PER_STEP; s++) {
            int idx = step * SAMPLES_PER_STEP + s;
            tmpl[idx] = sinf(2.0f * M_PI * freq * idx / CHIRP_SAMPLE_RATE);
        }
    }
}

// The template's 90° twin. With both, the correlation's magnitude is an outline
// with one smooth peak per arrival. The raw correlation ripples at the chirp's
// pitch, and taking its earliest peak landed one wave (0.43 ms, 15 cm) early in
// up to 60% of chirps.
static void buildChirpTemplateQ(float* tmpl) {
    for (int step = 0; step < CHIRP_STEPS; step++) {
        float freq = (float)chirpFrequencies[step];
        for (int s = 0; s < SAMPLES_PER_STEP; s++) {
            int idx = step * SAMPLES_PER_STEP + s;
            tmpl[idx] = cosf(2.0f * M_PI * freq * idx / CHIRP_SAMPLE_RATE);
        }
    }
}

#if CHIRP_DIAG
#include "mbedtls/base64.h"
// Diagnostic build (env Client_Diag), for one board on a usb cable: every
// arrival in every window, the raw recording, and what the old rule on the raw
// correlation would have picked.
static int diagChirp = 0;                          // window index, set by runClapTask
// the old rule, for comparison: normalised correlation, earliest peak above
// 70% of its maximum, noise gate at mean + 5 sigma
static const float OLD_MIN_CORR       = 0.15f;
static const float OLD_GATE_SIGMAS    = 5.0f;
static const float OLD_PEAK_RATIO     = 0.7f;

// The whole recording as base64 of the int16 samples, in parts that each fit a
// line. Blocking writes, set by the caller — the usual non-blocking ones would
// drop most of it.
static void diagDumpRecording(const int16_t* rec) {
    static const int PART = 120;   // samples per line
    static unsigned char text[((PART * 2 + 2) / 3) * 4 + 4];
    const int parts = (RECORD_SAMPLES + PART - 1) / PART;
    for (int p = 0; p < parts; p++) {
        int n = RECORD_SAMPLES - p * PART;
        if (n > PART) n = PART;
        size_t olen = 0;
        mbedtls_base64_encode(text, sizeof(text), &olen, (const unsigned char*)(rec + p * PART), n * 2);
        text[olen] = 0;
        Serial.printf("DIAGRAW c=%d p=%d/%d %s\n", diagChirp, p, parts, (const char*)text);
    }
    Serial.setTxTimeoutMs(0);
}
#endif

// Records one window and correlates it against the template. Returns the delay
// from the first sample to the chirp arrival in samples, or -1 when nothing
// plausible was heard. recordStart is the esp_timer stamp of the first sample.
float MessageHandler::detectChirp(int audioPin, const float* tmpl, float tmplEnergy,
                                  unsigned long long& recordStart, float& peakCorr) {
    static int16_t recBuf[RECORD_SAMPLES];
    static float envBuf[MAX_LAG + 1];  // the correlation's outline, indexed by lag in samples
    static float tmplQ[CHIRP_SAMPLES];
    static bool tmplQBuilt = false;
    if (!tmplQBuilt) {
        buildChirpTemplateQ(tmplQ);
        tmplQBuilt = true;
    }
#if CHIRP_DIAG
    static float corrBuf[MAX_LAG + 1]; // the raw correlation, for the old rule
#endif

    const unsigned long long intervalUs = 1000000ULL / CHIRP_SAMPLE_RATE; // 100 µs
    recordStart = esp_timer_get_time();
    unsigned long long nextSample = recordStart;
    for (int i = 0; i < RECORD_SAMPLES; i++) {
        while (esp_timer_get_time() < nextSample) {}
        recBuf[i] = (int16_t)(analogRead(audioPin) - 2048);
        nextSample += intervalUs;
    }

    // What the microphone actually produced this window. A flat line means
    // nothing reached the ADC, which no amount of correlation can rescue — and
    // it is the difference between "the chirp never got here" and "the chirp
    // got here and did not match".
    int16_t rawLo = 32767, rawHi = -32768;
    long long rawSum = 0, rawSumSq = 0;
    for (int i = 0; i < RECORD_SAMPLES; i++) {
        int16_t v = recBuf[i];
        if (v < rawLo) rawLo = v;
        if (v > rawHi) rawHi = v;
        rawSum   += v;
        rawSumSq += (long long)v * v;
    }
    long long rawMean = rawSum / RECORD_SAMPLES;
    long long rawVar  = rawSumSq / RECORD_SAMPLES - rawMean * rawMean;
    int rawRms = (rawVar > 0) ? (int)sqrtf((float)rawVar) : 0;

    // The mic idles well off the ADC's middle (one board sat at 1275 of 4096).
    // Left in, that offset counts as signal energy and dilutes every match
    // score about threefold.
    for (int i = 0; i < RECORD_SAMPLES; i++) recBuf[i] -= (int16_t)rawMean;

    // Where in the window the energy actually is, independent of the template.
    // A chirp is a burst: one 10 ms block much louder than the rest. If the
    // loudest block barely beats the quietest, nothing arrived and the template
    // is irrelevant; if it stands out clearly but correlation is still zero,
    // the sound got here and simply does not match what we are looking for.
    const int blk = 100;                       // 10 ms at 10 kHz
    const int nblk = RECORD_SAMPLES / blk;     // 30
    int loudBlk = 0, loudRms = 0, quietRms = 32767;
    for (int b = 0; b < nblk; b++) {
        long long e = 0;
        for (int i = 0; i < blk; i++) {
            int d = recBuf[b * blk + i];
            e += (long long)d * d;
        }
        int r = (int)sqrtf((float)(e / blk));
        if (r > loudRms) { loudRms = r; loudBlk = b; }
        if (r < quietRms) quietRms = r;
    }

    // Sound cannot arrive before it was emitted. The emitter announces every chirp
    // with its own timestamp just before it plays, so the search starts there
    // instead of at a fixed skip — that skip was what made close boards invisible.
    // The stamp is taken before the DAC and amp, so the real arrival is always
    // later than it; the margin only covers timer sync error.
    int minLag = 0;
    long long rawLag = 0;          // before clamping — the clamp hides how wrong a bad stamp is
    long long staleOffsetUs = 0;   // set when a stamp arrived but could not belong to this window
    long long emissionOffsetUs = 0;
    unsigned long long emission = lastChirpEmission;
    if (emission != 0) {
        long long emissionLocal = (long long)emission - ledInstance->getTimerOffset();
        long long offsetUs = emissionLocal - (long long)recordStart;
        emissionOffsetUs = offsetUs;
        // Reject a stamp that cannot belong to this window: one from the previous
        // chirp, or any stamp at all from an emitter whose clock was never synced
        // (its syncedTime() is then raw uptime, out by seconds). Searching the
        // whole window is the right fallback; trusting a wild stamp is not, since
        // it pins minLag at MAX_LAG and searches exactly one lag.
        if (offsetUs < -(long long)CHIRP_BURST_PERIOD_MS * 1000 ||
            offsetUs > (long long)RECORD_MS * 1000) {
            emission = 0;
            staleOffsetUs = offsetUs;   // remember why, for the log
        } else {
            rawLag = offsetUs / (long long)intervalUs - EMISSION_MARGIN;
            if (rawLag > 0) minLag = (rawLag < MAX_LAG) ? (int)rawLag : MAX_LAG;
        }
    }

    float envMax = 0;
    for (int d = minLag; d <= MAX_LAG; d++) {
        float corr = 0, corrQ = 0;
#if CHIRP_DIAG
        float sigEnergy = 0;
#endif
        for (int k = 0; k < CHIRP_SAMPLES; k++) {
            float s = (float)recBuf[d + k];
            corr  += s * tmpl[k];
            corrQ += s * tmplQ[k];
#if CHIRP_DIAG
            sigEnergy += s * s;
#endif
        }
        // Not normalised by the signal's own energy at this lag: that made every
        // faint, chirp-shaped echo score like the direct sound, and at 4 m the
        // echoes lifted the noise gate above the arrival in every window.
        envBuf[d] = sqrtf(corr * corr + corrQ * corrQ);
#if CHIRP_DIAG
        corrBuf[d] = (sigEnergy > 0) ? corr / sqrtf(tmplEnergy * sigEnergy) : 0;
#endif
        if (envBuf[d] > envMax) envMax = envBuf[d];
    }

    // The median lag holds noise, or at most a faint tail: the chirp and its loud
    // echoes span a few hundred of these 2,700 lags.
    int lagCount = MAX_LAG - minLag + 1;
    static float sortBuf[MAX_LAG + 1];
    memcpy(sortBuf, envBuf + minLag, lagCount * sizeof(float));
    std::nth_element(sortBuf, sortBuf + lagCount / 2, sortBuf + lagCount);
    float noiseFloor = sortBuf[lagCount / 2];
    float strength = (noiseFloor > 0) ? envMax / noiseFloor : 0.0f;

    // Everything the accept/reject decision rests on, in one line:
    //   rms ~0                  -> the mic heard nothing, look at the mic
    //   emission=MISSING        -> the emitter's announce never arrived
    //   minLag at MAX_LAG       -> the search window is empty, timing is off
    //   loud but peak under 6x  -> sound arrived but did not match the template
    // rawLag is the whole story when minLag is pinned: a value just over MAX_LAG
    // means the emission is a little too late (schedule/lead-in), while one
    // orders of magnitude out means the emitter's clock is not synced at all.
    ESP_LOGI("CLAP", "win rms%d loud%dms x%.0f | annc%lu %s emit=%llums our=%lldms rec=%llums off=%lldms lag%d/%d peak %.1fx noise",
             rawRms, loudBlk * 10,
             quietRms > 0 ? (float)loudRms / (float)quietRms : 0.0f,
             (unsigned long)chirpAnnounceCount,
             (emission != 0) ? "ok" : (staleOffsetUs != 0 ? "STALE" : "NONE"),
             (unsigned long long)(lastChirpEmission / 1000),
             (long long)(ledInstance->getTimerOffset() / 1000),
             (unsigned long long)(recordStart / 1000),
             (emission != 0 ? (long long)(emissionOffsetUs / 1000) : staleOffsetUs / 1000),
             minLag, MAX_LAG, strength);

    // The direct sound always arrives first, so take the earliest match peak
    // close to the strongest rather than the strongest itself.
    int peakIdx = -1;
    if (strength >= NOISE_GATE_RATIO) {
        float accept = envMax * FIRST_PEAK_RATIO;
        // from minLag + 1: the local-max test and the parabolic fit both read d - 1,
        // and envBuf below minLag still holds the previous window's values
        for (int d = (minLag > 0 ? minLag + 1 : 1); d < MAX_LAG; d++) {
            if (envBuf[d] >= accept && envBuf[d] >= envBuf[d - 1] && envBuf[d] >= envBuf[d + 1]) {
                peakIdx = d;
                break;
            }
        }
    }
    float pos = -1.0f;
    peakCorr = strength;
    if (peakIdx > 0) {
        // parabolic fit through the peak for sub-sample resolution (one sample = 3.4 cm)
        float c0 = envBuf[peakIdx - 1], c1 = envBuf[peakIdx], c2 = envBuf[peakIdx + 1];
        float denom = c0 - 2.0f * c1 + c2;
        float frac  = (denom < -1e-9f) ? 0.5f * (c0 - c2) / denom : 0.0f;
        pos = (float)peakIdx + constrain(frac, -0.5f, 0.5f);
        peakCorr = c1 / noiseFloor;
    }

#if CHIRP_DIAG
    {
        // what the old rule would have picked
        float bestCorr = 0, rawSumC = 0, rawSumSqC = 0;
        for (int d = minLag; d <= MAX_LAG; d++) {
            if (corrBuf[d] > bestCorr) bestCorr = corrBuf[d];
            rawSumC   += corrBuf[d];
            rawSumSqC += corrBuf[d] * corrBuf[d];
        }
        float rawMeanC = rawSumC / lagCount;
        float rawVarC = rawSumSqC / lagCount - rawMeanC * rawMeanC;
        float rawGate = fmaxf(OLD_MIN_CORR, rawMeanC + OLD_GATE_SIGMAS * (rawVarC > 0 ? sqrtf(rawVarC) : 0.0f));
        int oldIdx = -1;
        if (bestCorr >= rawGate) {
            float accept = bestCorr * OLD_PEAK_RATIO;
            for (int d = (minLag > 0 ? minLag + 1 : 1); d < MAX_LAG; d++) {
                if (corrBuf[d] >= accept && corrBuf[d] >= corrBuf[d - 1] && corrBuf[d] >= corrBuf[d + 1]) {
                    oldIdx = d;
                    break;
                }
            }
        }
        // every match peak above a fifth of the strongest, as a share of it
        char peaks[420];
        int len = 0, listed = 0;
        peaks[0] = 0;
        for (int d = (minLag > 0 ? minLag + 1 : 1); d < MAX_LAG && listed < 16 && len < (int)sizeof(peaks) - 24; d++) {
            if (envBuf[d] < envMax * 0.2f || envBuf[d] < envBuf[d - 1] || envBuf[d] < envBuf[d + 1]) continue;
            len += snprintf(peaks + len, sizeof(peaks) - len, " %d:%.3f", d, envBuf[d] / envMax);
            listed++;
        }
        // lags in samples of 0.1 ms from the window's first sample; emit is where
        // the chirp device's stamp falls on that same scale. Blocking from here:
        // this line alone overflows the 256-byte send buffer, and the driver then
        // dropped the start of the dump that follows.
        Serial.setTxTimeoutMs(50);
        Serial.printf("DIAG c=%d stamp=%s emit=%.1f minLag=%d gate=%.3f best=%.3f old=%d envGate=%.3f envMax=%.3f new=%.2f peaks%s\n",
                      diagChirp, (emission != 0) ? "ok" : (staleOffsetUs != 0 ? "stale" : "none"),
                      emissionOffsetUs / 100.0f, minLag, rawGate, bestCorr, oldIdx, noiseFloor * NOISE_GATE_RATIO, envMax, pos, peaks);
        diagDumpRecording(recBuf);
    }
#endif
    return pos;
}

void MessageHandler::runClapTask() {
    int audioPin = 5;
    pinMode(audioPin, INPUT);

    if (getChirpMode()) {
        // chirp cross-correlation mode, one measurement per chirp of the burst
        ESP_LOGI("CLAP", "Chirp mode: %d chirps, %d ms window at %d Hz",
                 CHIRP_BURST_COUNT, RECORD_MS, CHIRP_SAMPLE_RATE);

        static float chirpTmpl[CHIRP_SAMPLES];
        buildChirpTemplate(chirpTmpl);
        float tmplEnergy = 0;
        for (int k = 0; k < CHIRP_SAMPLES; k++) tmplEnergy += chirpTmpl[k] * chirpTmpl[k];

        const unsigned long long intervalUs = 1000000ULL / CHIRP_SAMPLE_RATE;
        // the emitter counts the same schedule off the same calibration broadcast,
        // so window n lines up with chirp n without any further messages
        TickType_t base = xTaskGetTickCount();
        for (int chirp = 0; chirp < CHIRP_BURST_COUNT; chirp++) {
            unsigned long long recordStart = 0;
            float peakCorr = 0;
            // Deliberately not cleared. The emitter announces each chirp as it
            // queues it, ~40 ms after the broadcast that opened window 0, so a
            // board that opens its window late gets the stamp just *before* it —
            // clearing here would throw that away and force a full-window search.
#if CHIRP_DIAG
            diagChirp = chirp;
#endif
            float delaySamples = detectChirp(audioPin, chirpTmpl, tmplEnergy, recordStart, peakCorr);

            message_data clapMessage = createClapMessage(true);
            memcpy(clapMessage.targetAddress, hostAddress, 6);
            clapMessage.payload.clap.chirpIndex = (uint8_t)chirp;

            if (delaySamples >= 0) {
                float delayS = delaySamples / CHIRP_SAMPLE_RATE;
                // clapTime: detection moment in master time domain (offset applied by createClapMessage,
                // we override with the actual detection timestamp + same offset)
                long long detectionTime = (long long)recordStart
                                          + (long long)llroundf(delaySamples * (float)intervalUs)
                                          + ledInstance->getTimerOffset();
                clapMessage.payload.clap.clapTime = (unsigned long long)detectionTime;
                clapMessage.payload.clap.clapHappened = true;
                ESP_LOGI("CLAP", "Chirp %d peak at delay %.2f samples (%.1f ms) = %.2f m, %.1fx noise",
                         chirp, delaySamples, delayS * 1000.0f, delayS * SOUND_SPEED, peakCorr);
                // heard it: two red flashes
                message_animation flash = ledInstance->createFlash(esp_timer_get_time(), 300, 2, 0, 255, 255);
                ledInstance->pushToAnimationQueue(flash);
            } else {
                clapMessage.payload.clap.clapHappened = false;
                ESP_LOGI("CLAP", "Chirp %d: nothing heard (strongest %.1fx noise)", chirp, peakCorr);
                // listened and heard nothing: one blue flash, so a board that takes
                // part shows it even when it hears nothing
                message_animation flash = ledInstance->createFlash(esp_timer_get_time(), 150, 1, 160, 255, 255);
                ledInstance->pushToAnimationQueue(flash);
            }
            pushToSendQueue(clapMessage);
            // absolute schedule off the base, a slow correlation costs one window
            // instead of shifting every window after it
            TickType_t target = base + pdMS_TO_TICKS((chirp + 1) * CHIRP_BURST_PERIOD_MS);
            int32_t wait = (int32_t)(target - xTaskGetTickCount());
            if (wait > 0) vTaskDelay(wait);
        }
        ESP_LOGI("CLAP", "Chirp burst done");

    } else {
        // original peak-detection clap mode
        ESP_LOGI("CLAP", "Starting clap detection task");
        PeakDetection peakDetection;
        peakDetection.begin(48, 10, 0.5);
        unsigned long lastClapTime = millis();
        unsigned long lastPing = millis();
        while (true) {
            double data = (double)analogRead(audioPin) / 512 - 1;
            peakDetection.add(data);
            int peak = peakDetection.getPeak();
            double filtered = peakDetection.getFilt();
            if ((peak == -1 || (millis() - lastClapTime > CLAP_TIMEOUT)) && (getHasClapHappened() == true || (getCalibrationTest() == true && millis() - lastClapTime > 1000))) {
                message_data clapMessage = createClapMessage(true);
                message_animation animation = ledInstance->createFlash(millis(), 300, 2, 0, 255, 255);
                ledInstance->pushToAnimationQueue(animation);
                if (millis() - lastClapTime > CLAP_TIMEOUT) {
                    clapMessage.payload.clap.clapHappened = false;
                } else {
                    ESP_LOGI("CLAP", "Clap detected with peak: %d, filtered: %.2f", peak, filtered);
                    clapMessage.payload.clap.clapHappened = true;
                }
                lastClapTime = millis();
                memcpy(clapMessage.targetAddress, hostAddress, 6);
                pushToSendQueue(clapMessage);
                setHasClapHappened(false);
                clapTaskHandle = nullptr;
                vTaskDelete(NULL);
                ESP_LOGI("CLAP", "Clap task finished");
            }
            if (millis() - lastPing > 1000) {
                lastPing = millis();
            }
            taskYIELD();
        }
    }

    clapTaskHandle = nullptr;
    vTaskDelete(NULL);
}

// Listen for half a second and report what the microphone actually produced.
// A dead mic reads a flat line, a broken bias network sits on a rail, and a
// healthy one hovers near mid-scale with a few counts of room noise — which is
// the difference between "the mic is broken" and "the chirp never got here".
void MessageHandler::runMicTest() {
    const int pin = AUDIO_PIN;
    const int rate = 10000;              // same as the chirp recorder
    const int wanted = rate / 2;         // half a second
    pinMode(pin, INPUT);

    uint16_t lo = 4095, hi = 0;
    uint64_t sum = 0, sumsq = 0;
    unsigned long long next = esp_timer_get_time();
    const unsigned long long interval = 1000000ULL / rate;
    for (int i = 0; i < wanted; i++) {
        while (esp_timer_get_time() < next) {}
        uint16_t v = (uint16_t)analogRead(pin);
        if (v < lo) lo = v;
        if (v > hi) hi = v;
        sum += v;
        sumsq += (uint64_t)v * v;
        next += interval;
    }

    uint16_t mean = (uint16_t)(sum / wanted);
    double var = (double)sumsq / wanted - (double)mean * mean;
    if (var < 0) var = 0;

    message_data msg;
    msg.messageType = MSG_MIC_TEST;
    memcpy(msg.targetAddress, hostAddress, 6);
    msg.payload.micTest.minLevel  = lo;
    msg.payload.micTest.maxLevel  = hi;
    msg.payload.micTest.meanLevel = mean;
    msg.payload.micTest.rms       = (uint16_t)sqrt(var);
    msg.payload.micTest.samples   = (uint16_t)wanted;
    ESP_LOGI("MIC", "min %u max %u mean %u rms %u", lo, hi, mean, msg.payload.micTest.rms);
    pushToSendQueue(msg);

    micTestTaskHandle = nullptr;
    vTaskDelete(NULL);
}

void MessageHandler::runMicTestWrapper(void *pvParameters) {
    ((MessageHandler *)pvParameters)->runMicTest();
}

void MessageHandler::startMicTestTask() {
    if (micTestTaskHandle != nullptr) return;      // already measuring
    if (clapTaskHandle != nullptr) return;         // a calibration owns the ADC
    xTaskCreatePinnedToCore(runMicTestWrapper, "runMicTest", 4096, this, 5, &micTestTaskHandle, 1);
}

void MessageHandler::startClapTask() {
    // Two writers own this handle — the task clears it as it exits, and
    // CMD_CANCEL_CALIBRATION deletes it — so it can hold a task that is already
    // gone. vTaskDelete on that is undefined; check before touching it.
    if (clapTaskHandle != nullptr) {
        eTaskState st = eTaskGetState(clapTaskHandle);
        if (st != eDeleted && st != eInvalid) vTaskDelete(clapTaskHandle);
        clapTaskHandle = nullptr;
    }
    // 10 KB of stack, and the result was discarded. A failed allocation left no
    // task listening and nothing said so: the calibration command arrived, every
    // chirp announcement arrived, and the board simply never opened a window —
    // which reads downstream as "no board heard the chirps".
    BaseType_t r = xTaskCreatePinnedToCore(runClapTaskWrapper, "runClapTask", 10000,
                                           this, 20, &clapTaskHandle, 1);
    if (r != pdPASS) {
        clapTaskHandle = nullptr;
        ESP_LOGE("CLAP", "could not start clap task (%d), free heap %u — not listening",
                 (int)r, (unsigned)ESP.getFreeHeap());
    } else {
        ESP_LOGI("CLAP", "clap task started, chirpMode=%d, free heap %u",
                 (int)getChirpMode(), (unsigned)ESP.getFreeHeap());
    }
}

void MessageHandler::runClapTaskWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runClapTask();
}


#endif