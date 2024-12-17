#include "Arduino.h"
#if DEVICE_USED == 4
#include "espnow.h"
#else 
#include "esp_now.h"
#endif

#include "time.h"
#ifndef DEFINE_H
#define DEFINE_H

//LEDHANDLER
#define LEDC_TIMER_12_BIT  8
#define LEDC_BASE_FREQ     5000
#define LEDC_START_DUTY   (0)
#define LEDC_TARGET_DUTY  (4095)
#define LEDC_FADE_TIME    (3000)

//device modes

#define MASTER 0
#define CLIENT 1
#define WEBSERVER 2
const float version = 1.0;

#define V1 1
#define V2 2
#define V3 3
#define V4 5

//magic numbers
#define OCTAVE 12
#define OCTAVESONKEYBOARD 8



//config variables
#define NUM_CLIENTS 200
#define TIMER_FREQUENCY 100
#define TIMER_ARRAY_COUNT 10
#define WIFI_SSID "SPARKLES"
#define WIFI_PASSWORD "sparklesAdmin"

static constexpr uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};




#if (DEVICE_USED == V1)
    const int ledPinBlue1 = 20;  // 16 corresponds to GPIO16
    const int ledPinRed1 = 9; // 17 corresponds to GPIO17
    const int ledPinGreen1 = 3;  // 5 corresponds to GPIO5
    const int ledPinGreen2 = 8;
    const int ledPinRed2 = 19;
    const int ledPinBlue2 = 18;

#elif (DEVICE_USED == V2)


    const int ledPinBlue1 = 18;  // 16 corresponds to GPIO16
    const int ledPinRed1 = 38; // 17 cmsgrorresponds to GPIO17
    const int ledPinGreen1 = 8;  // 5 corresponds to GPIO5
    const int ledPinGreen2 = 3;
    const int ledPinRed2 = 9;
    const int ledPinBlue2 = 37;
#elif (DEVICE_USED == V3)
    const int ledPinBlue1 = 17;  // 16 corresponds to GPIO16
    const int ledPinRed1 = 38; // 17 cmsgrorresponds to GPIO17
    const int ledPinGreen1 = 8;  // 5 corresponds to GPIO5
    const int ledPinGreen2 = 3;
    const int ledPinRed2 = 9;
    const int ledPinBlue2 = 37;
#elif (DEVICE_USED == V4)
#define LEDPINBLUE1 17  // 16 corresponds to GPIO16
#define LEDPINBLUE2 37
#define LEDPINRED1 38 // 17 cmsgrorresponds to GPIO17
#define LEDPINRED2 9
#define LEDPINGREEN1 8  // 5 corresponds to GPIO5
#define LEDPINGREEN2 3

#endif


#define BATTERY_PIN 4
#define AUDIO_PIN 5
#define NUM_DEVICES 180
#define NUM_CLAPS 20

#define MSG_ADDRESS 1
#define MSG_TIMER 2
#define MSG_GOT_TIMER 3
#define MSG_STATUS 4
#define MSG_ANIMATION 5
#define MSG_SEND_CLAP_TIMES 6
enum activeStatus {
  ACTIVE, 
  INACTIVE, 
  WAITING, 
  SETTING_TIMER, 
  DEAD,
  UNREACHABLE

};

struct message_send_clap_times {
  uint8_t messageType = MSG_SEND_CLAP_TIMES;
  int clapCounter;
  unsigned long long timeStamp[NUM_CLAPS]; //offsetted.
  float xLoc = 0.0;
  float yLoc = 0.0;
  float zLoc = 0.0;
};

enum animationEnum {
    OFF,
    FLASH,
    BLINK,
    CANDLE,
    SYNC_ASYNC_BLINK,
    SYNC_BLINK,
    SLOW_STARTUP,
    SYNC_END,
    LED_ON,
    CONCENTRIC, 
    STROBE, 
    MIDI
};
struct midiNoteTable {
  int velocity;
  int note;
  unsigned long long startTime;
  bool sustainPressed;
};
   
struct client_address {
  uint8_t address[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  int id;
  float xLoc;
  float yLoc;
  float zLoc;
  uint32_t timerOffset;
  int delay;
  message_send_clap_times clapTimes;
  float distances[NUM_CLAPS];  
  activeStatus active = INACTIVE;
  float batteryPercentage;
  int tries;
  float distanceFromCenter;
  unsigned long lastUpdateTime;
} ;


struct message_address{
  uint8_t messageType = MSG_ADDRESS;
  uint8_t address[6];
  float version = version;
  message_address() : messageType(MSG_ADDRESS), address{0}, version(version) {}
} ;

struct animation_strobe {
  uint8_t frequency;
  unsigned long long startTime;
  int duration;
  int hue;
  int saturation;
  int brightness;
  animation_strobe() : frequency(0), startTime(0), duration(0), hue(0), saturation(0), brightness(0) {}
};

struct animation_blink {
  uint8_t repetitions;
  int duration;
  unsigned long long startTime;
  int hue;
  int saturation;
  int brightness;
  animation_blink() : repetitions(0), duration(0), startTime(0), hue(0), saturation(0), brightness(0) {}
};

struct animation_midi {
  uint8_t note;
  uint8_t velocity;
  uint8_t octaveDistance;
  animation_midi() : note(0), velocity(0), octaveDistance(0) {}
};

union animation_params {
  struct animation_strobe strobe;
  struct animation_midi midi;
  struct animation_blink blink;
  animation_params() {}
  ~animation_params() {}
};


struct message_animation {
  uint8_t messageType = MSG_ANIMATION;
  animationEnum animationType;
  animation_params animationParams;
  unsigned long timeStamp;
  message_animation() : messageType(MSG_ANIMATION), animationType(OFF), animationParams(), timeStamp(0) {}
};

struct message_got_timer{
  uint8_t messageType = MSG_GOT_TIMER;
  int delayAverage;
  float batteryPercentage;
  message_got_timer() : messageType(MSG_GOT_TIMER), delayAverage(0) {}
};

struct message_status {
  uint8_t messageType = MSG_STATUS;
  float batteryPercentage;
  message_status() : messageType(0), batteryPercentage(0.0) {}
};
     
struct message_timer {
  uint8_t messageType = MSG_TIMER;
  uint16_t counter;
  unsigned long long sendTime;
  unsigned long long receiveTime;
  uint16_t lastDelay;
  bool reset = false;
  int addressId = 0;
  message_timer() : messageType(MSG_TIMER), counter(0), sendTime(0), receiveTime(0), lastDelay(0), reset(false), addressId(0) {}
} ;


union message_payload { 
  struct message_address address;
  struct message_timer timer;
  struct message_animation animation;
  struct message_send_clap_times clapTimes;
  struct message_got_timer gotTimer;
  struct message_status status;

  message_payload() {}
  ~message_payload() {}
};

struct message_data {
  uint8_t messageType;
  uint8_t address[6];
  message_payload payload;
  message_data() : messageType(0), address{0}, payload() {}
};
#endif