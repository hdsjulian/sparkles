#include "MessageHandler.h"

#if DEVICE_MODE == MASTER
#include "Arduino.h"
#include "esp_now.h"
#include "WiFi.h"
#include <ArduinoJson.h>

// A chirp needs 200 ms to travel further than the client's record window reaches,
// anything outside that is a bad correlation or a stale clock, not a distance
static constexpr long long MAX_CHIRP_FLIGHT_US = 200000;
// How far the emitter's idea of master time may be from ours before its chirp
// timestamps are worthless. Mesh latency and the sync's own median are worth a
// couple of ms; anything approaching a second is a stale clock generation.
static constexpr long long EMITTER_SKEW_TOLERANCE_US = 50000;
// how long after the last chirp the burst waits for the final client replies
static constexpr int DIST_CAL_SETTLE_MS = 1500;
// a tight cluster of measurements must not start rejecting its own members
static constexpr float CHIRP_OUTLIER_FLOOR_M = 0.30f;

static void sortFloats(float* values, int count) {
    for (int i = 1; i < count; i++) {
        float v = values[i];
        int j = i - 1;
        while (j >= 0 && values[j] > v) { values[j + 1] = values[j]; j--; }
        values[j + 1] = v;
    }
}

// Reflections and missed peaks show up as single wild measurements, so keep only
// what sits near the median and average that. Sorts values in place.
static float averageWithoutOutliers(float* values, int count, int& kept) {
    kept = 0;
    if (count <= 0) return 0.0f;
    sortFloats(values, count);
    float median = (count % 2) ? values[count / 2]
                               : 0.5f * (values[count / 2 - 1] + values[count / 2]);

    float deviations[NUM_CLAPS];
    for (int i = 0; i < count; i++) deviations[i] = fabsf(values[i] - median);
    sortFloats(deviations, count);
    float mad = (count % 2) ? deviations[count / 2]
                            : 0.5f * (deviations[count / 2 - 1] + deviations[count / 2]);
    float tolerance = fmaxf(CHIRP_OUTLIER_FLOOR_M, 2.5f * mad);

    float sum = 0;
    for (int i = 0; i < count; i++) {
        if (fabsf(values[i] - median) > tolerance) continue;
        sum += values[i];
        kept++;
    }
    return (kept > 0) ? (sum / kept) : median;
}

static void serialEmitBoard(int id, const client_address& a) {
    JsonDocument doc;
    doc["event"] = "update_board";
    doc["id"]    = id;
    char mac[18];
    snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
        a.address[0], a.address[1], a.address[2],
        a.address[3], a.address[4], a.address[5]);
    doc["address"]           = mac;
    doc["status"]            = (a.active == ACTIVE) ? "active" : "inactive";
    doc["batteryPercentage"] = a.batteryPercentage;
    doc["distance"]          = a.distanceFromCenter;
    doc["xpos"]              = a.xPos;
    doc["ypos"]              = a.yPos;
    doc["lastUpdateTime"]    = (unsigned long)a.lastUpdateTime;
    doc["timerOffset"]       = (long)a.timerOffset;
    doc["delay"]             = a.delay;
    String out; serializeJson(doc, out); Serial.println(out);
}


void MessageHandler::turnWifiOn() {
    WiFi.mode(WIFI_AP_STA);
    if (esp_now_init() != ESP_OK)
    {
      Serial.println("Error initializing ESP-NOW");
      return;
    }
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);
    addPeer(const_cast<uint8_t*>(hostAddress));
    addPeer(const_cast<uint8_t*>(broadcastAddress));
}


void MessageHandler::handleAddressStruct() {
    if (!readStructsFromFile(addressList, NUM_DEVICES,  "/clientAddress")) {
        ESP_LOGE("FS", "Failed to read client addresses from file");
    }
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (memcmp(addressList[i].address, emptyAddress, 6) == 0) {
            ESP_LOGI("FS", "Loaded %d addresses from file", i);
            break;
        }
        else {
            addressList[i].active = INACTIVE;
            addressList[i].batteryPercentage = 0;
            addressList[i].lastUpdateTime = 0;
        }
    } 
}



