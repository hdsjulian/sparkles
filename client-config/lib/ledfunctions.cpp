#include "ledfunctions.h"
#include "declarations.h"
#include "messaging.h"
#include "Arduino.h"
void hueToRGB(uint8_t hue, uint8_t brightness)
{
    uint16_t scaledHue = (hue * 6);
    uint8_t segment = scaledHue / 256; // segment 0 to 5 around the
                                            // color wheel
    uint16_t segmentOffset =
      scaledHue - (segment * 256); // position within the segment

    uint8_t complement = 0;
    uint16_t prev = (brightness * ( 255 -  segmentOffset)) / 256;
    uint16_t next = (brightness *  segmentOffset) / 256;

    if(invert)
    {
      brightness = 255 - brightness;
      complement = 255;
      prev = 255 - prev;
      next = 255 - next;
    }

    switch(segment ) {
    case 0:      // red
        R = brightness;
        G = next;
        B = complement;
    break;
    case 1:     // yellow
        R = prev;
        G = brightness;
        B = complement;
    break;
    case 2:     // green
        R = complement;
        G = brightness;
        B = next;
    break;
    case 3:    // cyan
        R = complement;
        G = prev;
        B = brightness;
    break;
    case 4:    // blue
        R = next;
        G = complement;
        B = brightness;
    break;
   case 5:      // magenta
    default:
        R = brightness;
        G = complement;
        B = prev;
    break;
    }
}


void blink(message_blink receiveBlinkMessage, bool isMaster = false) {
  int blinkOperator = 1;
  int sendCount = 1;
  Serial.print("Number of registered Addresses ");
  Serial.println(addressCounter);
 for (int i = 0; i < steps*2; i++) {
    if (i > steps) {
      blinkOperator = -1;
    }
    redfloat += blinkOperator*(float)receiveBlinkMessage.color[0]/steps;
    greenfloat += blinkOperator*(float)receiveBlinkMessage.color[1]/steps;
    bluefloat += blinkOperator*(float)receiveBlinkMessage.color[2]/steps;
    ledcWrite(ledChannelRed2, (int)floor(redfloat));
    ledcWrite(ledChannelGreen2, (int)floor(greenfloat));
    ledcWrite(ledChannelBlue2, (int)floor(bluefloat));
    ledcWrite(ledChannelRed1, (int)floor(redfloat));
    ledcWrite(ledChannelGreen1, (int)floor(greenfloat));
    ledcWrite(ledChannelBlue1, (int)floor(bluefloat));
    // schickt einfach keinen zweiten Blink. 
    if ((i > 0) and (i % (2*steps/(addressCounter+1))*sendCount) == 0 and (isMaster = true)) {
      Serial.print("Sending blink to ");
      printAddress(addressList[sendCount-1].address);
      Serial.print(" counter ") ;
      Serial.println(i);
      blinkMessage.color[0] = random(128);
      blinkMessage.color[1] = random(128);
      blinkMessage.color[2] = random(128);
      esp_now_send(addressList[sendCount-1].address, (uint8_t *) &blinkMessage, sizeof(blinkMessage));
      sendCount++;
    }
    delay(stepdelay);
 }
     ledcWrite(ledChannelRed2, 0);
    ledcWrite(ledChannelGreen2, 0);
    ledcWrite(ledChannelBlue2, 0);
    ledcWrite(ledChannelRed1, 0);
    ledcWrite(ledChannelGreen1, 0);
    ledcWrite(ledChannelBlue1, 0);
}



void ledSetup() { 
    ledcSetup(ledChannelRed1, freq, resolution);
  ledcSetup(ledChannelGreen1, freq, resolution);
  ledcSetup(ledChannelBlue1, freq, resolution);
 ledcSetup(ledChannelRed2, freq, resolution);
  ledcSetup(ledChannelGreen2, freq, resolution);
  ledcSetup(ledChannelBlue2, freq, resolution);
  ledcAttachPin(ledPinRed, ledChannelRed1);
  ledcAttachPin(ledPinGreen, ledChannelGreen1);
  ledcAttachPin(ledPinBlue, ledChannelBlue1);
  ledcAttachPin(ledPinRed2, ledChannelRed2);
  ledcAttachPin(ledPinBlue2, ledChannelBlue2);
  ledcAttachPin(ledPinGreen2, ledChannelGreen2);
  ledcWrite(ledChannelRed1, 0);
  ledcWrite(ledChannelBlue1, 0);
  ledcWrite(ledChannelGreen1, 0);
  
  ledcWrite(ledChannelRed2, 0);
  ledcWrite(ledChannelBlue2, 0);
  ledcWrite(ledChannelGreen2, 0);
}

void blinkquick(int red, int green, int blue, int blinkDelay) {
    ledcWrite(ledChannelRed2, red);
  ledcWrite(ledChannelGreen2, green);
  ledcWrite(ledChannelBlue2, blue);
  ledcWrite(ledChannelRed1, red);
  ledcWrite(ledChannelGreen1, green);
  ledcWrite(ledChannelBlue1, blue);   
  delay(blinkDelay);
    ledcWrite(ledChannelRed2, 0);
    ledcWrite(ledChannelGreen2, 0);
  ledcWrite(ledChannelBlue2, 0);
  ledcWrite(ledChannelRed1, 0);
  ledcWrite(ledChannelGreen1, 0);
  ledcWrite(ledChannelBlue1, 0);   
}