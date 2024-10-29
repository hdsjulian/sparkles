#include <Arduino.h>
#include <BLEMidi.h>
#include "esp_now.h"
#include "WiFi.h" 
#include <HTTPClient.h>
#include <LittleFS.h>
#include "../extra_src/myDefines.h"
const char* fileURL = "http://192.168.4.1/clientAddress";
unsigned long lastKeyPressed = 0;
const char* filePath = "/clientAddress";
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t hostAddress[6] = {0x34, 0x85, 0x18, 0x8e, 0xf9, 0x68 };
esp_now_peer_info_t peerInfo;
esp_now_peer_num_t peerNum;
unsigned long bliblub =0;
void onNoteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp)
{

  if (channel == 0) {
    esp_now_send(broadcastAddress, (uint8_t *) &note, sizeof(note));
  }

  Serial.printf("Received note on : channel %d, note %d, velocity %d (timestamp %dms)\n", channel, note, velocity, timestamp);
}

void onNoteOff(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp)
{
  Serial.printf("Received note off : channel %d, note %d, velocity %d (timestamp %dms)\n", channel, note, velocity, timestamp);
}

void  OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {}
void OnDataRecv(const esp_now_recv_info * mac, const uint8_t *incomingData, int len) {}


void wifiDownload() {
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
    }
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(fileURL);
    int httpCode = http.GET();

    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();

        // Save the file to LittleFS
        File file = LittleFS.open(filePath, FILE_WRITE);
        if (!file) {
          Serial.println("Failed to open file for writing");
          return;
        }
        file.print(payload);
        file.close();
        Serial.println("File downloaded and saved to LittleFS");
      }
    } else {
      Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
    ESP_LOGI(TAG, "WiFi connected");
    WiFi.disconnect(true);
  } else {
    Serial.println("WiFi not connected");
  }
  
}



void setup() {
    Serial.begin(115200);
    Serial.println("Initializing bluetooth");
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != 0) {
      Serial.println("Error initializing ESP-NOW");
      return;
    }
    esp_now_get_peer_num(&peerNum);

    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
      // Add peer        
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add peer");
      return;
    }    
    BLEMidiClient.begin("Midi client"); // "Midi client" is the name you want to give to the ESP32
    BLEMidiClient.setNoteOnCallback(onNoteOn);
    BLEMidiClient.setNoteOffCallback(onNoteOff);
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv); 
    wifiDownload(); 
    //BLEMidiClient.enableDebugging();  // Uncomment to see debugging messages from the library

}

void loop() {
    if(!BLEMidiClient.isConnected()) {
        // If we are not already connected, we try te connect to the first BLE Midi device we find
        int nDevices = BLEMidiClient.scan();
        if(nDevices > 0) {
            if(BLEMidiClient.connect(0))
                Serial.println("Connection established");
            else {
                Serial.println("Connection failed");
                delay(3000);    // We wait 3s before attempting a new connection
            }
        }
    }
    else {
        if (bliblub+1000 < millis()) {
          Serial.println("bliblub");
          bliblub = millis();
        }
        // Real world example : it starts the drum function of the NUX Mighty Lit BT guitar amplifier
        // https://www.nuxefx.com/mighty-lite-bt.html
        // This is the only bluetooth controllable device that I have.
    }
}