#include "messaging.h"
#include "ledfunctions.h"
#include "definitions.h"
extern uint8_t masterAddress[6];
extern message_address outgoingAddressMessage;
extern esp_now_peer_info_t peerInfo;
extern esp_now_peer_num_t numPeers;
extern esp_now_peer_info_t testPeerInfo;
extern addresses addressList[NUM_ADDRESSES-1];
extern message_address_rcvd addressRcvd;


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
    default: 
      Serial.print("Unknown message type ");
      Serial.println(messageType);
  }
}

void sendAddress() {
  esp_now_send(masterAddress, (uint8_t *) &outgoingAddressMessage, sizeof(outgoingAddressMessage));
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

void registerAddress(uint8_t address[6]) {
  Serial.print("registering new");
  printAddress(address);

  memcpy(peerInfo.peer_addr, address, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
    esp_now_get_peer_num(&numPeers);

}
void receiveAddress(uint8_t address[6]) {
  Serial.println("received Address");
  if (esp_now_get_peer(address, &testPeerInfo) == ESP_OK) {
    Serial.println("found address in list!");
  }
  else {
    memcpy(addressList[addressCounter].address, address, sizeof(uint8_t) * ADDRESS_SIZE);
    addressList[addressCounter].registered=1;
    registerAddress(address);
     addressCounter++;
  }
  memcpy(addressRcvd.address, outgoingAddressMessage.address, sizeof(uint8_t) * ADDRESS_SIZE);
  blinkquick(255, 0, 0, 100);
  esp_now_send(address, (uint8_t *) &addressRcvd, sizeof(addressRcvd));
}