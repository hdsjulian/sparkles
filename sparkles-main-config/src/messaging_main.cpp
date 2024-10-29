#include "Arduino.h"

#include "esp_now.h"
#include "WiFi.h"
#include "messaging.h"

void messaging::setup(modeMachine &modeHandler, ledHandler &globalHandleLed, esp_now_peer_info_t &globalPeerInfo, webserver &myWebserver) {
    WiFi.macAddress(myAddress);
    handleLed = &globalHandleLed;
    globalModeHandler = &modeHandler;
    peerInfo = &globalPeerInfo;
    sleepTime.hours = GOODNIGHT_HOUR;
    sleepTime.minutes = GOODNIGHT_MINUTE;
    sleepTime.seconds = 0; 
    wakeupTime.hours = GOODMORNING_HOUR;
    wakeupTime.minutes = GOODMORNING_MINUTE;
    wakeupTime.seconds = 0;
    webServer = &myWebserver;
    memcpy(&clapDevice.address, clapDeviceAddress, 6);
    addPeer(clapDeviceAddress);
    if (!readStructsFromFile(clientAddresses, NUM_DEVICES,  "/clientAddress")) {
        addError("Failed to read client addresses from file");
    }
    else {
        addError("successfully read client addresses from file\n");
        addressCounter = 0;
        for (int i = 0; i < NUM_DEVICES; i++) {
            if (memcmp(clientAddresses[i].address, emptyAddress, 6) != 0) {
                addError("Found address at index "+String(i)+"\n");
                addError(stringAddress(clientAddresses[i].address));
                addressCounter++;
                clientAddresses[i].clapTimes.clapCounter = 0;
                for (int j = 0; j < NUM_CLAPS; j++) {
                    clientAddresses[i].clapTimes.timeStamp[j] = 0;
                }
                clientAddresses[i].delay = 0;
                clientAddresses[i].timerOffset = 0;
                clientAddresses[i].active = INACTIVE;
            }
            else {
                addError("We have "+String(addressCounter)+" addresses\n"); 
                webServer->updateDeviceNum();
                return;
            }
        }
        addError("Address counter: "+String(addressCounter)+"\n");
    
    }
}





