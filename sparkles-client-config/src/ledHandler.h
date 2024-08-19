#include "myDefines.h"
#ifndef LED_HANDLER_H
#define LED_HANDLER_H
//#include <stateMachine.h>

//#include <../../sparkles-main-config/src/messaging.h>
#include <queue>

#define FRACTION 5

class ledHandler {
    private: 
        float rgb[3];
        float oldrgb[3];
        float steps[6] = {0, 0.33, 0.44, 0.55, 0.66, 1.0};
        int currentStep = 0;
        float redfloat = 0, greenfloat = 0, bluefloat = 0;
        float redsteps, greensteps, bluesteps;
        int minRed = 180;
        int maxRed = 255;
        int minGreen = 60;
        int maxGreen = 120;
        int minBlue = 0;
        int maxBlue = 30;
        int maxSpeed = 3000;
        int minSpeed = 1000;
        int maxPause = 200;
        int minPause = 0;
        int maxSpread = 1000;
        int minSpread = 300;
        int maxReps = 10;
        int minReps = 5;
        int maxAniReps = 20;
        int minAniReps = 5;
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
        int maxX = 0;
        int maxY = 0;        
        bool once = false;
        int fraction = 0;
        int globalBrightness = 150;

    public:
    ledHandler();
    void setup();
    void setupAnimation(message_animate *animationSetupMessage);
    void run();
    float fract(float x);
    float mix(float a, float b, float t);
    float step(float e, float x);
    float clamp(float value);
    float* hsv2rgb(float h, float s, float b, float* rgb);
    void ledsOff();
    void flash(int r = 255, int g = 0, int b = 0, int duration = 50, int reps = 2, int pause = 50);
    void blink();
    void candle(int duration, int reps, int pause, unsigned long startTime, unsigned long timeOffset);
    void syncAsyncBlink();
    void slowStartup();
    void rowBlink();
    void syncEnd();
    void setupSyncAsyncBlink();
    void setupRowBlink();
    void setupSlowStartup();
    void setupSyncEnd();

    void setTimeOffset(unsigned long setOffset, int offsetMultiplier);

    void ledOn(int r, int g, int b, int duration, int frontback);
    void concentric();
    void setDistance(float dist);
    void writeLeds();
    float calculateFlash(int targetVal, unsigned long timeElapsed, int speedfactor = 1);
    void calculateCandle(int targetVal, unsigned long timeElapsed, int speedfactor = 1);

    void setPosition(int position);
    void setLocation(int xpos, int ypos, int zpos);
    void printStatus();
    
    unsigned long calculate(message_animate *animationMessage);
    unsigned long calculateSyncAsyncBlink(message_animate *animationMessage);
    unsigned long calculateSlowStartup(message_animate *animationMessage);
    unsigned long calculateRowBlink(message_animate *animationMessage);
    unsigned long calculateSyncEnd(message_animate *animationMessage);
    void getNextAnimation(message_animate *animationMessage);
    void createSyncAsyncBlink(message_animate *animationMessage);
    void createSlowStartup(message_animate* animationmessage);
    void createRowBlink(message_animate *animationMessage);
    void createSyncEnd(message_animate *animationMessage);
    void printAnimationMessage(const message_animate &animationMessage);
    void startFlashTask();
    void flashTask(void *parameters);
    void turnOff();
    void setGlobalBrightness(int brightness);
    void setSyncAsyncParams(int minS, int maxS, int minP, int maxP, int minSp, int maxSp, int minR, int maxR, int minAR, int maxAR, int minRGBR, int maxRGBR, int minRGBG, int maxRGBG, int minRGBB, int maxRGBB);
    String getParamsJson();
};

#endif