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

void MessageHandler::setSettingClapSync(bool set) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        settingClapSync = set;
        xSemaphoreGive(configMutex);
    }
}

bool MessageHandler::getSettingClapSync() {
    bool returnSettingClapSync;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnSettingClapSync = settingClapSync;
        xSemaphoreGive(configMutex);
    }
    return returnSettingClapSync;
}


void MessageHandler::setLastSendTime(unsigned long long time) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        lastSendTime = time;
        xSemaphoreGive(configMutex);
    }
}
unsigned long long MessageHandler::getLastSendTime() {
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
void MessageHandler::setMsgReceiveTime(unsigned long long time) {
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

void MessageHandler::setTimeOffset(long long offsetAvg) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        timeOffset = offsetAvg;
        ESP_LOGI("MSG", "Setting time offset: %lld", timeOffset);
        xSemaphoreGive(configMutex);
    }
    ledInstance->setTimerOffset(timeOffset);
}
 long long MessageHandler::getTimeOffset() {
     long long returnOffset;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnOffset = timeOffset;
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

void MessageHandler::setAvailable(int index) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        addressList[index].active = ACTIVE;
        addressList[index].tries = 0;
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
    int activeCount = 0;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < NUM_CLIENTS; i++) {
            if (addressList[i].active == ACTIVE) {
                activeCount++;
            }
        }
        xSemaphoreGive(configMutex);
    }
    return activeCount;
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
message_data MessageHandler::createClapMessage(bool isHost) {
    message_data clapMessage;
    clapMessage.messageType = MSG_CLAP;
    if (!isHost) {
        memcpy(clapMessage.targetAddress, broadcastAddress, 6);
    } else {
        memcpy(clapMessage.targetAddress, clapDeviceAddress, 6);
    }
    message_clap clapPayload;
    
    ESP_LOGI("CLAP", "Peak Detected at %lu", micros());
    ESP_LOGI("CLAP", "Timer Offset: %lld", ledInstance->getTimerOffset());
    ESP_LOGI("CLAP", "Clap Time: %llu", clapPayload.clapTime);
    clapPayload.clapTime = micros() + ledInstance->getTimerOffset();
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


message_data MessageHandler::createCommandMessage(int commandType, bool isBroadcast ) {
    message_data commandMessage;
    commandMessage.messageType = MSG_COMMAND;
    if (isBroadcast) {
        memcpy(commandMessage.targetAddress, broadcastAddress, 6);
    } else {
        memcpy(commandMessage.targetAddress, clapDeviceAddress, 6);
    }
    commandMessage.payload.command.commandType = commandType;
    WiFi.macAddress(commandMessage.senderAddress);
    return commandMessage;
}

message_data MessageHandler::createStatusMessage() {
    message_data statusMessage;
    statusMessage.messageType = MSG_STATUS;
    memcpy(statusMessage.targetAddress, broadcastAddress, 6);
    message_status statusPayload;
    statusPayload.batteryPercentage = getBatteryPercentage();
    memcpy(&statusMessage.payload.status, &statusPayload, sizeof(statusPayload));
    WiFi.macAddress(statusMessage.senderAddress);
    return statusMessage;
}

void MessageHandler::setClap(float xPos, float yPos) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        if (clapIndex >= NUM_CLAPS) {
            clapIndex = 0;
        }
        clapTable[clapIndex].xPos = xPos;
        clapTable[clapIndex].yPos = yPos;
        clapTable[clapIndex].clapTime = lastClapTime; // Use the last clap time
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

void MessageHandler::setIsOTAUpdating(bool isUpdating) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        isOTAUpdating = isUpdating;
        xSemaphoreGive(configMutex);
    }
}
bool MessageHandler::getIsOTAUpdating() {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        bool returnIsUpdating = isOTAUpdating;
        xSemaphoreGive(configMutex);
        return returnIsUpdating;
    }
    return false;
  }

void MessageHandler::setRequestingOTAUpdate(bool request) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        requestingOTAUpdate = request;
        xSemaphoreGive(configMutex);
    }
}
bool MessageHandler::getRequestingOTAUpdate() {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        bool returnRequesting = requestingOTAUpdate;
        xSemaphoreGive(configMutex);
        return returnRequesting;
    }
    return false;
}

void MessageHandler::setNextOTAAddress(bool next) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        nextOTAAddress = next;
        xSemaphoreGive(configMutex);
    }
}
bool MessageHandler::getNextOTAAddress() {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        bool returnNext = nextOTAAddress;
        xSemaphoreGive(configMutex);
        return returnNext;
    }
    return false;
}

void MessageHandler::setLastClapTime(unsigned long long time) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        lastClapTime = time;
        xSemaphoreGive(configMutex);
    }
}
unsigned long long MessageHandler::getLastClapTime() {
    unsigned long long returnTime;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnTime = lastClapTime;
    }
    xSemaphoreGive(configMutex);
    return returnTime;
}

