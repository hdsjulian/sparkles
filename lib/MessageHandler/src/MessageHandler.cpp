#include "MessageHandler.h"
MessageHandler* MessageHandler::instance = nullptr;

MessageHandler::MessageHandler() {
    configMutex = xSemaphoreCreateMutex();
    receiveQueue = xQueueCreate(300, sizeof(message_data));
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
    xTaskCreatePinnedToCore(handleReceiveWrapper, "handleReceive", 10000, this, 5, &handleReceiveHandle, 0);
    xTaskCreatePinnedToCore(handleSendWrapper, "handleSegnd", 10000, this, 5, &handleSendHandle, 1);
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
        bool noClientList = !LittleFS.exists("/clientAddress");
        handleAddressStruct();
        startBroadcastSettleTask();
        if (noClientList) {
            ESP_LOGI("MSG", "No client list found, broadcasting CMD_REANNOUNCE");
            delay(500);
            broadcastReannounce();
        }
    #endif
    #if (DEVICE_MODE == CLIENT)
        ESP_LOGI("MSG", "Client setup");
        //startWiFiToggleTask();
    
    #endif
    
}



void MessageHandler::pushToRecvQueue(const esp_now_recv_info *mac, const uint8_t *incomingData, int len) {
    if (len < 1 || len > (int)sizeof(message_data)) {
        ESP_LOGW("RECV", "Bad size len=%d sizeof=%d, dropping", len, (int)sizeof(message_data));
        return;
    }
    message_data msg;
    memcpy(&msg, incomingData, len);
    // MSG_TIMER receiveTime is already stamped 64-bit in onDataRecv, don't re-stamp here
    if (xQueueSend(receiveQueue, &msg, 0) != pdTRUE) {
        ESP_LOGW("MSG", "Receive queue full, dropping message type %d", msg.messageType);
    }
}


void MessageHandler::pushToSendQueue(message_data& msg) {
    if (xQueueSend(sendQueue, &msg, pdMS_TO_TICKS(200)) != pdTRUE) {
        ESP_LOGE("MSG", "Send queue full, message dropped");
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


