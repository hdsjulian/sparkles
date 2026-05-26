# Arduino Serial Handler sketch

This is a reference for the code to add to `src/Master-Device/main.cpp` when
you're ready to wire up the Pi↔ESP32 serial bridge.  Nothing here modifies
any existing library — it's a self-contained block you can paste into the
master's main loop.

## 1. Add to global scope (above `setup()`)

```cpp
// ── Serial bridge ────────────────────────────────────────────────────────────
#include <ArduinoJson.h>   // add "bblanchon/ArduinoJson" to platformio.ini

static String serialLineBuffer;

static void serialSendEvent(const char* eventName, JsonDocument& doc) {
    doc["event"] = eventName;
    String out;
    serializeJson(doc, out);
    Serial.println(out);
}

static void handleSerialCommand(const String& line) {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, line);
    if (err) return;

    const char* cmd = doc["cmd"] | "";
    MessageHandler& mh = MessageHandler::getInstance();

    if (strcmp(cmd, "animate_toggle") == 0) {
        bool running = mh.isAnimationLoopRunning();
        running ? mh.stopAllAnimations() : mh.startAnimationLoopTask();
        StaticJsonDocument<64> r;
        r["status"] = !running;
        serialSendEvent("animate_status", r);

    } else if (strcmp(cmd, "animation_off") == 0) {
        mh.stopAllAnimations();

    } else if (strcmp(cmd, "blink") == 0) {
        message_animation a{};
        a.animationType = BLINK;
        a.animationParams.blink.brightness  = 255;
        a.animationParams.blink.duration    = 500;
        a.animationParams.blink.repetitions = 3;
        a.animationParams.blink.startTime   = esp_timer_get_time() + 1000000;
        mh.sendAnimation(a, doc["boardId"].as<int>());

    } else if (strcmp(cmd, "blink_all") == 0) {
        message_animation a{};
        a.animationType = BLINK;
        a.animationParams.blink.brightness  = 255;
        a.animationParams.blink.duration    = 500;
        a.animationParams.blink.repetitions = 3;
        a.animationParams.blink.startTime   = esp_timer_get_time() + 1000000;
        mh.sendAnimation(a, -1);

    } else if (strcmp(cmd, "sync") == 0) {
        mh.setCurrentTimerIndex(doc["index"].as<int>());
        mh.startTimerSyncTask();

    } else if (strcmp(cmd, "sync_all") == 0) {
        mh.startAllTimerSyncTask();

    } else if (strcmp(cmd, "submit_positions") == 0) {
        mh.setBoardPosition(doc["boardId"], doc["xpos"].as<float>(), doc["ypos"].as<float>());

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
        mh.setSleepTime(doc["hours"], doc["minutes"], doc["seconds"]);

    } else if (strcmp(cmd, "set_wakeup_time") == 0) {
        mh.setWakeupTime(doc["hours"], doc["minutes"], doc["seconds"]);

    } else if (strcmp(cmd, "get_address_list") == 0) {
        // send each known board as an update_board event
        for (int i = 0; i < NUM_DEVICES; i++) {
            client_address a = mh.getItemFromAddressList(i);
            if (memcmp(a.address, MessageHandler::emptyAddress, 6) == 0) break;
            StaticJsonDocument<256> r;
            r["event"] = "update_board";
            r["id"]    = i;
            char mac[18];
            snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                a.address[0],a.address[1],a.address[2],
                a.address[3],a.address[4],a.address[5]);
            r["address"]           = mac;
            r["status"]            = (a.active == ACTIVE) ? "active" : "inactive";
            r["batteryPercentage"] = a.batteryPercentage;
            r["distance"]          = a.distanceFromCenter;
            r["xpos"]              = a.xPos;
            r["ypos"]              = a.yPos;
            r["lastUpdateTime"]    = (unsigned long)a.lastUpdateTime;
            r["timerOffset"]       = (long)a.timerOffset;
            r["delay"]             = a.delay;
            String out; serializeJson(r, out); Serial.println(out);
        }
        // also send a summary
        StaticJsonDocument<64> summary;
        summary["event"]      = "address_list";
        summary["numDevices"] = mh.getNumDevices();
        String out; serializeJson(summary, out); Serial.println(out);

    } else if (strcmp(cmd, "get_midi_params") == 0) {
        message_midi_params p = mh.getMidiParams();
        StaticJsonDocument<256> r;
        r["minVal"]=p.valMin; r["maxVal"]=p.valMax;
        r["minSat"]=p.satMin; r["maxSat"]=p.satMax;
        r["hue"]=p.hue; r["saturation"]=p.saturation;
        r["rangeMin"]=p.rangeMin; r["rangeMax"]=p.rangeMax;
        r["minDb"]=p.rmsMin; r["maxDb"]=p.rmsMax;
        r["distance"]=p.distance; r["distanceSwitch"]=p.distanceSwitch;
        r["distanceMode"]=p.distanceMode; r["mode"]=p.mode;
        serialSendEvent("midi_params", r);

    } else if (strcmp(cmd, "set_midi_params") == 0) {
        mh.setMidiParams(
            doc["minVal"],doc["maxVal"],doc["minSat"],doc["maxSat"],
            doc["midiHue"],doc["midiSaturation"],
            doc["rangeMin"],doc["rangeMax"],
            doc["minDb"].as<float>(),doc["maxDb"].as<float>(),
            doc["mode"],doc["distance"],
            doc["distanceSwitch"].as<bool>(),doc["distanceMode"]);

    } else if (strcmp(cmd, "get_darkroom_params") == 0) {
        message_darkroom_params p = mh.getDarkroomParams();
        StaticJsonDocument<256> r;
        r["strobeMin"]=p.strobeMin; r["strobeMax"]=p.strobeMax;
        r["redlightMin"]=p.redlightMin; r["redlightMax"]=p.redlightMax;
        r["candlelightMin"]=p.candlelightMin; r["candlelightMax"]=p.candlelightMax;
        serialSendEvent("darkroom_params", r);

    } else if (strcmp(cmd, "set_darkroom_params") == 0) {
        mh.setDarkroomParams(
            doc["strobeMin"],doc["strobeMax"],
            doc["redlightMin"],doc["redlightMax"],
            doc["candlelightMin"],doc["candlelightMax"],
            doc["redLightEnabled"].as<bool>(),doc["candleLightEnabled"].as<bool>());

    } else if (strcmp(cmd, "get_system_info") == 0) {
        struct tm ti; char buf[32]="not set";
        if (getLocalTime(&ti)) snprintf(buf,sizeof(buf),"%02d:%02d:%02d",ti.tm_hour,ti.tm_min,ti.tm_sec);
        StaticJsonDocument<256> r;
        r["systemTime"]    = buf;
        r["sleepSet"]      = mh.isSleepSet();
        r["sleepIn"]       = (long)(mh.getSleepTime()/1000);
        r["sleepDuration"] = (long)(mh.getSleepDuration()/1000);
        serialSendEvent("system_info", r);

    } else if (strcmp(cmd, "calibration_start") == 0)         { mh.startCalibrationMaster(); }
    else if (strcmp(cmd, "calibration_cancel") == 0)          { mh.cancelCalibration(); }
    else if (strcmp(cmd, "calibration_reset") == 0)           { mh.resetCalibration(); }
    else if (strcmp(cmd, "calibration_continue") == 0)        { mh.continueCalibration(doc["x"],doc["y"]); }
    else if (strcmp(cmd, "calibration_end") == 0)             { mh.endCalibration(); }
    else if (strcmp(cmd, "calibration_test") == 0)            { mh.testCalibration(); }
    else if (strcmp(cmd, "calibration_calibrate") == 0)       { mh.commandCalibrate(doc["boardId"]); }
    else if (strcmp(cmd, "dist_cal_start") == 0)              { mh.startDistanceCalibrationMaster(); }
    else if (strcmp(cmd, "dist_cal_continue") == 0)           { mh.continueDistanceCalibration(); }
    else if (strcmp(cmd, "dist_cal_end") == 0)                { mh.endDistanceCalibration(); }
    else if (strcmp(cmd, "dist_cal_cancel") == 0)             { mh.cancelCalibration(); }
    else if (strcmp(cmd, "dist_cal_abort") == 0)              { mh.abortDistanceCalibration(); }
    else if (strcmp(cmd, "ota_update") == 0)                  { mh.startOTAUpdateTask(); }
    else if (strcmp(cmd, "reannounce") == 0)                  { mh.broadcastReannounce(); }
    else if (strcmp(cmd, "reset_system") == 0)                { mh.resetSystem(); }

    else if (strcmp(cmd, "toggle_test_mode") == 0) {
        bool next = !mh.getTestMode();
        mh.setTestMode(next);
        StaticJsonDocument<64> r; r["testMode"]=next;
        serialSendEvent("test_mode", r);

    } else if (strcmp(cmd, "toggle_logging") == 0) {
        extern bool g_loggingEnabled;
        g_loggingEnabled = !g_loggingEnabled;
        StaticJsonDocument<64> r; r["logging"]=g_loggingEnabled;
        serialSendEvent("logging", r);

    } else if (strcmp(cmd, "factory_reset") == 0) {
        if (LittleFS.exists("/clientAddress")) LittleFS.remove("/clientAddress");
        ESP.restart();
    }
}
// ─────────────────────────────────────────────────────────────────────────────
```

## 2. In the Arduino `loop()` (or a dedicated FreeRTOS task)

```cpp
// Serial bridge – read commands from Pi
while (Serial.available()) {
    char c = Serial.read();
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
```

## 3. Outgoing events (call these wherever WebServer calls are currently made)

Anywhere you'd call e.g. `webServer.updateAddress(id)` or `webServer.clapReceived(...)`,
add an equivalent `Serial.println(jsonFrame)` beside it so the Pi receives the event.

Example — board update:
```cpp
// after updating addressList[id]
StaticJsonDocument<256> doc;
doc["event"] = "update_board";
doc["id"]    = id;
// … fill remaining fields …
String out; serializeJson(doc, out); Serial.println(out);
```

## 4. platformio.ini addition

```ini
lib_deps =
    bblanchon/ArduinoJson@^7.0.0
```
