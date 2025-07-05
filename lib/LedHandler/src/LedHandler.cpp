#include "LedHandler.h"

LedHandler::LedHandler()
{
    configMutex = xSemaphoreCreateMutex();
    if (configMutex == NULL) {
        ESP_LOGI("ERROR", "Failed to create configMutex");
    }
    ledQueue = xQueueCreate(10, sizeof(message_animation)); // Ensure the queue is created
    if (ledQueue == NULL) {
        Serial.println("Failed to create ledQueue");
    }
    midiNoteTableMutex = xSemaphoreCreateMutex();
    if (midiNoteTableMutex == NULL) {
        ESP_LOGI("ERROR", "Failed to create midiNoteTableMutex");
    }
}

void LedHandler::setup()
{

    ledcAttach(LEDPINRED1, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    
    ledcAttach(LEDPINBLUE1, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    
    ledcAttach(LEDPINGREEN1, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    ledcAttach(LEDPINRED2, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    ledcAttach(LEDPINGREEN2, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    //ledcAttach(LEDPINBLUE2, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
    
    ledsOff();
    ESP_LOGI("LED", "LED SETUP ENDED");
    startLedTask();
    ESP_LOGI("LED", "LED TASK STARTED");
}

void LedHandler::ledsOff() {
    ledcWrite(LEDPINRED1, 0);
    ledcWrite(LEDPINGREEN1, 0);
    ledcWrite(LEDPINBLUE1, 0);
    ledcWrite(LEDPINRED2, 0);
    ledcWrite(LEDPINGREEN2, 0);
    ledcWrite(LEDPINBLUE2, 0);
  
}

void LedHandler:: writeLeds(CRGB color) {
    //ESP_LOGI("LED", "writing leds %d, %d, %d ", (int)rgb[0],(int)rgb[1], (int)rgb[2]);
    ledcWrite(LEDPINRED1, color.r);
    ledcWrite(LEDPINGREEN1, color.g);
    ledcWrite(LEDPINBLUE1, color.b);
    ledcWrite(LEDPINRED2, color.r);
    ledcWrite(LEDPINGREEN2, color.g);
    ledcWrite(LEDPINBLUE2, color.b);

}

void LedHandler::startLedTask()
{
    ESP_LOGI("LED", "Starting ledTask");
    xTaskCreatePinnedToCore(ledTaskWrapper, "ledTask", 10000, this, 2, NULL, 0);
}

void LedHandler::ledTaskWrapper(void *pvParameters)
{
    ESP_LOGI("LED", "Starting ledTaskWraphab ich beper");

    LedHandler *LedHandlerInstance = (LedHandler *)pvParameters;
    ESP_LOGI("LED", "Starting ledTaskWrapper2");
    LedHandlerInstance->ledTask();
}
void LedHandler::runMidiWrapper(void *pvParameters) {
    LedHandler *LedHandlerInstance = (LedHandler *)pvParameters;
    LedHandlerInstance->runMidi();
}
void LedHandler::runBlinkWrapper(void *pvParameters) {
    LedHandler *LedHandlerInstance = (LedHandler *)pvParameters;
    ESP_LOGI("LED", "Starting blink task");
    LedHandlerInstance->runBlink();
}

void LedHandler::runStrobeWrapper(void *pvParameters) {
    LedHandler *LedHandlerInstance = (LedHandler *)pvParameters;
    LedHandlerInstance->runStrobe();
}

void LedHandler::runSyncAsyncBlinkWrapper(void *pvParameters) {
    LedHandler *LedHandlerInstance = (LedHandler *)pvParameters;
    LedHandlerInstance->runSyncAsyncBlink();
}

void LedHandler::ledTask()
{
    animationEnum animationType = OFF;
    unsigned long long animationStart = 0;
    message_animation animationData;
    while (true)
    {
        if (getCurrentAnimation() == OFF)
        {   
            ledsOff();
            if (xQueueReceive(ledQueue, &animationData, portMAX_DELAY) == pdTRUE) 
            {
                int timeSpent = micros()-animationData.timeStamp;
                handleQueue(animation, animationData, getCurrentPosition());
                if (animationData.animationType == STROBE) {
                    ESP_LOGI("LED", "Strobe. Hue: %d, Saturation: %d, Brightness: %d", animation.animationParams.strobe.hue, animation.animationParams.strobe.saturation, animation.animationParams.strobe.brightness);
                }
                else if (animationData.animationType == BLINK) {
                    ESP_LOGI("LED", "Blink. Hue: %d, Saturation: %.2f, Brightness: %.2f", animation.animationParams.blink.hue, animation.animationParams.blink.saturation, animation.animationParams.blink.brightness);
                }
            }
            continue;
        }
        else {
            if (xQueueReceive(ledQueue, &animationData, 0) == pdTRUE) 
            {   
                if (animation.animationType == STROBE) {
                    ESP_LOGI("LED", "Strobe. Hue: %d, Saturation: %d, Brightness: %d", animation.animationParams.strobe.hue, animation.animationParams.strobe.saturation, animation.animationParams.strobe.brightness);
                }
                else if (animation.animationType == BLINK) {
                    ESP_LOGI("LED", "Blink. Hue: %d, Saturation: %d, Brightness: %d", animation.animationParams.blink.hue, animation.animationParams.blink.saturation, animation.animationParams.blink.brightness);
                }
                if (getCurrentAnimation() != MIDI) {
                    if (animationTaskHandle != NULL) {
                        vTaskDelete(animationTaskHandle);
                        animationTaskHandle = NULL;
                    }
                    if (midiTaskHandle != NULL) {
                        vTaskDelete(midiTaskHandle);
                        midiTaskHandle = NULL;
                    }
                }
                if (animation.animationType == MIDI && getCurrentAnimation() != MIDI) {
                    if (midiTaskHandle != NULL) {
                        vTaskDelete(midiTaskHandle);
                        midiTaskHandle = NULL;
                    }
                }
                ESP_LOGI("LED", "Old Anim %d", getCurrentAnimation());
                handleQueue(animation, animationData, getCurrentPosition());
                ESP_LOGI("LED", "Received %d", getCurrentAnimation());
                if (animationTaskHandle != NULL) {
                    ESP_LOGI("LED", "Animation Task Handle is not null");
                }
                else {
                    ESP_LOGI("LED", "Animation Task Handle is Null");
                }
            }
        }    
         if (getCurrentAnimation() == MIDI){   
            if (midiTaskHandle == NULL || eTaskGetState(midiTaskHandle) == eDeleted) {
                xTaskCreate(runMidiWrapper, "runMidi", 10000, this, 1, &midiTaskHandle); // Create the runMidi task
            }

        }           
        else if (getCurrentAnimation() == STROBE)
            {   
                if (animationTaskHandle == NULL || eTaskGetState(animationTaskHandle) == eDeleted) {
                    xTaskCreate(runStrobeWrapper, "runStrobe", 10000, this, 1, &animationTaskHandle);
                }
            }
        else if (getCurrentAnimation() == BLINK)
            {   
                if (animationTaskHandle == NULL || eTaskGetState(animationTaskHandle) == eDeleted) {
                    xTaskCreate(runBlinkWrapper, "runBlink", 10000, this, 1, &animationTaskHandle);
                }
            }
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}


void LedHandler::handleQueue(message_animation& animation, message_animation& animationData, int currentPosition) {
    if (animationData.animationType == MIDI) {
        addToMidiTable(midiNoteTableArray, animationData, position);
    }
    else if (animationData.animationType == OFF) {
        ledsOff();
        setCurrentAnimation(OFF);
    }
    setAnimation(animationData);
    setCurrentAnimation(animationData.animationType);

}

void LedHandler::runStrobe() {
    unsigned long long microsUntilStart = animation.animationParams.strobe.startTime-getTimerOffset()-micros();
    microsUntilStart = getTimerOffset() > 0 ? animation.animationParams.strobe.startTime-getTimerOffset()-micros() :getTimerOffset()-animation.animationParams.strobe.startTime-micros();
    TickType_t ticksUntilStart = microsToTicks(microsUntilStart);
    TickType_t currentTicks = xTaskGetTickCount();
    unsigned long long endTimeMicros = animation.animationParams.strobe.duration*1000+microsUntilStart+micros();
    TickType_t endTimeTicks = microsToTicks(endTimeMicros);
    ESP_LOGI("LED", "Strobe Task Started: Hue: %d, Saturation: %d, Value %d", animation.animationParams.strobe.hue, animation.animationParams.strobe.saturation, animation.animationParams.strobe.brightness);
    ESP_LOGI("LED", "waiting until %llu", microsUntilStart);
    ESP_LOGI("LED", "Ticks until start: %d", ticksUntilStart);
      vTaskDelayUntil(&currentTicks, ticksUntilStart);
    ledsOff();
    float rgb[3];
    while (true) {
        CRGB color = CHSV(animation.animationParams.strobe.hue, animation.animationParams.strobe.saturation, animation.animationParams.strobe.brightness);
        writeLeds(color);
        vTaskDelay(1000/animation.animationParams.strobe.frequency);
        if (xTaskGetTickCount() >= endTimeTicks) {
            ledsOff();
            setCurrentAnimation(OFF);
            animationTaskHandle = NULL;
            vTaskDelete(NULL);
        }
        else {
            ledsOff();
            vTaskDelay(1000/animation.animationParams.strobe.frequency);
        }
    }
    
}

void LedHandler::runSyncAsyncBlink() {
    message_animation animation = getAnimation();
    float hue = animation.animationParams.syncAsyncBlink.hue;
    float saturation = animation.animationParams.syncAsyncBlink.saturation;
    float value = animation.animationParams.syncAsyncBlink.brightness;
    TickType_t currentTicks = xTaskGetTickCount();
    unsigned long long microsUntilStart = animation.animationParams.syncAsyncBlink.startTime-getTimerOffset()-micros();
    TickType_t ticksUntilStart = microsToTicks(microsUntilStart);
    int repetitions = animation.animationParams.syncAsyncBlink.repetitions;
    int animationReps = animation.animationParams.syncAsyncBlink.animationReps;
    int spreadTime = animation.animationParams.syncAsyncBlink.spreadTime;
    int blinkDuration = animation.animationParams.syncAsyncBlink.blinkDuration;
    int pause = animation.animationParams.syncAsyncBlink.pause;
    uint8_t fraction = animation.animationParams.syncAsyncBlink.fraction;
    ledsOff();
    vTaskDelayUntil(&currentTicks, ticksUntilStart);
    float rgb[3];
    for (int i = 0; i < animationReps; i++) {
        for (int j = 0; j < repetitions; j++) {
            if (j <= repetitions/2) {
                float brightness = value * (1 - (float)j/(repetitions/2));
                CRGB color = CHSV(hue, saturation, brightness);
                writeLeds(color);
                vTaskDelay(blinkDuration);
                ledsOff();
                vTaskDelay(spreadTime/(repetitions/2)*fraction*j);
            }
            else {
                float brightness = value * (1 - (float)(repetitions-j)/(repetitions/2));
                CRGB color = CHSV(hue, saturation, brightness);
                writeLeds(color);
                vTaskDelay(blinkDuration);
                ledsOff();
                vTaskDelay(spreadTime/(repetitions/2)*fraction*(repetitions-j));
            }
        }
        vTaskDelay(pause);
    }
}

void LedHandler::runBlink() {
    message_animation animation = getAnimation();
    float hue = animation.animationParams.blink.hue;
    float saturation = animation.animationParams.blink.saturation;
    float value = animation.animationParams.blink.brightness;
    CRGB color = CHSV(hue, saturation, value);
    ESP_LOGI("LED", "Red: %d, Green: %d, Blue: %d", color.r, color.g, color.b);
    ESP_LOGI("LED", "Blink Task Started: Hue: %d, Saturation: %d, Value %d", animation.animationParams.blink.hue, animation.animationParams.blink.saturation, animation.animationParams.blink.brightness);
    if (animation.animationParams.blink.startTime > micros()) {
        unsigned long long microsUntilStart = animation.animationParams.blink.startTime-micros();
        TickType_t ticksUntilStart = microsToTicks(microsUntilStart);
        TickType_t currentTicks = xTaskGetTickCount();
        vTaskDelayUntil(&currentTicks, ticksUntilStart);
    }
    float rgb[3];

    for (int i = 0; i < animation.animationParams.blink.repetitions; i++) {

        writeLeds(color);
        vTaskDelay(animation.animationParams.blink.duration);
        ledsOff();
        vTaskDelay(animation.animationParams.blink.duration);
    }
    setCurrentAnimation(OFF);
    animationTaskHandle = NULL;
    vTaskDelete(NULL);
    
}
void LedHandler::pushToAnimationQueue(message_animation& animation)
{   
    if (xQueueSend(ledQueue, &animation, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE("LED", "Failed to send animation to queue");
    }
}

void placeholder(int octave, int octaveDistance, float distanceFactor, float currentBrightness, float midiDecayFactor, float note) {
    ESP_LOGI("PLACEHOLDER", "octave: %d, octaveDistance: %d, distanceFactor: %.2f, currentBrightness: %.2f, midiDecayFactor: %.2f, note: %.2f", octave, octaveDistance, distanceFactor, currentBrightness, midiDecayFactor, note);
    return;
}


void LedHandler::runMidi()
{   
    float brightness = 0.0;
    float huemod = 0;
    float satmod = 0;
    midiNoteTable localMidiNoteTableArray[OCTAVESONKEYBOARD];
    while (true) {
        getMidiNoteTableArray(localMidiNoteTableArray, sizeof(localMidiNoteTableArray));
        bool brightnessZero = true;
        bool newBrightness = false;
        brightness = 0.0;
        for (int i = 0; i < OCTAVESONKEYBOARD; i++)
        {   
            if (localMidiNoteTableArray[i].velocity == 0)
            {
                continue;
            }
            float midiDecayFactor = calculateMidiDecay(localMidiNoteTableArray[i].startTime, localMidiNoteTableArray[i].velocity, localMidiNoteTableArray[i].note);
            if (midiDecayFactor == 0.0)
            {
                continue;
            }
            //ESP_LOGI("LED", "Decay factor: %f at %d", midiDecayFactor, micros());
            if (midiDecayFactor <= 1.0)
            {
                localMidiNoteTableArray[i].velocity = 0;
                localMidiNoteTableArray[i].note = 0;
                localMidiNoteTableArray[i].startTime = 0;
            }
            else
            {
                int note = localMidiNoteTableArray[i].note;
                int velocity = localMidiNoteTableArray[i].velocity;
                int octave = (note / OCTAVE) - 1;
                int ledOctave = getOctaveFromPosition(position);
                int octaveDistance = abs((octave % getNumDevices()) - ledOctave);
                float distanceFactor = 0.2 * octaveDistance;
                float currentBrightness = (int)(velocity * (1-midiDecayFactor) * (1 - distanceFactor));
                if (currentBrightness > brightness)
                {
                    brightness = currentBrightness;
                    huemod = -0.02 * octaveDistance;
                    satmod = 0.02 * octaveDistance;
                }
                brightnessZero = false;
                            
            }
        }
        if (brightness == 0 || brightnessZero == true)  
        {   
            ledsOff();
            setCurrentAnimation(OFF);
            vTaskDelete(NULL);
        }
        else {
            CRGB color = CHSV(max(midiHue + huemod, 0.0f) * 255, (midiSat + satmod) * 255, brightness*2);


            if (newBrightness == true)
            {   
            }
            writeLeds(color);
        }
        vTaskDelay((1000/FPS)/portTICK_PERIOD_MS);
    }
}


void LedHandler::runBlinkOld() {
    float targetRgb[3] = {50, 0, 0};
    float currentRgb[3] = {0, 0, 0};
    int steps = 100; // Number of steps for fading
    int delayTime = 10; // Delay time in milliseconds for each step

    // Increase brightness
    for (int i = 0; i <= steps; i++) {
        currentRgb[0] = (targetRgb[0] / steps) * i;
        currentRgb[1] = (targetRgb[1] / steps) * i;
        currentRgb[2] = (targetRgb[2] / steps) * i;
        //ESP_LOGI("LED", "Brightness: %.2f, rgb: %d, %d, %d", (float)i, (int)currentRgb[0], (int)currentRgb[1], (int)currentRgb[2]);
        CRGB color = CRGB(currentRgb[0], currentRgb[1], currentRgb[2]);
        writeLeds(color);
        vTaskDelay(pdMS_TO_TICKS(delayTime));
    }

    // Decrease brightness
    for (int i = steps; i >= 0; i--) {
        currentRgb[0] = (targetRgb[0] / steps) * i;
        currentRgb[1] = (targetRgb[1] / steps) * i;
        currentRgb[2] = (targetRgb[2] / steps) * i;
        CRGB color = CRGB(currentRgb[0], currentRgb[1], currentRgb[2]);
        writeLeds(color);
        vTaskDelay(pdMS_TO_TICKS(delayTime));
    }
}


