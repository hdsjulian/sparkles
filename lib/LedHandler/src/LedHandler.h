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
    void setTimerOffset(unsigned long long newOffset);
    unsigned long long getTimerOffset();
    void setMidiNoteTable(int index, midiNoteTable note);
    midiNoteTable getMidiNoteTable(int index);
    void getMidiNoteTableArray(midiNoteTable* buffer, size_t size);
    animationEnum getCurrentAnimation();
    void setCurrentAnimation(animationEnum animation);
    int getCurrentPosition();
    void setCurrentPosition(int position);
    void setAnimation(message_animation& animationData);
    message_animation getAnimation();
    unsigned long long calculateAnimation(message_animation& animationData);
    unsigned long long calculateSyncAsyncBlink(message_animation& animationData);
    void setNumDevices(int numDevices);
    int getNumDevices();


private:
    LedHandler();
    LedHandler(const LedHandler&) = delete;
    LedHandler& operator=(const LedHandler&) = delete;
    static void ledTaskWrapper(void *pvParameters);
    static void runMidiWrapper(void *pvParameters);
    static void runBlinkWrapper(void *pvParameters);
    static void runStrobeWrapper(void *pvParameters);
    static void runSyncAsyncBlinkWrapper(void *pvParameters);
    void ledTask();
    void runMidi();
    void runBlink();
    void runStrobe();
    void runSyncAsyncBlink();
    static void ledsOff();
    static constexpr float midiHue = 25.0f / 360.0f;
    static constexpr float midiSat = 0.84;
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
    void handleQueue(message_animation& animation, message_animation& animationData, int currentPosition);
    int numDevices;
    int timerOffset;
    int mode;
    int position;
    SemaphoreHandle_t configMutex;
    QueueHandle_t ledQueue;
    message_animation animation;
    midiNoteTable midiNoteTableArray[OCTAVESONKEYBOARD];
    SemaphoreHandle_t midiNoteTableMutex; 
    animationEnum currentAnimation;
    TaskHandle_t midiTaskHandle, animationTaskHandle;
};

#endif