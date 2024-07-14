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
                return;
            }
        }
        addError("Address counter: "+String(addressCounter)+"\n");
    
    }
    int * time;
    time = getSystemTime();
    goToSleepTime = (int)calculateTimeDifference(time[5], time[4], time[3], time[0], time[1], time[2], sleepTime.hours, sleepTime.minutes, sleepTime.seconds)*1000;
}





void messaging::handleReceive(const esp_now_recv_info * mac, const uint8_t *incomingData, int len, unsigned long msgReceiveTime) {
    addError("Handling Received "+String(incomingData[0]), false);
    //Serial.println(messageCodeToText(incomingData[0]));
    addError(" from ");
    if (mac!=NULL) {
        addError(stringAddress(mac->src_addr));
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
            else {
                setTimerReceiver(incomingData);
            }
            break;

        case MSG_GOT_TIMER:
            handleGotTimer(incomingData, mac->src_addr);
            break; 
        case MSG_SEND_CLAP_TIMES:
        Serial.println("1");
            memcpy(&sendClapTimes, incomingData, sizeof(sendClapTimes));
            
            receiveClapTimes(mac);
        break;

        /*
        case MSG_ANIMATION:
            addError("Animation Message Incoming\n");
            if (DEVICE_MODE == MAIN and memcmp(mac->src_addr, clapDeviceAddress, 6) == 0) {
                memcpy(&animationMessage, incomingData, sizeof(animationMessage));
                animationMessage.startTime = micros()+3000000;
                animationMessage.animationreps = 1;
                animationMessage.num_devices = addressCounter -1;
                Serial.println("Starting animation with num devices "+String(animationMessage.num_devices));
                pushDataToSendQueue(broadcastAddress, MSG_ANIMATION, -1);
                nextAnimationPing = millis()+handleLed->calculate(&animationMessage);
                Serial.println("Animation ends at "+String(millis()));
                globalModeHandler->switchMode(MODE_ANIMATE);
                //endAnimation = true;
            }  
            else {
                if (globalModeHandler->getMode() == MODE_ANIMATE or globalModeHandler->getMode() == MODE_NEUTRAL) {
                addError("Blinking\n");
                globalModeHandler->switchMode(MODE_ANIMATE);
                memcpy(&animationMessage, incomingData, sizeof(animationMessage));
                handleLed->setupAnimation(&animationMessage);
                nextAnimationPing = millis()+handleLed->calculate(&animationMessage);
                Serial.println("setting endanimation to true");
                //endAnimation = true;
                }
            }
        break;
        */
        case MSG_STATUS: {
            memcpy(&statusMessage, incomingData, sizeof(statusMessage));
            addError("Battery Status: "+String(statusMessage.batteryPercentage)+"\n");
            int id = getAddressId(mac->src_addr);
            clientAddresses[id].batteryPercentage = statusMessage.batteryPercentage;
                Serial.println("---msgstatus---");

            updateAddressesToWebserver();
            
        break;    
        }
        case MSG_SEND_SINGLE_CLAP:
            addError("Got a single clap message at "+String(micros())+"\n");
            memcpy(&sendSingleClapMessage, incomingData, sizeof(sendSingleClapMessage));
            addError("Single clap timestamp "+String(sendSingleClapMessage.timeStamp)+"\n");
            addError("Clap Counter: "+String(sendSingleClapMessage.clapCounter)+"\n");
            addError("Difference "+String(micros() - sendSingleClapMessage.timeStamp)+"\n");
            clapDevice.clapTimes.timeStamp[sendSingleClapMessage.clapCounter] = sendSingleClapMessage.timeStamp;
            updateAddressesToWebserver();
            jsonString = "{\"status\" : \"2\", \"clapId\" : "+String(sendSingleClapMessage.clapCounter)+", \"timeStamp\" : "+String(sendSingleClapMessage.timeStamp)+"}";
            webServer->events.send(jsonString.c_str(), "clapCheck");
            globalModeHandler->switchMode(MODE_MASTERCLAP_OCCURRED);
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














void messaging::receiveClapTimes(const esp_now_recv_info * mac) {
    Serial.println(2);
    if (memcmp(mac->src_addr, clapDeviceAddress, 6) == 0) {
        Serial.println(3);
        memcpy(&clapDevice.clapTimes, &sendClapTimes, sizeof(clapDevice.clapTimes));
        if (addressCounter > 0) {
            Serial.println(4);
            getClapTimes(0);
        }
        else {
            globalModeHandler->switchMode(MODE_NEUTRAL);    
        }
        
        addError("from within receiveClapTimes");
        addError(printClapTimes(clapDevice.clapTimes.timeStamp, NUM_CLAPS));
        addError(printClapTimes(myClapTimes.timeStamp, NUM_CLAPS));
        timeoutRetry.currentId = 0;
        timeoutRetry.tries = 0;
        timeoutRetry.unavailableCounter = 0;
        Serial.println(5);
        calculateDistances(-1);
    }
    else {
        int id = getAddressId(mac->src_addr);
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
    pushDataToSendQueue(broadcastAddress, MSG_COMMANDS, CMD_START_CALIBRATION_MODE);
   
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
        Serial.println("breaking for "+String(i));
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

void messaging::handleGotTimer(const uint8_t * incomingData, uint8_t * macAddress) {
    addError("Handling Got Timer\n");
    memcpy(&gotTimerMessage, incomingData, sizeof(gotTimerMessage));
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
    if (globalModeHandler->getMode() == MODE_RESET_TIMER) {
        if (timersUpdated < addressCounter) {
            timersUpdated++;
            globalModeHandler->switchMode(MODE_NEUTRAL);
            handleTimerUpdates();
            return;
        }
    }
    Serial.println("got timer");
    globalModeHandler->switchMode(MODE_NEUTRAL);
    timersUpdated = addressCounter;
    timerCounter = 0;
    lastDelay = 0;
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
    goToSleepTime = millis()+(int)calculateTimeDifference(time[5], time[4], time[3], time[0], time[1], time[2], sleepTime.hours, sleepTime.minutes, sleepTime.seconds)*1000;
    Serial.println("Setting good night");
    Serial.println("Hours"+String(sleepTime.hours));
    Serial.println("Minutes "+String(sleepTime.minutes));
    Serial.println("System Hours "+String(time[0]));
    Serial.println("System minutes "+String(time[1]));
    Serial.println("now millis "+String(millis()));
    Serial.println("gotosleeptime "+String(goToSleepTime));
    Serial.println("diff "+String(goToSleepTime-millis()));

}
