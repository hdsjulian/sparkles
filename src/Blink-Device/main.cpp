#include <Arduino.h>
#include <MyDefines.h>
#include <esp_log.h>
#include "esp_now.h"
#include "WiFi.h"

static esp_now_peer_info_t broadcastPeer;

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        ESP_LOGE("BLINK", "ESP-NOW init failed");
        return;
    }
    esp_now_register_send_cb(onDataSent);
    memset(&broadcastPeer, 0, sizeof(broadcastPeer));
    memcpy(broadcastPeer.peer_addr, broadcastAddress, 6);
    broadcastPeer.channel = 0;
    broadcastPeer.encrypt = false;
    esp_now_add_peer(&broadcastPeer);
    ESP_LOGI("BLINK", "Blink device ready");
}

void loop() {
    message_data msg;
    msg.messageType = MSG_ANIMATION;
    memcpy(msg.targetAddress, broadcastAddress, 6);
    WiFi.macAddress(msg.senderAddress);
    msg.payload.animation.animationType = BLINK;
    msg.payload.animation.timeStamp = millis();
    msg.payload.animation.animationParams.blink.startTime  = 0;
    msg.payload.animation.animationParams.blink.duration   = 500;
    msg.payload.animation.animationParams.blink.repetitions = 1;
    msg.payload.animation.animationParams.blink.hue        = 0;
    msg.payload.animation.animationParams.blink.saturation = 0;
    msg.payload.animation.animationParams.blink.brightness = 255;

    esp_now_send(broadcastAddress, (uint8_t *)&msg, ESPNOW_CLIENT_COMPAT_SIZE);
    ESP_LOGI("BLINK", "Sent blink");

    delay(3000);
}
