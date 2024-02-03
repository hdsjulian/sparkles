#include <Arduino.h>

int mode;
bool sendAddressFlag= true;
bool blinking = false;
#define PRINT_MODE -1
#define HELLO 0
#define BLINK 1
#define ADDRESS_RCVD 2
#define NUM_ADDRESSES 8
#define ADDRESS_SIZE 6
int randnum = 0;
int bla = 0;


uint8_t color = 0;          // a value from 0 to 255 representing the hue
uint32_t R, G, B;           // the Red Green and Blue color components
uint8_t brightness = 255;  // 255 is maximum brightness, but can be changed.  Might need 256 for common anode to fully turn off.
const boolean invert = true;

float redfloat = 0, greenfloat = 0, bluefloat = 0;
int steps = 500;
int stepdelay = 1;
//Network
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t masterAddress[] = {0xA0, 0x76, 0x4E, 0x8C, 0x12, 0xB8};
bool isMaster = false;
//uint8_t broadcastAddress[] = {0xB4,0xE6,0x2D,0xE9,0x3C,0x21};
uint8_t myAddress[6];
esp_now_peer_info_t peerInfo;
esp_now_peer_num_t numPeers;
esp_now_peer_info_t testPeerInfo;

//Variables for Time Offset


struct message_address {
  uint8_t messageType = HELLO;
  uint8_t address[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
} outgoingAddressMessage, incomingAddressMessage;

struct message_blink {
  uint8_t messageType = BLINK;
  uint8_t color[3];
  bool broadcast = false;
} blinkMessage;

struct addresses {
  uint8_t address[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  bool registered;
} addressList[NUM_ADDRESSES-1];
int addressCounter = 0;

struct msg_address_rcvd {
  uint8_t messageType = ADDRESS_RCVD;
  uint8_t address[6];
} addressRcvd;

int addressReceived = false;
const int ledPinBlue = 20;  // 16 corresponds to GPIO16
const int ledPinRed = 9; // 17 corresponds to GPIO17
const int ledPinGreen = 3;  // 5 corresponds to GPIO5
const int ledPinGreen2 = 8;
const int ledPinRed2 = 19;
const int ledPinBlue2 = 18;
const int ledChannelRed1 = 0;
const int ledChannelGreen1 = 1;
const int ledChannelBlue1 = 2;
const int ledChannelRed2 = 3;
const int ledChannelGreen2 = 4;
const int ledChannelBlue2 = 5;
int freq = 5000;
int resolution = 8;
