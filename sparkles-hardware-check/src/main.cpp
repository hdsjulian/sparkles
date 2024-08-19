#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <PeakDetection.h> 
//#include <Preferences.h>
//#include <LittleFS.h>
#include <time.h>
#include <myDefines.h>
#include <ledHandler.h>
#include <ArduinoJson.h>
#include "soc/rtc.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "soc/rtc.h"
//#include "rtc_time.h"
//#include "ESPAsyncWebServer.h"
//#include "webserver.h"
#ifndef DINGSBUMS
#define DINGSBUMS 0
#endif
int audioPin = 5;
int freq = 5000;
int resolution = 8;
ledHandler handleLed;
const char* ssid = "Sparkles-Hw-Test";
const char* password = "sparkles";

const int batteryPin = 4; 

struct testing {
  bool rtc = true;
  bool send_message = false;
  bool recv_message = false;
  bool findGreen = false;
  bool lighttest = false;
  bool lighttest_full = false;
  bool light_and_send = false;
  bool claptest = false;
  bool claptest_send = false;
  bool measure_battery = false;
  bool deep_sleep_test = false;
  bool read_write_preferences = false;
  bool read_write_struct_littlefs = false;
  bool clockSetting = false;
  bool animationTest = false;
  bool ctd = false;
  bool webserver = false;
  bool printIP = false;
} testStruct;

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t myAddress[6];
esp_now_peer_info_t peerInfo;
esp_now_peer_num_t peerNum;
// put function declarations here:
PeakDetection peakDetection; 
PeakDetection peakDetection2;
//Preferences preferences;
int lastClap;
int lastClap2;
int claps = 0;
int measurements = 0;
int tick = 0;
bool clockset = false;
unsigned long measurementsTime;
unsigned long measurementsTimeEnd;
unsigned long measurementsTimeRead;
bool sendrecv = false;
struct message_hardware_check {
  uint8_t messageType = 0;
  unsigned long sendTime;
  int light_level;
  uint8_t address[6];
  bool clapOccurred = false;
  int adcVoltage;
  float voltage;
  int counter = 0;
} ;

#define NUM_CLAPS 20
#define TAG "rtc"



//4+4*NUM_CLAPS, currently 44
//todo
client_address clientAddress[200];

message_hardware_check announceMessage;
message_animate animationMessage;
//webserver myWebserver(&LittleFS);
unsigned long msgRecvTime = 0;
unsigned long msgSendTime = 0;




void setClock(int hours, int minutes, int seconds) {
    struct tm timeinfo;
    memset(&timeinfo, 0, sizeof(timeinfo));
    timeinfo.tm_hour = hours;
    timeinfo.tm_min = minutes;
    timeinfo.tm_sec = seconds;
    timeinfo.tm_year = 2024;
    timeinfo.tm_mon = 6;
    timeinfo.tm_mday = 21;
    struct timeval tv;
    tv.tv_sec = mktime(&timeinfo);
    Serial.println("Time set to: "+String(tv.tv_sec));
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
}

