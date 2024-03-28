#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
//#include "ESP32TimerInterrupt.h"
//#include "driver/gptimer.h"
#define TIMER_INTERVAL_MS       1000
#define USING_TIM_DIV1 true
#define GOT_TIMER 3
#define DONE -1
#define SENDING_TIMER 1

int mode = SENDING_TIMER;
hw_timer_t * timer = NULL;
int interruptCounter;  //for counting interrupt
int totalInterruptCounter;   	//total interrupt counting

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t clientAddress[] = {0x30, 0xae, 0xa4, 0x8d, 0xcd, 0x4c};
uint8_t hostAddress[] = {0x7c, 0x87, 0xce, 0x2d, 0xcf, 0x98 };
esp_now_peer_info_t peerInfo;

//-------
//message types
//--------
struct message_timer {
  uint8_t messageType;
  uint16_t counter;
  uint16_t sendTime;
  uint16_t lastDelay;
} timerMessage;

struct message_got_timer {
  uint8_t messageType = GOT_TIMER;
  uint16_t delayAvg;
} gotTimerMessage;
//-----------
//Timer config variables
//-----------
uint32_t msgSendTime;
uint32_t msgArriveTime;
uint32_t msgReceiveTime;
int timerCounter = 0;
int lastDelay = 0;
int oldTimerCounter = 0;
int delayAvg = 0;


void printAddress(const uint8_t * mac_addr){
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);
}


void IRAM_ATTR onTimer()
{    
  if (mode == SENDING_TIMER) {
    msgSendTime = micros();
    timerCounter++;
    timerMessage.messageType = 1;
    timerMessage.sendTime = msgSendTime;
    timerMessage.counter = timerCounter;
    timerMessage.lastDelay = lastDelay;
    esp_err_t result = esp_now_send(clientAddress, (uint8_t *) &timerMessage, sizeof(timerMessage));
  }
  //return true;
}


 
void addPeer(uint8_t * address) {
  memcpy(&peerInfo.peer_addr, address, 6);
  if (esp_now_get_peer(peerInfo.peer_addr, &peerInfo) == ESP_OK) {
    Serial.println("Found Peer");
    return;
  }
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
    // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  else {
    Serial.println("Added Peer");
  }
}

void removePeer(uint8_t address[6]) {
  if (esp_now_del_peer(address) != ESP_OK) {
    Serial.println("coudln't delete peer");
    return;
  }
}

void  OnDataRecv(const esp_now_recv_info * mac, const uint8_t *incomingData, int len) {
    //how to actually store the clap data?
  if (incomingData[0] == GOT_TIMER) {
    memcpy(&gotTimerMessage, incomingData, sizeof(gotTimerMessage));
    mode = DONE;
    delayAvg = gotTimerMessage.delayAvg;
  }
  msgReceiveTime = micros();
  }




void  OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {
  //das geht natürlich nur wenn der richtige message status 
  if (sendStatus == ESP_NOW_SEND_SUCCESS) {
    msgArriveTime = micros();
    lastDelay = msgArriveTime-msgSendTime;
  }
  else {
    msgArriveTime = 0;
  }
}


void setup() {
  Serial.begin(115200);

  timer = timerBegin(1000000);           	// timer 0, prescalar: 80, UP counting
  timerAttachInterrupt(timer, &onTimer); 	// Attach interrupt
  timerWrite(timer, 0);  		// Match value= 1000000 for 1 sec. delay.
  timerStart(timer);   
  timerAlarm(timer, 1000000, true, 0);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  //esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
    // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);  
  addPeer(clientAddress);
  //esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  timerCounter = 0;
}

void loop() {
//esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &announceMessage, sizeof(announceMessage));
if (mode == SENDING_TIMER) {
  if (oldTimerCounter < timerCounter) {
    oldTimerCounter = timerCounter;
    Serial.print("Timer Counter ");
    Serial.println(timerCounter);
    Serial.print("message send time");
    Serial.println(msgSendTime);
    Serial.print("Message arrive time ");
    Serial.println(msgArriveTime);
    Serial.print("last delay ");
    Serial.println(lastDelay);

  }
}
else if (mode == DONE) {
  Serial.print("delay average ");
  Serial.println(delayAvg);
}
Serial.println(".");
delay(500);
 /* else if (mode == ASK_CLAP_TIME) {
    //make sure claps actually correspond, in case one got lost or so?
    for (int i =0; i <= addressCounter; i++) {
      if (clientAddresses[i].address == emptyAddress) {
        mode = ANIMATE;
        break;
      }
      else { 
        for (int j = 0; j < clapCounter; j++) {;
          sendClap.clapIndex = claps[j].timerCounter;
          memcpy(&sendClap.address, clientAddresses[i].address, sizeof(clientAddresses[i].address));
          esp_now_send(clientAddresses[i].address, (uint8_t *) &sendClap, sizeof(sendClap) );
        }
      }
    }
  }*/

}

