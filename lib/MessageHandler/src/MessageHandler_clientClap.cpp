
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
        peakdetection.add(data);
        int peak = peakDetection.getPeak(); 
        if (peak == -1 and millis() > lastClap+1000) {
            Serial.println("Happened "+String(peakDetection.ago)+" microseconds ago");
            Serial.println("avg at clap "+String(peakDetection.avgatclap));
            Serial.println("data: "+String(data));
            ledInstance->pushToAnimationQueue(ledInstance->createFlash(millis(), 300, 2, 0, 255, 255));
            message_data clapMessage = createClapMessage(hostAddress);
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