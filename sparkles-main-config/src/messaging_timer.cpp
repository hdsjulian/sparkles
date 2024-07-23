#include "Arduino.h"

#include "esp_now.h"
#include "WiFi.h"
#include "messaging.h"
#include "driver/rtc_io.h"
#include "soc/rtc.h"
void messaging::goodNight() {
    if (globalModeHandler->getMode() == MODE_WOKEUP) {
        return;
    }
    if (goToSleepTime == 0) {
        return;
    }   
    int* time;  
    time = getSystemTime();

    unsigned long difference;
    if (millis() > goToSleepTime && sentToSleep == false) {
        Serial.println("should be time to say good night and waiting ten seconds");
        difference = wakeupTime.hours*3600+wakeupTime.minutes*60+wakeupTime.seconds;
        commandMessage.param = (int)difference+10;
        pushDataToSendQueue(broadcastAddress, MSG_COMMANDS, CMD_GO_TO_SLEEP,  difference);
        goToSleepTime = goToSleepTime+10000;
        Serial.println("time is "+String(millis()));
        Serial.println("setting gotosleeptime to "+String(goToSleepTime));
        sentToSleep = true;
    }
    if ((millis() > goToSleepTime && goToSleepTime!=0 && sentToSleep == true)) {
        difference = wakeupTime.hours*3600+wakeupTime.minutes*60+wakeupTime.seconds;
        Serial.println("time is "+String(millis()));
        Serial.println("and system tieme is "+String(time[0])+":"+String(time[1])+":"+String(time[2]));
        Serial.println("sleeping for "+String(difference));
        Serial.println("sleeping until "+String(wakeupTime.hours)+":"+String(wakeupTime.minutes)+":"+String(wakeupTime.seconds));
        WiFi.mode(WIFI_OFF);
        esp_now_deinit();
        esp_sleep_enable_timer_wakeup((unsigned long)((unsigned long)difference*1000000UL));
        esp_light_sleep_start();
        Serial.println("woke up");
        webServer->setWifi();
        if (esp_now_init() != 0) {
            Serial.println("Error initializing ESP-NOW");
        }
        esp_now_peer_info_t peerInfo;
        memcpy(peerInfo.peer_addr, broadcastAddress, 6);
        peerInfo.channel = 0;  
        peerInfo.encrypt = false;
        // Add peer        
        if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Serial.println("Failed to add peer broadcast");
        return;
        }
        time = getSystemTime();
        commandMessage.messageType == NOPE;
        int delayTime = 11*1000;
        Serial.println("delaying for "+String(delayTime));
        delay(delayTime);
        goToSleepTime = (int)calculateTimeDifference(time[5], time[4], time[3], time[0], time[1], time[2], sleepTime.hours, sleepTime.minutes, sleepTime.seconds)*1000;
        Serial.println("new go to sleep time is "+String(goToSleepTime)+" which is in "+String(millis()-goToSleepTime)+" ms");
        globalModeHandler->switchMode(MODE_WOKEUP);
        sentToSleep = false;
        timersUpdated = 0;
        timeoutRetry.currentId = 0;
        setAddressesInactive();
                //how to prevent sleeping again?
    }
    else if ((millis() < goToSleepTime-10000 && (millis() % 1000 == 0)) && goToSleepTime!=0 && sentToSleep == true) {
        Serial.println("Time to Sleep "+String(goToSleepTime-millis()));
        delay(1);
    }
    else if (millis() % 10000 == 0) {
        Serial.println("called");
        Serial.println("sleep time at "+String(goToSleepTime));
        Serial.println("time now "+String(millis()));
        Serial.println("diff "+String(goToSleepTime-millis()) );
        delay(1);
    }

}

double messaging::calculateGoodNight(bool sleepWakeup) {
    int* time;  
    time = getSystemTime();
    double difference;
    if (sleepWakeup == true) {
        difference = calculateTimeDifference(time[5], time[4], time[3], time[0], time[1], time[2], sleepTime.hours, sleepTime.minutes, sleepTime.seconds);
    }
    else {
    difference = calculateTimeDifference(time[5], time[4], time[3], time[0], time[1], time[2], wakeupTime.hours, wakeupTime.minutes, wakeupTime.seconds);
    }
    return difference;
}


void messaging::setAddressesInactive() {
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
            clientAddresses[i].active = INACTIVE;
        }
        else {
            addError("We have "+String(addressCounter)+" addresses\n"); 
            return;
        }
    }
}