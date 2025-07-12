#include "MessageHandler.h"

#if DEVICE_MODE == MASTER
#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"


void MessageHandler::turnWifiOn() {
    WebServer& webServerInstance = WebServer::getInstance(&LittleFS);
    webServerInstance.setWifi();
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
            ESP_LOGI("MSG", "Received from queue %d", incomingData.messageType);
            if (incomingData.messageType == MSG_ADDRESS) {
                if (memcmp(incomingData.senderAddress, clapDeviceAddress, 6) == 0) {
                    ESP_LOGI("MSG", "Received address from clap device, ignoring");
                    if (clapSyncHandle != NULL) {
                        vTaskDelete(clapSyncHandle);
                        clapSyncHandle = NULL;
                    }
                    startClapSyncTask();
                    continue;
                }
                vTaskDelay(1000/portTICK_PERIOD_MS);
                //ESP_LOGI("MSG", "Received animation message");
                //ESP_LOGI("MSG", "Animation type: %d", incomingData[1]);
                //message_animate *animation = (message_animate *)incomingData;
                //ESP_LOGI("MSG", "Midi note: %d", animation->animationParams.midi.note);
                //ESP_LOGI("MSG", "Midi velocity: %d", animation->animationParams.midi.velocity);
                if (incomingData.payload.address.version != version) {
                    ESP_LOGI("MSG", "Received address with lower version, ignoring. Version: %d, Current Version: %d", incomingData.payload.address.version, version);
                    ESP_LOGI("MSG", "This version is %s vs other version is %s", version.toString().c_str(), incomingData.payload.address.version.toString().c_str());
                    message_data updateVersionMessage = createUpdateVersionMessage(version);
                    memcpy(updateVersionMessage.targetAddress, incomingData.senderAddress, 6);
                    xQueueSend(sendQueue, &updateVersionMessage, portMAX_DELAY);
                    continue;
                }
                else if (!isOTAUpdating) {
                    message_address address = (message_address)incomingData.payload.address;
                    int index = addOrGetAddressId(address.address);
                    setCurrentTimerIndex(index);
                    startTimerSyncTask();
                }
            }
            else if (incomingData.messageType == MSG_GOT_TIMER) {
                ESP_LOGI("MSG", "Received got timer message");
                int timerIndex = getCurrentTimerIndex();
                //vTaskDelete(timerSyncHandle);
                removePeer(addressList[getCurrentTimerIndex()].address);
                //timerSyncHandle = NULL;
                addressList[timerIndex].active = ACTIVE;
                addressList[timerIndex].batteryPercentage = incomingData.payload.gotTimer.batteryPercentage;
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
                    ESP_LOGI("MSG", "Checking address %d", i);
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
                    ESP_LOGI("MSG", "Received clap from clap device, setting clap time");
                    setLastClapTime(incomingData.payload.clap.clapTime - getClapDeviceDelay());
                    WebServer& webServerInstance = WebServer::getInstance(&LittleFS);
                    webServerInstance.clapReceived(clapIndex, incomingData.payload.clap.clapTime);
                }
                else {
                    for (int i = 0; i < NUM_DEVICES; i++) {
                        if (memcmp(addressList[i].address, incomingData.senderAddress, 6) == 0) {
                            ESP_LOGI("MSG", "Received clap from address %d", i);
                            addressList[i].distances[clapIndex] = convertMicrosToMeters(getLastClapTime() - incomingData.payload.clap.clapTime);
                            ESP_LOGI("MSG", "Distance: %.2f", addressList[i].distances[clapIndex]);
                            break;
                        }
                    }
                }
            }

            else if (incomingData.messageType == MSG_SYSTEM_STATUS) {
                ESP_LOGI("MSG", "Received system status message");
                ESP_LOGI("MSG", "Number of devices: %d", incomingData.payload.systemStatus.numDevices);
            }

            else if (incomingData.messageType == MSG_COMMAND && incomingData.payload.command.commandType == CMD_ASK_ADMIN_PRESENT) {

                if (getAdminPresent() + 60000 > millis()) {
                    ESP_LOGI("MSG", "Received ask admin present message");
                    message_data commandMessage = createCommandMessage(CMD_SET_ADMIN_PRESENT);
                    memcpy(commandMessage.targetAddress, incomingData.senderAddress, 6);
                    pushToSendQueue(commandMessage);
                }
                else {
                    message_data commandMessage = createCommandMessage(CMD_SET_ADMIN_NOT_PRESENT);
                    memcpy(commandMessage.targetAddress, incomingData.senderAddress, 6);
                    pushToSendQueue(commandMessage);
                    ESP_LOGI("MSG", "Admin hasn't been present for a while");
                }

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
            if (memcmp(messageData.targetAddress, broadcastAddress, 6) != 0) {
                addPeer(messageData.targetAddress);
            }

            switch (messageData.messageType) {
                case MSG_ADDRESS:
                    esp_now_send(messageData.targetAddress, (uint8_t *) &messageData, sizeof(messageData));
                    break;
                default:
                    esp_now_send(messageData.targetAddress, (uint8_t *) &messageData, sizeof(messageData));
                    ESP_LOGI("MSG", "Sending message of type %d to %02x:%02x:%02x:%02x:%02x:%02x", messageData.messageType, messageData.targetAddress[0], messageData.targetAddress[1], messageData.targetAddress[2], messageData.targetAddress[3], messageData.targetAddress[4], messageData.targetAddress[5]);
                    ESP_LOGI("MSG", "Message tyepe: %d", messageData.messageType);
                    break;
            }        
            if (memcmp(messageData.targetAddress, broadcastAddress, 6) != 0) {
                removePeer(messageData.targetAddress);
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
        else if (instance.getSettingClapSync() == true) {
            instance.setLastDelay(micros() - instance.getLastSendTime());
        }
        else if (instance.getRequestingOTAUpdate() == true) {
            instance.setRequestingOTAUpdate(false);
            ESP_LOGI("MSG", "Requesting OTA update sent to %02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        }
        else {
            ESP_LOGI("MSG", "Message sent to %02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        }
        
    }
    else {
        if (instance.getRequestingOTAUpdate() == true) {
            instance.setRequestingOTAUpdate(false);
            ESP_LOGI("MSG", "Requesting OTA update failed");
            ESP_LOGI("MSG", "Address id cannot be reached: %d", instance.getOTAUpdateAddressId());
            instance.setNextOTAAddress(true);
        }

        else {
            ESP_LOGI("MSG", "Message sent to %02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        }
        ESP_LOGI("MSG", "Send failed");
    }
}

void MessageHandler::onDataRecv(const esp_now_recv_info * mac, const uint8_t *incomingData, int len) {
    //ALPHA
    MessageHandler& instance = getInstance();
    ESP_LOGI("MSG", "Received message of type %d from %02x:%02x:%02x:%02x:%02x:%02x", incomingData[0], mac->src_addr[0], mac->src_addr[1], mac->src_addr[2], mac->src_addr[3], mac->src_addr[4], mac->src_addr[5]);

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
    ESP_LOGI("MSG", "Sending animation message to %02x:%02x:%02x:%02x:%02x:%02x", message.targetAddress[0], message.targetAddress[1], message.targetAddress[2], message.targetAddress[3], message.targetAddress[4], message.targetAddress[5]);
    ESP_LOGI("MSG", "Animation type: %d", animationMessage.animationType);
    ESP_LOGI("MSG", "Animation params: %d", animationMessage.animationParams.blink.repetitions);
    ESP_LOGI("MSG", "Animation timeStamp: %lu", animationMessage.timeStamp);
    //ESP_LOGI("MSG", "Animation params: %d",
    pushToSendQueue(message);
}
void MessageHandler::startOTAUpdateTask() {
    xTaskCreatePinnedToCore(runOTAUpdateTaskWrapper, "runOTAUpdate", 10000, this, 2, &otaUpdateHandle, 0);
}
void MessageHandler::runOTAUpdateTaskWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runOTAUpdateTask();
}
void MessageHandler::runOTAUpdateTask() {
    setIsOTAUpdating(true);
    setOTAUpdateAddressId(0);
    message_data updateVersionMessage = createUpdateVersionMessage(version);
    ESP_LOGI("MSG", "Running OTA update task - THIS SHOULD NOT BE WORKING YET");
    while (isOTAUpdating) {
        if (getOTAUpdateAddressId() < 0 || getOTAUpdateAddressId() >= NUM_DEVICES) {
            ESP_LOGE("MSG", "Invalid OTA update address ID");
            isOTAUpdating = false;
            vTaskDelete(otaUpdateHandle);
            
        }
        if (getNextOTAAddress() == true) {
            setOTAUpdateAddressId(getOTAUpdateAddressId() + 1);
            if (getOTAUpdateAddressId() >= NUM_DEVICES || memcmp(addressList[getOTAUpdateAddressId()].address, emptyAddress, 6) == 0) {
                vTaskDelete(otaUpdateHandle);
                
            }
            message_data updateVersionMessage;
            updateVersionMessage = createUpdateVersionMessage(version);
            memcpy(updateVersionMessage.targetAddress, OTAUpdateAddress, 6);
            pushToSendQueue(updateVersionMessage);
        }


        // Here you would implement the actual OTA update logic
        // For now, we just simulate a successful update
        isOTAUpdating = false;
        ESP_LOGI("MSG", "OTA update completed");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void MessageHandler::startCalculatePositionsTask() {
    xTaskCreatePinnedToCore(calculatePositionsTaskWrapper, "calculatePositions", 10000, this, 2, &calculatePositionsHandle, 0);
}
void MessageHandler::calculatePositionsTaskWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runCalculatePositionsTask();
}
void MessageHandler::runCalculatePositionsTask() {
    ESP_LOGI("MSG", "Running calculate positions task");
    while (true) {
        for (int i = 0; i < NUM_DEVICES; i++) {
            if (memcmp(addressList[i].address, emptyAddress, 6) == 0) {
                continue;
            }
            ESP_LOGI("MSG", "Calculating position for device %d", i);

            // Prepare matrices for least squares
            int validClaps = 0;
            float A[NUM_CLAPS][2] = {0}; // Coefficients matrix
            float b[NUM_CLAPS] = {0};    // Constants vector

            // Reference point (first clap)
            float x1 = 0, y1 = 0, d1 = 0;
            bool referenceSet = false;

            for (int j = 0; j < NUM_CLAPS; j++) {
                if (clapTable[j].clapTime == 0) {
                    continue; // Skip if clap time is not set
                }

                float xj = clapTable[j].xPos;
                float yj = clapTable[j].yPos;
                float dj = addressList[i].distances[j];

                if (!referenceSet) {
                    // Set the first clap as the reference point
                    x1 = xj;
                    y1 = yj;
                    d1 = dj;
                    referenceSet = true;
                    continue;
                }

                // Fill the A matrix and b vector
                A[validClaps][0] = 2 * (xj - x1);
                A[validClaps][1] = 2 * (yj - y1);
                b[validClaps] = d1 * d1 - dj * dj - x1 * x1 - y1 * y1 + xj * xj + yj * yj;

                validClaps++;
            }

            // Ensure we have at least 3 valid claps for trilateration
            if (validClaps < 2) {
                ESP_LOGW("MSG", "Not enough claps for trilateration for device %d", i);
                continue;
            }

            // Solve the system using least squares
            float AtA[2][2] = {0}; // A^T * A
            float Atb[2] = {0};    // A^T * b

            for (int row = 0; row < validClaps; row++) {
                AtA[0][0] += A[row][0] * A[row][0];
                AtA[0][1] += A[row][0] * A[row][1];
                AtA[1][0] += A[row][1] * A[row][0];
                AtA[1][1] += A[row][1] * A[row][1];

                Atb[0] += A[row][0] * b[row];
                Atb[1] += A[row][1] * b[row];
            }

            // Calculate the determinant of AtA
            float det = AtA[0][0] * AtA[1][1] - AtA[0][1] * AtA[1][0];
            if (det == 0) {
                ESP_LOGE("MSG", "Trilateration failed: Determinant is zero");
                continue;
            }

            // Invert AtA
            float invAtA[2][2] = {
                {AtA[1][1] / det, -AtA[0][1] / det},
                {-AtA[1][0] / det, AtA[0][0] / det}
            };

            // Calculate the position (x, y)
            float x = invAtA[0][0] * Atb[0] + invAtA[0][1] * Atb[1];
            float y = invAtA[1][0] * Atb[0] + invAtA[1][1] * Atb[1];

            // Store the calculated position
            addressList[i].xPos = x;
            addressList[i].yPos = y;

            ESP_LOGI("MSG", "Device %d position calculated: X=%.2f, Y=%.2f", i, x, y);
        }

        // Delay before recalculating
        WebServer& webServerInstance = WebServer::getInstance(&LittleFS);
        webServerInstance.setCalculationDone(true);
        webServerInstance.updateAddressList();
        vTaskDelete(calculatePositionsHandle);
    }
}

void MessageHandler::endCalibration() {
    message_data endMessage = createCommandMessage(CMD_END_CALIBRATION, true);
    pushToSendQueue(endMessage);
    WebServer& webServerInstance = WebServer::getInstance(&LittleFS);
    startCalculatePositionsTask();
}

void MessageHandler::startCalibrationMaster() {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        ESP_LOGI("MSG", "Starting calibration as master");
        clapIndex = 0;
        memset(clapTable, 0, sizeof(clap_table) * NUM_CLAPS);
        message_data startMessage = createCommandMessage(CMD_START_CALIBRATION, true);
        pushToSendQueue(startMessage);
        xSemaphoreGive(configMutex);
    }
}
void MessageHandler::cancelCalibration() {
    ESP_LOGI("MSG", "Cancelling calibration");
    setLastClapTime(0);
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (memcmp(addressList[i].address, emptyAddress, 6) == 0) {
            continue;
        }
        addressList[i].distances[clapIndex] = 0;
    }
}
void MessageHandler::continueCalibration(float xPos, float yPos) {
    ESP_LOGI("MSG", "Continuing calibration");
    setClap(xPos, yPos);
    clapIndex++;
    message_data continueMessage = createCommandMessage(CMD_CONTINUE_CALIBRATION, true);
    pushToSendQueue(continueMessage);
}

void MessageHandler::commandCalibrate(int boardId) {
    ESP_LOGI("MSG", "Sending calibration command to board ID: %d", boardId);
    message_data commandMessage = createCommandMessage(CMD_START_CALIBRATION, false);
    memcpy(commandMessage.targetAddress, addressList[boardId].address, sizeof(addressList[boardId].address));
    pushToSendQueue(commandMessage);
}

#endif