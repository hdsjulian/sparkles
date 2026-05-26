#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_log.h>
#include <MyDefines.h>

static const char* animName(uint8_t t) {
    switch (t) {
        case OFF:              return "OFF";
        case FLASH:            return "FLASH";
        case BLINK:            return "BLINK";
        case BATTERY_BLINK:    return "BATTERY_BLINK";
        case CANDLE:           return "CANDLE";
        case SYNC_ASYNC_BLINK: return "SYNC_ASYNC_BLINK";
        case SYNC_BLINK:       return "SYNC_BLINK";
        case SLOW_STARTUP:     return "SLOW_STARTUP";
        case SYNC_END:         return "SYNC_END";
        case LED_ON:           return "LED_ON";
        case CONCENTRIC:       return "CONCENTRIC";
        case MIDI:             return "MIDI";
        case BACKGROUND_SHIMMER: return "BACKGROUND_SHIMMER";
        case STROBE:           return "STROBE";
        default:               return "UNKNOWN";
    }
}

static const char* msgTypeName(uint8_t t) {
    switch (t) {
        case MSG_ADDRESS:            return "MSG_ADDRESS";
        case MSG_TIMER:              return "MSG_TIMER";
        case MSG_GOT_TIMER:          return "MSG_GOT_TIMER";
        case MSG_STATUS:             return "MSG_STATUS";
        case MSG_ANIMATION:          return "MSG_ANIMATION";
        case MSG_SEND_CLAP_TIMES:    return "MSG_SEND_CLAP_TIMES";
        case MSG_SYSTEM_STATUS:      return "MSG_SYSTEM_STATUS";
        case MSG_ASK_COMMAND:        return "MSG_ASK_COMMAND";
        case MSG_WAIT_FOR_INSTRUCTIONS: return "MSG_WAIT_FOR_INSTRUCTIONS";
        case MSG_SLEEP_WAKEUP:       return "MSG_SLEEP_WAKEUP";
        case MSG_SYNC:               return "MSG_SYNC";
        case MSG_CLAP:               return "MSG_CLAP";
        case MSG_CONFIG_DATA:        return "MSG_CONFIG_DATA";
        case MSG_UPDATE_VERSION:     return "MSG_UPDATE_VERSION";
        case MSG_COMMAND:            return "MSG_COMMAND";
        case MSG_MIDI_PARAMS:        return "MSG_MIDI_PARAMS";
        case MSG_DARKROOM_PARAMS:    return "MSG_DARKROOM_PARAMS";
        default:                     return "UNKNOWN";
    }
}

void onDataRecv(const esp_now_recv_info* info, const uint8_t* data, int len) {
    if (len < 1) return;
    const uint8_t* mac = info->src_addr;
    message_data msg;
    memcpy(&msg, data, min((int)sizeof(msg), len));

    ESP_LOGI("LOG", "FROM %02X:%02X:%02X:%02X:%02X:%02X | type=%s(%d) | len=%d",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
        msgTypeName(msg.messageType), msg.messageType, len);

    if (msg.messageType == MSG_ANIMATION) {
        ESP_LOGI("LOG", "  ANIMATION type=%s(%d)",
            animName(msg.payload.animation.animationType),
            msg.payload.animation.animationType);
        if (msg.payload.animation.animationType == BACKGROUND_SHIMMER) {
            ESP_LOGI("LOG", "  SHIMMER hue=%d sat=%d val=%d",
                msg.payload.animation.animationParams.backgroundShimmer.hue,
                msg.payload.animation.animationParams.backgroundShimmer.saturation,
                msg.payload.animation.animationParams.backgroundShimmer.value);
        } else if (msg.payload.animation.animationType == MIDI) {
            ESP_LOGI("LOG", "  MIDI note=%d vel=%d",
                msg.payload.animation.animationParams.midi.note,
                msg.payload.animation.animationParams.midi.velocity);
        } else if (msg.payload.animation.animationType == BLINK ||
                   msg.payload.animation.animationType == BATTERY_BLINK) {
            ESP_LOGI("LOG", "  BLINK reps=%d dur=%d brightness=%d",
                msg.payload.animation.animationParams.blink.repetitions,
                msg.payload.animation.animationParams.blink.duration,
                msg.payload.animation.animationParams.blink.brightness);
        }
    } else if (msg.messageType == MSG_COMMAND) {
        ESP_LOGI("LOG", "  COMMAND type=%d", msg.payload.command.commandType);
    } else if (msg.messageType == MSG_MIDI_PARAMS) {
        ESP_LOGI("LOG", "  MIDI_PARAMS minVal=%d maxVal=%d minDb=%.2f maxDb=%.2f mode=%d",
            msg.payload.midiParams.valMin, msg.payload.midiParams.valMax,
            msg.payload.midiParams.rmsMin, msg.payload.midiParams.rmsMax,
            msg.payload.midiParams.mode);
    } else if (msg.messageType == MSG_LOG) {
        msg.payload.log.text[sizeof(msg.payload.log.text) - 1] = '\0';
        ESP_LOGI("MASTER", "%s", msg.payload.log.text);
    }
}

void setup() {
    esp_log_level_set("*", ESP_LOG_INFO);
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    unsigned long start = millis();
    while (!Serial && millis() - start < 5000);
    delay(200);

    uint8_t mac[6];
    WiFi.macAddress(mac);
    ESP_LOGI("LOG", "Log Device MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    if (esp_now_init() != ESP_OK) {
        ESP_LOGE("LOG", "ESP-NOW init failed");
        return;
    }
    esp_now_register_recv_cb(onDataRecv);
    ESP_LOGI("LOG", "Listening for ESP-NOW messages...");
}

void loop() {}
