#include <Arduino.h>
#include <MyDefines.h>
#include <esp_log.h>
#include "esp_now.h"
#include <LittleFS.h>
#include "WiFi.h"
#include <LedHandler.h>
#include <MessageHandler.h>
#include <Version.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "soc/rtc.h"
// put function declarations here:



uint8_t myAddress[6];
//uint8_t hostAddress[6] = {0x34, 0x85, 0x18, 0x8f, 0xc1, 0x48};// Example address, replace with actual host address
uint8_t hostAddress[6] = {0x34, 0x85, 0x18, 0x8f, 0xbf, 0xb8};
QueueHandle_t receiveQueue, sendQueue ;
TaskHandle_t clapTaskHandle = NULL;
esp_now_peer_info_t peerInfo;
bool startUp = true;
#define CLAP_PIN 47
volatile bool buttonPressed = false;
static bool isInterruptAttached = false;
volatile unsigned long long buttonPressTime = 0;
void IRAM_ATTR handleButtonPress() {
  unsigned long long now = esp_timer_get_time();
  if (buttonPressed == false and (now - buttonPressTime) > 2500000ULL) {
    buttonPressTime = now;
    buttonPressed = true;
  }
}

// Timer sync, same algorithm as the client handleTimer: each packet carries the
// measured TX latency of the previous one (paired by counter), the burst's median wins.
unsigned long long lastReceiveTime = 0;
int delayAverage = 0;
int delayCounter = 0;
volatile long long timeOffset = 0;
volatile bool timerSynced = false;
long long pendingRaw = 0;
uint16_t pendingCounter = 0;
bool pendingValid = false;
long long samples[TIMER_ARRAY_COUNT];
int sampleCount = 0;

static void handleTimer(const message_data &msg) {
    const message_timer &timerMessage = msg.payload.timer;

    unsigned long long sinceLast = timerMessage.receiveTime - lastReceiveTime;
    if (timerMessage.reset || sinceLast > 1000000ULL) {
        // new burst, drop stale state
        sampleCount = 0;
        pendingValid = false;
        delayCounter = 0;
        delayAverage = 0;
    }

    if (pendingValid && timerMessage.counter == (uint16_t)(pendingCounter + 1)
        && timerMessage.lastDelay > 0 && timerMessage.lastDelay < 6000) {
        if (sampleCount < TIMER_ARRAY_COUNT) {
            samples[sampleCount++] = pendingRaw + (long long)timerMessage.lastDelay;
        }
        delayAverage = (delayAverage * delayCounter + timerMessage.lastDelay) / (delayCounter + 1);
        delayCounter++;
    }
    pendingRaw = (long long)timerMessage.sendTime - (long long)timerMessage.receiveTime;
    pendingCounter = timerMessage.counter;
    pendingValid = true;
    lastReceiveTime = timerMessage.receiveTime;

    if (sampleCount >= TIMER_ARRAY_COUNT) {
        long long sorted[TIMER_ARRAY_COUNT];
        memcpy(sorted, samples, sizeof(sorted));
        for (int i = 1; i < TIMER_ARRAY_COUNT; i++) {
            long long v = sorted[i];
            int j = i - 1;
            while (j >= 0 && sorted[j] > v) { sorted[j + 1] = sorted[j]; j--; }
            sorted[j + 1] = v;
        }
        timeOffset = (sorted[TIMER_ARRAY_COUNT / 2 - 1] + sorted[TIMER_ARRAY_COUNT / 2]) / 2;
        timerSynced = true;
        ESP_LOGI("CLAP", "Timer synced. Offset %lld, delay average %d", (long long)timeOffset, delayAverage);
        sampleCount = 0;
        pendingValid = false;
        delayCounter = 0;
        delayAverage = 0;
    }
}

