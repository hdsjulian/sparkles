#include "MessageHandler.h"

#if DEVICE_MODE == MASTER
#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"



void MessageHandler::handleAddressStruct() {
    if (!readStructsFromFile(addressList, NUM_DEVICES,  "/clientAddress")) {
        ESP_LOGE("FS", "Failed to read client addresses from file");
    }
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (memcmp(addressList[i].address, emptyAddress, 6) == 0) {
            ESP_LOGI("FS", "Empty address found at %d", i);
            break;
        }
        else {
            addressList[i].active = INACTIVE;
            addressList[i].batteryPercentage = 0;
            addressList[i].lastUpdateTime = 0;
            ESP_LOGI("FS", "Address found at %d", i);
            ESP_LOGI("FS", "Address: %02x:%02x:%02x:%02x:%02x:%02x", addressList[i].address[0], addressList[i].address[1], addressList[i].address[2], addressList[i].address[3], addressList[i].address[4], addressList[i].address[5]);
        }
    } 
}



void MessageHandler::handleReceive() {
    message_data incomingData;
    
    while (true) {

        if (xQueueReceive(receiveQueue, &incomingData, portMAX_DELAY) == pdTRUE) {
            //BETA
            ESP_LOGI("MSG", "Received from queue");
            if (incomingData.messageType == MSG_ADDRESS) {
                vTaskDelay(1000/portTICK_PERIOD_MS);
                //ESP_LOGI("MSG", "Received animation message");
                //ESP_LOGI("MSG", "Animation type: %d", incomingData[1]);
                //message_animate *animation = (message_animate *)incomingData;
                //ESP_LOGI("MSG", "Midi note: %d", animation->animationParams.midi.note);
                //ESP_LOGI("MSG", "Midi velocity: %d", animation->animationParams.midi.velocity);
                if (incomingData.payload.address.version != version) {
                    ESP_LOGI("MSG", "Received address with lower version, ignoring");
                    
                    return;
                }
                else {
                    message_address address = (message_address)incomingData.payload.address;
                    int index = addOrGetAddressId(address.address);
                    setCurrentTimerIndex(index);
                    startTimerSyncTask();
                }
            }
            else if (incomingData.messageType == MSG_GOT_TIMER) {
                int timerIndex = getCurrentTimerIndex();
                vTaskDelete(timerSyncHandle);
                removePeer(addressList[getCurrentTimerIndex()].address);
                timerSyncHandle = NULL;
                addressList[timerIndex].active = ACTIVE;
                addressList[timerIndex].batteryPercentage = incomingData.payload.gotTimer.batteryPercentage;
                ESP_LOGI("MSG", "Battery Percentage %.2f",incomingData.payload.gotTimer.batteryPercentage );
                addressList[timerIndex].lastUpdateTime = millis();
                setCurrentTimerIndex(-1);
                setSettingTimer(false);
                writeStructsToFile(addressList, NUM_DEVICES, "/clientAddress");
                sendSystemStatus();
                WebServer& webServerInstance = WebServer::getInstance(&LittleFS);
                webServerInstance.updateAddress(timerIndex);

            }
            else if (incomingData.messageType == MSG_STATUS) {
                ESP_LOGI("MSG", "Received status message");
                ESP_LOGI("MSG", "Battery Percentage %.2f",incomingData.payload.status.batteryPercentage );
                for (int i = 0; i < NUM_DEVICES; i++) {
                    ESP_LOGI("MSG", "Address %d: %02x:%02x:%02x:%02x:%02x:%02x", i, addressList[i].address[0], addressList[i].address[1], addressList[i].address[2], addressList[i].address[3], addressList[i].address[4], addressList[i].address[5]);
                    ESP_LOGI("MSG", "Incoming address: %02x:%02x:%02x:%02x:%02x:%02x", incomingData.senderAddress[0], incomingData.senderAddress[1], incomingData.senderAddress[2], incomingData.senderAddress[3], incomingData.senderAddress[4], incomingData.senderAddress[5]);
                    if (memcmp(addressList[i].address, incomingData.senderAddress, 6) == 0) {
                        addressList[i].batteryPercentage = incomingData.payload.status.batteryPercentage;
                        addressList[i].lastUpdateTime = millis();
                        WebServer& webServerInstance = WebServer::getInstance(&LittleFS);
                        ESP_LOGI("MSG", "Updating address %d", i);
                        webServerInstance.updateAddress(i);      
                        break;
                    }
                }
                //writeStructsToFile(addressList, NUM_DEVICES, "/clientAddress");
            }

            // Doesn't need to be sent so nothing will happen right now
            // #BATSAVE
            /*
            else if (incomingData.messageType == MSG_ASK_COMMAND) {
                ESP_LOGI("MSG", "Received ask command message");
                ESP_LOGI("MSG", "Battery Percentage %.2f",incomingData.payload.askCommand.batteryPercentage );
                ESP_LOGI("MSG", "Perceived Time %llu", incomingData.payload.askCommand.perceivedTime);
                retrieveCommand(incomingData.senderAddress);
            }*/

            else if (incomingData.messageType == MSG_CLAP) {
                ESP_LOGI("MSG", "Received clap message");
                ESP_LOGI("MSG", "Clap time %llu", incomingData.payload.clap.clapTime);
                int clapIndex = getClapIndex();
                if (memcmp(incomingData.senderAddress, clapDeviceAddress, 6) == 0) {
                    clapTable[getClapIndex()].clapTime = incomingData.payload.clap.clapTime;
                }
                else {
                    for (int i = 0; i < NUM_DEVICES; i++) {
                        if (memcmp(addressList[i].address, incomingData.senderAddress, 6) == 0) {
                            addressList[i].distances[clapIndex] = convertMicrosToMeters(clapTable[clapIndex].clapTime - incomingData.payload.clap.clapTime);
                            break;
                        }
                    }
                }
            }
            else if (incomingData.messageType == MSG_SLEEP_WAKEUP) {
                //handleSleepWakeup(incomingData);
            }
            else if (incomingData.messageType == MSG_SYSTEM_STATUS) {
                ESP_LOGI("MSG", "Received system status message");
                ESP_LOGI("MSG", "Number of devices: %d", incomingData.payload.systemStatus.numDevices);
            }
            else {
                ESP_LOGI("MSG", "Unknown message type ");
            }
        }
    }
}

