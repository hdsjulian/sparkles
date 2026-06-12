#if DEVICE_MODE == MASTER
#include "MessageHandler.h"
#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"


void MessageHandler::startTimerSyncTask() {
    if (timerSyncHandle == NULL)
        {

            xTaskCreatePinnedToCore(runTimerSyncWrapper, "runTimerSync", 10000, this, 2, &timerSyncHandle, 1);
        }
}
void MessageHandler::startClapSyncTask() {
    if (clapSyncHandle == NULL)
        {
            xTaskCreatePinnedToCore(runClapSyncWrapper, "runClapSync", 10000, this, 2, &clapSyncHandle, 1);
        }
}

void MessageHandler::startAllTimerSyncTask() {
    ESP_LOGI("TIMER", "Starting all timer sync task");
    if (allTimerSyncHandle == NULL)
        {
            xTaskCreatePinnedToCore(runAllTimerSyncWrapper, "runAllTimerSync", 10000, this, 2, &allTimerSyncHandle, 1);
        }
}

int MessageHandler::acquireTxSlot(const uint8_t *mac) {
    int slot = -1;
    portENTER_CRITICAL(&txSlotsMux);
    for (int i = 0; i < TX_SLOT_COUNT; i++) {
        if (!txSlots[i].inUse) {
            memcpy(txSlots[i].mac, mac, 6);
            txSlots[i].sendTime = 0;
            txSlots[i].lastDelay = 0;
            txSlots[i].inUse = true;
            slot = i;
            break;
        }
    }
    portEXIT_CRITICAL(&txSlotsMux);
    if (slot < 0) {
        ESP_LOGW("TIMER", "No free TX slot");
    }
    return slot;
}

void MessageHandler::releaseTxSlot(int slot) {
    if (slot >= 0 && slot < TX_SLOT_COUNT) {
        txSlots[slot].inUse = false;
    }
}

void MessageHandler::recordTxDelay(const uint8_t *mac) {
    unsigned long long now = esp_timer_get_time();
    for (int i = 0; i < TX_SLOT_COUNT; i++) {
        if (txSlots[i].inUse && txSlots[i].sendTime != 0 && memcmp(txSlots[i].mac, mac, 6) == 0) {
            txSlots[i].lastDelay = (int)(now - txSlots[i].sendTime);
            txSlots[i].sendTime = 0; // consumed, one measurement per send
            return;
        }
    }
}

// lastDelay is uint16 on the wire, 0 tells the device to skip the sample
static uint16_t clampTxDelay(int delay) {
    return (delay > 0 && delay < 60000) ? (uint16_t)delay : 0;
}

// Fast resync, parallel pool of FAST_RESYNC_POOL workers, one per client
#define FAST_RESYNC_POOL 5

struct FastResyncArgs {
    MessageHandler* self;
    int index;
};

static void fastResyncWorker(void* pv) {
    FastResyncArgs* a = (FastResyncArgs*)pv;
    a->self->runTimerSyncAt(a->index);
    delete a;
    vTaskDelete(NULL);
}

void MessageHandler::runFastResyncAll() {
    ESP_LOGI("TIMER", "Fast resync starting");

    // count known clients
    int total = 0;
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (memcmp(getItemFromAddressList(i).address, emptyAddress, 6) == 0) break;
        total++;
    }

    // dispatch clients in batches of FAST_RESYNC_POOL
    for (int i = 0; i < total; i += FAST_RESYNC_POOL) {
        int batch = min(FAST_RESYNC_POOL, total - i);
        TaskHandle_t handles[FAST_RESYNC_POOL] = {};
        for (int j = 0; j < batch; j++) {
            FastResyncArgs* args = new FastResyncArgs{this, i + j};
            char name[16];
            snprintf(name, sizeof(name), "frsync_%d", i + j);
            xTaskCreatePinnedToCore(fastResyncWorker, name, 4096, args, 2, &handles[j], 1);
        }
        // wait for batch to finish before starting next
        for (int j = 0; j < batch; j++) {
            if (handles[j]) {
                while (eTaskGetState(handles[j]) != eDeleted) {
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                }
            }
        }
    }

    ESP_LOGI("TIMER", "Fast resync done");
}

