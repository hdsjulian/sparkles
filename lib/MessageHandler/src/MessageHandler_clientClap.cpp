
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
static const float SOUND_SPEED        = 343.0f;          // m/s
static const float MIN_CORR_THRESHOLD = 0.15f;           // normalised, below = invalid
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

void MessageHandler::runClapTask() {
    int audioPin = 5;
    pinMode(audioPin, INPUT);

    if (getTestMode()) {
        // chirp cross-correlation mode
        ESP_LOGI("CLAP", "Chirp mode: recording %d ms at %d Hz", RECORD_MS, CHIRP_SAMPLE_RATE);

        static int16_t  recBuf[RECORD_SAMPLES];
        static float    chirpTmpl[CHIRP_SAMPLES];
        buildChirpTemplate(chirpTmpl);

        unsigned long long startTime = esp_timer_get_time();
        unsigned long long nextSample = startTime;
        const unsigned long long intervalUs = 1000000ULL / CHIRP_SAMPLE_RATE; // 100 µs

        for (int i = 0; i < RECORD_SAMPLES; i++) {
            while (esp_timer_get_time() < nextSample) {}
            recBuf[i] = (int16_t)(analogRead(audioPin) - 2048);
            nextSample += intervalUs;
        }
        ESP_LOGI("CLAP", "Recording done, correlating...");

        // Normalise template energy once
        float tmplEnergy = 0;
        for (int k = 0; k < CHIRP_SAMPLES; k++) tmplEnergy += chirpTmpl[k] * chirpTmpl[k];

        // Slide template over recording, skip first CHIRP_SAMPLES (direct path / pre-signal)
        static const int CORR_OFFSET = CHIRP_SAMPLES;
        static const int CORR_COUNT  = RECORD_SAMPLES - 2 * CHIRP_SAMPLES + 1;
        static float corrBuf[CORR_COUNT];

        float bestCorr = 0;
        for (int d = 0; d < CORR_COUNT; d++) {
            float corr = 0;
            float sigEnergy = 0;
            for (int k = 0; k < CHIRP_SAMPLES; k++) {
                float s = (float)recBuf[CORR_OFFSET + d + k];
                corr      += s * chirpTmpl[k];
                sigEnergy += s * s;
            }
            // Normalise by signal energy to make threshold signal-level independent
            corrBuf[d] = (sigEnergy > 0) ? (corr / sqrtf(tmplEnergy * sigEnergy)) : 0;
            if (corrBuf[d] > bestCorr) bestCorr = corrBuf[d];
        }

        // a reflection can correlate stronger than the direct path, which always
        // arrives first, so take the earliest local peak close to the global max
        int peakIdx = -1;
        float accept = bestCorr * FIRST_PEAK_RATIO;
        for (int d = 1; d < CORR_COUNT - 1; d++) {
            if (corrBuf[d] >= accept && corrBuf[d] >= corrBuf[d - 1] && corrBuf[d] >= corrBuf[d + 1]) {
                peakIdx = d;
                break;
            }
        }

        message_data clapMessage = createClapMessage(true);
        memcpy(clapMessage.targetAddress, hostAddress, 6);

        if (peakIdx >= 0 && corrBuf[peakIdx] >= MIN_CORR_THRESHOLD) {
            // parabolic fit through the peak for sub-sample resolution (one sample = 3.4 cm)
            float c0 = corrBuf[peakIdx - 1], c1 = corrBuf[peakIdx], c2 = corrBuf[peakIdx + 1];
            float denom = c0 - 2.0f * c1 + c2;
            float frac  = (denom < -1e-9f) ? 0.5f * (c0 - c2) / denom : 0.0f;
            frac = constrain(frac, -0.5f, 0.5f);
            float delaySamples = (float)(CORR_OFFSET + peakIdx) + frac;
            float delayS    = delaySamples / CHIRP_SAMPLE_RATE;
            float distanceM = delayS * SOUND_SPEED;
            // clapTime: detection moment in master time domain (offset applied by createClapMessage,
            // we override with the actual detection timestamp + same offset)
            long long detectionTime = (long long)startTime
                                      + (long long)llroundf(delaySamples * (float)intervalUs)
                                      + ledInstance->getTimerOffset();
            clapMessage.payload.clap.clapTime = (unsigned long long)detectionTime;
            clapMessage.payload.clap.clapHappened = true;
            ESP_LOGI("CLAP", "Chirp peak at delay %.2f samples (%.1f ms) = %.2f m, corr=%.3f",
                     delaySamples, delayS * 1000.0f, distanceM, corrBuf[peakIdx]);
        } else {
            clapMessage.payload.clap.clapHappened = false;
            ESP_LOGI("CLAP", "Chirp: no valid peak (best corr=%.3f)", bestCorr);
        }

        message_animation flash = ledInstance->createFlash(esp_timer_get_time(), 300, 2, 0, 255, 255);
        ledInstance->pushToAnimationQueue(flash);
        pushToSendQueue(clapMessage);

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