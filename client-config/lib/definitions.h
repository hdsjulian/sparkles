#include <Arduino.h>
#include <esp_now.h>
#define PRINT_MODE -1
#define HELLO 0
#define BLINK 1
#define ADDRESS_RCVD 2
#define NUM_ADDRESSES 8
#define ADDRESS_SIZE 6

struct message_address {
  uint8_t messageType = HELLO;
  uint8_t address[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
} ;

struct message_blink {
  uint8_t messageType = BLINK;
  uint8_t color[3];
  bool broadcast = false;
} ;

struct addresses {
  uint8_t address[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  bool registered;
} ;
int addressCounter = 0;

struct message_address_rcvd {
  uint8_t messageType = ADDRESS_RCVD;
  uint8_t address[6];
} ;

