#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <myDefines.h>
#include <queue>
#include <mutex>
#include <cstdint>

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

volatile bool buttonPressed = false;
  static bool isInterruptAttached = false;

void IRAM_ATTR handleButtonPress() {
  if (buttonPressed == false and (micros()-buttonPressTime)>2500000) {
    buttonPressTime = micros();
    buttonPressed = true;
  } 
}

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
  Serial.print("Received ");
  if (incomingData[0] != MSG_ANNOUNCE) {
    Serial.print("Received ");
  messageHandler.printAddress(mac->src_addr);
  messageHandler.messageCodeToText(incomingData[0]);
  }
  uint8_t senderAddress[6];
  memcpy(senderAddress, mac->src_addr, 6);
  messageHandler.pushDataToReceivedQueue(senderAddress, incomingData, len, micros());
}

int debugcounter = 0;
int timeadd = 0;
unsigned long debugtime = 0;

void setup() {
    Serial.begin(115200);
   int startTime = millis();


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
  int currentMode = stateMachine.getMode();
      messageHandler.handleErrors();

  messageHandler.processDataFromReceivedQueue();
  messageHandler.processDataFromSendQueue();
  // Attach the interrupt if the state is MODE_CALIBRATE
  if (stateMachine.getMode() == MODE_CALIBRATE) {
  if (!isInterruptAttached) {
    attachInterrupt(digitalPinToInterrupt(CLAP_PIN), handleButtonPress, RISING);
    isInterruptAttached = true;
    Serial.println("Interrupt attached");
  } 
  // Detach the interrupt if the state is not MODE_CALIBRATE
    if (buttonPressed == true) {
      buttonPressed = false;
      messageHandler.addClap(buttonPressTime);
      
      Serial.println("CLAP! BPT: "+String(buttonPressTime) );
      stateMachine.switchMode(MODE_NEUTRAL);
      //messageHandler.sendSingleClap(buttonPressTime);
    }
  }
  else {
   if (isInterruptAttached) {
    detachInterrupt(digitalPinToInterrupt(CLAP_PIN));
    isInterruptAttached = false;
    Serial.println("Interrupt detached");
  }

  // put your main code here, to run repeatedly:
     sent = false;    

      if (lastDings + 5000 < millis() )
      {
        Serial.print("Webserver still alive ");
        Serial.println(count);
        WiFi.macAddress(myAddress);

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