void MessageHandler::startFastResyncTask() {
    if (fastResyncHandle != NULL) {
        ESP_LOGI("TIMER", "Fast resync already running");
        return;
    }
    if (animationLoopHandle != NULL) {
        vTaskDelete(animationLoopHandle);
        animationLoopHandle = NULL;
    }
    xTaskCreatePinnedToCore([](void* pv) {
        MessageHandler* self = (MessageHandler*)pv;
        self->runFastResyncAll();
        self->fastResyncHandle = NULL;
        self->startAnimationLoopTask();
        vTaskDelete(NULL);
    }, "fastResync", 4096, this, 2, &fastResyncHandle, 1);
}

void MessageHandler::startBroadcastSettleTask() {
    xTaskCreatePinnedToCore(broadcastSettleWrapper, "broadcastSettle", 4000, this, 2, NULL, 1);
}

void MessageHandler::broadcastSettleWrapper(void* pv) {
    MessageHandler* self = (MessageHandler*)pv;
    self->runBroadcastSettle();
}

void MessageHandler::runBroadcastSettle() {
    // If address list is empty (e.g. after reset), wait for clients to announce first
    if (memcmp(addressList[0].address, emptyAddress, 6) == 0) {
        ESP_LOGI("TIMER", "Address list empty, waiting 10s for clients to announce before settle");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI("TIMER", "Unicast settle: sending timer to each known client");

    message_data msg;
    msg.messageType = MSG_TIMER;
    message_timer t;
    t.counter = 0;
    t.lastDelay = 0;
    t.reset = false;
    t.addressId = -1;

    for (int i = 0; i < NUM_DEVICES; i++) {
        if (memcmp(addressList[i].address, emptyAddress, 6) == 0) break;
        t.sendTime = esp_timer_get_time();
        memcpy(&msg.payload.timer, &t, sizeof(t));
        memcpy(msg.targetAddress, addressList[i].address, 6);
        addPeer(addressList[i].address);
        esp_now_send(addressList[i].address, (uint8_t*)&msg, ESPNOW_CLIENT_COMPAT_SIZE);
        removePeer(addressList[i].address);
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    ESP_LOGI("TIMER", "Unicast settle done, waiting for announces to clear");
    vTaskDelay(3000 / portTICK_PERIOD_MS);

    ESP_LOGI("TIMER", "Starting full timer sync");
    startAllTimerSyncTask();
    vTaskDelete(NULL);
}
void MessageHandler::runTimerSyncWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runTimerSync();
}
void MessageHandler::runClapSyncWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runClapSync();
}

void MessageHandler::runAllTimerSyncWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->setAddressListInactive();
    ESP_LOGI("TIMER", "Starting all timer sync");
    int numAdresses = 0;
    for (int j = 0; j < 3; j++) {
        numAdresses = 0;
        for (int i = 0; i < NUM_DEVICES; i++) {
            if (memcmp(messageHandlerInstance->getItemFromAddressList(i).address, messageHandlerInstance->emptyAddress, 6) == 0) {
                break;
            }
            else {
                numAdresses++;
            }
            activeStatus status = messageHandlerInstance->getActiveStatus(i);
            if (status == INACTIVE) {
                ESP_LOGI("TIMER", "Syncing index %d", i);
                messageHandlerInstance->setCurrentTimerIndex(i);
                messageHandlerInstance->setTimerReset(true);
                messageHandlerInstance->runTimerSync();
            }
        }
        ESP_LOGI("TIMER", "All timer sync iteration %d done, %d devices", j, numAdresses);
    }
    if (numAdresses > 0) {
        ESP_LOGI("TIMER", "RUNNING ANIMATION LOOP TASK FROM ALL TIMER SYNC");
        messageHandlerInstance->startAnimationLoopTask();
    }
    messageHandlerInstance->allTimerSyncHandle = NULL;
    vTaskDelete(NULL);
}

void MessageHandler::runClapSync() {
    ESP_LOGI("CLAP", "Starting clap sync");
    message_data messageData;
    messageData.messageType = MSG_COMMAND;
    messageData.payload.command.commandType = CMD_MESSAGE;
    memcpy(messageData.targetAddress, clapDeviceAddress, 6);
    setSettingClapSync(true);
    TickType_t lastWakeTime = xTaskGetTickCount();
    int delayAverage = 0;
    addPeer(clapDeviceAddress);
    for (int i = 0; i < 11; i++) {
        ESP_LOGI("CLAP", "Clap sync iteration %d", i);
        if (i > 0) {
            delayAverage += getLastDelay();
        }
        ESP_LOGI("CLAP", "Last delay: %d", getLastDelay());
        lastWakeTime = xTaskGetTickCount();
         setLastSendTime(esp_timer_get_time());
        esp_now_send(clapDeviceAddress, (uint8_t *) &messageData, ESPNOW_CLIENT_COMPAT_SIZE);
         vTaskDelayUntil(&lastWakeTime, TIMER_FREQUENCY/portTICK_PERIOD_MS);
    }
    removePeer(clapDeviceAddress);
    delayAverage /= 10;
    setClapDeviceDelay(delayAverage / 2);
    ESP_LOGI("CLAP", "Clap device delay set to %d", getClapDeviceDelay());

    // Send MSG_TIMER to chirp device for clock offset sync.
    // Does not expect MSG_GOT_TIMER, chirp device accumulates offset silently.
    runClapDeviceTimerSync();

    ESP_LOGI("CLAP", "Clap sync finished");
    clapSyncHandle = NULL;
    vTaskDelete(NULL);
}



