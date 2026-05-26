#include <Arduino.h>
#include <MyDefines.h>
#include <FastLED.h>
#include "esp_sleep.h"
#include "driver/uart.h"
#include "soc/rtc.h"
#include <WiFi.h>
#include <Ota.h>

// ── Test mode selector ──────────────────────────────────────────────────────
#define TEST_XTAL  0
#define TEST_LED   1
#define TEST_MODE  TEST_LED          // <── change this to switch test
// ────────────────────────────────────────────────────────────────────────────

#define SLEEP_DURATION_US  (60 * 1000000ULL)
#define BLINK_BRIGHTNESS   5
#define BLINK_ON_MS        300
#define BLINK_OFF_MS       250

// ── LED helpers ──────────────────────────────────────────────────────────────
void setLed1(uint8_t r, uint8_t g, uint8_t b) {
    ledcWrite(LEDPINRED1,   r);
    ledcWrite(LEDPINGREEN1, g);
    ledcWrite(LEDPINBLUE1,  b);
}
void setLed2(uint8_t r, uint8_t g, uint8_t b) {
    ledcWrite(LEDPINRED2,   r);
    ledcWrite(LEDPINGREEN2, g);
    ledcWrite(LEDPINBLUE2,  b);
}
void setAllLeds(uint8_t r, uint8_t g, uint8_t b) {
    setLed1(r, g, b);
    setLed2(r, g, b);
}
void blink(uint8_t r, uint8_t g, uint8_t b, int times) {
    for (int i = 0; i < times; i++) {
        setAllLeds(r, g, b);
        delay(BLINK_ON_MS);
        setAllLeds(0, 0, 0);
        delay(BLINK_OFF_MS);
    }
}

// ── XTAL test ────────────────────────────────────────────────────────────────
static bool g_sleptOk   = false;
static bool g_otaFailed = false;

void lightSleepMs(uint64_t us) {
    uart_wait_tx_idle_polling(CONFIG_ESP_CONSOLE_UART_NUM);
    Serial.end();
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_sleep_enable_timer_wakeup(us);
    esp_light_sleep_start();
    uart_set_baudrate(CONFIG_ESP_CONSOLE_UART_NUM, 115200);
    Serial.begin(115200);
    delay(200);
}

void runXtalTest() {
    rtc_clk_slow_src_set(RTC_SLOW_FREQ_8MD256);
    uint32_t cal = rtc_clk_cal(RTC_CAL_RTC_MUX, 1000);
    ESP_LOGI("CLK", "RC_FAST_D256 set — cal=%u (expect ~7700000 for ~68kHz; 0 = failed)", cal);

    blink(BLINK_BRIGHTNESS, 0, BLINK_BRIGHTNESS, 3);  // purple = about to sleep
    ESP_LOGI("SLEEP", "Entering light sleep for %llu us", SLEEP_DURATION_US);
    lightSleepMs(SLEEP_DURATION_US);

    unsigned long startTime = millis();
    while (!Serial && millis() - startTime < 3000) {}
    blink(BLINK_BRIGHTNESS, 0, BLINK_BRIGHTNESS, 3);  // purple = just woke up

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    g_sleptOk = (cause == ESP_SLEEP_WAKEUP_TIMER);
    ESP_LOGI("SLEEP", "Woke up — cause=%d sleptOk=%d", (int)cause, (int)g_sleptOk);

    if (g_sleptOk) {
        blink(BLINK_BRIGHTNESS, BLINK_BRIGHTNESS, BLINK_BRIGHTNESS, 3);
        ESP_LOGI("OTA", "Sleep OK — connecting to %s", OTA_WIFI_SSID);
        WiFi.mode(WIFI_OFF);
        delay(100);
        WiFi.mode(WIFI_STA);
        if (strlen(OTA_WIFI_PASSWORD) > 0) WiFi.begin(OTA_WIFI_SSID, OTA_WIFI_PASSWORD);
        else                               WiFi.begin(OTA_WIFI_SSID);

        int wifiRetries = 0;
        while (WiFi.status() != WL_CONNECTED && wifiRetries < 20) {
            setAllLeds(0, 0, 80); delay(250);
            setAllLeds(0, 0, 0);  delay(250);
            wifiRetries++;
        }
        if (WiFi.status() != WL_CONNECTED) {
            ESP_LOGW("OTA", "WiFi unavailable after %d attempts", wifiRetries);
            g_otaFailed = true;
        } else {
            blink(0, 80, 0, 3);
            OTAHandler& ota = OTAHandler::getInstance();
            ota.setup();
            ota.performUpdate();
            g_otaFailed = true;
        }
    } else {
        blink(BLINK_BRIGHTNESS, BLINK_BRIGHTNESS, 0, 3);  // yellow = sleep failed
    }
}

