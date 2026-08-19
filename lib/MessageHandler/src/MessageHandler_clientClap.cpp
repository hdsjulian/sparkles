
#include "MessageHandler.h"
#if (DEVICE_MODE == CLIENT)
#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"
#include <PeakDetection.h>
#include <math.h>

// Chirp correlation template. Step count, length and frequencies come from
// MyDefines.h so they always match what the Chirp-Device emits.
static const int   CHIRP_SAMPLE_RATE  = 10000;           // Hz
static const int   SAMPLES_PER_STEP   = CHIRP_SAMPLE_RATE * CHIRP_STEP_MS / 1000; // 30
static const int   CHIRP_SAMPLES      = CHIRP_STEPS * SAMPLES_PER_STEP;           // 240
static const int   RECORD_MS          = 220;             // > round-trip for 30 m
static const int   RECORD_SAMPLES     = CHIRP_SAMPLE_RATE * RECORD_MS / 1000;     // 2200
static const int   MAX_LAG            = RECORD_SAMPLES - CHIRP_SAMPLES;           // last window that fits
static const float SOUND_SPEED        = 343.0f;          // m/s
static const float MIN_CORR_THRESHOLD = 0.15f;           // normalised, floor under the noise gate
static const float NOISE_GATE_SIGMAS  = 5.0f;            // a real chirp beats the noise lags by this much
static const int   EMISSION_MARGIN    = 20;              // 2 ms of slack on the emission stamp
static const float FIRST_PEAK_RATIO   = 0.7f;            // earliest peak above this x global max wins

static void buildChirpTemplate(float* tmpl) {
    for (int step = 0; step < CHIRP_STEPS; step++) {
        float freq = (float)chirpFrequencies[step];
        for (int s = 0; s < SAMPLES_PER_STEP; s++) {
            int idx = step * SAMPLES_PER_STEP + s;
            tmpl[idx] = sinf(2.0f * M_PI * freq * idx / CHIRP_SAMPLE_RATE);
        }
    }
}

// Records one window and correlates it against the template. Returns the delay
// from the first sample to the chirp arrival in samples, or -1 when nothing
// plausible was heard. recordStart is the esp_timer stamp of the first sample.
float MessageHandler::detectChirp(int audioPin, const float* tmpl, float tmplEnergy,
                                  unsigned long long& recordStart, float& peakCorr) {
    static int16_t recBuf[RECORD_SAMPLES];
    static float corrBuf[MAX_LAG + 1]; // indexed by lag in samples

    const unsigned long long intervalUs = 1000000ULL / CHIRP_SAMPLE_RATE; // 100 µs
    recordStart = esp_timer_get_time();
    unsigned long long nextSample = recordStart;
    for (int i = 0; i < RECORD_SAMPLES; i++) {
        while (esp_timer_get_time() < nextSample) {}
        recBuf[i] = (int16_t)(analogRead(audioPin) - 2048);
        nextSample += intervalUs;
    }

    // Sound cannot arrive before it was emitted. The emitter announces every chirp
    // with its own timestamp just before it plays, so the search starts there
    // instead of at a fixed skip — that skip was what made close boards invisible.
    // The stamp is taken before the DAC and amp, so the real arrival is always
    // later than it; the margin only covers timer sync error.
    int minLag = 0;
    unsigned long long emission = lastChirpEmission;
    if (emission != 0) {
        long long emissionLocal = (long long)emission - ledInstance->getTimerOffset();
        long long lag = (emissionLocal - (long long)recordStart) / (long long)intervalUs - EMISSION_MARGIN;
        if (lag > 0) minLag = (lag < MAX_LAG) ? (int)lag : MAX_LAG;
    }

    float bestCorr = 0;
    float sum = 0, sumSq = 0;
    for (int d = minLag; d <= MAX_LAG; d++) {
        float corr = 0;
        float sigEnergy = 0;
        for (int k = 0; k < CHIRP_SAMPLES; k++) {
            float s = (float)recBuf[d + k];
            corr      += s * tmpl[k];
            sigEnergy += s * s;
        }
        // Normalise by signal energy to make threshold signal-level independent
        corrBuf[d] = (sigEnergy > 0) ? (corr / sqrtf(tmplEnergy * sigEnergy)) : 0;
        if (corrBuf[d] > bestCorr) bestCorr = corrBuf[d];
        sum   += corrBuf[d];
        sumSq += corrBuf[d] * corrBuf[d];
    }

    // Nearly every lag holds noise, so their spread says what a real chirp has to
    // beat. A fixed threshold can't: 0.15 sits below the largest value noise alone
    // produces over this many lags, which is a silent board reporting a distance.
    int lagCount = MAX_LAG - minLag + 1;
    float mean = sum / lagCount;
    float variance = sumSq / lagCount - mean * mean;
    float sigma = (variance > 0) ? sqrtf(variance) : 0.0f;
    float gate = fmaxf(MIN_CORR_THRESHOLD, mean + NOISE_GATE_SIGMAS * sigma);

    peakCorr = bestCorr;
    if (bestCorr < gate) return -1.0f;

    // a reflection can correlate stronger than the direct path, which always
    // arrives first, so take the earliest local peak close to the global max
    int peakIdx = -1;
    float accept = bestCorr * FIRST_PEAK_RATIO;
    // from minLag + 1: the local-max test and the parabolic fit both read d - 1,
    // and corrBuf below minLag still holds the previous window's values
    for (int d = (minLag > 0 ? minLag + 1 : 1); d < MAX_LAG; d++) {
        if (corrBuf[d] >= accept && corrBuf[d] >= corrBuf[d - 1] && corrBuf[d] >= corrBuf[d + 1]) {
            peakIdx = d;
            break;
        }
    }
    if (peakIdx < 0) return -1.0f;
    peakCorr = corrBuf[peakIdx];

    // parabolic fit through the peak for sub-sample resolution (one sample = 3.4 cm)
    float c0 = corrBuf[peakIdx - 1], c1 = corrBuf[peakIdx], c2 = corrBuf[peakIdx + 1];
    float denom = c0 - 2.0f * c1 + c2;
    float frac  = (denom < -1e-9f) ? 0.5f * (c0 - c2) / denom : 0.0f;
    frac = constrain(frac, -0.5f, 0.5f);
    return (float)peakIdx + frac;
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
            lastChirpEmission = 0; // this chirp's announce lands during its own window
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
                ESP_LOGI("CLAP", "Chirp %d peak at delay %.2f samples (%.1f ms) = %.2f m, corr=%.3f",
                         chirp, delaySamples, delayS * 1000.0f, delayS * SOUND_SPEED, peakCorr);
                message_animation flash = ledInstance->createFlash(esp_timer_get_time(), 300, 2, 0, 255, 255);
                ledInstance->pushToAnimationQueue(flash);
            } else {
                clapMessage.payload.clap.clapHappened = false;
                ESP_LOGI("CLAP", "Chirp %d: no valid peak (best corr=%.3f)", chirp, peakCorr);
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
    if (clapTaskHandle != nullptr) {
        vTaskDelete(clapTaskHandle);
        clapTaskHandle = nullptr;
    }
    xTaskCreatePinnedToCore(runClapTaskWrapper, "runClapTask", 10000, this, 20, &clapTaskHandle, 1);
}

void MessageHandler::runClapTaskWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runClapTask();
}


#endif