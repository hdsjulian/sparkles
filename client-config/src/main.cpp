#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
//#include "ESP32TimerInterrupt.h"
//#include "driver/gptimer.h"
#define TIMER_INTERVAL_MS       1000
#define USING_TIM_DIV1 true
#define GOT_TIMER 3
hw_timer_t * timer = NULL;
int interruptCounter;  //for counting interrupt
int totalInterruptCounter;   	//total interrupt counting
#define TIMER_ARRAY_COUNT 10
int timerArray[TIMER_ARRAY_COUNT];
int arrayCounter = 0;
int delayAvg = 0;

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t clientAddress[] = {0x30, 0xae, 0xa4, 0x8d, 0xcd, 0x4c};
uint8_t hostAddress[] = {0x7c, 0x87, 0xce, 0x2d, 0xcf, 0x98 };
esp_now_peer_info_t peerInfo;
uint32_t lastTime;
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
int blub;

uint32_t msgSendTime;
uint32_t msgArriveTime;
uint32_t msgReceiveTime;
int timerCounter = 0;
int lastDelay = 0;
int oldTimerCounter = 0;


void printAddress(const uint8_t * mac_addr){
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);
}


void IRAM_ATTR onTimer()
{   
    msgSendTime = micros();
    timerCounter++;
    timerMessage.messageType = 1;
    timerMessage.sendTime = msgSendTime;
    timerMessage.counter = timerCounter;
    timerMessage.lastDelay = lastDelay;
    esp_err_t result = esp_now_send(clientAddress, (uint8_t *) &timerMessage, sizeof(timerMessage));
  //return true;
}


void receiveTimer(int messageArriveTime) {
  //wenn die letzte message maximal 300 mikrosekunden abweicht und der letzte delay auch nicht mehr als 1500ms her war, dann muss die msg korrekt sein
  int difference = messageArriveTime - lastTime;
  lastDelay = timerMessage.lastDelay;

  if (abs(difference-1000000) < 300 and abs(timerMessage.lastDelay) <2500) {
    if (arrayCounter <TIMER_ARRAY_COUNT) {

      timerArray[arrayCounter] = timerMessage.lastDelay;
    }
    else {
      for (int i = 0; i< TIMER_ARRAY_COUNT; i++) {
        delayAvg += timerArray[i];
      } 
      delayAvg = delayAvg/TIMER_ARRAY_COUNT;
      gotTimerMessage.delayAvg = delayAvg;
      esp_now_send(hostAddress, (uint8_t *) &gotTimerMessage, sizeof(gotTimerMessage) );
    }
    //damit hab ich den zeitoffset.. 
    // if zeit - timeoffset % 1000 = 0: blink
    //timeOffset = lastTime-oldMessage.sendTime;
    //    Serial.println(esp_now_send(broadcastAddress,(uint8_t *) &timerReceivedMessage, sizeof(timerReceivedMessage) ));
    arrayCounter++;
  }
  else {
    lastTime = messageArriveTime;
  }
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
    Serial.println("rcvd");
    timerCounter++;
    memcpy(&timerMessage,incomingData,sizeof(timerMessage));
  //esp_now_send(hostAddress, (uint8_t *) &timerMessage, sizeof(timerMessage));
    receiveTimer(micros());

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
/*
  timer = timerBegin(1000000);           	// timer 0, prescalar: 80, UP counting
  timerAttachInterrupt(timer, &onTimer); 	// Attach interrupt
  timerWrite(timer, 0);  		// Match value= 1000000 for 1 sec. delay.
  timerStart(timer);   
  timerAlarm(timer, 5000000, true, 0);
*/
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
  //esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  addPeer(hostAddress);
  timerCounter = 0;
}

void loop() {
//esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &announceMessage, sizeof(announceMessage));

if (delayAvg > 0) {
  Serial.print("Delay Average");
  Serial.println(delayAvg);
}
Serial.println(".");
delay(1000);
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