void messaging::handleReceive(uint8_t *senderAddress, const uint8_t *incomingData, int len, unsigned long long msgReceiveTime) {
    addError("Handling Received "+String(incomingData[0]), false);
    //Serial.println(messageCodeToText(incomingData[0]));
    addError(" from ");
    if (senderAddress!=NULL) {
        addError(stringAddress(senderAddress));
        addError("\n");
    }
    else {
        Serial.println("mac is null\n");
    }
    String jsonString;

    switch (incomingData[0]) {
        //cases for main
/*
                case CMD_START_ANIMATION:
                    globalModeHandler->switchMode(MODE_ANIMATE);
                    switchModeMessage.mode = MODE_ANIMATE;
                    pushDataToSendQueue(broadcastAddress, MSG_SWITCH_MODE, -1);
                    animationMessage.animationType = SYNC_ASYNC_BLINK;
                    handleLed->getNextAnimation(&animationMessage);
                    animationMessage.startTime = micros()+1000000;
                    animationMessage.num_devices = addressCounter;
                    handleLed->printAnimationMessage(animationMessage);
                    nextAnimationPing = 1000+millis()+handleLed->calculate(&animationMessage);
                    Serial.println("Next animation ping = "+String(nextAnimationPing));
                    Serial.println("Now = "+String(millis()));
                    pushDataToSendQueue(broadcastAddress, MSG_ANIMATION, -1);
                    endAnimation == false;
  */
        case MSG_ADDRESS: 
            addError("Received Address and setting timer receiver\n");
            if (globalModeHandler->getMode() == MODE_WAIT_FOR_TIMER or globalModeHandler->getMode() == MODE_RESET_TIMER) {
                break;
            }
            else if (globalModeHandler->getMode() == MODE_OTA) {
                if (memcmp(senderAddress, otaAddress, 6) != 0)  {
                    ESP_LOGI("ota", "received address from different device while in ota mode");
                    break;
                }
                else if (incomingData[offsetof(message_address, version)] < version) {
                    ESP_LOGI("ota", "received address while in OTA mode, but version still smaller");
                    pushDataToSendQueue(senderAddress, MSG_OTA, -1);
                }
                else {
                    ESP_LOGI("ota", "received address while in OTA mode, version is ok");
                    setTimerReceiver(incomingData);
                }
            }
            else if (incomingData[offsetof(message_address, version)] < version && false) {
                ESP_LOGI("ota", "received address while not in OTA mode, but version smaller");
                pushDataToSendQueue(senderAddress, MSG_OTA, -1);
                memcpy(&otaAddress, senderAddress, 6);
                globalModeHandler->switchMode(MODE_OTA);
            }
            else {
                ESP_LOGI("ota", "received address while not in OTA mode, version is ok");
                setTimerReceiver(incomingData);
            }
            break;

        case MSG_GOT_TIMER:                                          
            handleGotTimer(incomingData, senderAddress);
            break; 
        case MSG_SEND_CLAP_TIMES:
            addError("calling receive clap times");
            memcpy(&sendClapTimes, incomingData, sizeof(sendClapTimes));
            
            receiveClapTimes(senderAddress);
        break;


        case MSG_STATUS: {
            memcpy(&statusMessage, incomingData, sizeof(statusMessage));
            addError("Battery Status: "+String(statusMessage.batteryPercentage)+"\n");
            int id = getAddressId(senderAddress);
            clientAddresses[id].batteryPercentage = statusMessage.batteryPercentage;
                Serial.println("---msgstatus---");

            updateAddressesToWebserver();
            
        break;    
        }
        case MSG_SEND_SINGLE_CLAP:
            addError("Got a single clap message at "+String(micros())+"\n");
            memcpy(&sendSingleClapMessage, incomingData, sizeof(sendSingleClapMessage));
            globalModeHandler->switchMode(MODE_MASTERCLAP_OCCURRED);
            break;
        case MSG_TIMESYNC:
            break;
            memcpy(&timeSyncMessage, incomingData, sizeof(timeSyncMessage));
            addError("My Micros: "+String(micros()), false);
            addError("Client Micros: "+String(timeSyncMessage.myTime), false);
            addError("Client Offset: "+String(timeSyncMessage.offset), false);
            addError("Estimated: "+String(timeSyncMessage.myTime+timeSyncMessage.offset), false);
            addError("difference: " +String((int)timeSyncMessage.myTime+(int)timeSyncMessage.offset+clientAddresses[getAddressId(senderAddress)].delay*2-(int)micros()), false);
            addError("delay: "+String(clientAddresses[getAddressId(senderAddress)].delay), false);
            break;
        default: 
            addError("message not recognized: ");
            addError(messageCodeToText(incomingData[0]));
            addError("\n");

            break;
    }
}




//this is the test version



void messaging::deleteClap(int clapId) {
    clapDevice.clapTimes.timeStamp[clapId] = 0;
    clapDevice.clapTimes.clapCounter--;
    pushDataToSendQueue(clapDeviceAddress, MSG_COMMANDS, CMD_DELETE_CLAP, clapId);
}




void messaging::handleSingleClap() {
    String jsonString;
    addError("Single clap timestamp "+String(sendSingleClapMessage.timeStamp)+"\n");
    addError("Clap Counter: "+String(sendSingleClapMessage.clapCounter)+"\n");
    addError("Difference "+String(micros() - sendSingleClapMessage.timeStamp)+"\n");
    clapDevice.clapTimes.timeStamp[sendSingleClapMessage.clapCounter] = sendSingleClapMessage.timeStamp;
    updateAddressesToWebserver();
    jsonString = "{\"status\" : \"2\", \"clapId\" : "+String(sendSingleClapMessage.clapCounter)+", \"timeStamp\" : "+String(sendSingleClapMessage.timeStamp)+"}";
    webServer->events.send(jsonString.c_str(), "clapCheck");
    pushDataToSendQueue(broadcastAddress, MSG_COMMANDS, CMD_MASTERCLAP_OCCURRED);

}









