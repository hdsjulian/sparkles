
#include "MessageHandler.h"
#if (DEVICE_MODE == CLIENT)
#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"
#include "Ota.h"

// Crash breadcrumb: survives any reset except power loss. The wake-path's own
// error output is unrecoverable (USB is still re-enumerating when it would
// print), so we record WHERE in the sleep cycle we were and read it after the
// reboot in setup(). Magic values make power-on garbage indistinguishable
// from "not sleeping" ~impossible to misread.
#define SLEEP_MARKER_SLEEPING 0xA55A0001u
#define SLEEP_MARKER_WAKING   0xA55A0002u
RTC_NOINIT_ATTR uint32_t g_lightSleepMarker;

void MessageHandler::handleReceive() {
    message_data incomingData;
    while (true) {
        if (xQueueReceive(receiveQueue, &incomingData, portMAX_DELAY) == pdTRUE) {
            //BETA
                //handleTimer(incomingData);            
            if (incomingData.messageType == MSG_ANIMATION) {
                
                float batteryPercentage = getBatteryPercentage();
                if (batteryPercentage < BATTERY_LOW_THRESHOLD) {
                    message_data batteryLow;
                    //ESP_LOGI("MSG", "Battery low, percentage: %f", batteryPercentage);
                    /*
                    batteryLow.messageType = MSG_STATUS;
                    batteryLow.payload.status.batteryPercentage = getBatteryPercentage();
                    memcpy(batteryLow.targetAddress, hostAddress, 6);
                    xQueueSend(sendQueue, &batteryLow, portMAX_DELAY);
                    */
                    continue;
                }
                else {
                    message_animation animation = (message_animation)incomingData.payload.animation;

                    if (animation.animationType == BATTERY_BLINK) {
                        float pct = constrain(getBatteryPercentage(), 0.0f, 100.0f);
                        animation.animationParams.blink.hue        = 0;
                        animation.animationParams.blink.saturation = (int)(255.0f * (1.0f - pct / 100.0f));
                        animation.animationParams.blink.brightness = 255;
                        animation.animationType = BLINK;
                    }

                    ledInstance->pushToAnimationQueue(animation);
                }
            }
            else if (incomingData.messageType == MSG_TIMER) {
                handleTimer(incomingData);
            }
            else if (incomingData.messageType == MSG_SYSTEM_STATUS) {
                handleSystemStatus(incomingData);
            }
            else if (incomingData.messageType == MSG_WAIT_FOR_INSTRUCTIONS) {
            }
            else if (incomingData.messageType == MSG_SLEEP_WAKEUP) {
                handleSleepWakeup(incomingData);
            }
            else if (incomingData.messageType == MSG_MIDI_PARAMS) {
                message_midi_params midiParamsMessage = (message_midi_params)incomingData.payload.midiParams;
                ledInstance->setMaxDistanceFromCenter(midiParamsMessage.distance);
                ledInstance->setUseDistanceSwitch(midiParamsMessage.distanceSwitch);
                ledInstance->setDistanceMode(midiParamsMessage.distanceMode);
                ledInstance->setMidiParams(midiParamsMessage);
                ESP_LOGI("MIDI", "Received Midi Params minVal %d, maxVal %d, minRms %.2f, maxRMS %.2f", midiParamsMessage.valMin, midiParamsMessage.valMax, midiParamsMessage.rmsMin, midiParamsMessage.rmsMax);
            }
            else if (incomingData.messageType == MSG_COMMAND) {
                message_command commandMessage = (message_command)incomingData.payload.command;
                if (commandMessage.commandType == CMD_START_CALIBRATION || commandMessage.commandType == CMD_START_DISTANCE_CALIBRATION || commandMessage.commandType == CMD_CONTINUE_CALIBRATION || commandMessage.commandType == CMD_CONTINUE_DISTANCE_CALIBRATION) {
                    startCalibrationClient();
                }
                if (commandMessage.commandType == CMD_TEST_CALIBRATION) {
                    setCalibrationTest(true);
                    startCalibrationClient();
                }
                if (commandMessage.commandType == CMD_CANCEL_CALIBRATION || commandMessage.commandType == CMD_END_CALIBRATION) {
                    ESP_LOGI("MSG", "Cancel calibration command received");

                    if (clapTaskHandle != NULL) {
                        vTaskDelete(clapTaskHandle);
                        clapTaskHandle = NULL;
                    }

                }

                if (commandMessage.commandType == CMD_SET_ADMIN_NOT_PRESENT) {
                    setAdminPresent(false);
                }
                if (commandMessage.commandType == CMD_SET_ADMIN_PRESENT) {
                    setAdminPresent(true);
                    ESP_LOGI("MSG", "Admin present, not going to sleep");
                }
                if (commandMessage.commandType == CMD_MESSAGE) {
                }
                if (commandMessage.commandType == CMD_RESET_SYSTEM) {
                    ESP_LOGI("MSG", "Received reset system command");
                    delay(1000);
                    delay(100*ledInstance->getCurrentPosition());
                    ESP.restart();
                }
                if (commandMessage.commandType == CMD_TEST_MODE_ON) {
                    setTestMode(true);
                    ledInstance->setTestMode(true);
                    float spacing = commandMessage.param > 0.0f ? commandMessage.param : 1.0f;
                    float dist = spacing * (float)ledInstance->getCurrentPosition();
                    ledInstance->setDistanceFromCenter(dist);
                    ESP_LOGI("MSG", "Test mode ON: pos %d, spacing %.2f m, distance %.2f m, MIDI note %d",
                        ledInstance->getCurrentPosition(), spacing, dist, 60 + ledInstance->getCurrentPosition());
                }
                if (commandMessage.commandType == CMD_TEST_MODE_OFF) {
                    setTestMode(false);
                    ledInstance->setTestMode(false);
                }
                if (commandMessage.commandType == CMD_SET_MAX_DISTANCE) {
                    ledInstance->setMaxDistanceFromCenter((int)commandMessage.param);
                    ESP_LOGI("MSG", "Max distance from center set to %.2f m", commandMessage.param);
                }
                if (commandMessage.commandType == CMD_OTA_UPDATE) {
                    ESP_LOGI("MSG", "CMD_OTA_UPDATE received, connecting to %s", OTA_WIFI_SSID);
                    WiFi.mode(WIFI_OFF);
                    delay(100);
                    WiFi.mode(WIFI_STA);
                    if (strlen(OTA_WIFI_PASSWORD) > 0) {
                        WiFi.begin(OTA_WIFI_SSID, OTA_WIFI_PASSWORD);
                    } else {
                        WiFi.begin(OTA_WIFI_SSID);
                    }
                    int retries = 0;
                    while (WiFi.status() != WL_CONNECTED && retries < 20) {
                        delay(500);
                        retries++;
                    }
                    if (WiFi.status() == WL_CONNECTED) {
                        OTAHandler& ota = OTAHandler::getInstance();
                        ota.setup();
                        ota.performUpdate(); // reboots on success
                    } else {
                        ESP_LOGE("MSG", "CMD_OTA_UPDATE: WiFi failed, aborting");
                    }
                }
                if (commandMessage.commandType == CMD_REANNOUNCE) {
                    uint8_t mac[6];
                    WiFi.macAddress(mac);
                    uint32_t delayMs = (mac[5] % 30) * 2000;
                    ESP_LOGI("MSG", "CMD_REANNOUNCE: re-announcing in %u ms", delayMs);
                    vTaskDelay(delayMs / portTICK_PERIOD_MS);
                    setAddressAnnounced(false);
                    if (announceTaskHandle == NULL) {
                        xTaskCreatePinnedToCore(announceAddressWrapper, "runAnnounceAddress", 10000, this, 2, &announceTaskHandle, 1);
                    }
                }

                
            }
            else if (incomingData.messageType == MSG_STATUS) {
                // another client's status broadcast, ignore
            }
            else if (incomingData.messageType == MSG_CLAP) {
                message_clap clapMessage = incomingData.payload.clap;
                setHasClapHappened(true);
                
            }
            else if (incomingData.messageType == MSG_CONFIG_DATA) {
                message_config_data configData = incomingData.payload.configData;
                ESP_LOGI("MSG", "Received config data for board ID: %d", configData.boardId);
                ledInstance->setCurrentPosition(configData.boardId);
                if (configData.distance > 0) {
                    ledInstance->setDistanceFromCenter(configData.distance);
                    ESP_LOGI("MSG", "Setting distance from center to: %f", configData.distance);
                }
                else {
                }
                if (configData.xPos != 0.0 || configData.yPos != 0.0) {
                    ledInstance->setLocation(configData.xPos, configData.yPos);
                    ESP_LOGI("MSG", "Setting position to: (%f, %f)", configData.xPos, configData.yPos);
                }
                else {
                }
            }
            else if (incomingData.messageType == MSG_UPDATE_VERSION) {
                message_update_version updateVersionMessage = incomingData.payload.updateVersion;
                if (updateVersionMessage.version >= version && updateVersionMessage.version != version) {
                    ledInstance->blink(esp_timer_get_time(), 100, 5, 200, 255, 127);
                    // guard: vTaskDelete(NULL) would delete THIS task (the receive
                    // handler), leaving the client deaf instead of updating
                    if (announceTaskHandle != NULL) {
                        vTaskDelete(announceTaskHandle);
                        announceTaskHandle = NULL;
                    }
                    OTAHandler& ota = OTAHandler::getInstance();
                    // use URL from message if provided, else fall back to compile-time define
                    if (updateVersionMessage.otaUrl[0] != '\0') {
                        ota.setup(updateVersionMessage.otaUrl);
                    } else {
                        ota.setup();
                    }
                    ota.performUpdate();
                }
                else {
                    ESP_LOGI("MSG", "Received update version message with lower version, ignoring");
                }
            }

            else if (incomingData.messageType == MSG_TIMER_QUERY) {
                message_data reply{};
                reply.messageType = MSG_TIMER_RESPONSE;
                memcpy(reply.targetAddress, hostAddress, 6);
                WiFi.macAddress(reply.senderAddress);
                reply.payload.timerResponse.estimatedMasterTime = (int64_t)esp_timer_get_time() + getTimeOffset();
                reply.payload.timerResponse.addressId = ledInstance->getCurrentPosition();
                addPeer(hostAddress);
                esp_now_send(hostAddress, (uint8_t*)&reply, ESPNOW_CLIENT_COMPAT_SIZE);
                removePeer(hostAddress);
            }
            else {
                ESP_LOGI("MSG", "Unknown message type %d", incomingData.messageType);
            }
        }
    }
}

