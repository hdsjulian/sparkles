
#include "MessageHandler.h"
#if (DEVICE_MODE == CLIENT)
#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"
#include <PeakDetection.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Chirp template — 8 frequency steps × 3 ms each at 10 kHz sample rate.
// Frequencies chosen to be pseudo-random and well-separated from voice/wind.
// ---------------------------------------------------------------------------
static const int   CHIRP_SAMPLE_RATE  = 10000;           // Hz
static const int   CHIRP_STEP_MS      = 3;               // ms per frequency step
static const int   CHIRP_STEPS        = 8;
static const int   SAMPLES_PER_STEP   = CHIRP_SAMPLE_RATE * CHIRP_STEP_MS / 1000; // 30
static const int   CHIRP_SAMPLES      = CHIRP_STEPS * SAMPLES_PER_STEP;           // 240
static const int   RECORD_MS          = 220;             // > round-trip for 30 m
static const int   RECORD_SAMPLES     = CHIRP_SAMPLE_RATE * RECORD_MS / 1000;     // 2200
static const float SOUND_SPEED        = 343.0f;          // m/s
static const float MIN_CORR_THRESHOLD = 0.15f;           // normalised, below = invalid

// Frequencies in Hz for each step (1–2 kHz range, crappy-mic friendly)
static const int CHIRP_FREQS[CHIRP_STEPS] = {1000, 1700, 1200, 2000, 1500, 1100, 1800, 1300};

static void buildChirpTemplate(float* tmpl) {
    for (int step = 0; step < CHIRP_STEPS; step++) {
        float freq = (float)CHIRP_FREQS[step];
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
        // ---- Chirp cross-correlation mode ----
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

        float bestCorr = 0;
        int   bestDelay = 0;
        // Slide template over recording, skip first CHIRP_SAMPLES (direct path / pre-signal)
        for (int d = CHIRP_SAMPLES; d <= RECORD_SAMPLES - CHIRP_SAMPLES; d++) {
            float corr = 0;
            float sigEnergy = 0;
            for (int k = 0; k < CHIRP_SAMPLES; k++) {
                float s = (float)recBuf[d + k];
                corr      += s * chirpTmpl[k];
                sigEnergy += s * s;
            }
            // Normalise by signal energy to make threshold signal-level independent
            float normCorr = (sigEnergy > 0) ? (corr / sqrtf(tmplEnergy * sigEnergy)) : 0;
            if (normCorr > bestCorr) {
                bestCorr  = normCorr;
                bestDelay = d;
            }
        }

        message_data clapMessage = createClapMessage(true);
        memcpy(clapMessage.targetAddress, hostAddress, 6);

        if (bestCorr >= MIN_CORR_THRESHOLD) {
            float delayS    = (float)bestDelay / CHIRP_SAMPLE_RATE;
            float distanceM = delayS * SOUND_SPEED;
            // clapTime: detection moment in master time domain (offset applied by createClapMessage,
            // we override with the actual detection timestamp + same offset)
            long long detectionTime = (long long)(startTime + (unsigned long long)(bestDelay * intervalUs))
                                      + ledInstance->getTimerOffset();
            clapMessage.payload.clap.clapTime = (unsigned long long)detectionTime;
            clapMessage.payload.clap.clapHappened = true;
            ESP_LOGI("CLAP", "Chirp peak at delay %d samples (%.1f ms) = %.2f m, corr=%.3f",
                     bestDelay, delayS * 1000.0f, distanceM, bestCorr);
        } else {
            clapMessage.payload.clap.clapHappened = false;
            ESP_LOGI("CLAP", "Chirp: no valid peak (best corr=%.3f)", bestCorr);
        }

        message_animation flash = ledInstance->createFlash(esp_timer_get_time(), 300, 2, 0, 255, 255);
        ledInstance->pushToAnimationQueue(flash);
        pushToSendQueue(clapMessage);

    } else {
        // ---- Original peak-detection / clap mode ----
        ESP_LOGI("CLAP", "Starting clap detection task");
        PeakDetection peakDetection;
        peakDetection.begin(48, 10, 0.5);
        unsigned long lastClapTime = millis();
        unsigned long lastPing = millis();
        ESP_LOGI("CLAP", "Started at %lu", lastClapTime);
        ESP_LOGI("CLAP", "Has clap happened: %d", getHasClapHappened());
        while (true) {
            double data = (double)analogRead(audioPin) / 512 - 1;
            peakDetection.add(data);
            int peak = peakDetection.getPeak();
            double filtered = peakDetection.getFilt();
            if ((peak == -1 || (millis() - lastClapTime > CLAP_TIMEOUT)) && (getHasClapHappened() == true || (getCalibrationTest() == true && millis() - lastClapTime > 1000))) {
                ESP_LOGI("CLAP", "Clap happened? %d", (int)getHasClapHappened());
                message_data clapMessage = createClapMessage(true);
                message_animation animation = ledInstance->createFlash(millis(), 300, 2, 0, 255, 255);
                ledInstance->pushToAnimationQueue(animation);
                if (millis() - lastClapTime > CLAP_TIMEOUT) {
                    ESP_LOGI("CLAP", "No clap detected, resetting hasClapHappened");
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
                ESP_LOGI("CLAP", "Has Clap Happened: %d", getHasClapHappened());
                ESP_LOGI("CLAP", "Last Clap Time: %lu", lastClapTime);
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