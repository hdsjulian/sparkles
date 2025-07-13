#include <Arduino.h>
#include <MyDefines.h>
#include <esp_log.h>
#include "esp_now.h"
#include <FastLED.h>
#define MIDI_MODE 1
#define FREQUENCY 2
#define INPUT_MODE FREQUENCY
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
const float minRms = 0.1;
const float maxRms = 8.0;
uint8_t value = 0;
bool shimmerOn = false;
float lastPitch = 0.0;
bool builtinLedOn = false;
void outputFrequency(float pitch, float rms) {
      if (rms < minRms ) {
        if (shimmerOn) {
            shimmerOn = false;
            messageData.payload.animation.animationParams.backgroundShimmer.hue = hueRed;
            messageData.payload.animation.animationParams.backgroundShimmer.value = 0;
            esp_now_send(broadcastAddress, (uint8_t*)&messageData, sizeof(messageData));
            rgbLedWrite(RGB_BUILTIN, 0, 0, 0);  // Turn off the RGB LED
            Serial.println("Shimmer off");
        }
        return;
    }

      // Detect pitch change greater than a quarter note
      if (lastPitch > 0 && pitch > 0) {
        float ratio = pitch / lastPitch;
        float quarterNoteRatio = pow(2.0, 1.0/48.0);
        if (ratio > quarterNoteRatio || ratio < 1.0/quarterNoteRatio) {

        }
      }
      lastPitch = pitch;
        if (pitch >= minPitch && pitch <= maxPitch) {
            float scale = (log2(pitch / minPitch)) / (log2(maxPitch / minPitch));
            hue = hueRed + (hueBlue - hueRed) * scale;
        } else if (pitch > maxPitch) {
            hue = hueBlue;
        }
        messageData.payload.animation.animationParams.backgroundShimmer.hue = hue;
        if (rms <= minRms) {
            value = 0;
        } else if (rms < 0.5) {
            // Map [minRms, 0.5) to [0, 180)
            value = (uint8_t)(180.0 * (rms - minRms) / (0.5 - minRms));
        } else if (rms < 1.0) {
            // Map [0.5, 1.0) to [180, 220)
            value = 180 + (uint8_t)(40.0 * (rms - 0.5) / (1.0 - 0.5));
        } else if (rms <= maxRms) {
            // Map [1.0, maxRms] to [220, 255]
            value = 220 + (uint8_t)(35.0 * (rms - 1.0) / (maxRms - 1.0));
        } else {
            value = 255;
        }
        messageData.payload.animation.animationParams.backgroundShimmer.value = value;
        uint8_t saturation = 0;
        if (rms <= minRms) {
            saturation = 100;
        } else if (rms < 0.5) {
            // Map [minRms, 0.5) to [100, 180)
            saturation = (uint8_t)(100.0 + 80.0 * (rms - minRms) / (0.5 - minRms));
        } else if (rms < 1.0) {
            // Map [0.5, 1.0) to [180, 220)
            saturation = 180 + (uint8_t)(40.0 * (rms - 0.5) / (1.0 - 0.5));
        } else if (rms <= maxRms) {
            // Map [1.0, maxRms] to [220, 255]
            saturation = 220 + (uint8_t)(35.0 * (rms - 1.0) / (maxRms - 1.0));
        } else {
            saturation = 255;
        }
        messageData.payload.animation.animationParams.backgroundShimmer.saturation = saturation;

        messageData.payload.animation.animationParams.backgroundShimmer.saturation = 200;
        shimmerOn = true;
        CHSV hsvColor(hue, saturation, value);
        CRGB rgbColor;
        rgbColor = hsvColor; // FastLED will convert CHSV to CRGB
        rgbLedWrite(RGB_BUILTIN, rgbColor.r, rgbColor.g, rgbColor.b);  // Set the RGB LED color
        esp_now_send(broadcastAddress, (uint8_t*)&messageData, sizeof(messageData));
        //ESP_LOGV("LED", "Output frequency: pitch=%.2f, rms=%.2f, hue=%d, value=%d, saturation=%d",
                 //pitch, rms, hue, value, saturation);

        //digitalWrite(LED_BUILTIN, HIGH);
}
unsigned long lastTick, lastTick2;
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
  messageData.messageType = MSG_ANIMATION;
  if (INPUT_MODE == FREQUENCY) {
    messageData.payload.animation.animationType = BACKGROUND_SHIMMER;
    } else if (INPUT_MODE == MIDI) {
    messageData.payload.animation.animationType = MIDI;
    }
    lastTick = millis();
    rgbLedWrite(RGB_BUILTIN, 0, 0, 0);  // Off / black
    delay(1000);
}

void loop() {

 
  if (Serial.available()) {
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
      if (INPUT_MODE == FREQUENCY) {
        outputFrequency(pitch, rms);
      } else if (INPUT_MODE == MIDI_MODE) {
        // Convert pitch to MIDI note
      }  
    }
    }

}

