// #include "../../include/myDefines.h"
#include <MyDefines.h>
#ifndef LED_HANDLER_H
#define LED_HANDLER_H
#include <queue>
#include <FastLED.h>

#define FPS 60
class LedHandler
{
public:
    static LedHandler& getInstance() {
        static LedHandler instance; // Guaranteed to be destroyed and instantiated on first use
        return instance;
    }

    void setup();
    void startLedTask();
    
    void updatePosition();
    void updateTimerOffset();
    void addToMidiTable(midiNoteTable midiNoteTableArray[OCTAVESONKEYBOARD], message_animation animation, int position);
    void pushToAnimationQueue(message_animation& animation);
    static void runBlinkOld();
    void setTimerOffset(long long newOffset);
    long long getTimerOffset();
    void setMidiNoteTable(int index, midiNoteTable note);
    midiNoteTable getMidiNoteTable(int index);
    void getMidiNoteTableArray(midiNoteTable* buffer, size_t size, int instrument = INSTRUMENT_KEYBOARD);
    animationEnum getCurrentAnimation();
    void setCurrentAnimation(animationEnum animation);
    int getCurrentPosition();
    void setCurrentPosition(int position);
    void setLocation(float x, float y);
    float getXLocation();
    float getYLocation();
    void setAnimation(message_animation& animationData);
    void setSustain(bool sustain);
    bool getSustain();
    message_animation getAnimation();
    long long getMicrosUntilStart();
    void setMicrosUntilStart(unsigned long long masterStartTime);
    unsigned long long calculateMicrosUntilStart(unsigned long long masterStartTime);
    TickType_t getNextAnimationTicks();
    void setMicrosUntilEnd(message_animation& animationData);
    void resetMicrosUntilEnd();

    bool getBackgroundShimmerFadeout();
    void setBackgroundShimmerFadeout(bool fadeout);
    float getDistanceFromCenter();
    void setDistanceFromCenter(float distance);
    int getMaxDistanceFromCenter();
    void setMaxDistanceFromCenter(int distance);
    void setUseDistanceSwitch(bool use);
    bool getUseDistanceSwitch();
    void setMidiParams(message_midi_params& params);
    message_midi_params getMidiParams();

    unsigned long long calculateAnimation(message_animation& animationData);
    unsigned long long calculateSyncAsyncBlink(message_animation& animationData);
    unsigned long long calculateBlinkTime(message_animation& animationData);
    unsigned long long calculateStrobeTime(message_animation& animationData);
    void setNumDevices(int num);
    int getNumDevices();
    message_animation createAnimation(animationEnum animationType);
    message_animation createFlash(unsigned long long startTime, unsigned long long duration, int repetitions, int hue, int saturation, int brightness);
    message_animation createSyncAsyncBlinkRandom();
    message_animation createCandle(unsigned long long startTime, int duration, int hue, int saturation, int value);
    void blink(unsigned long long startTime, unsigned long long duration, int repetitions, int hue, int saturation, int brightness);
    void batteryBlink(float batteryPercentage);
    void stopAnimationTask();
    void turnOff();
    void candleLight(unsigned long long duration, float hue, float saturation, float value);
    void resetLedTask();
    bool isTimedAnimation(animationEnum type);
private:
    LedHandler();
    LedHandler(const LedHandler&) = delete;
    LedHandler& operator=(const LedHandler&) = delete;
    static void ledTaskWrapper(void *pvParameters);
    static void runMidiWrapper(void *pvParameters);
    static void runBlinkWrapper(void *pvParameters);
    static void runStrobeWrapper(void *pvParameters);
    static void runSyncAsyncBlinkWrapper(void *pvParameters);
    static void runBackgroundShimmerWrapper(void *pvParameters);
    static void runCandleWrapper(void *pvParameters);
    void ledTask();
    void runMidi();
    void runBlink();
    void runStrobe();
    void runSyncAsyncBlink();
    void runBackgroundShimmer();
    void runCandle();
    
    static void ledsOff();
    float midiHue = 25.0f / 360.0f;
    float midiSat = 0.4f;
    static void writeLeds(CRGB color);
    static float *hsv2rgb(float h, float s, float b, float *rgb);
    static float float_to_sRGB(float x);
    static float sRGB_to_float(float val);
    static float fract(float x);
    static float mix(float a, float b, float t);
    static float step(float edge, float x);
    static float intRGBToFloat(int val);
    static int getOctaveFromPosition(int position);
    static float calculateMidiDecay(unsigned long long startTime, int velocity, int note);
    static int getDecayTime(int midiNote, int velocity);
    static int getMidiNoteFromPosition(int position);
    TickType_t microsToTicks(unsigned long long micros);
    void handleQueue(message_animation& animationData, int currentPosition);
    int numDevices;
    long long timerOffset;
    int mode;
    int position;
    float xLocation;
    float yLocation;
    float distanceFromCenter;
    int maxDistanceFromCenter;
    long long microsUntilStart;
    long long microsUntilEnd = 0;
    bool sustain;
    int syncAsyncMinDuration = 500;
    int syncAsyncMaxDuration = 1500;
    int syncAsyncMinPause = 1000;
    int syncAsyncMaxPause = 3000;
    int syncAsyncMinReps = 10;
    int syncAsyncMaxReps = 20;
    int syncAsyncMinAniReps = 10;
    int syncAsyncMaxAniReps = 20;
    int syncAsyncMinSpread = 500;
    int syncAsyncMaxSpread = 2000;
    bool backgroundShimmerFadeout = false;
    bool useDistanceSwitch = false;
    SemaphoreHandle_t configMutex;
    QueueHandle_t ledQueue, backgroundShimmerQueue;
    message_animation animation;
    midiNoteTable midiNoteTableArray[OCTAVESONKEYBOARD];
    midiNoteTable micNoteTableArray[OCTAVESONKEYBOARD];
    SemaphoreHandle_t midiNoteTableMutex; 
    animationEnum currentAnimation = OFF;
    TaskHandle_t midiTaskHandle, animationTaskHandle;
    message_midi_params midiParams;
};

#endif