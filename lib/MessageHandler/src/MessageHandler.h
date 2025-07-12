#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H
#include <Arduino.h>
#include <esp_now.h>
#include <LedHandler.h>
#include "LittleFS.h"
#include <WebServer.h>
#include <MyDefines.h>
#include <Version.h>
#include <time.h>
class MessageHandler
{
public:
    static constexpr uint8_t emptyAddress[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t OTAUpdateAddress[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; 
    bool isOTAUpdating = false;
    bool nextOTAAddress = false;
    bool requestingOTAUpdate = false;
    int otaUpdateAddressId = -1;
    bool hasClapHappened = false;
    

    //singleton
    static MessageHandler& getInstance() {
        static MessageHandler instance; // Guaranteed to be destroyed and instantiated on first use
        return instance;
    }
    void setup(LedHandler &globalHandleLed);
    //base messaging functions
    static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
    static void onDataRecv(const esp_now_recv_info *mac, const uint8_t *incomingData, int len);
    void pushToRecvQueue(const esp_now_recv_info *mac, const uint8_t *incomingData, int len);
    void pushToSendQueue(message_data& msg);
    Version version;


    //getters and setters
    int getCurrentTimerIndex();
    void setCurrentTimerIndex(int index);
    void setTimerSet(bool set);
    bool getTimerSet();
    void setSettingTimer(bool set);
    bool getSettingTimer();
    void setSettingClapSync(bool set);
    bool getSettingClapSync();
    void setAddressAnnounced(bool set);
    bool getAddressAnnounced();
    client_address getItemFromAddressList(int index);
    void setItemFromAddressList(int index, client_address item);
    int addOrGetAddressId(uint8_t * address);
    int getLastDelay();
    void setLastDelay(int delay);
    unsigned long long getLastReceiveTime();
    void setLastReceiveTime(unsigned long long time);
    void setTimeOffset(unsigned long long sendTime, unsigned long long receiveTime, int delay);
    long long getTimeOffset();
    bool getTimerReset();
    void setTimerReset(bool set);
    void setLastTimerCounter();
    int getLastTimerCounter();
    int incrementTimerCounter();
    int getTimerCounter();
    void setTimerCounter(int counter);
    void setUnavailable(int index);
    activeStatus getActiveStatus(int index);
    float getBatteryPercentage();
    void setNumDevices(int num);
    int getNumDevices();
    void recordTimeOfDayBeforeSleep();
    void setTimeOfDayAfterSleep();
    message_data retrieveCommand(uint8_t * address);
    void setCommand(message_data command, uint8_t * address);
    void setAddressListInactive();
    message_data createClapMessage(bool isHost = false);
    message_data createConfigMessage(int boardId);
    message_data createUpdateVersionMessage(Version version);
    message_data createCommandMessage(int commandType, bool isBroadcast = false);
    message_data createStatusMessage();
    

    void setBoardPosition(int boardId, float xPos, float yPos);
    void setClap(float xPos, float yPos);
    int getClapIndex();
    bool getBatteryLow();
    void setBatteryLow(bool low);

    clap_table getClap(int index);
    void setIsOTAUpdating(bool isUpdating);
    bool getIsOTAUpdating();
    void setOTAUpdateAddressId(int id);
    int getOTAUpdateAddressId();
    void setRequestingOTAUpdate(bool request);
    bool getRequestingOTAUpdate();
    void setNextOTAAddress(bool next);
    bool getNextOTAAddress();
    unsigned long long getLastClapTime();
    void setLastClapTime(unsigned long long time);
    void setSleepTime(int hours, int minutes, int seconds);
    void setWakeupTime(int hours, int minutes, int seconds);
    void setHasClapHappened(bool hasClapHappened);
    bool getHasClapHappened();
    void setClapDeviceDelay(int delay);
    int getClapDeviceDelay();
    
    unsigned long getSleepTime();
    bool isInSleepPhase();
    unsigned long setNextSleepMillis();
    unsigned long getSleepDuration();
    bool isSleepSet();
    unsigned long getAdminPresent();
    void setAdminPresent(unsigned long time);
    //tasks
    void startTimerSyncTask();
    void startBatterySyncTask();
    void startWiFiToggleTask();
    void startAllTimerSyncTask();
    void startClapTask();
    void startOTAUpdateTask();
    void startCalculatePositionsTask();
    void startAnnounceAddressTask();
    void startClapSyncTask();
    void handleSleepWakeup(message_data incomingData);
    void handleReceive();
    void handleSend();
    void runAnnounceAddress();
    void runTimerSync();
    void runBatterySync();
    void toggleWiFiTask();
    void runClapTask();
    void runOTAUpdateTask();
    void runCalculatePositionsTask();
    void runClapSync();
    int addPeer(uint8_t * address);
    bool addressAnnounced = false;
    void sendAnimation(message_animation animationMessage, int addressId);
    //helpers

    void printAllPeers();
    void printAllAddresses();
    void printAddress(const uint8_t * mac_addr);
    String stringAddress(const uint8_t * mac_addr, bool debug);
    void stringAllAddresses();
    void removePeer(uint8_t * address); 
    unsigned long long timeDiffAbs(unsigned long long a, unsigned long long b);
    void sendSystemStatus();
    void turnWifiOn();
    void turnWifiOff();
    float convertMicrosToMeters(unsigned long long timeInMicros);
    void continueCalibration(float xPos, float yPos);
    void endCalibration();
    void startCalibrationMaster();
    void startCalibrationClient();
    void cancelCalibration();
    void resetCalibration();
    void sendSleepWakeupMessage(unsigned long sleepDuration);
    void commandCalibrate(int boardId);
private:
    // Static Constants
    static constexpr uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static constexpr uint8_t hostAddress[6] = {0x34, 0x85, 0x18, 0x8f, 0xc1, 0x48};
    uint8_t clapDeviceAddress[6] = {0x64, 0xe8, 0x33, 0x54, 0x3c, 0x24};

    // Static Members
    static MessageHandler* instance;

    // Member Variables
    struct tm beforeSleepTimeinfo;
    unsigned long long beforeSleepMillis;
    int beforeSleepSecondsOfDay = 0;
    int currentTimerIndex = 0;
    int numDevices = 0;
    unsigned long long lastSendTime = 0;
    int lastDelay = 0;
    unsigned long long msgReceiveTime;
    const uint8_t *incomingData;
    client_address addressList[NUM_CLIENTS];
    SemaphoreHandle_t configMutex, updateOTAMutex;
    TaskHandle_t announceTaskHandle, timerSyncHandle, allTimerSyncHandle, batterySyncHandle, wifiToggleTask, otaUpdateHandle, clapTaskHandle, calculatePositionsHandle, clapSyncHandle, handleSendHandle, handleReceiveHandle;
    esp_now_peer_info_t peerInfo;
    esp_now_peer_num_t peerNum;
    QueueHandle_t receiveQueue, sendQueue ;
    LedHandler* ledInstance = nullptr;
    int delays[10];
    int timerCounter = 0;
    int lastTimerCounter = 0;
    int delayAverage = 0;
    int delayCounter = 0;
    bool timerSet = false;
    bool settingTimer = false;
    bool settingClapSync = false;
    bool timerReset = false;
    int lastTimerArrived = 0;
    long long timeOffset = 0;
    unsigned long long lastReceiveTime = 0;
    int offsetMultiplier = 1;
    const int wifiSleepTime = 60000;
    bool nextCommandReceived = false;
    int currentCommandId = 0;
    int sleepTimeHours = 0;
    int sleepTimeMinutes = 0;
    int sleepTimeSeconds = 0;
    int wakeupTimeHours = 0;
    int wakeupTimeMinutes = 0;
    int wakeupTimeSeconds = 0;
    int clapDeviceDelay = 0;
    bool isBatteryLow = false;
    clap_table clapTable[NUM_CLAPS] = {0};
    unsigned long lastAdminPresent = 0;
    int clapIndex = 0;
    unsigned long long lastClapTime = 0;
    // Constructor and Deleted Functions
    MessageHandler();
    MessageHandler(const MessageHandler&) = delete;
    MessageHandler& operator=(const MessageHandler&) = delete;

    // Static Wrapper Functions
    static void handleReceiveWrapper(void *pvParameters);
    static void runTimerSyncWrapper(void *pvParameters);
    static void announceAddressWrapper(void *pvParameters);
    static void handleSendWrapper(void *pvParameters);
    static void runAllTimerSyncWrapper(void *pvParameters);
    static void runBatterySyncWrapper(void *pvParameters);
    static void runToggleWiFiTaskWrapper(void *pvParameters);
    static void runClapTaskWrapper(void *pvParameters);    
    static void runOTAUpdateTaskWrapper(void *pvParameters);
    static void calculatePositionsTaskWrapper(void *pvParameters);
    static void runClapSyncWrapper(void *pvParameters);
    // Member Functions
    int getLastSendTime();
    void setLastSendTime(unsigned long long time);
    void setMsgReceiveTime(unsigned long long time);
    unsigned long long getMsgReceiveTime();
    void handleTimer(message_data incomingData);
    void handleSystemStatus(message_data incomingData);
    void handleAddressStruct();
    bool readStructsFromFile(client_address* data, int count, const char* filename);
    void writeStructsToFile(client_address* data, int count, const char* filename);
    void goToSleep();

};
#endif