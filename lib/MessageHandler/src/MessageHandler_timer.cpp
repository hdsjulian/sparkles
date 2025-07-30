#if DEVICE_MODE == MASTER
#include "MessageHandler.h"
#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"


void MessageHandler::startTimerSyncTask() {
    if (timerSyncHandle == NULL)
        {

            xTaskCreatePinnedToCore(runTimerSyncWrapper, "runTimerSync", 10000, this, 2, &timerSyncHandle, 0);
        }
}
void MessageHandler::startClapSyncTask() {
    if (clapSyncHandle == NULL)
        {
            xTaskCreatePinnedToCore(runClapSyncWrapper, "runClapSync", 10000, this, 2, &clapSyncHandle, 1);
        }
}

void MessageHandler::startAllTimerSyncTask() {
    ESP_LOGI("TIMER", "Starting all timer sync task");
    if (allTimerSyncHandle == NULL)
        {
            xTaskCreatePinnedToCore(runAllTimerSyncWrapper, "runAllTimerSync", 10000, this, 2, &allTimerSyncHandle, 0);
        }
}
void MessageHandler::runTimerSyncWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runTimerSync();
}
void MessageHandler::runClapSyncWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runClapSync();
}

void MessageHandler::runAllTimerSyncWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->setAddressListInactive();
    ESP_LOGI("TIMER", "Starting all timer sync wrapper");
    int numAdresses = 0;
    for (int j = 0; j < 3; j++) {
        ESP_LOGI("TIMER", "Starting all timer sync, iteration %d", j);
        numAdresses = 0;
        for (int i = 0; i < NUM_DEVICES; i++) {
            if (memcmp(messageHandlerInstance->getItemFromAddressList(i).address, messageHandlerInstance->emptyAddress, 6) == 0) {
                break;
            }
            else {
                numAdresses++;
            }
            ESP_LOGI("TIMER", "Checking address %02x:%02x:%02x:%02x:%02x:%02x", messageHandlerInstance->addressList[i].address[0], messageHandlerInstance->addressList[i].address[1], messageHandlerInstance->addressList[i].address[2], messageHandlerInstance->addressList[i].address[3], messageHandlerInstance->addressList[i].address[4], messageHandlerInstance->addressList[i].address[5]);
            activeStatus status = messageHandlerInstance->getActiveStatus(i);
            if (status == INACTIVE) {
                ESP_LOGI("TIMER", "Starting timer sync for index %d with address %02x:%02x:%02x:%02x:%02x:%02x", i, messageHandlerInstance->addressList[i].address[0], messageHandlerInstance->addressList[i].address[1], messageHandlerInstance->addressList[i].address[2], messageHandlerInstance->addressList[i].address[3], messageHandlerInstance->addressList[i].address[4], messageHandlerInstance->addressList[i].address[5]);
                messageHandlerInstance->setCurrentTimerIndex(i);
                messageHandlerInstance->setTimerReset(true);
                messageHandlerInstance->runTimerSync();
            }
            else {
                ESP_LOGI("TIMER", "Skipping timer sync for index %d with address %02x:%02x:%02x:%02x:%02x:%02x, status: %d", i, messageHandlerInstance->addressList[i].address[0], messageHandlerInstance->addressList[i].address[1], messageHandlerInstance->addressList[i].address[2], messageHandlerInstance->addressList[i].address[3], messageHandlerInstance->addressList[i].address[4], messageHandlerInstance->addressList[i].address[5], status);
                
                continue;
            }
        }
    }
    if (numAdresses > 2) {
        messageHandlerInstance->startAnimationLoopTask();
    }
    messageHandlerInstance->allTimerSyncHandle = NULL;
    vTaskDelete(NULL);
}

void MessageHandler::runClapSync() {
    ESP_LOGI("CLAP", "Starting clap sync");
    message_data messageData;
    messageData.messageType = MSG_COMMAND;
    messageData.payload.command.commandType = CMD_MESSAGE;
    memcpy(messageData.targetAddress, clapDeviceAddress, 6);
    setSettingClapSync(true);
    TickType_t lastWakeTime = xTaskGetTickCount();
    int delayAverage = 0;
    addPeer(clapDeviceAddress);
    for (int i = 0; i < 11; i++) {
        ESP_LOGI("CLAP", "Clap sync iteration %d", i);
        if (i > 0) {
            delayAverage += getLastDelay();
        }
        ESP_LOGI("CLAP", "Last delay: %d", getLastDelay());
        lastWakeTime = xTaskGetTickCount();
         setLastSendTime(micros());
        esp_now_send(clapDeviceAddress, (uint8_t *) &messageData, sizeof(messageData));
         vTaskDelayUntil(&lastWakeTime, TIMER_FREQUENCY/portTICK_PERIOD_MS);
    }
    removePeer(clapDeviceAddress);
    delayAverage /= 10;
    setClapDeviceDelay(delayAverage / 2);
    ESP_LOGI("CLAP", "Clap device delay set to %d", getClapDeviceDelay());
    ESP_LOGI("CLAP", "Clap sync finished");
    clapSyncHandle = NULL;
    vTaskDelete(NULL);
}