void messaging::receiveClapTimes(uint8_t *senderAddress) {
    if (memcmp(senderAddress, clapDeviceAddress, 6) == 0) {
        addError("receive Clap times from clap device", false);
        memcpy(&clapDevice.clapTimes, &sendClapTimes, sizeof(clapDevice.clapTimes));
        if (addressCounter > 0) {
            getClapTimes(0);
        }
        else {
            globalModeHandler->switchMode(MODE_NEUTRAL);    
        }
        
        addError("from within receiveClapTimes", false);
        addError(printClapTimes(clapDevice.clapTimes.timeStamp, NUM_CLAPS));
        addError(printClapTimes(myClapTimes.timeStamp, NUM_CLAPS));
        timeoutRetry.currentId = 0;
        timeoutRetry.tries = 0;
        timeoutRetry.unavailableCounter = 0;
        Serial.println(5);
        calculateDistances(-1);
    }
    else {
        int id = getAddressId(senderAddress);
        addError("receive Clap times from  device "+String(id), false);
        if (id != -1) {
            memcpy(&clientAddresses[id].clapTimes, &sendClapTimes, sizeof(clientAddresses[id].clapTimes));
            clientAddresses[id].active = ACTIVE;
            for (int j=0; j<clientAddresses[id].clapTimes.clapCounter; j++) {
                addError("Clap: "+String(clientAddresses[id].clapTimes.timeStamp[j])+"\n");
            }
            if (id<=addressCounter) {
                calculateDistances(id);
            }
            else if (id == addressCounter) {
            }
            if (timeoutRetry.currentId <= addressCounter) {
                timeoutRetry.currentId++;
                timeoutRetry.tries = 0;
                getClapTimes(timeoutRetry.currentId);
            }
            else {
                writeStructsToFile(clientAddresses, NUM_DEVICES, "/clientAddress");
                addError("writing structs", false);
                globalModeHandler->switchMode(MODE_NEUTRAL);
            }

        }
    }
}
void messaging::startCalibrationMode() {
    globalModeHandler->switchMode(MODE_CALIBRATE);
    pushDataToSendQueue(broadcastAddress, MSG_COMMANDS, CMD_START_CALIBRATION_MODE);
}

