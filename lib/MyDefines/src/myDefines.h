#include "Arduino.h"
#include "WiFi.h"
#include "Version.h"
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
#define CLAP_DEVICE 3
#define RASPI_DEVICE 4
#if DEVICE_MODE != CLIENT
  #define VERSION "1.0.2"
#elif DEVICE_MODE == CLIENT
  #define VERSION "1.0.2"
#endif

#define V1 1
#define V2 2
#define V3 3
#define V4 5

//magic numbers
#define OCTAVE 12
#define OCTAVESONKEYBOARD 8
#define ADMIN_PRESENT_TIMEOUT 60000
#define MIDI_MIN_RANGE 102
#define MIDI_MAX_RANGE 802
#define MIDI_VAL_MIN 0
#define MIDI_VAL_MAX 127
#define MIDI_SAT_MIN 0
#define MIDI_SAT_MAX 255
#define MIDI_MODE 1
#define FREQUENCY_MODE 2
#define INPUT_MODE FREQUENCY_MODE
#define MIDI_HUE 40
#define MIDI_SATURATION 51

#define INSTRUMENT_MIC 0
#define INSTRUMENT_KEYBOARD 1
#define INSTRUMENT_CC 3

//config variables
#define NUM_CLIENTS 200
#define TIMER_FREQUENCY 100
#define TIMER_ARRAY_COUNT 10
#define WIFI_SSID "SPARKLES"
#define WIFI_PASSWORD "sparklesAdmin"
#define OTA_WIFI_SSID "Pi_AP"
#define OTA_WIFI_PASSWORD "raspberry"
#define OTA_UPDATE_URL "http://192.168.4.1/firmware_client.bin" // Update URL for OTA updates
#define BATTERY_LOW_THRESHOLD 0.0 // Percentage below which battery is considered low
#define CLAP_TIMEOUT 10000
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
#define MSG_SYSTEM_STATUS 7
#define MSG_ASK_COMMAND 8
#define MSG_WAIT_FOR_INSTRUCTIONS 9
#define MSG_SLEEP_WAKEUP 10
#define MSG_SYNC 11
#define MSG_CLAP 12
#define MSG_CONFIG_DATA 13
#define MSG_UPDATE_VERSION 14
#define MSG_COMMAND 15
#define MSG_MIDI_PARAMS 16

#define CMD_START_CALIBRATION 1
#define CMD_CONTINUE_CALIBRATION 2
#define CMD_STOP_CALIBRATION 3
#define CMD_END_CALIBRATION 4
#define CMD_ASK_ADMIN_PRESENT 5
#define CMD_SET_ADMIN_PRESENT 6 
#define CMD_SET_ADMIN_NOT_PRESENT 7
#define CMD_MESSAGE 8
#define CMD_TEST_CALIBRATION 9
#define CMD_START_DISTANCE_CALIBRATION 10
#define CMD_CONTINUE_DISTANCE_CALIBRATION 11
#define CMD_END_DISTANCE_CALIBRATION 12




enum activeStatus {
  ACTIVE, 
  INACTIVE, 
  WAITING, 
  SETTING_TIMER, 
  DEAD,
  UNREACHABLE

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
    MIDI, 
    BACKGROUND_SHIMMER
};
struct midiNoteTable {
  int velocity;
  int note;
  unsigned long long startTime;
  bool sustainPressed;
  int instrument;
};
   