void MessageHandler::handleTimer(message_data incomingData) {
     message_timer timerMessage = incomingData.payload.timer;
    setAddressAnnounced(true);
    if (announceTaskHandle != NULL) {
        vTaskDelete(announceTaskHandle);
        announceTaskHandle = NULL;
    }
    if (timerMessage.addressId >=0) {
        ledInstance->setCurrentPosition(timerMessage.addressId);

    }

    // Each packet carries the measured TX latency of the previous one (paired by
    // counter), so queueing/backoff and retransmission delays cancel out per sample.
    static long long pendingRaw = 0;
    static uint16_t pendingCounter = 0;
    static bool pendingValid = false;
    static long long samples[TIMER_ARRAY_COUNT];
    static int sampleCount = 0;

    unsigned long long sinceLast = timerMessage.receiveTime - getLastReceiveTime();
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
    pendingRaw     = (long long)timerMessage.sendTime - (long long)timerMessage.receiveTime;
    pendingCounter = timerMessage.counter;
    pendingValid   = true;
    setLastReceiveTime(timerMessage.receiveTime);

    if (sampleCount >= TIMER_ARRAY_COUNT) {
        // median of the burst, robust to outliers the gates didn't catch
        long long sorted[TIMER_ARRAY_COUNT];
        memcpy(sorted, samples, sizeof(sorted));
        for (int i = 1; i < TIMER_ARRAY_COUNT; i++) {
            long long v = sorted[i];
            int j = i - 1;
            while (j >= 0 && sorted[j] > v) { sorted[j + 1] = sorted[j]; j--; }
            sorted[j + 1] = v;
        }
        long long median = (sorted[TIMER_ARRAY_COUNT / 2 - 1] + sorted[TIMER_ARRAY_COUNT / 2]) / 2;
        // A jump of many seconds means a clock-epoch reset — the master power-cycled
        // (its esp_timer restarted at 0 while we kept running), so the new offset is
        // correct but any animation still queued/running carries a startTime from the
        // OLD epoch. Mixing an old-epoch startTime with the new offset produced the
        // "absurd microsUntilStart" garbage. Flush the LED state so only fresh-epoch
        // animations (which schedule correctly against the new offset) are evaluated.
        long long offsetJump = median - getTimeOffset();
        if (offsetJump < 0) offsetJump = -offsetJump;
        setTimeOffset(median);
        if (offsetJump > 2000000) {
            ESP_LOGW("MSG", "Timer epoch shift (%lld us jump) — master rebooted; flushing stale animations", offsetJump);
            ledInstance->resetLedTask();
        }

        message_data gotTimerMessage;
        gotTimerMessage.messageType = MSG_GOT_TIMER;
        gotTimerMessage.payload.gotTimer.delayAverage = delayAverage;
        gotTimerMessage.payload.gotTimer.batteryPercentage = getBatteryPercentage();
        gotTimerMessage.payload.gotTimer.offset = getTimeOffset();
        memcpy(gotTimerMessage.targetAddress, hostAddress, 6);
        ESP_LOGI("MSG", "Timer set. Offset (burst median): %lld, delay average: %d", median, delayAverage);
        xQueueSend(sendQueue, &gotTimerMessage, portMAX_DELAY);
        setTimerSet(true);
        ledInstance->blink(esp_timer_get_time(), 300, 3, 100, 255, 127);
        startBatterySyncTask();
        sampleCount  = 0;
        pendingValid = false;
        delayCounter = 0;
        delayAverage = 0;
    }
}

