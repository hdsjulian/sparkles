/*
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

void messaging::pushDataToSendQueue(const uint8_t * address, int messageId, int param1, int param2) {
  debugVariable = 3;
    addError("Sending message "+messageCodeToText(messageId)+ "\n");
    addError("To: "+stringAddress(address)+ "\n");
    if (messageId ==MSG_COMMANDS) {
        addError("Command "+messageCodeToText(commandMessage.messageId));
    }

    std::lock_guard<std::mutex> lock(sendQueueMutex); // Lock the mutex
    SendData sendData {address, messageId, param1, param2};
    sendQueue.push(sendData); // Push the received data into the queue
}


void messaging::processDataFromReceivedQueue() {

    std::vector<ReceivedData> receivedDataList; // To store data temporarily
    {
        std::lock_guard<std::mutex> lock(receiveQueueMutex); // Lock the mutex
        while (!dataQueue.empty()) {
            ReceivedData receivedData = dataQueue.front(); // Get the front element
            // Process the received data here...
            dataQueue.pop(); // Remove the front element from the queue
            receivedDataList.push_back(receivedData);
        }
    } // Mutex is unlocked here

    // Call handleReceive outside the mutex scope
    for (const auto& receivedData : receivedDataList) {
        handleReceive(receivedData.senderAddress, receivedData.incomingData, receivedData.len, receivedData.msgReceiveTime);
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
          commandMessage.param = sendData.param2;
          Serial.print("Sending");
          Serial.print(messageCodeToText(sendData.messageId));
          Serial.print(" -- ");
          Serial.println(sendData.param2);
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
            addError("Sending Clap Times \n");
            addError(printClapTimes(sendClapTimes.timeStamp, NUM_CLAPS), false);
            esp_now_send(hostAddress, (uint8_t* ) &sendClapTimes, sizeof(sendClapTimes));
          break;

        }
    }
} 

String messaging::printClapTimes(unsigned long* array, int size) {
  String returnString;
  for (int i = 0; i < NUM_CLAPS; i++) {
    returnString +=array[i];
    if (i < size - 1) {
      returnString +=", ";
    }
  }
  returnString += "\n";
  return returnString;
}


void messaging::handleReceive(uint8_t *senderAddress, const uint8_t *incomingData, int len, unsigned long msgReceiveTime) {
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
        break;
        case CMD_DELETE_CLAP: 
          Serial.println("deleting clap "+String(commandMessage.param));
          sendClapTimes.timeStamp[commandMessage.param] = 0;
          sendClapTimes.clapCounter--;
          break;
      }
    }
 
  }

}



void messaging::addClap(unsigned long timeStamp) {
    //todo refactor
    if (sendClapTimes.clapCounter < NUM_CLAPS) {
        sendClapTimes.timeStamp[sendClapTimes.clapCounter] = timeStamp+timeOffset;
        addError("Clap Counter" + String(sendClapTimes.clapCounter), false);
        addError("clap! TS: "+String(timeStamp)+" TS+O"+String(timeStamp+timeOffset), false);
    }
    else {
        addError("TOO MANY CLAPS");
    }
    sendClapTimes.clapCounter++;
    sendSingleClap(timeStamp);
    globalModeHandler->switchMode(MODE_NEUTRAL);
    addError("send clap times is "+String(printClapTimes(sendClapTimes.timeStamp, NUM_CLAPS)), false);
    Serial.println("whyyyy");

}

void messaging::sendSingleClap(unsigned long buttonPressTime){
  sendSingleClapMessage.timeStamp = buttonPressTime+timeOffset;
  Serial.println("NOw: "+String(micros()+timeOffset));
  sendSingleClapMessage.clapCounter = sendClapTimes.clapCounter;
  esp_now_send(hostAddress, (uint8_t *) &sendSingleClapMessage, sizeof(sendSingleClapMessage));
  commandMessage.messageId = CMD_MASTERCLAP_OCCURRED;
  esp_now_send(broadcastAddress, (uint8_t *) &commandMessage, sizeof(commandMessage));
}

*/