String messaging::printClapTimes(unsigned long long* array, int size) {
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

void messaging::clientAddressToJsonObject(JsonObject& jsonObj, client_address& client) {
  JsonArray addressArray = jsonObj["address"].to<JsonArray>();
  for (int i = 0; i < 6; i++) {
    addressArray.add(client.address[i]);
  }

  jsonObj["id"] = client.id;
  jsonObj["xLoc"] = client.xLoc;
  jsonObj["yLoc"] = client.yLoc;
  jsonObj["zLoc"] = client.zLoc;
  jsonObj["timerOffset"] = client.timerOffset;
  jsonObj["delay"] = client.delay;
  jsonObj["distanceFromCenter"] = client.distanceFromCenter;

  JsonObject clapTimes = jsonObj["clapTimes"].to<JsonObject>();
  clapTimes["messageType"] = client.clapTimes.messageType;
  clapTimes["clapCounter"] = client.clapTimes.clapCounter;
  JsonArray timeStampArray = clapTimes["timeStamp"].to<JsonArray>();
  for (int i = 0; i < NUM_CLAPS; i++) {
    timeStampArray.add(client.clapTimes.timeStamp[i]);
  }

  JsonArray distancesArray = jsonObj["distances"].to<JsonArray>();
  for (int i = 0; i < NUM_CLAPS; i++) {
    distancesArray.add(client.distances[i]);
  }

  jsonObj["active"] = client.active == ACTIVE ? "ACTIVE" : "INACTIVE";
  jsonObj["batteryPercentage"] = client.batteryPercentage;
  jsonObj["tries"] = client.tries;
}

// Function to convert all clientAddresses to a JSON string
String messaging::allClientAddressesToJson() {
  // Create a JSON document
  JsonDocument doc; // Adjust size as necessary

  // Create a JSON array
  JsonArray addressesArray = doc["clientAddresses"].to<JsonArray>();
  JsonArray clapDeviceArray = doc["clapDevice"].to<JsonArray>();
  JsonObject clientObj = clapDeviceArray.add<JsonObject>();
  clientAddressToJsonObject(clientObj, clapDevice);
  JsonArray myArray = doc["hostDevice"].to<JsonArray>();
  JsonObject mainObj = myArray.add<JsonObject>();
  JsonArray addressArray = mainObj["address"].to<JsonArray>();
  for (int i = 0; i < 6; i++) {
    addressArray.add(myAddress[i]);
  }
  JsonObject clapTimes = mainObj["clapTimes"].to<JsonObject>();
  clapTimes["messageType"] = myClapTimes.messageType;
  clapTimes["clapCounter"] = myClapTimes.clapCounter;
  mainObj["batteryPercentage"] = getBattery();
  JsonArray timeStampArray = clapTimes["timeStamp"].to<JsonArray>();
  
  for (int i = 0; i < NUM_CLAPS; i++) {
    timeStampArray.add(myClapTimes.timeStamp[i]);
  }
  JsonArray distancesArray = mainObj["distances"].to<JsonArray>();
  for (int i = 0; i < NUM_CLAPS; i++) {
    distancesArray.add(myDistances[i]);
  }
  // Populate the JSON array with client addresses
  for (int i = 0; i < NUM_DEVICES; i++) {
    if (memcmp(clientAddresses[i].address, emptyAddress, 6) == 0) {
        break;
    }
    JsonObject clientObj = addressesArray.add<JsonObject>();
    clientAddressToJsonObject(clientObj, clientAddresses[i]);
  }

  // Serialize JSON document to string
  String output;
  serializeJson(doc, output);
//Serial.println(output);
  return output;
}

void messaging::updateDevice(int id) {
    commandMessage.messageType = CMD_GET_STATUS;
    pushDataToSendQueue(clientAddresses[id].address, MSG_COMMANDS, -1);
}


void messaging::setTimerReceiver(const uint8_t * incomingData) {
    memcpy(&addressMessage,incomingData,sizeof(addressMessage));
    globalModeHandler->setPreviousMode();
    globalModeHandler->switchMode( MODE_SENDING_TIMER);
    addError("setting timer receiver to "+stringAddress(addressMessage.address));
    if (memcmp(&addressMessage.address, clapDeviceAddress, 6) == 0) {
        addError("setting timer receiver to clap device");
        memcpy(&timerReceiver, addressMessage.address, 6);
        addPeer(timerReceiver);
        return;
    }
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (memcmp(&clientAddresses[i].address, emptyAddress, 6) == 0) {
            //printAddress(addressMessage.address);
            memcpy(&clientAddresses[i].address, addressMessage.address, 6);
            clientAddresses[i].id = i;
            memcpy(&timerReceiver, addressMessage.address, 6);
            addPeer(timerReceiver);
            timerMessage.addressId = i;
            addressCounter++;
            webServer->updateDeviceNum();
            handleLed->flash(0, 125, 0, 100, 2, 50);
            break;
        }
        else if (memcmp(&clientAddresses[i].address, addressMessage.address, 6) == 0) {
            memcpy(&timerReceiver, addressMessage.address, 6);
            timerMessage.addressId = i;
            addPeer(timerReceiver);
            break;
        }
    }
}
void messaging::handleGotTimer(const uint8_t * incomingData, uint8_t * macAddress) {
    addError("Handling Got Timer\n");
    memcpy(&gotTimerMessage, incomingData, sizeof(gotTimerMessage));
    addError("Arrive time"+ String(micros()), false);
    addError("SendTime"+String(gotTimerMessage.sendTime), false);
    addError("Offset "+String(gotTimerMessage.timerOffset), false);
    addError("Delay "+String(gotTimerMessage.delayAvg), false);
    unsigned long long expect = gotTimerMessage.sendTime+gotTimerMessage.delayAvg+gotTimerMessage.timerOffset;
    addError("expected "+String(expect));
    addError("Difference "+String(micros()-expect));

    if (memcmp(timerReceiver, clapDeviceAddress, 6) != 0) {
        removePeer(timerReceiver);
        int addressId = getAddressId(macAddress);
        addError("Delay "+String(clientAddresses[addressId].delay=gotTimerMessage.delayAvg));
        clientAddresses[addressId].delay=gotTimerMessage.delayAvg;
        clientAddresses[addressId].timerOffset = gotTimerMessage.timerOffset;
        clientAddresses[addressId].active = ACTIVE;
        clientAddresses[addressId].batteryPercentage = gotTimerMessage.batteryPercentage;
        
    }
    else {
        clapDevice.delay=gotTimerMessage.delayAvg;
        clapDevice.timerOffset = gotTimerMessage.timerOffset;
    }
    addError("sending client addresses to JSON");
    updateAddressesToWebserver();
    timerCounter = 0;
    lastDelay = 0;
    if (globalModeHandler->getMode() == MODE_RESET_TIMER) {
        if (timersUpdated < addressCounter) {
            timersUpdated++;
            if (timersUpdated == addressCounter) {
                timeoutRetry.currentId = addressCounter;
                addError("reverting to previous mode "+String(globalModeHandler->getPreviousMode()));
                globalModeHandler->revertToPreviousMode();

            }
            else {
                addError("timersupdated "+String(timersUpdated), false);
                addError("address counter "+String(addressCounter), false);
            }

            addError("Timers Updated increase to "+String(timersUpdated));
            handleTimerUpdates();
            return;
        }
    }
    Serial.println("got timer");
    globalModeHandler->revertToPreviousMode();
    globalModeHandler->switchMode(MODE_NEUTRAL);
    timersUpdated = addressCounter;
    addError("Writing Struct to file\n");
    writeStructsToFile(clientAddresses, NUM_DEVICES, "/clientAddress");
}
void messaging::updateAddressesToWebserver() {
    //addError(allClientAddressesToJson()+"\n");
        webServer->events.send(allClientAddressesToJson().c_str(), "updateDeviceList");

}

