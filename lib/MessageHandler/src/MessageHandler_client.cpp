
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
                ledInstance->setMidiParams(midiParamsMessage);
                ESP_LOGI("MIDI", "Received Midi Params minVal %d, maxVal %d, minRms %.2f, maxRMS %.2f", midiParamsMessage.valMin, midiParamsMessage.valMax, midiParamsMessage.rmsMin, midiParamsMessage.rmsMax);
            }
            else if (incomingData.messageType == MSG_COMMAND) {
                message_command commandMessage = (message_command)incomingData.payload.command;
                if (commandMessage.commandType == CMD_START_CALIBRATION || commandMessage.commandType == CMD_START_DISTANCE_CALIBRATION || CMD_CONTINUE_CALIBRATION || CMD_CONTINUE_DISTANCE_CALIBRATION) {
                    startCalibrationClient();
                }
                if (commandMessage.commandType == CMD_TEST_CALIBRATION) {
                    setCalibrationTest(true);
                    startCalibrationClient();
                }
                if (commandMessage.commandType == CMD_CANCEL_CALIBRATION || CMD_END_CALIBRATION) {
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
                    ledInstance->blink(micros(), 100, 5, 200, 255, 127);
                    vTaskDelete(announceTaskHandle);
                    announceTaskHandle = NULL;
                    OTAHandler::getInstance().setup();
                    OTAHandler::getInstance().performUpdate();
                }
                else {
                    ESP_LOGI("MSG", "Received update version message with lower version, ignoring");
                }
            }

            else {
                ESP_LOGI("MSG", "Unknown message type ");
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
            unsigned long long now = micros();
            long long timeOffset = getTimeOffset();
            gotTimerMessage.messageType = MSG_GOT_TIMER;
            gotTimerMessage.payload.gotTimer.delayAverage = delayAverage;
            gotTimerMessage.payload.gotTimer.batteryPercentage = getBatteryPercentage();
            gotTimerMessage.payload.gotTimer.offset = getTimeOffset();           
            memcpy(gotTimerMessage.targetAddress, hostAddress, 6);
            ESP_LOGI("MSG", "Sending got timer message with perceived time: %lld, delay average: %d, battery percentage: %f", gotTimerMessage.payload.gotTimer.perceivedTime, gotTimerMessage.payload.gotTimer.delayAverage, gotTimerMessage.payload.gotTimer.batteryPercentage);
            xQueueSend(sendQueue, &gotTimerMessage, portMAX_DELAY);
            setTimerSet(true);
            ledInstance->blink(micros(), 300, 3, 100, 255, 127);
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
    ledInstance->blink(micros(), 100, 4, 160, 255, 127);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    message_animation animationMessage = ledInstance->createAnimation(OFF);
    ledInstance->pushToAnimationQueue(animationMessage);
    turnWifiOff();
    esp_sleep_enable_timer_wakeup(sleepWakeupMessage.duration); // Convert seconds to microseconds
    esp_light_sleep_start();
    ESP_LOGI("MSG", "Woke up");
    turnWifiOn();
    ledInstance->resetLedTask();
    ledInstance->blink(micros(), 150, 2, 160, 255, 127);
    ESP_LOGI("MSG", "Woke up from sleep, current time: %llu", micros());
    Serial.begin(115200);
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
    unsigned long long receiveTime = micros();
    MessageHandler& instance = getInstance();
    if (incomingData[0] == MSG_TIMER) {
        message_data* messageData = (message_data*)incomingData;
        messageData->payload.timer.receiveTime = receiveTime;
    }
    if (incomingData[0] == MSG_ANIMATION && instance.getBatteryPercentage() > BATTERY_LOW_THRESHOLD) {
        message_data* messageData = (message_data*)incomingData;
        if (messageData->payload.animation.animationType == MIDI || messageData->payload.animation.animationType == BACKGROUND_SHIMMER) {
            LedHandler& ledInstance = LedHandler::getInstance(); 
            if (messageData->payload.animation.animationType == BACKGROUND_SHIMMER) {
                ESP_LOGI("RECV", "Received background shimmer animation with hue %d, saturation %d, value %d", messageData->payload.animation.animationParams.backgroundShimmer.hue, messageData->payload.animation.animationParams.backgroundShimmer.saturation, messageData->payload.animation.animationParams.backgroundShimmer.value);
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
    memcpy(messageData.targetAddress, hostAddress, 6);
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
                    now = micros();
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
        float batteryPercentage = getBatteryPercentage(); // Use your actual battery reading function
        if (batteryPercentage < 10.0 && false) {
            if (getBatteryLow() == false) {
                setBatteryLow(true);
                ledInstance->turnOff();
                message_data statusMessage = createStatusMessage();
                esp_now_send(hostAddress, (uint8_t *) &statusMessage, sizeof(statusMessage));
                vTaskDelay(1000 / portTICK_PERIOD_MS); // Wait for 1 second before checking again
            }   
            message_data askAdminPresentMessage = createCommandMessage(CMD_ASK_ADMIN_PRESENT);
            memcpy(askAdminPresentMessage.targetAddress, hostAddress, 6);
            esp_now_send(hostAddress, (uint8_t *) &askAdminPresentMessage, sizeof(askAdminPresentMessage));
            vTaskDelay(1000 / portTICK_PERIOD_MS); // Wait for 1
            if (getAdminPresent()) {
                ESP_LOGI("MSG", "Admin present, not going to sleep");
                setAdminPresent(0);
                ledInstance->blink(micros(), 200, 3, 0, 255, 255); // Skip the sleep if admin is present
                vTaskDelay(60000 / portTICK_PERIOD_MS); // Wait for 1 minute before checking again
            }
            else {
                turnWifiOff();
                //go to sleep for 5 minutes
                esp_sleep_enable_timer_wakeup(5 * 60 * 1000000); //
                esp_light_sleep_start();
                turnWifiOn();
            }

        }
        else {
        setBatteryLow(false);
        message_data statusMessage = createStatusMessage();
        esp_now_send(hostAddress, (uint8_t *) &statusMessage, sizeof(statusMessage));
        vTaskDelay(30 * 60 * 1000 / portTICK_PERIOD_MS); // 30 minutes

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
    addPeer(const_cast<uint8_t*>(hostAddress));
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