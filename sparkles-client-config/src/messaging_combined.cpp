
#include "Arduino.h"

#include "esp_now.h"
#include "WiFi.h"
#include "messaging.h"


messaging::messaging() {
};

void messaging::removePeer(uint8_t address[6]) {
        addError("REMOVING PEER");
        addError(stringAddress(address)+"\n");
        if (esp_now_del_peer(address) != ESP_OK) {
        addError("coudln't delete pee\n");

        return;
    }
}
void messaging::printAddress(const uint8_t * mac_addr){
    if (memcmp(mac_addr, hostAddress, 6) == 0) {
        Serial.println("HOST ADDRESS");
        return;
    }
    if (memcmp(mac_addr, clapDeviceAddress, 6) ==0) {
        Serial.println("CLAP DEVICE");
        return;
    }
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    Serial.println(macStr);
}

String messaging::stringAddress(const uint8_t * mac_addr){
    
    if (memcmp(mac_addr, hostAddress, 6) == 0) {
        return "HOST ADDRESS";
    }
    if (memcmp(mac_addr, clapDeviceAddress, 6) ==0) {
        return "CLAP DEVICE";

    }
    String macStr;
    macStr += String(mac_addr[0], HEX);
    macStr += ":";
    macStr += String(mac_addr[1], HEX);
    macStr += ":";
    macStr += String(mac_addr[2], HEX);
    macStr += ":";
    macStr += String(mac_addr[3], HEX);
    macStr += ":";
    macStr += String(mac_addr[4], HEX);
    macStr += ":";
    macStr += String(mac_addr[5], HEX);

    return macStr;
}


void messaging::printBroadcastAddress(){
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
            broadcastAddress[0], broadcastAddress[1], broadcastAddress[2], broadcastAddress[3], broadcastAddress[4], broadcastAddress[5]);
    Serial.println("blubl");
    Serial.println(macStr);
}




