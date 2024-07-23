#define V1 1
#define V2 2 
#define D1 3
#ifndef DEVICE_USED
#define DEVICE_USED V2
#endif
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <PeakDetection.h> 
#include <ledHandler.h>
#include <stateMachine.h>
#include <messaging.h>
#include <LittleFS.h>
#include <webserver.h>
#define SWITCH_PIN 48
//#include "ESP32TimerInterrupt.h"
//#include "driver/gptimer.h"
#define TIMER_INTERVAL_MS       600
#define USING_TIM_DIV1 true

        uint8_t myAddress[6];

int mode;
//#define NUM_DEVICES 180
hw_timer_t * timer = NULL;
PeakDetection peakDetection; 
ledHandler handleLed;
modeMachine modeHandler;
messaging messageHandler;
message_send_clap_times clapTimes;
webserver myWebserver(&LittleFS);

int interruptCounter;  //for counting interrupt
int totalInterruptCounter;   	//total interrupt counting
bool start = true;
bool lfs_started = true;
esp_now_peer_info_t peerInfo;

int audioPin = 5;
int cycleCounter = 0;

bool isSetup = false;
bool isTimerSet = false;



//-----------
//Timer config variables
//-----------
unsigned long msgSendTime;
uint32_t msgArriveTime;
uint32_t msgReceiveTime;
int lastDelay = 0;
int delayAvg = 0;


//calibration
int sensorValue;
int lastClap = 0;
int lastTick = 0;
int clapStop = 0;
uint32_t lastClapTime;

//receive addresses
//client_address clientAddresses[NUM_DEVICES];




void IRAM_ATTR onTimer()
{   
 msgSendTime = micros();
 if (modeHandler.getMode() != MODE_SENDING_TIMER and modeHandler.getMode() != MODE_RESET_TIMER and modeHandler.getMode() != MODE_BROADCAST_TIMER && modeHandler.getMode() != MODE_PRE_CALIBRATION_BROADCAST) {
  return;
 }
  //Serial.println(msgSendTime/1000);
  messageHandler.incrementTimerCounter();
    //wait for timer vs wait for calibrate
    if (messageHandler.getTimerCounter() == 100) {
      //messageHandler.timeoutRetryHandler();
      //what to do here?
      messageHandler.setNoSuccess();
      messageHandler.setTimerCounter(0);
    }    



    if (modeHandler.getMode() == MODE_RESET_TIMER) {
        messageHandler.timerMessage.reset = true;
    }
      messageHandler.timerMessage.sendTime = msgSendTime;
      messageHandler.timerMessage.counter = messageHandler.getTimerCounter();
      messageHandler.timerMessage.lastDelay = lastDelay;
    if (modeHandler.getMode() == MODE_SENDING_TIMER or modeHandler.getMode() == MODE_RESET_TIMER) {
      messageHandler.timerMessage.messageType = MSG_TIMER_CALIBRATION; 
     esp_err_t result = esp_now_send(messageHandler.timerReceiver, (uint8_t *) &messageHandler.timerMessage, sizeof(messageHandler.timerMessage));

    }
    if (modeHandler.getMode() == MODE_BROADCAST_TIMER) {
      messageHandler.timerMessage.messageType = MSG_BROADCAST_TIMER;
      esp_err_t result = esp_now_send(messageHandler.broadcastAddress, (uint8_t *) &messageHandler.timerMessage, sizeof(messageHandler.timerMessage));

    }
      //messageHandler.addError("Sending Timer+ "+String(timerCounter)+"\n");

      //messageHandler.addError(messageHandler.stringAddress(messageHandler.timerReceiver)+"\n");
    

}


void  OnDataRecv(const esp_now_recv_info * mac, const uint8_t *incomingData, int len) {
    if (incomingData[0] == MSG_TIMESYNC) {
        messageHandler.addError("arriveTime: "+String(micros()), false);
    }
  uint8_t senderAddress[6];
  memcpy(senderAddress, mac->src_addr, 6);
  messageHandler.addError("rcvd "+messageHandler.messageCodeToText(incomingData[0])+" from "+messageHandler.stringAddress(mac->src_addr)+"\n");
  msgReceiveTime = micros();
  messageHandler.pushDataToReceivedQueue(senderAddress, incomingData, len, msgReceiveTime);
}




