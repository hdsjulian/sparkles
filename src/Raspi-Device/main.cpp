#include <Arduino.h>
#include <MyDefines.h>
#include <esp_log.h>
#include "esp_now.h"
#include <FastLED.h>
#include <math.h>


#define RX_PIN 18
#define TX_PIN 17
#define RX0_PIN 44
#define TX0_PIN 43
#define LEDPIN 2
#define NUM_LEDS    1   
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
message_data messageData;
CRGB leds[NUM_LEDS];
const float minPitch = 100.0;
const float maxPitch = 1000.0;
const uint8_t hueRed = 0;
const uint8_t hueBlue = 160;
uint8_t hue = hueRed; // default
const float minRms = 0.008;
const float maxRms = 1.0;
int minVal = 0;
int maxVal = 255;
int minSat = 127;
int maxSat = 255;
//int hue = 25;
uint8_t value = 0;
bool shimmerOn = false;
float lastPitch = 0.0;
bool builtinLedOn = false;
message_midi_params midiParams;

// Force a resend if the shimmer value hasn't changed for a while
static uint8_t lastSentShimmerValue = 255; // invalid initial value to ensure first comparison fails
static unsigned long lastShimmerSendMs = 0;
static const unsigned long SHIMMER_RESEND_INTERVAL_MS = 300;

void writeLeds(CRGB color) {
    //ESP_LOGI("LED", "writing leds %d, %d, %d ", (int)rgb[0],(int)rgb[1], (int)rgb[2]);
    ledcWrite(LEDPINRED1, color.r);
    ledcWrite(LEDPINGREEN1, color.g);
    ledcWrite(LEDPINBLUE1, color.b);
    ledcWrite(LEDPINRED2, color.r);
    ledcWrite(LEDPINGREEN2, color.g);
    ledcWrite(LEDPINBLUE2, color.b);

}
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI("ESP-NOW", "Data sent successfully");
    } else {
        ESP_LOGE("ESP-NOW", "Failed to send data");
    }
}
void onDataRecv(const esp_now_recv_info * mac, const uint8_t *incomingData, int len) {
    ESP_LOGI("MIDI", "Received msg");
    if (incomingData[0] == MSG_MIDI_PARAMS) {
        message_data midiParamsData;
        memcpy(&midiParamsData, incomingData, sizeof(message_data));
        memcpy (&midiParams, &midiParamsData.payload.midiParams, sizeof(message_midi_params));
        ESP_LOGI("MIDI", "Received MIDI params: minVal=%d, maxVal=%d, minSat=%d, maxSat=%d, hue=%d, rangeMin=%d, rangeMax=%d, minRms=%.2f, maxRms=%.2f, mode=%d",
                 midiParams.valMin, midiParams.valMax, midiParams.satMin, midiParams.satMax, midiParams.hue,  midiParams.rangeMin, midiParams.rangeMax, midiParams.rmsMin, midiParams.rmsMax, midiParams.mode);
        if (midiParams.mode == FREQUENCY_MODE) {
             messageData.payload.animation.animationType = BACKGROUND_SHIMMER;
        } else if (midiParams.mode == MIDI) {
           messageData.payload.animation.animationType = MIDI;
        }                 
    }
}

