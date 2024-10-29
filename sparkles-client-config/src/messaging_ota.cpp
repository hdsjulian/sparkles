#include "Arduino.h"

#include "esp_now.h"
#include "WiFi.h"
#include "messaging.h"
#include <HTTPClient.h>
#include <HTTPUpdate.h>

void messaging::handleOTA() {
    if (otaMessage.version <= version) {
        addError("Version "+String(otaMessage.version)+"\n");
        addError("matches current version "+String(version)+"\n");
        pushDataToSendQueue(hostAddress, MSG_OTA_UPDATE, -1);
    }
    else {
        addError("Version "+String(otaMessage.version)+"\n");
        addError("does not match current version "+String(version)+"\n");
        addError("Starting OTA Update\n");
        WiFi.begin(WIFI_SSID, PASSWORD);
        while (WiFi.status() != WL_CONNECTED) {
            delay(10);
        }
        performOTAUpdate();
    }
}

void messaging::performOTAUpdate() {
    NetworkClient client;
    t_httpUpdate_return ret = httpUpdate.update(client, FIRMWARE_URL);
    if (ret == HTTP_UPDATE_OK) {
        addError("OTA Update Success\n");
        ESP.restart();
    }
    else {
        addError("OTA Update Failed\n");
    }

}