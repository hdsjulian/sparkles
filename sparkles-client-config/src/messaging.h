

#include "myDefines.h"
#include "Arduino.h"
#include "stateMachine.h"
#if DEVICE_MODE != WEBSERVER
#include "ledHandler.h"
#endif
#if DEVICE_MODE == MAIN
#include "webserver.h"
class webserver;
#endif
#include <iostream>
#include <queue>
#include <mutex>
#include <vector>
#include <cstdint>
#include "LittleFS.h"
#include <ArduinoJson.h>

#ifndef MESSAGING_H
#define MESSAGING_H




class messaging {
    private:  
        client_address clapDevice;
        float myDistances[NUM_CLAPS];
        modeMachine* globalModeHandler;
        #if DEVICE_MODE == MAIN
        webserver* webServer;
        #endif
        #if DEVICE_MODE != WEBSERVER
        ledHandler* handleLed;
        #endif
        modeMachine messagingModeHandler;
        int debugVariable = 0;
        struct SendData {
          const uint8_t * address;
          int messageId;
          int param1 =-1;
          int param2 =0;
        };
        
        unsigned long long arriveTime, receiveTime, sendTime, lastDelay, lastTime, timeOffset;
        int offsetMultiplier = 1;
        int timerCounter = 0;
        int timerArray[TIMER_ARRAY_COUNT];
        int arrayCounter =0;
        int delayAvg = 0;
        int tick = 0;
        unsigned long long oldMsgReceiveTime; 
        esp_now_peer_info_t* peerInfo;
        bool haveSentAddress = false;
      struct ReceivedData {
          uint8_t* senderAddress;
          const uint8_t* incomingData;
          int len;
          unsigned long long msgReceiveTime;
          
      };

      std::queue<ReceivedData> dataQueue;
      std::queue<SendData> sendQueue;
      std::mutex sendQueueMutex;
      std::mutex receiveQueueMutex;
      int msgCounter = 0;
    public: 
        client_address clientAddresses[NUM_DEVICES];