void outputMidi(float pitch, float rms) {
    static int lastMidiNote = -1;
    if (pitch <= 0) return;

    // Use midiParams for dynamic mapping
    float minPitchParam = midiParams.rangeMin > 0 ? midiParams.rangeMin : minPitch;
    float maxPitchParam = midiParams.rangeMax > 0 ? midiParams.rangeMax : maxPitch;
    int valMin = midiParams.valMin > 0 ? midiParams.valMin : minVal;
    int valMax = midiParams.valMax > 0 ? midiParams.valMax : maxVal;
    int satMin = midiParams.satMin > 0 ? midiParams.satMin : minSat;
    int satMax = midiParams.satMax > 0 ? midiParams.satMax : maxSat;
    int hue = midiParams.hue > 0 ? midiParams.hue : hue;

    // Convert pitch to MIDI note number
    int midiNote = round(69 + 12 * log2(pitch / 440.0));
    if (midiNote < 0) midiNote = 0;
    if (midiNote > 127) midiNote = 127;

    // Only send if note changed
    if (midiNote != lastMidiNote) {
        lastMidiNote = midiNote;

        // Map rms to velocity (0–127)

        uint8_t velocity = 0;
        if (rms <= minRms) {
            velocity = 0;
        } else if (rms >= maxRms) {
            velocity = 127;
        } else {
            velocity = (uint8_t)(127.0 * (rms - minRms) / (maxRms - minRms));
        }

        messageData.payload.animation.animationType = MIDI;
        messageData.payload.animation.animationParams.midi.note = midiNote;
        messageData.payload.animation.animationParams.midi.velocity = velocity;
        messageData.payload.animation.animationParams.midi.instrument = INSTRUMENT_MIC;
        // octaveDistance and offset are ignored (left as 0)
        //printMessageData(messageData);
        esp_now_send(broadcastAddress, (uint8_t*)&messageData, sizeof(messageData));
        ESP_LOGI("MIDI", "MIDI note: %d, velocity: %d", midiNote, velocity);
    }
}
void outputFrequency(float pitch, float rms) {
    // Use midiParams for dynamic mapping
    float minPitchParam = midiParams.rangeMin > 0 ? midiParams.rangeMin : minPitch;
    float maxPitchParam = midiParams.rangeMax > 0 ? midiParams.rangeMax : maxPitch;
    uint8_t hueStart = 0;
    uint8_t hueEnd = 160; // Use saturation as hueEnd if needed, or add a new field
    int valMin = midiParams.valMin > 0 ? midiParams.valMin : minVal;
    int valMax = midiParams.valMax > 0 ? midiParams.valMax : maxVal;
    int satMin = midiParams.satMin > 0 ? midiParams.satMin : minSat;
    int satMax = midiParams.satMax > 0 ? midiParams.satMax : maxSat;
    float minRmsParam = midiParams.rmsMin > 0.0 ? midiParams.rmsMin : minRms;
    float maxRmsParam = midiParams.rmsMax > 0.0 ? midiParams.rmsMax : maxRms;


    if (rms < minRmsParam) {
        if (shimmerOn) {
            shimmerOn = false;
            messageData.payload.animation.animationParams.backgroundShimmer.hue = hueStart;
            messageData.payload.animation.animationParams.backgroundShimmer.value = 0;
            //printMessageData(messageData);
            esp_now_send(broadcastAddress, (uint8_t*)&messageData, sizeof(messageData));
            // Update resend tracking for value=0
            lastSentShimmerValue = 0;
            lastShimmerSendMs = millis();
            writeLeds(CRGB(0,0,0));
            Serial.println("Shimmer off");
        }
        return;
    }

    // Only send a new shimmer message if pitch deviates enough (e.g., more than a quarter note)
    bool sendShimmer = false;
    float ratio = 1.0f;
    float quarterNoteRatio = pow(2.0, 1.0/48.0);
    if (lastPitch > 0 && pitch > 0) {
        ratio = pitch / lastPitch;
        if (ratio > quarterNoteRatio || ratio < 1.0/quarterNoteRatio) {
            sendShimmer = true;
        }
    } else {
        // First run, always send
        sendShimmer = true;
    }
    lastPitch = pitch;

    // Map pitch to hue (increase faster in lower range, slower in upper range)
    float scale = 0.0f;
    if (pitch >= minPitchParam && pitch <= maxPitchParam) {
        scale = (log2(pitch / minPitchParam)) / (log2(maxPitchParam / minPitchParam));
        hue = hueStart + (hueEnd - hueStart) * scale;
    } else if (pitch > maxPitchParam) {
        hue = hueEnd;
    } else {
        hue = hueStart;
    }
    messageData.payload.animation.animationParams.backgroundShimmer.hue = hue;
    ESP_LOGI("MIDI", "Pitch updated: %f, Hue updated: %d, scale: %f, minPitch: %f, maxPitch: %f", pitch, hue, scale, minPitchParam, maxPitchParam);

    // Map rms to value using continuous mapping; clamp rms to [minRmsParam, maxRmsParam]
    {
        const float denom = (maxRmsParam - minRmsParam);
        float rClamped = rms;
        if (rClamped < minRmsParam) rClamped = minRmsParam;
        if (rClamped > maxRmsParam) rClamped = maxRmsParam;
        float norm = (denom > 0.0f) ? (rClamped - minRmsParam) / denom : 0.0f;
        float valF = valMin + norm * (valMax - valMin);
        value = (uint8_t)(valF + 0.5f); // round to nearest 0..255
    }
    messageData.payload.animation.animationParams.backgroundShimmer.value = value;

    // Map rms to saturation using continuous mapping; clamp rms to [minRmsParam, maxRmsParam]
    uint8_t saturation = 0;
    {
        const float denomSat = (maxRmsParam - minRmsParam);
        float rSat = rms;
        if (rSat < minRmsParam) rSat = minRmsParam;
        if (rSat > maxRmsParam) rSat = maxRmsParam;
        float normSat = (denomSat > 0.0f) ? (rSat - minRmsParam) / denomSat : 0.0f;
        float satF = satMin + normSat * (satMax - satMin);
        saturation = (uint8_t)(satF + 0.5f);
    }
    messageData.payload.animation.animationParams.backgroundShimmer.saturation = saturation;

    shimmerOn = true;
    CHSV hsvColor(hue, saturation, value);
    CRGB rgbColor = hsvColor;
    writeLeds(rgbColor);

    // Force a refresh if value hasn't changed for SHIMMER_RESEND_INTERVAL_MS
    bool refreshDue = (value == lastSentShimmerValue) && (millis() - lastShimmerSendMs >= SHIMMER_RESEND_INTERVAL_MS);

    // Send when volume (value) changed more than 10% compared to the previous sent value
    float denom = (lastSentShimmerValue == 0) ? 1.0f : (float)lastSentShimmerValue;
    bool volumeChangeDue = (fabs((float)value - (float)lastSentShimmerValue) / denom) > 0.10f;

    if (sendShimmer || refreshDue || volumeChangeDue) {
        //printMessageData(messageData);
        esp_now_send(broadcastAddress, (uint8_t*)&messageData, sizeof(messageData));
        lastSentShimmerValue = value;
        lastShimmerSendMs = millis();
    }
}
unsigned long lastTick, lastTick2;
/*
void outputMidi(float pitch, float rms) {
    static int lastMidiNote = -1;
    if (pitch <= 0) return;

    // Convert pitch to MIDI note number
    int midiNote = round(69 + 12 * log2(pitch / 440.0));
    if (midiNote < 0) midiNote = 0;
    if (midiNote > 127) midiNote = 127;

    // Only send if note changed
    if (midiNote != lastMidiNote) {
        lastMidiNote = midiNote;

        // Map rms to velocity (0–127)
        const float minRms = 0.5;
        const float maxRms = 8.0;
        uint8_t velocity = 0;
        if (rms <= minRms) {
            velocity = 0;
        } else if (rms >= maxRms) {
            velocity = 127;
        } else {
            velocity = (uint8_t)(127.0 * (rms - minRms) / (maxRms - minRms));
        }

        messageData.payload.animation.animationType = MIDI;
        messageData.payload.animation.animationParams.midi.note = midiNote;
        messageData.payload.animation.animationParams.midi.velocity = velocity;
        // octaveDistance and offset are ignored (left as 0)
        esp_now_send(broadcastAddress, (uint8_t*)&messageData, sizeof(messageData));
    }
}*/

