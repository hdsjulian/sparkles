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
#include <Preferences.h>

LedHandler& ledInstance = LedHandler::getInstance();
MessageHandler& msgHandler = MessageHandler::getInstance();
static Preferences prefs; // clock checkpoint + sleep schedule, survives reboots without the pi
uint8_t myAddress[6];
bool g_loggingEnabled = false;

static const char* animationTypeName(animationEnum type) {
    switch (type) {
        case OFF:                return "off";
        case FLASH:              return "flash";
        case BLINK:              return "blink";
        case BATTERY_BLINK:      return "battery_blink";
        case CANDLE:             return "candle";
        case SYNC_ASYNC_BLINK:   return "sync_async_blink";
        case SYNC_BLINK:         return "sync_blink";
        case SLOW_STARTUP:       return "slow_startup";
        case SYNC_END:           return "sync_end";
        case LED_ON:             return "led_on";
        case CONCENTRIC:         return "concentric";
        case MIDI:               return "midi";
        case BACKGROUND_SHIMMER: return "background_shimmer";
        case STROBE:             return "strobe";
        case BREATH:             return "breath";
        case BIOLUMINESCENCE:    return "bioluminescence";
        default:                 return "unknown";
    }
}

MessageHandler& getMessageHandlerInstance() { return msgHandler; }

bool lfs_started = true;

void OnDataRecv(const esp_now_recv_info *mac, const uint8_t *incomingData, int len) {
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {
}

unsigned long lastTick = 0;

#define SLEEP_BROADCAST_INTERVAL_MS 1000
#define SLEEP_BROADCAST_DURATION_MS (5 * 60 * 1000)  // 5 minutes in ms
#define MIDI_IDLE_ANIMATION_MS (4 * 60 * 1000)  // 4 minutes of no MIDI before the idle animation resumes

static TaskHandle_t sleepBroadcastTaskHandle = NULL;
static TaskHandle_t sleepTestTaskHandle = NULL;
static volatile bool sleepTestCancel = false;
static TaskHandle_t sleepUntilTaskHandle = NULL;
static volatile bool sleepUntilCancel = false;
// true once the wake sequence has started (cancelled or the target time was
// reached) but before the tail-end sentinel broadcast finishes — the UI
// should stop showing "sleeping" here, not 11 minutes later when the task
// object itself finally frees
static volatile bool sleepUntilWaking = false;
static void serialSendDoc(JsonDocument& doc);

// One-shot "sleep right now until HH:MM" — for the pack-up/power-cycle workflow:
// set the fleet up, then either work on it live or force it dark until showtime,
// independent of (and without touching) the recurring daily sleep/wakeup schedule.
// wrapped (if given) reports whether the target had already passed today —
// correct to roll to tomorrow for a fresh command, wrong when resuming one
// that was interrupted by a power cycle (see the suActive resume in setup())
static long secondsUntilTimeOfDay(int targetH, int targetM, int targetS, bool* wrapped = nullptr) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    time_t now = tv.tv_sec;
    struct tm *ct = localtime(&now);
    long nowSeconds = ct->tm_hour * 3600L + ct->tm_min * 60L + ct->tm_sec;
    long targetSeconds = (long)targetH * 3600L + (long)targetM * 60L + (long)targetS;
    long diff = targetSeconds - nowSeconds;
    bool didWrap = diff <= 0;
    if (didWrap) diff += 24L * 3600L; // already passed today -> tomorrow
    if (wrapped) *wrapped = didWrap;
    return diff;
}

