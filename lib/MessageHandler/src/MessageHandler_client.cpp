
#include "MessageHandler.h"
#if (DEVICE_MODE == CLIENT) 
#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"


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
                    batteryLow.messageType = MSG_STATUS;
                    batteryLow.payload.status.batteryPercentage = getBatteryPercentage();
                    ESP_LOGI("MSG", "Battery percentage: %f", batteryLow.payload.status.batteryPercentage);
                    memcpy(batteryLow.address, hostAddress, 6);
                    xQueueSend(sendQueue, &batteryLow, portMAX_DELAY);
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
        }
        else {
            message_data gotTimerMessage;
            gotTimerMessage.messageType = MSG_GOT_TIMER;
            gotTimerMessage.payload.gotTimer.delayAverage = delayAverage;
            gotTimerMessage.payload.gotTimer.batteryPercentage = getBatteryPercentage();
            memcpy(gotTimerMessage.address, hostAddress, 6);
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
          }
    }
    else {
        ESP_LOGI("MSG", "Time diff too large %lld or delay too large  %d", timeDiff, timerMessage.lastDelay);
    }
    setLastReceiveTime(timerMessage.receiveTime);


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
    memcpy(messageData.address, hostAddress, 6);
    WiFi.macAddress(messageData.payload.address.address);
    while (getAddressAnnounced() == false) {    
        ESP_LOGI("MSG", "Announcing address");    
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
                    esp_now_send(messageData.address, (uint8_t *) &messageData, sizeof(messageData));
                    break;
                case MSG_ADDRESS:
                    esp_now_send(messageData.address, (uint8_t *) &messageData, sizeof(messageData));
                    break;
                case MSG_STATUS:
                    esp_now_send(messageData.address, (uint8_t *) &messageData, sizeof(messageData));
                    break;
                default:
                    break;
            }        
        }
    }
}

#endif