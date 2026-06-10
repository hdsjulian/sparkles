
#include "MessageHandler.h"
#if (DEVICE_MODE == CLIENT)
#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"
#include "Ota.h"

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
                    ESP_LOGI("MSG", "Battery percentage: %f", batteryLow.payload.status.batteryPercentage);
                    memcpy(batteryLow.targetAddress, hostAddress, 6);
                    xQueueSend(sendQueue, &batteryLow, portMAX_DELAY);
                    */
                    continue;
                }
                else {
                    message_animation animation = (message_animation)incomingData.payload.animation;
                    ESP_LOGI("MSG", "Received Animation type %d", animation.animationType);

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
                ESP_LOGI("MSG", "Do nothing");
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
                    ESP_LOGI("MSG", "Received message command");
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
                    ESP_LOGI("MSG", "CMD_OTA_UPDATE received — connecting to %s", OTA_WIFI_SSID);
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
                // another client's status broadcast — ignore
            }
            else if (incomingData.messageType == MSG_CLAP) {
                message_clap clapMessage = incomingData.payload.clap;
                ESP_LOGI("MSG", "Received clap message");
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
                    ESP_LOGI("MSG", "Received config data with no distance, not setting distance from center");
                }
                if (configData.xPos != 0.0 || configData.yPos != 0.0) {
                    ledInstance->setLocation(configData.xPos, configData.yPos);
                    ESP_LOGI("MSG", "Setting position to: (%f, %f)", configData.xPos, configData.yPos);
                }
                else {
                    ESP_LOGI("MSG", "Received config data with no position, not setting position");
                }
            }
            else if (incomingData.messageType == MSG_UPDATE_VERSION) {
                message_update_version updateVersionMessage = incomingData.payload.updateVersion;
                if (updateVersionMessage.version >= version && updateVersionMessage.version != version) {
                    ledInstance->blink(esp_timer_get_time(), 100, 5, 200, 255, 127);
                    vTaskDelete(announceTaskHandle);
                    announceTaskHandle = NULL;
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
            ESP_LOGI("MSG", "Board position: %d", timerMessage.addressId);

    }
    ESP_LOGI("LED", "Current position set to: %d", ledInstance->getCurrentPosition());
    unsigned long long timeDiff = timerMessage.receiveTime-getLastReceiveTime();
    unsigned long long timerFrequencyMicros = TIMER_FREQUENCY*1000;
    if (timeDiffAbs(timeDiff, timerFrequencyMicros) < 2500 and timerMessage.lastDelay < 6000) {

        unsigned long long offset;
        if (timerMessage.receiveTime < timerMessage.sendTime) {
            offset = timerMessage.sendTime-timerMessage.receiveTime;
            offsetMultiplier = 1;
        }
        else {
            offset = timerMessage.receiveTime-timerMessage.sendTime;
            offsetMultiplier = -1;
        }
        offsetSum += offset;
        offsetCount++;

        setTimeOffset(offsetMultiplier*(offsetSum / offsetCount));
        if (delayCounter < TIMER_ARRAY_COUNT) {
            delayAverage = (delayAverage * delayCounter + timerMessage.lastDelay) / (delayCounter + 1);    
            ESP_LOGI("MSG", "Delay average: %d", delayAverage);
            delayCounter++;
            
        }
        else {  
            long long correctedOffset = offsetMultiplier * (offsetSum / offsetCount) + offsetMultiplier * (delayAverage / 2);
            setTimeOffset(correctedOffset);
            message_data gotTimerMessage;
            unsigned long long now = esp_timer_get_time();
            long long timeOffset = getTimeOffset();
            gotTimerMessage.messageType = MSG_GOT_TIMER;
            gotTimerMessage.payload.gotTimer.delayAverage = delayAverage;
            gotTimerMessage.payload.gotTimer.batteryPercentage = getBatteryPercentage();
            gotTimerMessage.payload.gotTimer.offset = getTimeOffset();           
            memcpy(gotTimerMessage.targetAddress, hostAddress, 6);
            ESP_LOGI("MSG", "Sending got timer message with perceived time: %lld, delay average: %d, battery percentage: %f", gotTimerMessage.payload.gotTimer.perceivedTime, gotTimerMessage.payload.gotTimer.delayAverage, gotTimerMessage.payload.gotTimer.batteryPercentage);
            xQueueSend(sendQueue, &gotTimerMessage, portMAX_DELAY);
            setTimerSet(true);
            ledInstance->blink(esp_timer_get_time(), 300, 3, 100, 255, 127);
            ESP_LOGI("MSG", "Timer set. time offset: %lld", getTimeOffset());
            startBatterySyncTask();
            delayCounter = 0;
            delayAverage = 0;
          }
    }
    else {
        //ESP_LOGI("MSG", "Time diff too large %lld or delay too large  %d", timeDiff, timerMessage.lastDelay);
        if (timerMessage.lastDelay > 4000) {
            //ESP_LOGI("MSG", "Last delay too large, %d", timerMessage.lastDelay);       
         }
        else {
            //ESP_LOGI("MSG", "Time diff too large: %lld", timeDiff);
        }
    }
    setLastReceiveTime(timerMessage.receiveTime);


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
    message_sleep_wakeup sleepWakeupMessage = incomingData.payload.sleepWakeup;
    ESP_LOGI("MSG", "Going to sleep for %llu microseconds", sleepWakeupMessage.duration);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    ledInstance->blink(esp_timer_get_time(), 100, 4, 160, 255, 127);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    message_animation animationMessage = ledInstance->createAnimation(OFF);
    ledInstance->pushToAnimationQueue(animationMessage);
    turnWifiOff();
    Serial.end();
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_sleep_enable_timer_wakeup(sleepWakeupMessage.duration);
    esp_light_sleep_start();
    Serial.begin(115200);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    ESP_LOGI("MSG", "Woke up");
    turnWifiOn();
    ledInstance->resetLedTask();
    ledInstance->blink(esp_timer_get_time(), 150, 2, 160, 255, 127);
    ESP_LOGI("MSG", "Woke up from sleep, current time: %llu", micros());
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    ESP_LOGI("MSG", "Should be back up");
}

