#include "MessageHandler.h"
#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"

int MessageHandler::getCurrentTimerIndex() {
    int returnIndex ; 
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnIndex = currentTimerIndex;
        xSemaphoreGive(configMutex);
    }
    return returnIndex;
}
void MessageHandler::setCurrentTimerIndex(int index) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        currentTimerIndex = index;
        xSemaphoreGive(configMutex);
    }
}
void MessageHandler::setTimerSet(bool set) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        timerSet = set;
        xSemaphoreGive(configMutex);
    }
}
bool MessageHandler::getTimerSet() {
    bool returnSet;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnSet = timerSet;
        xSemaphoreGive(configMutex);
    }
    return returnSet;
}
void MessageHandler::setSettingTimer(bool set) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        settingTimer = set;
        xSemaphoreGive(configMutex);
    }
}
bool MessageHandler::getSettingTimer() {
    bool returnSettingTimer;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnSettingTimer = settingTimer;
        xSemaphoreGive(configMutex);
    }
    return returnSettingTimer;
}

void MessageHandler::setLastSendTime(int time) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        lastSendTime = time;
        xSemaphoreGive(configMutex);
    }
}
int MessageHandler::getLastSendTime() {
    int returnTime;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnTime = lastSendTime;
        xSemaphoreGive(configMutex);
    }
    return returnTime;
}
void MessageHandler::setLastDelay(int delay) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        lastDelay = delay;
        xSemaphoreGive(configMutex);
    }
}
int MessageHandler::getLastDelay() {
    int returnDelay;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnDelay = lastDelay;
        xSemaphoreGive(configMutex);
    }
    return returnDelay;
}

client_address MessageHandler::getItemFromAddressList(int index) {
    client_address returnItem;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnItem = addressList[index];
        xSemaphoreGive(configMutex);
    }
    return returnItem;
}
void MessageHandler::setItemFromAddressList(int index, client_address item) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        addressList[index] = item;
        xSemaphoreGive(configMutex);
    }
}
void MessageHandler::setMsgReceiveTime(int time) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        msgReceiveTime = time;
        xSemaphoreGive(configMutex);
    }
}
unsigned long long MessageHandler::getMsgReceiveTime() {
    unsigned long long returnTime;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnTime = msgReceiveTime;
        xSemaphoreGive(configMutex);
    }
    return returnTime;
}

void MessageHandler::setAddressAnnounced(bool set) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        addressAnnounced = set;
        xSemaphoreGive(configMutex);
    }
}
bool MessageHandler::getAddressAnnounced() {
    bool returnSet;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnSet = addressAnnounced;
        xSemaphoreGive(configMutex);
    }
    return returnSet;
}
int MessageHandler::addOrGetAddressId(uint8_t * address) {
    for (int i = 0; i < NUM_CLIENTS; i++) {
        if (memcmp(addressList[i].address, address, 6) == 0) {
            return i;
        }
        if (memcmp(addressList[i].address, emptyAddress, 6) == 0) {
            memcpy(addressList[i].address, address, 6);
            setNumDevices(i+1);
            return i;
        }
    }
    return -1;
}

void MessageHandler::setTimeOffset(unsigned long long sendTime, unsigned long long receiveTime, int delay) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        if (receiveTime < sendTime) {
            timeOffset = sendTime-receiveTime - delay/2;
            offsetMultiplier = -1;
        }
        else {
            timeOffset = receiveTime-sendTime - delay/2;
            offsetMultiplier = 1;
        }
        xSemaphoreGive(configMutex);
    }
    ledInstance->setTimerOffset(timeOffset);
}
 long long MessageHandler::getTimeOffset() {
     long long returnOffset;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnOffset = timeOffset*offsetMultiplier;
        xSemaphoreGive(configMutex);
    }
    return returnOffset;
}

void MessageHandler::setLastReceiveTime(unsigned long long time) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        lastReceiveTime = time;
        xSemaphoreGive(configMutex);
    }
}
unsigned long long MessageHandler::getLastReceiveTime() {
    int returnTime;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnTime = lastReceiveTime;
        xSemaphoreGive(configMutex);
    }
    return returnTime;
}

bool MessageHandler::getTimerReset() {
    bool returnReset;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnReset = timerReset;
        xSemaphoreGive(configMutex);
    }
    return returnReset;
}
void MessageHandler::setTimerReset(bool set) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        timerReset = set;
        xSemaphoreGive(configMutex);
    }
}

int MessageHandler::incrementTimerCounter() {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        timerCounter++;
        xSemaphoreGive(configMutex);
    }
    return timerCounter;
}
int MessageHandler::getTimerCounter() {
    int returnCounter;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnCounter = timerCounter;
        xSemaphoreGive(configMutex);
    }
    return returnCounter;
}
void MessageHandler::setTimerCounter(int counter) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        timerCounter = counter;
        xSemaphoreGive(configMutex);
    }
}
int MessageHandler::getLastTimerCounter() {
    int returnCounter;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnCounter = lastTimerCounter;
        xSemaphoreGive(configMutex);
    }
    return returnCounter;
}