int* getSystemTime() {
    static int timeArray[3];
    struct tm timeinfo;
    time_t now;

    time(&now);
    Serial.println("now =  " + String(now));
    localtime_r(&now, &timeinfo);

    timeArray[0] = timeinfo.tm_hour;
    timeArray[1] = timeinfo.tm_min;
    timeArray[2] = timeinfo.tm_sec;

    return timeArray;
}
double calculateTimeDifference(int hours1, int minutes1, int seconds1, int hours2, int minutes2, int seconds2) {
    struct tm timeinfo1;
    struct tm timeinfo2;
    time_t time1;
    time_t time2;

    // Set the first time
    timeinfo1.tm_hour = hours1;
    timeinfo1.tm_min = minutes1;
    timeinfo1.tm_sec = seconds1;
    timeinfo1.tm_year = 2024;
    timeinfo1.tm_mon = 6;
    timeinfo1.tm_mday = 21;
    time1 = mktime(&timeinfo1);
    Serial.println("time1 =   " + String(time1));

    // Set the second time
    timeinfo2.tm_hour = hours2;
    timeinfo2.tm_min = minutes2;
    timeinfo2.tm_sec = seconds2;
    timeinfo2.tm_year = 2024;
    timeinfo2.tm_mon = 6;
    if (timeinfo2.tm_hour < timeinfo1.tm_hour) {
        timeinfo2.tm_mday = 22;
    }
    else if (timeinfo2.tm_hour == timeinfo1.tm_hour and timeinfo2.tm_min < timeinfo1.tm_min) {
        timeinfo2.tm_mday = 22;
    }
    else if (timeinfo2.tm_hour == timeinfo1.tm_hour and timeinfo2.tm_min == timeinfo1.tm_min and timeinfo2.tm_sec < timeinfo1.tm_sec) {
        timeinfo2.tm_mday = 22;
    }
    else {
        timeinfo2.tm_mday = 21;
    }

    time2 = mktime(&timeinfo2);
    Serial.println("time2 =   " + String(time2));

    // Calculate the difference
    double difference = difftime(time2, time1);

    return difference;
}
/*
void writeStructsToFile(const client_address* data, int count, const char* filename) {
    File file = LittleFS.open(filename, "w");
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
        
    }
    for (int i = 0; i < count; i++) {
        file.write((uint8_t*)&data[i], sizeof(client_address));
    }
    file.close();
}

// Function to read an array of structs from a file
void readStructsFromFile(client_address* data, int count, const char* filename) {
    File file = LittleFS.open(filename, "r");
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }
    for (int i = 0; i < count; i++) {
        file.read((uint8_t*)&data[i], sizeof(client_address));
    }
    file.close();
    
}
*/
String stringAddress(const uint8_t * mac_addr){
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    return(String(macStr));
}
void  OnDataRecv(const esp_now_recv_info * mac, const uint8_t *incomingData, int len) {
  sendrecv = true;
}

void  OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t sendStatus)  {
     if (sendStatus == ESP_NOW_SEND_SUCCESS) {
     }
}
void setup() {
    Serial.begin(115200);
//  while (!Serial) {
 //   ;
 // }
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  //WiFi.softAP(ssid, password);
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;

 if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
    /*
    if (!LittleFS.begin()) {
        Serial.println("LittleFS Mount Failed");
        if (LittleFS.format()) {
            Serial.println("LittleFS formatted successfully");
        } else {
            Serial.println("LittleFS format failed");
            return;
        }
    }*/

bool clockset = false;
  handleLed.setup();

  analogReadResolution(12);
  analogSetPinAttenuation(batteryPin, ADC_11db);
  WiFi.macAddress(myAddress);
  pinMode(audioPin, INPUT); 
  peakDetection.begin(48, 10, 0.5);
  //esp_sleep_enable_timer_wakeup(10000000); 
  //handleLed.ledsOff();
  Serial.println("Setup done ");
  //preferences.begin("sparkles", true);
  // put your setup code here, to run once:
  //handleLed.setPosition(1);
  animationMessage.speed = 1000;
animationMessage.pause = 1000;
animationMessage.reps = 20;
animationMessage.rgb1[0] = 255;
animationMessage.rgb1[1] = 0;
animationMessage.rgb1[2] = 0;
animationMessage.startTime = micros() + 1000000;
animationMessage.num_devices = 1;
animationMessage.animationType = SYNC_ASYNC_BLINK;
  if (testStruct.webserver == true ) {
   //   myWebserver.setup();

  }
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

}


  static inline bool rtc_clk_cal_32k_valid(uint32_t xtal_freq, uint32_t slowclk_cycles, uint64_t actual_xtal_cycles)
  {
      uint64_t expected_xtal_cycles = (xtal_freq * 1000000ULL * slowclk_cycles) >> 15; // xtal_freq(hz) * slowclk_cycles / 32768
      uint64_t delta = expected_xtal_cycles / 2000;                                    // 5/10000
      return (actual_xtal_cycles >= (expected_xtal_cycles - delta)) && (actual_xtal_cycles <= (expected_xtal_cycles + delta));
  }

