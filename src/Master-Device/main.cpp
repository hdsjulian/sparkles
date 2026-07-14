#include <Arduino.h>
#include <MyDefines.h>
#include <esp_log.h>
#include "esp_now.h"
#include <LittleFS.h>
#include "WiFi.h"
#include <LedHandler.h>
#include <MessageHandler.h>
#include <Version.h>
#include <ArduinoJson.h>
#include "esp_sleep.h"
#include "soc/rtc.h"

LedHandler& ledInstance = LedHandler::getInstance();
MessageHandler& msgHandler = MessageHandler::getInstance();
uint8_t myAddress[6];
bool g_loggingEnabled = false;

MessageHandler& getMessageHandlerInstance() { return msgHandler; }

bool lfs_started = true;

void OnDataRecv(const esp_now_recv_info *mac, const uint8_t *incomingData, int len) {
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {
}

unsigned long lastTick = 0;

#define SLEEP_BROADCAST_INTERVAL_MS 1000
#define SLEEP_BROADCAST_DURATION_MS (5 * 60 * 1000)  // 5 minutes in ms

static TaskHandle_t sleepBroadcastTaskHandle = NULL;
static void serialSendDoc(JsonDocument& doc);

static void sleepBroadcastTask(void* pvParameters) {
    unsigned long long durationMicros = (unsigned long long)SLEEP_BROADCAST_DURATION_MS * 1000ULL;

    {
        JsonDocument r; r["event"] = "sleep_phase"; r["status"] = "start";
        serialSendDoc(r);
    }

    // Resync all clients once before sending sleep so they wake up with aligned timers.
    msgHandler.startFastResyncTask();
    while (msgHandler.isFastResyncRunning()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    while (msgHandler.isInSleepPhase()) {
        msgHandler.sendSleepWakeupMessage(durationMicros);
        vTaskDelay(pdMS_TO_TICKS(SLEEP_BROADCAST_INTERVAL_MS));
    }
    // Sleep phase ended — reannounce so clients that are awake can re-pair
    msgHandler.setAddressListInactive();
    msgHandler.startBroadcastSettleTask();
    msgHandler.broadcastReannounce();
    {
        JsonDocument r; r["event"] = "sleep_phase"; r["status"] = "end";
        serialSendDoc(r);
    }
    sleepBroadcastTaskHandle = NULL;
    vTaskDelete(NULL);
}

// ── Serial bridge ─────────────────────────────────────────────────────────────

// fixed-size line buffer, no String churn on the hot path (same as Music-Device had)
static char serialLineBuffer[256];
static size_t serialLineLen = 0;

static void serialSendDoc(JsonDocument& doc) {
    String out;
    serializeJson(doc, out);
    Serial.println(out);
}

static void handleSerialCommand(const char* line) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, line);
    if (err) return;

    const char* cmd = doc["cmd"] | "";

    if (strcmp(cmd, "set_log_level") == 0) {
        int level = doc["level"] | 0;
        esp_log_level_set("*", (esp_log_level_t)level);
        JsonDocument r; r["event"] = "log_level"; r["level"] = level;
        serialSendDoc(r);

    } else if (strcmp(cmd, "animate_toggle") == 0) {
        bool running = msgHandler.isAnimationLoopRunning();
        running ? msgHandler.stopAllAnimations() : msgHandler.startAnimationLoopTask();
        JsonDocument r;
        r["event"] = "animate_status";
        r["status"] = !running;
        serialSendDoc(r);

    } else if (strcmp(cmd, "animation_off") == 0) {
        msgHandler.stopAllAnimations();

    } else if (strcmp(cmd, "get_animate_status") == 0) {
        JsonDocument r;
        r["event"]  = "animate_status";
        r["status"] = msgHandler.isAnimationLoopRunning();
        serialSendDoc(r);

    } else if (strcmp(cmd, "blink") == 0) {
        message_animation a{};
        a.animationType = BLINK;
        a.animationParams.blink.brightness  = 255;
        a.animationParams.blink.duration    = 500;
        a.animationParams.blink.repetitions = 3;
        a.animationParams.blink.startTime   = esp_timer_get_time() + 100000;
        msgHandler.sendAnimation(a, doc["boardId"].as<int>());

    } else if (strcmp(cmd, "blink_all") == 0) {
        message_animation a{};
        a.animationType = BLINK;
        a.animationParams.blink.brightness  = 255;
        a.animationParams.blink.duration    = 500;
        a.animationParams.blink.repetitions = 3;
        a.animationParams.blink.startTime   = esp_timer_get_time() + 100000;
        msgHandler.sendAnimation(a, -1);

    } else if (strcmp(cmd, "blink_battery_all") == 0) {
        message_animation a{};
        a.animationType = BATTERY_BLINK;
        a.animationParams.blink.brightness  = 255;
        a.animationParams.blink.duration    = 500;
        a.animationParams.blink.repetitions = 3;
        a.animationParams.blink.startTime   = 0;
        msgHandler.sendAnimation(a, -1);

    } else if (strcmp(cmd, "strobe_all") == 0) {
        message_animation a{};
        a.animationType = STROBE;
        a.animationParams.strobe.frequency  = doc["frequency"].as<int>();
        a.animationParams.strobe.duration   = doc["duration"].as<int>();
        a.animationParams.strobe.hue        = doc["hue"].as<int>();
        a.animationParams.strobe.saturation = doc["saturation"].as<int>();
        a.animationParams.strobe.brightness = doc["brightness"].as<int>();
        a.animationParams.strobe.startTime  = esp_timer_get_time() + 1000000;
        msgHandler.sendAnimation(a, -1);

    } else if (strcmp(cmd, "sync") == 0) {
        msgHandler.setCurrentTimerIndex(doc["index"].as<int>());
        msgHandler.startTimerSyncTask();

    } else if (strcmp(cmd, "sync_all") == 0) {
        msgHandler.startAllTimerSyncTask();

    } else if (strcmp(cmd, "sync_fast") == 0) {
        msgHandler.startFastResyncTask();

    } else if (strcmp(cmd, "timer_test") == 0) {
        // one query is a single NTP sample, +-rtt/2 asymmetry noise. burst per
        // board, the UI keeps the min-rtt sample as the trustworthy one.
        xTaskCreatePinnedToCore([](void* pv) {
            MessageHandler& mh = *((MessageHandler*)pv);
            for (int i = 0; i < NUM_DEVICES; i++) {
                client_address a = mh.getItemFromAddressList(i);
                if (memcmp(a.address, MessageHandler::emptyAddress, 6) == 0) break;
                if (a.active != ACTIVE) continue;
                mh.addPeer(a.address);
                for (int s = 0; s < 8; s++) {
                    message_data q{};
                    q.messageType = MSG_TIMER_QUERY;
                    memcpy(q.targetAddress, a.address, 6);
                    WiFi.macAddress(q.senderAddress);
                    a = mh.getItemFromAddressList(i);
                    a.timerQuerySendTime = esp_timer_get_time();
                    mh.setItemFromAddressList(i, a);
                    esp_now_send(a.address, (uint8_t*)&q, ESPNOW_CLIENT_COMPAT_SIZE);
                    vTaskDelay(pdMS_TO_TICKS(40));
                }
                mh.removePeer(a.address);
            }
            vTaskDelete(NULL);
        }, "timerTest", 4096, &msgHandler, 2, NULL, 1);

    } else if (strcmp(cmd, "submit_positions") == 0) {
        msgHandler.setBoardPosition(doc["boardId"].as<int>(), doc["xpos"].as<float>(), doc["ypos"].as<float>());

    } else if (strcmp(cmd, "set_time") == 0) {
        struct tm t{};
        t.tm_year = doc["year"].as<int>() - 1900;
        t.tm_mon  = doc["month"].as<int>() - 1;
        t.tm_mday = doc["day"].as<int>();
        t.tm_hour = doc["hours"].as<int>();
        t.tm_min  = doc["minutes"].as<int>();
        t.tm_sec  = doc["seconds"].as<int>();
        struct timeval tv{ mktime(&t), 0 };
        settimeofday(&tv, NULL);
        setenv("TZ", "UTC", 1);
        tzset();

    } else if (strcmp(cmd, "set_sleep_time") == 0) {
        msgHandler.setSleepTime(doc["hours"].as<int>(), doc["minutes"].as<int>(), doc["seconds"].as<int>());

    } else if (strcmp(cmd, "set_wakeup_time") == 0) {
        msgHandler.setWakeupTime(doc["hours"].as<int>(), doc["minutes"].as<int>(), doc["seconds"].as<int>());

    } else if (strcmp(cmd, "get_address_list") == 0) {
        for (int i = 0; i < NUM_DEVICES; i++) {
            client_address a = msgHandler.getItemFromAddressList(i);
            if (memcmp(a.address, MessageHandler::emptyAddress, 6) == 0) break;
            JsonDocument r;
            r["event"] = "update_board";
            r["id"]    = i;
            char mac[18];
            snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                a.address[0], a.address[1], a.address[2],
                a.address[3], a.address[4], a.address[5]);
            r["address"]           = mac;
            r["status"]            = (a.active == ACTIVE) ? "active" : "inactive";
            r["batteryPercentage"] = a.batteryPercentage;
            r["distance"]          = a.distanceFromCenter;
            r["xpos"]              = a.xPos;
            r["ypos"]              = a.yPos;
            r["lastUpdateTime"]    = (unsigned long)a.lastUpdateTime;
            r["timerOffset"]       = (long)a.timerOffset;
            r["delay"]             = a.delay;
            serialSendDoc(r);
        }
        JsonDocument summary;
        summary["event"]      = "address_list";
        summary["numDevices"] = msgHandler.getNumDevices();
        serialSendDoc(summary);

    } else if (strcmp(cmd, "get_midi_params") == 0) {
        message_midi_params p = msgHandler.getMidiParams();
        JsonDocument r;
        r["minVal"] = p.valMin;   r["maxVal"] = p.valMax;
        r["minSat"] = p.satMin;   r["maxSat"] = p.satMax;
        r["hue"]    = p.hue;      r["saturation"] = p.saturation;
        r["rangeMin"] = p.rangeMin; r["rangeMax"] = p.rangeMax;
        r["minDb"]  = p.rmsMin;   r["maxDb"] = p.rmsMax;
        r["distance"] = p.distance; r["distanceSwitch"] = p.distanceSwitch;
        r["distanceMode"] = p.distanceMode; r["mode"] = p.mode;
        r["event"] = "midi_params";
        serialSendDoc(r);

    } else if (strcmp(cmd, "set_midi_params") == 0) {
        msgHandler.setMidiParams(
            doc["minVal"].as<int>(), doc["maxVal"].as<int>(),
            doc["minSat"].as<int>(), doc["maxSat"].as<int>(),
            doc["midiHue"].as<int>(), doc["midiSaturation"].as<int>(),
            doc["rangeMin"].as<int>(), doc["rangeMax"].as<int>(),
            doc["minDb"].as<float>(), doc["maxDb"].as<float>(),
            doc["mode"].as<int>(), doc["distance"].as<int>(),
            doc["distanceSwitch"].as<bool>(), doc["distanceMode"].as<int>());

    } else if (strcmp(cmd, "get_darkroom_params") == 0) {
        message_darkroom_params p = msgHandler.getDarkroomParams();
        JsonDocument r;
        r["event"] = "darkroom_params";
        r["strobeMin"] = p.strobeMin; r["strobeMax"] = p.strobeMax;
        r["redlightMin"] = p.redlightMin; r["redlightMax"] = p.redlightMax;
        r["candlelightMin"] = p.candlelightMin; r["candlelightMax"] = p.candlelightMax;
        serialSendDoc(r);

    } else if (strcmp(cmd, "set_darkroom_params") == 0) {
        msgHandler.setDarkroomParams(
            doc["strobeMin"].as<int>(), doc["strobeMax"].as<int>(),
            doc["redlightMin"].as<int>(), doc["redlightMax"].as<int>(),
            doc["candlelightMin"].as<int>(), doc["candlelightMax"].as<int>(),
            doc["redLightEnabled"].as<bool>(), doc["candleLightEnabled"].as<bool>());

    } else if (strcmp(cmd, "get_system_info") == 0) {
        struct tm ti;
        char buf[32] = "not set";
        // timeout 0: the default blocks 5s while the clock is unset, tripping the loop() watchdog
        if (getLocalTime(&ti, 0)) snprintf(buf, sizeof(buf), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
        uint8_t mac[6]; WiFi.macAddress(mac);
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        JsonDocument r;
        r["event"]        = "system_info";
        r["systemTime"]   = buf;
        r["macAddress"]   = macStr;
        r["sleepSet"]     = msgHandler.isSleepSet();
        r["sleepIn"]      = (long)(msgHandler.getSleepTime() / 1000);
        r["sleepDuration"]= (long)(msgHandler.getSleepDuration() / 1000);
        r["testMode"]     = msgHandler.getTestMode();
        serialSendDoc(r);

    } else if (strcmp(cmd, "calibration_start") == 0)    { msgHandler.startCalibrationMaster(); }
    else if (strcmp(cmd, "calibration_cancel") == 0)     { msgHandler.cancelCalibration(); }
    else if (strcmp(cmd, "calibration_reset") == 0)      { msgHandler.resetCalibration(); }
    else if (strcmp(cmd, "calibration_continue") == 0)   { msgHandler.continueCalibration(doc["x"].as<float>(), doc["y"].as<float>()); }
    else if (strcmp(cmd, "calibration_end") == 0)        { msgHandler.endCalibration(); }
    else if (strcmp(cmd, "calibration_test") == 0)       { msgHandler.testCalibration(); }
    else if (strcmp(cmd, "calibration_calibrate") == 0)  { msgHandler.commandCalibrate(doc["boardId"].as<int>()); }
    else if (strcmp(cmd, "dist_cal_start") == 0)         { msgHandler.startDistanceCalibrationMaster(); }
    else if (strcmp(cmd, "dist_cal_continue") == 0)      { msgHandler.continueDistanceCalibration(); }
    else if (strcmp(cmd, "dist_cal_end") == 0)           { msgHandler.endDistanceCalibration(); }
    else if (strcmp(cmd, "dist_cal_cancel") == 0)        { msgHandler.cancelCalibration(); }
    else if (strcmp(cmd, "dist_cal_abort") == 0)         { msgHandler.abortDistanceCalibration(); }
    else if (strcmp(cmd, "ota_update") == 0)             { msgHandler.startOTAUpdateTask(); }
    else if (strcmp(cmd, "set_ota_url") == 0) {
        const char* url = doc["url"] | "";
        msgHandler.setOtaUrl(url);
    }
    else if (strcmp(cmd, "reannounce") == 0)             { msgHandler.broadcastReannounce(); }
    else if (strcmp(cmd, "reset_system") == 0)           { msgHandler.resetSystem(); }

    else if (strcmp(cmd, "toggle_test_mode") == 0) {
        bool next = !msgHandler.getTestMode();
        float spacing = doc["spacing"] | 1.0f;
        msgHandler.setTestMode(next, spacing);
        JsonDocument r;
        r["event"]    = "test_mode";
        r["testMode"] = next;
        serialSendDoc(r);

    } else if (strcmp(cmd, "toggle_logging") == 0) {
        g_loggingEnabled = !g_loggingEnabled;
        JsonDocument r;
        r["event"]   = "logging";
        r["logging"] = g_loggingEnabled;
        serialSendDoc(r);

    } else if (strcmp(cmd, "command_message") == 0) {
        int boardId = doc["boardId"].as<int>();
        message_data msg = msgHandler.createCommandMessage(CMD_MESSAGE, false);
        memcpy(msg.targetAddress, msgHandler.getItemFromAddressList(boardId).address, 6);
        msgHandler.pushToSendQueue(msg);

    } else if (strcmp(cmd, "factory_reset") == 0) {
        if (LittleFS.exists("/clientAddress")) LittleFS.remove("/clientAddress");
        ESP.restart();

    } else if (strcmp(cmd, "set_maintenance_mode") == 0) {
        bool active = doc["active"].as<bool>();
        msgHandler.setAdminPresent(active ? millis() : 0);
        if (!active) {
            // tell all clients to stop shimmering
            message_animation stopAnim;
            stopAnim.animationType = OFF;
            msgHandler.sendAnimation(stopAnim, -1);
        }
        JsonDocument r;
        r["event"]  = "maintenance_mode";
        r["active"] = active;
        serialSendDoc(r);

    } else if (strcmp(cmd, "shimmer") == 0) {
        int boardId = doc["boardId"] | -1;
        message_animation anim = ledInstance.createCandle(esp_timer_get_time() + 100000, 30000, 30, 80, 30);
        msgHandler.sendAnimation(anim, boardId);

    } else if (strcmp(cmd, "bioluminescence") == 0) {
        message_animation anim;
        anim.animationType = BIOLUMINESCENCE;
        anim.animationParams.bioluminescence.minInterval  = doc["minInterval"]  | 2000;
        anim.animationParams.bioluminescence.maxInterval  = doc["maxInterval"]  | 8000;
        anim.animationParams.bioluminescence.fadeDuration = doc["fadeDuration"] | 1500;
        anim.animationParams.bioluminescence.repetitions  = doc["repetitions"]  | 0;
        anim.animationParams.bioluminescence.hue          = doc["hue"]          | 140;
        anim.animationParams.bioluminescence.hueVariance  = doc["hueVariance"]  | 20;
        anim.animationParams.bioluminescence.saturation   = doc["saturation"]   | 220;
        anim.animationParams.bioluminescence.brightness   = doc["brightness"]   | 80;
        msgHandler.sendAnimation(anim, -1);

    } else if (strcmp(cmd, "breath") == 0) {
        message_animation anim;
        anim.animationType = BREATH;
        anim.animationParams.breath.startTime   = esp_timer_get_time() + 2000000ULL;
        anim.animationParams.breath.cycleDuration = doc["cycleDuration"] | 4000;
        anim.animationParams.breath.spreadDelay   = doc["spreadDelay"]   | 2000;
        anim.animationParams.breath.repetitions   = doc["repetitions"]   | 0;
        anim.animationParams.breath.hue           = doc["hue"]           | 96;
        anim.animationParams.breath.saturation    = doc["saturation"]    | 180;
        anim.animationParams.breath.brightness    = doc["brightness"]    | 200;
        msgHandler.sendAnimation(anim, -1);
    } else if (strcmp(cmd, "candle_all") == 0) {
        message_animation anim;
        anim.animationType = CANDLE;
        anim.animationParams.candle.startTime  = esp_timer_get_time() + 500000ULL;
        anim.animationParams.candle.duration   = 0; // 0 = loop forever
        anim.animationParams.candle.hue        = doc["hue"]        | 20;
        anim.animationParams.candle.saturation = doc["saturation"] | 210;
        anim.animationParams.candle.value      = doc["brightness"] | 180;
        msgHandler.sendAnimation(anim, -1);

    } else if (strcmp(cmd, "aubio_shimmer") == 0) {
        message_animation anim;
        anim.animationType = BACKGROUND_SHIMMER;
        anim.animationParams.backgroundShimmer.hue        = doc["hue"]        | 22;
        anim.animationParams.backgroundShimmer.saturation = doc["saturation"] | 255;
        anim.animationParams.backgroundShimmer.value      = doc["value"]      | 0;
        msgHandler.sendAnimation(anim, -1);     // direct broadcast (latency-sensitive fast path)
        msgHandler.setLastMidiTime(millis());   // keep the idle animation loop suppressed

    } else if (strcmp(cmd, "aubio_midi") == 0) {
        message_animation anim;
        anim.animationType = MIDI;
        anim.animationParams.midi.note       = doc["note"]     | 0;
        anim.animationParams.midi.velocity   = doc["velocity"] | 0;
        anim.animationParams.midi.instrument = 0; // mic
        anim.animationParams.midi.hue        = doc["hue"]        | 18;
        anim.animationParams.midi.saturation = doc["saturation"] | 102;
        msgHandler.sendAnimation(anim, -1);
        msgHandler.setLastMidiTime(millis());

    } else if (strcmp(cmd, "keyboard_midi") == 0) {
        message_animation anim;
        anim.animationType = MIDI;
        anim.animationParams.midi.note       = doc["note"]     | 0;
        anim.animationParams.midi.velocity   = doc["velocity"] | 0;
        anim.animationParams.midi.instrument = 1; // keyboard
        anim.animationParams.midi.hue        = doc["hue"]        | 18;
        anim.animationParams.midi.saturation = doc["saturation"] | 102;
        msgHandler.sendAnimation(anim, -1);
        msgHandler.setLastMidiTime(millis());

    } else if (strcmp(cmd, "identify") == 0) {
        JsonDocument r;
        r["event"] = "identity";
        r["role"]  = "master";
        serialSendDoc(r);

    } else if (strcmp(cmd, "sustain_pedal") == 0) {
        // sustain pedal: value >= 64 = down, < 64 = up
        // extend animation decay when pedal is held — placeholder for future effect

    } else if (strcmp(cmd, "test_sleep_cycle") == 0) {
        struct SleepTestParams { int sleepDurationS; int phaseDurationS; };
        auto* p = new SleepTestParams{
            doc["sleep_duration_s"] | 15,
            doc["phase_duration_s"] | 60
        };
        xTaskCreatePinnedToCore([](void* pv) {
            auto* params = (SleepTestParams*)pv;
            int sleepDurationS = params->sleepDurationS;
            int phaseDurationS = params->phaseDurationS;
            delete params;

            auto emit = [](const char* event, JsonDocument& extra) {
                extra["event"] = event;
                String out; serializeJson(extra, out); Serial.println(out);
            };

            // 1. Snapshot clients
            int total = 0;
            for (int i = 0; i < NUM_DEVICES; i++) {
                if (memcmp(msgHandler.getItemFromAddressList(i).address,
                           MessageHandler::emptyAddress, 6) == 0) break;
                total++;
            }
            { JsonDocument r; r["clients"] = total;
              r["sleep_duration_s"] = sleepDurationS;
              r["phase_duration_s"] = phaseDurationS;
              emit("sleep_test_start", r); }

            // 2. Fast resync
            unsigned long t0 = millis();
            { JsonDocument r; emit("sleep_test_resync_start", r); }
            msgHandler.startFastResyncTask();
            while (msgHandler.isFastResyncRunning())
                vTaskDelay(pdMS_TO_TICKS(100));
            { JsonDocument r; r["elapsed_ms"] = (long)(millis() - t0);
              emit("sleep_test_resync_done", r); }

            // 3. Broadcast sleep for phaseDurationS
            // Enforce minimum so clients cycle through at least 2 sleep periods
            if (phaseDurationS < sleepDurationS * 2 + 5)
                phaseDurationS = sleepDurationS * 2 + 5;

            unsigned long long durationMicros = (unsigned long long)sleepDurationS * 1000000ULL;
            unsigned long phaseStart = millis();
            int broadcasts = 0;
            int nextCycleLog = sleepDurationS; // log a heartbeat every sleepDurationS seconds
            { JsonDocument r;
              r["phase_duration_s"] = phaseDurationS;
              r["cycles_expected"] = phaseDurationS / sleepDurationS;
              emit("sleep_test_broadcast_start", r); }

            while (millis() - phaseStart < (unsigned long)phaseDurationS * 1000) {
                msgHandler.sendSleepWakeupMessage(durationMicros);
                broadcasts++;
                vTaskDelay(pdMS_TO_TICKS(1000));

                long elapsedS = (long)((millis() - phaseStart) / 1000);

                // Check for unexpected wakeups — any ACTIVE client mid-phase means
                // they woke up and didn't receive the sleep rebroadcast in time.
                for (int i = 0; i < NUM_DEVICES; i++) {
                    if (memcmp(msgHandler.getItemFromAddressList(i).address,
                               MessageHandler::emptyAddress, 6) == 0) break;
                    if (msgHandler.getActiveStatus(i) == ACTIVE) {
                        JsonDocument r;
                        r["id"] = i;
                        r["elapsed_ms"] = (long)(millis() - phaseStart);
                        emit("sleep_test_unexpected_wakeup", r);
                    }
                }

                // Heartbeat at each expected sleep cycle boundary
                if (elapsedS >= nextCycleLog) {
                    JsonDocument r;
                    r["broadcasts"] = broadcasts;
                    r["elapsed_s"] = elapsedS;
                    r["cycle"] = elapsedS / sleepDurationS;
                    emit("sleep_test_cycle_heartbeat", r);
                    nextCycleLog += sleepDurationS;
                }
            }
            { JsonDocument r;
              r["broadcasts"] = broadcasts;
              r["elapsed_ms"] = (long)(millis() - phaseStart);
              emit("sleep_test_broadcast_end", r); }

            // 4. Wake-up: mark all inactive, reannounce, wait for clients to return
            msgHandler.setAddressListInactive();
            msgHandler.broadcastReannounce();
            { JsonDocument r; emit("sleep_test_waiting_for_wakeup", r); }

            unsigned long wakeStart = millis();
            unsigned long waitMaxMs = ((unsigned long)sleepDurationS + 30) * 1000;
            int returned = 0;
            while (millis() - wakeStart < waitMaxMs) {
                // Count active clients
                int active = 0;
                for (int i = 0; i < NUM_DEVICES; i++) {
                    if (memcmp(msgHandler.getItemFromAddressList(i).address,
                               MessageHandler::emptyAddress, 6) == 0) break;
                    if (msgHandler.getActiveStatus(i) == ACTIVE) active++;
                }
                if (active > returned) {
                    returned = active;
                    JsonDocument r;
                    r["returned"] = returned;
                    r["expected"] = total;
                    r["elapsed_ms"] = (long)(millis() - wakeStart);
                    emit("sleep_test_client_back", r);
                }
                if (returned >= total) break;
                vTaskDelay(pdMS_TO_TICKS(500));
            }

            // 5. List any missing clients
            { JsonDocument r;
              r["expected"] = total;
              r["returned"] = returned;
              r["elapsed_ms"] = (long)(millis() - wakeStart);
              JsonArray missing = r["missing_ids"].to<JsonArray>();
              for (int i = 0; i < NUM_DEVICES; i++) {
                  if (memcmp(msgHandler.getItemFromAddressList(i).address,
                             MessageHandler::emptyAddress, 6) == 0) break;
                  if (msgHandler.getActiveStatus(i) != ACTIVE) missing.add(i);
              }
              r["success"] = (returned == total);
              emit("sleep_test_done", r); }

            vTaskDelete(NULL);
        }, "sleepTest", 8192, p, 1, NULL, 1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void setup()
{
    Serial.setRxBufferSize(2048); // default 256 holds ~5 music messages — a burst during a loop stall would overflow
    Serial.setTxBufferSize(4096); // default 256 truncates the 1 Hz housekeeping burst mid-line with tx timeout 0
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0); // non-blocking CDC writes — housekeeping drops bytes rather than stalling the music broadcast path
    delay(500);

    unsigned long long startTime = millis();
    while (!Serial) {
        if (millis() - startTime > 3000) break;
    }

    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        lfs_started = false;
    }

    rtc_clk_slow_src_set(RTC_SLOW_FREQ_8MD256);
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    delay(1000);
    ledInstance.setup();
    msgHandler.setup(ledInstance);
    // no webserver, the serial bridge handles all communication
    enableLoopWDT(); // if loop() stalls past the watchdog timeout, panic with a backtrace
}

void loop()
{
    // Read serial commands from Pi
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n') {
            while (serialLineLen > 0 && serialLineBuffer[serialLineLen - 1] == '\r') serialLineLen--;
            serialLineBuffer[serialLineLen] = '\0';
            if (serialLineLen > 0) {
                handleSerialCommand(serialLineBuffer);
            }
            serialLineLen = 0;
        } else if (serialLineLen < sizeof(serialLineBuffer) - 1) {
            serialLineBuffer[serialLineLen++] = c;
        } else {
            serialLineLen = 0; // overrun, drop the line
        }
    }

    if (lastTick + 5000 < millis()) {
        lastTick = millis();
        msgHandler.tickInactiveTimeout();

        // While music is streaming, skip the housekeeping dump: building and
        // serializing all those JSON docs stalls loop() for several ms, which
        // delays the 30 Hz music parsing. Status keeps accumulating and the
        // dump resumes 30 s after the last note.
        bool musicActive = msgHandler.getLastMidiTime() > 0 &&
                           millis() - msgHandler.getLastMidiTime() < 30000;
        if (!musicActive) {

        // Send animate status to Pi so it stays in sync
        {
            JsonDocument r;
            r["event"]  = "animate_status";
            r["status"] = msgHandler.isAnimationLoopRunning();
            serialSendDoc(r);
        }

        // Heap report, watch for a downward trend that ends in a hang
        {
            JsonDocument r;
            r["event"] = "heap";
            r["free"]  = ESP.getFreeHeap();
            r["min"]   = ESP.getMinFreeHeap();
            serialSendDoc(r);
        }

        // Send all known board states to Pi
        for (int i = 0; i < NUM_DEVICES; i++) {
            client_address a = msgHandler.getItemFromAddressList(i);
            if (memcmp(a.address, MessageHandler::emptyAddress, 6) == 0) break;
            JsonDocument r;
            r["event"] = "update_board";
            r["id"]    = i;
            char mac[18];
            snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                a.address[0], a.address[1], a.address[2],
                a.address[3], a.address[4], a.address[5]);
            r["address"]           = mac;
            r["status"]            = (a.active == ACTIVE) ? "active" : "inactive";
            r["batteryPercentage"] = a.batteryPercentage;
            r["distance"]          = a.distanceFromCenter;
            r["xpos"]              = a.xPos;
            r["ypos"]              = a.yPos;
            r["lastUpdateTime"]    = (unsigned long)a.lastUpdateTime;
            r["timerOffset"]       = (long)a.timerOffset;
            r["delay"]             = a.delay;
            serialSendDoc(r);
        }

        {
            JsonDocument r;
            r["event"]      = "num_devices";
            r["numDevices"] = msgHandler.getNumDevices();
            serialSendDoc(r);
        }

        } // !musicActive

        if (millis() - msgHandler.getLastMidiTime() > 60000 && msgHandler.getLastMidiTime() > 0) {
            ESP_LOGI("MSG", "No MIDI message for 60 seconds, starting animation loop");
            msgHandler.startAnimationLoopTask();
            msgHandler.setLastMidiTime(0);
        }
    }

    if (msgHandler.isInSleepPhase() && sleepBroadcastTaskHandle == NULL) {
        xTaskCreatePinnedToCore(sleepBroadcastTask, "sleepBroadcast", 4096, NULL, 1, &sleepBroadcastTaskHandle, 1);
    }
}