void MessageHandler::setLastTimerCounter() {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        lastTimerCounter = timerCounter;
        xSemaphoreGive(configMutex);
    }
}

void MessageHandler::setUnavailable(int index) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        addressList[index].active = INACTIVE;
        addressList[index].tries++;
        if (addressList[index].tries > 3) {
            addressList[index].active = UNREACHABLE;
        }
        xSemaphoreGive(configMutex);
    }
}
activeStatus MessageHandler::getActiveStatus(int index) {
    activeStatus returnStatus;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnStatus = addressList[index].active;
        xSemaphoreGive(configMutex);
    }
    return returnStatus;
}

void MessageHandler::setNumDevices(int num) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        ledInstance->setNumDevices(num);
        numDevices = num;
        xSemaphoreGive(configMutex);
    }
}
int MessageHandler::getNumDevices() {
    int returnNum;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnNum = numDevices;
        xSemaphoreGive(configMutex);
    }
    return returnNum;
}
/*
message_data MessageHandler::retrieveCommand(uint8_t * address) {
    message_data returnCommand;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < NUM_CLIENTS; i++) {
            if (memcmp(addressList[i].address, address, 6) == 0) {
                returnCommand = addressList[i].nextCommand;
                break;
            }
        }
        xSemaphoreGive(configMutex);
    }
    return returnCommand;
}

void MessageHandler::setCommand(message_data command, uint8_t * address) {
    message_data emptyCommand;
    emptyCommand.messageType = MSG_WAIT_FOR_INSTRUCTIONS;


    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        if (memcmp(command.targetAddress, broadcastAddress, 6) == 0) {
            for (int i = 0; i < NUM_CLIENTS; i++) {
                if (memcmp(addressList[i].address, address, 6) == 0) {
                    addressList[i].nextCommand = command;
                    xSemaphoreGive(configMutex);
                }
            }
        }
        for (int i = 0; i < NUM_CLIENTS; i++) {
            if (memcmp(addressList[i].address, address, 6) == 0) {
                addressList[i].nextCommand = command;
                xSemaphoreGive(configMutex);
            }
            else if (memcmp(addressList[i].address, emptyAddress, 6) == 0) {
                // If we reach an empty address, we can stop searching
                break;
            }
            else {
                addressList[i].nextCommand = emptyCommand; // Set the command for all other addresses
                memcpy(addressList[i].nextCommand.targetAddress, addressList[i].address, 6); // Ensure the target address is set correctly
             
            }
        }
        xSemaphoreGive(configMutex);
    }
}
*/
message_data MessageHandler::createClapMessage(uint8_t * address) {
    message_data clapMessage;
    clapMessage.messageType = MSG_CLAP;
    memcpy(clapMessage.targetAddress, address, 6);
    message_clap clapPayload;
    clapPayload.clapTime = micros() - ledInstance->getTimerOffset();
    memcpy(&clapMessage.payload.clap, &clapPayload, sizeof(clapPayload));
    WiFi.macAddress(clapMessage.senderAddress);
    return clapMessage;
}

message_data MessageHandler::createConfigMessage(int boardId) {
    message_data configMessage;
    configMessage.messageType = MSG_CONFIG_DATA;
    memcpy(configMessage.targetAddress, addressList[boardId].address, 6);
    message_config_data configPayload;
    configPayload.boardId = boardId;
    configPayload.xPos = addressList[boardId].xPos;
    configPayload.yPos = addressList[boardId].yPos;
    memcpy(&configMessage.payload.configData, &configPayload, sizeof(configPayload));
    WiFi.macAddress(configMessage.senderAddress);
    return configMessage;
}
message_data MessageHandler::createUpdateVersionMessage(Version version) {
    message_data updateMessage;
    updateMessage.messageType = MSG_UPDATE_VERSION;
    memcpy(updateMessage.targetAddress, broadcastAddress, 6);
    message_update_version updatePayload;
    updatePayload.version = version;
    memcpy(&updateMessage.payload.updateVersion, &updatePayload, sizeof(updatePayload));
    WiFi.macAddress(updateMessage.senderAddress);
    return updateMessage;
}

void MessageHandler::setClap(float xPos, float yPos) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        clapIndex++;
        if (clapIndex >= NUM_CLAPS) {
            clapIndex = 0;
        }
        clapTable[clapIndex].xPos = xPos;
        clapTable[clapIndex].yPos = yPos;
        xSemaphoreGive(configMutex);
    }
}
int MessageHandler::getClapIndex() {
    int returnIndex;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnIndex = clapIndex;
        xSemaphoreGive(configMutex);
    }
    return returnIndex;
}
clap_table MessageHandler::getClap(int index) {
    clap_table returnClap;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnClap = clapTable[index];
        xSemaphoreGive(configMutex);
    }
    return returnClap;
}