
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
    ESP_LOGI("CLAP", "Started at %lu", lastClapTime);
    ESP_LOGI("CLAP", "Has clap happened: %d", getHasClapHappened());
    while (true) {
        double data = (double)analogRead(audioPin)/512-1;
        peakDetection.add(data); 
        int peak = peakDetection.getPeak(); 
        double filtered = peakDetection.getFilt(); 
        
        if (peak == -1 && (getHasClapHappened() == true || millis() - lastClapTime > 1000)) {
            lastClapTime = millis();
            ESP_LOGI("CLAP", "Peak Detected at %lu", millis());
            message_animation animation = ledInstance->createFlash(millis(), 300, 2, 0, 255, 255);
            ledInstance->pushToAnimationQueue(animation);
            message_data clapMessage = createClapMessage(true);
            pushToSendQueue(clapMessage);
            setHasClapHappened(false);
            vTaskDelete(NULL);
        } 
        taskYIELD(); 
          
    }
}

void MessageHandler::startClapTask() {
    xTaskCreatePinnedToCore(runClapTaskWrapper, "runClapTask", 10000, this, 20, NULL, 1);
}

void MessageHandler::runClapTaskWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runClapTask();
}


#endif