static void sleepUntilTask(void* pvParameters) {
    int* target = (int*)pvParameters;
    int targetH = target[0], targetM = target[1], targetS = target[2];
    delete[] target;

    long totalSeconds = secondsUntilTimeOfDay(targetH, targetM, targetS);
    prefs.putInt("suH", targetH);
    prefs.putInt("suM", targetM);
    prefs.putInt("suS", targetS);
    prefs.putBool("suActive", true);
    {
        JsonDocument r; r["event"] = "sleep_until_start";
        r["targetHours"] = targetH; r["targetMinutes"] = targetM; r["targetSeconds"] = targetS;
        r["duration_s"] = totalSeconds;
        serialSendDoc(r);
    }

    msgHandler.startFastResyncTask();
    while (msgHandler.isFastResyncRunning()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(3000)); // clients still blink/settle after a resync — give them 3s before the first sleep broadcast

    unsigned long long durationMicros = (unsigned long long)SLEEP_BROADCAST_DURATION_MS * 1000ULL;
    unsigned long phaseStart = millis();
    unsigned long phaseDurationMs = (unsigned long)totalSeconds * 1000UL;
    while (!sleepUntilCancel && millis() - phaseStart < phaseDurationMs) {
        msgHandler.sendSleepWakeupMessage(durationMicros);
        vTaskDelay(pdMS_TO_TICKS(SLEEP_BROADCAST_INTERVAL_MS));
    }
    bool wasCancelled = sleepUntilCancel;
    sleepUntilCancel = false;
    sleepUntilWaking = true;
    prefs.putBool("suActive", false);

    msgHandler.setAddressListInactive();
    msgHandler.startBroadcastSettleTask();
    msgHandler.broadcastReannounce();
    {
        JsonDocument r; r["event"] = "sleep_until_done"; r["cancelled"] = wasCancelled;
        serialSendDoc(r);
    }
    // fail-closed clients need to hear "morning": broadcast the wake sentinel
    // for two full nap cycles plus margin, so even a board that misses its
    // entire first listen window gets a second full chance
    unsigned long sentinelStart = millis();
    while (millis() - sentinelStart < 2UL * SLEEP_BROADCAST_DURATION_MS + 60000UL) {
        msgHandler.sendSleepWakeupMessage(0);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    sleepUntilWaking = false;
    sleepUntilTaskHandle = NULL;
    vTaskDelete(NULL);
}

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
    vTaskDelay(pdMS_TO_TICKS(3000)); // clients still blink/settle after a resync — give them 3s before the first sleep broadcast

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
    // Clients fail closed and keep sleeping through silence, so shout "morning"
    // (zero duration = wake sentinel) for two full sleep chunks plus margin —
    // a board that misses its entire first listen window gets a second full chance.
    unsigned long sentinelStart = millis();
    while (millis() - sentinelStart < 2UL * SLEEP_BROADCAST_DURATION_MS + 60000UL) {
        msgHandler.sendSleepWakeupMessage(0);
        vTaskDelay(pdMS_TO_TICKS(2000));
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

// Full address-list dump, shared by get_address_list and anything that mutates
// the list (remove_device/remove_all_devices) — the UI clears its local device
// map and rebuilds strictly from this, so removed/shifted boards never linger.
static void emitAddressList() {
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
        r["animation"] = animationTypeName(msgHandler.getLastAnimationType());
        serialSendDoc(r);

    } else if (strcmp(cmd, "animation_off") == 0) {
        msgHandler.stopAllAnimations();

    } else if (strcmp(cmd, "get_animate_status") == 0) {
        JsonDocument r;
        r["event"]  = "animate_status";
        r["status"] = msgHandler.isAnimationLoopRunning();
        r["animation"] = animationTypeName(msgHandler.getLastAnimationType());
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
        prefs.putLong64("clock", (int64_t)tv.tv_sec);

    } else if (strcmp(cmd, "set_sleep_time") == 0) {
        msgHandler.setSleepTime(doc["hours"].as<int>(), doc["minutes"].as<int>(), doc["seconds"].as<int>());
        prefs.putInt("sleepH", doc["hours"].as<int>());
        prefs.putInt("sleepM", doc["minutes"].as<int>());
        prefs.putInt("sleepS", doc["seconds"].as<int>());

    } else if (strcmp(cmd, "set_wakeup_time") == 0) {
        msgHandler.setWakeupTime(doc["hours"].as<int>(), doc["minutes"].as<int>(), doc["seconds"].as<int>());
        prefs.putInt("wakeH", doc["hours"].as<int>());
        prefs.putInt("wakeM", doc["minutes"].as<int>());
        prefs.putInt("wakeS", doc["seconds"].as<int>());

    } else if (strcmp(cmd, "sleep_until") == 0) {
        if (sleepUntilTaskHandle != NULL) {
            JsonDocument r; r["event"] = "sleep_until_error"; r["detail"] = "already running — cancel it first";
            serialSendDoc(r);
        } else if (sleepBroadcastTaskHandle != NULL) {
            JsonDocument r; r["event"] = "sleep_until_error"; r["detail"] = "the scheduled sleep phase is already running";
            serialSendDoc(r);
        } else {
            int* target = new int[3]{ doc["hours"] | 0, doc["minutes"] | 0, doc["seconds"] | 0 };
            xTaskCreatePinnedToCore(sleepUntilTask, "sleepUntil", 4096, target, 1, &sleepUntilTaskHandle, 1);
        }

    } else if (strcmp(cmd, "sleep_until_cancel") == 0) {
        if (sleepUntilTaskHandle != NULL) sleepUntilCancel = true;

    } else if (strcmp(cmd, "get_address_list") == 0) {
        emitAddressList();

    } else if (strcmp(cmd, "remove_device") == 0) {
        int index = doc["index"] | -1;
        if (msgHandler.getSettingTimer() || msgHandler.isFastResyncRunning() || msgHandler.isAllTimerSyncRunning()) {
            JsonDocument r; r["event"] = "remove_device_error";
            r["detail"] = "a sync is in progress, try again shortly";
            serialSendDoc(r);
        } else if (msgHandler.removeDeviceAt(index)) {
            JsonDocument r; r["event"] = "device_removed"; r["index"] = index;
            serialSendDoc(r);
            emitAddressList(); // indices shift, full dump is the only correct refresh
            // every board from the removed index onward now answers to a different
            // addressId — resync so each one's own position (MIDI octave, etc.)
            // matches its new slot instead of the one it was told before the shift
            msgHandler.startFastResyncTask();
        } else {
            JsonDocument r; r["event"] = "remove_device_error"; r["detail"] = "invalid index";
            serialSendDoc(r);
        }

    } else if (strcmp(cmd, "remove_all_devices") == 0) {
        if (msgHandler.getSettingTimer() || msgHandler.isFastResyncRunning() || msgHandler.isAllTimerSyncRunning()) {
            JsonDocument r; r["event"] = "remove_device_error";
            r["detail"] = "a sync is in progress, try again shortly";
            serialSendDoc(r);
        } else {
            msgHandler.removeAllDevices();
            JsonDocument r; r["event"] = "all_devices_removed";
            serialSendDoc(r);
            emitAddressList(); // empty list + numDevices 0
        }

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
        time_t nowEpoch;
        time(&nowEpoch);
        JsonDocument r;
        r["event"]        = "system_info";
        r["systemTime"]   = buf;
        r["epoch"]        = (long long)nowEpoch; // clock negotiation: pi adopts or donates
        r["macAddress"]   = macStr;
        r["sleepSet"]     = msgHandler.isSleepSet();
        r["sleepIn"]      = (long)(msgHandler.getSleepTime() / 1000);
        r["sleepDuration"]= (long)(msgHandler.getSleepDuration() / 1000);
        r["testMode"]     = msgHandler.getTestMode();
        // "sleeping" only while actually broadcasting sleep — once the wake
        // sequence starts (cancelled or target reached) the boards are
        // already on their way up, whatever the tail-end sentinel is still doing
        bool sleepUntilBroadcasting = (sleepUntilTaskHandle != NULL) && !sleepUntilWaking;
        r["sleepUntilActive"] = sleepUntilBroadcasting;
        r["sleepUntilWaking"] = (sleepUntilTaskHandle != NULL) && sleepUntilWaking;
        if (sleepUntilBroadcasting) {
            r["sleepUntilHours"]   = prefs.getInt("suH", 0);
            r["sleepUntilMinutes"] = prefs.getInt("suM", 0);
        }
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
    else if (strcmp(cmd, "reset_clients") == 0)          { msgHandler.broadcastResetClients(); }

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

    } else if (strcmp(cmd, "test_sleep_cancel") == 0) {
        if (sleepTestTaskHandle != NULL) sleepTestCancel = true;

    } else if (strcmp(cmd, "test_sleep_cycle") == 0) {
        if (sleepTestTaskHandle != NULL) {
            JsonDocument r; r["event"] = "sleep_test_busy";
            serialSendDoc(r);
            return;
        }
        struct SleepTestParams { int sleepDurationS; int phaseDurationS; };
        auto* p = new SleepTestParams{
            doc["sleep_duration_s"] | 15,
            doc["phase_duration_s"] | 60
        };
        sleepTestCancel = false;
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
            vTaskDelay(pdMS_TO_TICKS(3000)); // clients still blink/settle after a resync — give them 3s before the first sleep broadcast

            // 3. Broadcast sleep for phaseDurationS
            // Enforce minimum so clients cycle through at least 2 sleep periods
            if (phaseDurationS < sleepDurationS * 2 + 5)
                phaseDurationS = sleepDurationS * 2 + 5;

            unsigned long long durationMicros = (unsigned long long)sleepDurationS * 1000000ULL;
            unsigned long phaseStart = millis();
            int broadcasts = 0;
            int nextCycleLog = sleepDurationS; // log a heartbeat every sleepDurationS seconds
            bool wokeReported[NUM_DEVICES] = {}; // one report per board, not one per second
            // We know sleep was sent — just hold that assumption and watch the existing
            // keepalive bookkeeping: every message from a board (10 min status broadcast,
            // announce, sync) bumps lastUpdateTime, so a bump mid-phase means not sleeping.
            unsigned long lastSeen[NUM_DEVICES] = {};
            for (int i = 0; i < NUM_DEVICES; i++) {
                client_address a = msgHandler.getItemFromAddressList(i);
                if (memcmp(a.address, MessageHandler::emptyAddress, 6) == 0) break;
                lastSeen[i] = a.lastUpdateTime;
            }
            { JsonDocument r;
              r["phase_duration_s"] = phaseDurationS;
              r["cycles_expected"] = phaseDurationS / sleepDurationS;
              emit("sleep_test_broadcast_start", r); }

            while (!sleepTestCancel && millis() - phaseStart < (unsigned long)phaseDurationS * 1000) {
                msgHandler.sendSleepWakeupMessage(durationMicros);
                broadcasts++;
                vTaskDelay(pdMS_TO_TICKS(1000));

                long elapsedS = (long)((millis() - phaseStart) / 1000);

                // Check for unexpected wakeups — any ACTIVE client mid-phase means
                // they woke up and didn't receive the sleep rebroadcast in time.
                for (int i = 0; i < NUM_DEVICES; i++) {
                    client_address a = msgHandler.getItemFromAddressList(i);
                    if (memcmp(a.address, MessageHandler::emptyAddress, 6) == 0) break;
                    // settle window: gotTimer confirmations from the resync trail in for
                    // a second or two, and clients blink ~3 s before actually sleeping —
                    // keep refreshing the baseline instead of flagging those
                    if (elapsedS <= 5) { lastSeen[i] = a.lastUpdateTime; continue; }
                    if (a.lastUpdateTime != lastSeen[i] && !wokeReported[i]) {
                        wokeReported[i] = true;
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
            if (sleepTestCancel) {
                // cancel = end the phase early but still run the wake-up sequence,
                // otherwise sleeping boards fail closed and nap indefinitely
                sleepTestCancel = false;
                JsonDocument r;
                r["elapsed_ms"] = (long)(millis() - phaseStart);
                emit("sleep_test_cancelled", r);
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
            // chunk + mac announce stagger (<=29s) + sync time, so stragglers aren't false "missing"
            unsigned long waitMaxMs = ((unsigned long)sleepDurationS + 90) * 1000;
            int returned = 0;
            int tick = 0;
            while (millis() - wakeStart < waitMaxMs) {
                if (sleepTestCancel) break; // second cancel aborts even the wake wait
                // wake sentinel every 2 s — fail-closed clients re-sleep through silence
                if (tick++ % 4 == 0) msgHandler.sendSleepWakeupMessage(0);
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

            // Unlike production sleep/sleep_until, this test never calls
            // startBroadcastSettleTask() (heavier than a test needs), so
            // without this a straggler that missed every sentinel packet
            // would sit in silence until the idle-animation timeout (minutes)
            // gives it something else to catch. Resume broadcasting now.
            msgHandler.startAnimationLoopTask();

            sleepTestTaskHandle = NULL;
            vTaskDelete(NULL);
        }, "sleepTest", 8192, p, 1, &sleepTestTaskHandle, 1);
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

    // Restore clock + sleep schedule from NVS — the pi may be offline for hours.
    // The clock checkpoint is at most ~5 min stale after a crash, but off by the
    // full outage after a power cut; the pi corrects it whenever it reconnects.
    prefs.begin("sparkles");
    time_t savedClock = (time_t)prefs.getLong64("clock", 0);
    if (savedClock > 1700000000) {
        struct timeval tv{ savedClock, 0 };
        settimeofday(&tv, NULL);
        setenv("TZ", "UTC", 1);
        tzset();
    }
    if (prefs.isKey("sleepH")) {
        msgHandler.setSleepTime(prefs.getInt("sleepH"), prefs.getInt("sleepM"), prefs.getInt("sleepS"));
        msgHandler.setWakeupTime(prefs.getInt("wakeH"), prefs.getInt("wakeM"), prefs.getInt("wakeS"));
    }
    // A "sleep until HH:MM" that was active when the master lost power (the
    // exact pack-up/power-cycle case this feature is for) resumes here —
    // recomputed against the restored clock, so it just continues counting
    // down to the same target rather than being silently dropped.
    //
    // But if the target has ALREADY passed (a dismantle-and-return gap longer
    // than planned — packed up "sleep until 10" and didn't power back on
    // until well past 10), rolling to tomorrow would be wrong: the one-shot
    // command's intent was "this near-term time", not "every night going
    // forward" (that's what the recurring schedule above is for). Drop it
    // instead — normal boot resumes, the idle animation loop starts, and any
    // client still asleep from before wakes on its own next listen window via
    // the "master's alive but not saying sleep" fallback.
    if (prefs.getBool("suActive", false)) {
        bool alreadyPassed = false;
        secondsUntilTimeOfDay(prefs.getInt("suH", 0), prefs.getInt("suM", 0), prefs.getInt("suS", 0), &alreadyPassed);
        if (alreadyPassed) {
            ESP_LOGI("MSG", "Sleep-until target already passed by the time we rebooted, dropping it");
            prefs.putBool("suActive", false);
        } else {
            int* target = new int[3]{ prefs.getInt("suH", 0), prefs.getInt("suM", 0), prefs.getInt("suS", 0) };
            xTaskCreatePinnedToCore(sleepUntilTask, "sleepUntil", 4096, target, 1, &sleepUntilTaskHandle, 1);
        }
    }

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

        // clock checkpoint so a reboot restores approximate wall time without the pi
        static unsigned long lastClockSave = 0;
        if (millis() - lastClockSave > 300000UL) {
            lastClockSave = millis();
            time_t nowSecs;
            time(&nowSecs);
            if (nowSecs > 1700000000) prefs.putLong64("clock", (int64_t)nowSecs);
        }

        // Send animate status to Pi so it stays in sync
        {
            JsonDocument r;
            r["event"]  = "animate_status";
            r["status"] = msgHandler.isAnimationLoopRunning();
            r["animation"] = animationTypeName(msgHandler.getLastAnimationType());
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

        if (millis() - msgHandler.getLastMidiTime() > MIDI_IDLE_ANIMATION_MS && msgHandler.getLastMidiTime() > 0) {
            ESP_LOGI("MSG", "No MIDI message for %d minutes, starting animation loop", MIDI_IDLE_ANIMATION_MS / 60000);
            msgHandler.startAnimationLoopTask();
            msgHandler.setLastMidiTime(0);
        }
    }

    if (msgHandler.isInSleepPhase() && sleepBroadcastTaskHandle == NULL && sleepUntilTaskHandle == NULL) {
        xTaskCreatePinnedToCore(sleepBroadcastTask, "sleepBroadcast", 4096, NULL, 1, &sleepBroadcastTaskHandle, 1);
    }
}
