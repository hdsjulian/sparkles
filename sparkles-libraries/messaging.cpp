#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <../../sparkles-main-config/src/messaging.h>

messaging::messaging(modeMachine globalModeMachine, ledHandler handleLed) {
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
    return;            
    }
    memcpy(&peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    WiFi.macAddress(myAddress);
    messagingModeMachine = globalModeMachine;
};

void messaging::removePeer(uint8_t address[6]) {
        if (esp_now_del_peer(address) != ESP_OK) {
        Serial.println("coudln't delete peer");
        return;
    }
}
void messaging::printAddress(const uint8_t * mac_addr){
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    Serial.println(macStr);
}

int messaging::addPeer(uint8_t * address) {
    memcpy(&peerInfo.peer_addr, address, 6);
    if (esp_now_get_peer(peerInfo.peer_addr, &peerInfo) == ESP_OK) {
        Serial.println("Found Peer");
        return 0;
    }
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
        // Add peer        
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Serial.println("Failed to add peer");
        return -1;
    }
    else {
        Serial.println("Added Peer");
        return 1;
    }
}

void messaging::handleAddressMessage(const esp_now_recv_info * mac) {
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (memcmp(clientAddresses[i].address, emptyAddress, 6) == 0) {
            Serial.println("need to add peer");
            memcpy(clientAddresses[i].address, addressMessage.address, 6);
            memcpy(&timerReceiver, mac->src_addr, 6);
            addPeer(timerReceiver);
            addressCounter++;
            messagingModeMachine.switchMode(MODE_SENDING_TIMER);
            break;
            }
            else if (memcmp(&clientAddresses[i].address, &addressMessage.address, 6) == true) {
            Serial.print("found: ");
            printAddress(addressMessage.address);
            messagingModeMachine.switchMode(MODE_SENDING_TIMER);

            break;
            }
        }

}
void messaging::handleGotTimer() {
    removePeer(timerReceiver);
    timerCounter = 0;
    lastDelay = 0;
    messagingModeMachine.switchMode(MODE_ANIMATE);
}
int messaging::getLastDelay() {
    return lastDelay;
}
void messaging::setLastDelay(int delay) {
    lastDelay = delay;
}

void messaging::handleClapTime() {
    
}
int messaging::getTimerCounter(){
    return timerCounter;
}
void messaging::setTimerCounter(int counter) {
    timerCounter = counter;
}
void messaging::incrementTimerCounter() {
    timerCounter++;
}
void messaging::setSendTime(unsigned long time) {
    sendTime = time;
}
void messaging::setArriveTime(unsigned long time) {
    arriveTime = time;
}
unsigned long messaging::getSendTime() {
    return sendTime;
}
unsigned long messaging::getArriveTime() {
    return arriveTime;
}
void messaging::printMessage(int message) { 
    Serial.print("Message: ");
    switch (message) {
        case MSG_HELLO:
            Serial.println("MSG_HELLO");
        break;
        case MSG_ANNOUNCE:
            Serial.println("MSG_ANNOUNCE");
        break;
        case MSG_GOT_TIMER : 
            Serial.println("MSG_GOT_TIMER ");
        break;
        case MSG_TIMER_CALIBRATION : 
            Serial.println("MSG_TIMER_CALIBRATION ");
        break;
        case MSG_SEND_CLAP_TIME:
            Serial.println("MSG_SEND_CLAP_TIME");
        break;
        case MSG_ANIMATION: 
            Serial.println("MSG_ANIMATION");
        default: 
            Serial.println("Didn't recognize Message");
            Serial.println(message);
    }
}
void messaging::receiveTimer(int messageArriveTime) {
  //add condition that if nothing happened after 5 seconds, situation goes back to start
  //wenn die letzte message maximal 300 mikrosekunden abweicht und der letzte delay auch nicht mehr als 1500ms her war, dann muss die msg korrekt sein
  int difference = messageArriveTime - lastTime;
  lastDelay = timerMessage.lastDelay;

  if (abs(difference-CALIBRATION_FREQUENCY*1000) < 500 and abs(timerMessage.lastDelay) <2500) {
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
      printAddress(hostAddress);
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
  void messaging::prepareAnnounceMessage() {
    memcpy(&announceMessage.address, myAddress, 6);
    announceMessage.sendTime = sendTime;
  }
  void messaging::prepareTimerMessage() {
    timerMessage.sendTime = sendTime;
    timerMessage.counter = timerCounter;
    timerMessage.lastDelay = lastDelay;
  }