void MessageHandler::runTimerSync() {
    message_timer timerMessage;
    message_data messageData;
    messageData.messageType = MSG_TIMER;
    setSettingTimer(true);
    setTimerCounter(0);
    setLastTimerCounter();
    int timerIndex  = getCurrentTimerIndex();
    TickType_t lastWakeTime = xTaskGetTickCount();

    if (timerIndex > -1) {
        addPeer(addressList[timerIndex].address);
        ESP_LOGI("TIMER", "Starting timer sync for index %d with address %02x:%02x:%02x:%02x:%02x:%02x", timerIndex, addressList[timerIndex].address[0], addressList[timerIndex].address[1], addressList[timerIndex].address[2], addressList[timerIndex].address[3], addressList[timerIndex].address[4], addressList[timerIndex].address[5]);
        //setCommand(messageData, addressList[timerIndex].address);
    }
    else {
        ESP_LOGI("TIMER", "Timer Sync: Addres is -1");
    }
    unsigned long long lastTick = 0;
    while (getSettingTimer() == true) {
        lastWakeTime = xTaskGetTickCount();
        timerMessage.counter = incrementTimerCounter();
        timerMessage.sendTime = micros();
        timerMessage.lastDelay = getLastDelay();
        timerMessage.reset = getTimerReset();
        timerMessage.addressId = getCurrentTimerIndex();
        memcpy(&messageData.payload.timer, &timerMessage, sizeof(timerMessage));
        timerMessage.sendTime = micros();
        setLastSendTime(timerMessage.sendTime);
        if (timerIndex == -1) {
            esp_now_send(broadcastAddress, (uint8_t *) &messageData, sizeof(messageData));
        }
        else if (timerIndex > -1) {
            esp_now_send(addressList[timerIndex].address, (uint8_t *) &messageData, sizeof(messageData));
            if (!esp_now_is_peer_exist(addressList[timerIndex].address)) {
                ESP_LOGI("ESP-NOW", "Peer does not exist");
            }
        }

        if ((getLastTimerCounter() < getTimerCounter()-5 || getTimerCounter() > 100)   && timerIndex > -1) {
            setUnavailable(getCurrentTimerIndex()); 
            removePeer(addressList[timerIndex].address);
            setSettingTimer(false);
            ESP_LOGI("TIMER", "Setting unavailable. Last counter: %d, current counter: %d, index: %d", getLastTimerCounter(), getTimerCounter(), timerIndex);
        }
        
        //ESP_LOGI("TIMER", "TIMER %d SENT AT %llu - exact difference %llu", timerMessage.counter, timerMessage.sendTime, timerMessage.sendTime-lastTick);
        //ESP_LOGI("TIMER", "Last Delay was %d", timerMessage.lastDelay);
        lastTick = timerMessage.sendTime;
         vTaskDelayUntil(&lastWakeTime, TIMER_FREQUENCY/portTICK_PERIOD_MS);
    }
    if (getSettingTimer() == false)   {
        ESP_LOGI("TIMER", "Timer sync finished for index %d with address %02x:%02x:%02x:%02x:%02x:%02x", timerIndex, addressList[timerIndex].address[0], addressList[timerIndex].address[1], addressList[timerIndex].address[2], addressList[timerIndex].address[3], addressList[timerIndex].address[4], addressList[timerIndex].address[5]);
        if (!esp_now_is_peer_exist(addressList[timerIndex].address)) {
            ESP_LOGI("ESP-NOW", "Peer does not exist, can't delete");
        }
        removePeer(addressList[timerIndex].address);
        if (xTaskGetCurrentTaskHandle() == timerSyncHandle) {
            timerSyncHandle = NULL;
            vTaskDelete(NULL); // Safely delete self
        } else if (timerSyncHandle != NULL) {
            vTaskDelete(timerSyncHandle); // Delete from another task
            timerSyncHandle = NULL;
        }
        if (addressList[timerIndex].active == INACTIVE) {
            addressList[timerIndex].active = ACTIVE;
            addressList[timerIndex].lastUpdateTime = millis();
        }
    }
    
}

void MessageHandler::setAddressListInactive() {
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (memcmp(addressList[i].address, emptyAddress, 6) == 0) {
            break;
        }
        else {
            addressList[i].active = INACTIVE;
            addressList[i].batteryPercentage = 0;
            addressList[i].lastUpdateTime = 0;
        }
    } 
}


#endif