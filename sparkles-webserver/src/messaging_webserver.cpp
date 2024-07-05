
#define DEVICE_MODE 2
#include <messaging.h>
#include <helperFuncs.h>
#include <ArduinoJson.h>




void messaging::setup(modeMachine &modeHandler, esp_now_peer_info_t &globalPeerInfo) {
    debugVariable = 1;
    globalModeHandler = &modeHandler;
    peerInfo = &globalPeerInfo;
    setHostAddress(hostAddress);
    addError("This should somehow...\n");

}


void messaging::setHostAddress(uint8_t address[6]) {
    debugVariable = 2;
    memcpy (hostAddress, address, 6);
    addPeer(hostAddress);
    memcpy(addressMessage.address, clapDeviceAddress, 6);
    addError("my address "+stringAddress(clapDeviceAddress));
    pushDataToSendQueue(hostAddress, MSG_ADDRESS, -1);
}

void messaging::pushDataToSendQueue(const uint8_t * address, int messageId, int param) {
  debugVariable = 3;
    addError("Sending message "+messageCodeToText(messageId)+ "\n");
    addError("To: "+stringAddress(address)+ "\n");
    if (messageId ==MSG_COMMANDS) {
        addError("Command "+messageCodeToText(commandMessage.messageId));
    }

    std::lock_guard<std::mutex> lock(sendQueueMutex); // Lock the mutex
    SendData sendData {address, messageId, param};
    sendQueue.push(sendData); // Push the received data into the queue
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
  debugVariable = 4;
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
          case MSG_ADDRESS: 
            esp_now_send(hostAddress, (uint8_t *) &addressMessage, sizeof(addressMessage));
            break;
          case MSG_GOT_TIMER: 
            esp_now_send(hostAddress, (uint8_t *) &gotTimerMessage, sizeof(gotTimerMessage));
            break;
          case MSG_SEND_CLAP_TIMES: 
          sendClapTimes.clapCounter = 10;
            esp_now_send(hostAddress, (uint8_t* ) &sendClapTimes, sizeof(sendClapTimes));
          break;
          case MSG_TIME_THING: 
          esp_now_send(hostAddress, (uint8_t *) &timeThingMessage, sizeof(timeThingMessage));
          break;
        }
    }
} 

void messaging::handleReceive(const esp_now_recv_info * mac, const uint8_t *incomingData, int len, unsigned long msgReceiveTime) {
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
    case MSG_COMMANDS: {
      memcpy(&commandMessage,incomingData,sizeof(commandMessage));
      Serial.println("received cmd "+String(commandMessage.messageId));
      switch (commandMessage.messageId) {
        case CMD_START_CALIBRATION_MODE: 
          globalModeHandler->switchMode(MODE_CALIBRATE);
          break;
        case CMD_END_CALIBRATION_MODE: 
          globalModeHandler->switchMode(MODE_NEUTRAL);
          break;
        case CMD_GET_CLAP_TIMES: 
        Serial.println("received cmd get clap times");
          pushDataToSendQueue(hostAddress, MSG_SEND_CLAP_TIMES, -1);
      }
    }
 
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



void messaging::addClap(unsigned long timeStamp) {
    //todo refactor
   
    if (sendClapTimes.clapCounter < NUM_CLAPS) {
        sendClapTimes.timeStamp[sendClapTimes.clapCounter] = timeStamp+timeOffset;
        Serial.println("clap! TS: "+String(timeStamp)+" TS+O"+String(timeStamp+timeOffset));
    }
    else {
        addError("TOO MANY CLAPS");
    }
    sendClapTimes.clapCounter++;
}

void messaging::sendSingleClap(unsigned long buttonPressTime){
  sendSingleClapMessage.clapTime = buttonPressTime+timeOffset;
  Serial.println("NOw: "+String(micros()+timeOffset));
  sendSingleClapMessage.clapCounter = sendClapTimes.clapCounter;
  esp_now_send(hostAddress, (uint8_t *) &sendSingleClapMessage, sizeof(sendSingleClapMessage));

}