#include <Arduino.h>

#include "esp_now.h"
#include "WiFi.h"
#include "messaging.h"





// Function to read an array of structs from a file


void reorderTimestamps(unsigned long timestamps[], int size) {
    // Sort the timestamps
    std::sort(timestamps, timestamps + size);
    
    // Find the first occurrence of zero
    unsigned long* zeroPos = std::find(timestamps, timestamps + size, 0);

    // Move all non-zero elements to the beginning of the array
    std::rotate(timestamps, zeroPos, std::remove(timestamps, timestamps + size, 0));
}

void messaging::filterClaps(int index) {
    float THRESHOLD = 1000;
    for (int i = 0; i < clapDevice.clapTimes.clapCounter; i++) {
        bool found = false;
        for (int j = 0; j < clientAddresses[index].clapTimes.clapCounter; j++) {
            if ((clapDevice.clapTimes.timeStamp[i] > clientAddresses[index].clapTimes.timeStamp[j] ? 
                clapDevice.clapTimes.timeStamp[i] - clientAddresses[index].clapTimes.timeStamp[j] : 
                clientAddresses[index].clapTimes.timeStamp[j] - clapDevice.clapTimes.timeStamp[i]) <= THRESHOLD) {
                found = true;
                break;
            }
        }
        if (!found) {
            clientAddresses[index].clapTimes.timeStamp[i] = 0;
            clientAddresses[index].clapTimes.timeStamp[i]--;
        }
    }
    std::sort(clientAddresses[index].clapTimes.timeStamp, clientAddresses[index].clapTimes.timeStamp + NUM_CLAPS);
}


void messaging::calculateDistances(int id) {
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
        clientAddresses[id].distance = dist;
        distanceMessage.distance = dist;
        pushDataToSendQueue(clientAddresses[id].address, MSG_DISTANCE, -1);
        addError("Distance calculated"+String(clientAddresses[id].distance)+" Centimeters\n");
    }
    else {
        //if there are no claps, or the claps are too far apart, set the distance to 0
        addError("No distances found\n");
        clientAddresses[id].distance = 0;
    }

    Serial.println("Done Calculating distances");
}
/*
void messaging::calculateDistances() {
    //go through all devices
    for (int i = 1; i < addressCounter; i++ ){
        int cumul = 0;
        int clapCount = 0;
        addError("Device: "+String(i)+"\n");
        addError("Clap Counter: "+String(clientAddresses[i].clapTimes.clapCounter)+"\n");
        if (clientAddresses[i].clapTimes.clapCounter == 0) { continue; }
        for (int j = 0; j < webserverClapTimes.clapCounter; j++){
            int clapId = -1;
            ind clapIdW = -1
            for (int k = 0; k < clientAddresses[i].clapTimes.clapCounter; k++) {
                unsigned long timeStampDifference = (clientAddresses[i].clapTimes.timeStamp[k] > webserverClapTimes.timeStamp[j]) ?
                                                (clientAddresses[i].clapTimes.timeStamp[k] - webserverClapTimes.timeStamp[j]) :
                                                (webserverClapTimes.timeStamp[j] - clientAddresses[i].clapTimes.timeStamp[k]);
                if (timeStampDifference < 1000) {
                    clapId = k;
                    clapIdW = j;
                }
            }

            unsigned long timeStampDifference = (clientAddresses[i].clapTimes.timeStamp > webserverClapTimes.timeStamp[j]) ?
                                                (clientAddresses[i].clapTimes.timeStamp - webserverClapTimes.timeStamp[j]) :
                                                (webserverClapTimes.timeStamp[j] - clientAddresses[i].clapTimes.timeStamp);
        }
    }
}
/*
void messaging::calculateDistances() {
    //go through all client devices
    for (int i = 1;i < NUM_DEVICES;i++) {
        int cumul = 0;
        int clapCount = 0;
        //go through all claps on the client device
        addError("Device: "+String(i))+"\n");
        addError("Clap Counter: "+String(clientAddresses[i].clapTimes.clapCounter)+"\n");
        if (clientAddresses[i].clapTimes.clapCounter == 0){ continue;}
        for (int j = 0; j<clientAddresses[i].clapTimes.clapCounter; j++) {
            //if the client device's clap from the counter isn't around the master's device one:
            // this is until i have the clapping board ready
            int clapId = -1;
            unsigned long timeStampDifference = (clientAddresses[0].clapTimes.timeStamp[j] > clientAddresses[i].clapTimes.timeStamp[j]) ?
                                                (clientAddresses[0].clapTimes.timeStamp[j] - clientAddresses[i].clapTimes.timeStamp[j]) :
                                                (clientAddresses[i].clapTimes.timeStamp[j] - clientAddresses[0].clapTimes.timeStamp[j]);
            if (timeStampDifference < 1000) {
                for (int k = 0; k < clientAddresses[0].clapTimes.clapCounter; k++) {
                    unsigned long timeStampDifference2 = (clientAddresses[0].clapTimes.timeStamp[k] > clientAddresses[i].clapTimes.timeStamp[j]) ?
                                     (clientAddresses[0].clapTimes.timeStamp[k] - clientAddresses[i].clapTimes.timeStamp[j]) :
                                     (clientAddresses[i].clapTimes.timeStamp[j] - clientAddresses[0].clapTimes.timeStamp[k]);

                    if(timeStampDifference2 < 1000) {
                        clapId = k;
                    }
                }
            }
            else {
                clapId = j;
            }
            if (clapId != -1) {
                clapCount++;
                unsigned long timeStampDifference3 = (clientAddresses[0].clapTimes.timeStamp[clapId] > clientAddresses[i].clapTimes.timeStamp[j]) ?
                                     (clientAddresses[0].clapTimes.timeStamp[clapId] - clientAddresses[i].clapTimes.timeStamp[j]) :
                                     (clientAddresses[i].clapTimes.timeStamp[j] - clientAddresses[0].clapTimes.timeStamp[clapId]);

                cumul += timeStampDifference3;
            }
        }
        if( cumul > 0 and clapCount != 0) {
            clientAddresses[i].distance = (float)((float)cumul/clapCount);
        }
        else {
            clientAddresses[i].distance = 0;
        }
        addError("Distance: ");
        addError(String(clientAddresses[i].distance));
    }
}

*/



