#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <FS.h>
#include "ESPAsyncWebServer.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <myDefines.h>
//#include <ESPAsyncTCP.h>
#include <queue>
#include <mutex>
#include <cstdint>
#include <helperFuncs.h>
#include <webserver.h>
#include <messaging.h>
#include <stateMachine.h>
// put function declarations here:

bool timerRecvd = false;
bool msgRecvd = false;
uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t myAddress[6]= {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t hostAddress[6] = {0x68, 0xb6, 0xb3, 0x08, 0xe9, 0xae};
int sent = 0;
int msgCounter = 0;

std::mutex sendQueueMutex;
std::mutex receiveQueueMutex;
const char* ssid = "Spargels";
const char* password = "sparkles";
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
int deviceId = -1;

bool calibrationStatus = false;
unsigned long lastDings = 0;
unsigned long lastPress = 0;
unsigned long buttonPressTime = 0;
bool buttonOn = false;
bool previousButton = false;
bool buttonPressStarted = false;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {
  Serial.println("Sent");
   if (sendStatus == 0) {
      sent = 1;
    }
    else {
     sent = -1;
    }
}
void  OnDataRecv(const esp_now_recv_info * mac, const uint8_t *incomingData, int len) {
  
  if (incomingData[0] != MSG_ANNOUNCE) {
    Serial.print("Received ");
  messageHandler.printAddress(mac->src_addr);
  messageHandler.messageCodeToText(incomingData[0]);
  }
  messageHandler.pushDataToReceivedQueue(mac, incomingData, len, micros());
}


bool connected = false;
int count = 0;
int something = 1;

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
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS");
    return;
  }
 

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid, password);
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  myWebserver.setup(messageHandler, stateMachine);
  messageHandler.setup(myWebserver, stateMachine, peerInfo);
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());
  pinMode(CLAP_PIN, INPUT_PULLDOWN);
  stateMachine.switchMode(MODE_NEUTRAL);

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  messageHandler.setHostAddress(hostAddress);
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  WiFi.macAddress(myAddress);
    // put your setup code here, to run once:
}


void loop() {
  sent = false;
  messageHandler.handleErrors();
  messageHandler.processDataFromReceivedQueue();
  messageHandler.processDataFromSendQueue();
  if (stateMachine.getMode() == MODE_NEUTRAL) {
    if (something == 1) {
      messageHandler.addPeer(hostAddress);
      something = -1;
    }
    else if (something == 0){
      Serial.println("something wrong");
      something = -1;
    }
    
  }
  if (stateMachine.getMode() == MODE_CALIBRATE) {
    buttonOn = digitalRead(CLAP_PIN);
    if (buttonOn and previousButton == false and buttonPressStarted == false) {
      buttonPressTime = micros();
      buttonPressStarted = true;
      previousButton = true;
    }
    if (buttonOn and previousButton == true and micros() - buttonPressTime > 100000) {
      messageHandler.addClap(buttonPressTime);
      Serial.println("CLAP!");
      previousButton = false;
    }
    else
    if (!buttonOn and buttonPressStarted == true) {
      previousButton = false;
      buttonPressStarted = false;
    }
  }
  else {
  // put your main code here, to run repeatedly:
    if (outputJson != "")  {
      Serial.print ("JSON: \n");
      Serial.println(outputJson);
      outputJson = "";
    }
  }
      if (lastDings + 5000 < millis() )
      {
        Serial.print("Webserver still alive ");
        Serial.println(count);
        Serial.println(WiFi.softAPIP());
        Serial.println(WiFi.channel());
        messageHandler.printAddress(myAddress);
        Serial.print("Free Heap: ");
        size_t freeHeap = ESP.getFreeHeap();
        Serial.print(freeHeap);
        Serial.println(" bytes");

        count++;
        //printAddress(myAddress);
        lastDings = millis();
      }
  
}

/*
  //server.on("/async", HTTP_GET, );)

  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;
    int foo = 1;
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      inputParam = PARAM_INPUT_1;
      foo = 1;


    }
  });
*/