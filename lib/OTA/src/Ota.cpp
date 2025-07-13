#include "Ota.h"
#include <MyDefines.h>
OTAHandler& OTAHandler::getInstance() {
    static OTAHandler instance;
    return instance;
}

OTAHandler::OTAHandler() : updateUrl(nullptr) {}

void OTAHandler::setup() {
    this->updateUrl = OTA_UPDATE_URL;
}

void OTAHandler::connectToWiFi() {

    ESP_LOGI("OTA", "Connecting to WiFi: %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
        delay(500);
        ESP_LOGI("OTA", "Attempting to connect to WiFi... (%d)", retryCount + 1);
        retryCount++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        ESP_LOGI("OTA", "Connected to WiFi. IP Address: %s", WiFi.localIP().toString().c_str());
    } else {
        ESP_LOGE("OTA", "Failed to connect to WiFi after multiple attempts.");
    }
}

void OTAHandler::performUpdate() {
    if (!updateUrl) {
        ESP_LOGI("OTA", "Update URL not set. Please call setup() first.");
        return;
    }
    connectToWiFi(); // Ensure WiFi is connected
    ESP_LOGI("OTA", "Starting OTA update from URL: %s", updateUrl);

    HTTPClient http;
    http.begin(updateUrl);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        ESP_LOGI("OTA", "HTTP GET failed, error: %s", http.errorToString(httpCode).c_str());
        http.end();
        return;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        ESP_LOGI("OTA", "Invalid content length: %d", contentLength);
        http.end();
        return;
    }

    bool canBegin = Update.begin(contentLength);
    if (!canBegin) {
        Serial.println("Not enough space to begin OTA update.");
        http.end();
        return;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);

    if (written == contentLength) {
        ESP_LOGI("OTA", "OTA update written successfully.");
    } else {
        ESP_LOGI("OTA", "OTA update failed. Written: %d, Expected: %d", written, contentLength);
    }

    if (Update.end()) {
        if (Update.isFinished()) {
            ESP_LOGI("OTA", "OTA update completed successfully. Restarting...");
            ESP.restart();
        } else {
            ESP_LOGI("OTA", "OTA update not finished. Something went wrong.");
        }
    } else {
        ESP_LOGI("OTA", "OTA update failed. Error: %s", Update.errorString());
    }

    http.end();
}