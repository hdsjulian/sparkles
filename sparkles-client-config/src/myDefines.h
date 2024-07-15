#include <Arduino.h>
#if DEVICE_USED == 4
#include <espnow.h>
#else 
#include <esp_now.h>
#endif

#ifndef DEFINE_H
#define DEFINE_H

//LEDHANDLER
#define LEDC_TIMER_12_BIT  8
#define LEDC_BASE_FREQ     5000
#define LEDC_START_DUTY   (0)
#define LEDC_TARGET_DUTY  (4095)
#define LEDC_FADE_TIME    (3000)

//clapping
#define NUM_CLAPS 10
#define CLAP_THRESHOLD 1000000

/*
#if (DEVICE == V1)
    const int ledPinBlue1 = 20;  // 16 corresponds to GPIO16
    const int ledPinRed1 = 9; // 17 corresponds to GPIO17
    const int ledPinGreen1 = 3;  // 5 corresponds to GPIO5
    const int ledPinGreen2 = 8;
    const int ledPinRed2 = 19;
    const int ledPinBlue2 = 18;
#elif (DEVICE == V2)
*/

    const int ledPinBlue1 = 18;  // 16 corresponds to GPIO16
    const int ledPinRed1 = 38; // 17 cmsgrorresponds to GPIO17
    const int ledPinGreen1 = 8;  // 5 corresponds to GPIO5
    const int ledPinGreen2 = 3;
    const int ledPinRed2 = 9;
    const int ledPinBlue2 = 37;

//#endif
const int ledChannelRed1 = 0;
const int ledChannelGreen1 = 1;
const int ledChannelBlue1 = 2;
const int ledChannelRed2 = 3;
const int ledChannelGreen2 = 4;
const int ledChannelBlue2 = 5;

//webserv
#define CLAP_PIN 47
#define ADDRESS_LIST 1

//MESSAGING
#define MSG_ADDRESS 0
#define MSG_ANNOUNCE 1
#define MSG_TIMER_CALIBRATION 2
#define MSG_GOT_TIMER 3
#define MSG_ASK_CLAP_TIMES 5
#define MSG_SEND_CLAP_TIMES 6
#define MSG_ANIMATION 7
#define MSG_SWITCH_MODE 8
#define MSG_DISTANCE 9
#define MSG_NOCLAPFOUND -1
#define MSG_COMMANDS 101
#define MSG_ADDRESS_LIST 102
#define MSG_STATUS_UPDATE 103
#define MSG_END_CALIBRATION 104

#define CMD_START 200
#define CMD_MSG_SEND_ADDRESS_LIST 201
#define CMD_START_CALIBRATION_MODE 202
#define CMD_END_CALIBRATION_MODE 203
#define CMD_BLINK 204
#define CMD_MODE_NEUTRAL 205
#define CMD_GET_TIMER 206
#define CMD_START_ANIMATION 207
#define CMD_STOP_ANIMATION 208
#define CMD_END 220


#define NUM_DEVICES 20
#ifndef CALIBRATION_FREQUENCY
#define CALIBRATION_FREQUENCY 1000
#endif
#ifndef TIMER_INTERVAL_MS
#define TIMER_INTERVAL_MS 600
#endif
#define TIMER_ARRAY_COUNT 2


//MESSAGE STRUCTS
struct message_timer {
  uint8_t messageType;
  uint16_t counter;
  unsigned long sendTime;
  uint16_t lastDelay;
} ;
// 13 bytes

struct message_got_timer {
  uint8_t messageType = MSG_GOT_TIMER;
  uint16_t delayAvg;
  uint32_t timerOffset;
};
//9 bytes

struct message_switch_mode {
  uint8_t messageType = MSG_SWITCH_MODE;
  uint8_t mode;
} ;
//2 bytes
struct message_timer_received {
  uint8_t messageType = MSG_GOT_TIMER;
  uint8_t address[6];
  uint32_t timerOffset;
} ;
//14 bytes
struct message_announce {
  uint8_t messageType = MSG_ANNOUNCE;
  unsigned long sendTime;
  uint8_t address[6];
} ;


//15 bytes
struct message_address{
  uint8_t messageType = MSG_ADDRESS;
  uint8_t address[6];
} ;

struct message_distance{
  uint8_t messageType = MSG_DISTANCE;
  float distance;
};

// 7 bytes


struct message_set_sleep_wakeup {
  uint8_t messageType = MSG_SET_SLEEP_WAKEUP;
  int hours;
  int minutes;
  int seconds;
  bool isWakeup;
};



struct message_send_clap_times {
  uint8_t messageType = MSG_SEND_CLAP_TIMES;
  int clapCounter;
  unsigned long timeStamp[NUM_CLAPS]; //offsetted.
  float distance = 0;
};

//4+4*NUM_CLAPS, currently 44
struct client_address {
  uint8_t address[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  int id;
  float xLoc;
  float yLoc;
  float zLoc;
  uint32_t timerOffset;
  int delay;
  message_send_clap_times clapTimes;
  float distance;
} ;


struct message_address_list {
  uint8_t messageType = MSG_ADDRESS_LIST;
  int index;
  client_address clientAddress;
  int addressCounter = 0;
  int status;
};



struct message_animate {
  uint8_t messageType = MSG_ANIMATION; 
  uint8_t animationType;
  uint16_t speed;
  uint16_t delay;
  uint16_t reps;
  uint8_t rgb1[3];
  uint8_t rgb2[3];
  unsigned long startTime;
} ;


struct message_command {
  int messageType = MSG_COMMANDS;
  int messageId;
  int param = -1;
  
} ;

struct message_status_update {
  int messageType = MSG_STATUS_UPDATE;
  int mode;
  
} ;


struct concentric_animation {
  uint8_t speed = 0;
  unsigned long startTime = 0;
  uint8_t reps = 0;
  uint8_t rgb1[3] = {0,0,0};
  uint8_t rgb2[3] = {0,0,0};
};


//STATE MACHINE
#define MODE_INIT -1
#define MODE_SEND_ANNOUNCE 0
#define MODE_SENDING_TIMER 1
#define MODE_STARTUP 2
#define MODE_WAIT_FOR_TIMER 3
#define MODE_CALIBRATE 5
#define MODE_ANIMATE 7
#define MODE_NEUTRAL 8


#define MODE_NO_SEND 90
#define MODE_RESPOND_ANNOUNCE 91
#define MODE_RESPOND_TIMER 92
#define MODE_WAIT_TIMER_RESPONSE 93
#define MODE_WAIT_ANNOUNCE_RESPONCE 94
#define MODE_SEND_ADDRESS_LIST 95

#endif