void messaging::setHostAddress(uint8_t address[6]) {
    memcpy (&hostAddress, address, 6);
}


int messaging::getMessagingMode() {
    return messagingModeHandler.getMode();
}
void messaging::printMessagingMode() {
    messagingModeHandler.printCurrentMode();
}
void messaging::setMessagingMode(int mode) {
    messagingModeHandler.switchMode(mode);
    addError(messagingModeHandler.modeToText(mode));
}


void messaging::printMessageModeLog() {
    messagingModeHandler.printLog();
}


void messaging::writeStructsToFile(const client_address* data, int count, const char* filename) {
    File file = LittleFS.open(filename, "w");
    if (!file) {
        addError("Failed to open file for writing");
        return;
        
    }
    for (int i = 0; i < count; i++) {
        file.write((uint8_t*)&data[i], sizeof(client_address));
    }
    file.close();
}

// Function to read an array of structs from a file
bool messaging::readStructsFromFile(client_address* data, int count, const char* filename) {
    Serial.println("reading Structs from file");
    File file = LittleFS.open(filename, "r");
    if (!file) {
        Serial.println("Failed to open file for reading");
        return false;
    }
    size_t totalSize = count * sizeof(client_address);
    
    // Check if the file is large enough
    if (file.size() < totalSize) {
        Serial.println("File size is smaller than expected data size");
        addError("File size is smaller than expected data size");
        file.close();
        return false;
    }
    else {
        Serial.println("File size is exactly right");
    }
    for (int i = 0; i < count; i++) {
        size_t bytesRead = file.read((uint8_t*)&data[i], sizeof(client_address));
        if (bytesRead != sizeof(client_address)) {
            Serial.print("Failed to read complete structure at index ");
            Serial.println(i);
            addError("Failed to read complete structure");
            file.close();
            return false;
        }
    }
    
    // Check if the file is large enough

    file.close();
    return true;

}


void messaging::handleTimerUpdates() {

 if (timersUpdated == addressCounter)  {  
    Serial.println("all timers updated. TimersUpdated: "+String(timersUpdated)+" addressCounter: "+String(addressCounter));
    delay(1000);
    return;
 }

 if (timeoutRetry.currentId < addressCounter) {
    for (int i = timeoutRetry.currentId; i < addressCounter; i++) {
        //darf natürlich nicht weiter gehen. entweder hier noch mit status check oder die ganze funktion nur alle sekunde aufrufen
        if (clientAddresses[i].active == WAITING or clientAddresses[i].active == UNREACHABLE) {
            addError("Updating timers for device "+String(i)+"\n");
            updateTimers(i);
            return;
        }
        else if (clientAddresses[i].active == INACTIVE) {
               clientAddresses[i].active = WAITING;
            }
        else if (clientAddresses[i].active == ACTIVE) {
            timerUpdateCounter++;
        }
    }
 }

}
void messaging::setNoSuccess() {
    addError("setting no success\n");
    if (timeoutRetry.tries == NUM_RETRIES) {
        addError("setting "+String(timeoutRetry.currentId)+" to dead\n");
        clientAddresses[timeoutRetry.currentId].active = DEAD;   
        if (globalModeHandler->getMode() == MODE_RESET_TIMER) {
            timersUpdated++;
        }
        
        }
    else {
        addError("setting "+String(timeoutRetry.currentId)+" to unreachable\n");
        clientAddresses[timeoutRetry.currentId].active = UNREACHABLE;
        clientAddresses[timeoutRetry.currentId].tries++;
        timeoutRetry.unavailableCounter++;
    }
    if (timeoutRetry.unavailableCounter == addressCounter) {
        addError("All devices are unreachable\n");
        globalModeHandler->switchMode(MODE_NEUTRAL);
    }
    timeoutRetryHandler();
}