void MessageHandler::handleReceive() {
    message_data incomingData;
    while (true) {
        if (xQueueReceive(receiveQueue, &incomingData, pdMS_TO_TICKS(5000)) == pdTRUE) {
            // Set lastMidiTime if animation message of type MIDI is received
            
            
            if (incomingData.messageType == MSG_ADDRESS) {
                if (pendingBroadcastCommand != 0 && millis() < pendingBroadcastExpiry) {
                    message_data pendingMsg = createCommandMessage(pendingBroadcastCommand, false);
                    memcpy(pendingMsg.targetAddress, incomingData.senderAddress, 6);
                    pushToSendQueue(pendingMsg);
                    continue;
                }
                if (pendingBroadcastCommand != 0 && millis() >= pendingBroadcastExpiry) {
                    pendingBroadcastCommand = 0;
                }
                if (memcmp(incomingData.senderAddress, clapDeviceAddress, 6) == 0) {
                    if (clapSyncHandle != NULL) {
                        vTaskDelete(clapSyncHandle);
                        clapSyncHandle = NULL;
                    }
                    startClapSyncTask();
                    continue;
                }
                //ESP_LOGI("MSG", "Received animation message");
                //ESP_LOGI("MSG", "Animation type: %d", incomingData[1]);
                //message_animate *animation = (message_animate *)incomingData;
                //ESP_LOGI("MSG", "Midi note: %d", animation->animationParams.midi.note);
                //ESP_LOGI("MSG", "Midi velocity: %d", animation->animationParams.midi.velocity);
                if (incomingData.payload.address.version != version) {
                    // Note it and carry on. This used to answer with
                    // MSG_UPDATE_VERSION and skip the announce entirely, so a
                    // board on a different build was never synced — it was
                    // expected to update itself instead. With that gone, refusing
                    // to sync it would just strand it. Update deliberately, over
                    // USB or the OTA command.
                    ESP_LOGW("MSG", "Board %02x:%02x:%02x:%02x:%02x:%02x is on %s, master is %s — syncing anyway",
                             incomingData.payload.address.address[0], incomingData.payload.address.address[1],
                             incomingData.payload.address.address[2], incomingData.payload.address.address[3],
                             incomingData.payload.address.address[4], incomingData.payload.address.address[5],
                             incomingData.payload.address.version.toString().c_str(),
                             version.toString().c_str());
                }
                if (!isOTAUpdating) {
                    // A sync is in flight, so this board cannot be synced right
                    // now — but dropping the announce made it re-announce and
                    // race for a gap. Queue it for the end of the current sync.
                    if (timerSyncHandle != NULL && eTaskGetState(timerSyncHandle) != eDeleted) {
                        if (!pendingAnnounce) {
                            memcpy(pendingAnnounceMac, incomingData.senderAddress, 6);
                            pendingAnnounce = true;
                        }
                        continue;
                    }
                    // Stop animation loop during sync so it doesn't interfere with lastDelay timing
                    stopAnimationLoop();
                    message_address address = (message_address)incomingData.payload.address;
                    int index = addOrGetAddressId(address.address);
                    ESP_LOGI("MSG", "Address from index %d, total %d", index, getNumDevices());
                    setCurrentTimerIndex(index);
                    setAvailable(index);
                    startTimerSyncTask();
                }
            }
            else if (incomingData.messageType == MSG_SOUND_DEVICE) {
                // Visible at CORE_DEBUG_LEVEL=0, unlike ESP_LOG: this is the only
                // way the master learns the emitter's address, and when it silently
                // stops happening every chirp burst is refused for an unconfirmed
                // clock with nothing indicating why.
                {
                    JsonDocument d;
                    d["event"] = "sound_device_seen";
                    char mac[18];
                    snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                             incomingData.senderAddress[0], incomingData.senderAddress[1],
                             incomingData.senderAddress[2], incomingData.senderAddress[3],
                             incomingData.senderAddress[4], incomingData.senderAddress[5]);
                    d["address"] = mac;
                    d["delayMeasured"] = clapDelayMeasured;
                    String out; serializeJson(d, out); Serial.println(out);
                }
                // clap/chirp device announce, learn its address so either board works
                bool newDevice = memcmp(clapDeviceAddress, incomingData.senderAddress, 6) != 0;
                if (newDevice) {
                    memcpy(clapDeviceAddress, incomingData.senderAddress, 6);
                    setClapDeviceDelay(0);       // measured delay belonged to the previous device
                    clapDelayMeasured = false;
                    LOG_I("MSG", "Learned sound device address %02x:%02x:%02x:%02x:%02x:%02x",
                          clapDeviceAddress[0], clapDeviceAddress[1], clapDeviceAddress[2],
                          clapDeviceAddress[3], clapDeviceAddress[4], clapDeviceAddress[5]);
                }
                // Only sync when there is something to learn. The chirp device now
                // re-announces periodically so a rebooted master can rediscover it,
                // and restarting the delay sync on every announce would kill the
                // in-flight one each time, so it could never finish.
                if (newDevice || !clapDelayMeasured) {
                    if (clapSyncHandle != NULL) {
                        vTaskDelete(clapSyncHandle);
                        clapSyncHandle = NULL;
                    }
                    startClapSyncTask();
                }
            }
            else if (incomingData.messageType == MSG_GOT_TIMER) {
                // Chirp device sends MSG_GOT_TIMER but is not in addressList, remove peer and stop timer, skip addressList writes.
                if (memcmp(incomingData.senderAddress, clapDeviceAddress, 6) == 0) {
                    // Don't just note that a sync happened — check it produced a
                    // correct offset. MSG_GOT_TIMER carries perceivedTime, the
                    // emitter's own idea of master time, so comparing it against
                    // our actual clock catches an offset belonging to a previous
                    // master generation. Acknowledging without checking let a
                    // burst run with stamps a whole uptime out, which reads as
                    // tens of metres and is discarded downstream.
                    long long skew = (long long)esp_timer_get_time()
                                   - (long long)incomingData.payload.gotTimer.perceivedTime;
                    if (skew < 0) skew = -skew;
                    bool accepted = (skew < EMITTER_SKEW_TOLERANCE_US);
                    if (accepted) clapDeviceSyncedAt = millis();
                    // As a JSON event, not a log: the master runs at
                    // CORE_DEBUG_LEVEL=0, so ESP_LOG says nothing and LOG_I only
                    // goes over the mesh. Reporting the verdict without the
                    // number it was based on is what made this opaque.
                    {
                        JsonDocument d;
                        d["event"]    = "emitter_clock";
                        d["skewUs"]   = (long long)skew;
                        d["accepted"] = accepted;
                        d["toleranceUs"] = (long long)EMITTER_SKEW_TOLERANCE_US;
                        d["offset"]   = (long long)incomingData.payload.gotTimer.offset;
                        d["delayAvg"] = incomingData.payload.gotTimer.delayAverage;
                        String out; serializeJson(d, out); Serial.println(out);
                    }
                    removePeer(clapDeviceAddress);
                    setSettingTimer(false);
                    continue;
                }
                int timerIndex = getCurrentTimerIndex();
                if (timerIndex < 0 || timerIndex >= NUM_DEVICES) {
                    // late/stale MSG_GOT_TIMER — the sync it belongs to already finished
                    // (currentTimerIndex reset to -1) or never validly started. addressList
                    // has no slot -1; indexing it was corrupting adjacent members.
                    ESP_LOGW("MSG", "MSG_GOT_TIMER with no active sync target (index %d), dropping", timerIndex);
                    continue;
                }
                //vTaskDelete(timerSyncHandle);
                removePeer(addressList[timerIndex].address);
                //timerSyncHandle = NULL;
                LOG_I("SYNC", "board %d answered the timer burst — offset %lld, delay avg %d",
                         timerIndex, (long long)incomingData.payload.gotTimer.offset,
                         incomingData.payload.gotTimer.delayAverage);
                addressList[timerIndex].active = ACTIVE;
                addressList[timerIndex].batteryPercentage = incomingData.payload.gotTimer.batteryPercentage;
                addressList[timerIndex].lastUpdateTime = millis();
                addressList[timerIndex].delay = incomingData.payload.gotTimer.delayAverage;
                // saturate: int32 display field overflows when uptimes differ >~35min
                long long gotOffset = incomingData.payload.gotTimer.offset;
                addressList[timerIndex].timerOffset = (int32_t)constrain(gotOffset, (long long)INT32_MIN, (long long)INT32_MAX);
                unsigned long long now = micros();
                setCurrentTimerIndex(-1);
                setSettingTimer(false);
                writeStructsToFile(addressList, NUM_DEVICES, "/clientAddress");
                sendSystemStatus();
                serialEmitBoard(timerIndex, addressList[timerIndex]);
                resumeAnimationLoop();
            }
            else if (incomingData.messageType == MSG_STATUS) {
                for (int i = 0; i < NUM_DEVICES; i++) {
                    if (memcmp(addressList[i].address, incomingData.senderAddress, 6) == 0) {
                        addressList[i].batteryPercentage = incomingData.payload.status.batteryPercentage;
                        addressList[i].lastUpdateTime = millis();
                        
                        serialEmitBoard(i, addressList[i]);
                        break;
                    }
                }

                //writeStructsToFile(addressList, NUM_DEVICES, "/clientAddress");
            }
            else if (incomingData.messageType == MSG_ANIMATION &&
                     (incomingData.payload.animation.animationType == MIDI ||
                      incomingData.payload.animation.animationType == BACKGROUND_SHIMMER)) {
                lastMidiTime = millis();
                requestAnimationLoopStop(); // don't block the MIDI path waiting for it
            }

            // Doesn't need to be sent so nothing will happen right now
            // #BATSAVE
            /*
            else if (incomingData.messageType == MSG_ASK_COMMAND) {
                retrieveCommand(incomingData.senderAddress);
            }*/

            else if (incomingData.messageType == MSG_CLAP) {
                const message_clap clap = incomingData.payload.clap;
                bool burst = burstActive;
                // during a burst the chirp index picks the slot, so a reply that
                // arrives after the next chirp is still counted correctly
                int slot = burst ? (int)clap.chirpIndex : getClapIndex();
                if (slot < 0 || slot >= (burst ? CHIRP_BURST_COUNT : NUM_CLAPS)) {
                    ESP_LOGW("MSG", "MSG_CLAP with out of range chirp index %d, dropping", slot);
                    continue;
                }
                if (memcmp(incomingData.senderAddress, clapDeviceAddress, 6) == 0) {
                    if (!clap.clapHappened) continue; // nothing emitted, no timestamp to keep
                    // sound devices send their emission timestamp in master time,
                    // 0 means old firmware or unsynced device
                    unsigned long long emission = (clap.clapTime != 0)
                                                  ? clap.clapTime
                                                  : (micros() - getClapDeviceDelay());
                    setLastClapTime(emission);
                    if (burst) recordChirpEmission(slot, emission);
                    {
                        JsonDocument doc;
                        doc["event"]    = "calibration_status";
                        doc["status"]   = 3;
                        doc["clapId"]   = slot;
                        doc["clapTime"] = emission;
                        String out; serializeJson(doc, out); Serial.println(out);
                    }
                    if (burst) emitBurstStatus("running", slot + 1);

                }
                else {
                    if (!clap.clapHappened) continue; // client heard nothing this round
                    unsigned long long emission = burst ? chirpEmissions[slot] : getLastClapTime();
                    if (emission == 0) continue;      // no emission stamp for this slot
                    long long flightTime = (long long)clap.clapTime - (long long)emission;
                    if (flightTime <= 0 || flightTime > MAX_CHIRP_FLIGHT_US) {
                        ESP_LOGW("MSG", "Chirp %d: implausible flight time %lld us, dropping", slot, flightTime);
                        continue;
                    }
                    for (int i = 0; i < NUM_DEVICES; i++) {
                        if (memcmp(addressList[i].address, incomingData.senderAddress, 6) == 0) {
                            // during a burst this is one of ten samples for the same spot,
                            // it only becomes a distance once the burst is averaged
                            float meters = convertMicrosToMeters((unsigned long long)flightTime);
                            if (burst) {
                                burstSamples[i][slot] = meters;
                            } else {
                                addressList[i].distances[slot] = meters;
                            }
                            {
                                JsonDocument doc;
                                doc["event"]        = "client_clap";
                                doc["clapId"]       = slot;
                                doc["boardId"]      = i;
                                doc["clapDistance"] = meters;
                                String out; serializeJson(doc, out); Serial.println(out);
                            }

                            break;
                        }
                    }
                }

            }

            else if (incomingData.messageType == MSG_MIC_TEST) {
                const message_mic_test &m = incomingData.payload.micTest;
                for (int i = 0; i < NUM_DEVICES; i++) {
                    if (memcmp(addressList[i].address, incomingData.senderAddress, 6) != 0) continue;
                    // a flat line means nothing reached the ADC at all; a mean
                    // pinned to a rail means the bias network, not the mic
                    const char *verdict = "ok";
                    if (m.maxLevel == m.minLevel)            verdict = "flat — no signal at all";
                    else if (m.meanLevel < 100)              verdict = "pinned low";
                    else if (m.meanLevel > 3995)             verdict = "pinned high";
                    else if (m.rms < 2)                      verdict = "silent — mic or bias";
                    JsonDocument doc;
                    doc["event"]   = "mic_test";
                    doc["boardId"] = i;
                    doc["min"]     = m.minLevel;
                    doc["max"]     = m.maxLevel;
                    doc["mean"]    = m.meanLevel;
                    doc["rms"]     = m.rms;
                    doc["verdict"] = verdict;
                    String out; serializeJson(doc, out); Serial.println(out);
                    break;
                }
            }

            else if (incomingData.messageType == MSG_HEALTH) {
                const message_health &h = incomingData.payload.health;
                for (int i = 0; i < NUM_DEVICES; i++) {
                    if (memcmp(addressList[i].address, incomingData.senderAddress, 6) != 0) continue;
                    // answering at all is proof of life, so the board counts as
                    // active again even if it had timed out into INACTIVE
                    addressList[i].batteryPercentage = h.batteryPercentage;
                    addressList[i].lastUpdateTime = millis();
                    addressList[i].active = ACTIVE;
                    JsonDocument doc;
                    doc["event"]             = "client_health";
                    doc["boardId"]           = i;
                    doc["reportedId"]        = h.addressId;
                    doc["batteryPercentage"] = h.batteryPercentage;
                    doc["freeHeap"]          = h.freeHeap;
                    doc["minFreeHeap"]       = h.minFreeHeap;
                    doc["uptimeS"]           = h.uptimeS;
                    doc["rssi"]              = h.rssi;
                    doc["resetReason"]       = h.resetReason;
                    doc["version"]           = h.version;
                    String out; serializeJson(doc, out); Serial.println(out);
                    // same board object the dashboard cards already listen for,
                    // so a health reply refreshes the card without a second path
                    serialEmitBoard(i, addressList[i]);
                    break;
                }
            }

            else if (incomingData.messageType == MSG_SYSTEM_STATUS) {
            }

            else if (incomingData.messageType == MSG_COMMAND && incomingData.payload.command.commandType == CMD_ASK_ADMIN_PRESENT) {

                if (getAdminPresent() + 60000 > millis()) {
                    message_data commandMessage = createCommandMessage(CMD_SET_ADMIN_PRESENT);
                    memcpy(commandMessage.targetAddress, incomingData.senderAddress, 6);
                    pushToSendQueue(commandMessage);
                }
                else {
                    message_data commandMessage = createCommandMessage(CMD_SET_ADMIN_NOT_PRESENT);
                    memcpy(commandMessage.targetAddress, incomingData.senderAddress, 6);
                    pushToSendQueue(commandMessage);
                }

             }
            else if (incomingData.messageType == MSG_TIMER_RESPONSE) {
                int64_t masterNow = (int64_t)esp_timer_get_time();
                for (int i = 0; i < NUM_DEVICES; i++) {
                    if (memcmp(addressList[i].address, incomingData.senderAddress, 6) == 0) {
                        int64_t sendTime = (int64_t)addressList[i].timerQuerySendTime;
                        addressList[i].timerQuerySendTime = 0; // consumed, one measurement per query
                        if (sendTime == 0) break;
                        int64_t rtt = masterNow - sendTime;
                        if (rtt <= 0 || rtt > 500000) break;  // stale pairing, not a measurement
                        int64_t delta = masterNow - (incomingData.payload.timerResponse.estimatedMasterTime + rtt / 2);
                        if (delta < 0) delta = -delta;
                        JsonDocument doc;
                        doc["event"]   = "timer_test_result";
                        doc["boardId"] = incomingData.payload.timerResponse.addressId;
                        doc["deltaUs"] = (long long)delta;
                        doc["rttUs"]   = (long long)rtt;
                        String out; serializeJson(doc, out); Serial.println(out);
                        break;
                    }
                }
            }
            else {
                ESP_LOGI("MSG", "Unknown message type  %d received", incomingData.messageType);
            }
        }
    }
}