void MessageHandler::goToSleep() {
    message_animation animationMessage = ledInstance->createAnimation(OFF);
    ledInstance->pushToAnimationQueue(animationMessage);
    vTaskDelay(1000/portTICK_PERIOD_MS);

}

//parse message, set sleep / wakeup times, start task that runs until sleeptime 
//ich geis done, turns off lights, after wakeup restarts and kills itself.
// WHEN to go to sleep? NOW. there is no when Timestamp


void MessageHandler::handleSleepWakeup(message_data incomingData) {
    unsigned long long duration = incomingData.payload.sleepWakeup.duration;
    if (duration == 0) return; // morning sentinel, only meaningful inside the sleep loop below
    ESP_LOGI("MSG", "Going to sleep for %llu microseconds", duration);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    ledInstance->blink(esp_timer_get_time(), 100, 4, 160, 255, 127);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    message_animation animationMessage = ledInstance->createAnimation(OFF);
    ledInstance->pushToAnimationQueue(animationMessage);

    // Sleep in chunks, fully passive in between: the master rebroadcasts sleep at
    // 1 Hz all night and a zero-duration wake sentinel for minutes each morning.
    // Hearing sleep -> sleep again. Hearing the sentinel -> morning immediately.
    // Total silence -> master is gone, keep sleeping instead of burning the
    // battery all night. We never transmit to find out what time it is.
    //
    // Any OTHER master broadcast (animation, status, etc.) is also treated as
    // "morning" — but only as a fallback if we finish the whole listen window
    // having heard nothing else. This is the safety net for the master
    // rebooting mid-wake-window (its sleep-schedule state resets, but if it's
    // back to normal operation it'll be broadcasting animations again) and for
    // a client that's asleep longer than the wake sentinel stays up. Treating
    // "other traffic" as an immediate, first-packet trigger (as an earlier
    // version did) raced against the authoritative 1 Hz sleep-continue ping:
    // sendSystemStatus() broadcasts to ALL clients whenever ANY board's sync
    // completes, and if that packet happened to land before that second's
    // sleep-continue ping, the client woke early on a completely unrelated
    // event. Waiting out the full window lets the authoritative signal win
    // whenever the phase is genuinely still active.
    bool morning = false;
    while (!morning) {
        turnWifiOff();
        Serial.end();
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
        esp_sleep_enable_timer_wakeup(duration);
        g_lightSleepMarker = SLEEP_MARKER_SLEEPING;
        esp_light_sleep_start();
        g_lightSleepMarker = SLEEP_MARKER_WAKING;
        Serial.begin(115200);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        turnWifiOn();
        g_lightSleepMarker = 0;

        lastSleepMsgMillis = 0;
        lastMasterMsgMillis = 0;
        bool sleepMsgHeard = false;
        unsigned long listenStart = millis();
        while (millis() - listenStart < 6000) {
            if (lastSleepMsgMillis != 0) {
                unsigned long long d = lastSleepMsgDuration;
                if (d > 0) duration = d; else morning = true;
                sleepMsgHeard = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        // fallback: heard the master do something all window, but never the
        // authoritative sleep-continue -> the phase must genuinely be over
        if (!sleepMsgHeard && lastMasterMsgMillis != 0) morning = true;
        xQueueReset(receiveQueue); // drop whatever queued up during the listen window
    }

    ledInstance->resetLedTask();
    ledInstance->blink(esp_timer_get_time(), 150, 2, 160, 255, 127);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    // stagger the announce by MAC so a large fleet doesn't stampede the master
    uint8_t macAddr[6];
    WiFi.macAddress(macAddr);
    vTaskDelay(pdMS_TO_TICKS((macAddr[5] % 30) * 1000));
    setAddressAnnounced(false);
    if (announceTaskHandle == NULL) {
        xTaskCreatePinnedToCore(announceAddressWrapper, "runAnnounceAddress", 10000, this, 2, &announceTaskHandle, 1);
    }
}

void MessageHandler::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        if (memcmp(mac_addr, broadcastAddress, 6) == 0) {
        }
        else {
        }
    }
    else {
    }
}

