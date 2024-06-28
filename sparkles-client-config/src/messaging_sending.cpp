#include "Arduino.h"

#include "esp_now.h"
#include "WiFi.h"
#include "messaging.h"

void messaging::announceAddress() {
    haveSentAddress = true;
    if (gotTimer == true) {
        return;
    }
    if (globalModeHandler->getMode() == MODE_WAIT_FOR_TIMER) {
        if (lastTime < millis()+2000 and lastTime != 0) {
             return;   
        }
    }
    //handleLed->flash(0, 0, 125, 100, 2, 50);
    announceTime = millis();
    pushDataToSendQueue(hostAddress, MSG_ADDRESS, -1);
}

  void messaging::prepareTimerMessage() {
    timerMessage.sendTime = sendTime;
    timerMessage.counter = timerCounter;
    timerMessage.lastDelay = lastDelay;
  }
void messaging::getClapTimes(int i) {
    addError("Get clap times called" + String(i) + "\n");
    stringAllAddresses();
    if (i == -1) {
        addError("Asking Clap Device for Clap Time\n");
        askClapTimesMessage.millisA = millis();
        askClapTimesMessage.debug = "a";
        pushDataToSendQueue(clapDeviceAddress, MSG_ASK_CLAP_TIMES, -1);
    }
    else if (i < addressCounter) {
        addError("asking device " + String(i) + " for clap time\n");
        addError("Address: "+stringAddress(clientAddresses[i].address)+"\n");
        askClapTimesMessage.millisA = millis();
        pushDataToSendQueue(clientAddresses[i].address, MSG_ASK_CLAP_TIMES, -1);
    }
}

void messaging::pushDataToSendQueue(const uint8_t * address, int messageId, int param) {
    addError("Sending message "+messageCodeToText(messageId)+ "\n");
    addError("To: "+stringAddress(address)+ "\n");
    if (messageId ==MSG_COMMANDS) {
        addError("Command "+messageCodeToText(commandMessage.messageId));
    }

    std::lock_guard<std::mutex> lock(sendQueueMutex); // Lock the mutex
    SendData sendData {address, messageId, param};
    sendQueue.push(sendData); // Push the received data into the queue
}
void messaging::processDataFromSendQueue() {
    std::lock_guard<std::mutex> lock(sendQueueMutex); // Lock the mutex
    uint8_t peerAddress[6];
    int foundPeer = 0;
    while (!sendQueue.empty()) {
        SendData sendData = sendQueue.front(); // Get the front element
        Serial.println("processing sent");

        if (memcmp(sendData.address, broadcastAddress, 6) != 0 and memcmp(sendData.address, clapDeviceAddress, 6) != 0) {
            memcpy(peerAddress, sendData.address, 6);
            foundPeer = addPeer(peerAddress);
            addError("Sent "+messageCodeToText(sendData.messageId)+" to ");
            addError(stringAddress(sendData.address)+"\n");
        }
        else {
            addError("Sent "+messageCodeToText(sendData.messageId)+" to ");
            addError(stringAddress(sendData.address)+"\n");
        
        }
        if (sendData.messageId == MSG_COMMANDS) {
            addError("Command = "+messageCodeToText(sendData.param));

        }


        // Process the received data here...
        sendQueue.pop(); // Remove the front element from the queue
        switch(sendData.messageId) {
            case MSG_ADDRESS:
                esp_now_send(sendData.address, (uint8_t*) &addressMessage, sizeof(addressMessage));
                break;
            case MSG_ANNOUNCE:
                esp_now_send(sendData.address, (uint8_t*) &announceMessage, sizeof(announceMessage));
                break;
            case MSG_TIMER_CALIBRATION:
                esp_now_send(sendData.address, (uint8_t*) &timerMessage, sizeof(timerMessage));
                //todo timer reset
                break;
            case MSG_GOT_TIMER:
                esp_now_send(sendData.address, (uint8_t*) &gotTimerMessage, sizeof(gotTimerMessage));
                break;
            case MSG_ASK_CLAP_TIMES:
                addError("ASK CLAP TIMES CALLED, sending to "+stringAddress(sendData.address)+" and ask clap times message type is "+String(askClapTimesMessage.message_type)+"\n");
                askClapTimesMessage.millisB = millis();
                esp_now_send(sendData.address, (uint8_t*) &askClapTimesMessage, sizeof(askClapTimesMessage));
                break;
            case MSG_SWITCH_MODE: 
                esp_now_send(sendData.address, (uint8_t*) &switchModeMessage, sizeof(switchModeMessage));
                break;
            case MSG_SEND_CLAP_TIMES:
                esp_now_send(sendData.address, (uint8_t*) &sendClapTimes, sizeof(sendClapTimes));
                break;
            case MSG_ANIMATION:
                esp_now_send(sendData.address, (uint8_t*) &animationMessage, sizeof(animationMessage));
                break;
            case MSG_DISTANCE:
                addError("Sending distance message\n");
                esp_now_send(sendData.address, (uint8_t*) &distanceMessage, sizeof(distanceMessage));
                break;
            case MSG_NOCLAPFOUND:
                addError("No Clap Found\n");
                // Handle MSG_NOCLAPFOUND message type
                break;
            case MSG_COMMANDS:
                if (sendData.param != -1) {
                    commandMessage.param = sendData.param;
                }
                esp_now_send(sendData.address, (uint8_t*) &commandMessage, sizeof(commandMessage));
                commandMessage.param = -1;
                if (commandMessage.messageId == CMD_RESET) {
                    memset(clientAddresses, 0, sizeof(clientAddresses));
                    addError("Resetting Addresses\n");
                    writeStructsToFile(clientAddresses, NUM_DEVICES, "/clientAddress");
                    ESP.restart();
                }
                break;
            case MSG_SET_POSITIONS:
                esp_now_send(sendData.address, (uint8_t*) &setPositionsMessage, sizeof(setPositionsMessage));
                break;
            case MSG_BATTERY_STATUS:
                esp_now_send(sendData.address, (uint8_t*) &batteryStatusMessage, sizeof(batteryStatusMessage));
                break;
            default: 
                addError("Message to send: unknown\n");
                break;
        }
        if (memcmp(sendData.address, hostAddress, 6) != 0 and memcmp(sendData.address, broadcastAddress, 6) != 0 and memcmp(sendData.address, clapDeviceAddress, 6) != 0 and foundPeer >0) {

            removePeer(peerAddress);
            addError("Removed Peer "+stringAddress(peerAddress)+"\n"); 
        }
    }
}   



