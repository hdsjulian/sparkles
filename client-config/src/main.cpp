// test ing
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "ESP32TimerInterrupt.h"
#define TIMER_INTERVAL_MS       1000
#define USING_TIM_DIV1 true

#define PRINT_MODE -1
#define HELLO 0
#define BLINK 1
#define ADDRESS_RCVD 2
#define LETSGO 3
#define NUM_ADDRESSES 4
int mode;
bool letsgo = false;
bool blinking = false;
int freq = 5000;
int resolution = 8;
int randnum = 0;

const int ledPinBlue = 20;  // 16 corresponds to GPIO16
const int ledPinRed = 9; // 17 corresponds to GPIO17
const int ledPinGreen = 3;  // 5 corresponds to GPIO5
const int ledPinGreen2 = 8;
const int ledPinRed2 = 19;
const int ledPinBlue2 = 18;
const int ledChannelRed1 = 0;
const int ledChannelGreen1 = 1;
const int ledChannelBlue1 = 2;
const int ledChannelRed2 = 3;
const int ledChannelGreen2 = 4;
const int ledChannelBlue2 = 5;

float redfloat = 0, greenfloat = 0, bluefloat = 0;
int steps = 1000;
//Network
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
//uint8_t broadcastAddress[] = {0xB4,0xE6,0x2D,0xE9,0x3C,0x21};
uint8_t myAddress[6];
esp_now_peer_info_t peerInfo;


//Variables for Time Offset


struct message_address {
  uint8_t messageType = HELLO;
  uint8_t address[6];
} addressMessage;

struct message_blink {
  uint8_t message_Type = BLINK;
  uint8_t color[3];
  uint8_t address[6];
} blinkMessage;

struct addresses {
  uint8_t address[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t rcvd[NUM_ADDRESSES];
} addressList[NUM_ADDRESSES];
int addressCounter = 0;

struct msg_address_rcvd {
  uint8_t message = ADDRESS_RCVD;
  uint8_t address[6];
} addressRcvd;

struct msg_letsgo {
  uint8_t message = LETSGO;
} letsgoMsg;
//address sending
int addressReceived = false;
int addressSending = 0;




void sendAddress() {
  memcpy(&addressMessage.address, myAddress, sizeof(myAddress));
  esp_now_send(broadcastAddress, (uint8_t *) &addressMessage, sizeof(addressMessage));
}

void blink(message_blink blinkMessage) {
  blinking = true;
  for (int i = 0; i < steps; i++) {
    redfloat += (float)blinkMessage.color[0]/steps;
    greenfloat += (float)blinkMessage.color[1]/steps;
    bluefloat += (float)blinkMessage.color[2]/steps;
  ledcWrite(ledChannelRed2, (int)floor(redfloat));
  ledcWrite(ledChannelGreen2, (int)floor(greenfloat));
  ledcWrite(ledChannelBlue2, (int)floor(bluefloat));
  ledcWrite(ledChannelRed1, (int)floor(redfloat));
  ledcWrite(ledChannelGreen1, (int)floor(greenfloat));
  ledcWrite(ledChannelBlue1, (int)floor(bluefloat));
  delay(2);
  }
  while (true) {
    randnum = random(NUM_ADDRESSES);
    if (memcmp(&blinkMessage.address, &addressList[randnum], sizeof(blinkMessage.address)) == false) {
      memcpy(&blinkMessage.address, myAddress, sizeof(myAddress));
      blinkMessage.color[0] = random(256);
      blinkMessage.color[1] = random(256);
      blinkMessage.color[2] = random(256);
      esp_now_send(addressList[randnum].address, (uint8_t *) &blinkMessage, sizeof(blinkMessage));
      break;
    }

  }
  for (int i = steps; i > 0; i--) {
    redfloat -= (float)blinkMessage.color[0]/steps;
    greenfloat -= (float)blinkMessage.color[1]/steps;
    bluefloat -= (float)blinkMessage.color[2]/steps;
  ledcWrite(ledChannelRed2, (int)floor(redfloat));
  ledcWrite(ledChannelGreen2, (int)floor(greenfloat));
  ledcWrite(ledChannelBlue2, (int)floor(bluefloat));
  ledcWrite(ledChannelRed1, (int)floor(redfloat));
  ledcWrite(ledChannelGreen1, (int)floor(greenfloat));
  ledcWrite(ledChannelBlue1, (int)floor(bluefloat));   
  delay(2); 
  }
  blinking = false;
}


void receiveAddress(uint8_t address[6]) {
  Serial.print("Address received ");
  Serial.println(address[5]);
  for (int i = 0; i<sizeof(addressList); i++) {
    if (memcmp(&address, &addressList[i], sizeof(address))) {
      return;
    }
    memcpy(&addressList[addressCounter], &address, sizeof(address));
    memcpy(&addressRcvd.address, myAddress, sizeof(myAddress));
    esp_now_send(address, (uint8_t *) &addressRcvd, sizeof(addressRcvd));
    addressCounter++;
    if (addressCounter == NUM_ADDRESSES-1 and letsgo == false) {
      esp_now_send(broadcastAddress, (uint8_t *) &letsgoMsg, sizeof(letsgoMsg));
      blinkMessage.color[0] = 0xFF;
      blinkMessage.color[1] = 0xFF; 
      blinkMessage.color[2] = 0xFF;
      delay(5);
      blink(blinkMessage);
    }
  }
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  switch (incomingData[0]) {
    case HELLO:  
      memcpy(&addressMessage,incomingData,sizeof(addressMessage));
      receiveAddress(addressMessage.address);
      break;
    case ADDRESS_RCVD: 
      //add stuff regarding received address
      break;
    case BLINK: 
      if (blinking == false) {}
        memcpy(&blinkMessage,incomingData,sizeof(blinkMessage));
        blink(blinkMessage);
      break;
    case LETSGO:
      letsgo = true;
      break;

    default: 
      Serial.println("Data type not recognized");
  }
}
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {

}









void setup() {
 
  // Initialize Serial Monitor
  Serial.begin(115200);
  // start timer
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  WiFi.macAddress(addressMessage.address);

  // Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
    // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);  
  esp_now_register_send_cb(OnDataSent);
  ledcSetup(ledChannelRed1, freq, resolution);
  ledcSetup(ledChannelGreen1, freq, resolution);
  ledcSetup(ledChannelBlue1, freq, resolution);
 ledcSetup(ledChannelRed2, freq, resolution);
  ledcSetup(ledChannelGreen2, freq, resolution);
  ledcSetup(ledChannelBlue2, freq, resolution);

}


void loop() {
  if (addressCounter <NUM_ADDRESSES-1) {
    Serial.println("Address sent");
    sendAddress();
    delay(1);
  }
  else {
    Serial.println("Address counter full");
  }

}