#include "Arduino.h"
#include "myDefines.h"
#ifndef MODE_MACHINE_H
#define MODE_MACHINE_H


    class modeMachine {
        private: 
            int currentMode = MODE_INIT;
            String modeLog = "";
        public:
            modeMachine();
            void switchMode(int mode);
            void logMode(int mode);
            void printLog();
            int getMode();
            void printMode(int mode);
            String modeToText(int mode);
            void printCurrentMode();
    };
#endif