void printMessageData(const message_data& msg) {
    Serial.print("messageType: "); Serial.println(msg.messageType);
    Serial.print("targetAddress: ");
    for (int i = 0; i < 6; ++i) { Serial.printf("%02X", msg.targetAddress[i]); if (i<5) Serial.print(":"); }
    Serial.println();
    Serial.print("senderAddress: ");
    for (int i = 0; i < 6; ++i) { Serial.printf("%02X", msg.senderAddress[i]); if (i<5) Serial.print(":"); }
    Serial.println();
    Serial.print("msgReceiveTime: "); Serial.println((uint64_t)msg.msgReceiveTime);
    Serial.print("payload.animationType: "); Serial.println((int)msg.payload.animation.animationType);
    // Print animation params depending on type
    switch (msg.payload.animation.animationType) {
        case MIDI:
            Serial.print("  MIDI note: "); Serial.println(msg.payload.animation.animationParams.midi.note);
            Serial.print("  velocity: "); Serial.println(msg.payload.animation.animationParams.midi.velocity);
            Serial.print("  instrument: "); Serial.println(msg.payload.animation.animationParams.midi.instrument);
            break;
        case BACKGROUND_SHIMMER:
            Serial.print("  hue: "); Serial.println(msg.payload.animation.animationParams.backgroundShimmer.hue);
            Serial.print("  sat: "); Serial.println(msg.payload.animation.animationParams.backgroundShimmer.saturation);
            Serial.print("  val: "); Serial.println(msg.payload.animation.animationParams.backgroundShimmer.value);
            break;
        default:
            Serial.println("  (other animation type)");
            break;
    }
}

