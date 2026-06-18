// Music-Device: dedicated ESP-NOW broadcaster for the live music output.
// The Pi (aubioAlgo + keyboard_midi) sends aubio_shimmer / aubio_midi /
// keyboard_midi commands over serial, this device converts each to a
// MSG_ANIMATION and broadcasts it to the clients. Nothing else runs here, so
// the 30 Hz media stream never competes with the master's sync/management work.

#include <Arduino.h>
#include <MyDefines.h>
#include <esp_log.h>
#include "esp_now.h"
#include "WiFi.h"
#include <ArduinoJson.h>

uint8_t myAddress[6];

// fixed-size line buffer, no String churn on the hot path
static char lineBuffer[256];
static size_t lineLen = 0;

void addBroadcastPeer() {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, broadcastAddress, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_get_peer(peer.peer_addr, &peer) != ESP_OK) {
        esp_now_add_peer(&peer);
    }
}

void broadcastAnimation(const message_animation& anim) {
    message_data message;
    message.messageType = MSG_ANIMATION;
    memcpy(message.targetAddress, broadcastAddress, 6);
    memcpy(message.senderAddress, myAddress, 6);
    memcpy(&message.payload.animation, &anim, sizeof(anim));
    // 80-byte compat frame, same as the master sends, so older clients accept it
    esp_err_t r = esp_now_send(broadcastAddress, (uint8_t*)&message, ESPNOW_CLIENT_COMPAT_SIZE);
    if (r != ESP_OK) {
        ESP_LOGW("MUSIC", "esp_now_send failed: %d", r);
    }
}

void sendIdentity() {
    JsonDocument doc;
    doc["event"] = "identity";
    doc["role"]  = "music";
    char mac[18];
    snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
             myAddress[0], myAddress[1], myAddress[2], myAddress[3], myAddress[4], myAddress[5]);
    doc["address"] = mac;
    String out; serializeJson(doc, out); Serial.println(out);
}

void handleSerialCommand(const char* line) {
    JsonDocument doc;
    if (deserializeJson(doc, line)) return;
    const char* cmd = doc["cmd"] | "";

    if (strcmp(cmd, "aubio_shimmer") == 0) {
        message_animation anim;
        anim.animationType = BACKGROUND_SHIMMER;
        anim.animationParams.backgroundShimmer.hue        = doc["hue"]        | 22;
        anim.animationParams.backgroundShimmer.saturation = doc["saturation"] | 255;
        anim.animationParams.backgroundShimmer.value      = doc["value"]      | 0;
        broadcastAnimation(anim);

    } else if (strcmp(cmd, "aubio_midi") == 0) {
        message_animation anim;
        anim.animationType = MIDI;
        anim.animationParams.midi.note       = doc["note"]     | 0;
        anim.animationParams.midi.velocity   = doc["velocity"] | 0;
        anim.animationParams.midi.instrument = 0; // mic
        broadcastAnimation(anim);

    } else if (strcmp(cmd, "keyboard_midi") == 0) {
        message_animation anim;
        anim.animationType = MIDI;
        anim.animationParams.midi.note       = doc["note"]     | 0;
        anim.animationParams.midi.velocity   = doc["velocity"] | 0;
        anim.animationParams.midi.instrument = 1; // keyboard
        broadcastAnimation(anim);

    } else if (strcmp(cmd, "identify") == 0) {
        sendIdentity();
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.macAddress(myAddress);

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    addBroadcastPeer();

    delay(500);
    sendIdentity(); // announce role on boot so the Pi can route to us
    ESP_LOGI("MUSIC", "Ready");
}

void loop() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n') {
            lineBuffer[lineLen] = '\0';
            if (lineLen > 0) {
                handleSerialCommand(lineBuffer);
            }
            lineLen = 0;
        } else if (lineLen < sizeof(lineBuffer) - 1) {
            lineBuffer[lineLen++] = c;
        } else {
            lineLen = 0; // overrun, drop the line
        }
    }
}