// Map the 32-bit hardware MAC-time RX stamp into the esp_timer domain. The smallest
// observed span over a burst is the clock-domain offset, jitter only ever adds delay.
static unsigned long long timerRxTimestamp(const esp_now_recv_info *info, unsigned long long cbTime) {
    if (info == nullptr || info->rx_ctrl == nullptr) return cbTime;
    int32_t span = (int32_t)((uint32_t)cbTime - info->rx_ctrl->timestamp);
    static int32_t minSpan = INT32_MAX;
    static unsigned long long lastCbTime = 0;
    if (cbTime - lastCbTime > 1000000ULL) minSpan = INT32_MAX; // stale after a 1 s gap
    lastCbTime = cbTime;
    if (span < minSpan) minSpan = span;
    return cbTime - (unsigned long long)(span - minSpan);
}

void MessageHandler::onDataRecv(const esp_now_recv_info * mac, const uint8_t *incomingData, int len) {
    unsigned long long receiveTime = esp_timer_get_time();
    MessageHandler& instance = getInstance();
    if (len < 1 || len > (int)sizeof(message_data)) {
        ESP_LOGW("RECV", "Bad size len=%d sizeof=%d, dropping", len, (int)sizeof(message_data));
        return;
    }
    message_data localData;
    memcpy(&localData, incomingData, len);
    // RX visibility: log every frame the radio delivers, EXCEPT the high-rate
    // music animations (MIDI/shimmer) which would flood. If a client goes
    // "unreachable", watch this: no RX lines at all = nothing is arriving
    // (radio/range/channel); RX lines present but no reaction = a logic problem.
    {
        bool musicAnim = (localData.messageType == MSG_ANIMATION) &&
            (localData.payload.animation.animationType == MIDI ||
             localData.payload.animation.animationType == BACKGROUND_SHIMMER);
        if (!musicAnim) {
            ESP_LOGI("RX", "type=%d from %02x:%02x:%02x:%02x:%02x:%02x len=%d",
                     localData.messageType,
                     mac->src_addr[0], mac->src_addr[1], mac->src_addr[2],
                     mac->src_addr[3], mac->src_addr[4], mac->src_addr[5], len);
        }
    }
    if (localData.messageType == MSG_TIMER) {
        // learn master MAC from the first MSG_TIMER we receive
        if (!instance.hostAddressLearned) {
            memcpy(instance.hostAddress, mac->src_addr, 6);
            instance.hostAddressLearned = true;
            instance.addPeer(instance.hostAddress);
        }
        localData.payload.timer.receiveTime = timerRxTimestamp(mac, receiveTime);
    }
    // stamps for the sleep listen window (handleSleepWakeup blocks the receive task,
    // so it reads these instead of the queue)
    if (localData.messageType == MSG_SLEEP_WAKEUP) {
        instance.lastSleepMsgDuration = localData.payload.sleepWakeup.duration;
        instance.lastSleepMsgMillis = millis();
    } else if (instance.hostAddressLearned && memcmp(mac->src_addr, instance.hostAddress, 6) == 0) {
        instance.lastMasterMsgMillis = millis();
    }
    if (localData.messageType == MSG_ANIMATION && instance.getBatteryPercentage() > BATTERY_LOW_THRESHOLD) {
        if (localData.payload.animation.animationType == MIDI || localData.payload.animation.animationType == BACKGROUND_SHIMMER) {
            LedHandler& ledInstance = LedHandler::getInstance();
            if (localData.payload.animation.animationType == BACKGROUND_SHIMMER) {
                auto& shimmer = localData.payload.animation.animationParams.backgroundShimmer;
                if (shimmer.value > 0) {
                    uint8_t mac[6]; WiFi.macAddress(mac);
                    uint8_t range = (uint8_t)(shimmer.value * 0.1f);
                    int8_t offset = (range > 0) ? (int8_t)((mac[5] % (range * 2 + 1)) - range) : 0;
                    shimmer.value = (uint8_t)constrain((int)shimmer.value + offset, 0, 255);
                }
            }
            ledInstance.pushToAnimationQueue(localData.payload.animation);
            return;

        }

    }
    else if (localData.messageType == MSG_ANIMATION && instance.getBatteryPercentage() <= BATTERY_LOW_THRESHOLD) {
    }

    instance.pushToRecvQueue(mac, (const uint8_t*)&localData, len);


}