int MessageHandler::getOTAUpdateAddressId() {
    int returnId;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnId = otaUpdateAddressId;
        xSemaphoreGive(configMutex);
    }
    return returnId;
}

void MessageHandler::setOTAUpdateAddressId(int id) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        otaUpdateAddressId = id;
        xSemaphoreGive(configMutex);
    }
}

void MessageHandler::setSleepTime(int hours, int minutes, int seconds) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        sleepTimeHours = hours;
        sleepTimeMinutes = minutes;
        sleepTimeSeconds = seconds;
        ESP_LOGI("Sleep", "Setting sleep time to %02d:%02d:%02d", sleepTimeHours, sleepTimeMinutes, sleepTimeSeconds);
        xSemaphoreGive(configMutex);
    }
}
void MessageHandler::setWakeupTime(int hours, int minutes, int seconds) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        wakeupTimeHours = hours;
        wakeupTimeMinutes = minutes;
        wakeupTimeSeconds = seconds;
        ESP_LOGI("Sleep", "Setting wakeup time to %02d:%02d:%02d", wakeupTimeHours, wakeupTimeMinutes, wakeupTimeSeconds);
        xSemaphoreGive(configMutex);
    }
}


unsigned long MessageHandler::getSleepTime() {
    unsigned long sleepTime;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        if (sleepTimeHours == 0 && sleepTimeMinutes == 0 && sleepTimeSeconds == 0) {
            sleepTime = 0; // No sleep time set
            xSemaphoreGive(configMutex);
            return sleepTime;
        }
        else {
            ESP_LOGI("Sleep Time", "Sleep time hours %d, minutes %d, seconds %d", sleepTimeHours,sleepTimeMinutes, sleepTimeSeconds);
        }
        unsigned long currentMillis = millis();
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        time_t now = tv.tv_sec;
        struct tm *currentTime = localtime(&now);
        int currentSeconds = currentTime->tm_hour * 3600 + currentTime->tm_min * 60 + currentTime->tm_sec;
        int targetSeconds = sleepTimeHours * 3600 + sleepTimeMinutes * 60 + sleepTimeSeconds;
        int secondsUntilSleep = targetSeconds - currentSeconds;
        if (secondsUntilSleep <= 0) {
            secondsUntilSleep += 24 * 3600;
        }
        sleepTime = secondsUntilSleep * 1000UL - (tv.tv_usec / 1000);
        xSemaphoreGive(configMutex);
    }
    return sleepTime;
}

bool MessageHandler::isInSleepPhase() {
    bool inSleep = false;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        if ((sleepTimeHours == 0 && sleepTimeMinutes == 0 && sleepTimeSeconds == 0) ||
            (wakeupTimeHours == 0 && wakeupTimeMinutes == 0 && wakeupTimeSeconds == 0)) {
            // No sleep/wakeup time set
            xSemaphoreGive(configMutex);
            return false;
        }
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        time_t now = tv.tv_sec;
        struct tm *currentTime = localtime(&now);
        int currentSeconds = currentTime->tm_hour * 3600 + currentTime->tm_min * 60 + currentTime->tm_sec;
        int sleepSeconds = sleepTimeHours * 3600 + sleepTimeMinutes * 60 + sleepTimeSeconds-1;
        int wakeupSeconds = wakeupTimeHours * 3600 + wakeupTimeMinutes * 60 + wakeupTimeSeconds-1;
        if (sleepSeconds < wakeupSeconds) {
            // Sleep and wakeup are on the same day
            inSleep = (currentSeconds >= sleepSeconds) && (currentSeconds < wakeupSeconds);
        } else {
            // Sleep starts before midnight, wakeup is after midnight
            inSleep = (currentSeconds >= sleepSeconds) || (currentSeconds < wakeupSeconds);
        }
        xSemaphoreGive(configMutex);
    }
    return inSleep;
}

unsigned long MessageHandler::getSleepDuration() {
    unsigned long duration = 0;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        if (wakeupTimeHours == 0 && wakeupTimeMinutes == 0 && wakeupTimeSeconds == 0) {
            xSemaphoreGive(configMutex);
            return 0; // No wakeup time set
        }
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        time_t now = tv.tv_sec;
        struct tm *currentTime = localtime(&now);
        int currentSeconds = currentTime->tm_hour * 3600 + currentTime->tm_min * 60 + currentTime->tm_sec;
        int wakeupSeconds = wakeupTimeHours * 3600 + wakeupTimeMinutes * 60 + wakeupTimeSeconds;
        int durationSeconds = (wakeupSeconds - currentSeconds + 24 * 3600) % (24 * 3600);
        duration = durationSeconds * 1000UL - (tv.tv_usec / 1000);
        xSemaphoreGive(configMutex);
    }
    return duration;
}
unsigned long MessageHandler::getAdminPresent() {
    unsigned long returnTime;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnTime = lastAdminPresent;
        xSemaphoreGive(configMutex);
    }
    return returnTime;
}

