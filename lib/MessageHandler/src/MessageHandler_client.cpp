
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
                handleTimer(incomingData);            
            
            if (incomingData.messageType == MSG_ANIMATION) {
                float batteryPercentage = getBatteryPercentage();
                if (batteryPercentage < 5) {
                    message_data batteryLow;
                    ESP_LOGI("MSG", "Battery low, percentage: %f", batteryPercentage);
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
            else if (incomingData.messageType == MSG_COMMAND) {
                message_command commandMessage = (message_command)incomingData.payload.command;
                ESP_LOGI("MSG", "Received command message with type: %d", commandMessage.commandType);
                if (commandMessage.commandType == CMD_START_CALIBRATION) {
                    ESP_LOGI("MSG", "Starting calibration");
                    startCalibrationClient();
                }
                if (commandMessage.commandType == CMD_SET_ADMIN_NOT_PRESENT) {
                    setAdminPresent(false);
                }
                if (commandMessage.commandType == CMD_SET_ADMIN_PRESENT) {
                    setAdminPresent(true);
                    ESP_LOGI("MSG", "Admin present, not going to sleep");
                }

                
            }
            else if (incomingData.messageType == MSG_CLAP) {
                message_clap clapMessage = incomingData.payload.clap;
                startClapTask();
            }
            else if (incomingData.messageType == MSG_CONFIG_DATA) {
                message_config_data configData = incomingData.payload.configData;
                ESP_LOGI("MSG", "Received config data for board ID: %d", configData.boardId);
                setBoardPosition(configData.boardId, configData.xPos, configData.yPos);
            }
            else if (incomingData.messageType == MSG_UPDATE_VERSION) {
                message_update_version updateVersionMessage = incomingData.payload.updateVersion;
                if (updateVersionMessage.version >= version && updateVersionMessage.version != version) {
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
    unsigned long long timeDiff = timerMessage.receiveTime-getLastReceiveTime();
    unsigned long long timerFrequencyMicros = TIMER_FREQUENCY*1000;
    if (timeDiffAbs(timeDiff, timerFrequencyMicros) < 1000 and timerMessage.lastDelay < 4000) {
        if (delayCounter < TIMER_ARRAY_COUNT) {
            delayAverage = (delayAverage * delayCounter + timerMessage.lastDelay) / (delayCounter + 1);    
            delayCounter++;    
            ESP_LOGI("MSG", "Delay average: %d, Delay Counter %d", delayAverage, delayCounter);

        }
        else {
            message_data gotTimerMessage;
            gotTimerMessage.messageType = MSG_GOT_TIMER;
            gotTimerMessage.payload.gotTimer.delayAverage = delayAverage;
            gotTimerMessage.payload.gotTimer.batteryPercentage = getBatteryPercentage();
            memcpy(gotTimerMessage.targetAddress, hostAddress, 6);
            ESP_LOGI("MSG", "Delay average: %d", delayAverage);
            ESP_LOGI("MSG", "Battery percentage: %f", gotTimerMessage.payload.gotTimer.batteryPercentage);
            setTimeOffset(timerMessage.sendTime, timerMessage.receiveTime, delayAverage);
            xQueueSend(sendQueue, &gotTimerMessage, portMAX_DELAY);
            setTimerSet(true);
            message_animation animation;
            animation.animationType = BLINK;
            animation.animationParams.blink.duration = 300;
            animation.animationParams.blink.startTime = micros();
            animation.animationParams.blink.repetitions = 3;
            animation.animationParams.blink.hue = 100;
            animation.animationParams.blink.saturation = 255;
            animation.animationParams.blink.brightness = 127;
            ESP_LOGI("MSG", "Animation Type: %d", animation.animationType);
            ESP_LOGI("MSG", "...");
            ledInstance->pushToAnimationQueue(animation);
            ESP_LOGI("MSG", "Timer set. time offset: %lld", getTimeOffset());
            startBatterySyncTask();
            delayCounter = 0;
            delayAverage = 0;
          }
    }
    else {
        ESP_LOGI("MSG", "Time diff too large %lld or delay too large  %d", timeDiff, timerMessage.lastDelay);
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
    message_animation animationMessage = ledInstance->createAnimation(OFF);
    ledInstance->pushToAnimationQueue(animationMessage);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    esp_sleep_enable_timer_wakeup(sleepWakeupMessage.duration); // Convert seconds to microseconds
    esp_light_sleep_start();
    turnWifiOn();
}

void MessageHandler::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    /*
    if (status == ESP_NOW_SEND_SUCCESS) {
        if (memcmp(mac_addr, this, 6) == 0) {
            ESP_LOGI("MSG", "Broadcast message sent");
        }
        else {
            ESP_LOGI("MSG", "Message sent to %02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        }
    }
    else {
        ESP_LOGI("MSG", "Send failed");
    }*/
}

void MessageHandler::onDataRecv(const esp_now_recv_info * mac, const uint8_t *incomingData, int len) {
    MessageHandler& instance = getInstance();
    if (incomingData[0] == MSG_TIMER) {
        message_data* messageData = (message_data*)incomingData;
        messageData->payload.timer.receiveTime = micros();
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
    ESP_LOGI("MSG", "Announcing address with version: %s", version.toString().c_str());
    messageData.payload.address.version = version;
    ESP_LOGI("MSG", "Version is now : %s", version.toString().c_str());
    WiFi.macAddress(messageData.payload.address.address);
    while (getAddressAnnounced() == false) {    
        ESP_LOGI("MSG", "Announcing address");  
        ESP_LOGI("MSG", "Version is now : %s", version.toString().c_str());
  
        xQueueSend(sendQueue, &messageData, portMAX_DELAY);
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}


void MessageHandler::handleSend() {
    message_data messageData;
    while(true) {
        if (xQueueReceive(sendQueue, &messageData, portMAX_DELAY) == pdTRUE) {
            switch (messageData.messageType) {
                case MSG_GOT_TIMER:
                    esp_now_send(messageData.targetAddress, (uint8_t *) &messageData, sizeof(messageData));
                    break;
                case MSG_ADDRESS:
                    esp_now_send(messageData.targetAddress, (uint8_t *) &messageData, sizeof(messageData));
                    break;
                case MSG_STATUS:
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
                ledInstance->createFlash(micros(), 200, 100, 0, 255, 255); // Skip the sleep if admin is present
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
    WiFi.mode(WIFI_STA);
    Serial.println("should initialize");
}
void MessageHandler::turnWifiOff() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
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