void MessageHandler::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        if (memcmp(mac_addr, broadcastAddress, 6) == 0) {
            ESP_LOGI("MSG", "Broadcast message sent");
        }
        else {
            ESP_LOGI("MSG", "Message sent to %02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        }
    }
    else {
        ESP_LOGI("MSG", "Send failed");
    }
}

void MessageHandler::onDataRecv(const esp_now_recv_info * mac, const uint8_t *incomingData, int len) {
    unsigned long long receiveTime = esp_timer_get_time();
    MessageHandler& instance = getInstance();
    if (incomingData[0] == MSG_TIMER) {
        // Learn master MAC from the first MSG_TIMER we receive
        if (!instance.hostAddressLearned) {
            memcpy(instance.hostAddress, mac->src_addr, 6);
            instance.hostAddressLearned = true;
            instance.addPeer(instance.hostAddress);
        }
        message_data* messageData = (message_data*)incomingData;
        messageData->payload.timer.receiveTime = receiveTime;
    }
    if (incomingData[0] == MSG_ANIMATION && instance.getBatteryPercentage() > BATTERY_LOW_THRESHOLD) {
        message_data* messageData = (message_data*)incomingData;
        if (messageData->payload.animation.animationType == MIDI || messageData->payload.animation.animationType == BACKGROUND_SHIMMER) {
            LedHandler& ledInstance = LedHandler::getInstance(); 
            if (messageData->payload.animation.animationType == BACKGROUND_SHIMMER) {
                auto& shimmer = messageData->payload.animation.animationParams.backgroundShimmer;
                if (shimmer.value > 0) {
                    uint8_t mac[6]; WiFi.macAddress(mac);
                    uint8_t range = (uint8_t)(shimmer.value * 0.1f);
                    int8_t offset = (range > 0) ? (int8_t)((mac[5] % (range * 2 + 1)) - range) : 0;
                    shimmer.value = (uint8_t)constrain((int)shimmer.value + offset, 0, 255);
                }
            }
            ledInstance.pushToAnimationQueue(messageData->payload.animation);
            return;
            
        }
        
    }
    else if (incomingData[0] == MSG_ANIMATION && instance.getBatteryPercentage() <= BATTERY_LOW_THRESHOLD) {
        ESP_LOGI("RECV", "Threshold too low %f of %f", instance.getBatteryPercentage(), BATTERY_LOW_THRESHOLD);
    }

    instance.pushToRecvQueue(mac, incomingData, len);

    
}


void MessageHandler::announceAddressWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runAnnounceAddress();
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
                // Admin left maintenance mode — fall through, next iteration sleeps 5 min
                ESP_LOGI("MSG", "Maintenance mode ended, returning to critical sleep cycle");
            }
            // If maintenance mode is off, loop back immediately — next iteration sleeps 5 min again
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
    if (esp_now_init() != ESP_OK)
    {
      Serial.println("Error initializing ESP-NOW");
      return;
    }
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);
    addPeer(const_cast<uint8_t*>(broadcastAddress));
    Serial.println("should initialize");
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