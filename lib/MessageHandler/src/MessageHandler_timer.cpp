#if DEVICE_MODE == MASTER
#include "MessageHandler.h"
#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"


void MessageHandler::startTimerSyncTask() {
    xTaskCreatePinnedToCore(runTimerSyncWrapper, "runTimerSync", 10000, this, 2, NULL, 0);
}
void MessageHandler::runTimerSyncWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runTimerSync();
}

void MessageHandler::runAllTimerSyncWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    for (int j = 0; j < 3; j++) {
        for (int i = 0; i < NUM_DEVICES; i++) {
            if (memcmp(messageHandlerInstance->getItemFromAddressList(i).address, messageHandlerInstance->emptyAddress, 6) == 0) {
                break;
            }
            activeStatus status = messageHandlerInstance->getActiveStatus(i);
            if (status == INACTIVE) {
                messageHandlerInstance->setCurrentTimerIndex(i);
                messageHandlerInstance->setTimerReset(true);
                messageHandlerInstance->runTimerSync();
            }
            else {
                continue;
            }
        }
    }
}


void MessageHandler::runTimerSync() {
    message_timer timerMessage;
    message_data messageData;
    messageData.messageType = MSG_TIMER;
    setSettingTimer(true);
    setTimerCounter(0);
    setLastTimerCounter();
    int timerIndex  = getCurrentTimerIndex();
    if (timerIndex > -1) {
        addPeer(addressList[timerIndex].address);
    }
    while (getTimerSet() == false) {
        
        timerMessage.counter = incrementTimerCounter();
        timerMessage.sendTime = micros();
        timerMessage.lastDelay = getLastDelay();
        timerMessage.reset = getTimerReset();
        timerMessage.addressId = getCurrentTimerIndex();
        memcpy(&messageData.payload.timer, &timerMessage, sizeof(timerMessage));
        setLastSendTime(timerMessage.sendTime);
        if (timerIndex == -1) {
            esp_now_send(broadcastAddress, (uint8_t *) &messageData, sizeof(messageData));
        }
        else if (timerIndex > -1) {
            esp_now_send(addressList[timerIndex].address, (uint8_t *) &messageData, sizeof(messageData));
        }
        if (getLastTimerCounter() < getTimerCounter()-5 && timerIndex > -1) {
            setUnavailable(getCurrentTimerIndex()); 
            ESP_LOGI("TIMER", "Setting unavailable. Last counter: %d, current counter: %d, index: %d", getLastTimerCounter(), getTimerCounter(), timerIndex);
            esp_now_del_peer(addressList[timerIndex].address);
            vTaskDelete(timerSyncHandle);

        }
        vTaskDelay(TIMER_FREQUENCY/portTICK_PERIOD_MS);
    }
}



#endif