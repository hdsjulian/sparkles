#include "Arduino.h"
#include "myDefines.h"
#if DEVICE_MODE == MAIN
#include "webserver.h"
class webserver;
#endif
#ifndef MODE_MACHINE_H
#define MODE_MACHINE_H


    class modeMachine {
        private: 
            int currentMode = MODE_INIT;
            int previousMode = MODE_INIT;
            String modeLog = "";
            #if DEVICE_MODE == MAIN
            webserver* webServer = nullptr;
            #endif
        public:
            modeMachine();
            #if DEVICE_MODE == MAIN
            void setup(webserver &myWebserver);
            #endif
            void switchMode(int mode);
            void logMode(int mode);
            void printLog();
            int getMode();
            void printMode(int mode);
            String modeToText(int mode);
            void printCurrentMode();
            int getPreviousMode();
            void revertToPreviousMode();
            void setPreviousMode();
    };
#endif