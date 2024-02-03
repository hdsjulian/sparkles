#include <Arduino.h>
//#include <../lib/definitions.h>
#include <WiFi.h>
#include <../lib/declarations.h>
#include <../lib/ledfunctions.h>
#include <../lib/messaging.h>
//#include <ledfunctions.cpp>

message_blink blinkMessage;
message_address outgoingAddressMessage, incomingAddressMessage;




void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  switch (incomingData[0]) {
    case HELLO:  
      Serial.println("received Address");
      memcpy(&incomingAddressMessage,incomingData,sizeof(incomingAddressMessage));
      receiveAddress(incomingAddressMessage.address);
      break;
    case ADDRESS_RCVD: 
     Serial.println("address received");
      sendAddressFlag = false;
      break;
    case BLINK: 
        memcpy(&blinkMessage,incomingData,sizeof(blinkMessage));
        blink(blinkMessage);
      break;
    default: 
      Serial.println("Data type not recognized");
  }
}
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {

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
  esp_now_register_recv_cb(OnDataRecv);  
  esp_now_register_send_cb(OnDataSent);
  if (memcmp(myAddress, masterAddress, sizeof(uint8_t) * ADDRESS_SIZE) != 0) {
    registerAddress(masterAddress);
  }
  else {
    isMaster = true;
    } 
  registerAddress(broadcastAddress);
  delay(30000);
}


void loop() {
//networking stuff 
 if (sendAddressFlag == true and memcmp(myAddress, masterAddress, sizeof(uint8_t) * ADDRESS_SIZE) != 0) {
  Serial.println("sent address");
      sendAddress();
      delay(100);
  }
  else if (isMaster == true) {
/*    for (int i = 0; i < NUM_ADDRESSESl; i++) {
      
      if (esp_now_get_peer(addressList[i].address, &testPeerInfo) == ESP_OK) {
          blinkMessage.color[0] = random(128);
          blinkMessage.color[1] = random(128);
          blinkMessage.color[2] = random(128);
          blink(blinkMessage);

        Serial.println("blinking");
        printAddress(addressList[0].address);
      }
      
    }
  */    
      blinkMessage.color[0] = random(128);
      blinkMessage.color[1] = random(128);
      blinkMessage.color[2] = random(128);
      blink(blinkMessage);
    }
    delay(steps);
    

  }