void loop() {
  while (tick < 10) {
    ESP_LOGI("TAG", "start %d", tick);
    delay(200);
    tick++;
  }
if (testStruct.rtc == true) {
  delay(3000);
  Serial.println("blubblub");
  ESP_LOGI(TAG, "Dingsbums %d", DINGSBUMS);
  rtc_cpu_freq_config_t config;
  rtc_clk_cpu_freq_get_config(&config);
  int mhz = (int)config.freq_mhz;
  ESP_LOGI(TAG, "Clock Config, Current CPU Freq: %d MHz", mhz);
  String xtal = config.source == RTC_CPU_FREQ_SRC_XTAL ? "XTAL" : "Other";
  ESP_LOGI(TAG, "Clock Config, Source: %s", xtal.c_str());
  rtc_slow_freq_t slow_clk_src = rtc_clk_slow_src_get();
  const char* source_name = "";
  float frequency_kHz = 0;
  switch (slow_clk_src) {
    case RTC_SLOW_FREQ_RTC:
      source_name = "Internal 150 kHz RC oscillator";
      frequency_kHz = 150;
      break;
    case RTC_SLOW_FREQ_32K_XTAL:
      source_name = "External 32.768 kHz XTAL";
      frequency_kHz = 32.768;
      break;
    case RTC_SLOW_FREQ_8MD256:
      source_name = "Internal 8 MHz RC oscillator, divided by 256";
      frequency_kHz = 8 * 1000 / 256;
      break;
    default:
      source_name = "Unknown";
      break;
  }
  ESP_LOGI(TAG, "RTC INFO: RTC Slow Clock Source: %s", source_name);
  ESP_LOGI(TAG, "RTC INFO: Frequency: %.3f kHz and %d", frequency_kHz, slow_clk_src);
  unsigned int cal = rtc_clk_cal(RTC_CAL_RTC_MUX, 1000);
  ESP_LOGI("Sleep", "RTC Calibration value: %u", cal);
  rtc_clk_32k_enable(true);
  rtc_clk_slow_src_set(RTC_SLOW_FREQ_32K_XTAL);
  delay(100);
  int j = 0;
  for (int i = 0; i < 1000000; i++) {
    j++;
  }
  slow_clk_src = rtc_clk_slow_src_get();
  delay(1000);
  if (slow_clk_src == RTC_SLOW_FREQ_32K_XTAL) {
    ESP_LOGI(TAG, "RTC slow clock source set to external 32.768 kHz XTAL successfully.");
  } else {
    ESP_LOGI(TAG, "Failed to set RTC slow clock source to external 32.768 kHz XTAL.");
  }
  ESP_LOGI(TAG, "Sleeping");
  
  esp_sleep_enable_timer_wakeup((unsigned long long)(20*1000000ULL));


  delay(100);
  uint32_t slowclk_cycles = 100;

      rtc_cal_sel_t cal_clk = RTC_CAL_RTC_MUX;
    if (slow_clk_src == SOC_RTC_SLOW_CLK_SRC_XTAL32K) {
        cal_clk = RTC_CAL_32K_XTAL;
    } else if (slow_clk_src == SOC_RTC_SLOW_CLK_SRC_RC_FAST_D256) {
        cal_clk  = RTC_CAL_8MD256;
    }
  assert(slowclk_cycles);
    rtc_xtal_freq_t xtal_freq = rtc_clk_xtal_freq_get();
    uint64_t xtal_cycles = rtc_clk_cal_internal(cal_clk, slowclk_cycles);

    if ((cal_clk == RTC_CAL_32K_XTAL) && !rtc_clk_cal_32k_valid(xtal_freq, slowclk_cycles, xtal_cycles)) {
      ESP_LOGI(TAG, "ZERO");
    }

    uint64_t divider = ((uint64_t)xtal_freq) * slowclk_cycles;
    uint64_t period_64 = ((xtal_cycles << RTC_CLK_CAL_FRACT) + divider / 2 - 1) / divider;
    uint32_t period = (uint32_t)(period_64 & UINT32_MAX);
    
  ESP_LOGI(TAG, "XTAL_FREQ = %d", xtal_freq);
  ESP_LOGI (TAG, "PERIOD %d", period);
  ESP_LOGI(TAG, "divider", divider);
  ESP_LOGI(TAG, "RTC_CAL_RTC_MUX %d", RTC_CAL_RTC_MUX);
  ESP_LOGI(TAG, "cal_clck %d", cal_clk);
  ESP_LOGI(TAG, "XTAL cycles = %d", xtal_cycles);


  ESP_LOGI(TAG, "woke up");
  slow_clk_src = rtc_clk_slow_src_get();
  if (slow_clk_src == RTC_SLOW_FREQ_32K_XTAL) {
    ESP_LOGI(TAG, "RTC slow clock source set to external 32.768 kHz XTAL.");
  } else {
    ESP_LOGI(TAG, "RTC Clock Source not set to external 32.768 kHz XTAL.");
  }

    esp_err_t result = esp_light_sleep_start();
    if (result == ESP_OK) {
        ESP_LOGI("Sleep", "Woke up from light sleep");
    } else {
        ESP_LOGE("Sleep", "Failed to enter light sleep");
    }
  delay(10000);
}


if (testStruct.findGreen == true) {
  for (int i = 0; i<40;i++) {
    Serial.println("Trying "+String(i));
    ledcAttach(i, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    ledcWrite(i, 255);
    delay(1000);
    ledcWrite(i, 0);

  }

}

if (testStruct.send_message == true) {
  if (millis() > msgSendTime+700) {
    Serial.println("Sending message");
    esp_now_send(broadcastAddress, (uint8_t *) &announceMessage, sizeof(announceMessage));
    msgSendTime = millis();
  }

}
if (testStruct.recv_message == true ){
  if (sendrecv == true) {
    msgRecvTime = millis();
    handleLed.flash(0, 125, 0, 200, 1, 500);
    sendrecv = false;
  }

    
}
if (testStruct.lighttest == true) {
  Serial.println("Leds individually on");
  Serial.println("Red front");
  handleLed.ledOn(255, 0, 0, 1000, 0);
  handleLed.ledsOff();
  Serial.println("Green front");
  handleLed.ledOn(0, 255, 0, 1000, 0);
  handleLed.ledsOff();
  Serial.println("Blue front");
  handleLed.ledOn(0, 0, 255, 1000, 0);
  handleLed.ledsOff();
  Serial.println("Red back");
  handleLed.ledOn(255, 0, 0, 1000, 1);
  handleLed.ledsOff();
  Serial.println("Green back");
  handleLed.ledOn(0, 255, 0, 1000, 1);
  handleLed.ledsOff();
  Serial.println("Blue back ");
  handleLed.ledOn(0, 0, 255, 1000, 1);
  handleLed.ledsOff();
  Serial.println("Leds together");
  Serial.println("Red both");
  handleLed.ledOn(255, 0, 0, 1000, 2);
  handleLed.ledsOff();
  Serial.println("Green both");
  handleLed.ledOn(0, 255, 0, 1000, 2);
  handleLed.ledsOff();
  Serial.println("Blue both ");
  handleLed.ledOn(0, 0, 255, 1000, 2);
  handleLed.ledsOff();
}
if (testStruct.lighttest_full == true) {
  Serial.println("all");
  handleLed.ledOn(255, 255, 255, 1000, 2);
  handleLed.ledsOff();
}
if (testStruct.light_and_send == true) {
  Serial.println("lamps on 64 and sending data");
  announceMessage.light_level = 64;
  announceMessage.counter++;
  handleLed.ledOn(64, 64, 64, 0, 2);
  esp_now_send(broadcastAddress, (uint8_t *) &announceMessage, sizeof(announceMessage));
  delay(500);
  handleLed.ledsOff();
  Serial.println("lamps on 128 and sending data");
  announceMessage.light_level = 128;
  announceMessage.counter++;
  handleLed.ledOn(128, 128, 128, 0, 2);
  esp_now_send(broadcastAddress, (uint8_t *) &announceMessage, sizeof(announceMessage));
  delay(500);
  handleLed.ledsOff();
  Serial.println("lamps on 192 and sending data");
  announceMessage.light_level = 192;
  announceMessage.counter++;
  handleLed.ledOn(192, 192, 192, 0, 2);
  delay(500);
  handleLed.ledsOff();
 esp_now_send(broadcastAddress, (uint8_t *) &announceMessage, sizeof(announceMessage));
/*
  Serial.println("lamps on 255 and sending data");
  announceMessage.light_level = 255;
  handleLed.ledOn(255, 255, 255, 0, 2);

  esp_now_send(broadcastAddress, (uint8_t *) &announceMessage, sizeof(announceMessage));
  */
  delay(1000);
  handleLed.ledsOff();
  announceMessage.light_level = 0;

}
if (testStruct.claptest == true or testStruct.claptest_send == true) {
  ESP_LOGI("clap", "start clapping");

  //Serial.println(sensorValue);
  while (true) {
    double data = (double)analogRead(audioPin)/2048-1;
    peakDetection.add(data); 

    int peak = peakDetection.getPeak(); 


    if (peak == -1 and millis() > lastClap+1000) {
      lastClap = millis();
      ESP_LOGI("clap", "Clap!");
      handleLed.flash(100, 0, 0, 200, 1, 50);
      handleLed.ledsOff();
      claps++;
      if (testStruct.claptest_send == true) {
        ESP_LOGI("clap", "clap, sending");
        announceMessage.clapOccurred = true;
        announceMessage.counter++;
        announceMessage.light_level = 255;
        ESP_LOGI("clap", "Sending clap message");
        ESP_LOGI("clap", "%d", announceMessage.clapOccurred);
    
        esp_now_send(broadcastAddress, (uint8_t *) &announceMessage, sizeof(announceMessage));
 
        announceMessage.clapOccurred = false;
      }
    }
    
  }
}
if (testStruct.measure_battery == true) {
   handleLed.ledsOff();

  while(true) {
    int adcValue = analogRead(batteryPin); // Read the ADC value
    float voltage = adcValue * (4.2 / 3220.0);
    float percentage;
    if (voltage >= 4.2) {
        percentage = 100.0;
    } else if (voltage >= 3.8 && voltage < 4.2) {
        percentage =  70.0 + (voltage - 3.8) / (4.2 - 3.8) * (100.0 - 70.0);
    } else if (voltage >= 3.6 && voltage < 3.8) {
        percentage = 15.0 + (voltage - 3.6) / (3.8 - 3.6) * (70.0 - 15.0);
    } else if (voltage >= 3.0 && voltage < 3.6) {
        percentage =  (voltage - 3.0) / (3.6 - 3.0) * 15.0;
    } else {
        percentage = 0.0;
    }
    percentage = round(percentage * 100) / 100;
    Serial.print("ADC Value: ");
    Serial.print(adcValue);
    Serial.print(" | Voltage: ");
    Serial.print(voltage);
    Serial.print("V | Percentage: ");
    Serial.println(percentage);
    announceMessage.adcVoltage = adcValue;
    announceMessage.voltage = voltage;
    announceMessage.counter++;
    esp_now_send(broadcastAddress, (uint8_t *) &announceMessage, sizeof(announceMessage));
    delay(1000);
    measurements++;
    if (measurements == 2) {
      measurements = 0;
      break;
    }
  }
}


if (testStruct.deep_sleep_test == true) {
  if (clockset == false) {
    Serial.println("Going to sleep now"+String(micros()));
    unsigned long long sleepTimeMicroseconds = (unsigned long long) 10 * 1000000ULL;
    esp_sleep_enable_timer_wakeup(sleepTimeMicroseconds);
    esp_light_sleep_start();
    Serial.println("Woke up"+String(micros()));
    clockset = true;
  }
  else {
    Serial.println("leds");
    handleLed.ledOn(125, 125, 125, 500, 2);
    handleLed.ledsOff();
    delay(1000);
  }
}

if (testStruct.read_write_preferences == true) {
  while (true) {
    Serial.println("Size: "+String(sizeof(clientAddress)));
    testStruct.read_write_preferences = false;
    break;
    }
/*    measurementsTime = micros();
    measurementsTimeRead = preferences.getULong("time");
    measurementsTimeEnd = micros();
    Serial.println("Starting time reading from preferences: "+String(measurementsTime)+" Time to read "+String(measurementsTimeEnd-measurementsTime));
    measurements++;

    if (measurements == 10) {
      measurements = 0;
      break;
    }
  }
  */
}
  if (testStruct.read_write_struct_littlefs == true) {
    
     Serial.println("hä");
      while (true) { 
        
        for (int i = 0; i < 200; i++)
        {
        clientAddress[i].address[0] = 0x01;
        clientAddress[i].address[1] = 0x02;
        clientAddress[i].address[2] = 0x03;
        clientAddress[i].address[3] = 0x04;
        clientAddress[i].address[4] = 0x05;
        clientAddress[i].address[5] = 0x06;
        clientAddress[i].id = 1;
        clientAddress[i].xLoc = 1.0;
        clientAddress[i].yLoc = 2.0;
        clientAddress[i].zLoc = 3.0;
        clientAddress[i].timerOffset = 4;
        clientAddress[i].delay = 5;
        clientAddress[i].clapTimes.clapCounter = 6;
        clientAddress[i].clapTimes.timeStamp[0] = 7;
        clientAddress[i].clapTimes.timeStamp[1] = 8;
        clientAddress[i].clapTimes.timeStamp[2] = 9;
        clientAddress[i].clapTimes.timeStamp[3] = 10;
        clientAddress[i].clapTimes.timeStamp[4] = 11;
        clientAddress[i].clapTimes.timeStamp[5] = 12;
        clientAddress[i].clapTimes.timeStamp[6] = 13;
        }

        Serial.println("Size: "+String(sizeof(clientAddress)));
        //writeStructsToFile(clientAddress, 200, "/clientAddress");        
        clientAddress[10].id = 8;
        //memset(clientAddress, 0, sizeof(clientAddress));
        //readStructsFromFile(clientAddress, 200,  "/clientAddress");
            Serial.print("Address: ");
        for (int i = 0; i < 6; i++) {
            Serial.print(clientAddress[10].address[i], HEX);
            if (i < 5) Serial.print(":");
        }
 
        Serial.println();
        Serial.println("ID: "+String(clientAddress[10].id));
        Serial.println("X: "+String(clientAddress[10].xLoc));
        Serial.println("Y: "+String(clientAddress[10].yLoc));
        Serial.println("Z: "+String(clientAddress[10].zLoc));
        Serial.println("TimerOffset: "+String(clientAddress[10].timerOffset));
        Serial.println("Delay: "+String(clientAddress[10].delay));
        Serial.println("ClapCounter: "+String(clientAddress[10].clapTimes.clapCounter));
        testStruct.read_write_struct_littlefs = false; 
       break;
      }
      
  }
  if (testStruct.ctd == true) {
    Serial.println(String(calculateTimeDifference(23, 50, 0, 4, 30, 0)));
    delay(1000);
  }
  if (testStruct.clockSetting == true) {
    Serial.println("Setting clock");
    if (clockset == false) {
      setClock(1, 30, 0);
      clockset = true;
    }
    int* time;
    time = getSystemTime();
    Serial.println("time is "+String(time[0])+":"+String(time[1])+":"+String(time[2])); 
    if (time[0] >= 1 and time[1] >= 30 and time[2] > 10) {
        double difference = calculateTimeDifference(time[0], time[1], time[2], 1, 30, 20);
        Serial.println("going to sleep for "+String(difference)+" seconds");
        esp_sleep_enable_timer_wakeup((unsigned long)(difference*1000000));
        esp_light_sleep_start();
        Serial.println("woke up");
        testStruct.clockSetting = false;
    }
    else {
        Serial.print("not going to sleep"+String(time[0])+":"+String(time[1])+":"+String(time[2]));
        if (time[0] <= 1) {
          Serial.println("hours "+String(time[0]));
        }
        if (time[1] <= 30) {
          Serial.println("minutes "+String(time[1]+1));
        }
        if (time[2] <= 10) {
          Serial.println("seconds "+String(time[2]));
        }
        delay(1000);
    }
  }

if (testStruct.animationTest == true) {
  handleLed.setupAnimation(&animationMessage);
  handleLed.run();
}

if (testStruct.webserver == true ) {
  if (tick+30000 < millis()) {
    tick = millis();
    esp_now_send(broadcastAddress, (uint8_t *) &announceMessage, sizeof(announceMessage));
  }
}


}


  // put your main code here, to run repeatedly:


