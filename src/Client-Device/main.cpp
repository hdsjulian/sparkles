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

extern uint32_t g_lightSleepMarker; // crash breadcrumb, see MessageHandler_client.cpp

static const char* resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT";
    case ESP_RST_SW:        return "SW";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    default:                return "UNKNOWN";
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0); // non-blocking CDC writes — drop bytes rather than hang
  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("LED", ESP_LOG_NONE);
  delay(100);
  ESP_LOGW("BOOT", "reset reason: %s (%d)", resetReasonName(esp_reset_reason()), (int)esp_reset_reason());
  // the wake path can't report its own death (USB still re-enumerating), so the
  // sleep loop leaves a breadcrumb in RTC memory and we read it here instead
  if (g_lightSleepMarker == 0xA55A0001u) {
    ESP_LOGE("BOOT", "previous boot DIED DURING LIGHT SLEEP (or instantly at wake)");
  } else if (g_lightSleepMarker == 0xA55A0002u) {
    ESP_LOGE("BOOT", "previous boot DIED IN THE WAKE PATH (Serial/WiFi re-init)");
  }
  g_lightSleepMarker = 0;
  if (!LittleFS.begin(true)) // format on fail — the crash-reboots corrupted at least one board's fs
  {
    Serial.println("LittleFS mount failed");
    lfs_started = false;
  }

  // Light-sleep timing clock, in preference order (same probe as the clap device):
  // 1. the populated 32k xtal — dead on about half the boards, so only trust it
  //    after calibration proves it actually oscillates (bounded probe, ~100ms)
  // 2. default RC_SLOW otherwise (a few % nap drift, absorbed by the sleep design).
  // Never 8MD256: that source hangs the light-sleep entry/exit sync with
  // interrupts off -> INT_WDT reset (found via the RTC crash breadcrumb).
  rtc_clk_32k_enable(true);
  rtc_clk_32k_bootstrap(10);
  uint32_t xtalCal = 0;
  for (int i = 0; i < 10 && xtalCal == 0; i++) { delay(10); xtalCal = rtc_clk_cal(RTC_CAL_32K_XTAL, 1000); }
  if (xtalCal != 0) { rtc_clk_slow_src_set(RTC_SLOW_FREQ_32K_XTAL); ESP_LOGI("XTAL", "32kHz xtal OK (cal=%u)", xtalCal); }
  else { rtc_clk_32k_enable(false); ESP_LOGW("XTAL", "32kHz xtal dead, using internal RC slow clock"); }
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // modem sleep adds RX latency and skews hardware RX timestamps
  ESP_LOGI("", "Setup1");
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  delay(1000);
  ledInstance.setup();
  msgHandler.setup(ledInstance);
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
