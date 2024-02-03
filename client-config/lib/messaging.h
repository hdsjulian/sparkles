#include <Arduino.h>
#include <esp_now.h>

//#include <../lib/declarations.h>

#ifndef MESSAGING_H
#define MESSAGING_H
void printAddress(uint8_t address[6]);
void sendAddress();
void printMessageTypes(int messageType);
void registerAddress(uint8_t address[6]);
void receiveAddress(uint8_t address[6]) ;
#endif