void MessageHandler::handleSend() {
    message_data messageData;
    while(true) {
        if (xQueueReceive(sendQueue, &messageData, portMAX_DELAY) == pdTRUE) {
            if (memcmp(messageData.targetAddress, broadcastAddress, 6) != 0) {
                addPeer(messageData.targetAddress);
            }
            WiFi.macAddress(messageData.senderAddress);
            switch (messageData.messageType) {
                case MSG_LOG:
                    esp_now_send(messageData.targetAddress, (uint8_t *) &messageData, sizeof(messageData));
                    break;
                default:
                    esp_now_send(messageData.targetAddress, (uint8_t *) &messageData, ESPNOW_CLIENT_COMPAT_SIZE);
                    break;
            }        
            if (memcmp(messageData.targetAddress, broadcastAddress, 6) != 0) {
                removePeer(messageData.targetAddress);
            }
        }
    }
}

void MessageHandler::onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    MessageHandler& instance = getInstance();
    if (status == ESP_NOW_SEND_SUCCESS) {
        instance.recordTxDelay(mac_addr);
        if (instance.getSettingTimer() == true) {
            instance.setTimerReset(false);
            instance.setLastTimerCounter();
            if (memcmp(mac_addr, instance.getItemFromAddressList(instance.getCurrentTimerIndex()).address, 6) == 0) {
                instance.setLastDelay(esp_timer_get_time() - instance.getLastSendTime());   
            }
            else {
            }
        }
        else if (instance.getSettingClapSync() == true) {
            int lastDelay = esp_timer_get_time() - instance.getLastSendTime();
            instance.setLastDelay(lastDelay);
        }
        else if (instance.getRequestingOTAUpdate() == true) {
            instance.setRequestingOTAUpdate(false);
            ESP_LOGI("MSG", "Requesting OTA update sent to %02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        }
    }
    else {
        if (instance.getRequestingOTAUpdate() == true) {
            instance.setRequestingOTAUpdate(false);
            ESP_LOGI("MSG", "Requesting OTA update failed");
            ESP_LOGI("MSG", "Address id cannot be reached: %d", instance.getOTAUpdateAddressId());
            instance.setNextOTAAddress(true);
        }
    }
}

void MessageHandler::onDataRecv(const esp_now_recv_info * mac, const uint8_t *incomingData, int len) {
    MessageHandler& instance = getInstance();
    unsigned long long now = micros();

    // During timer sync, only accept MSG_GOT_TIMER from the device being synced
    bool syncActive = (instance.allTimerSyncHandle != NULL) ||
                      (instance.timerSyncHandle != NULL && eTaskGetState(instance.timerSyncHandle) != eDeleted);
    if (syncActive && instance.getNumDevices() > 0) {
        // Remember an announce rather than losing it. Just the MAC here: the
        // address-list lookup and the sync it triggers belong on a task, not in
        // this callback.
        if (incomingData[0] == MSG_ADDRESS && !instance.pendingAnnounce) {
            memcpy(instance.pendingAnnounceMac, mac->src_addr, 6);
            instance.pendingAnnounce = true;
        }
        // The sound device announces as MSG_SOUND_DEVICE, not MSG_ADDRESS, so it
        // was dropped here on every sweep — and with it the master's only way to
        // learn the emitter's address. No address means no delay measurement, no
        // clock sync, and a chirp burst refused for an unconfirmed clock, for
        // ever. Let it through: the handler is cheap and it arrives every 2 s at
        // worst, so it cannot disturb the sync it is passing through.
        if (incomingData[0] != MSG_GOT_TIMER) return;
        int syncIndex = instance.getCurrentTimerIndex();
        if (syncIndex < 0 || memcmp(mac->src_addr, instance.addressList[syncIndex].address, 6) != 0) return;
    }

    // Drop MSG_ADDRESS if queue is backing up, clients re-announce every second
    if (incomingData[0] == MSG_ADDRESS && uxQueueMessagesWaiting(instance.receiveQueue) > 10) return;

    ESP_LOGD("MSG", "Recv type %d from %02x:%02x:%02x:%02x:%02x:%02x",
        incomingData[0],
        mac->src_addr[0], mac->src_addr[1], mac->src_addr[2],
        mac->src_addr[3], mac->src_addr[4], mac->src_addr[5]);

    // Stamp the sender from the radio, for everything. senderAddress is only
    // whatever the sender remembered to fill in, and several senders fill it
    // from a cached MAC that can be all zeros — the sound device's announce and
    // its MSG_GOT_TIMER both arrive that way. The master compared those zeros
    // against clapDeviceAddress, never matched, and so never learned the
    // emitter's address, never measured its delay, never confirmed its clock,
    // and refused every chirp burst. src_addr cannot be wrong, and the client
    // and chirp device already do exactly this on their own receive paths.
    message_data localData;
    memset(&localData, 0, sizeof(localData));
    memcpy(&localData, incomingData, len);
    memcpy(localData.senderAddress, mac->src_addr, 6);

    // MSG_GOT_TIMER and MSG_CLAP carry timing that only the receiver can stamp
    if (incomingData[0] == MSG_GOT_TIMER || incomingData[0] == MSG_CLAP) {
        localData.msgReceiveTime = now;
    }
    // and the link quality of a health reply is likewise only knowable here
    if (incomingData[0] == MSG_HEALTH && mac->rx_ctrl) {
        localData.payload.health.rssi = (int8_t)mac->rx_ctrl->rssi;
    }
    instance.pushToRecvQueue(mac, (uint8_t*)&localData, sizeof(message_data));
}



void MessageHandler::tickInactiveTimeout() {
    static constexpr unsigned long INACTIVE_TIMEOUT_MS = 25UL * 60UL * 1000UL; // 25 minutes
    unsigned long now = millis();
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (memcmp(addressList[i].address, emptyAddress, 6) == 0) break;
        if (addressList[i].active == ACTIVE &&
            addressList[i].lastUpdateTime > 0 &&
            now - addressList[i].lastUpdateTime > INACTIVE_TIMEOUT_MS) {
            addressList[i].active = INACTIVE;
            ESP_LOGW("MSG", "Device %d timed out after 25 min, marking inactive", i);
            serialEmitBoard(i, addressList[i]);
        }
    }
}

