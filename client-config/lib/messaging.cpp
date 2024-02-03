#include "messaging.h"
void printMessageTypes(int messageType) {
  switch (messageType) {
    case HELLO: 
      Serial.println ("Message Hello");
      break;
    case BLINK: 
      Serial.println("Blink");
      break;
    case ADDRESS_RCVD:
      Serial.println("Address received");
      break;
    default: 
      Serial.print("Unknown message type ");
      Serial.println(messageType);
  }
}