void MessageHandler::setAdminPresent(unsigned long time) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        lastAdminPresent = time;
        xSemaphoreGive(configMutex);
    }
}


void MessageHandler::setBatteryLow(bool low) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        isBatteryLow = low;
        xSemaphoreGive(configMutex);
    }
}
bool MessageHandler::getBatteryLow() {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        bool returnLow = isBatteryLow;
        xSemaphoreGive(configMutex);
        return returnLow;
    }
    return false;
}

void MessageHandler::recordTimeOfDayBeforeSleep() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        beforeSleepTimeinfo = timeinfo;
        beforeSleepSecondsOfDay = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
        beforeSleepMillis = millis();
    }
}


void MessageHandler::setTimeOfDayAfterSleep() {
    unsigned long long afterSleepMillis = millis();
    unsigned long elapsedMillis = afterSleepMillis - beforeSleepMillis;
    int elapsedSeconds = elapsedMillis / 1000;
    int afterSleepSecondsOfDay = (beforeSleepSecondsOfDay + elapsedSeconds) % (24 * 3600);

    struct tm newTimeinfo = beforeSleepTimeinfo;
    newTimeinfo.tm_hour = afterSleepSecondsOfDay / 3600;
    newTimeinfo.tm_min = (afterSleepSecondsOfDay % 3600) / 60;
    newTimeinfo.tm_sec = afterSleepSecondsOfDay % 60;

    time_t newTime = mktime(&newTimeinfo);
    struct timeval tv = { .tv_sec = newTime, .tv_usec = 0 };
    settimeofday(&tv, nullptr);
}
void MessageHandler::setHasClapHappened(bool hasClapHappened) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        this->hasClapHappened = hasClapHappened;
        xSemaphoreGive(configMutex);
    }
}
bool MessageHandler::getHasClapHappened() {
    bool returnHasClapHappened;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnHasClapHappened = hasClapHappened;
        xSemaphoreGive(configMutex);
    }
    return returnHasClapHappened;
}

void MessageHandler::setClapDeviceDelay(int delay) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        clapDeviceDelay = delay;
        xSemaphoreGive(configMutex);
    }
}
int MessageHandler::getClapDeviceDelay() {
    int returnDelay;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnDelay = clapDeviceDelay;
        xSemaphoreGive(configMutex);
    }
    return returnDelay;
}

void MessageHandler::setMidiParams(int minVal, int maxVal, int minSat, int maxSat, int rangeMin, int rangeMax, float rmsMin, float rmsMax, int mode) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        midiParams.valMin = minVal;
        midiParams.valMax = maxVal;
        midiParams.satMin = minSat;
        midiParams.satMax = maxSat;
        midiParams.rangeMin = rangeMin;
        midiParams.rangeMax = rangeMax;
        midiParams.rmsMin = rmsMin;
        midiParams.rmsMax = rmsMax;
        midiParams.mode = mode;
        xSemaphoreGive(configMutex);
    }
    ESP_LOGI("MIDI", "Set Midi Params");
    message_data midiParamsMessage = createMidiParamsMessage(midiParams);
    ESP_LOGI("MIDI", "midi params message %.2f -- %.2f ", midiParams.rmsMin, midiParams.rmsMax);
    pushToSendQueue(midiParamsMessage);
}
message_midi_params MessageHandler::getMidiParams() {
    message_midi_params returnParams;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnParams = midiParams;
        xSemaphoreGive(configMutex);
    }       
    return returnParams;
}

message_data MessageHandler::createMidiParamsMessage(message_midi_params params) {
    message_data midiParamsMessage;
    midiParamsMessage.messageType = MSG_MIDI_PARAMS;
    memcpy(midiParamsMessage.targetAddress, raspiDeviceAddress, 6);
    memcpy(&midiParamsMessage.payload.midiParams, &params, sizeof(params));
    WiFi.macAddress(midiParamsMessage.senderAddress);
    return midiParamsMessage;
}
bool MessageHandler::getCalibrationTest() {
    bool returnCalibrationTest;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnCalibrationTest = calibrationTest;
        xSemaphoreGive(configMutex);
    }
    return returnCalibrationTest;
}
void MessageHandler::setCalibrationTest(bool test) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        calibrationTest = test;
        xSemaphoreGive(configMutex);
    }
}
void MessageHandler::setLastMidiTime(unsigned long long timeStamp) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        lastMidiTime = timeStamp;
        xSemaphoreGive(configMutex);
    }
}
unsigned long long MessageHandler::getLastMidiTime() {
    unsigned long long returnLastMidiTime;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnLastMidiTime = lastMidiTime;
        xSemaphoreGive(configMutex);
    }
    return returnLastMidiTime;
}