void MessageHandler::sendSystemStatus() {
    message_system_status systemStatus;
    systemStatus.numDevices = getNumDevices();
    message_data message;
    message.messageType = MSG_SYSTEM_STATUS;
    memcpy(&message.payload, &systemStatus, sizeof(systemStatus));
    memcpy(&message.targetAddress, broadcastAddress, sizeof(broadcastAddress));
    pushToSendQueue(message);
}

void MessageHandler::sendAnimation(message_animation animationMessage, int addressId) {
    lastAnimationType = animationMessage.animationType;
    message_data message;
    message.messageType = MSG_ANIMATION;
    memcpy(&message.payload.animation, &animationMessage, sizeof(animationMessage));
    if (addressId == -1) {
        memcpy(&message.targetAddress, broadcastAddress, sizeof(broadcastAddress));
    }
    else {
        memcpy(&message.targetAddress, addressList[addressId].address, sizeof(addressList[addressId].address));
    }
    // Master blinks immediately; clients use the original startTime
    message_animation masterAnimation = animationMessage;
    masterAnimation.animationParams.blink.startTime = esp_timer_get_time() + 10000;
    ledInstance->setAnimation(masterAnimation);
    // Latency-sensitive animations bypass the send queue and go direct
    if (animationMessage.animationType == BACKGROUND_SHIMMER ||
        animationMessage.animationType == MIDI) {
        WiFi.macAddress(message.senderAddress);
        esp_err_t r = esp_now_send(broadcastAddress, (uint8_t*)&message, ESPNOW_CLIENT_COMPAT_SIZE);
        if (r != ESP_OK) {
            ESP_LOGW("MSG", "esp_now_send failed: %d (TX queue full under shimmer load?)", r);
        }
    } else {
        pushToSendQueue(message);
    }
}
void MessageHandler::startOTAUpdateTask() {
    xTaskCreatePinnedToCore(runOTAUpdateTaskWrapper, "runOTAUpdate", 10000, this, 2, &otaUpdateHandle, 0);
}
void MessageHandler::runOTAUpdateTaskWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runOTAUpdateTask();
}
// Called when a sync finishes. A board that announced while we were busy gets
// its turn now instead of having to shout again.
void MessageHandler::servicePendingAnnounce() {
    if (!pendingAnnounce) return;
    uint8_t mac[6];
    memcpy(mac, pendingAnnounceMac, 6);
    pendingAnnounce = false;
    if (memcmp(mac, emptyAddress, 6) == 0) return;
    int index = addOrGetAddressId(mac);
    if (index < 0) {
        LOG_I("MSG", "pending announce from %02x:%02x:%02x:%02x:%02x:%02x has no slot",
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return;
    }
    LOG_I("MSG", "servicing queued announce from board %d", index);
    stopAnimationLoop();
    setCurrentTimerIndex(index);
    setAvailable(index);
    startTimerSyncTask();
}

void MessageHandler::runOTAUpdateTask() {
    ESP_LOGI("OTA", "OTA update task started");
    int sent = 0;
    for (int i = 0; i < NUM_CLIENTS; i++) {
        client_address item = getItemFromAddressList(i);
        if (item.active != ACTIVE) continue;
        if (memcmp(item.address, "\x00\x00\x00\x00\x00\x00", 6) == 0) continue;

        // the network and the url travel with the request: a board has no way
        // to know either one, and the firmware lives on the pi, whose address
        // depends on which of its two networks is currently up
        message_data msg;
        msg.messageType = MSG_OTA_REQUEST;
        memcpy(msg.targetAddress, item.address, 6);
        WiFi.macAddress(msg.senderAddress);
        message_ota_request& req = msg.payload.otaRequest;
        strncpy(req.ssid,     _otaSsid,     sizeof(req.ssid) - 1);
        strncpy(req.password, _otaPassword, sizeof(req.password) - 1);
        strncpy(req.url,      _otaUrl,      sizeof(req.url) - 1);
        req.ssid[sizeof(req.ssid) - 1] = '\0';
        req.password[sizeof(req.password) - 1] = '\0';
        req.url[sizeof(req.url) - 1] = '\0';

        addPeer(item.address);
        // full frame, not the 80-byte compat size — the credentials do not fit
        esp_now_send(item.address, (uint8_t*)&msg, sizeof(msg));

        ESP_LOGI("OTA", "Sent OTA request to device %d (%d/%d) — network %s, url %s",
                 item.id, ++sent, getNumDevices(),
                 strlen(_otaSsid) > 0 ? _otaSsid : "(client default)",
                 strlen(_otaUrl) > 0 ? _otaUrl : "(client default)");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI("OTA", "OTA commands sent to %d devices", sent);
    vTaskDelete(otaUpdateHandle);
}

void MessageHandler::startCalculatePositionsTask() {
    if (calculatePositionsHandle != NULL && eTaskGetState(calculatePositionsHandle) != eDeleted) {
        ESP_LOGW("MSG", "Position solve already running, ignoring");
        return;
    }
    xTaskCreatePinnedToCore(calculatePositionsTaskWrapper, "calculatePositions", 10000, this, 2, &calculatePositionsHandle, 0);
}
void MessageHandler::calculatePositionsTaskWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runCalculatePositionsTask();
}
void MessageHandler::runCalculatePositionsTask() {
    while (true) {
        int solved = 0;
        int locations = 0;
        float worstResidual = 0.0f;
        for (int j = 0; j < NUM_CLAPS; j++) {
            if (clapTable[j].clapTime != 0) locations++;
        }
        for (int i = 0; i < NUM_DEVICES; i++) {
            if (memcmp(addressList[i].address, emptyAddress, 6) == 0) {
                continue;
            }

            // Prepare matrices for least squares
            int validClaps = 0;
            float A[NUM_CLAPS][2] = {0}; // Coefficients matrix
            float b[NUM_CLAPS] = {0};    // Constants vector

            // Reference point (first clap)
            float x1 = 0, y1 = 0, d1 = 0;
            bool referenceSet = false;

            for (int j = 0; j < NUM_CLAPS; j++) {
                if (clapTable[j].clapTime == 0) {
                    continue; // Skip if clap time is not set
                }

                float xj = clapTable[j].xPos;
                float yj = clapTable[j].yPos;
                float dj = addressList[i].distances[j];
                if (dj <= 0) {
                    continue; // client missed this clap, a zero distance would corrupt the solve
                }

                if (!referenceSet) {
                    // Set the first clap as the reference point
                    x1 = xj;
                    y1 = yj;
                    d1 = dj;
                    referenceSet = true;
                    continue;
                }

                // Fill the A matrix and b vector
                A[validClaps][0] = 2 * (xj - x1);
                A[validClaps][1] = 2 * (yj - y1);
                b[validClaps] = d1 * d1 - dj * dj - x1 * x1 - y1 * y1 + xj * xj + yj * yj;

                validClaps++;
            }

            // Ensure we have at least 3 valid claps for trilateration
            if (validClaps < 2) {
                ESP_LOGW("MSG", "Not enough claps for trilateration for device %d", i);
                continue;
            }

            // Solve the system using least squares
            float AtA[2][2] = {0}; // A^T * A
            float Atb[2] = {0};    // A^T * b

            for (int row = 0; row < validClaps; row++) {
                AtA[0][0] += A[row][0] * A[row][0];
                AtA[0][1] += A[row][0] * A[row][1];
                AtA[1][0] += A[row][1] * A[row][0];
                AtA[1][1] += A[row][1] * A[row][1];

                Atb[0] += A[row][0] * b[row];
                Atb[1] += A[row][1] * b[row];
            }

            // det near zero relative to the matrix scale means the claps are
            // (nearly) collinear and the solution would explode
            float det = AtA[0][0] * AtA[1][1] - AtA[0][1] * AtA[1][0];
            float trace = AtA[0][0] + AtA[1][1];
            if (fabsf(det) < 1e-3f * trace * trace) {
                ESP_LOGE("MSG", "Trilateration failed: claps are (nearly) collinear");
                continue;
            }

            // Invert AtA
            float invAtA[2][2] = {
                {AtA[1][1] / det, -AtA[0][1] / det},
                {-AtA[1][0] / det, AtA[0][0] / det}
            };

            // Calculate the position (x, y)
            float x = invAtA[0][0] * Atb[0] + invAtA[0][1] * Atb[1];
            float y = invAtA[1][0] * Atb[0] + invAtA[1][1] * Atb[1];

            // Store the calculated position
            addressList[i].xPos = x;
            addressList[i].yPos = y;
            solved++;

            // How far the measured distances sit from the solved point. Large on
            // every board means a constant offset (speaker latency), large on one
            // board means that board's measurements are bad.
            float sumSq = 0;
            int used = 0;
            for (int j = 0; j < NUM_CLAPS; j++) {
                if (clapTable[j].clapTime == 0) continue;
                float dj = addressList[i].distances[j];
                if (dj <= 0) continue;
                float dx = x - clapTable[j].xPos;
                float dy = y - clapTable[j].yPos;
                float err = sqrtf(dx * dx + dy * dy) - dj;
                sumSq += err * err;
                used++;
            }
            float residual = (used > 0) ? sqrtf(sumSq / used) : 0.0f;
            if (residual > worstResidual) worstResidual = residual;

            // the board needs its own position for the distance and position effects
            message_data configMessage = createConfigMessage(i);
            memcpy(configMessage.targetAddress, addressList[i].address, 6);
            pushToSendQueue(configMessage);

            ESP_LOGI("MSG", "Device %d position calculated: X=%.2f, Y=%.2f, residual %.2f m over %d locations",
                     i, x, y, residual, used);
            {
                JsonDocument doc;
                doc["event"]     = "position_result";
                doc["boardId"]   = i;
                doc["x"]         = x;
                doc["y"]         = y;
                doc["locations"] = used;
                doc["residual"]  = residual;
                String out; serializeJson(doc, out); Serial.println(out);
            }
        }

        for (int i = 0; i < NUM_DEVICES; i++) {
            if (memcmp(addressList[i].address, emptyAddress, 6) == 0) break;
            serialEmitBoard(i, addressList[i]);
        }
        {
            JsonDocument doc;
            doc["event"]  = "calibration_status";
            doc["status"] = 5;
            String out; serializeJson(doc, out); Serial.println(out);
        }
        {
            JsonDocument doc;
            doc["event"]         = "position_status";
            doc["status"]        = solved > 0 ? "solved" : "failed";
            doc["boards"]        = solved;
            doc["locations"]     = locations;
            doc["worstResidual"] = worstResidual;
            if (solved == 0) doc["reason"] = "no board heard three usable locations";
            String out; serializeJson(doc, out); Serial.println(out);
        }
        ESP_LOGI("MSG", "Trilateration done: %d boards from %d locations, worst residual %.2f m",
                 solved, locations, worstResidual);
        writeStructsToFile(addressList, NUM_DEVICES, "/clientAddress"); // keep the positions over a reboot
        vTaskDelete(calculatePositionsHandle);
    }
}

void MessageHandler::endCalibration() {
    message_data endMessage = createCommandMessage(CMD_END_CALIBRATION, true);
    pushToSendQueue(endMessage);

    startCalculatePositionsTask();
}

void MessageHandler::abortDistanceCalibration() {
    LOG_I("MSG", "Aborting distance calibration, resetting all data");
    burstActive = false;
    // Abort left distCalHandle set, so a burst task that was killed or wedged
    // blocked every later burst for good — and the refusal was silent, so there
    // was no way to tell and no way back short of rebooting the master. Abort is
    // the recovery path, so it has to actually clear it.
    if (distCalHandle != NULL) {
        TaskHandle_t h = distCalHandle;
        distCalHandle = NULL;
        if (eTaskGetState(h) != eDeleted) vTaskDelete(h);
        LOG_I("MSG", "cleared a stuck chirp burst task");
    }
    memset(chirpEmissions, 0, sizeof(chirpEmissions));
    clearClapMeasurements();
    message_data cancelMsg = createCommandMessage(CMD_CANCEL_CALIBRATION, true);
    pushToSendQueue(cancelMsg);
}

void MessageHandler::endDistanceCalibration() {
    ESP_LOGI("MSG", "Ending distance calibration");
    message_data endMessage = createCommandMessage(CMD_END_CALIBRATION, true);
    pushToSendQueue(endMessage);
    calculateDistances();

}

void MessageHandler::emitBurstStatus(const char* status, int chirpsHeard, int boardsMeasured, const char* reason) {
    JsonDocument doc;
    doc["event"]  = burstIsDistance ? "distance_status" : "position_status";
    doc["status"] = status;
    doc["chirp"]  = chirpsHeard;
    doc["total"]  = CHIRP_BURST_COUNT;
    doc["slot"]   = burstSlot;
    doc["x"]      = burstX;
    doc["y"]      = burstY;
    doc["boards"] = boardsMeasured;
    if (reason != nullptr) doc["reason"] = reason;
    String out; serializeJson(doc, out); Serial.println(out);
}

void MessageHandler::clearClapMeasurements() {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        clapIndex = 0;
        memset(clapTable, 0, sizeof(clap_table) * NUM_CLAPS);
        for (int i = 0; i < NUM_DEVICES; i++) {
            memset(addressList[i].distances, 0, sizeof(float) * NUM_CLAPS);
        }
        xSemaphoreGive(configMutex);
    }
}