void setup() {
  Serial.begin(115200); // Native USB CDC or UART0 (GPIO 43/44)
  //Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN); // UART1 on GPIO 17/18
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    while (true) delay(1000);
    pinMode(LED_BUILTIN, OUTPUT);
  }
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
  esp_now_register_recv_cb(onDataRecv);
    esp_now_register_send_cb(onDataSent);
  messageData.messageType = MSG_ANIMATION;
  if (midiParams.mode == FREQUENCY_MODE) {
    messageData.payload.animation.animationType = BACKGROUND_SHIMMER;
    } else if (midiParams.mode == MIDI) {
    messageData.payload.animation.animationType = MIDI;
    }
    lastTick = millis();
    rgbLedWrite(RGB_BUILTIN, 0, 0, 0);  // Off / black
    midiParams.rmsMin = minRms;
    midiParams.rmsMax = maxRms;
    midiParams.valMin = minVal;
    midiParams.valMax = maxVal;
    midiParams.satMin = minSat;
    midiParams.satMax = maxSat;
    midiParams.hue = hue;
    ledcAttach(LEDPINRED1, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    
    ledcAttach(LEDPINBLUE1, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    
    ledcAttach(LEDPINGREEN1, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    ledcAttach(LEDPINRED2, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    ledcAttach(LEDPINGREEN2, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    ledcWrite(LEDPINRED1, 0);
    ledcWrite(LEDPINGREEN1, 0);
    ledcWrite(LEDPINBLUE1, 0);
    ledcWrite(LEDPINRED2, 0);
    ledcWrite(LEDPINGREEN2, 0);
    ledcWrite(LEDPINBLUE2, 0);
    delay(1000);
}

void loop() {

 if (millis() - lastTick > 3000) {
    uint8_t address[6];
    WiFi.macAddress(address);
    ESP_LOGI("MAC", "Device MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             address[0], address[1], address[2], address[3], address[4], address[5]);
    lastTick = millis();
    ESP_LOGI("MIDI", "MIDI params: minVal=%d, maxVal=%d, minSat=%d, maxSat=%d, rangeMin=%d, rangeMax=%d, minRms=%.2f, maxRms=%.2f, mode=%d",
             midiParams.valMin, midiParams.valMax, midiParams.satMin, midiParams.satMax, midiParams.rangeMin, midiParams.rangeMax, midiParams.rmsMin, midiParams.rmsMax, midiParams.mode);
             
 }
  if (Serial.available() ) {
     String line = Serial.readStringUntil('\n');
    //ESP_LOGV("Serial", "Received line: %s", line.c_str());
    line.trim();
    if (line.length() == 0) return;

    // Parse CSV: counter,elapsed_ms,pitch,rms
    int firstComma = line.indexOf(',');


    if (firstComma > 0 ) {
      // Extract values
      String pitchStr = line.substring(0, firstComma);
      String rmsStr = line.substring(firstComma + 1);
      float pitch = pitchStr.toFloat();
      float rms = rmsStr.toFloat();
      if (midiParams.mode == FREQUENCY_MODE) {
        outputFrequency(pitch, rms);
      } else if (midiParams.mode == MIDI_MODE) {
        outputMidi(pitch, rms);
        // Convert pitch to MIDI note
      }  
    }
    }

}

