#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#define PRINT_MODE -1
#define HELLO 0
#define BLINK 1
#define ADDRESS_RCVD 2
#define LETSGO 3
#define NUM_ADDRESSES 4
#define ADDRESS_SIZE 6
int mode;
int letsgo = 0;
int first = 0;
bool sendingLetsgo = false;
bool blinking = false;
int freq = 5000;
int resolution = 8;
int randnum = 0;
int bla = 0;
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
esp_now_peer_num_t numPeers;
esp_now_peer_info_t testPeerInfo;

//Variables for Time Offset


struct message_address {
  uint8_t messageType = HELLO;
  uint8_t address[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
} outgoingAddressMessage, incomingAddressMessage;

struct message_blink {
  uint8_t messageType = BLINK;
  uint8_t color[3];
  uint8_t address[6] = {0x01, 0x00, 0x00, 0x01, 0x01, 0x01};;
} blinkMessage;

struct addresses {
  uint8_t address[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t rcvd;
} addressList[NUM_ADDRESSES-1];
int addressCounter = 0;

struct msg_address_rcvd {
  uint8_t messageType = ADDRESS_RCVD;
  uint8_t address[6];
} addressRcvd;

struct msg_letsgo {
  uint8_t messageType = LETSGO;
} letsgoMsg;
//address sending
int addressReceived = false;
int addressSending = 0;

void printMessageTypes(int messageType) {
  switch (messageType) {
    case HELLO: 
      Serial.println ("Message Hello");
      break;
    case BLINK: 
      Serial.println("Blink");
      break;
    case ADDRESS_RCVD:
      Serial.println("Address received");
      break;
    case LETSGO: 

      Serial.println("Lets Go");
      break;
    default: 
      Serial.print("Unknown message type ");
      Serial.println(messageType);
  }
}

void printAddress(uint8_t address[6]) {

    for (int i = 0; i < ADDRESS_SIZE; i++) {
      Serial.print(address[i]);
      if (i < ADDRESS_SIZE-1) {
        Serial.print(":");
      }
    }
    Serial.println();
    
}

void sendAddress() {
  Serial.print ("Sent ");
  printMessageTypes(outgoingAddressMessage.messageType);
  Serial.print(" My Address is ");
  printAddress(outgoingAddressMessage.address);
  esp_now_send(broadcastAddress, (uint8_t *) &outgoingAddressMessage, sizeof(outgoingAddressMessage));
}

void blink(message_blink blinkMessage) {
  while (true) {
    randnum = random(NUM_ADDRESSES);
    //randnum = 0;
    Serial.print("Rand num ");
    Serial.println(randnum);
    printAddress(blinkMessage.address);
    printAddress(addressList[randnum].address);
    if (memcmp(blinkMessage.address, addressList[randnum].address, sizeof(uint8_t) * ADDRESS_SIZE) == 0) {
      memcpy(blinkMessage.address, myAddress, sizeof(uint8_t) * ADDRESS_SIZE);
      blinkMessage.color[0] = random(128);
      blinkMessage.color[1] = random(128);
      blinkMessage.color[2] = random(128);
      esp_now_send(addressList[randnum].address, (uint8_t *) &blinkMessage, sizeof(blinkMessage));
      break;
    }
    Serial.println("Didn't break");
    delay(500);

  }
  delay(steps);
  Serial.println("blink");
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
  if (i % 100 == 0) {
    Serial.print ("Step ");
    Serial.println(i);
  }
  delay(2);
  }
  // networking stuff

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

void blinkquick(int red, int green, int blue) {
    ledcWrite(ledChannelRed2, red);
  ledcWrite(ledChannelGreen2, green);
  ledcWrite(ledChannelBlue2, blue);
  ledcWrite(ledChannelRed1, red);
  ledcWrite(ledChannelGreen1, green);
  ledcWrite(ledChannelBlue1, blue);   
  delay(100);
    ledcWrite(ledChannelRed2, 0);
    ledcWrite(ledChannelGreen2, 0);
  ledcWrite(ledChannelBlue2, 0);
  ledcWrite(ledChannelRed1, 0);
  ledcWrite(ledChannelGreen1, 0);
  ledcWrite(ledChannelBlue1, 0);   

    
}

void registerAddress(uint8_t address[6]) {

  memcpy(peerInfo.peer_addr, address, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
    esp_now_get_peer_num(&numPeers);
    Serial.print("Added Address to Peer list. Total Number: ");
    Serial.println(numPeers.total_num);   

}
void receiveAddress(uint8_t address[6]) {
  Serial.print("received address ");
  printAddress(address);
  if (esp_now_get_peer(address, &testPeerInfo) == ESP_OK) {
    Serial.println("found!");
    return;
  }
  memcpy(addressList[addressCounter].address, address, sizeof(uint8_t) * ADDRESS_SIZE);
  memcpy(addressRcvd.address, outgoingAddressMessage.address, sizeof(uint8_t) * ADDRESS_SIZE);
 //blinkquick(255, 0, 0);
  registerAddress(address);
  esp_now_send(address, (uint8_t *) &addressRcvd, sizeof(addressRcvd));
  addressCounter++;
  if (addressCounter == NUM_ADDRESSES-1) {
    Serial.println("address counter full, ready to go");
    sendAddress();
    sendingLetsgo = true;
    esp_now_send(broadcastAddress, (uint8_t *) &letsgoMsg, sizeof(letsgoMsg));
  }
  
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  switch (incomingData[0]) {
    case HELLO:  
      memcpy(&incomingAddressMessage,incomingData,sizeof(incomingAddressMessage));
      receiveAddress(incomingAddressMessage.address);
      break;
    case ADDRESS_RCVD: 
      letsgo++;
      break;
    case BLINK: 
      if (blinking == false) {}
        memcpy(&blinkMessage,incomingData,sizeof(blinkMessage));
        blink(blinkMessage);
      break;
    case LETSGO:
      Serial.println("received letsgo");
      if (first == 0) {
        first = -1;
      }
      break;

    default: 
      Serial.println("Data type not recognized");
  }
}
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {
  if (sendingLetsgo == true and sendStatus == ESP_NOW_SEND_SUCCESS) {
    sendingLetsgo = false;
    if (first != -1) { 
      first = 1;
    }
    
  }
}







void ledSetup() { 
    ledcSetup(ledChannelRed1, freq, resolution);
  ledcSetup(ledChannelGreen1, freq, resolution);
  ledcSetup(ledChannelBlue1, freq, resolution);
 ledcSetup(ledChannelRed2, freq, resolution);
  ledcSetup(ledChannelGreen2, freq, resolution);
  ledcSetup(ledChannelBlue2, freq, resolution);
  ledcAttachPin(ledPinRed, ledChannelRed1);
  ledcAttachPin(ledPinGreen, ledChannelGreen1);
  ledcAttachPin(ledPinBlue, ledChannelBlue1);
  ledcAttachPin(ledPinRed2, ledChannelRed2);
  ledcAttachPin(ledPinBlue2, ledChannelBlue2);
  ledcAttachPin(ledPinGreen2, ledChannelGreen2);
  ledcWrite(ledChannelRed1, 0);
  ledcWrite(ledChannelBlue1, 0);
  ledcWrite(ledChannelGreen1, 0);
  
  ledcWrite(ledChannelRed2, 0);
  ledcWrite(ledChannelBlue2, 0);
  ledcWrite(ledChannelGreen2, 0);
}


void setup() {
 ledSetup();
  // Initialize Serial Monitor
  Serial.begin(115200);
  // start timer
  // Set device as a Wi-Fi Station
  //Networking Stuff
  WiFi.mode(WIFI_STA);
  WiFi.macAddress(outgoingAddressMessage.address);
  WiFi.macAddress(myAddress);
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
  delay(100);
}


void loop() {
//networking stuff   
Serial.print(" First = ");
Serial.println(first);
  if (letsgo<NUM_ADDRESSES-1) {
      sendAddress();
      delay(500);
  }
  else {
    if (first == 1) {
      //Serial.println("blinkign");
      blinkMessage.color[0] = random(128);
      blinkMessage.color[1] = random(128);
      blinkMessage.color[2] = random(128);
      memcpy(blinkMessage.address,addressList[0].address, sizeof(uint8_t) * ADDRESS_SIZE);
      blink(blinkMessage);
    }
    delay(2000);
  }

}