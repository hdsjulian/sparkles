#include "Arduino.h"

#include "esp_now.h"
#include "WiFi.h"
#include "messaging.h"

void messaging::setup(modeMachine &modeHandler, ledHandler &globalHandleLed, esp_now_peer_info_t &globalPeerInfo) {
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
    int peerMsg = addPeer(hostAddress);
    if (peerMsg == 1) {
        //handleLed->flash(0, 125, 125, 200, 1, 50);
    }
    else if (peerMsg == -1) {
        //
        addError("COULD NOT ADD PEER");
    }



    
}



void messaging::handleReceive(uint8_t *senderAddress, const uint8_t *incomingData, int len, unsigned long msgReceiveTime) {
    /*
    if (memcmp(senderAddress, hostAddress, 6) !=0 and memcmp(senderAddress, clapDeviceAddress, 6) !=0 ) {
        addError("received command from untrusted source\n");
        return;
    }*/
    addError("Handling Received ");
    addError(messageCodeToText(incomingData[0]));
    addError(" from ");
    addError(stringAddress(senderAddress));
    addError("\n");
    switch (incomingData[0]) {
        //cases for main
        case MSG_COMMANDS: 
            memcpy(&commandMessage,incomingData,sizeof(commandMessage));
            addError("command:");
            addError(messageCodeToText(commandMessage.messageId));
            addError("\n");
            switch (commandMessage.messageId) {
                case CMD_GET_TIMER: 
                    if (globalModeHandler->getMode() == MODE_SEND_ANNOUNCE or globalModeHandler->getMode()== MODE_ANIMATE or globalModeHandler->getMode()== MODE_NEUTRAL) {
                        handleLed->flash(0, 65, 65, 100, 2, 50);
                        memcpy(&timerReceiver, clapDeviceAddress, 6);
                        globalModeHandler->switchMode(MODE_SENDING_TIMER);
                    };
                break;
                case CMD_BLINK: 
                handleLed->flash(125, 125, 55, commandMessage.param, 1, 50);
                break;
                case CMD_TIMER_CALIBRATION: 
                    globalModeHandler->switchMode(MODE_WAIT_FOR_TIMER);
                break;
                case CMD_GO_TO_SLEEP:
                        Serial.println("received go to sleep");
                         goToSleep(commandMessage.param*1000000);
                break;
                case CMD_RESET:
                    ESP.restart();
                break;
                case CMD_GET_STATUS:
                    setBattery();
                    //todo ledhandler:flash
                    handleLed->flash(0, 255, 0, 300, 1, 0);
                    pushDataToSendQueue(hostAddress, MSG_STATUS, -1);
                break;
                case CMD_START_CALIBRATION_MODE: 
                    globalModeHandler->switchMode(MODE_CALIBRATE);
                break;
                case CMD_END_CALIBRATION_MODE: 
                    globalModeHandler->switchMode(MODE_NEUTRAL);
                break;
                case CMD_MASTERCLAP_OCCURRED:
                    addError("Received MASTERCLAP OCCURRED");
                    if (globalModeHandler->getMode() == MODE_CLAPPING) {
                        globalModeHandler->switchMode(MODE_MASTERCLAP_OCCURRED);
                    }
                break;
                case CMD_RESET_CALIBRATION: 
                    globalModeHandler->switchMode(MODE_NEUTRAL);
                    memset(&myClapTimes, 0, sizeof(myClapTimes));
                break;
                case CMD_GET_CLAP_TIMES: 
                Serial.println("received cmd get clap times");
                pushDataToSendQueue(hostAddress, MSG_SEND_CLAP_TIMES, -1);
                break;
                case CMD_START_BROADCAST:
                    addError("Received Begin Broadcast\n");
                    globalModeHandler->switchMode(MODE_RECEIVE_BROADCAST);
                break;   
            }
            break;
        case MSG_SWITCH_MODE: 
            memcpy(&switchModeMessage, incomingData, sizeof(switchModeMessage));
            if (switchModeMessage.mode == MODE_CALIBRATE) {
                handleLed->flash(0, 125, 0, 100, 1, 50);
            }
            else if (switchModeMessage.mode == MODE_NEUTRAL and globalModeHandler->getMode() == MODE_CALIBRATE) {
                handleLed->flash(125, 125, 0, 100, 3, 50);
            }

            addError("Switching mode to "+String(switchModeMessage.mode)+"\n");
            globalModeHandler->switchMode(switchModeMessage.mode);
        break;

        case MSG_BROADCAST_TIMER: 
            addError("Received Broadcast Timer\n");
            if (globalModeHandler->getMode() == MODE_RECEIVE_BROADCAST) {
                memcpy(&timerMessage, incomingData, sizeof(timerMessage));
                receiveBroadcastTimer(msgReceiveTime);
            }
            break;
        case MSG_TIMER_CALIBRATION:  
        { 
            addError("received timer calibration message\n");
        memcpy(&timerMessage,incomingData,sizeof(timerMessage));
        if (timerMessage.reset == true and timerMessage.counter <=3) {
            addError("Timerreset = true");
            gotTimer = false;
        }
        else if (gotTimer == true) {
            addError("Got Timer = true");
            break;
        }
        if (timerMessage.addressId != 0) {
            handleLed->setPosition(timerMessage.addressId);
        }
            //handleLed->flash(0, 0, 125 , 100, 2, 50); 
        globalModeHandler->switchMode(MODE_WAIT_FOR_TIMER);
        receiveTimer(msgReceiveTime);
        }
        break;
        case MSG_ASK_CLAP_TIMES: 
        {   
            pushDataToSendQueue(hostAddress, MSG_SEND_CLAP_TIMES, -1);
        }
        break;
        case MSG_ANNOUNCE: 
            {
                memcpy(&announceMessage, incomingData, sizeof(announceMessage));
                setHostAddress(announceMessage.address);
                break;
            }
        case MSG_DISTANCE: 
        {
         memcpy(&distanceMessage, incomingData, sizeof(distanceMessage));
         addError("Distance: "+String(distanceMessage.distance)+"\n");
         handleLed->setDistance(distanceMessage.distance);
        }
        break;
        case MSG_ANIMATION:
            addError("Animation Message Incoming\n");
            if (DEVICE_MODE == MAIN and memcmp(senderAddress, clapDeviceAddress, 6) == 0) {
                memcpy(&animationMessage, incomingData, sizeof(animationMessage));
                animationMessage.startTime = micros()+3000000;
                animationMessage.animationreps = 1;
                animationMessage.num_devices = addressCounter -1;
                Serial.println("Starting animation with num devices "+String(animationMessage.num_devices));
                pushDataToSendQueue(broadcastAddress, MSG_ANIMATION, -1);
                nextAnimationPing = millis()+handleLed->calculate(&animationMessage);
                Serial.println("Animation ends at "+String(millis()));
                globalModeHandler->switchMode(MODE_ANIMATE);
                //endAnimation = true;
            }  
            else {
                if (globalModeHandler->getMode() == MODE_ANIMATE or globalModeHandler->getMode() == MODE_NEUTRAL) {
                addError("Blinking\n");
                globalModeHandler->switchMode(MODE_ANIMATE);
                memcpy(&animationMessage, incomingData, sizeof(animationMessage));
                handleLed->setupAnimation(&animationMessage);
                nextAnimationPing = millis()+handleLed->calculate(&animationMessage);
                Serial.println("setting endanimation to true");
                //endAnimation = true;
                }
            }
        break;
        case MSG_SET_POSITIONS: 
            addError("Setting Positions\n");
            if (DEVICE_MODE == MAIN) {
                memcpy(&setPositionsMessage, incomingData, sizeof(setPositionsMessage));
                clientAddresses[setPositionsMessage.id].xLoc = setPositionsMessage.xpos;
                if (setPositionsMessage.xpos > maxPos) {
                    maxPos = setPositionsMessage.xpos;
                }
                clientAddresses[setPositionsMessage.id].yLoc = setPositionsMessage.ypos;
                clientAddresses[setPositionsMessage.id].zLoc = setPositionsMessage.zpos;
                writeStructsToFile(clientAddresses, NUM_DEVICES, "/clientAddress");
                pushDataToSendQueue(clientAddresses[setPositionsMessage.id].address, MSG_SET_POSITIONS, -1);
            }
            else if (DEVICE_MODE == CLIENT) {
                memcpy(&setPositionsMessage, incomingData, sizeof(setPositionsMessage));
                handleLed->setLocation(setPositionsMessage.xpos, setPositionsMessage.ypos, setPositionsMessage.zpos);
            }
        break;

        case MSG_SET_SLEEP_WAKEUP: {
            memcpy(&setSleepWakeupMessage, incomingData, sizeof(setSleepWakeupMessage));
            addError("Setting Sleep Wakeup\n");
            if (setSleepWakeupMessage.isGoodNight == true) {
                addError("Setting sleep: "+String(setSleepWakeupMessage.hours)+"\n");
                sleepTime.hours = setSleepWakeupMessage.hours;
                sleepTime.minutes = setSleepWakeupMessage.minutes;
                sleepTime.seconds = setSleepWakeupMessage.seconds;
            }
            else {
                addError("Setting wakeup: "+String(setSleepWakeupMessage.hours)+"\n");
                wakeupTime.hours = setSleepWakeupMessage.hours;
                wakeupTime.minutes = setSleepWakeupMessage.minutes;
                wakeupTime.seconds = setSleepWakeupMessage.seconds;
            }
            break;
        }
        default: 
            addError("message not recognized");
            addError(messageCodeToText(incomingData[0]));
            addError("\n");

            break;
    }
}

