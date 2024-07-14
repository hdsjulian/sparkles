#ifndef WEBSERV
#define WEBSERV
#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include "ESPAsyncWebServer.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <myDefines.h>
#include <queue>
#include <mutex>
#include <cstdint>
//#include <helperFuncs.h>
#include <messaging.h>
#include <stateMachine.h>
#define CALIBRATION_NOT_HAPPENED 0
#define CALIBRATION_IN_PROGRESS 1
#define CALIBRATION_CLAP_CHECK 2
#define CALIBRATION_ENDED 3
#define CALIBRATION_IN_BETWEEN 4
class webserver {
    private:
        const char* ssid = "Sparkles-Admin";
        const char* password = "sparkles";
        int msgType;
        String outputJson;
        int calibrationStatus;
        bool connected = false;
        bool isSetup = false;

    public:
    bool PdParamsChanged = false;
    int lag = 48;
    int threshold = 8;
    double influence = 0.7;
    int counter = 0;
    webserver(FS* fs);
    void setup(messaging &Messaging, modeMachine &modeHandler);
    void setWifi();
    void serveStaticFile(AsyncWebServerRequest *request);
    AsyncWebServer server;
    AsyncEventSource events;
    FS* filesystem;
    messaging* messageHandler;
    modeMachine* stateMachine;
    int debugVariable = 0;
    void configRoutes();
    void handleClientConnect(AsyncEventSourceClient *client);
    void commandAnimate(AsyncWebServerRequest *request);
    void commandGoToSleep(AsyncWebServerRequest *request);
    void updateDeviceList(AsyncWebServerRequest *request);
    void updateDeviceList();
    void commandGoodNight(AsyncWebServerRequest *request);
    void commandSetWakeup(AsyncWebServerRequest *request);
    void setTime(AsyncWebServerRequest *request);
    void submitPositions(AsyncWebServerRequest *request);
    void confirmClap(AsyncWebServerRequest *request);
    void cancelClap(AsyncWebServerRequest *request);    
    void resetCalibration(AsyncWebServerRequest *request); 
    void commandCalibrate(AsyncWebServerRequest *request);
    void updateCalibrationStatus(AsyncWebServerRequest *request);
    void commandCalculate(AsyncWebServerRequest *request);
    void endCalibration(AsyncWebServerRequest *request);   
    void sendSyncAsyncAnimation(AsyncWebServerRequest *request);
    void statusUpdate(AsyncWebServerRequest *request);
    void statusUpdate();
    void setNeutral(AsyncWebServerRequest *request);
    void submitPdParams(AsyncWebServerRequest *request);
    void updateMode(String modeText);
    void setFull();

};
#endif