void MessageHandler::handleSend() {
    message_data messageData;

    while(true) {
        if (xQueueReceive(sendQueue, &messageData, portMAX_DELAY) == pdTRUE) {
            switch (messageData.messageType) {
                case MSG_ADDRESS:
                    esp_now_send(messageData.targetAddress, (uint8_t *) &messageData, sizeof(messageData));
                    break;
                default:
                    esp_now_send(messageData.targetAddress, (uint8_t *) &messageData, sizeof(messageData));
                    break;
            }        
        }
    }
}

void MessageHandler::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    MessageHandler& instance = getInstance();
    if (status == ESP_NOW_SEND_SUCCESS) {
        if (instance.getSettingTimer() == true) {
            instance.setTimerReset(false);
            instance.setLastTimerCounter();
            if (memcmp(mac_addr, instance.getItemFromAddressList(instance.getCurrentTimerIndex()).address, 6) == 0) {
                instance.setLastDelay(micros() - instance.getLastSendTime());   
            }
            else {
                ESP_LOGI("MSG", "Message sent to %02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
            }
        }
    }
    else {
        ESP_LOGI("MSG", "Send failed");
    }
}

void MessageHandler::onDataRecv(const esp_now_recv_info * mac, const uint8_t *incomingData, int len) {
    //ALPHA
    MessageHandler& instance = getInstance();
    instance.pushToRecvQueue(mac, incomingData, len);
}


void MessageHandler::sendSystemStatus() {
    message_system_status systemStatus;
    systemStatus.numDevices = getNumDevices();
    message_data message;
    message.messageType = MSG_SYSTEM_STATUS;
    memcpy(&message.payload, &systemStatus, sizeof(systemStatus));
    memcpy(&message.targetAddress, broadcastAddress, sizeof(broadcastAddress));
    pushToSendQueue(message);
}

void MessageHandler::sendAnimation(message_animation animationMessage, int addressId) {
    message_data message;
    message.messageType = MSG_ANIMATION;
    memcpy(&message.payload.animation, &animationMessage, sizeof(animationMessage));
    if (addressId == -1) {
        memcpy(&message.targetAddress, broadcastAddress, sizeof(broadcastAddress));
    }
    else {
        memcpy(&message.targetAddress, addressList[addressId].address, sizeof(addressList[addressId].address));
    }
    pushToSendQueue(message);
}
#endif