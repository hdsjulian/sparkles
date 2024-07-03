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
}





void messaging::handleReceive(const esp_now_recv_info * mac, const uint8_t *incomingData, int len, unsigned long msgReceiveTime) {
    addError("Handling Received ");
    addError(messageCodeToText(incomingData[0]));
    addError(" from ");
    addError(stringAddress(mac->src_addr));
    addError("\n");
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
        default: 
            addError("message not recognized");
            addError(messageCodeToText(incomingData[0]));
            addError("\n");

            break;
    }
}


//this is the test version
void messaging::calculateDistances(int id) {

    orderClaps(id);
    if (id > -1) {
        for (int i = 0; i<clientAddresses[id].clapTimes.clapCounter; i++) {
            if (clientAddresses[id].clapTimes.timeStamp[i] == 0) {continue;}
            else {
                int timeDifference = clientAddresses[id].clapTimes.timeStamp[i]-clapDevice.clapTimes.timeStamp[i];
                clientAddresses[id].distances[i] = 34300*(timeDifference/1000000);
            }   
        }
    }
    else {
        Serial.println("calculating distances");
        Serial.println(printClapTimes(myClapTimes.timeStamp, NUM_CLAPS));
        for (int i = 0; i<myClapTimes.clapCounter; i++) {
            if (clientAddresses[id].clapTimes.timeStamp[i] == 0) {continue;}
            else {
                int timeDifference = myClapTimes.timeStamp[i]-clapDevice.clapTimes.timeStamp[i];
                myDistances[i] = 34300*(timeDifference/1000000);
        }   
    }
    }
    Serial.println("---calculate distances---");
    updateAddressesToWebserver();

}


