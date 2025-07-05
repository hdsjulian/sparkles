
#include "MessageHandler.h"
#if (DEVICE_MODE == CLIENT) 
#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"
#include <PeakDetection.h> 

void MessageHandler::runClapTask() {
    PeakDetection peakDetection; 
    int audioPin = 35; 
    pinMode(audioPin, INPUT); 
    peakDetection.begin(48, 10, 0.5);  
    double data;
    while (true) {
        data = (double)analogRead(audioPin)/2048-1;
        peakDetection.add(data);
        int peak = peakDetection.getPeak(); 
        if (peak == -1) {
            ESP_LOGI("CLAP", "Peak Detected at %i", micros());
            message_animation animation = ledInstance->createFlash(millis(), 300, 2, 0, 255, 255);
            ledInstance->pushToAnimationQueue(animation);
            message_data clapMessage = createClapMessage(true);
            pushToSendQueue(clapMessage);
            vTaskDelete(NULL);
          }
    }
}

void MessageHandler::startClapTask() {
    xTaskCreatePinnedToCore(runClapTaskWrapper, "runClapTask", 10000, this, 2, NULL, 0);
}

void MessageHandler::runClapTaskWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runClapTask();
}


#endif