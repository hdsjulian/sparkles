#include "MessageHandler.h"
#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"

void MessageHandler::printAllPeers() {
       // Get the peer list
    esp_now_peer_info_t peerList;
    for (int i = 0;i<2;i++) {
        if (i == 0) {
            esp_now_fetch_peer(true, &peerList);
        }
        else {
            esp_now_fetch_peer(true, &peerList);
        }
        Serial.print("Peer ");
        Serial.print(": MAC Address=");
        for (int j = 0; j < 6; ++j) {
            Serial.print(peerList.peer_addr[j], HEX);
            if (j < 5) {
                Serial.print(":");
            }
        }
        Serial.print(", Channel=");
        Serial.print(peerList.channel);
        Serial.println();

    }
      
}

void MessageHandler::printAddress(const uint8_t * mac_addr){
    if (memcmp(mac_addr, hostAddress, 6) == 0) {
        Serial.println("HOST ADDRESS");
        return;
    }
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    Serial.println(macStr);
}

String MessageHandler::stringAddress(const uint8_t * mac_addr, bool debug){
    String macStr;
    if (memcmp(mac_addr, hostAddress, 6) == 0) {
        macStr += "HOST ADDRESS";
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
    if (esp_now_del_peer(address) != ESP_OK) {
        ESP_LOGI("peer", "coudln't delete peer");
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
        ESP_LOGI("FS", "File size is large enough");
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
        ESP_LOGE("FS", "File does not exist");
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
    int adcValue = analogRead(BATTERY_PIN); // Read the ADC value
    float voltage = adcValue * (4.2 / 3220.0);
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
    percentage = round(percentage * 100) / 100;
    return percentage;
}