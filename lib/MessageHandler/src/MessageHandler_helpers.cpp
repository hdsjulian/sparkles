#include "MessageHandler.h"
#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"

String MessageHandler::stringAddress(const uint8_t * mac_addr, bool debug){
    String macStr;
    if (memcmp(mac_addr, hostAddress, 6) == 0) {
        macStr += "HOST ADDRESS";
    }
    else if (memcmp(mac_addr, clapDeviceAddress, 6) == 0) {
        macStr += "CLAP DEVICE ADDRESS";
    }
    else if (memcmp(mac_addr, midiDeviceAddress, 6) == 0) {
        macStr += "BROADCAST ADDRESS";
    }
    else if (memcmp(mac_addr, raspiDeviceAddress, 6) == 0) {
        macStr += "RASPI DEVICE ADDRESS";
    }

    else {
        macStr += "ADDRESS: ";
    }
    String separator = debug ? ", 0x" : ":";
    macStr += String(mac_addr[0], HEX);
    macStr += separator;
    macStr += String(mac_addr[1], HEX);
    macStr += separator; 
    macStr += String(mac_addr[2], HEX);
    macStr += separator; 
    macStr += String(mac_addr[3], HEX);
    macStr += separator; 
    macStr += String(mac_addr[4], HEX);
    macStr += separator; 
    macStr += String(mac_addr[5], HEX);

    return macStr;
}


int MessageHandler::addPeer(uint8_t * address) {
    memcpy(&peerInfo.peer_addr, address, 6);

    if (esp_now_get_peer(peerInfo.peer_addr, &peerInfo) == ESP_OK) {
        return 0;
    }
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        return -1;
    }
    else {
        return 1;
    }
    
}

void MessageHandler::removePeer(uint8_t address[6]) {
    if (!esp_now_is_peer_exist(address)) {
        return;
    }
    if (esp_now_del_peer(address) != ESP_OK) {
    }
    return;
}

unsigned long long MessageHandler::timeDiffAbs(unsigned long long a, unsigned long long b) {
    return (a > b) ? (a - b) : (b - a);
}


bool MessageHandler::readStructsFromFile(client_address* data, int count, const char* filename) {
    File file;
    if (!LittleFS.exists(filename)) {
        ESP_LOGE("FS", "File does not exist");
        file = LittleFS.open(filename, "w");
        if (!file) {
            ESP_LOGE("FS", "Failed to create file");
            return false;
        }
    }
    else {
        file = LittleFS.open(filename, "r");
        if (!file) {
            ESP_LOGE("FS", "Failed to open file for reading");
            return false;
        }
    }
    size_t totalSize = count * sizeof(client_address);
    
    // Check if the file is large enough
    if (file.size() < totalSize) {

        ESP_LOGE("FS", "File size is too small");
        file.close();
        return false;
    }
    else {
    }
    for (int i = 0; i < count; i++) {
        size_t bytesRead = file.read((uint8_t*)&data[i], sizeof(client_address));
        if (bytesRead != sizeof(client_address)) {
            ESP_LOGE("FS", "Failed to read complete structure at %d", i);
            file.close();
            return false;
        }
    }
    
    // Check if the file is large enough

    file.close();
    return true;

}

void MessageHandler::writeStructsToFile(client_address* data, int count, const char* filename) {
    if (!LittleFS.exists(filename)) {
    }
    File file = LittleFS.open(filename, "w");
    if (!file) {
        ESP_LOGE("FS", "Failed to open file for writing");
        return;
        
    }
    for (int i = 0; i < count; i++) {
        file.write((uint8_t*)&data[i], sizeof(client_address));
    }
    file.close();
}



float MessageHandler::getBatteryPercentage() {
    analogReadResolution(12);
    analogSetPinAttenuation(BATTERY_PIN, ADC_11db);
    int adcValue = 0;
    for (int i = 0; i < 8; i++) adcValue += analogRead(BATTERY_PIN);
    adcValue /= 8;
    float voltage = adcValue * (4.2 / 2550.0);
    float percentage;

    if (voltage >= 4.2) {
        percentage = 100.0;
    } else if (voltage >= 3.8 && voltage < 4.2) {
        percentage =  70.0 + (voltage - 3.8) / (4.2 - 3.8) * (100.0 - 70.0);
    } else if (voltage >= 3.6 && voltage < 3.8) {
        percentage = 15.0 + (voltage - 3.6) / (3.8 - 3.6) * (70.0 - 15.0);
    } else if (voltage >= 3.0 && voltage < 3.6) {
        percentage =  (voltage - 3.0) / (3.6 - 3.0) * 15.0;
    } else {
        percentage = 0.0;
    }

    batteryHistory[batteryHistoryIndex] = percentage;
    batteryHistoryIndex = (batteryHistoryIndex + 1) % BATTERY_HISTORY_SIZE;
    if (batteryHistoryIndex == 0) batteryHistoryFull = true;

    int count = batteryHistoryFull ? BATTERY_HISTORY_SIZE : batteryHistoryIndex;
    float sum = 0;
    for (int i = 0; i < count; i++) sum += batteryHistory[i];
    percentage = sum / count;

    percentage = round(percentage * 100) / 100;
    return percentage;
}