void messaging::confirmClap(int id, float xpos, float ypos, float zpos) {
    addError("ConfirmClap id "+String(id)+"\n");

    clapDeviceLocations[id].xLoc = xpos;
    clapDeviceLocations[id].yLoc = ypos;
    clapDeviceLocations[id].zLoc = zpos;   
    pushDataToSendQueue(broadcastAddress, MSG_COMMANDS, CMD_END_CALIBRATION_MODE); 
}


void messaging::resetCalibration() {
    addError("Resetting Calibration\n");
    for (int i = 0; i < NUM_DEVICES; i++) {
        clientAddresses[i].xLoc = 0;
        clientAddresses[i].yLoc = 0;
        clientAddresses[i].zLoc = 0;
        memset(&clientAddresses[i].clapTimes, 0, sizeof(clientAddresses[i].clapTimes));
        memset(&clientAddresses[i].distances, 0, sizeof(clientAddresses[i].distances));
    }
    memset (&clapDevice.clapTimes, 0, sizeof(clapDevice.clapTimes));
    memset (&clapDeviceLocations, 0, sizeof(clapDeviceLocations));
    memset (&myClapTimes, 0, sizeof(myClapTimes));
    memset (&myDistances, 0, sizeof(myDistances));
    pushDataToSendQueue(broadcastAddress, MSG_COMMANDS, CMD_RESET_CALIBRATION);
    globalModeHandler->switchMode(MODE_NEUTRAL);
}

void messaging::setWakeup(int hours, int minutes, int seconds){
    wakeupTime.hours = hours;
    wakeupTime.minutes = minutes;
    wakeupTime.seconds = seconds;
}
void messaging::setGoodNight(int hours, int minutes, int seconds){
    sleepTime.hours = hours;
    sleepTime.minutes = minutes;
    sleepTime.seconds = seconds;
    int * time = getSystemTime();
    goToSleepTime = (int)calculateTimeDifference(time[5], time[4], time[3], time[0], time[1], time[2], sleepTime.hours, sleepTime.minutes, sleepTime.seconds);
    goToSleepTime *=1000;
    goToSleepTime += millis();
    Serial.println("Setting good night");
    Serial.println("Hours"+String(sleepTime.hours));
    Serial.println("Minutes "+String(sleepTime.minutes));
    Serial.println("System Hours "+String(time[0]));
    Serial.println("System minutes "+String(time[1]));
    Serial.println("now millis "+String(millis()));
    Serial.println("gotosleeptime "+String(goToSleepTime));
    Serial.println("diff "+String(goToSleepTime-millis()));

}

void messaging::broadcastTimer() {
    
    globalModeHandler->switchMode(MODE_BROADCAST_TIMER);
    setTimerCounter(0);

}