void messaging::updateTimers(int addressId) {
    globalModeHandler->switchMode(MODE_RESET_TIMER);
    addError("Updating timers for address "+String(addressId)+"\n");
    memcpy(&timerReceiver, clientAddresses[addressId].address, 6);
    addPeer(timerReceiver);
    timerMessage.reset = true;
    timerMessage.addressId = addressId;
    clientAddresses[addressId].active = SETTING_TIMER;
}


void messaging::sendCommand(int commandId) {
    addError("Sending command "+messageCodeToText(commandId)+"\n");
    pushDataToSendQueue(broadcastAddress, commandId, -1);
}
void messaging::sendMode(int modeId) {
    addError("Sending mode "+messageCodeToText(modeId)+"\n");
    pushDataToSendQueue(broadcastAddress, MSG_SWITCH_MODE, modeId);
}
void messaging::sendMessageById(int messageId, int addressId, int param) {
    addError("Sending Message by Id "+messageCodeToText(messageId)+" address "+stringAddress(clientAddresses[addressId].address));
    if (param != -1) {
        addError(" Command "+messageCodeToText(param)+"\n");
    }
    else {
        addError("\n");
    }
    pushDataToSendQueue(clientAddresses[addressId].address, messageId, param);
}

void messaging::nextRetry() {
    if (timeoutRetry.lastMsg == 0) {return;}
    if (timeoutRetry.currentId > 0) {return;}
    if (timeoutRetry.currentId == 0 and millis() < timeoutRetry.lastMsg+60000) {
        return;
    } 
    else if (timeoutRetry.currentId == 0 and millis() > timeoutRetry.lastMsg+60000 and timeoutRetry.lastMsg > 0) {
        addError("Starting all over");
        timeoutRetry.lastMsg = 0;
        timeoutRetry.unavailableCounter = 0; 
        timeoutRetryHandler();
    }
    
}

void messaging::timeoutRetryHandler() {
    Serial.println("Timeout Retry Handler  ");
    Serial.println("Current Id: "+String(timeoutRetry.currentId));
    Seria.println("Address Counter: "+String(addressCounter));
    
    if (timeoutRetry.currentId == addressCounter-1) {
        timeoutRetry.currentId = 0;
        timeoutRetry.lastMsg = millis();
        addError("reached Address counter, waiting 60 seconds");
        if (timeoutRetry.unavailableCounter == 0) {
            addError("all done");
            globalModeHandler->switchMode(MODE_NEUTRAL);
            return;
        }
    }
    timeoutRetry.currentId++;
     switch (globalModeHandler->getMode()) {
        case MODE_GET_CALIBRATION_DATA:
                getClapTimes(timeoutRetry.currentId);
            break;
        case MODE_RESET_TIMER:
                handleTimerUpdates();
            break;
        case MODE_SENDING_TIMER:
                globalModeHandler->switchMode(MODE_NEUTRAL);
            break;
        
    }

}







