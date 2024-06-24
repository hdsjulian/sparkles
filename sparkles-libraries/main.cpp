#define V1 1
#define V2 2
#define D1 3
#define DEVICE DEVICE_USED

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <PeakDetection.h> 
#include <ledHandler.h>
//#include <../../sparkles-main-config/src/messaging.h>
//#include <../../sparkles-main-config/src/stateMachine.h>

#define CALIBRATION_FREQUENCY 1000
int freq = 5000;
int resolution = 8;


bool test = true;
int audioPin = 5;

PeakDetection peakDetection; 
ledHandler handleLed;
//calibration stuff
int sensorValue;
int microphonePin = A0;
int clapCounter = 0;
int lastClap;
bool clapSent = false;







void modeSwitch(int switchMode) {
  Serial.print("Switched mode to ");
  printMode(switchMode);
  mode = switchMode;
}



void OnDataRecv(const esp_now_recv_info * mac, const uint8_t *incomingData, int len) {
  Serial.print("received ");
  printMessage(incomingData[0]);
  msgReceiveTime = micros();
  memcpy(&hostAddress, mac->src_addr, sizeof(hostAddress));
  switch (incomingData[0]) {
    case MSG_ANNOUNCE: 
    {
      if (mode == MODE_WAIT_FOR_ANNOUNCE) {
        if (addPeer(hostAddress) == 1) {
          Serial.println("Adding peer ");
          flash(0, 125, 125, 200, 1, 50);
          memcpy(&announceMessage, incomingData, sizeof(announceMessage));
          esp_now_send(hostAddress, (uint8_t*) &addressMessage, sizeof(addressMessage));
          //modeSwitch(MODE_WAIT_FOR_TIMER);
        }
      }
      
    }
      break;
    case MSG_TIMER_CALIBRATION:  
    { 
      addPeer(hostAddress);
      memcpy(&timerMessage,incomingData,sizeof(timerMessage));
      flash(125, 0, 0 , 200, 1, 50);
      receiveTimer(msgReceiveTime);
    }
      break;
    case MSG_ANIMATION:
    Serial.println("animation message incoming");
    if (mode == MODE_ANIMATE) {
      Serial.println("Calling Blink");
      memcpy(&animationMessage, incomingData, sizeof(animationMessage));
      Serial.print("Animation Message speed");
      Serial.println(animationMessage.speed);
      candle(animationMessage.speed, animationMessage.reps, animationMessage.delay);
    }
      break;
    default: 
      Serial.println("Data type not recognized");
  }
  
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {
  //turn into MODE WAIT FOR TIMER if message to host is saved.
  Serial.println("sent message to");
  printAddress(mac_addr);
  printMode(mode);
  if (mode == MODE_WAIT_FOR_ANNOUNCE) {
    if (sendStatus == ESP_NOW_SEND_SUCCESS) {
      modeSwitch(MODE_WAIT_FOR_TIMER);
    }
  }
  else if (mode == MODE_WAIT_FOR_TIMER) {
    Serial.println("should send");
    if (sendStatus == ESP_NOW_SEND_SUCCESS) {
      modeSwitch(MODE_ANIMATE);
    }
  }
    else {
      //esp_now_send(broadcastAddress,(uint8_t *) &timerReceivedMessage, sizeof(timerReceivedMessage) );
    }
}





void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  WiFi.macAddress(messageHandler.myAddress);

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
  modeSwitch(MODE_WAIT_FOR_ANNOUNCE);
  esp_now_register_recv_cb(OnDataRecv);  
  esp_now_register_send_cb(OnDataSent);
   WiFi.macAddress(addressMessage.address);
   esp_now_get_peer_num(&peerNum);
  
  Serial.print("Number of Peers start: ");
  Serial.println(peerNum.total_num);
  if (DEVICE != D1){

  
  ledsOff();
  }
  else {
    audioPin = 35;
  }
 Serial.println("huch");
  pinMode(audioPin, INPUT); 
  peakDetection.begin(30, 3, 0);   
  delay(1000);
    clapTime.clapCounter = 0;
  flash(0, 255, 0, 200, 2, 50);
}

void loop() {
if (mode == MODE_ANIMATE) {
  //flash(0, 255, 0, 200, 2, 50);

 
  double data = (double)analogRead(audioPin)/512-1;
  peakDetection.add(data); 
  int peak = peakDetection.getPeak(); 
  double filtered = peakDetection.getFilt(); 
  //Serial.println(sensorValue);
  if (peak == -1 and millis() > lastClap+1000) {
    clapTime.clapCounter++;
    clapTime.timeStamp = micros()-timeOffset;
    Serial.print("Clap Time ") ;
    Serial.println(clapTime.timeStamp);
    Serial.print("Clap Counter ");
    Serial.println(clapTime.clapCounter);
    esp_now_send(hostAddress, (uint8_t *) &clapTime, sizeof(clapTime));
    flash();
    //esp_now_send(broadcastAddress, (uint8_t *) &blinkMessage, sizeof(blinkMessage));
    lastClap = millis();
  }
  else if (millis()>(lastClap+5000)) 
  {
    Serial.println("Still alive");
    lastClap = millis();
  }

}
  else if (millis()>(lastClap+5000)) 
  {
    Serial.println("Still alive");
    lastClap = millis();
  }

/*    sensorValue = analogRead(microphonePin);
    if (sensorValue < 50 and millis() > lastClap+1000) {
      blinkMessage.color[0] = random(128);
       blinkMessage.color[1] = random(128);
       blinkMessage.color[2] = random(128);
       esp_now_send(broadcastAddress, (uint8_t *) &blinkMessage, sizeof(blinkMessage));
      lastClap = millis();
      clapCounter++;
      
    }
  }*/


    //printAddress(addressMessage.address);


} 