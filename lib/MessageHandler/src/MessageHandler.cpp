#include "MessageHandler.h"
MessageHandler* MessageHandler::instance = nullptr;

MessageHandler::MessageHandler() {
    configMutex = xSemaphoreCreateMutex();
    receiveQueue = xQueueCreate(100, sizeof(message_data));
    if (receiveQueue == NULL) {
        ESP_LOGE("ERROR", "Failed to create receiveQueue");
    }
    sendQueue = xQueueCreate(10, sizeof(message_data));
    if (sendQueue == NULL) {
        ESP_LOGE("ERROR", "Failed to create sendQueue");
    }
}

void MessageHandler::setup(LedHandler &globalLedInstance) {
    ledInstance = &globalLedInstance;
    xTaskCreatePinnedToCore(handleReceiveWrapper, "handleReceive", 10000, this, 10, &handleReceiveHandle, 0);
    xTaskCreatePinnedToCore(handleSendWrapper, "handleSegnd", 10000, this, 10, &handleSendHandle, 0);
    addPeer(const_cast<uint8_t*>(broadcastAddress));
    version= VERSION;
    #if (DEVICE_MODE == MASTER) 
        //startTimerSyncTask();
    #endif
    #if (DEVICE_MODE == CLIENT)
        addPeer(const_cast<uint8_t*>(hostAddress));
        xTaskCreatePinnedToCore(announceAddressWrapper, "runAnnounceAddress", 10000, this, 2, &announceTaskHandle, 1);
    #endif
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);
    #if (DEVICE_MODE == MASTER)
        ESP_LOGI("MSG", "Master setup");
        handleAddressStruct();
        startAllTimerSyncTask();
         // using const char*
    #endif
    #if (DEVICE_MODE == CLIENT)
        ESP_LOGI("MSG", "Client setup");
        //startWiFiToggleTask();
    
    #endif
    
}



void MessageHandler::pushToRecvQueue(const esp_now_recv_info *mac, const uint8_t *incomingData, int len) {
    if (len != sizeof(message_data)) return;
    message_data *msg = (message_data *)incomingData;
     if (msg->messageType == MSG_TIMER) {
        msg->payload.timer.receiveTime = micros();
     }

    if (xQueueSend(receiveQueue, msg, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE("MSG", "Failed to send data to receive queue");
    }
}


void MessageHandler::pushToSendQueue(message_data& msg) {
    ESP_LOGI("MSG", "Pushing to send queue");
    if (xQueueSend(sendQueue, &msg, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE("MSG", "Failed to send data to send queue");
    }
}

void MessageHandler::handleReceiveWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->handleReceive();
}

void MessageHandler::handleSendWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->handleSend();
}