void MessageHandler::runClapDeviceTimerSync() {
    if (getClapDeviceDelay() == 0) {
        // Clap/chirp device never completed a delay sync, nothing to refresh
        return;
    }
    ESP_LOGI("CLAP", "Starting chirp device timer sync");
    addPeer(clapDeviceAddress);
    {
        message_data timerData;
        timerData.messageType = MSG_TIMER;
        memcpy(timerData.targetAddress, clapDeviceAddress, 6);
        message_timer t = {};
        t.addressId = -1;
        int txSlot = acquireTxSlot(clapDeviceAddress);
        TickType_t wake = xTaskGetTickCount();
        for (int i = 0; i < TIMER_ARRAY_COUNT + 5; i++) {
            wake = xTaskGetTickCount();
            t.counter   = i;
            t.lastDelay = (txSlot >= 0) ? clampTxDelay(txSlots[txSlot].lastDelay) : 0;
            t.reset     = (i == 0);
            t.sendTime  = esp_timer_get_time();
            memcpy(&timerData.payload.timer, &t, sizeof(t));
            if (txSlot >= 0) txSlots[txSlot].sendTime = t.sendTime;
            esp_now_send(clapDeviceAddress, (uint8_t*)&timerData, ESPNOW_CLIENT_COMPAT_SIZE);
            vTaskDelayUntil(&wake, TIMER_FREQUENCY / portTICK_PERIOD_MS);
        }
        releaseTxSlot(txSlot);
    }
    removePeer(clapDeviceAddress);
    ESP_LOGI("CLAP", "Chirp device timer sync done");
}

void MessageHandler::runTimerSyncAt(int index) {
    // Race-free version for parallel fast resync, index passed directly, not via shared state.
    // Sends TIMER_ARRAY_COUNT + 5 packets then stops; no shared counter state touched.
    message_timer timerMessage;
    message_data messageData;
    messageData.messageType = MSG_TIMER;

    addPeer(addressList[index].address);
    ESP_LOGI("TIMER", "Fast resync index %d start", index);

    int txSlot = acquireTxSlot(addressList[index].address);
    TickType_t lastWakeTime = xTaskGetTickCount();
    for (int i = 0; i < TIMER_ARRAY_COUNT + 5; i++) {
        lastWakeTime = xTaskGetTickCount();
        timerMessage.counter   = i;
        timerMessage.lastDelay = (txSlot >= 0) ? clampTxDelay(txSlots[txSlot].lastDelay) : 0;
        timerMessage.reset     = (i == 0);
        timerMessage.addressId = index;
        timerMessage.sendTime  = esp_timer_get_time();
        memcpy(&messageData.payload.timer, &timerMessage, sizeof(timerMessage));
        if (txSlot >= 0) txSlots[txSlot].sendTime = timerMessage.sendTime;
        esp_now_send(addressList[index].address, (uint8_t*)&messageData, ESPNOW_CLIENT_COMPAT_SIZE);
        vTaskDelayUntil(&lastWakeTime, TIMER_FREQUENCY / portTICK_PERIOD_MS);
    }
    releaseTxSlot(txSlot);

    removePeer(addressList[index].address);
    addressList[index].active = ACTIVE;
    addressList[index].lastUpdateTime = millis();
    ESP_LOGI("TIMER", "Fast resync index %d done", index);
}