void messaging::resetTimer() {
    timersUpdated = 0;
    timeoutRetry.currentId = 0;
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (memcmp(clientAddresses[i].address, emptyAddress, 6) != 0) {
            addError("Found address at index "+String(i)+"\n");
            addError(stringAddress(clientAddresses[i].address));
            clientAddresses[i].clapTimes.clapCounter = 0;
            clientAddresses[i].delay = 0;
            clientAddresses[i].timerOffset = 0;
            clientAddresses[i].active = INACTIVE;
        }
        else {
            addError("We have "+String(addressCounter)+" addresses\n"); 
            return;
        }
    }
}
void messaging::handleTimerUpdates() {
if (millis() > tick+10000) {
    addError("Handling Timer Updates\n");
    addError("timeout retry current id "+String(timeoutRetry.currentId), false);
    addError("address counter "+String(addressCounter), false);
    addError("timers updated "+String(timersUpdated), false);
    tick = millis();
}
 if (timersUpdated == addressCounter)  {  
    return;
 }

 if (timeoutRetry.currentId < addressCounter) {
    for (int i = timeoutRetry.currentId; i < addressCounter; i++) {
        //darf natürlich nicht weiter gehen. entweder hier noch mit status check oder die ganze funktion nur alle sekunde aufrufen
        if (clientAddresses[i].active == INACTIVE or clientAddresses[i].active == UNREACHABLE) {
            addError("Updating timers for device "+String(i)+"\n");
            timeoutRetry.currentId = i;
            updateTimers(i);
            return;
        }
    }
 }

}

void messaging::updateTimers(int addressId) {
    if (globalModeHandler->getMode() != MODE_RESET_TIMER) {
        addError("setting preivious mode to "+globalModeHandler->modeToText(globalModeHandler->getMode()));
        globalModeHandler->setPreviousMode();
    }
    globalModeHandler->switchMode(MODE_RESET_TIMER);
    addError("Updating timers for address "+String(addressId)+"\n");
    addError("timeout retry currentide "+String(timeoutRetry.currentId), false);
    addError("address counter "+String(addressCounter), false);
    addError("timers updated "+String(timersUpdated), false);
    memcpy(&timerReceiver, clientAddresses[addressId].address, 6);
    addPeer(timerReceiver);
    timerMessage.reset = true;
    timerMessage.addressId = addressId;
    timerMessage.counter = 0;
    clientAddresses[addressId].active = SETTING_TIMER;
}


int messaging::getTimeoutRetryId() {
    return timeoutRetry.currentId;
}

void messaging::startAnimation() {
    Serial.println("start animation called");
    animationMessage.animationType = SYNC_ASYNC_BLINK;
    handleLed->getNextAnimation(&animationMessage);
    animationMessage.startTime = micros()+1000000;
    animationMessage.animationreps = 1;
    animationMessage.num_devices = addressCounter;
    Serial.println("Starting animation with num devices "+String(animationMessage.num_devices));
    pushDataToSendQueue(broadcastAddress, MSG_ANIMATION, -1);
    nextAnimationPing = millis()+handleLed->calculate(&animationMessage);
    Serial.println("Now: "+String(millis()));
    Serial.println("Animation ends at "+String(nextAnimationPing));
    globalModeHandler->switchMode(MODE_ANIMATE);
}
 
void messaging::nextAnimation() {
    if (nextAnimationPing == 0) {
        Serial.println("next animation ping 0");
        delay(1000);
        return;
    }
    if (millis() < nextAnimationPing) {
        if (millis() % 1000 == 0) {
            delay(1);
            Serial.println("Next animation in "+String((nextAnimationPing-millis())/1000)+" seconds, nextanimationping = "+String(nextAnimationPing));
        }
        return;
    }
    if (millis() > nextAnimationPing) {
        if (millis() > lastBroadcastTimer+BROADCAST_TIMER_FREQ*1000) {
            Serial.println("calling broadcast timer");
            broadcastTimer();

        }
        else {
            Serial.println("lastBroadcasttimer is "+String(lastBroadcastTimer)+" and timerfreq "+String(BROADCAST_TIMER_FREQ*1000)+" so difference "+String(millis()-lastBroadcastTimer+BROADCAST_TIMER_FREQ*1000));
        }
        if (finishAnimation == true) {
            Serial.println("We want to end the animations or want to ");
            globalModeHandler->switchMode(MODE_NEUTRAL);
        }
        else {
            handleLed->getNextAnimation(&animationMessage);
            handleLed->printStatus();
            animationMessage.startTime = micros()+1000000;
            nextAnimationPing = millis()+handleLed->calculate(&animationMessage);
            Serial.println("Next animation ping "+String(nextAnimationPing));
            Serial.println("now = "+String(millis()));
            pushDataToSendQueue(broadcastAddress, MSG_ANIMATION, -1);
        }
    }
}