void messaging::blink() {
    //handleLed->flash(0, 0, 255, 200, 2, 50);
}
int messaging::getLastDelay() {
    return lastDelay;
}
void messaging::setLastDelay(int delay) {
    lastDelay = delay;
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

unsigned long messaging::getTimeOffset() {
    return timeOffset;
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


void messaging::printMessage(int message) { 
    Serial.println(messageCodeToText(message));
}
String messaging::messageCodeToText(int message) {
    String out = "";
    out += "message ";
 switch (message) {
    case MSG_ADDRESS:
        out = "MSG_ADDRESS";
        break;
    case MSG_ANNOUNCE:
        out = "MSG_ANNOUNCE";
        break;
    case MSG_TIMER_CALIBRATION:
        out = "MSG_TIMER_CALIBRATION";
        break;
    case MSG_GOT_TIMER:
        out = "MSG_GOT_TIMER";
        break;
    case MSG_ASK_CLAP_TIMES:
        out = "MSG_ASK_CLAP_TIMES";
        break;
    case MSG_SEND_CLAP_TIMES:
        out = "MSG_SEND_CLAP_TIMES";
        break;
    case MSG_SWITCH_MODE:
        out = "MSG_SWITCH_MODE";
        break;
    case MSG_DISTANCE:
        out = "MSG_DISTANCE";
        break;
    case MSG_SET_TIME:
        out = "MSG_SET_TIME";
        break;
    case MSG_ANIMATION:
        out = "MSG_ANIMATION";
        break;
    case MSG_NOCLAPFOUND:
        out = "MSG_NOCLAPFOUND";
        break;
    case MSG_COMMANDS:
        out = "MSG_COMMANDS";
        break;
    case MSG_ADDRESS_LIST:
        out = "MSG_ADDRESS_LIST";
        break;
    case MSG_STATUS_UPDATE:
        out = "MSG_STATUS_UPDATE";
        break;
    case MSG_END_CALIBRATION:
        out = "MSG_END_CALIBRATION";
        break;
    case MSG_WAKEUP:
        out = "MSG_WAKEUP";
        break;
    case MSG_SET_POSITIONS:
        out = "MSG_SET_POSITIONS";
        break;
    case MSG_STATUS:
        out = "MSG_STATUS";
        break;
    case MSG_SET_SLEEP_WAKEUP:
        out = "MSG_SET_SLEEP_WAKEUP";
        break;
    case MSG_SEND_SINGLE_CLAP:
        out = "MSG_SEND_SINGLE_CLAP";
        break;
    case MSG_TIMESYNC:
        out = "MSG_TIMESYNC";
        break;
    case CMD_START:
        out = "CMD_START";
        break;
    case CMD_MSG_SEND_ADDRESS_LIST:
        out = "CMD_MSG_SEND_ADDRESS_LIST";
        break;
    case CMD_START_CALIBRATION_MODE:
        out = "CMD_START_CALIBRATION_MODE";
        break;
    case CMD_END_CALIBRATION_MODE:
        out = "CMD_END_CALIBRATION_MODE";
        break;
    case CMD_BLINK:
        out = "CMD_BLINK";
        break;
    case CMD_MODE_NEUTRAL:
        out = "CMD_MODE_NEUTRAL";
        break;
    case CMD_GET_TIMER:
        out = "CMD_GET_TIMER";
        break;
    case CMD_START_ANIMATION:
        out = "CMD_START_ANIMATION";
        break;
    case CMD_STOP_ANIMATION:
        out = "CMD_STOP_ANIMATION";
        break;
    case CMD_DELETE_CLIENTS:
        out = "CMD_DELETE_CLIENTS";
        break;
    case CMD_TIMER_CALIBRATION:
        out = "CMD_TIMER_CALIBRATION";
        break;
    case CMD_GO_TO_SLEEP:
        out = "CMD_GO_TO_SLEEP";
        break;
    case CMD_RESET:
        out = "CMD_RESET";
        break;
    case CMD_RESET_SYSTEM:
        out = "CMD_RESET_SYSTEM";
        break;
    case CMD_GET_STATUS:
        out = "CMD_GET_STATUS";
        break;
    case CMD_GET_CLAP_TIMES:
        out = "CMD_GET_CLAP_TIMES";
        break;

    case CMD_MASTERCLAP_OCCURRED:
        out = "CMD_MASTERCLAP_OCCURRED";
        break;
    default:
        out = "UNKNOWN_MESSAGE";
        break;
}

    return out;
}
String messaging::getMessageLog() {
    String returnLog = messageLog;
    messageLog = "";
    return returnLog;
}
void messaging::addMessageLog(String message){
    messageLog +=message;
}



void messaging::printAllPeers() {
       // Get the peer list
    esp_now_peer_info_t peerList;
    Serial.println("Peer List:");
    for (int i = 0;i<2;i++) {
        if (i == 0) {
            esp_now_fetch_peer(true, &peerList);
        }
        else {
            esp_now_fetch_peer(true, &peerList);
        }
        Serial.print("Peer ");
        Serial.print(": MAC Address=");
        for (int j = 0; j < 6; ++j) {
            Serial.print(peerList.peer_addr[j], HEX);
            if (j < 5) {
                Serial.print(":");
            }
        }
        Serial.print(", Channel=");
        Serial.print(peerList.channel);
        Serial.println();

    }
      
}
void messaging::stringAllAddresses() {
    for (int i=1;i<addressCounter; i++) {
        addError("Address "+String(i)+": "+stringAddress(clientAddresses[i].address)+"\n");
    }
}

void messaging::printAllAddresses() {
    for (int i=0;i<addressCounter; i++) {
        Serial.println(String(i)+": "+stringAddress(clientAddresses[i].address));
    }
}



void messaging::handleErrors() {
    if (error_message != "") {
        Serial.println("\nERRORS:");
        Serial.println(error_message);
        error_message = "";
    }
}
void messaging::handleSent() {
    if (message_sent != "") {
        Serial.print("Sent from Handler: ");
        Serial.println(message_sent);
        message_sent = "";
    } 
}
void messaging::addError(String error, bool noNL) {
    error_message += error;
    if (noNL == false) {
        error_message += "\n";
    }
}
void messaging::addSent(String sent) {
    message_sent += sent;
    message_sent += "\n";
}


void messaging::receiveTimer(int messageArriveTime) {
  //add condition that if nothing happened after 5 seconds, situation goes back to start
  //wenn die letzte message maximal 300 mikrosekunden abweicht und der letzte delay auch nicht mehr als 1500ms her war, dann muss die msg korrekt sein
  int difference = messageArriveTime - lastTime;
  lastDelay = timerMessage.lastDelay;
  announceCounter = 0;
  addError("Difference: "+String(difference)+"\n");
  addError("Message Arrive Time "+String(messageArriveTime)+"\n");
  addError("Last Time "+String(lastTime)+"\n");

  if (abs(difference-CALIBRATION_FREQUENCY*TIMER_INTERVAL_MS) < 1000 and abs(timerMessage.lastDelay) <2500) {
    addError("Counts. Arraycounter: ");
    addError(String(arrayCounter));
    addError("\n");

    if (arrayCounter <TIMER_ARRAY_COUNT) {
      timerArray[arrayCounter] = timerMessage.lastDelay;
    }
    else {
      for (int i = 0; i< TIMER_ARRAY_COUNT; i++) {
        delayAvg += timerArray[i];
      } 
      delayAvg = delayAvg/TIMER_ARRAY_COUNT;
      gotTimerMessage.delayAvg = delayAvg;
        Serial.println("Send time is "+String(timerMessage.sendTime));
        Serial.println("Arrive time is: "+String(messageArriveTime));

      if (timerMessage.sendTime > messageArriveTime) {
        Serial.println("multiplier positive");
        timeOffset = timerMessage.sendTime-messageArriveTime-delayAvg/2;
        offsetMultiplier = 1;
      }
      else if (timerMessage.sendTime < messageArriveTime) {
        Serial.println("multiplier negative");
        timeOffset = messageArriveTime-timerMessage.sendTime-delayAvg/2;
        offsetMultiplier = -1;
      }
      Serial.println("timeOffset "+String(timeOffset));
      //timeOffset = messageArriveTime-timerMessage.sendTime-delayAvg/2;
      gotTimerMessage.timerOffset = timeOffset;
      gotTimer = true;
      #if DEVICE_MODE != WEBSERVER
      handleLed->setTimeOffset(timeOffset, offsetMultiplier);
      handleLed->flash(125,0,0, 200, 3, 300);  
      gotTimerMessage.batteryPercentage = getBattery();
      #endif
      pushDataToSendQueue(hostAddress, MSG_GOT_TIMER, -1);
      gotTimer = true;
      globalModeHandler->switchMode(MODE_NEUTRAL);
      addError("switched mode to Neutral");

      
    }
    arrayCounter++;
  }
  else {
    addError("Doesn't Count.");
    if (abs(difference-CALIBRATION_FREQUENCY*TIMER_INTERVAL_MS) >= 500) {
        addError(" Difference ");
        addError(String(abs(difference-CALIBRATION_FREQUENCY*TIMER_INTERVAL_MS)));
    }
    else if (abs(timerMessage.lastDelay) >=2500) {
    addError(" Last delay = ");
    addError(String(abs(timerMessage.lastDelay)));
    }
    addError("\n");
  }
   lastTime = messageArriveTime;
}


void messaging::pushDataToReceivedQueue(uint8_t* senderAddress, const uint8_t *incomingData, int len, unsigned long msgReceiveTime) {
    std::lock_guard<std::mutex> lock(receiveQueueMutex); // Lock the mutex
    dataQueue.push(ReceivedData{senderAddress, incomingData, len, msgReceiveTime}); // Push the received data into the queue
}
#if DEVICE_MODE != WEBSERVER
void messaging::addClap(unsigned long timeStamp) {
    //todo refactor
       //todo send clap times / my clap times
    if (myClapTimes.clapCounter < NUM_CLAPS) {
        addError("clap occurred at "+String(timeStamp+timeOffset)+"\n");
        myClapTimes.timeStamp[myClapTimes.clapCounter] = timeStamp+timeOffset;
    }
    else {
        addError("TOO MANY CLAPS");
    }
    myClapTimes.clapCounter++;
    if (DEVICE_MODE == MAIN) {
        updateAddressesToWebserver();
    }

}
#endif


int messaging::getAddressId(const uint8_t * address) {
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (memcmp(&clientAddresses[i].address, address, 6) == 0) {
            return i; // Found the address, return its ID
        }
    }

    return -1; // Address not found
}

int messaging::addPeer(uint8_t * address) {

    memcpy(&peerInfo->peer_addr, address, 6);

    if (esp_now_get_peer(peerInfo->peer_addr, peerInfo) == ESP_OK) {
       // addError("found peer\n");
        //Serial.println("Found Peer");
        return 0;
    }
    else {
      //  addError("couldn't find peer");
    }
    peerInfo->channel = 0;  
    peerInfo->encrypt = false;
         // Add peer        
    if (esp_now_add_peer(peerInfo) != ESP_OK){
       //addError("failed to add peer\n");
        //addError("Failed to add peer");
        return -1;
    }
    else {
        //addError("added peer\n");
        //Serial.println("Added Peer");
        return 1;
    }
    
}
void messaging::goToSleep(unsigned long sleepTime) {
    Serial.println("time is "+String(millis()));
    Serial.println("going to sleep for "+String(sleepTime)); 
    esp_sleep_enable_timer_wakeup(sleepTime);
    esp_light_sleep_start();
    Serial.println("time is "+String(millis()));
    int randNum = random(1000, 5000);
    delay(randNum);
    pushDataToSendQueue(hostAddress, MSG_WAKEUP, -1);
}

float messaging::getBattery() {
        analogReadResolution(12);
    analogSetPinAttenuation(BATTERY_PIN, ADC_11db);  
    int adcValue = analogRead(BATTERY_PIN); // Read the ADC value
    float voltage = adcValue * (4.2 / 3220.0);
    float percentage;
    if (voltage >= 4.2) {
        percentage = 100.0;
    } else if (voltage >= 3.8 && voltage < 4.2) {
        percentage =  70.0 + (voltage - 3.8) / (4.2 - 3.8) * (100.0 - 70.0);
    } else if (voltage >= 3.6 && voltage < 3.8) {
        percentage = 15.0 + (voltage - 3.6) / (3.8 - 3.6) * (70.0 - 15.0);
    } else if (voltage >= 3.0 && voltage < 3.6) {
        percentage =  (voltage - 3.0) / (3.6 - 3.0) * 15.0;
    } else {
        percentage = 0.0;
    }
    percentage = round(percentage * 100) / 100;
    return percentage;
}

void messaging::setBattery() {
    float percentage = getBattery();
    statusMessage.batteryPercentage = percentage;

}
void messaging::setAnimation(message_animate* messageAnimate) {
    memcpy(&animationMessage, messageAnimate, sizeof(animationMessage));
}   

void messaging::printTimerStuff() {
    if (gotTimer == true) {
        Serial.println("Got Timer, timer offset = "+String(timeOffset)+" and time right now "+String(micros()));
    }

}
