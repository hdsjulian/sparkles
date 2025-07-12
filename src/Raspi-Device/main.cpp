#include <Arduino.h>
#include <MyDefines.h>
#include <esp_log.h>
#include "esp_now.h"

#define MIDI_MODE 1
#define FREQUENCY 2
#define INPUT_MODE FREQUENCY

#define LEDPIN LED_BUILTIN
message_data messageData;

const float minPitch = 100.0;
const float maxPitch = 1000.0;
const uint8_t hueRed = 0;
const uint8_t hueBlue = 160;
uint8_t hue = hueRed; // default
const float minRms = 0.5;
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
            digitalWrite(LED_BUILTIN, LOW);
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
        } else if (rms >= maxRms) {
          value = 255;
        } else {
          value = (uint8_t)(255.0 * (rms - minRms) / (maxRms - minRms));    
        }
        messageData.payload.animation.animationParams.backgroundShimmer.value = value;
        uint8_t saturation = 0;
        if (rms <= minRms) {
            saturation = 100;
        } else if (rms >= maxRms) {
            saturation = 255;
        } else {
            saturation = (uint8_t)(100 + 155.0 * (rms - minRms) / (maxRms - minRms));
        }
        messageData.payload.animation.animationParams.backgroundShimmer.saturation = saturation;

        messageData.payload.animation.animationParams.backgroundShimmer.saturation = 200;
        shimmerOn = true;
        esp_now_send(broadcastAddress, (uint8_t*)&messageData, sizeof(messageData));
        Serial1.printf("Output frequency: pitch=%.2f, rms=%.2f, hue=%d, value=%d, saturation=%d",
                 pitch, rms, hue, value, saturation);
        Serial1.println();
        digitalWrite(LED_BUILTIN, HIGH);
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
    digitalWrite(LEDPIN, HIGH); // Ensure LED is off at startup
    delay(3000);
    digitalWrite(LEDPIN, LOW); // Turn off LED after 3 seconds
}

void loop() {

 
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    Serial.printf("Received line: %s\n", line.c_str());
    ESP_LOGV("MYTAG", "Serial data available, reading line.");
    digitalWrite(LEDPIN, HIGH); // Turn on LED to indicate data received
    delay(1000);
    digitalWrite(LEDPIN, LOW);
  }
}
/*    else if (millis() - lastTick > 1000) {
    ESP_LOGV("MYTAG", "No serial data available.");
   
    lastTick = millis();
    }
    */
  
  /*  String line = Serial.readStringUntil('\n');
    ESP_LOGV("MYTAG", "Received line: %s", line.c_str());
    digitalWrite(LEDPIN, HIGH); // Turn on LED to indicate data received
    delay(1000);
    digitalWrite(LEDPIN, LOW); // Turn off LED after 1 second
    line.trim();
    if (line.length() == 0) return;

    // Parse CSV: counter,elapsed_ms,pitch,rms
    int firstComma = line.indexOf(',');
    int secondComma = line.indexOf(',', firstComma + 1);
    int thirdComma = line.indexOf(',', secondComma + 1);

    if (firstComma > 0 && secondComma > firstComma && thirdComma > secondComma) {
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

}*/