void messaging::endAnimation() {
    pushDataToSendQueue(broadcastAddress, MSG_COMMANDS, CMD_END_ANIMATION);
    nextAnimationPing = 0;
    globalModeHandler->switchMode(MODE_NEUTRAL);
}
bool messaging::allTimersUpdated() {
    if (timersUpdated == addressCounter) {
        return true;
    }  
    else {
        return false;
    }
}

float messaging::largestDistance() {
    float maxDistance = 0.0;
    for (int i = 0; i < addressCounter; i++ ) {
        if (clientAddresses[i].distanceFromCenter > maxDistance) {
            maxDistance = clientAddresses[i].distanceFromCenter;
        }
    }
    return maxDistance;
}

void messaging::resetSystem() {
    pushDataToSendQueue(broadcastAddress, MSG_COMMANDS, CMD_RESET_SYSTEM);
}
unsigned long long messaging::getLastBroadcastTimer() {
    return lastBroadcastTimer;
}
void messaging::setLastBroadcastTimer() {
    lastBroadcastTimer = millis();
}

void messaging::printPeers() {

      esp_now_peer_num_t peerNum;
  if (esp_now_get_peer_num(&peerNum) == ESP_OK) {
    Serial.print("Number of registered peers: ");
    Serial.println(peerNum.total_num);

    esp_now_peer_info_t peerInfo;
    esp_err_t result;

    esp_now_peer_info_t fetchedPeerInfo;

    // Start from the head of the peer list
    bool from_head = true;

    // Fetch peer information
    while (esp_now_fetch_peer(from_head, &fetchedPeerInfo) == ESP_OK) {
        Serial.println("Peer information fetched successfully.");
        Serial.print("Peer MAC Address: ");
        for (int i = 0; i < 6; i++) {
            Serial.print(fetchedPeerInfo.peer_addr[i], HEX);
            if (i < 5) Serial.print(":");
        }
        Serial.println();
        Serial.print("Channel: ");
        Serial.println(fetchedPeerInfo.channel);
        Serial.print("Encrypt: ");
        Serial.println(fetchedPeerInfo.encrypt ? "true" : "false");
        
        // Set from_head to false to fetch the next peer
        from_head = false;
    }
  } else {
    Serial.println("Failed to get number of peers");
  }
}

void messaging::setGlobalBrightness(int brightness) {
    handleLed->setGlobalBrightness(brightness);
}

String messaging::getLedHandlerParams() {
    return handleLed->getParamsJson();
}
void messaging::setSyncAsyncParams(int minS, int maxS, int minP, int maxP, int minSp, int maxSp, int minR, int maxR, int minAR, int maxAR, int minRGBR, int maxRGBR, int minRGBG, int maxRGBG, int minRGBB, int maxRGBB) {
    Serial.println("setting sync async params");
    Serial.println("minS "+String(minS)+" maxS "+String(maxS)+" minP "+String(minP)+" maxP "+String(maxP)+" minSp "+String(minSp)+" maxSp "+String(maxSp)+" minR "+String(minR)+" maxR "+String(maxR)+" minAR "+String(minAR)+" maxAR "+String(maxAR)+" minRGBR "+String(minRGBR)+" maxRGBR "+String(maxRGBR)+" minRGBG "+String(minRGBG)+" maxRGBG "+String(minRGBG)+" minRGBB "+String(minRGBB)+" maxRGBB "+String(maxRGBB));
    handleLed->setSyncAsyncParams(minS, maxS, minP, maxP, minSp, maxSp, minR, maxR, minAR, maxAR, minRGBR, maxRGBR, minRGBG, maxRGBG, minRGBB, maxRGBB);

}

void messaging::sendMidi(uint8_t note, uint8_t velocity) {
    midiMessage.note = note;
    midiMessage.velocity = velocity;
    Serial.println("sending midi note "+String(note)+" velocity "+String(velocity));
    esp_now_send(broadcastAddress, (uint8_t*) &midiMessage, sizeof(midiMessage));
}