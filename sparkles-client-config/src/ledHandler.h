#ifndef LED_HANDLER_H
#define LED_HANDLER_H

#include "myDefines.h"

//#include <stateMachine.h>

//#include <../../sparkles-main-config/src/messaging.h>
#include <queue>



class ledHandler {
    private: 
        float rgb[3];
        float redfloat = 0, greenfloat = 0, bluefloat = 0;
        float redsteps, greensteps, bluesteps;
        concentric_animation concentricAnimation;
        float distance;
        animationEnum currentAnimation;      
        message_animate animationMessage;
        int animationRun = 0;
        int repeatCounter = 0;
        int animationRepeatCounter = 0;
        int animationNextStep = 0;
        unsigned long localAnimationStart = 0;
        unsigned long globalAnimationStart = 0;
        unsigned long localAnimationTimeframe = 0;
        unsigned long globalAnimationTimeframe = 0;
        unsigned long timeOffset = 0;
        int position;
        int runs = 0;
        int runs2 =0;
        int runs3 = 0;
        int xPos = 0;
        int yPos = 0;
        int zPos = 0;
        int offsetMultiplier;

        bool once = false;

    public:
    ledHandler();
    void setup();
    void setupAnimation(message_animate *animationSetupMessage);
    void run();
    float fract(float x);
    float mix(float a, float b, float t);
    float step(float e, float x);
    float* hsv2rgb(float h, float s, float b, float* rgb);
    void ledsOff();
    void flash(int r = 255, int g = 0, int b = 0, int duration = 50, int reps = 2, int pause = 50);
    void blink();
    void candle(int duration, int reps, int pause, unsigned long startTime, unsigned long timeOffset);
    void syncAsyncBlink();
    void rowBlink();
    void setupSyncAsyncBlink();
    void setupRowBlink();
    void setTimeOffset(unsigned long setOffset, int offsetMultiplier);

    void ledOn(int r, int g, int b, int duration, int frontback);
    void concentric();
    void setDistance(float dist);
    void writeLeds();
    float calculateFlash(int targetVal, unsigned long timeElapsed);
    void setPosition(int position);
    void setLocation(int xpos, int ypos, int zpos);
    void printStatus();
    void setupSyncBlink();
    void runSyncBlink();
    unsigned long calculate(message_animate *animationMessage);
    unsigned long calculateSyncAsyncBlink(message_animate *animationMessage);
    unsigned long calculateRowBlink(message_animate *animationMessage);
    void getNextAnimation(message_animate *animationMessage);
    void createSyncAsyncBlink(message_animate *animationMessage);
    void createRowBlink(message_animate *animationMessage);
    void printAnimationMessage(const message_animate &animationMessage);

};

#endif