// ── LED test ─────────────────────────────────────────────────────────────────
#define LED_BRIGHT 80
#define LED_STEP(fn, r, g, b, lbl) do { setAllLeds(0,0,0); fn(r,g,b); ESP_LOGI("LED_TEST", lbl); delay(2000); } while(0)

// Shimmer colors from Raspi-Device: hueStart=22 sat=255 (low pitch), hueEnd=8 sat=0 (high pitch)
static void shimmerColor(uint8_t hue, uint8_t sat, uint8_t val, uint8_t* r, uint8_t* g, uint8_t* b) {
    CRGB rgb = CHSV(hue, sat, val);
    *r = rgb.r; *g = rgb.g; *b = rgb.b;
}

void runLedTest() {
    uint8_t r, g, b;
    while (true) {
        LED_STEP(setLed1, LED_BRIGHT, 0,          0,          "LED1 - should be RED");
        LED_STEP(setLed1, 0,          LED_BRIGHT, 0,          "LED1 - should be GREEN");
        LED_STEP(setLed1, 0,          0,          LED_BRIGHT, "LED1 - should be BLUE");
        LED_STEP(setLed2, LED_BRIGHT, 0,          0,          "LED2 - should be RED");
        LED_STEP(setLed2, 0,          LED_BRIGHT, 0,          "LED2 - should be GREEN");
        LED_STEP(setLed2, 0,          0,          LED_BRIGHT, "LED2 - should be BLUE");

        shimmerColor(22, 255, LED_BRIGHT, &r, &g, &b);
        setAllLeds(0, 0, 0); setAllLeds(r, g, b);
        ESP_LOGI("LED_TEST", "SHIMMER LOW PITCH - should be warm orange (hue=22 sat=255)");
        delay(2000);

        shimmerColor(8, 0, LED_BRIGHT, &r, &g, &b);
        setAllLeds(0, 0, 0); setAllLeds(r, g, b);
        ESP_LOGI("LED_TEST", "SHIMMER HIGH PITCH - should be near-white warm (hue=8 sat=0)");
        delay(2000);

        setAllLeds(0, 0, 0);
        delay(500);
    }
}

// ── Entry points ─────────────────────────────────────────────────────────────
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

#if TEST_MODE == TEST_LED
    ESP_LOGI("TEST", "Mode: LED — cycling R/G/B on each LED independently");
    runLedTest();  // never returns
#else
    ESP_LOGI("TEST", "Mode: XTAL — light sleep + OTA");
    runXtalTest();
#endif
}

void loop() {
#if TEST_MODE == TEST_XTAL
    if (g_otaFailed) {
        setAllLeds(BLINK_BRIGHTNESS, BLINK_BRIGHTNESS, BLINK_BRIGHTNESS);
    } else {
        setAllLeds(BLINK_BRIGHTNESS, BLINK_BRIGHTNESS, 0);
    }
    delay(1500);
    setAllLeds(0, 0, 0);
    delay(1500);
#endif
}