void MessageHandler::runTimerSync() {
    message_timer timerMessage;
    message_data messageData;
    messageData.messageType = MSG_TIMER;
    setSettingTimer(true);
    setTimerCounter(0);
    setLastTimerCounter();
    int timerIndex  = getCurrentTimerIndex();
    TickType_t lastWakeTime = xTaskGetTickCount();

    if (timerIndex > -1) {
        addPeer(addressList[timerIndex].address);
        ESP_LOGI("TIMER", "Starting timer sync for index %d with address %02x:%02x:%02x:%02x:%02x:%02x", timerIndex, addressList[timerIndex].address[0], addressList[timerIndex].address[1], addressList[timerIndex].address[2], addressList[timerIndex].address[3], addressList[timerIndex].address[4], addressList[timerIndex].address[5]);
        //setCommand(messageData, addressList[timerIndex].address);
    }
    else {
        ESP_LOGI("TIMER", "Timer Sync: Addres is -1");
    }
    unsigned long long lastTick = 0;
    int txSlot = acquireTxSlot(timerIndex > -1 ? addressList[timerIndex].address : broadcastAddress);
    while (getSettingTimer() == true) {
        lastWakeTime = xTaskGetTickCount();
        timerMessage.counter = incrementTimerCounter();
        timerMessage.lastDelay = (txSlot >= 0) ? clampTxDelay(txSlots[txSlot].lastDelay) : getLastDelay();
        timerMessage.reset = getTimerReset();
        timerMessage.addressId = getCurrentTimerIndex();
        timerMessage.sendTime = esp_timer_get_time();
        memcpy(&messageData.payload.timer, &timerMessage, sizeof(timerMessage));
        setLastSendTime(timerMessage.sendTime);
        if (txSlot >= 0) txSlots[txSlot].sendTime = timerMessage.sendTime;
        if (timerIndex == -1) {
            esp_now_send(broadcastAddress, (uint8_t *) &messageData, ESPNOW_CLIENT_COMPAT_SIZE);
        }
        else if (timerIndex > -1) {
            esp_now_send(addressList[timerIndex].address, (uint8_t *) &messageData, ESPNOW_CLIENT_COMPAT_SIZE);
            if (!esp_now_is_peer_exist(addressList[timerIndex].address)) {
                ESP_LOGI("ESP-NOW", "Peer does not exist");
            }
        }

        if ((getLastTimerCounter() < getTimerCounter()-5 || getTimerCounter() > 100)   && timerIndex > -1) {
            setUnavailable(getCurrentTimerIndex());
            removePeer(addressList[timerIndex].address);
            setSettingTimer(false);
            ESP_LOGI("TIMER", "Setting unavailable. Last counter: %d, current counter: %d, index: %d", getLastTimerCounter(), getTimerCounter(), timerIndex);
        }

        //ESP_LOGI("TIMER", "TIMER %d SENT AT %llu - exact difference %llu", timerMessage.counter, timerMessage.sendTime, timerMessage.sendTime-lastTick);
        //ESP_LOGI("TIMER", "Last Delay was %d", timerMessage.lastDelay);
        lastTick = timerMessage.sendTime;
         vTaskDelayUntil(&lastWakeTime, TIMER_FREQUENCY/portTICK_PERIOD_MS);
    }
    releaseTxSlot(txSlot);
    if (getSettingTimer() == false)   {
        ESP_LOGI("TIMER", "Timer sync finished for index %d with address %02x:%02x:%02x:%02x:%02x:%02x", timerIndex, addressList[timerIndex].address[0], addressList[timerIndex].address[1], addressList[timerIndex].address[2], addressList[timerIndex].address[3], addressList[timerIndex].address[4], addressList[timerIndex].address[5]);
        if (!esp_now_is_peer_exist(addressList[timerIndex].address)) {
            ESP_LOGI("ESP-NOW", "Peer does not exist, can't delete");
        }
        removePeer(addressList[timerIndex].address);
        if (addressList[timerIndex].active == INACTIVE) {
            addressList[timerIndex].active = ACTIVE;
            addressList[timerIndex].lastUpdateTime = millis();
        }
        startAnimationLoopTask();
        if (xTaskGetCurrentTaskHandle() == timerSyncHandle) {
            timerSyncHandle = NULL;
            vTaskDelete(NULL);
        } else if (timerSyncHandle != NULL) {
            vTaskDelete(timerSyncHandle);
            timerSyncHandle = NULL;
        }
    }
    
}

void MessageHandler::setAddressListInactive() {
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (memcmp(addressList[i].address, emptyAddress, 6) == 0) {
            break;
        }
        else {
            addressList[i].active = INACTIVE;
            addressList[i].batteryPercentage = 0;
            addressList[i].lastUpdateTime = 0;
        }
    } 
}


#endif