void messaging::goodNight() {
    int* time;  
    time = getSystemTime();
    double difference;
    if (time[0] >= sleepTime.hours and time[1] >= sleepTime.minutes and goToSleepTime == 0) {
        Serial.println("should be time to say good night and waiting a minute");
        difference = calculateTimeDifference(time[0], time[1], time[2], wakeupTime.hours, wakeupTime.minutes, wakeupTime.seconds);
        commandMessage.messageType = CMD_GO_TO_SLEEP;
        commandMessage.param = (int)difference+10;
        pushDataToSendQueue(broadcastAddress, MSG_COMMANDS, difference);
        goToSleepTime = (millis()+10000);
    }
    if (goToSleepTime != 0 and (millis() > goToSleepTime)) {
        difference = calculateTimeDifference(time[0], time[1], time[2], wakeupTime.hours, wakeupTime.minutes-1, 0);
        Serial.println("sleeping for "+String(difference));
        esp_sleep_enable_timer_wakeup((unsigned long)(difference*1000000));
        esp_light_sleep_start();
        goToSleepTime = 0;
        globalModeHandler->switchMode(MODE_NEUTRAL);
    }
    else if ((goToSleepTime != 0) and (millis() % 1000 == 0)) {
        Serial.println("Time to Sleep "+String(goToSleepTime-millis()));
    }

}

double messaging::calculateGoodNight(bool sleepWakeup) {
    int* time;  
    time = getSystemTime();
    double difference;
    if (sleepWakeup == true) {
        difference = calculateTimeDifference(time[0], time[1], time[2], sleepTime.hours, sleepTime.minutes, sleepTime.seconds);
    }
    else {
    difference = calculateTimeDifference(time[0], time[1], time[2], wakeupTime.hours, wakeupTime.minutes, wakeupTime.seconds);
    }
    return difference;
}
void messaging::setGoodNight(int hours, int minutes, int seconds){
    wakeupTime.hours = hours;
    wakeupTime.minutes = minutes;
    wakeupTime.seconds = seconds;
}
void messaging::setWakeup(int hours, int minutes, int seconds){
    sleepTime.hours = hours;
    sleepTime.minutes = minutes;
    sleepTime.seconds = seconds;    
}



void messaging::setClock(int hours, int minutes, int seconds) {
    struct tm timeinfo;
    memset(&timeinfo, 0, sizeof(timeinfo));
    timeinfo.tm_hour = hours;
    timeinfo.tm_min = minutes;
    timeinfo.tm_sec = seconds;
    timeinfo.tm_year = 2024;
    timeinfo.tm_mon = 6;
    timeinfo.tm_mday = 21;
    struct timeval tv;
    tv.tv_sec = mktime(&timeinfo);
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
}

int* messaging::getSystemTime() {
    static int timeArray[3];
    struct tm timeinfo;
    time_t now;

    time(&now);
    localtime_r(&now, &timeinfo);

    timeArray[0] = timeinfo.tm_hour;
    timeArray[1] = timeinfo.tm_min;
    timeArray[2] = timeinfo.tm_sec;

    return timeArray;
}
double messaging::calculateTimeDifference(int hours1, int minutes1, int seconds1, int hours2, int minutes2, int seconds2) {
    struct tm timeinfo1;
    struct tm timeinfo2;
    time_t time1;
    time_t time2;

    // Set the first time
    timeinfo1.tm_hour = hours1;
    timeinfo1.tm_min = minutes1;
    timeinfo1.tm_sec = seconds1;
    timeinfo1.tm_year = 2024;
    timeinfo1.tm_mon = 6;
    timeinfo1.tm_mday = 21;
    time1 = mktime(&timeinfo1);

    // Set the second time
    timeinfo2.tm_hour = hours2;
    timeinfo2.tm_min = minutes2;
    timeinfo2.tm_sec = seconds2;
    timeinfo2.tm_year = 2024;
    timeinfo2.tm_mon = 6;
    if (timeinfo2.tm_hour < timeinfo1.tm_hour) {
        timeinfo2.tm_mday = 22;
    }
    else if (timeinfo2.tm_hour == timeinfo1.tm_hour and timeinfo2.tm_min < timeinfo1.tm_min) {
        timeinfo2.tm_mday = 22;
    }
    else if (timeinfo2.tm_hour == timeinfo1.tm_hour and timeinfo2.tm_min == timeinfo1.tm_min and timeinfo2.tm_sec < timeinfo1.tm_sec) {
        timeinfo2.tm_mday = 22;
    }
    else {
        timeinfo2.tm_mday = 21;
    }

    time2 = mktime(&timeinfo2);

    // Calculate the difference
    double difference = difftime(time2, time1);

    return difference;
}


void messaging::forceDebug(int i) {
    return;
    forcedDebugCounter++;
    Serial.println("forced debug "+String(forcedDebugCounter)+" - "+String(i));
    //delay();
}

void messaging::nextAnimation() {
    return;
    if (millis() < nextAnimationPing) {
        return;
    }
    if (millis() > nextAnimationPing) {
        if (endAnimation == true) {
            Serial.println("End animation is true");
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

void messaging::switchMode(int mode) {
    switchModeMessage.mode = mode;
    pushDataToSendQueue(broadcastAddress, MSG_SWITCH_MODE, -1);
}