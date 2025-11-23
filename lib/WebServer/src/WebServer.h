#ifndef WEBSERV
#define WEBSERV
#if DEVICE_MODE == MASTER
#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include "ESPAsyncWebServer.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
// #include "../../include/myDefines.h"
#include <MyDefines.h>
#include <queue>
#include <mutex>
#include <cstdint>
//#include <helperFuncs.h>

class MessageHandler;

class WebServer {
    private:
        MessageHandler* messageHandlerInstance = nullptr;
        FS* filesystem;
        AsyncWebServer server;
        AsyncEventSource events;
        WebServer(FS* fs);
        bool connected = false;
        bool inCalibration = false;
        bool inDistanceCalibration = false;
    public:
        static WebServer& getInstance(FS* fs);
        WebServer(const WebServer&) = delete;
        WebServer& operator=(const WebServer&) = delete;
        bool PdParamsChanged = false;
        void setup(MessageHandler &globalMessageHandler);
        void setWifi();
        void end();
        void begin();
        void configRoutes();
        void handleClientConnect(AsyncEventSourceClient *client);
        void commandAnimate(AsyncWebServerRequest *request);
        void setTime(AsyncWebServerRequest *request);
        void submitPositions(AsyncWebServerRequest *request);  
        void commandSync(AsyncWebServerRequest *request);
        void commandSyncAll(AsyncWebServerRequest *request);
        void commandBlink(AsyncWebServerRequest *request);
        void commandBlinkAll(AsyncWebServerRequest *request);
        void commandStartCalibration(AsyncWebServerRequest *request);
        void commandOTAUpdate(AsyncWebServerRequest *request);
        void commandCancelCalibration(AsyncWebServerRequest *request);
        void commandResetCalibration(AsyncWebServerRequest *request);
        void commandContinueCalibration(AsyncWebServerRequest *request);
        void commandEndCalibration(AsyncWebServerRequest *request);
        void commandTestCalibration(AsyncWebServerRequest *request);
        void commandMessage(AsyncWebServerRequest *request);
        void commandCalibrate(AsyncWebServerRequest *request);
        void commandStartDistanceCalibration(AsyncWebServerRequest *request);
        void commandContinueDistanceCalibration(AsyncWebServerRequest *request);
        void commandCancelDistanceCalibration(AsyncWebServerRequest *request);
        void commandEndDistanceCalibration(AsyncWebServerRequest *request);
        void commandAnimationOff(AsyncWebServerRequest *request);
        void resetSystem(AsyncWebServerRequest *request);
        void setMidiParams(AsyncWebServerRequest *request);
        void getMidiParams(AsyncWebServerRequest *request);
        void getDarkroomParams(AsyncWebServerRequest *request);
        void setDarkroomParams(AsyncWebServerRequest *request);
        void setSleepTime(AsyncWebServerRequest *request);
        void setWakeupTime(AsyncWebServerRequest *request);
        void clapReceived(int clapId, unsigned long long clapTime);
        void clapReceivedClient(int clapId, int boardId, float clapDistance);
        void getAddressList(AsyncWebServerRequest *request);
        void serveOnNotFound(AsyncWebServerRequest *request);
        void updateAddressList();
        void setCalculationDone(bool done);
        void updateAddress(int id);
        String jsonFromAddress(int id);


};
#endif
#endif