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
#include <Ota.h>
// put function declarations here:


LedHandler& ledInstance = LedHandler::getInstance();
MessageHandler& msgHandler = MessageHandler::getInstance();

uint8_t myAddress[6];

// state
bool lfs_started = true;

void OnDataRecv(const esp_now_recv_info *mac, const uint8_t *incomingData, int len) {
ESP_LOGI("Received", "Data at %d", micros());

}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus) {}
unsigned long lastTick = 0;
int tickCount = 0;
void setup()
{
  Serial.begin(115200);
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
  if (!LittleFS.begin())
  {
    Serial.println("LittleFS mount failed");
    lfs_started = false;
  }

  // Init external 32kHz xtal for accurate light sleep timing; falls back to internal RC if xtal is dead
  rtc_clk_32k_enable(true);
  rtc_clk_32k_bootstrap(512);
  delay(500);
  int xtalConsecutive = 0;
  uint32_t xtalCal = 0;
  for (int i = 0; i < 20 && xtalConsecutive < 3; i++) {
    xtalCal = rtc_clk_cal(RTC_CAL_32K_XTAL, 1000);
    xtalConsecutive = (xtalCal != 0) ? xtalConsecutive + 1 : 0;
    delay(10);
  }
  bool xtalOk = (xtalConsecutive >= 3);
  if (xtalOk) { rtc_clk_slow_src_set(RTC_SLOW_FREQ_32K_XTAL); ESP_LOGI("XTAL", "32kHz xtal OK (cal=%u)", xtalCal); }
  else { rtc_clk_32k_enable(false); ESP_LOGW("XTAL", "32kHz xtal failed, using internal RC"); }
  WiFi.mode(WIFI_STA);
  ESP_LOGI("", "Setup1");
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  delay(1000);
  ledInstance.setup();
  msgHandler.setup(ledInstance);
  msgHandler.setXtalOk(xtalOk);
  ESP_LOGI("", "Setup");
  float batPercentage = msgHandler.getBatteryPercentage();
  ledInstance.batteryBlink(batPercentage);

  // put your setup code here, to run once:
}

void loop()
{
  if (lastTick + 10000 < millis())
  {
    lastTick = millis();
    uint8_t address[6];
    WiFi.macAddress(address);
    ESP_LOGI("", "Tick %s", msgHandler.stringAddress(address, true).c_str());
    ESP_LOGI("", "Current Time %llu", micros());
    ESP_LOGI("", "Battery: %.2f%%", msgHandler.getBatteryPercentage());
    ESP_LOGI("", "Version: %s", VERSION);


  }
  // put your main code here, to run repeatedly:
}