void messaging::orderClaps(int id) {
    addError("orderClaps 1\n");
    message_send_clap_times newClapTimes;
    memset(&newClapTimes, 0, sizeof(newClapTimes));
    addError("orderClaps 2\n");
   if (id == -1) {
    addError("orderclaps 3 = num claps "+String(clapDevice.clapTimes.clapCounter)+"\n");
        for (int i = 0; i < clapDevice.clapTimes.clapCounter; i++) {
            addError("clap "+String(i)+"\n");
            if (clapDevice.clapTimes.timeStamp[i] == 0) {addError(String(i)+" is zero\n"); return;}
            for (int j = 0; j<myClapTimes.clapCounter; j++) {
                addError("j "+String(j)+"\n");
                if (myClapTimes.timeStamp[j] == 0) {addError(String(j)+" is zero\n");break;}
                if (myClapTimes.timeStamp[j] <= clapDevice.clapTimes.timeStamp[i]+500000 or myClapTimes.timeStamp[j]>=clapDevice.clapTimes.timeStamp[i] -500000 ) {
                    addError("fits"+String(myClapTimes.timeStamp[j])+"\n");
                        newClapTimes.timeStamp[j] = myClapTimes.timeStamp[j];
                        break;
                }
                else if (myClapTimes.timeStamp[j]<=clapDevice.clapTimes.timeStamp[i]+500000) {
                    addError("smaller\n");
                }
                else if (myClapTimes.timeStamp[j]>=clapDevice.clapTimes.timeStamp[i] -500000 ) {
                    addError("larger\n");
                }
                else {
                    addError("neither" + String(myClapTimes.timeStamp[j])+" - "+String(clapDevice.clapTimes.timeStamp[i])+"\n");
                }
            }
        }
        memcpy(&myClapTimes, &newClapTimes, sizeof(newClapTimes));
        addError("FROM ORDERCLAPS\n");
        addError(printClapTimes(myClapTimes.timeStamp, NUM_CLAPS));
    }
    else {
        for (int i = 0; i < clapDevice.clapTimes.clapCounter; i++) {
            if (clapDevice.clapTimes.timeStamp[i] == 0) {return;}
            for (int j = 0; j<clientAddresses[id].clapTimes.clapCounter; j++) {
                if (clientAddresses[id].clapTimes.timeStamp[j] == 0) {break;}
                    if (clientAddresses[id].clapTimes.timeStamp[j] < clapDevice.clapTimes.timeStamp[i]+500000 and clientAddresses[id].clapTimes.timeStamp[j]>clapDevice.clapTimes.timeStamp[i] -500000 ) {
                        newClapTimes.timeStamp[j] = clientAddresses[id].clapTimes.timeStamp[j];
                        break;
                }
            }
        }
        memset(&clientAddresses[id].clapTimes, 0, sizeof(clientAddresses[id].clapTimes));
        memcpy(&clientAddresses[id].clapTimes, &newClapTimes, sizeof(newClapTimes));
    }
}
void messaging::triangulateDistances(int id) {
    // todoclapdevice claptimes
    Serial.println("Calculating distances");
    addError("CalculatingDistances\n");
    //filterClaps(0);
    //go through all devices

    //filterClaps(i);
    //initialize cumulative distance value and clap counter
    int cumul = 0;
    int clapCount = 0;
    //initialize bools for "this clap was found in the other device"
    bool countmeCD = false;
    bool countmeMain = false;
    //initialize last clap index for webserver and main device so that i don't have to start the loops again
    int lastClapDeviceClap = 0;
    int lastMainClap = 0;

    //output some stuff
    addError("Device: "+String(id)+"\n");
    addError("Clap Counter: "+String(clientAddresses[id].clapTimes.clapCounter)+"\n");
    //if there are no claps, lets just move on
    if (clientAddresses[id].clapTimes.clapCounter == 0) { addError("No claps detected\n"); return; }
    //iterate through the claps of the device
    addError("claps for device "+String(id)+"\n");
    for (int j = 0 ; j < clientAddresses[id].clapTimes.clapCounter; j++) {
        addError("Clap: "+String(j)+" at Time "+String(clientAddresses[id].clapTimes.timeStamp[j])+"\n");
        //iterate through the webserver/s claps
        addError("claps for webserver "+String(clapDevice.clapTimes.clapCounter)+"\n");
        for (int k = lastClapDeviceClap; k < clapDevice.clapTimes.clapCounter; k++) {
            //calculate the difference between the claps on the webserver and the device
            //addError("Clap: "+String(k)+" at Time "+String(webserverClapTimes.timeStamp[k])+"\n");
            unsigned long timeStampDifference = (clientAddresses[id].clapTimes.timeStamp[j] > clapDevice.clapTimes.timeStamp[k]) ?
                                            (clientAddresses[id].clapTimes.timeStamp[j] - clapDevice.clapTimes.timeStamp[k]) :
                                            (clapDevice.clapTimes.timeStamp[k] - clientAddresses[id].clapTimes.timeStamp[j]);
            //if the difference is less than a second we count the clap                                                
            if (timeStampDifference < CLAP_THRESHOLD) {
                //addError("Web should count\n");
                countmeCD = true;
                //set the index so that the next iteration starts appropriately
                lastClapDeviceClap = k-1;
                break;
            }
            //if we have iterated too far, we break the loop. the device's clap could not be found on the webserver. false positive.
            if (clientAddresses[id].clapTimes.timeStamp[j]+CLAP_THRESHOLD < clapDevice.clapTimes.timeStamp[k]) {
                //addError("Web didn't count\n");
                lastClapDeviceClap = k-1;
                break;
            }
        }
        //do the same for the host device
        //addError("Claps for main "+String(clientAddresses[0].clapTimes.clapCounter)+"\n");
        for (int k = lastMainClap; k < clientAddresses[0].clapTimes.clapCounter; k++) {
            // addError("Clap: "+String(k)+" at Time "+String(clientAddresses[0].clapTimes.timeStamp[k])+"\n");
            unsigned long timeStampDifference = (clientAddresses[id].clapTimes.timeStamp[j] > clientAddresses[0].clapTimes.timeStamp[k]) ?
                                            (clientAddresses[id].clapTimes.timeStamp[j] - clientAddresses[0].clapTimes.timeStamp[k]) :
                                            (clientAddresses[0].clapTimes.timeStamp[k] - clientAddresses[id].clapTimes.timeStamp[j]);
            if (timeStampDifference < CLAP_THRESHOLD) {
                // addError("Main should count\n");
                countmeMain = true;
                lastMainClap = k-1;
                break;
            }
            if (clientAddresses[id].clapTimes.timeStamp[j]+CLAP_THRESHOLD < clientAddresses[0].clapTimes.timeStamp[k]) {
                //addError("Main didn't count\n");
                lastMainClap = k-1;
                countmeMain = false;
                break;
            }
        }
        //for all claps that were found on all three devices devices, calculate the difference and add it to the cumulative distance
        if (countmeCD and countmeMain) {
            clapCount++;
            
            unsigned long timeStampDifference = (clientAddresses[0].clapTimes.timeStamp[lastMainClap+1] > clientAddresses[id].clapTimes.timeStamp[j]) ?
                                            (clientAddresses[0].clapTimes.timeStamp[lastMainClap+1] - clientAddresses[id].clapTimes.timeStamp[j]) :
                                            (clientAddresses[id].clapTimes.timeStamp[j] - clientAddresses[0].clapTimes.timeStamp[lastMainClap+1]);
            cumul += timeStampDifference;
            addError("Adding timeStampDifference: "+String(timeStampDifference)+"\n");
        }
    }
        //make sure i don't divide by zero, then divide.
    if( cumul > 0 and clapCount != 0) {
        float dist = (float)((float)cumul/clapCount);
        dist = 34300*(dist/1000000);
        Serial.println("dist"+String(dist));
        clientAddresses[id].distances[0] = dist;
        distanceMessage.distance = dist;
        pushDataToSendQueue(clientAddresses[id].address, MSG_DISTANCE, -1);
        addError("Distance calculated"+String(clientAddresses[id].distances[0])+" Centimeters\n");
    }
    else {
        //if there are no claps, or the claps are too far apart, set the distance to 0
        addError("No distances found\n");
        clientAddresses[id].distances[0] = 0;
    }

    Serial.println("Done Calculating distances");
}

void messaging::receiveClapTimes(const esp_now_recv_info * mac) {
    if (memcmp(mac->src_addr, clapDeviceAddress, 6) == 0) {
        memcpy(&clapDevice.clapTimes, &sendClapTimes, sizeof(clapDevice.clapTimes));
        getClapTimes(0);
        addError("from within receiveClapTimes");
        addError(printClapTimes(clapDevice.clapTimes.timeStamp, NUM_CLAPS));
        timeoutRetry.currentId = 0;
        timeoutRetry.tries = 0;
        timeoutRetry.unavailableCounter = 0;
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
                Serial.println("writing structs");
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
  Serial.println("battery percentage requested "+String(getBattery()));
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
  Serial.println(output);
  serializeJson(doc, output);

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
        clientAddresses[addressId].delay=gotTimerMessage.delayAvg;
        clientAddresses[addressId].timerOffset = gotTimerMessage.timerOffset;
        clientAddresses[addressId].active = ACTIVE;
        clientAddresses[addressId].batteryPercentage = gotTimerMessage.batteryPercentage;
        
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
    Serial.println("Cap Times");
    Serial.println(printClapTimes(myClapTimes.timeStamp, NUM_CLAPS));
        webServer->events.send(allClientAddressesToJson().c_str(), "updateDeviceList");

}