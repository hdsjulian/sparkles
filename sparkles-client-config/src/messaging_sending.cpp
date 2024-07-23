#include "Arduino.h"

#include "esp_now.h"
#include "WiFi.h"
#include "messaging.h"

void messaging::announceAddress() {
    announceCounter++;
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
    addError("get clap times called "+String(i)+"\n");
    if (i == -1) {

        commandMessage.messageId = CMD_GET_CLAP_TIMES;
        addError("pushing to send queue "+String(CMD_GET_CLAP_TIMES));
        pushDataToSendQueue(clapDevice.address, MSG_COMMANDS, CMD_GET_CLAP_TIMES);
   }
   else {
    if (memcmp(clientAddresses[i].address, emptyAddress, 6) == 0) {
        globalModeHandler->switchMode(MODE_NEUTRAL);
        return;
    }
    if (clientAddresses[i].active != ACTIVE) { i++;}
        commandMessage.messageId = CMD_GET_CLAP_TIMES;
        pushDataToSendQueue(clientAddresses[i].address, MSG_COMMANDS, CMD_GET_CLAP_TIMES);
    }
}

  /*
void messaging::getClapTimes(int i) {
    addError("Get clap times called" + String(i) + "\n");
    Serial.println("get clap times called");
    stringAllAddresses();
    if (i == -1) {
        Serial.println("Asking Clap Device for Clap Time\n");
        askClapTimesMessage.millisA = millis();
        askClapTimesMessage.debug = "a";
        pushDataToSendQueue(clapDeviceAddress, MSG_ASK_CLAP_TIMES, -1);
    }
    else if (i < addressCounter) {
        Serial.println("asking device " + String(i) + " for clap time\n");
        Serial.println("Address: "+stringAddress(clientAddresses[i].address)+"\n");
        askClapTimesMessage.millisA = millis();
        if (clientAddresses[i].active == ACTIVE) {
            pushDataToSendQueue(clientAddresses[i].address, MSG_ASK_CLAP_TIMES, -1);
        }
        else {
            getClapTimes(i+1);
        }       
    }
}*/

void messaging::pushDataToSendQueue(const uint8_t * address, int messageId, int param1, int param2) {
    addError("Sending message "+messageCodeToText(messageId)+ "\n");
    addError("To: "+stringAddress(address)+ "\n");
    if (messageId ==MSG_COMMANDS) {
        addError("Command "+messageCodeToText(commandMessage.messageId)+"\n");
    }

    std::lock_guard<std::mutex> lock(sendQueueMutex); // Lock the mutex
    SendData sendData {address, messageId, param1, param2};
    sendQueue.push(sendData); // Push the received data into the queue
}
void messaging::processDataFromSendQueue() {
    std::lock_guard<std::mutex> lock(sendQueueMutex); // Lock the mutex
    uint8_t peerAddress[6];
    int foundPeer = 0;
    while (!sendQueue.empty()) {
        SendData sendData = sendQueue.front(); // Get the front element
        addError("processing sent\n");

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
            addError("Command = "+messageCodeToText(sendData.param1)+"\n");

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
            case MSG_SWITCH_MODE: 
                addError("Calling and sending switchmode with mode "+globalModeHandler->modeToText(switchModeMessage.mode));
                esp_now_send(sendData.address, (uint8_t*) &switchModeMessage, sizeof(switchModeMessage));
                break;
            case MSG_SEND_CLAP_TIMES:
                addError("Sending Clap Times\n");
                for (int i = 0; i < NUM_CLAPS; i++ ) {
                    addError("Clap Time: "+String(sendClapTimes.timeStamp[i])+"\n");
                }
                esp_now_send(sendData.address, (uint8_t*) &myClapTimes, sizeof(myClapTimes));
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
                Serial.println("sending command "+messageCodeToText(sendData.param1));
                if (sendData.param1 != -1) {
                    commandMessage.messageId = sendData.param1;
                }
                commandMessage.param = sendData.param2;
                Serial.println("sending command "+messageCodeToText(commandMessage.messageId));
                Serial.println("sending message type "+messageCodeToText(commandMessage.messageType));

                esp_now_send(sendData.address, (uint8_t*) &commandMessage, sizeof(commandMessage));
                commandMessage.param = 0;
                switch(commandMessage.messageId) {
                    case CMD_RESET_SYSTEM:
                    for (size_t i = 0; i < std::size(clientAddresses); ++i) {
                        clientAddresses[i] = client_address{}; // Value-initialization
                    }
                    addError("Resetting Addresses\n");
                    writeStructsToFile(clientAddresses, NUM_DEVICES, "/clientAddress");
                    ESP.restart();
                    break;

                    default:
                    break;
                }

                break;
            case MSG_SET_POSITIONS:
                esp_now_send(sendData.address, (uint8_t*) &setPositionsMessage, sizeof(setPositionsMessage));
                break;
            case MSG_STATUS:
                esp_now_send(sendData.address, (uint8_t*) &statusMessage, sizeof(statusMessage));
                break;
            case MSG_TIMESYNC:
                esp_now_send(sendData.address, (uint8_t*) &timeSyncMessage, sizeof(timeSyncMessage));
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


void messaging::sendCommand(int commandId) {
    addError("Sending command "+messageCodeToText(commandId)+"\n");
    pushDataToSendQueue(broadcastAddress, commandId, -1);
}
void messaging::sendMode(int modeId) {
    addError("Sending mode "+messageCodeToText(modeId)+"\n");
    switchModeMessage.mode = modeId;
    pushDataToSendQueue(broadcastAddress, MSG_SWITCH_MODE, -1 );
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

void messaging::sendTimeSync(int id) {
    timeSyncMessage.offset = timeOffset;
    timeSyncMessage.myTime = micros();
    if (id == -1) {
        pushDataToSendQueue(hostAddress, MSG_TIMESYNC, -1);
    }
    else {
        pushDataToSendQueue(clientAddresses[id].address, MSG_TIMESYNC, -1);

    }
}









