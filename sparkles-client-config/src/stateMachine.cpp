#include "Arduino.h"
#include "stateMachine.h"

 
modeMachine::modeMachine() {

}
#if DEVICE_MODE == MAIN
void modeMachine::setup(webserver &myWebserver) {
    webServer = &myWebserver;
}
#endif
void modeMachine::switchMode(int mode) {
    ESP_LOGI("MODE", "Switched Mode to ");
    printMode(mode);
    currentMode = mode;

    logMode(mode);
     #if DEVICE_MODE == MAIN
        String modeText = "{\"status\" : \""+modeToText(mode)+"\"}";
        webServer->updateMode(modeText);
    #endif
}

void modeMachine::revertToPreviousMode() {
    //Serial.println("reverting "+String(previousMode));
    ESP_LOGI("MODE", "revert");
    switchMode(previousMode);
}
void modeMachine::setPreviousMode(bool override) {
    if (getMode() != MODE_NEUTRAL && getMode() != MODE_INIT && override != true) {
    ESP_LOGI("mode", "setting previous mode %d", currentMode);
    previousMode = currentMode;
    }
    else {
        Serial.println("called setPrev but mode is neutral");
    }
}
int modeMachine::getPreviousMode() {
    return previousMode;
}
void modeMachine::logMode(int mode) {
    modeLog += "modeSwitch ";
    modeLog += modeToText(mode);
    modeLog += "\n";
}
void modeMachine::printLog() {
    Serial.println(modeLog);
}
int modeMachine::getMode() {
    return currentMode;
}
void modeMachine::printMode(int mode) { 
    Serial.println(modeToText(mode));   
}
String modeMachine::modeToText(int mode) {
    String out ;
    out = "Mode: ";
    switch (mode) {
        case MODE_INIT:
        out += "MODE_INIT";
        break;
        case MODE_SEND_ANNOUNCE:
        out += "MODE_SEND_ANNOUNCE";
        break;
    case MODE_SENDING_TIMER:
        out += "MODE_SENDING_TIMER";
        break;
    case MODE_STARTUP:
        out += "MODE_STARTUP";
        break;
    case MODE_WAIT_FOR_TIMER:
        out += "MODE_WAIT_FOR_TIMER";
        break;
    case MODE_CALIBRATE: 
        out += "MODE_CALIBRATE";
        break;
    case MODE_ANIMATE:
        out += "MODE_ANIMATE";
        break;
    case MODE_NO_SEND: 
        out += "MODE_NO_SEND";
        break;
    case MODE_RESPOND_ANNOUNCE:
        out += "MODE_RESPOND_ANNOUNCE";
        break;
    case MODE_RESPOND_TIMER:
        out += "MODE_RESPOND_TIMER";
        break;
    case MODE_WAIT_TIMER_RESPONSE: 
        out += "MODE_WAIT_TIMER_RESPONSE";  
    break;      
    case MODE_WAIT_ANNOUNCE_RESPONCE:
        out += "MODE_WAIT_ANNOUNCE_RESPONCE";
        break;
    case MODE_RESET_TIMER:
        out += "MODE_RESET_TIMER";
        break;
    case MODE_NEUTRAL:
        out += "MODE_NEUTRAL";
        break;
    case MODE_GET_CALIBRATION_DATA:
        out += "MODE_GET_CALIBRATION_DATA";
        break;        
    case MODE_CLAPPING:
        out += "MODE_CLAPPING";
    break;        
    case MODE_WOKEUP: 
        out += "MODE_WOKEUP";
    break;
    case MODE_BROADCAST_TIMER: 
        out += "MODE_BROADCAST_TIMER";
    break;
    case MODE_PRE_CALIBRATION_BROADCAST:
        out += "MODE_PRE_CALIBRATION_BROADCAST";
        break;
    default: 
        out += "Mode unknown ";
        out += String(mode); // Convert 'mode' integer to String and concatenate
        break;
    }   
    return out;
}
void modeMachine::printCurrentMode() {
    printMode(currentMode);
}

