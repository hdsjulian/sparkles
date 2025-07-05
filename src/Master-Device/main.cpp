#include <Arduino.h>
#include <MyDefines.h>
#include <esp_log.h>
#include "esp_now.h"
#include <LittleFS.h>
#include "WiFi.h"
#include <LedHandler.h>
#include <MessageHandler.h>
#include <Version.h>
#include <WebServer.h>

// put function declarations here:


LedHandler& ledInstance = LedHandler::getInstance();
MessageHandler& msgHandler = MessageHandler::getInstance();
uint8_t myAddress[6];

// state
bool lfs_started = true;

void OnDataRecv(const esp_now_recv_info *mac, const uint8_t *incomingData, int len) {
ESP_LOGI("Received", "Data at %d", micros());

}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {

}
unsigned long lastTick = 0;
int tickCount = 0;
void setup()
{
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_INFO);
  unsigned long long startTime = millis();
  while (!Serial)
  {
    if (millis() - startTime > 3000)
    {
      break;
    }
  }
  if (!LittleFS.begin())
  {
    Serial.println("LittleFS mount failed");
    lfs_started = false;
  }
  WiFi.mode(WIFI_AP_STA);
  if (esp_now_init() != ESP_OK)

  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
    delay(1000);
    ledInstance.setup();
    msgHandler.setup(ledInstance);
    WebServer& webServer = WebServer::getInstance(&LittleFS);
    webServer.setup(msgHandler);

  // put your setup code here, to run once:
}

void loop()
{
  if (lastTick + 5000 < millis())
  {
    //ledInstance.runBlink();
    uint8_t address[6];
    WiFi.macAddress(address);
    lastTick = millis();
    ESP_LOGI("", "Tick %s", msgHandler.stringAddress(address, true).c_str());
    Serial.println("Blub");
  }
  if (millis() > msgHandler.getSleepTime() && msgHandler.getSleepTime() > 0 && msgHandler.getSleepDuration() > 0) {
    ESP_LOGI("Sleep", "Going to sleep");
    msgHandler.sendSleepWakeupMessage(msgHandler.getSleepDuration());
    vTaskDelay(1000 / portTICK_PERIOD_MS); // Wait for the message to be sent
    esp_sleep_enable_timer_wakeup(msgHandler.getSleepDuration() - 2000); // Sleep for 24 hours
    esp_light_sleep_start();

  };
  // put your main code here, to run repeatedly:
}
