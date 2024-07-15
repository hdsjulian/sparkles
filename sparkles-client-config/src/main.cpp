#define V1 1
#define V2 2
#define D1 3
#define DEVICE V2

#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"
#include "PeakDetection.h"
#include "Preferences.h"


//#include "ESP32TimerInterrupt.h"
#define TIMER_INTERVAL_MS       600
#define USING_TIM_DIV1 true
#include "messaging.h"
#include "ledHandler.h"
#include "stateMachine.h"

#define CALIBRATION_FREQUENCY 1000

int freq = 5000;
int resolution = 8;
bool test = true;
int audioPin = 5;
float redfloat = 0, greenfloat = 0, bluefloat = 0;
uint32_t timerDings;

float rgb[3];

bool gotTimer = false;
PeakDetection peakDetection; 
ledHandler handleLed;
modeMachine modeHandler;
messaging messageHandler;
Preferences preferences;


//Network
//uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t hostAddress[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t myAddress[6];
esp_now_peer_info_t peerInfo;
esp_now_peer_num_t peerNum;

//Variables for Time Offset
/*
message_animate animationMessage;
message_clap_time clapTime;
message_address addressMessage;
message_timer timerMessage;
message_got_timer gotTimerMessage;
message_announce announceMessage;
message_mode modeMessage;
message_timer_received timerReceivedMessage;
*/



//timer stuff
uint32_t timeOffset;
uint32_t lastTime = 0;
uint32_t msgSendTime;
uint32_t msgArriveTime;
uint32_t msgReceiveTime;
int timerCounter = 0;
int lastDelay = 0;
int oldTimerCounter = 0;
int timerArray[TIMER_ARRAY_COUNT];
int arrayCounter = 0;
int delayAvg = 0;
//general setup



//calibration stuff
int sensorValue;
int microphonePin = A0;
int lastClap;
int lastTick;
int clapStop = 0;

int lastFlash;
bool clapSent = false;

//address sending
int addressReceived = false;
int addressSending = 0;


//timer stuff
//ESP32Timer ITimer(0);


int cycleCounter = 0;
/*

void receiveTimer(int messageArriveTime) {
  //add condition that if nothing happened after 5 seconds, situation goes back to start
  //wenn die letzte message maximal 300 mikrosekunden abweicht und der letzte delay auch nicht mehr als 1500ms her war, dann muss die msg korrekt sein
  int difference = messageArriveTime - lastTime ;
  lastDelay = timerMessage.lastDelay;

  if (abs(difference-CALIBRATION_FREQUENCY*TIMER_INTERVAL_MS) < 500 and abs(timerMessage.lastDelay) <2500) {
    if (arrayCounter <TIMER_ARRAY_COUNT) {
      timerArray[arrayCounter] = timerMessage.lastDelay;
    }
    else {
      for (int i = 0; i< TIMER_ARRAY_COUNT; i++) {
        delayAvg += timerArray[i];
      } 
      delayAvg = delayAvg/TIMER_ARRAY_COUNT;
      gotTimerMessage.delayAvg = delayAvg;
      timeOffset = messageArriveTime-timerMessage.sendTime-delayAvg/2;
      //TODO: Make sure the message actually arrives.
      Serial.print("sending GOT_TIMER to ");
      messageHandler.printAddress(hostAddress);

      esp_now_send(hostAddress, (uint8_t *) &gotTimerMessage, sizeof(gotTimerMessage));
      Serial.print("delay avg ");
      Serial.println(delayAvg);
      Serial.println("Should Flash");
      handleLed.flash(255,0,0, 200, 3, 300);
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
*/

int testingMode = 0;
int count = 0;
bool didIreset = true;


void detectClaps(void *pvParameters) {
    peakDetection.begin(48, 9, 0.6);   
    while (1) {
      double data = (double)analogRead(audioPin)/2048-1;
      peakDetection.add(data); 
      int peak = peakDetection.getPeak(); 
      double filtered = peakDetection.getFilt(); 
      if (peak == -1 and millis() > lastClap+1000) {
        messageHandler.addClap(micros());
        lastClap = millis();
        if (modeHandler.getMode() == MODE_MASTERCLAP_OCCURRED) {
          modeHandler.switchMode(MODE_NEUTRAL);
          lastClap = 0;
          vTaskDelete(NULL);
        }
    }
  }
}

