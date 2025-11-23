
#include "MessageHandler.h"
#if (DEVICE_MODE == CLIENT) 
#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"
#include <PeakDetection.h> 

void MessageHandler::runClapTask() {
    ESP_LOGI("CLAP", "Starting clap detection task");
    PeakDetection peakDetection; 
    int audioPin = 5; 
    pinMode(audioPin, INPUT); 
    peakDetection.begin(48, 10, 0.5);  
    double data;
    unsigned long lastClapTime = millis();
    unsigned long lastPing = millis();
    ESP_LOGI("CLAP", "Started at %lu", lastClapTime);
    ESP_LOGI("CLAP", "Has clap happened: %d", getHasClapHappened());
    while (true) {
        double data = (double)analogRead(audioPin)/512-1;
        peakDetection.add(data); 
        int peak = peakDetection.getPeak(); 
        double filtered = peakDetection.getFilt(); 
        if ((peak == -1 || (millis() - lastClapTime > CLAP_TIMEOUT)) && (getHasClapHappened() == true || (getCalibrationTest() == true && millis() - lastClapTime > 1000 ) )) {            
            ESP_LOGI("CLAP" , "Clap happened? %d", (int)getHasClapHappened());
            message_data clapMessage = createClapMessage(true);
            lastClapTime = millis();
            //
            message_animation animation = ledInstance->createFlash(millis(), 300, 2, 0, 255, 255);
            ledInstance->pushToAnimationQueue(animation);
            
            if (millis() - lastClapTime > CLAP_TIMEOUT) {
                ESP_LOGI("CLAP", "No clap detected, resetting hasClapHappened");
                clapMessage.payload.clap.clapHappened = false;
            }            
            else {
                ESP_LOGI("CLAP", "Clap detected with peak: %d, filtered: %.2f", peak, filtered);
                clapMessage.payload.clap.clapHappened = true;
            }            
            memcpy(clapMessage.targetAddress, hostAddress, 6);
            pushToSendQueue(clapMessage);
            setHasClapHappened(false);
            vTaskDelete(NULL);
            ESP_LOGI("CLAP", "Clap task finished");
        } 
        if (millis() - lastPing > 1000) {
            ESP_LOGI("CLAP", "Has Clap Happened: %d", getHasClapHappened());    
            ESP_LOGI("CLAP", "Last Clap Time: %lu", lastClapTime);
            lastPing = millis();
        }
        taskYIELD(); 
          
    }
}

void MessageHandler::startClapTask() {
    xTaskCreatePinnedToCore(runClapTaskWrapper, "runClapTask", 10000, this, 20, &clapTaskHandle, 1);
}

void MessageHandler::runClapTaskWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runClapTask();
}


#endif