        int addressCounter = 0;
        int msgSendTime;
        int announceTime = 0;
        int myAddressId = 0;
        message_animate animationMessage;
        message_send_clap_times sendClapTimes, myClapTimes;
        message_address addressMessage; 
        message_timer timerMessage;
        message_got_timer gotTimerMessage;
        message_announce announceMessage;
        message_switch_mode switchModeMessage;
        message_timer_received timerReceivedMessage;
        message_address_list addressListMessage;
        message_command commandMessage;
        message_status_update statusUpdateMessage;
        message_distance distanceMessage;
        message_set_positions setPositionsMessage;
        message_status statusMessage;
        message_set_sleep_wakeup setSleepWakeupMessage;
        message_send_single_clap sendSingleClapMessage;
        message_timesync timeSyncMessage;
        message_ota otaMessage;
        message_midi midiMessage;
        clap_device_location clapDeviceLocations[NUM_CLAPS];
        String error_message = "";
        String message_received = "";
        String message_sent = "";
        String messageLog = "";
        sleep_wakeup_time sleepTime;
        sleep_wakeup_time wakeupTime;
        bool gotTimer = false;
        bool receivedAnnounce = false;
        uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        uint8_t emptyAddress[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        //sticker host address
        //uint8_t hostAddress[6] = {0x34, 0x85, 0x18, 0x8f, 0xc1, 0x74 };
        // host 3.16
        //uint8_t hostAddress[6] = {0x34, 0x85, 0x18, 0x95, 0xea, 0x54 };
        //host 3.1
        //uint8_t hostAddress[6] = {0x34, 0x85, 0x18, 0x8f, 0xc0, 0x80 };
        //host 1 15
        //uint8_t hostAddress[6] = {0x34, 0x85, 0x18, 0x8e, 0xf9, 0x20 };
        //host 2.44
        uint8_t hostAddress[6] = {0x34, 0x85, 0x18, 0x8e, 0xf9, 0x68 };
        //blue host address
        //uint8_t hostAddress[6] = {0x68, 0xb6, 0xb3, 0x08, 0xe9, 0xae}; 
        uint8_t clientAddress[6] = {0x68, 0xb6, 0xb3, 0x08, 0xbd, 0x8a};
        uint8_t myAddress[6];
        uint8_t timerReceiver[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        uint8_t otaAddress[6];
        unsigned long long ClapTime; 
        int clapsReceived = 0;
        timeout_retry timeoutRetry;
        int timersUpdated =0;
        int goToSleepTime = 0;
        bool sentToSleep = false;
        int forcedDebugCounter = 0;
        unsigned long long lastTry = 0;
        unsigned long long nextAnimationPing;
        int maxPos;
        int announceCounter = 0;
        bool finishAnimation = false;
        unsigned long long lastBroadcastTimer = 0;
        //esp8266
        //uint8_t webserverAddress[6] = {0xe8, 0xdb, 0x84, 0x99, 0x5e, 0x44};
        //        uint8_t clapDeviceAddress[6] = {0x80, 0x65, 0x99, 0xc7, 0xc2, 0x3c};
        uint8_t clapDeviceAddress[6] = {0x64, 0xe8, 0x33, 0x54, 0x3c, 0x24};
        messaging();
        #if DEVICE_MODE == WEBSERVER
          void setup(modeMachine &modeHandler, esp_now_peer_info_t &globalPeerInfo);
        #elif DEVICE_MODE == MAIN
          void setup(modeMachine &modeHandler, ledHandler &globalHandleLed, esp_now_peer_info_t &globalPeerInfo, webserver &myWebserver);
        #else
          void setup(modeMachine &modeHandler, ledHandler &globalHandleLed, esp_now_peer_info_t &globalPeerInfo);

        #endif
        void init();
        void blink();
        void removePeer(uint8_t address[6]);
        void printAddress(const uint8_t * mac_addr);
        int addPeer(uint8_t * address);
        void setHostAddress(uint8_t address[6]);
        void handleGotTimer(const uint8_t *incomingData, uint8_t * address);
        int getLastDelay();
        void setLastDelay(int delay);
        void handleClapTimes(const uint8_t *incomingData);
        void calculateDistances(int id);
        void unifyDistances(int id);
        void addClap(unsigned long long timeStamp);
        void getClapTimes(int i);
        int getTimerCounter();
        void setTimerCounter(int counter);
        void incrementTimerCounter();
        void setSendTime(unsigned long long time);
        void setArriveTime(unsigned long long time);
        unsigned long long getSendTime();
        unsigned long long getTimeOffset();
        void setTimeOffset();
        unsigned long long getArriveTime();
        void printMessage(int message);
        void receiveTimer(unsigned long long messageArriveTime);
        void prepareAnnounceMessage();
        void prepareTimerMessage();
        void printBroadcastAddress();
        void printAllPeers();
        void printAllAddresses();
        void stringAllAddresses();
        void setTimerReceiver(const uint8_t *incomingData);
        void announceAddress();
        void respondTimer();
        int getMessagingMode();
        void setMessagingMode(int mode);
        void handleErrors();
        void addError(String error, bool noNL = true);
        void handleReceived();
        void handleSent();
        void addSent(String sent);
        void pushDataToReceivedQueue(uint8_t* senderAddress, const uint8_t* incomingData, int len, unsigned long long msgReceiveTime);
        void processDataFromReceivedQueue();
        void pushDataToSendQueue(const uint8_t * address, int messageId, int param1, int param2=0);
        void pushDataToSendQueue(int messageId, int param1, int param2=0);
        void processDataFromSendQueue();
        void handleReceive(uint8_t* senderAddress, const uint8_t *incomingData, int len, unsigned long long  msgReceiveTime);
        void printMessagingMode();
        String stringAddress(const uint8_t * mac_addr);
        void printMessageModeLog();
        String getMessageLog();
        void addMessageLog(String message);
        String messageCodeToText(int message);
        void prepareSendAddress(int i);
        int getAddressId(const uint8_t * address);
        int getAddressCounter();
        void filterClaps(int index);
        void writeStructsToFile(const client_address* data, int count, const char* filename);
        bool readStructsFromFile(client_address* data, int count, const char* filename);
        void deleteFile(const char* filename);
        void checkFile(const char* filename);
        void updateTimers(int addressId);
        void goToSleep(unsigned long long sleepTime);
        void handleTimerUpdates();
        void globalHandleTimerUpdates();
        void setNoSuccess();
        void goodNight();
        void setGoodNight(int hours, int minutes, int seconds);
        void setWakeup(int hours, int minutes, int seconds);
        void setClock(int year, int month, int day, int hours, int minutes, int seconds);
        int* getSystemTime();
        double calculateTimeDifference(int year1, int month1, int day1, int hours1, int minutes1, int seconds1, int hours2, int minutes2, int seconds2);
        void setSetTimeMessage(int hours, int minutes, int seconds);
        void setBattery();
        float getBattery();
        void setTimerReceiverUnavailable();
        void setAnimation(message_animate* message);
        void nextAnimation();
        void forceDebug(int i = 0);
        void setGoodNightWakeUp(int hours, int minutes, int seconds, bool isGoodNight);
        void confirmClap(int id, float xpos, float  ypos, float zpos);
        void setPositions(int id, float xpos, float  ypos, float zpos);
        double calculateGoodNight(bool sleepWakeup);
        void switchMode(int mode);
        void sendCommand(int commandId);
        void sendMode(int modeId);
        void sendMessageById(int messageId, int addressId, int param) ;
        void receiveClapTimes(uint8_t * senderAdddress);
        void timeoutRetryHandler();
        void resetTimer();
        void setUnreachable(int id);
        void nextRetry();
        void updateDevice(int id);
        void orderClaps(int id);
        void startCalibrationMode();
        void printTimerStuff();
        void clientAddressToJsonObject(JsonObject& jsonObj, client_address& client);
        String allClientAddressesToJson();
        String printClapTimes(unsigned long long* array, int size);
        void updateAddressesToWebserver();
        void sendSingleClap(unsigned long long buttonPressTime);
        void deleteClap(int clapId);
        void resetCalibration();
        bool arePointsEqual(clap_device_location &point1, clap_device_location &point2);
        bool arePointsEqual(float point1[3], clap_device_location &point2);
        void estimatePoints(int id, float* x0);
        void testTrilateration();
        void calculate_distances(const float point[3]);
        float scaling_factor(float value);
        void weighted_distances_to_points(int numClaps, const float x[3], const calculation_struct pointsDistances, float result[NUM_CLAPS]);
        void groupPoints(int indices[NUM_CLAPS][CLAPS_PER_POINT]);
        int checkAndAverage(float x0[3], float x1[3], float x2[3]);
        void handleSingleClap();
        void sendTimeSync(int id = -1);
        void receiveBroadcastTimer(unsigned long long messageArriveTime);
        void broadcastTimer();
        void setAddressesInactive();
        int getTimeoutRetryId();
        void startAnimation();
        void endAnimation();
        bool allTimersUpdated();
        float largestDistance();
        void setDistanceFromCenter();
        void resetSystem();
        unsigned long long getLastBroadcastTimer();
        void setLastBroadcastTimer();
        void printPeers();
        void setGlobalBrightness(int brightness);
        String getLedHandlerParams();
        void setSyncAsyncParams(int minS, int maxS, int minP, int maxP, int minSp, int maxSp, int minR, int maxR, int minAR, int maxAR, int minRGBR, int maxRGBR, int minRGBG, int maxRGBG, int minRGBB, int maxRGBB);
        void handleOTA();
        void performOTAUpdate();
        void handleMidi(const uint8_t *incomingData);
        void sendMidi(uint8_t note, uint8_t velocity);
};




#endif
