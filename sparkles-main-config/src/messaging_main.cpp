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
    WiFi.macAddress(clientAddresses[0].address);
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