// One burst measures one spot: the chirps land in burstSamples and only the slot
// being measured is cleared, everything already recorded from other spots stays.
void MessageHandler::beginBurst(int slot, float xPos, float yPos, bool isDistance) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        memset(chirpEmissions, 0, sizeof(chirpEmissions));
        memset(burstSamples, 0, sizeof(burstSamples));
        memset(&clapTable[slot], 0, sizeof(clap_table));
        for (int i = 0; i < NUM_DEVICES; i++) {
            addressList[i].distances[slot] = 0.0f;
        }
        burstSlot = slot;
        burstX = xPos;
        burstY = yPos;
        burstIsDistance = isDistance;
        xSemaphoreGive(configMutex);
    }
    burstActive = true;
    ESP_LOGI("MSG", "Chirp burst at slot %d, position (%.2f, %.2f)", slot, xPos, yPos);
    emitBurstStatus("running", 0);
}

// Ten chirps in, one distance per board out: outliers dropped, the rest averaged.
void MessageHandler::finishBurst() {
    bool wasActive = burstActive;
    burstActive = false;
    if (!wasActive) {
        // aborted while the chirps were still running, nothing left to average
        ESP_LOGI("MSG", "Chirp burst was aborted, skipping the average");
        return;
    }
    message_data endMessage = createCommandMessage(CMD_END_CALIBRATION, true);
    pushToSendQueue(endMessage);

    unsigned long long firstEmission = 0;
    int chirpsHeard = 0;
    for (int c = 0; c < CHIRP_BURST_COUNT; c++) {
        if (chirpEmissions[c] == 0) continue;
        if (firstEmission == 0) firstEmission = chirpEmissions[c];
        chirpsHeard++;
    }
    if (chirpsHeard == 0) {
        // no emitter, no measurements — writing the slot now would zero every board
        ESP_LOGE("MSG", "Chirp device never reported, keeping the previous data");
        emitBurstStatus("failed", 0, 0, "no chirps from the chirp device");
        return;
    }

    int boardsMeasured = 0;
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (memcmp(addressList[i].address, emptyAddress, 6) == 0) continue;
        float samples[CHIRP_BURST_COUNT];
        int count = 0;
        for (int c = 0; c < CHIRP_BURST_COUNT; c++) {
            if (burstSamples[i][c] > 0) samples[count++] = burstSamples[i][c];
        }
        int kept = 0;
        float distance = averageWithoutOutliers(samples, count, kept);
        addressList[i].distances[burstSlot] = (count > 0) ? distance : 0.0f;
        if (count > 0) boardsMeasured++;
        ESP_LOGI("MSG", "Device %d at slot %d: %.2f m (%d of %d chirps kept)",
                 i, burstSlot, addressList[i].distances[burstSlot], kept, count);
        JsonDocument doc;
        doc["event"]    = "chirp_distance";
        doc["boardId"]  = i;
        doc["slot"]     = burstSlot;
        doc["distance"] = addressList[i].distances[burstSlot];
        doc["kept"]     = kept;
        doc["chirps"]   = count;
        String out; serializeJson(doc, out); Serial.println(out);
    }

    if (boardsMeasured == 0) {
        // the spot stays unrecorded so the operator can just chirp again from it
        ESP_LOGW("MSG", "Nobody heard the burst at slot %d, not recording the spot", burstSlot);
        emitBurstStatus("failed", chirpsHeard, 0, "no board heard the chirps");
        return;
    }

    // a slot only counts for the solver once it carries the emitter's spot and
    // the moment it fired there
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        clapTable[burstSlot].xPos = burstX;
        clapTable[burstSlot].yPos = burstY;
        clapTable[burstSlot].clapTime = firstEmission;
        if (!burstIsDistance && burstSlot == clapIndex && clapIndex < NUM_CLAPS) clapIndex++;
        xSemaphoreGive(configMutex);
    }

    if (burstIsDistance) calculateDistances();
    ESP_LOGI("MSG", "Burst at slot %d done, %d chirps, %d boards measured",
             burstSlot, chirpsHeard, boardsMeasured);
    emitBurstStatus("done", chirpsHeard, boardsMeasured);
}