void OnDataRecv(const esp_now_recv_info * mac, const uint8_t *incomingData, int len) {
    msgReceiveTime = micros();
    if (incomingData[0] != MSG_ANNOUNCE) {
    messageHandler.addError("RECEIVED MESSAGE "+messageHandler.messageCodeToText(incomingData[0])+" from "+messageHandler.stringAddress(mac->src_addr)+"\n");
    if (incomingData[0] == MSG_COMMANDS) {
      messageHandler.addError("RECEIVED MESSAGE "+messageHandler.messageCodeToText(incomingData[1])+" from "+messageHandler.stringAddress(mac->src_addr)+"\n");
    }
    } 
  uint8_t senderAddress[6];
  memcpy(senderAddress, mac->src_addr, 6);
  messageHandler.pushDataToReceivedQueue(senderAddress, incomingData, len, msgReceiveTime);

}


void  OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {
  count++;
  if (messageHandler.getMessagingMode() == MODE_WAIT_ANNOUNCE_RESPONCE) {
    if (sendStatus == ESP_NOW_SEND_SUCCESS) {
      messageHandler.setMessagingMode(MODE_NO_SEND);
      modeHandler.switchMode(MODE_WAIT_FOR_TIMER);
    }
    else {
      messageHandler.setMessagingMode(MODE_RESPOND_ANNOUNCE);
    }
  }
  else if (messageHandler.getMessagingMode() == MODE_WAIT_TIMER_RESPONSE) {
    if (sendStatus == ESP_NOW_SEND_SUCCESS) {
      modeHandler.switchMode(MODE_ANIMATE);
      messageHandler.setMessagingMode(MODE_NO_SEND);
    }
    else {
      messageHandler.setMessagingMode(MODE_RESPOND_TIMER);
    }
  }
  else {
    modeHandler.printCurrentMode();
  }

  //remove peer if conditions are met
}



void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  //WiFi.macAddress(addressMessage.address);

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

  handleLed.setup();
  messageHandler.setup(modeHandler, handleLed, peerInfo);
  modeHandler.switchMode(MODE_STARTUP);
  esp_now_register_recv_cb(OnDataRecv);  
  esp_now_register_send_cb(OnDataSent);
   WiFi.macAddress(messageHandler.addressMessage.address);
   esp_now_get_peer_num(&peerNum);
  Serial.print("Number of Peers start: ");
  Serial.println(peerNum.total_num);
   Serial.println("huch");
  pinMode(audioPin, INPUT); 
  delay(1000);
  timerDings = micros();
  lastFlash = 0;
  WiFi.macAddress(myAddress);
    PeakDetection peakDetection; 

}

void loop() {
  if (modeHandler.getMode() == MODE_STARTUP and messageHandler.gotTimer == false and millis() > 3000*(messageHandler.announceCounter*messageHandler.announceCounter)+messageHandler.announceTime) {
    messageHandler.announceAddress();
  }
  
  if (testingMode == 0) {

  messageHandler.processDataFromSendQueue();
  messageHandler.processDataFromReceivedQueue();
  messageHandler.handleErrors();
  messageHandler.handleSent();
  handleLed.run();
  if (modeHandler.getMode() == MODE_CALIBRATE && lastClap == 0) 
  {
    lastClap = millis();
    xTaskCreate(detectClaps, "detectClaps", 10000, NULL, 6, NULL);
  }

  else if (millis()>(lastTick+5000)) 
  {
        //handleLed.flash(0, 255, 0, 200, 1, 50);

    Serial.println("Client still alive");
    modeHandler.printCurrentMode();
    messageHandler.printAddress(myAddress);
    lastTick = millis();
    Serial.println(messageHandler.getMessageLog());
    Serial.println("-----");
    //messageHandler.sendTimeSync();
  }

  }
  else if (millis()>(lastTick+10000)) 
  {
  if (didIreset == true) {
    Serial.println("I RESETTED. WTF");
    didIreset = false;
  }
    //handleLed.flash(255, 0, 0, 200, 1, 50);
    //modeHandler.printLog();
    lastTick = millis();
    cycleCounter++;
    Serial.println("-----\nClient Still Alive");
    Serial.println(cycleCounter);
    //Serial.println(messageHandler.getMessageLog());
    modeHandler.printCurrentMode();
    messageHandler.printAddress(myAddress);
    handleLed.printStatus();

  }
  else {
    handleLed.ledOn(255, 0, 0, 10000, true);
  handleLed.ledOn(0, 255, 0, 10000, true);
    handleLed.ledOn(0, 0, 255, 10000, true);

  }
  if (modeHandler.getMode() == MODE_ANIMATE) {
    messageHandler.nextAnimation();
  }



} 