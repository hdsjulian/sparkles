#include <Arduino.h>
#include <MyDefines.h>
#include "soc/rtc.h"
#include "esp_sleep.h"
#include "driver/uart.h"
#include <WiFi.h>
#include <Ota.h>

#define SLEEP_DURATION_US  (1 * 1000000ULL)  // 1 second
#define BLINK_BRIGHTNESS   5                 // ~10% of 255 (8-bit LEDC)
#define BLINK_ON_MS        300
#define BLINK_OFF_MS       250

void setAllLeds(uint8_t r, uint8_t g, uint8_t b) {
    ledcWrite(LEDPINRED1,   r);
    ledcWrite(LEDPINGREEN1, g);
    ledcWrite(LEDPINBLUE1,  b);
    ledcWrite(LEDPINRED2,   r);
    ledcWrite(LEDPINGREEN2, g);
    ledcWrite(LEDPINBLUE2,  b);
}

void blink(uint8_t r, uint8_t g, uint8_t b, int times) {
    for (int i = 0; i < times; i++) {
        setAllLeds(r, g, b);
        delay(BLINK_ON_MS);
        setAllLeds(0, 0, 0);
        delay(BLINK_OFF_MS);
    }
}

// Returns true if xtal is oscillating and clock source was switched.
// Returns false and leaves internal RC in place if xtal is dead/missing.
bool initXtal() {
    rtc_clk_32k_enable(true);
    // Drive the crystal for enough cycles to overcome startup inertia.
    // rtc_clk_cal below takes ~30 ms per call (1000 cycles @ 32kHz),
    // so 512 bootstrap pulses is a safe minimum here.
    rtc_clk_32k_bootstrap(512);

    // 32kHz crystals need up to ~500 ms to reach stable oscillation.
    delay(500);

    // Require 3 consecutive non-zero calibration reads before trusting the xtal.
    // Each rtc_clk_cal(RTC_CAL_32K_XTAL, 1000) blocks ~30 ms.
    int consecutive = 0;
    uint32_t cal = 0;
    for (int i = 0; i < 20 && consecutive < 3; i++) {
        cal = rtc_clk_cal(RTC_CAL_32K_XTAL, 1000);
        ESP_LOGI("XTAL", "  cal[%d] = %u (consecutive=%d)", i, cal, consecutive);
        if (cal != 0) {
            consecutive++;
        } else {
            consecutive = 0;
        }
        delay(10);
    }

    if (consecutive >= 3) {
        rtc_clk_slow_src_set(RTC_SLOW_FREQ_32K_XTAL);
        ESP_LOGI("XTAL", "External 32kHz xtal OK (cal=%u)", cal);
        return true;
    }

    rtc_clk_32k_enable(false);
    ESP_LOGW("XTAL", "External 32kHz xtal failed — using internal RC oscillator");
    return false;
}

static bool g_xtalOk    = false;
static bool g_sleptOk   = false;
static bool g_otaFailed = false;

void lightSleepMs(uint64_t us) {
    // Drain TX before sleep
    uart_wait_tx_idle_polling(CONFIG_ESP_CONSOLE_UART_NUM);
    Serial.end();

    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_sleep_enable_timer_wakeup(us);
    esp_light_sleep_start();

    // ---- wakeup resumes here ----

    // Restore slow clock source
    if (g_xtalOk) {
        rtc_clk_slow_src_set(RTC_SLOW_FREQ_32K_XTAL);
    }

    // Restore UART (APB clock may have changed frequency)
    uart_set_baudrate(CONFIG_ESP_CONSOLE_UART_NUM, 115200);
    Serial.begin(115200);
    delay(200);
}