void MessageHandler::recordChirpEmission(int chirpIndex, unsigned long long emissionTime) {
    if (chirpIndex < 0 || chirpIndex >= CHIRP_BURST_COUNT) return;
    chirpEmissions[chirpIndex] = emissionTime;
}

void MessageHandler::calculateDistances() {
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (memcmp(addressList[i].address, emptyAddress, 6) == 0) {
            continue;
        }
        float measurements[NUM_CLAPS];
        int measured = 0;
        for (int j = 0; j < NUM_CLAPS; j++) {
            if (clapTable[j].clapTime == 0) {
                continue;
            }
            if (addressList[i].distances[j] <= 0) {
                continue;
            }
            measurements[measured++] = addressList[i].distances[j];
        }
        int kept = 0;
        if (measured > 0) {
            addressList[i].distanceFromCenter = averageWithoutOutliers(measurements, measured, kept);
            ESP_LOGI("MSG", "Device %d distance from center: %.2f m (%d of %d chirps kept)",
                     i, addressList[i].distanceFromCenter, kept, measured);
        } else {
            // heard nothing is not the same as standing at the emitter, keep what we had
            ESP_LOGW("MSG", "Device %d heard no chirp, keeping %.2f m",
                     i, addressList[i].distanceFromCenter);
        }
        {
            JsonDocument doc;
            doc["event"]    = "distance_result";
            doc["boardId"]  = i;
            doc["distance"] = addressList[i].distanceFromCenter;
            doc["kept"]     = kept;
            doc["chirps"]   = measured;
            String out; serializeJson(doc, out); Serial.println(out);
        }
        if (measured == 0) continue;
        serialEmitBoard(i, addressList[i]);
        message_data configMessage = createConfigMessage(i);
        memcpy(configMessage.targetAddress, addressList[i].address, 6);
        pushToSendQueue(configMessage);
    }

    float maxDist = 0.0f;
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (memcmp(addressList[i].address, emptyAddress, 6) == 0) break;
        if (addressList[i].distanceFromCenter > maxDist)
            maxDist = addressList[i].distanceFromCenter;
    }
    ESP_LOGI("MSG", "Broadcasting max distance: %.2f m", maxDist);
    message_data maxDistMsg = createCommandMessage(CMD_SET_MAX_DISTANCE, true);
    maxDistMsg.payload.command.param = maxDist;
    pushToSendQueue(maxDistMsg);

    writeStructsToFile(addressList, NUM_DEVICES, "/clientAddress"); // keep the distances over a reboot
}

void MessageHandler::startCalibrationMaster() {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        ESP_LOGI("MSG", "Starting calibration as master");
        clapIndex = 0;
        memset(clapTable, 0, sizeof(clap_table) * NUM_CLAPS);
        message_data startMessage = createCommandMessage(CMD_START_CALIBRATION, true);
        pushToSendQueue(startMessage);
        xSemaphoreGive(configMutex);
    }
}

// Distance from the centre is a single spot, so it always measures slot 0 and
// drops whatever else was in the table
void MessageHandler::startDistanceCalibrationMaster() {
    ESP_LOGI("MSG", "Starting Distance calibration as master");
    clearClapMeasurements();
    startChirpBurstTask(CMD_START_DISTANCE_CALIBRATION, 0, 0.0f, 0.0f, true);
}

// The chirp device stands at a known spot: one burst gives every board its
// distance to that spot. Three or more spots and the solver can trilaterate.
void MessageHandler::chirpAtPosition(float xPos, float yPos) {
    int slot = getClapIndex();
    if (slot < 0 || slot >= NUM_CLAPS) {
        ESP_LOGW("MSG", "No measurement slots left (%d), reset the calibration first", slot);
        burstIsDistance = false;
        emitBurstStatus("failed", 0, 0, "no measurement slots left, reset first");
        return;
    }
    startChirpBurstTask(slot == 0 ? CMD_START_DISTANCE_CALIBRATION : CMD_CONTINUE_DISTANCE_CALIBRATION,
                        slot, xPos, yPos, false);
}

// Clock offsets drift tens of µs per second, refresh every participant
// right before the measurement, then send the calibration command
struct ChirpBurstArgs { MessageHandler* self; int commandType; int slot; float xPos; float yPos; bool isDistance; };

// One command runs the whole thing: resync, one burst of CHIRP_BURST_COUNT chirps
// counted off by the emitter and the clients themselves, then the average.
void MessageHandler::startChirpBurstTask(int commandType, int slot, float xPos, float yPos, bool isDistance) {
    if (distCalHandle != NULL) {
        // Silent before: ESP_LOGW is compiled out at CORE_DEBUG_LEVEL=0 and no
        // event was emitted, so a stuck handle refused every future burst with
        // no trace anywhere — the UI just kept reporting that no board heard
        // the chirps, which was true and entirely uninformative.
        LOG_I("MSG", "Chirp burst refused: a previous one never finished");
        burstIsDistance = isDistance;
        emitBurstStatus("failed", 0, 0,
            "a chirp burst is already running, or a previous one never finished — abort it and retry");
        return;
    }
    auto* args = new ChirpBurstArgs{this, commandType, slot, xPos, yPos, isDistance};
    xTaskCreatePinnedToCore([](void* pv) {
        auto* a = (ChirpBurstArgs*)pv;
        MessageHandler* self = a->self;
        ChirpBurstArgs burst = *a;
        delete a;
        bool animationWasRunning = self->animationLoopHandle != NULL;
        if (animationWasRunning) {
            self->stopAnimationLoop();
        }
        self->runFastResyncAll();
        // set the burst kind before any early exit, or a refusal reports itself
        // under the wrong event name and the UI listening for it never hears
        self->burstIsDistance = burst.isDistance;
        if (!self->runClapDeviceTimerSync()) {
            // Without a fresh confirmation the emitter's stamps could be a whole
            // master generation out, which reads as tens of metres of flight and
            // is silently discarded downstream. Say so instead of measuring it.
            self->emitBurstStatus("failed", 0, 0,
                "emitter clock not confirmed — its chirp timestamps would be meaningless");
            if (animationWasRunning) self->resumeAnimationLoop();
            self->distCalHandle = NULL;
            vTaskDelete(NULL);
        }
        self->beginBurst(burst.slot, burst.xPos, burst.yPos, burst.isDistance);
        message_data commandMessage = self->createCommandMessage(burst.commandType, true);
        self->pushToSendQueue(commandMessage);
        // the burst runs unattended on the emitter and the clients, wait it out
        // plus a settle window for the last replies to land
        vTaskDelay(pdMS_TO_TICKS(CHIRP_BURST_COUNT * CHIRP_BURST_PERIOD_MS + DIST_CAL_SETTLE_MS));
        self->finishBurst();
        if (animationWasRunning) {
            self->resumeAnimationLoop();
        }
        self->distCalHandle = NULL;
        vTaskDelete(NULL);
    }, "chirpBurst", 8192, args, 2, &distCalHandle, 1);
}

