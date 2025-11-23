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
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "soc/rtc.h"
//#include <Elog.h>

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
  esp_log_level_set("MSG", ESP_LOG_NONE);
  esp_log_level_set("LED", ESP_LOG_NONE);
  esp_log_level_set("Sleep", ESP_LOG_NONE);
  esp_log_level_set("TIMER", ESP_LOG_NONE);
  esp_log_level_set("CLAP", ESP_LOG_INFO);
  esp_log_level_set("Tick", ESP_LOG_INFO);
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

  //rtc_clk_32k_enable(true);
  //rtc_clk_32k_bootstrap(10);

  //rtc_clk_slow_src_set(RTC_SLOW_FREQ_32K_XTAL);
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
    ESP_LOGI("TICK", "Tick %s", msgHandler.stringAddress(address, true).c_str());
    ESP_LOGI("TICK", "Ticks until end: %llu", (unsigned long long)ledInstance.getNextAnimationTicks());
    ESP_LOGI("", "Battery: %.2f%%", msgHandler.getBatteryPercentage());

    unsigned long long now = micros();
    //ESP_LOGI("TICK", "Now is %llu", now);
    esp_log_level_t level = esp_log_level_get("Sleep");
    //ESP_LOGI("TICK", "Sleep log level: %d", level);
    Serial.println(micros());
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        //ESP_LOGI("", "Current Time: %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        //ESP_LOGI("", "Failed to obtain time");
    }
    if (!msgHandler.isInSleepPhase()) {
        //ESP_LOGI("Sleep", "Not in sleep phase");
        unsigned long sleepTime = msgHandler.getSleepTime();
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Wait for the message to be sent
        if (sleepTime > 0) {
            //ESP_LOGI("Sleep", "Next sleep time in %lu ms", sleepTime);
        } else {
            //ESP_LOGI("Sleep", "No sleep time set"); 
        }
    }
    // If lastMidiTime is more than 1 minute ago, start animation loop task
    if (millis() - msgHandler.getLastMidiTime() > 60000 && msgHandler.getLastMidiTime() > 0) {
      ESP_LOGI("MSG", "No MIDI message for 60 seconds, starting animation loop");
        msgHandler.startAnimationLoopTask();
        msgHandler.setLastMidiTime(0);
    }
  
  }

  if (msgHandler.isInSleepPhase()) {
    ESP_LOGI("Sleep", "Going to sleep for %lu ms", msgHandler.getSleepDuration());
    unsigned long long sleepDuration = (unsigned long long)(msgHandler.getSleepDuration()-1)*1000;
    ESP_LOGI("Sleep", "Sleep duration in micros: %llu", sleepDuration);
    ESP_LOGI("Sleep", "Sleep duration in seconds, hours and minutes: %02llu:%02llu:%02llu", sleepDuration/1000000/3600, (sleepDuration/1000000%3600)/60, (sleepDuration/1000000%3600)%60);
    esp_sleep_enable_timer_wakeup(sleepDuration);
    msgHandler.sendSleepWakeupMessage(sleepDuration);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        ESP_LOGI("Sleep", "Before Sleep Current Time: %02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        ESP_LOGI("Sleep", "Failed to obtain time");
    }
    ESP_LOGI("Sleep", "Time in micros: %llu", micros());
    msgHandler.turnWifiOff();
    msgHandler.recordTimeOfDayBeforeSleep();
    esp_light_sleep_start();
    msgHandler.setTimeOfDayAfterSleep();
    msgHandler.turnWifiOn();
    ESP_LOGI("Sleep", "Delaying for 1200 seconds");
    vTaskDelay(1200000 / portTICK_PERIOD_MS);
    ESP_LOGI("Sleep", "Continuing");
    msgHandler.setAddressListInactive();
    msgHandler.startAllTimerSyncTask();
    
    
   // msgHandler.sendSleepWakeupMessage(wbmsgHandler.getSleepDuration());

   // esp_sleep_enable_timer_wakeup(msgHandler.getSleepDuration() - 2000); // Sleep for 24 hours
   // esp_light_sleep_start();
    // Get the RTC slow clock source

  };
  // put your main code here, to run repeatedly:
}
