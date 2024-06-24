#include <webserver.h>
#define DEVICE_MODE 2
#include <messaging.h>
#include <helperFuncs.h>




void messaging::setup(webserver &Webserver, modeMachine &modeHandler, esp_now_peer_info_t &globalPeerInfo) {
    webServer = &Webserver;
    globalModeHandler = &modeHandler;
    peerInfo = &globalPeerInfo;
}


void messaging::setHostAddress(uint8_t address[6]) {
    memcpy (&hostAddress, address, 6);
    addPeer(hostAddress);
}

void messaging::pushDataToSendQueue(int messageId, int param) {
    std::lock_guard<std::mutex> lock(sendQueueMutex);
    sendQueue.push({messageId, param}); // Push the received data into the queue
} 


void messaging::processDataFromReceivedQueue() {
    std::lock_guard<std::mutex> lock(receiveQueueMutex); // Lock the mutex
    while (!dataQueue.empty()) {
        ReceivedData receivedData = dataQueue.front(); // Get the front element

        // Process the received data here...
        dataQueue.pop(); // Remove the front element from the queue
        
        handleReceive(receivedData.mac, receivedData.incomingData, receivedData.len, receivedData.msgReceiveTime);
    }
}  

void messaging::processDataFromSendQueue() {
    std::lock_guard<std::mutex> lock(sendQueueMutex);
    while (!sendQueue.empty()) {
        SendData sendData = sendQueue.front(); // Get the front element
        // Process the received data here...
        sendQueue.pop(); // Remove the front element from the queue
        if (sendData.messageId > CMD_START and commandMessage.messageId < CMD_END) {
          commandMessage.messageId = sendData.messageId;
          commandMessage.param = sendData.param;
          Serial.print("Sending");
          Serial.print(messageCodeToText(sendData.messageId));
          Serial.print(" -- ");
          Serial.println(sendData.param);
          esp_now_send(hostAddress, (uint8_t *) &commandMessage, sizeof(commandMessage));
          return;
        } 
        switch (sendData.messageId) {
          case MSG_SEND_CLAP_TIMES: 
            Serial.println("Sending clap times");
            Serial.print("Number of claps ");
            Serial.println(sendClapTimes.clapCounter);
            esp_now_send(hostAddress, (uint8_t *) &sendClapTimes, sizeof(sendClapTimes));
            break;
          case MSG_SET_TIME:
            Serial.println("Sending set timer");
            esp_now_send(hostAddress, (uint8_t *) &setTimeMessage, sizeof(setTimeMessage));
            break;
          case MSG_SET_POSITIONS:
            Serial.println("Sending set positions");
            esp_now_send(hostAddress, (uint8_t *) &setPositionsMessage, sizeof(setPositionsMessage));
            break;
          case MSG_ANIMATION: 
            esp_now_send(hostAddress, (uint8_t *) &animationMessage, sizeof(animationMessage));
            break;
          case MSG_SET_SLEEP_WAKEUP:
          Serial.println("should send host to sleep at "+String(setSleepWakeupMessage.hours)+":"+String(setSleepWakeupMessage.minutes));
  
            esp_now_send(hostAddress, (uint8_t *) &setSleepWakeupMessage, sizeof(setSleepWakeupMessage));
            break;
        }
    }
} 