void MessageHandler::cancelCalibration() {
    ESP_LOGI("MSG", "Cancelling calibration");
    message_data cancelCalibrationMessage = createCommandMessage(CMD_CANCEL_CALIBRATION, true);
    pushToSendQueue(cancelCalibrationMessage);
    setLastClapTime(0);
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (memcmp(addressList[i].address, emptyAddress, 6) == 0) {
            continue;
        }
        addressList[i].distances[clapIndex] = 0;
    }
}
void MessageHandler::continueCalibration(float xPos, float yPos) {
    setClap(xPos, yPos);
    clapIndex++;
    message_data continueMessage = createCommandMessage(CMD_CONTINUE_CALIBRATION, true);
    pushToSendQueue(continueMessage);
}

// Same thing again, a full burst that replaces the previous measurements
void MessageHandler::continueDistanceCalibration() {
    clearClapMeasurements();
    startChirpBurstTask(CMD_CONTINUE_DISTANCE_CALIBRATION, 0, 0.0f, 0.0f, true);
}
void MessageHandler::commandCalibrate(int boardId) {
    message_data commandMessage = createCommandMessage(CMD_START_CALIBRATION, false);
    memcpy(commandMessage.targetAddress, addressList[boardId].address, sizeof(addressList[boardId].address));
    pushToSendQueue(commandMessage);
}

void MessageHandler::startAnimationLoopTask() {
    animationLoopEnabled = true; // explicit start, restores may bring it back
    if (animationLoopHandle != NULL) return;
    animationLoopStop = false;
    xTaskCreatePinnedToCore(runAnimationLoopWrapper, "runAnimationLoop", 10000, this, 2, &animationLoopHandle, 0);
}
void MessageHandler::runAnimationLoopWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runAnimationLoop();
}

void MessageHandler::runAnimationLoop() {
    ledInstance->resetMicrosUntilEnd();
    // Waits are sliced so a stop request is noticed within ~50ms instead of
    // sitting out a multi-second delay. Blink timing doesn't suffer: the sync
    // precision comes from the startTime stamped below, not from when we wake.
    const TickType_t SLICE = pdMS_TO_TICKS(50);
    while (!animationLoopStop) {
        // Something permanent is on air — shimmer, MIDI, hearth. Wait it out
        // rather than broadcasting over the top of it; when it is switched off
        // the animation goes back to a known length and pacing resumes.
        if (ledInstance->isAnimationEndless()) {
            for (int i = 0; i < 10 && !animationLoopStop; i++) vTaskDelay(SLICE);
            continue;
        }
        TickType_t ticksUntilStart = ledInstance->getNextAnimationTicks();
        TickType_t target = ticksUntilStart > 0 ? ticksUntilStart : pdMS_TO_TICKS(1000);
        for (TickType_t waited = 0; waited < target && !animationLoopStop; waited += SLICE) {
            vTaskDelay(SLICE);
        }
        if (animationLoopStop) break;
        message_animation newAnimation;
        newAnimation = ledInstance->createSyncAsyncBlinkRandom();
        newAnimation.animationParams.syncAsyncBlink.startTime = esp_timer_get_time() + 1000000;
        sendAnimation(newAnimation, -1);

    }
    // exit on our own terms so nobody vTaskDeletes us mid-send
    animationLoopHandle = NULL;
    vTaskDelete(NULL);
}

// Stop the idle animation loop safely. Never vTaskDelete it: it runs pinned to
// core 0 while callers sit on core 1, so an async delete can land in the middle
// of sendAnimation() (or a malloc) and leave the master wedged or panicking —
// that's what made sleep_now silently kill the master whenever the loop was
// actually running. Instead ask it to stop and wait for it to leave the loop.
void MessageHandler::stopAnimationLoop() {
    if (animationLoopHandle == NULL) return;
    animationLoopStop = true;
    for (int i = 0; i < 100 && animationLoopHandle != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (animationLoopHandle != NULL) { // wedged in a send — last resort
        ESP_LOGW("MSG", "animation loop did not exit in 1s, deleting");
        vTaskDelete(animationLoopHandle);
        animationLoopHandle = NULL;
    }
    animationLoopStop = false;
}

void MessageHandler::stopAllAnimations() {
    // deliberately does not clear animationLoopEnabled: the sleep paths call
    // this every night and ambient has to come back at wake. Only the explicit
    // user-facing stops clear it, via setAnimationLoopEnabled(false).
    stopAnimationLoop();
    ESP_LOGI("MSG", "Stopping all animations");
    // zero-initialised: the params ride along to every client, and garbage in
    // startTime made clients schedule the OFF absurdly far out and never darken
    message_animation stopAnimation{};
    stopAnimation.animationType = OFF;
    sendAnimation(stopAnimation, -1);
    ledInstance->setAnimation(stopAnimation);
}

void MessageHandler::startDarkroomTask() {
    if (darkroomHandle != NULL) {
        vTaskDelete(darkroomHandle);
        darkroomHandle = NULL;
    }
    xTaskCreatePinnedToCore(runDarkroomTaskWrapper, "runDarkroom", 10000, this, 2, &darkroomHandle, 0);
}
void MessageHandler::runDarkroomTaskWrapper(void *pvParameters) {
    MessageHandler *messageHandlerInstance = (MessageHandler *)pvParameters;
    messageHandlerInstance->runDarkroomTask();
}
void MessageHandler::runDarkroomTask() {
    while (true) {
        message_animation darkroomStrobe = ledInstance->createFlash(esp_timer_get_time() + 100000, 150, 1, 255, 0, 255);
        sendAnimation(darkroomStrobe, -1);
        vTaskDelay(200 / portTICK_PERIOD_MS);
        int nextBlink = random(darkroomParams.strobeMin, darkroomParams.strobeMax);
        message_animation darkroomCandle = ledInstance->createCandle(esp_timer_get_time() + 100000 + 150, nextBlink * 1000, 255, 255, darkroomParams.redlightMax);
        sendAnimation(darkroomCandle, -1);
        vTaskDelay(nextBlink * 1000 / portTICK_PERIOD_MS);
    }
}

void MessageHandler::sendLogMessage(const char* text) {
    message_data msg;
    msg.messageType = MSG_LOG;
    memcpy(msg.targetAddress, logDeviceAddress, 6);
    strncpy(msg.payload.log.text, text, sizeof(msg.payload.log.text) - 1);
    msg.payload.log.text[sizeof(msg.payload.log.text) - 1] = '\0';

    addPeer(logDeviceAddress);
    esp_now_send(logDeviceAddress, (uint8_t*)&msg, sizeof(msg));
    removePeer(logDeviceAddress);
}
#endif