void setup() {
    Serial.begin(115200);
    unsigned long startTime = millis();
    while (!Serial && millis() - startTime < 3000) {}

    ledcAttach(LEDPINRED1,   LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    ledcAttach(LEDPINGREEN1, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    ledcAttach(LEDPINBLUE1,  LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    ledcAttach(LEDPINRED2,   LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    ledcAttach(LEDPINGREEN2, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    ledcAttach(LEDPINBLUE2,  LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    setAllLeds(0, 0, 0);

    g_xtalOk = initXtal();

    ESP_LOGI("SLEEP", "Entering light sleep for %llu us", SLEEP_DURATION_US);
    lightSleepMs(SLEEP_DURATION_US);

    startTime = millis();
    while (!Serial && millis() - startTime < 3000) {}

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    g_sleptOk = (cause == ESP_SLEEP_WAKEUP_TIMER);

    ESP_LOGI("SLEEP", "Woke up — cause=%d xtal=%d", (int)cause, (int)g_xtalOk);

    if (g_xtalOk && g_sleptOk) {
        // 3 white blinks = hardware OK — proceed to OTA
        blink(BLINK_BRIGHTNESS, BLINK_BRIGHTNESS, BLINK_BRIGHTNESS, 3);

        ESP_LOGI("OTA", "Hardware OK — connecting to %s", OTA_WIFI_SSID);
        WiFi.mode(WIFI_OFF);
        delay(100);
        WiFi.mode(WIFI_STA);
        if (strlen(OTA_WIFI_PASSWORD) > 0) {
            WiFi.begin(OTA_WIFI_SSID, OTA_WIFI_PASSWORD);
        } else {
            WiFi.begin(OTA_WIFI_SSID);
        }
        int wifiRetries = 0;
        while (WiFi.status() != WL_CONNECTED && wifiRetries < 20) {
            // Slow blue blink = waiting for WiFi
            setAllLeds(0, 0, 80);
            delay(250);
            setAllLeds(0, 0, 0);
            delay(250);
            wifiRetries++;
            ESP_LOGI("OTA", "WiFi attempt %d — status %d", wifiRetries, (int)WiFi.status());
        }

        if (WiFi.status() != WL_CONNECTED) {
            ESP_LOGW("OTA", "WiFi unavailable after %d attempts — final status %d SSID: %s", wifiRetries, (int)WiFi.status(), OTA_WIFI_SSID);
            unsigned long blinkEnd = millis() + 3000;
            while (millis() < blinkEnd) {
                setAllLeds(BLINK_BRIGHTNESS, 0, 0);
                delay(300);
                setAllLeds(0, 0, 0);
                delay(300);
            }
            g_otaFailed = true;
        } else {
            blink(0, 80, 0, 3);
            ESP_LOGI("OTA", "WiFi connected — starting OTA from %s", OTA_UPDATE_URL);
            OTAHandler& ota = OTAHandler::getInstance();
            ota.setup();
            ota.performUpdate(); // reboots on success; returns only on failure

            // If we reach here the OTA download/flash failed
            ESP_LOGE("OTA", "OTA failed — check serial log");
            unsigned long blinkEnd = millis() + 3000;
            while (millis() < blinkEnd) {
                setAllLeds(BLINK_BRIGHTNESS, 0, 0);
                delay(300);
                setAllLeds(0, 0, 0);
                delay(300);
            }
            g_otaFailed = true;
        }
    } else if (g_sleptOk && !g_xtalOk) {
        // 3 red blinks = sleep OK, xtal missing
        blink(BLINK_BRIGHTNESS, 0, 0, 3);
    } else {
        // 3 yellow blinks = sleep failed (xtal result unreliable)
        blink(BLINK_BRIGHTNESS, BLINK_BRIGHTNESS, 0, 3);
    }
}

void loop() {
    // Slow heartbeat after a failed run — color shows which stage failed.
    // white  = hardware OK but OTA failed
    // red    = sleep OK, xtal missing
    // yellow = sleep failed
    // (success reboots into client firmware — we never reach loop)
    if (g_otaFailed) {
        setAllLeds(BLINK_BRIGHTNESS, BLINK_BRIGHTNESS, BLINK_BRIGHTNESS);
    } else if (g_sleptOk) {
        setAllLeds(BLINK_BRIGHTNESS, 0, 0);
    } else {
        setAllLeds(BLINK_BRIGHTNESS, BLINK_BRIGHTNESS, 0);
    }
    delay(1500);
    setAllLeds(0, 0, 0);
    delay(1500);
}
