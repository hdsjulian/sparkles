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
uint8_t hostAddress[6] = {0x34, 0x85, 0x18, 0x8f, 0xc1, 0x48};// Example address, replace with actual host address
QueueHandle_t receiveQueue, sendQueue ;
esp_now_peer_info_t peerInfo;
bool startUp = true;
#define CLAP_PIN 47
volatile bool buttonPressed = false;
static bool isInterruptAttached = false;
unsigned long buttonPressTime = 0;
void IRAM_ATTR handleButtonPress() {
  if (buttonPressed == false and (micros()-buttonPressTime)>2500000) {
    buttonPressTime = micros();
    buttonPressed = true;
  } 
}
static void clapTask(void *pvParameters) {
    ESP_LOGI("CLAP", "Clap2 task started");
    // Simulate clap detection
    if (!isInterruptAttached) {
        attachInterrupt(digitalPinToInterrupt(CLAP_PIN), handleButtonPress, RISING);
        isInterruptAttached = true;
        Serial.println("Interrupt attached");
    }        
    while (true) {
        if (buttonPressed == true) {
            buttonPressed = false;      
            Serial.println("CLAP! BPT: "+String(buttonPressTime) );
            message_data clapMessage;
            clapMessage.messageType = MSG_CLAP;
            memcpy(clapMessage.targetAddress, broadcastAddress, 6);
            WiFi.macAddress(clapMessage.senderAddress);
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
            ESP_LOGI("MSG", "Received from queue %d", incomingData.messageType);
            if (incomingData.messageType == MSG_COMMAND) {
                startUp = false;
                switch (incomingData.payload.command.commandType) {
                    case CMD_START_CALIBRATION:
                    case CMD_START_DISTANCE_CALIBRATION:
                    case CMD_CONTINUE_CALIBRATION:
                    case CMD_CONTINUE_DISTANCE_CALIBRATION:
                    ESP_LOGI("MSG", "Starting calibration");
                        xTaskCreatePinnedToCore(clapTask, "clapTask", 10000, NULL, 10, NULL, 1);
                        break;
                    case CMD_MESSAGE:
                        ESP_LOGI("MSG", "Received message command");
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
                    ESP_LOGI("MSG", "Sending command message");
                    esp_now_send(messageData.targetAddress, (uint8_t *) &messageData, sizeof(messageData));
                    break;
                case MSG_ADDRESS:
                    ESP_LOGI("MSG", "Sending address message");
                    esp_now_send(messageData.targetAddress, (uint8_t *) &messageData, sizeof(messageData));
                    break;
                    

           }
        }
    }   
}


void clapDetection() {

}


void pushToRecvQueue(const esp_now_recv_info *mac, const uint8_t *incomingData, int len) {
    if (len != sizeof(message_data)) return;
    message_data *msg = (message_data *)incomingData;
     if (msg->messageType == MSG_TIMER) {
        msg->payload.timer.receiveTime = micros();
     }

    if (xQueueSend(receiveQueue, msg, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE("MSG", "Failed to send data to receive queue");
    }
}


void pushToSendQueue(message_data& msg) {
    ESP_LOGI("MSG", "Pushing to send queue");
    if (xQueueSend(sendQueue, &msg, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE("MSG", "Failed to send data to send queue");

    }
}


void OnDataRecv(const esp_now_recv_info *mac, const uint8_t *incomingData, int len) {
ESP_LOGI("Received", "Data at %d", micros());
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

  //rtc_clk_32k_enable(true);
  //rtc_clk_32k_bootstrap(10);

  //rtc_clk_slow_src_set(RTC_SLOW_FREQ_32K_XTAL);
  WiFi.mode(WIFI_STA);
  ESP_LOGI("", "Setup1");
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
        addressMessage.messageType = MSG_ADDRESS;
        memcpy(addressMessage.targetAddress, hostAddress, 6);
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
    
    ESP_LOGI("", "My Address %02x:%02x:%02x:%02x:%02x:%02x", address[0], address[1], address[2], address[3], address[4], address[5]);
    unsigned long long currentTime = micros();
    ESP_LOGI("", "Current Time %llu", currentTime);
    ESP_LOGI("", "Current Time %llu", micros());
    ESP_LOGI("", )

  }
  // put your main code here, to run repeatedly:
}