void messaging::handleReceive(const esp_now_recv_info * mac, const uint8_t *incomingData, int len, unsigned long msgReceiveTime) {
  if (incomingData[0] != MSG_ANNOUNCE) {
    Serial.print("Received ");
    Serial.print(msgCounter);
    Serial.print(" - ");
    Serial.println(messageCodeToText(incomingData[0]));
    //printAddress(mac->src_addr);
    Serial.print("aha");
  }
  msgCounter++;
  String jsonString;
  JsonDocument receivedJson;
  switch(incomingData[0]) {
    case MSG_TIMER_CALIBRATION:
        memcpy(&timerMessage,incomingData,sizeof(timerMessage));
        Serial.println("received timer calibration");
            //handleLed->flash(0, 0, 125 , 100, 2, 50); 
        receiveTimer(msgReceiveTime);
    break;
    case MSG_ADDRESS_LIST: 
      memcpy(&addressListMessage,incomingData,sizeof(addressListMessage));
      receivedJson["index"] = String(addressListMessage.index);
      receivedJson["address"] = stringAddress(addressListMessage.clientAddress.address);
      receivedJson["delay"] = String(addressListMessage.clientAddress.delay);
      receivedJson["status"] = modeToText(addressListMessage.status);
      receivedJson["distance"] = String(addressListMessage.clientAddress.distance);
      receivedJson["addressCounter"] = String(addressListMessage.addressCounter);
      receivedJson["xpos"] = String(addressListMessage.clientAddress.xLoc);
      receivedJson["ypos"] = String(addressListMessage.clientAddress.yLoc);
      receivedJson["zpos"] = String(addressListMessage.clientAddress.zLoc);
      receivedJson["battery"] = String(addressListMessage.clientAddress.batteryStatus);

      serializeJson(receivedJson, jsonString);
      Serial.println(jsonString.c_str());
      webServer->events.send(jsonString.c_str(), "new_readings", millis());
      break;
    case MSG_STATUS_UPDATE: 
      Serial.println("received status update");
      memcpy(&statusUpdateMessage, incomingData, sizeof(statusUpdateMessage));
      receivedJson["status"] = modeToText(statusUpdateMessage.mode);
      receivedJson["statusId"] = String(statusUpdateMessage.mode);
      serializeJson(receivedJson, jsonString);
      webServer->events.send(jsonString.c_str(), "new_status", millis());
      break;
    case MSG_ASK_CLAP_TIMES: 
    memcpy(&askClapTimesMessage, incomingData, sizeof(askClapTimesMessage));
    Serial.println(" asked for clap times");
    Serial.println("Num Claps: "+String(sendClapTimes.clapCounter));
    Serial.println("millisA: "+String(askClapTimesMessage.millisA));
    Serial.println("millisB: "+String(askClapTimesMessage.millisB));
    Serial.println("debug string "+String(askClapTimesMessage.debug));
    for (int i = 0; i<sendClapTimes.clapCounter; i++) {
      Serial.println("Clap: "+String(sendClapTimes.timeStamp[i]));
    }
    Serial.println("Pushing Send clap times to queue");
    pushDataToSendQueue(MSG_SEND_CLAP_TIMES, 0);
    break;
  }

}
/*
void messaging::receiveTimer(int messageArriveTime) {
  //add condition that if nothing happened after 5 seconds, situation goes back to start
  //wenn die letzte message maximal 300 mikrosekunden abweicht und der letzte delay auch nicht mehr als 1500ms her war, dann muss die msg korrekt sein
  int difference = messageArriveTime - lastTime;
  lastDelay = timerMessage.lastDelay;

  if (abs(difference-CALIBRATION_FREQUENCY*TIMER_INTERVAL_MS) < 1000 and abs(timerMessage.lastDelay) <2500) {
    addMessageLog("Counts. Arraycounter: ");
    addMessageLog(String(arrayCounter));
    addMessageLog("\n");

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
      gotTimerMessage.timerOffset = timeOffset;
      pushDataToSendQueue(MSG_GOT_TIMER, -1);
      gotTimer = true;
      #if DEVICE_MODE != 2
      handleLed->flash(255,0,0, 200, 3, 300);
      globalModeHandler->switchMode(MODE_GOT_TIMER);
      #else
      pushDataToSendQueue(CMD_START_CALIBRATION_MODE, -1);
      globalModeHandler->switchMode(MODE_CALIBRATE);
      #endif
      
      
    }
    arrayCounter++;
  }
  else {
    addMessageLog("Doesn't Count.");
    if (abs(difference-CALIBRATION_FREQUENCY*TIMER_INTERVAL_MS) >= 500) {
        addMessageLog(" Difference ");
        addMessageLog(String(abs(difference-CALIBRATION_FREQUENCY*TIMER_INTERVAL_MS)));
    }
    else if (abs(timerMessage.lastDelay) >=2500) {
    addMessageLog(" Last delay = ");
    addMessageLog(String(abs(timerMessage.lastDelay)));
    }
    addMessageLog("\n");
  }
   lastTime = messageArriveTime;
}*/
void messaging::setSetTimeMessage(int hours, int minutes, int seconds) {
  setTimeMessage.hours = hours;
  setTimeMessage.minutes = minutes;
  setTimeMessage.seconds = seconds;
}

void messaging::setPositions(int id, float xpos, float ypos, float zpos ) {

  setPositionsMessage.xpos = xpos;
  setPositionsMessage.ypos = ypos;
  setPositionsMessage.zpos = zpos;
  setPositionsMessage.id = id;

}
void messaging::setGoodNightWakeUp(int hours, int minutes, int seconds, bool isGoodNight) {
  setSleepWakeupMessage.hours = hours;
  setSleepWakeupMessage.minutes = minutes;
  setSleepWakeupMessage.seconds = seconds;
  setSleepWakeupMessage.isGoodNight = isGoodNight;
}