void MessageHandler::handleSystemStatus(message_data incomingData) {
    message_system_status systemStatus = incomingData.payload.systemStatus;
    setNumDevices(systemStatus.numDevices);
}

float MessageHandler::convertMicrosToMeters(unsigned long long timeInMicros) {
    // Speed of sound in air at 20 degrees Celsius is approximately 343 meters per second
    // Convert time from microseconds to seconds and then to meters
    return (timeInMicros / 1e6) * 343.0;
}

void MessageHandler::setBoardPosition(int boardId, float xPos, float yPos) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        if (boardId >= 0 && boardId < NUM_DEVICES) {
            addressList[boardId].xPos = xPos;
            addressList[boardId].yPos = yPos;
        }
        xSemaphoreGive(configMutex);
    }
    message_data configMessage = createConfigMessage(boardId);
    pushToSendQueue(configMessage);
}



void MessageHandler::startCalibrationClient() {
    ledInstance->stopAnimationTask();
    startClapTask();
}

void MessageHandler::resetCalibration() {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        clapIndex = 0;
        memset(clapTable, 0, sizeof(clap_table) * NUM_CLAPS);
        for (int i = 0; i < NUM_DEVICES; i++) {
            addressList[i].xPos = 0.0f;
            addressList[i].yPos = 0.0f;
            memset(addressList[i].distances, 0, sizeof(float) * NUM_CLAPS);
        }
        xSemaphoreGive(configMutex);
    }
}

void MessageHandler::testCalibration() {
        message_data commandMessage = createCommandMessage(CMD_TEST_CALIBRATION, true);
        pushToSendQueue(commandMessage);
}

void MessageHandler::sendSleepWakeupMessage(unsigned long long sleepDuration) {
    message_data sleepWakeupMessage;
    sleepWakeupMessage.messageType = MSG_SLEEP_WAKEUP;
    memcpy(sleepWakeupMessage.targetAddress, broadcastAddress, 6);
    message_sleep_wakeup payload;
    payload.duration = sleepDuration;
    memcpy(&sleepWakeupMessage.payload.sleepWakeup, &payload, sizeof(payload));
    WiFi.macAddress(sleepWakeupMessage.senderAddress);
    pushToSendQueue(sleepWakeupMessage);
}

void MessageHandler::turnWifiOff() {
    esp_now_deinit();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

void MessageHandler::broadcastReannounce() {
    message_data msg = createCommandMessage(CMD_REANNOUNCE, true);
    pushToSendQueue(msg);
    ESP_LOGI("MSG", "CMD_REANNOUNCE queued for broadcast");
}

bool MessageHandler::getTestMode() {
    return testMode;
}

void MessageHandler::setTestMode(bool on, float spacingMeters) {
    testMode = on;
    ESP_LOGI("MSG", "Test mode: %s (%.2f m/client)", on ? "ON" : "OFF", spacingMeters);
    message_data msg = createCommandMessage(on ? CMD_TEST_MODE_ON : CMD_TEST_MODE_OFF, true);
    msg.payload.command.param = spacingMeters;
    pushToSendQueue(msg);

    if (on) {
        int numDevices = getNumDevices();
        float maxDist = spacingMeters * (numDevices > 0 ? numDevices - 1 : 0);
        ESP_LOGI("MSG", "Test mode max distance: %.2f m (%d devices)", maxDist, numDevices);
        message_data maxDistMsg = createCommandMessage(CMD_SET_MAX_DISTANCE, true);
        maxDistMsg.payload.command.param = maxDist;
        pushToSendQueue(maxDistMsg);
    }
}

void MessageHandler::resetSystem() {
    if (LittleFS.exists("/clientAddress")) {
        LittleFS.remove("/clientAddress");
        ESP_LOGI("FS", "File removed successfully");
    }
    pendingBroadcastCommand = CMD_RESET_SYSTEM;
    pendingBroadcastExpiry = millis() + 5000;
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    ESP.restart();
}