static void clapTask(void *pvParameters) {
    // Simulate clap detection
    if (!isInterruptAttached) {
        attachInterrupt(digitalPinToInterrupt(CLAP_PIN), handleButtonPress, RISING);
        isInterruptAttached = true;
        Serial.println("Interrupt attached");
    }
    while (true) {
        if (buttonPressed == true) {
            buttonPressed = false;
            Serial.printf("CLAP! BPT: %llu\n", buttonPressTime);
            message_data clapMessage;
            clapMessage.messageType = MSG_CLAP;
            memcpy(clapMessage.targetAddress, broadcastAddress, 6);
            WiFi.macAddress(clapMessage.senderAddress);
            // contact-closure timestamp in master time, 0 when unsynced so the
            // master falls back to its local receive stamp
            if (timerSynced) {
                clapMessage.payload.clap.clapTime = (unsigned long long)((long long)buttonPressTime + timeOffset);
            }
            clapMessage.payload.clap.clapHappened = true;
            esp_now_send(clapMessage.targetAddress, (uint8_t *)&clapMessage, sizeof(clapMessage));
            vTaskDelay(1000 / portTICK_PERIOD_MS); // Allow some time for the message to be sent
            vTaskDelete(NULL);
        }
    }

  }
  // Detach the interrupt if the state is not MODE_CALIBRATE


static void handleReceive(void *pvParameters) {
    message_data incomingData;
    
    while (true) {
        if (xQueueReceive(receiveQueue, &incomingData, portMAX_DELAY) == pdTRUE) {
            //BETA
            if (incomingData.messageType == MSG_TIMER) {
                startUp = false;
                handleTimer(incomingData);
                continue;
            }
            if (incomingData.messageType == MSG_COMMAND) {
                startUp = false;
                switch (incomingData.payload.command.commandType) {
                    case CMD_START_CALIBRATION:
                    case CMD_START_DISTANCE_CALIBRATION:
                    case CMD_CONTINUE_CALIBRATION:
                    case CMD_CONTINUE_DISTANCE_CALIBRATION:
                        xTaskCreatePinnedToCore(clapTask, "clapTask", 10000, NULL, 10, &clapTaskHandle, 1);
                        break;
                    case CMD_MESSAGE:
                        break;
                    case CMD_CANCEL_CALIBRATION:
                    case CMD_END_CALIBRATION:
                        if (isInterruptAttached) {
                            detachInterrupt(digitalPinToInterrupt(CLAP_PIN));
                            isInterruptAttached = false;
                            Serial.println("Interrupt detached");
                            if (clapTaskHandle != NULL) {
                                vTaskDelete(clapTaskHandle);
                                clapTaskHandle = NULL;
                            }
                        } 
                        break;
                    default:
                        ESP_LOGW("MSG", "Unknown command type: %d", incomingData.payload.command.commandType);
                        break;
                }
            }
            
        }
    }
}

static void handleSend(void *pvParameters) {
    message_data messageData;
    while(true) {
        if (xQueueReceive(sendQueue, &messageData, portMAX_DELAY) == pdTRUE) {
            switch (messageData.messageType) {
                case MSG_COMMAND:
                    esp_now_send(messageData.targetAddress, (uint8_t *) &messageData, sizeof(messageData));
                    break;
                case MSG_ADDRESS:
                case MSG_SOUND_DEVICE:
                    esp_now_send(messageData.targetAddress, (uint8_t *) &messageData, sizeof(messageData));
                    break;
                    

           }
        }
    }   
}


void clapDetection() {

}


// Map the 32-bit hardware MAC-time RX stamp into the esp_timer domain. The smallest
// observed span over a burst is the clock-domain offset, jitter only ever adds delay.
static unsigned long long timerRxTimestamp(const esp_now_recv_info *info, unsigned long long cbTime) {
    if (info == nullptr || info->rx_ctrl == nullptr) return cbTime;
    int32_t span = (int32_t)((uint32_t)cbTime - info->rx_ctrl->timestamp);
    static int32_t minSpan = INT32_MAX;
    static unsigned long long lastCbTime = 0;
    if (cbTime - lastCbTime > 1000000ULL) minSpan = INT32_MAX; // stale after a 1 s gap
    lastCbTime = cbTime;
    if (span < minSpan) minSpan = span;
    return cbTime - (unsigned long long)(span - minSpan);
}

