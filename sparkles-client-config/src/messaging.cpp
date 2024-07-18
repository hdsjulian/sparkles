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
    if (!LittleFS.exists(filename)) {
        addError("File doesn't exist while writing");
    }
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

void messaging::deleteFile(const char* filename) {

    if (!LittleFS.exists(filename)) {
        addError("File doesn't exist");
        return;
    }
    Serial.println("delete file");
    for (size_t i = 0; i < std::size(clientAddresses); ++i) {
    clientAddresses[i] = client_address{}; // Value-initialization
    }    
    writeStructsToFile(clientAddresses, NUM_DEVICES, "/clientAddress");
}

void messaging::checkFile(const char* filename) {
    if (!LittleFS.exists(filename)) {
        addError("File doesn't exist");
        return;
    }
    else {
        addError("File exists");
        return;
    }
}
// Function to read an array of structs from a file
bool messaging::readStructsFromFile(client_address* data, int count, const char* filename) {
    if (!LittleFS.exists(filename)) {
        addError("File doesn't exist");
        return false;
    }
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
void messaging::timeoutRetryHandler() {
    addError("Timeout Retry Handler  ");
    addError("Current Id: "+String(timeoutRetry.currentId));
    addError("Address Counter: "+String(addressCounter));
    
    if (timeoutRetry.currentId == addressCounter-1) {
        timeoutRetry.currentId = 0;
        timeoutRetry.lastMsg = millis();
        addError("reached Address counter, waiting 60 seconds");
        if (timeoutRetry.unavailableCounter == 0 or timersUpdated == addressCounter) {
            addError("all done");
            globalModeHandler->switchMode(MODE_NEUTRAL);
            return;
        }
    }
    else {
        timeoutRetry.currentId++;
    }
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



void messaging::setNoSuccess() {
    addError("setting no success\n");
    if (clientAddresses[timeoutRetry.currentId].tries == NUM_RETRIES) {
        addError("setting "+String(timeoutRetry.currentId)+" to dead\n");
        clientAddresses[timeoutRetry.currentId].active = DEAD;   
        if (globalModeHandler->getMode() == MODE_RESET_TIMER) {
            timersUpdated++;
        }
        timeoutRetry.unavailableCounter--;
        
        }
    else {
        addError("setting "+String(timeoutRetry.currentId)+" to unreachable\n");
        clientAddresses[timeoutRetry.currentId].active = UNREACHABLE;
        clientAddresses[timeoutRetry.currentId].tries++;
        timeoutRetry.unavailableCounter++;
        addError("retries "+String(clientAddresses[timeoutRetry.currentId].tries)+"\n");
    }
    if (timeoutRetry.unavailableCounter == addressCounter) {
        addError("All devices are unreachable\n");
        globalModeHandler->switchMode(MODE_NEUTRAL);
    }
    timeoutRetryHandler();
}




void messaging::setClock(int year, int month, int day, int hours, int minutes, int seconds) {
    struct tm timeinfo;
    memset(&timeinfo, 0, sizeof(timeinfo));
    timeinfo.tm_hour = hours;
    timeinfo.tm_min = minutes;
    timeinfo.tm_sec = seconds;
    timeinfo.tm_year = year;
    timeinfo.tm_mon = month;
    timeinfo.tm_mday = day;
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
    timeArray[3] = timeinfo.tm_mday;
    timeArray[4] = timeinfo.tm_mon;
    timeArray[5] = timeinfo.tm_year;

    return timeArray;
}
double messaging::calculateTimeDifference(int year1, int month1, int day1, int hours1, int minutes1, int seconds1, int hours2, int minutes2, int seconds2) {
    Serial.println("Calculating time difference "+String(day1)+"."+String(month1)+"."+String(year1)+" - "+String(hours1)+":"+String(minutes1)+":"+String(seconds1)+" to  "+String(hours2)+":"+String(minutes2)+":"+String(seconds2));
    struct tm timeinfo1 = {0};
    struct tm timeinfo2 = {0};
    time_t time1;
    time_t time2;

    // Set the first time
    timeinfo1.tm_hour = hours1;
    timeinfo1.tm_min = minutes1;
    timeinfo1.tm_sec = seconds1;
    timeinfo1.tm_year = year1 - 1900; // Years since 1900
    timeinfo1.tm_mon = month1 - 1; // Months since January (0-11)
    timeinfo1.tm_mday = day1;
    time1 = mktime(&timeinfo1);

    // Assume time2 is the same day for initial calculation
    timeinfo2.tm_hour = hours2;
    timeinfo2.tm_min = minutes2;
    timeinfo2.tm_sec = seconds2;
    timeinfo2.tm_year = year1 - 1900;
    timeinfo2.tm_mon = month1 - 1;
    timeinfo2.tm_mday = day1;
    time2 = mktime(&timeinfo2);

    // Adjust time2 if it's before time1, assuming it's the next day
    if (difftime(time2, time1) < 0) {
        timeinfo2.tm_mday += 1; // Move to the next day
        time2 = mktime(&timeinfo2); // Recalculate time2
    }

    // Calculate the difference
    double difference = difftime(time2, time1);
    Serial.println("Difference should be "+String(difference));
    return difference;
}

/*
double messaging::calculateTimeDifference(int year1, int month1, int day1, int hours1, int minutes1, int seconds1, int hours2, int minutes2, int seconds2) {
    Serial.println("Calculating time difference "+String(day1)+"."+String(month1)+"."+String(year1)+" - "+String(hours1)+":"+String(minutes1)+":"+String(seconds1)+" to  "+String(hours2)+":"+String(minutes2)+":"+String(seconds2));
    struct tm timeinfo1;
    struct tm timeinfo2;
    time_t time1;
    time_t time2;

    // Set the first time
    timeinfo1.tm_hour = hours1;
    timeinfo1.tm_min = minutes1;
    timeinfo1.tm_sec = seconds1;
    timeinfo1.tm_year = year1;
    timeinfo1.tm_mon = month1;
    timeinfo1.tm_mday = day1;
    time1 = mktime(&timeinfo1);

    // Set the second time
    timeinfo2.tm_hour = hours2;
    timeinfo2.tm_min = minutes2;
    timeinfo2.tm_sec = seconds2;
    int day2 = 0;
    int month2 = 0;
    int year2 = 0;
    if (timeinfo2.tm_hour < timeinfo1.tm_hour) {
         day2 = day1+1;
    }
    else if (timeinfo2.tm_hour == timeinfo1.tm_hour and timeinfo2.tm_min < timeinfo1.tm_min) {
        day2 = day1+1;
    }
    else if (timeinfo2.tm_hour == timeinfo1.tm_hour and timeinfo2.tm_min == timeinfo1.tm_min and timeinfo2.tm_sec < timeinfo1.tm_sec) {
        day2 = day1+1;
    }
    else {
        day2 = day1;
    }
    month2 = month1;
    if (month1 == 1 or month1 == 3 or month1 == 5 or month1 == 7 or month1 == 8 or month1 == 10 or month1 == 12) {
        if (day2 > 31) {
            day2 = 1;
            month2++;
        }
    }
    else if (month1 == 4 or month1 == 6 or month1 == 9 or month1 == 11) {
        if (day2 > 30) {
            day2 = 1;
            month2++;
        }
    }
    else if (month1 == 2) {
        if (day2 > 28) {
            day2 = 1;
            month2++;
        }
    }
    else {
        if (day2 > 31) {
            day2 = 1;
            month2++;
        }
        else {
        }
    }
    if (month2 > 12) {
        month2 = 1;
        year2++;
    }
    else {
        year2 = year1;
    }
    Serial.println("Day2: "+String(day2)+" Month2: "+String(month2)+" Year2: "+String(year2));
    timeinfo2.tm_year = year2;
    timeinfo2.tm_mon = month2;
    timeinfo2.tm_mday = day2;
    time2 = mktime(&timeinfo2);

    // Calculate the difference
    double difference = difftime(time2, time1);
    Serial.println("Difference is "+String(difference));
    return difference;
}
*/

void messaging::forceDebug(int i) {
    return;
    forcedDebugCounter++;
    Serial.println("forced debug "+String(forcedDebugCounter)+" - "+String(i));
    //delay();
}

void messaging::nextAnimation() {
    if (millis() < nextAnimationPing) {
        if (millis() % 1000 == 0) {
            delay(1);
            Serial.println("Next animation in "+String((nextAnimationPing-millis())/1000)+" seconds");
        }
        return;
    }
    if (millis() > nextAnimationPing) {
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

void messaging::switchMode(int mode) {
    switchModeMessage.mode = mode;
    pushDataToSendQueue(broadcastAddress, MSG_SWITCH_MODE, -1);
}