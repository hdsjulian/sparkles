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
    public:
        static WebServer& getInstance(FS* fs);
        WebServer(const WebServer&) = delete;
        WebServer& operator=(const WebServer&) = delete;
        bool PdParamsChanged = false;
        void setup(MessageHandler &globalMessageHandler);
        void setWifi();
        
        void configRoutes();
        void handleClientConnect(AsyncEventSourceClient *client);
        void commandAnimate(AsyncWebServerRequest *request);
        void commandGoToSleep(AsyncWebServerRequest *request);

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
        void getAddressList(AsyncWebServerRequest *request);
        void setNeutral(AsyncWebServerRequest *request);
        void triggerSync(AsyncWebServerRequest *request);
        void testAnim(AsyncWebServerRequest *request);
        void submitPdParams(AsyncWebServerRequest *request);
        void updateMode(String modeText);
        void setFull();
        void resetSystem(AsyncWebServerRequest *request);
        void setSyncAsyncParams(AsyncWebServerRequest *request);
        void serveOnNotFound(AsyncWebServerRequest *request);
        String jsonFromAddress(int id);
        void updateAddress(client_address address);
        void begin();
        void end();
        void updateAddress(int id);

};
#endif
#endif