void MessageHandler::announceAddressWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runAnnounceAddress();
    // usually handleTimer vTaskDeletes this task before the loop notices the
    // announced flag — but if the loop wins that race and returns, a FreeRTOS
    // task function returning makes ESP-IDF abort() the whole chip
    messageHandlerInstance->announceTaskHandle = NULL;
    vTaskDelete(NULL);
}

void MessageHandler::runAnnounceAddress() {
    message_data messageData;
    messageData.messageType = MSG_ADDRESS;
    memcpy(messageData.targetAddress, broadcastAddress, 6);
    messageData.payload.address.version = version;
    WiFi.macAddress(messageData.payload.address.address);
    while (getAddressAnnounced() == false) {
        xQueueSend(sendQueue, &messageData, portMAX_DELAY);
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}


void MessageHandler::handleSend() {
    message_data messageData;
    unsigned long long now;
    while(true) {
        if (xQueueReceive(sendQueue, &messageData, portMAX_DELAY) == pdTRUE) {
            esp_err_t err = esp_now_register_send_cb(onDataSent);
            addPeer(messageData.targetAddress);
            switch (messageData.messageType) {
                case MSG_GOT_TIMER:
                    now = esp_timer_get_time();
                    messageData.payload.gotTimer.sendTime = now;
                    messageData.payload.gotTimer.perceivedTime = (long long)now + getTimeOffset();
                    esp_now_send(messageData.targetAddress, (uint8_t *) &messageData, sizeof(messageData));
                    break;
                case MSG_ADDRESS:
                    esp_now_send(messageData.targetAddress, (uint8_t *) &messageData, sizeof(messageData));
                    break;
                case MSG_STATUS:
                    esp_now_send(messageData.targetAddress, (uint8_t *) &messageData, sizeof(messageData));
                    break;
                case MSG_CLAP:
                    esp_now_send(messageData.targetAddress, (uint8_t *) &messageData, sizeof(messageData));
                    break;
                default:
                    break;
            }        
        }
    }
}
void MessageHandler::startBatterySyncTask() {
    xTaskCreatePinnedToCore(runBatterySyncWrapper, "runBatterySync", 10000, this, 2, NULL, 0);
}

void MessageHandler::startWiFiToggleTask() {
    xTaskCreatePinnedToCore(runToggleWiFiTaskWrapper, "toggleWiFiTask", 10000, this, 2, NULL, 0);
}
void MessageHandler::runToggleWiFiTaskWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->toggleWiFiTask();
}
void MessageHandler::runBatterySyncWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runBatterySync();
}
void MessageHandler::runBatterySync() {
    while (true) {
        float batteryPercentage = getBatteryPercentage();
        if (batteryPercentage < BATTERY_LOW_THRESHOLD) {
            if (getBatteryLow() == false) {
                setBatteryLow(true);
                ledInstance->turnOff();
                message_data statusMessage = createStatusMessage();
                pushToSendQueue(statusMessage);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
            // Sleep 5 minutes, then ask master if maintenance mode is active
            turnWifiOff();
            esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
            esp_sleep_enable_timer_wakeup(5ULL * 60ULL * 1000000ULL);
            esp_light_sleep_start();
            turnWifiOn();
            vTaskDelay(2000 / portTICK_PERIOD_MS); // let ESP-NOW settle after wake

            message_data askAdminPresentMessage = createCommandMessage(CMD_ASK_ADMIN_PRESENT);
            memcpy(askAdminPresentMessage.targetAddress, hostAddress, 6);
            pushToSendQueue(askAdminPresentMessage);
            vTaskDelay(1500 / portTICK_PERIOD_MS); // wait for master response

            if (getAdminPresent()) {
                ESP_LOGI("MSG", "Critical battery: maintenance mode active, shimmering for visibility");
                setAdminPresent(0);
                // Repeat candlelight with 10 s cooldown as long as maintenance mode stays active
                do {
                    ledInstance->candleLight(30ULL * 1000000ULL, 30, 80, 30);
                    vTaskDelay(10000 / portTICK_PERIOD_MS); // 10 s dark cooldown between pulses

                    // Refresh admin-present: ask master again each cycle
                    message_data refreshMsg = createCommandMessage(CMD_ASK_ADMIN_PRESENT);
                    memcpy(refreshMsg.targetAddress, hostAddress, 6);
                    pushToSendQueue(refreshMsg);
                    vTaskDelay(1500 / portTICK_PERIOD_MS);
                } while (getAdminPresent());
                // Admin left maintenance mode, fall through, next iteration sleeps 5 min
                ESP_LOGI("MSG", "Maintenance mode ended, returning to critical sleep cycle");
            }
            // If maintenance mode is off, loop back immediately, next iteration sleeps 5 min again
        }
        else {
            setBatteryLow(false);
            message_data statusMessage = createStatusMessage();
            pushToSendQueue(statusMessage);
            vTaskDelay(10 * 60 * 1000 / portTICK_PERIOD_MS); // 10 minutes
        }
    }
}

void MessageHandler::turnWifiOn() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(); // Replace with your SSID and password
    WiFi.setSleep(false); // match boot: modem sleep back on after a nap makes the
                          // radio miss ESP-NOW packets -> intermittent unreachability
    if (esp_now_init() != ESP_OK)
    {
      Serial.println("Error initializing ESP-NOW");
      return;
    }
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);
    addPeer(const_cast<uint8_t*>(broadcastAddress));
}

 void MessageHandler::toggleWiFiTask() {
    while (true) {
        // Turn off Wi-Fi
        /*
        if (nextCommandReceived == true) {
            ESP_LOGI("WiFi", "Turning off Wi-Fi...");
            turnWifiOff();
            vTaskDelay(pdMS_TO_TICKS(wifiSleepTime)); // Wait for 5 seconds (adjust as needed)
            nextCommandReceived = false;
            turnWifiOn();
            message_data askCommandMessage;
            askCommandMessage.messageType = MSG_ASK_COMMAND;
            askCommandMessage.payload.askCommand.batteryPercentage = getBatteryPercentage();
            askCommandMessage.payload.askCommand.perceivedTime = micros()-getTimeOffset();
            memcpy(askCommandMessage.targetAddress, hostAddress, 6);
            esp_now_send(hostAddress, (uint8_t *) &askCommandMessage, sizeof(askCommandMessage));
        }*/
        // Turn Wi-Fi back on      
        // Wait for another 5 seconds (adjust as needed)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


#endif