void pushToRecvQueue(const esp_now_recv_info *mac, const uint8_t *incomingData, int len) {
    // Master sends 80-byte ESPNOW_CLIENT_COMPAT_SIZE frames, accept anything up to full size
    if (len < 1 || len > (int)sizeof(message_data)) return;
    message_data msg;
    memcpy(&msg, incomingData, len);
    if (msg.messageType == MSG_TIMER) {
        msg.payload.timer.receiveTime = timerRxTimestamp(mac, esp_timer_get_time());
    }
    // don't block the WiFi task, drop instead of waiting on a full queue
    if (xQueueSend(receiveQueue, &msg, 0) != pdTRUE) {
        ESP_LOGE("MSG", "Failed to send data to receive queue");
    }
}


void pushToSendQueue(message_data& msg) {
    if (xQueueSend(sendQueue, &msg, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE("MSG", "Failed to send data to send queue");

    }
}


void OnDataRecv(const esp_now_recv_info *mac, const uint8_t *incomingData, int len) {
    pushToRecvQueue(mac, incomingData, len);
}

int addPeer(uint8_t * address) {
    memcpy(&peerInfo.peer_addr, address, 6);

    if (esp_now_get_peer(peerInfo.peer_addr, &peerInfo) == ESP_OK) {
        return 0;
    }
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        return -1;
    }
    else {
        return 1;
    }
    
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {

}
unsigned long lastTick = 0;
int tickCount = 0;


void setup()
{
  Serial.begin(115200);
  receiveQueue = xQueueCreate(10, sizeof(message_data));
  sendQueue = xQueueCreate(10, sizeof(message_data));
  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("LED", ESP_LOG_NONE);
  unsigned long long startTime = millis();
  while (!Serial)
  {
    if (millis() - startTime > 3000)
    {
      break;
    }
  }
    pinMode(CLAP_PIN, INPUT_PULLDOWN);

  // Init external 32kHz xtal for accurate light sleep timing; falls back to internal RC if xtal is dead
  rtc_clk_32k_enable(true);
  rtc_clk_32k_bootstrap(10);
  uint32_t xtalCal = 0;
  for (int i = 0; i < 10 && xtalCal == 0; i++) { delay(10); xtalCal = rtc_clk_cal(RTC_CAL_32K_XTAL, 1000); }
  if (xtalCal != 0) { rtc_clk_slow_src_set(RTC_SLOW_FREQ_32K_XTAL); ESP_LOGI("XTAL", "32kHz xtal OK (cal=%u)", xtalCal); }
  else { rtc_clk_32k_enable(false); ESP_LOGW("XTAL", "32kHz xtal failed, using internal RC"); }
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  delay(1000);
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);   
  addPeer(const_cast<uint8_t*>(hostAddress));
  addPeer(const_cast<uint8_t*>(broadcastAddress));
  xTaskCreate(handleReceive, "handleReceive", 4096, NULL, 1, NULL);
  xTaskCreate(handleSend, "handleSend", 4096, NULL, 1, NULL);

  // put your setup code here, to run once:
}

void loop()
{
    if (startUp == true) {
        message_data addressMessage;
        addressMessage.messageType = MSG_SOUND_DEVICE;
        memcpy(addressMessage.targetAddress, broadcastAddress, 6);
        WiFi.macAddress(addressMessage.payload.address.address);
        memcpy(addressMessage.senderAddress, addressMessage.payload.address.address, 6);
        addressMessage.payload.address.version = VERSION;
        pushToSendQueue(addressMessage);
        delay(1000);
    }
  if (lastTick + 10000 < millis())
  {
    //ledInstance.runBlink();
    lastTick = millis();
    uint8_t address[6];
    WiFi.macAddress(address);
    
    unsigned long long currentTime = micros();

  }
  // put your main code here, to run repeatedly:
}
