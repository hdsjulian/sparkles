#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <myDefines.h>
//#include <ESPAsyncTCP.h>
#include <queue>
#include <mutex>
#include <cstdint>
#include <helperFuncs.h>
#include <messaging.h>
#include <stateMachine.h>
// put function declarations here:

bool timerRecvd = false;
bool msgRecvd = false;
uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t myAddress[6]= {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t hostAddress[6] = {0x68, 0xb6, 0xb3, 0x08, 0xe9, 0xae};
int sent = 0;

std::mutex sendQueueMutex;
std::mutex receiveQueueMutex;
esp_now_peer_info_t peerInfo;
const char* PARAM_INPUT_1 = "input1";

messaging messageHandler;
modeMachine stateMachine;

struct ReceivedData {
    const esp_now_recv_info * mac;
    const uint8_t* incomingData;
    int len;
};

struct SendData {
  int commandId;
  int param;
};
String outputJson;

std::queue<ReceivedData> dataQueue;
std::queue<SendData> sendQueue;

message_command commandMessage;
message_status_update statusUpdateMessage;
message_address_list addressListMessage;

int msgType = 0;

bool calibrationStatus = false;
unsigned long lastDings = 0;
unsigned long lastPress = 0;
unsigned long buttonPressTime = 0;
bool buttonOn = false;
bool previousButton = false;
bool buttonPressStarted = false;
int count = 0;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {
  Serial.println("Sent");
  count = 2000;
   if (sendStatus == 0) {
      sent = 1;
    }
    else {
     sent = -1;
    }
}
void  OnDataRecv(const esp_now_recv_info * mac, const uint8_t *incomingData, int len) {
  if (stateMachine.getMode() == MODE_CALIBRATE) {    
    if (incomingData[0] == MSG_COMMANDS and incomingData[1] != CMD_END_CALIBRATION_MODE) { 
      stateMachine.switchMode(MODE_NEUTRAL);
    }
    Serial.println("recvd during calibraiton mode");
  return;
  }
  Serial.print("Received ");
  if (incomingData[0] != MSG_ANNOUNCE) {
    Serial.print("Received ");
  messageHandler.printAddress(mac->src_addr);
  messageHandler.messageCodeToText(incomingData[0]);
  }
  messageHandler.pushDataToReceivedQueue(mac, incomingData, len, micros());
}

int debugcounter = 0;
int timeadd = 0;
unsigned long debugtime = 0;

void setup() {
    Serial.begin(115200);
    #if DEVICE_USED == 2
    if (DEVICE_USED == 2) {
    ledcAttach(ledPinRed1, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttach(ledPinGreen1, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttach(ledPinBlue1, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttach(ledPinRed2, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttach(ledPinGreen2, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttach(ledPinBlue2, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledsOff();
}
    #endif
  int startTime = millis();

  while (!Serial) {
    if ((int)millis() - startTime > 10000) {
      count = 5000;
      break;
    }
    count = 1000;
  }
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  WiFi.macAddress(myAddress);
  messageHandler.setup(stateMachine, peerInfo);
  pinMode(CLAP_PIN, INPUT_PULLDOWN);
  stateMachine.switchMode(MODE_NEUTRAL);
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
    // put your setup code here, to run once:
}


void loop() {
  if (stateMachine.getMode() == MODE_CALIBRATE) {
    if (debugtime == 0) {
      debugtime = micros();
      lastDings = millis();
      Serial.println("first");
    }
    else {
      debugcounter++;
      timeadd = micros() - debugtime;
      debugtime = micros();
    }
    buttonOn = digitalRead(CLAP_PIN);
    if (buttonOn and previousButton == false and buttonPressStarted == false) {
      buttonPressTime = micros();
      buttonPressStarted = true;
      previousButton = true;
    }
    if (buttonOn and previousButton == true and micros() - buttonPressTime > 100000) {
      messageHandler.addClap(buttonPressTime);
      Serial.println("CLAP! BPT: "+String(buttonPressTime) );
      messageHandler.sendTimeThing(buttonPressTime);
      previousButton = false;
    }
    else
    if (!buttonOn and buttonPressStarted == true) {
      previousButton = false;
      buttonPressStarted = false;
    }
    if (lastDings + 1000 < millis()  and false) {
      Serial.println("Average time: "+String(timeadd/debugcounter));
      lastDings = millis();
    }
  }
  else {
  // put your main code here, to run repeatedly:
     sent = false;
    messageHandler.handleErrors();
    messageHandler.processDataFromReceivedQueue();
    messageHandler.processDataFromSendQueue();

      if (lastDings + 5000 < millis() )
      {
        Serial.print("Webserver still alive ");
        Serial.println(count);
        messageHandler.printAddress(myAddress);
        Serial.print("Free Heap: ");
        size_t freeHeap = ESP.getFreeHeap();
        Serial.print(freeHeap);
        Serial.println(" bytes");
        messageHandler.printTimerStuff(); 

        count++;
        //printAddress(myAddress);
        lastDings = millis();
        //messageHandler.sendTimeThing();
      }
  }
  
}