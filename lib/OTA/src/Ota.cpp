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
    ESP_LOGI("OTA", "Connecting to WiFi: %s", OTA_WIFI_SSID);
    WiFi.begin(OTA_WIFI_SSID, OTA_WIFI_PASSWORD);
    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
        delay(500);
        ESP_LOGI("OTA", "Attempting to connect to WiFi... (%d)", retryCount + 1);
        retryCount++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        ESP_LOGI("OTA", "Connected to WiFi. IP: %s", WiFi.localIP().toString().c_str());
    } else {
        ESP_LOGE("OTA", "Failed to connect to WiFi.");
    }
}

void OTAHandler::performUpdate() {
    if (!updateUrl) {
        ESP_LOGE("OTA", "Update URL not set — call setup() first.");
        return;
    }
    ESP_LOGI("OTA", "Starting OTA from: %s", updateUrl);

    unsigned long t0 = millis();
    unsigned long t1 = 0;

    httpUpdate.onStart([&t1]() {
        t1 = millis();
    });

    httpUpdate.onEnd([t0, &t1]() {
        unsigned long now = millis();
        ESP_LOGI("OTA", "Flash write: %lu ms", now - t1);
        ESP_LOGI("OTA", "Total OTA time: %lu ms", now - t0);
    });

    WiFiClient client;
    t_httpUpdate_return ret = httpUpdate.update(client, updateUrl);

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            ESP_LOGE("OTA", "OTA failed (%d): %s",
                     httpUpdate.getLastError(),
                     httpUpdate.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            ESP_LOGI("OTA", "No update available.");
            break;
        case HTTP_UPDATE_OK:
            ESP_LOGI("OTA", "OTA OK — rebooting.");
            break;
    }
}