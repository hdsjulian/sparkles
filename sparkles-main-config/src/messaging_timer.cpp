#include "Arduino.h"

#include "esp_now.h"
#include "WiFi.h"
#include "messaging.h"

void messaging::goodNight() {
    int* time;  
    time = getSystemTime();
    double difference;
    if (millis() > goToSleepTime && sentToSleep == false) {
        Serial.println("should be time to say good night and waiting ten seconds");
        difference = calculateTimeDifference(time[5], time[4], time[3], time[0], time[1], time[2], wakeupTime.hours, wakeupTime.minutes, wakeupTime.seconds);
        commandMessage.messageType = CMD_GO_TO_SLEEP;
        commandMessage.param = (int)difference+10;
        pushDataToSendQueue(broadcastAddress, MSG_COMMANDS, difference);
        goToSleepTime = goToSleepTime+10000;
        Serial.println("time is "+String(millis()));
        Serial.println("setting gotosleeptime to "+String(goToSleepTime));
        sentToSleep = true;
    }
    if ((millis() > goToSleepTime) && goToSleepTime!=0 && sentToSleep == true) {
        difference = calculateTimeDifference(time[5], time[4], time[3], time[0], time[1], time[2], wakeupTime.hours, wakeupTime.minutes-1, 0);
        Serial.println("time is "+String(millis()));
        Serial.println("and gotosleeptime was set to "+String(goToSleepTime));
        Serial.println("sleeping for "+String(difference));
        esp_sleep_enable_timer_wakeup((unsigned long)(difference*1000000));
        esp_light_sleep_start();
        webServer->setWifi();
        time = getSystemTime();
        Serial.println("woke up");
        Serial.println("Time is: "+String(time[0])+":"+String(time[1])+":"+String(time[2]));
        Serial.println("Day is "+String(time[3])+"."+String(time[4])+"."+String(time[5]));
        goToSleepTime = (int)calculateTimeDifference(time[5], time[4], time[3], time[0], time[1], time[2], sleepTime.hours, sleepTime.minutes, sleepTime.seconds)*1000;
        globalModeHandler->switchMode(MODE_NEUTRAL);
        sentToSleep = false;
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