void  OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {
  if (modeHandler.getMode() == MODE_SENDING_TIMER or modeHandler.getMode() == MODE_RESET_TIMER) {
    if (sendStatus == ESP_NOW_SEND_SUCCESS) {
      msgArriveTime = micros();
      lastDelay = msgArriveTime-msgSendTime;
      messageHandler.addError("Sent to  "+messageHandler.stringAddress(mac_addr)+"\n");
      messageHandler.addError("ID "+String(messageHandler.getTimeoutRetryId())+"\n");
      messageHandler.addError("SUCCESS "+String(lastDelay)+"\n");
      
    }
    else if (messageHandler.getTimerCounter() % 5 == 0) {
      messageHandler.addError("No SUCCESS");
      messageHandler.addError("tried to send to "+messageHandler.stringAddress(mac_addr)+"\n");
     //messageHandler.setNoSuccess();
    }
  }
  else{
     if (sendStatus == ESP_NOW_SEND_SUCCESS) {
       messageHandler.addError("Se nt to  "+messageHandler.stringAddress(mac_addr)+"\n");
     }
     else {
       messageHandler.addError("Not sent to "+messageHandler.stringAddress(mac_addr)+"\n");
     }

  }
}


void setup() {
  Serial.begin(115200);
  unsigned long startTime = millis();
  while (!Serial) {
    if (millis() - startTime > 3000) {
      break;
    }
  }
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    lfs_started = false;
  }
    myWebserver.setup(messageHandler, modeHandler);

  timer = timerBegin(1000000);           	// timer 0, prescalar: 80, UP counting
  //timerAttachInterrupt(timer, &onTimer); 	// Attach interrupt

  WiFi.mode(WIFI_AP_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  handleLed.setup();
  
  messageHandler.setup(modeHandler, handleLed, peerInfo, myWebserver);
  modeHandler.setup(myWebserver);
  modeHandler.switchMode(MODE_NEUTRAL);
  Serial.println("after setup");
  //esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  memcpy(&peerInfo.peer_addr, messageHandler.broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
    // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);  
  WiFi.macAddress(messageHandler.announceMessage.address);
  if (DEVICE_USED == D1) {
    audioPin = 35; 
  }
  pinMode(audioPin, INPUT); 
  lastTick = millis(); 
  WiFi.macAddress(myAddress);
  pinMode(SWITCH_PIN, INPUT_PULLDOWN); 
  peakDetection.begin(48, 10, 0.5);   
  //randomSeed(analogRead(33));
  myWebserver.PdParamsChanged = true;
}
double data;
void loop() {
  if (false) {
    messageHandler.testTrilateration();
  }
  else {
  if (modeHandler.getMode() == MODE_SENDING_TIMER || modeHandler.getMode() == MODE_RESET_TIMER || modeHandler.getMode() == MODE_BROADCAST_TIMER || modeHandler.getMode() == MODE_PRE_CALIBRATION_BROADCAST) {
    if (isTimerSet == false ){
      Serial.println("attaching interrupt");
      timerAttachInterrupt(timer, &onTimer); 
      timerWrite(timer, 0);  		// Match value= 1000000 for 1 sec. delay.
      timerStart(timer);   
      timerAlarm(timer, TIMER_INTERVAL_MS*1000, true, 0);

      isTimerSet = true;
    }
  } 
   else {
      if (isTimerSet == true ) {
        timerDetachInterrupt(timer);
        isTimerSet = false;
      }
    }

  messageHandler.processDataFromReceivedQueue();
  messageHandler.processDataFromSendQueue();
  if (digitalRead(SWITCH_PIN) == HIGH) {
    Serial.println("switch pin high");
    messageHandler.deleteFile("/clientAddress");
    delay(5000);
    ESP.restart();
  }
  // testing peakdetection params
  if (false) {
    if (myWebserver.PdParamsChanged == true) {
      Serial.println("params changed");
      myWebserver.PdParamsChanged = false;
      peakDetection.begin(myWebserver.lag, myWebserver.threshold, myWebserver.influence);
      peakDetection.printParams();
    }

     data = (double)analogRead(audioPin)/2048-1;
    peakDetection.add(data); 
    int peak = peakDetection.getPeak(); 
    double filtered = peakDetection.getFilt(); 
    //Serial.println(sensorValue);
    if (peak == -1 and millis() > lastClap+1000) {
      Serial.println("Happened "+String(peakDetection.ago)+" microseconds ago");
      Serial.println("avg at clap "+String(peakDetection.avgatclap));
      Serial.println("data: "+String(data));
      lastClap = millis();
      handleLed.flash(125, 0, 55, 150, 1, 50);
    }
  }

  if (modeHandler.getMode() == MODE_CALIBRATE) {
    modeHandler.switchMode(MODE_CLAPPING);
  }
  if (modeHandler.getMode() == MODE_CLAPPING or modeHandler.getMode() == MODE_MASTERCLAP_OCCURRED) {
     double data = (double)analogRead(audioPin)/2048-1;
    peakDetection.add(data); 
    int peak = peakDetection.getPeak(); 
    double filtered = peakDetection.getFilt(); 
    //Serial.println(sensorValue);
    if (peak == -1 and millis() > lastClap+1000) {
      messageHandler.addClap(micros());
      Serial.println("Happened "+String(peakDetection.ago)+" microseconds ago");
      Serial.println("avg at clap "+String(peakDetection.avgatclap));
      Serial.println("data: "+String(data));
      Serial.println("Mode "+String(modeHandler.getMode()));
      lastClap = millis();
      handleLed.flash(125, 0, 55, 150, 1, 50);
      if (modeHandler.getMode() == MODE_MASTERCLAP_OCCURRED) {
        modeHandler.switchMode(MODE_NEUTRAL);
        lastClap = 0;
        messageHandler.handleSingleClap();
      }
    }
  } 
  else if (lastTick+5000 < millis() ) {
    messageHandler.handleErrors();
    lastTick = millis();
    modeHandler.printCurrentMode();
    Serial.print("Prev mode: ");
    modeHandler.printMode(modeHandler.getPreviousMode());

    cycleCounter++;
    Serial.print("-----\nMain still Alive ");
    Serial.println(cycleCounter);
    messageHandler.printAddress(myAddress);
    //messageHandler.printPeers();
    Serial.println("timerCounter "+String(messageHandler.getTimerCounter()));
  }

  if (messageHandler.clapsReceived == messageHandler.addressCounter && messageHandler.clapsReceived != 0) {
    Serial.println("All clap Times received");
    //messageHandler.clapsReceived = 0;
  }
  if (modeHandler.getMode() == MODE_WOKEUP) {
    Serial.println("registering cb functions");
    esp_now_register_recv_cb(OnDataRecv);  
    esp_now_register_send_cb(OnDataSent);
    modeHandler.setPreviousMode();
    modeHandler.switchMode(MODE_NEUTRAL);
    messageHandler.nextAnimationPing = 0;
      memcpy(&peerInfo.peer_addr, messageHandler.broadcastAddress, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
      // Add peer        
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      Serial.println("Failed to add peer broadcastaddress 2");
      return;
    }
  }
  if (modeHandler.getMode() != MODE_SENDING_TIMER && modeHandler.getMode() != MODE_RESET_TIMER && modeHandler.getMode() != MODE_PING_RESET) {
    messageHandler.handleTimerUpdates();
  }
  if (messageHandler.nextAnimationPing > 0) {
    messageHandler.nextAnimation();
  }
  if (modeHandler.getMode() == MODE_RESET_TIMER || modeHandler.getMode() == MODE_GET_CALIBRATION_DATA) {
     messageHandler.nextRetry();
  }
  messageHandler.goodNight();
  if ((modeHandler.getMode() == MODE_NEUTRAL && millis()> messageHandler.getLastBroadcastTimer()+ BROADCAST_TIMER_FREQ*1000) || modeHandler.getMode() == MODE_START_PRE_CALIBRATION_BROADCAST) {
    Serial.println("modecheck 4");
    Serial.println("Broadcasting Timer");
    Serial.println("Millis "+String(millis()));
    Serial.println("BroadcastTimer Frequency "+String(BROADCAST_TIMER_FREQ));
    Serial.println("Last Broadcast Timer "+String(messageHandler.getLastBroadcastTimer()));
    Serial.println("millis() - BROADCAST_TIMER_FREQ*1000"+String(millis() - BROADCAST_TIMER_FREQ*1000));
    modeHandler.setPreviousMode(true);
    messageHandler.broadcastTimer();
    messageHandler.setLastBroadcastTimer();
  }
  if (modeHandler.getMode() == MODE_CALIBRATION_WAITING) {
    messageHandler.startCalibrationMode();
  }
  if (modeHandler.getMode() == MODE_STARTUP_ANIMATION) {
    messageHandler.startAnimation();
  }
  if (modeHandler.getMode() == MODE_END_ANIMATION)  {
    messageHandler.endAnimation();
  }
  if (modeHandler.getMode() == MODE_ANIMATE) {
    messageHandler.nextAnimation();
  }
  if (modeHandler.getMode() == MODE_WOKEUP && messageHandler.allTimersUpdated()) {
  Serial.println ("Woke up and all timers updated");
    messageHandler.startAnimation();
  }
  if (modeHandler.getMode() == MODE_INIT) {
    modeHandler.switchMode(MODE_NEUTRAL);
  }
  if (messageHandler.getTimerCounter() == MAX_BROADCAST_TIMERS && modeHandler.getMode() == MODE_BROADCAST_TIMER) {
       modeHandler.revertToPreviousMode();
      messageHandler.setTimerCounter(0);
  }
  if (messageHandler.getTimerCounter() == MAX_BROADCAST_TIMERS && modeHandler.getMode() == MODE_PRE_CALIBRATION_BROADCAST) {
      modeHandler.switchMode(MODE_CALIBRATION_WAITING);
      messageHandler.setTimerCounter(0);
  }

} 
}