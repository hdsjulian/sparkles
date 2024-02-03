//#include "declarations.h"


#ifndef LEDFUNCTIONS_H
#define LEDFUNCTIONS_H
void hueToRGB(uint8_t hue, uint8_t brightness);
void blink(message_blink receiveBlinkMessage, bool isMaster = false);
void blinkquick(int red, int green, int blue, int blinkDelay);
void ledSetup();
#endif