struct client_address {
  uint8_t address[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  int id;
  float xPos;
  float yPos;
  float zPos;
  int32_t timerOffset;
  int delay;
  float distances[NUM_CLAPS];  
  activeStatus active = INACTIVE;
  float batteryPercentage;
  int tries;
  float distanceFromCenter;
  unsigned long lastUpdateTime;
} ;

struct clap_table {
  unsigned long long clapTime;
  float xPos;
  float yPos;
};

struct message_address{
  uint8_t address[6];
  Version version;
  message_address() : address{0}, version(VERSION) {}
  message_address(const message_address& other) : version(other.version) {
    if (this != &other) memcpy(address, other.address, sizeof(address));
  }
};

struct message_midi_params { 
  int valMin;
  int valMax;
  int satMin;
  int satMax;
  int rangeMin;
  int rangeMax;
  float rmsMin;
  float rmsMax;
  int mode;
  message_midi_params() : valMin(MIDI_VAL_MIN), valMax(MIDI_VAL_MAX), satMin(MIDI_SAT_MIN), satMax(MIDI_SAT_MAX), rangeMin(MIDI_MIN_RANGE), rangeMax(MIDI_MAX_RANGE), rmsMin(0.1f), rmsMax(8.0f), mode(INPUT_MODE) {}
  message_midi_params(const message_midi_params& other) : valMin(other.valMin), valMax(other.valMax), satMin(other.satMin), satMax(other.satMax), rangeMin(other.rangeMin), rangeMax(other.rangeMax), rmsMin(other.rmsMin), rmsMax(other.rmsMax), mode(other.mode) {}

};

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

struct animation_sync_async_blink {
  int spreadTime;
  int blinkDuration;
  int pause;
  uint8_t repetitions;
  uint16_t animationReps;
  unsigned long long startTime;
  int hue;
  int saturation;
  int brightness;
  uint8_t fraction;
  animation_sync_async_blink() : spreadTime(0), blinkDuration(0), pause(0), repetitions(0), animationReps(0), startTime(0), hue(0), saturation(0), brightness(0), fraction(0) {}
};
struct animation_midi {
  uint8_t note;
  uint8_t velocity;
  uint8_t octaveDistance;
  uint8_t offset; // Offset to adjust the MIDI note to the correct LED position
  uint8_t instrument; // Optional field for instrument type
  animation_midi() : note(0), velocity(0), octaveDistance(0), offset(0), instrument(0) {}
};

struct animation_background_shimmer {
  int hue;
  int saturation;
  int value;
  animation_background_shimmer() : hue(0), saturation(0), value(0) {}
};

union animation_params {
  struct animation_strobe strobe;
  struct animation_midi midi;
  struct animation_blink blink;
  struct animation_sync_async_blink syncAsyncBlink;
  struct animation_background_shimmer backgroundShimmer;
  animation_params() {}
  ~animation_params() {}
};


struct message_animation {
  animationEnum animationType;
  animation_params animationParams;
  unsigned long timeStamp;
  message_animation() : animationType(OFF), animationParams(), timeStamp(0) {}
};

struct message_got_timer{
  int delayAverage;
  float batteryPercentage;
  long long perceivedTime;
  long long offset;
  unsigned long long sendTime;
  message_got_timer() : delayAverage(0), batteryPercentage(0.0), perceivedTime(0), offset(0), sendTime(0) {}
};

struct message_status {
  float batteryPercentage;
  message_status() : batteryPercentage(0.0) {}
};

struct message_ask_command {
  float batteryPercentage;
  unsigned long long perceivedTime;
  message_ask_command() : batteryPercentage(0.0), perceivedTime(0) {}
};


struct message_system_status {
  int numDevices;
  message_system_status() :  numDevices(0) {}
};

struct message_sleep_wakeup {
  unsigned long long duration; 
  message_sleep_wakeup() : duration(0) {}

};
struct message_timer {
  uint16_t counter;
  uint64_t sendTime;
  uint64_t receiveTime;
  uint16_t lastDelay;
  bool reset;
  int addressId;
  message_timer() :  counter(0), sendTime(0), receiveTime(0), lastDelay(0), reset(false), addressId(0) {}
} ;

struct message_clap {
  unsigned long long clapTime;
  bool clapHappened;
  message_clap() : clapTime(0), clapHappened(true) {}
};
struct message_config_data {
  int boardId;
  float xPos;
  float yPos;
  message_config_data() : boardId(0), xPos(0.0), yPos(0.0) {}
};

struct message_update_version {
  Version version;
  message_update_version() : version(VERSION) {}
  message_update_version(const message_update_version& other) : version(other.version) {
  }
};

struct message_command {
  uint8_t commandType;
  message_command() : commandType(0) {}
  message_command(uint8_t cmdType) : commandType(cmdType) {}
  message_command(const message_command& other) : commandType(other.commandType) {
}
};


union message_payload {
  struct message_address        address;
  struct message_timer          timer;
  struct message_animation      animation;
  struct message_got_timer      gotTimer;
  struct message_status         status;
  struct message_system_status  systemStatus;
  struct message_ask_command    askCommand;
  struct message_sleep_wakeup   sleepWakeup;
  struct message_clap           clap;
  struct message_config_data    configData;
  struct message_update_version updateVersion;
  struct message_command        command;
  struct message_midi_params    midiParams;
  message_payload() {}
  message_payload(const message_payload& other) {
      if (this != &other) memcpy(this, &other, sizeof(message_payload));
  }
  message_payload& operator=(const message_payload& other) {
      if (this != &other)
          memcpy(this, &other, sizeof(message_payload));
      return *this;
  }
  ~message_payload() {
  }
};

struct message_data {
  uint8_t         messageType;
  uint8_t         targetAddress[6];
  uint8_t         senderAddress[6];
  uint64_t         msgReceiveTime;
  message_payload payload;
  message_data()
      : messageType(0), targetAddress{0}, senderAddress{0}, msgReceiveTime(0), payload() {}
  message_data(const message_data& other) {
      if (this != &other) {
          messageType = other.messageType;
          memcpy(this->targetAddress, other.targetAddress, sizeof(this->targetAddress));
          memcpy(this->senderAddress, other.senderAddress, sizeof(this->senderAddress));
          msgReceiveTime = other.msgReceiveTime;
          payload = other.payload;
      }
  }
  
  message_data& operator=(const message_data& other) {
      if (this != &other) {
          messageType = other.messageType;
          memcpy(this->targetAddress, other.targetAddress, sizeof(this->targetAddress));
          memcpy(this->senderAddress, other.senderAddress, sizeof(this->senderAddress));
          payload = other.payload;
      }
      return *this;
  }
};




#endif