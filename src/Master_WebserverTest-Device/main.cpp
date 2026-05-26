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
    ESP_LOGI("Received", "Data at %d", micros());
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {
}

unsigned long lastTick = 0;

// ── Serial bridge ─────────────────────────────────────────────────────────────

static String serialLineBuffer;

static void serialSendDoc(JsonDocument& doc) {
    String out;
    serializeJson(doc, out);
    Serial.println(out);
}

static void handleSerialCommand(const String& line) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, line);
    if (err) return;

    const char* cmd = doc["cmd"] | "";

    if (strcmp(cmd, "animate_toggle") == 0) {
        bool running = msgHandler.isAnimationLoopRunning();
        running ? msgHandler.stopAllAnimations() : msgHandler.startAnimationLoopTask();
        JsonDocument r;
        r["event"] = "animate_status";
        r["status"] = !running;
        serialSendDoc(r);

    } else if (strcmp(cmd, "animation_off") == 0) {
        msgHandler.stopAllAnimations();

    } else if (strcmp(cmd, "blink") == 0) {
        message_animation a{};
        a.animationType = BLINK;
        a.animationParams.blink.brightness  = 255;
        a.animationParams.blink.duration    = 500;
        a.animationParams.blink.repetitions = 3;
        a.animationParams.blink.startTime   = esp_timer_get_time() + 1000000;
        msgHandler.sendAnimation(a, doc["boardId"].as<int>());

    } else if (strcmp(cmd, "blink_all") == 0) {
        message_animation a{};
        a.animationType = BLINK;
        a.animationParams.blink.brightness  = 255;
        a.animationParams.blink.duration    = 500;
        a.animationParams.blink.repetitions = 3;
        a.animationParams.blink.startTime   = esp_timer_get_time() + 1000000;
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
        if (getLocalTime(&ti)) snprintf(buf, sizeof(buf), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
        JsonDocument r;
        r["event"]        = "system_info";
        r["systemTime"]   = buf;
        r["sleepSet"]     = msgHandler.isSleepSet();
        r["sleepIn"]      = (long)(msgHandler.getSleepTime() / 1000);
        r["sleepDuration"]= (long)(msgHandler.getSleepDuration() / 1000);
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
    else if (strcmp(cmd, "reannounce") == 0)             { msgHandler.broadcastReannounce(); }
    else if (strcmp(cmd, "reset_system") == 0)           { msgHandler.resetSystem(); }

    else if (strcmp(cmd, "toggle_test_mode") == 0) {
        bool next = !msgHandler.getTestMode();
        msgHandler.setTestMode(next);
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
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void setup()
{
    Serial.begin(115200);
    esp_log_level_set("*",      ESP_LOG_INFO);
    esp_log_level_set("MSG",    ESP_LOG_NONE);
    esp_log_level_set("LED",    ESP_LOG_NONE);
    esp_log_level_set("Sleep",  ESP_LOG_NONE);
    esp_log_level_set("TIMER",  ESP_LOG_NONE);
    esp_log_level_set("CLAP",   ESP_LOG_INFO);
    esp_log_level_set("Tick",   ESP_LOG_INFO);

    unsigned long long startTime = millis();
    while (!Serial) {
        if (millis() - startTime > 3000) break;
    }

    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        lfs_started = false;
    }

    rtc_clk_slow_src_set(RTC_SLOW_FREQ_8MD256);
    WiFi.mode(WIFI_AP_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    delay(1000);
    ledInstance.setup();
    msgHandler.setup(ledInstance);
    // No WebServer — serial bridge handles all communication
}

void loop()
{
    // Read serial commands from Pi
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n') {
            serialLineBuffer.trim();
            if (serialLineBuffer.length() > 0) {
                handleSerialCommand(serialLineBuffer);
            }
            serialLineBuffer = "";
        } else {
            serialLineBuffer += c;
        }
    }

    if (lastTick + 5000 < millis()) {
        lastTick = millis();

        uint8_t address[6];
        WiFi.macAddress(address);
        ESP_LOGI("TICK", "Tick %s", msgHandler.stringAddress(address, true).c_str());
        ESP_LOGI("TICK", "Ticks until end: %llu", (unsigned long long)ledInstance.getNextAnimationTicks());
        ESP_LOGI("", "Battery: %.2f%%", msgHandler.getBatteryPercentage());

        // Send animate status to Pi so it stays in sync
        {
            JsonDocument r;
            r["event"]  = "animate_status";
            r["status"] = msgHandler.isAnimationLoopRunning();
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

        if (!msgHandler.isInSleepPhase()) {
            unsigned long sleepTime = msgHandler.getSleepTime();
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            (void)sleepTime;
        }

        if (millis() - msgHandler.getLastMidiTime() > 60000 && msgHandler.getLastMidiTime() > 0) {
            ESP_LOGI("MSG", "No MIDI message for 60 seconds, starting animation loop");
            msgHandler.startAnimationLoopTask();
            msgHandler.setLastMidiTime(0);
        }
    }

    if (msgHandler.isInSleepPhase()) {
        ESP_LOGI("Sleep", "Going to sleep for %lu ms", msgHandler.getSleepDuration());
        unsigned long long sleepDuration = ((unsigned long long)msgHandler.getSleepDuration() - 1ULL) * 1000ULL;
        ESP_LOGI("Sleep", "Sleep duration in micros: %llu", sleepDuration);
        esp_sleep_enable_timer_wakeup(sleepDuration);
        msgHandler.sendSleepWakeupMessage(sleepDuration);
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            ESP_LOGI("Sleep", "Before Sleep Current Time: %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
        msgHandler.turnWifiOff();
        Serial.end();
        msgHandler.recordTimeOfDayBeforeSleep();
        esp_light_sleep_start();
        Serial.begin(115200);
        delay(200);
        msgHandler.setTimeOfDayAfterSleep(sleepDuration);
        msgHandler.turnWifiOn();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        msgHandler.setAddressListInactive();
        msgHandler.startBroadcastSettleTask();
        msgHandler.broadcastReannounce();
    }
}
