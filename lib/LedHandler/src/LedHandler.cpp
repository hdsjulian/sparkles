// Candle light effect: flickers around a HSV value, fades in and out

#include "LedHandler.h"

LedHandler::LedHandler()
{
    configMutex = xSemaphoreCreateMutex();
    if (configMutex == NULL) {
        ESP_LOGI("ERROR", "Failed to create configMutex");
    }
    ledQueue = xQueueCreate(512, sizeof(message_animation)); // Ensure the queue is created
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
    xTaskCreatePinnedToCore(ledTaskWrapper, "ledTask", 10000, this, 2, NULL, 1);
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
    //ESP_LOGI("LED", "Starting blink task");
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

void LedHandler::runBackgroundShimmerWrapper(void *pvParameters) {
    LedHandler *LedHandlerInstance = (LedHandler *)pvParameters;
    LedHandlerInstance->runBackgroundShimmer();
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
                memcpy(&animation, &animationData, sizeof(message_animation));
                int timeSpent = micros()-animationData.timeStamp;
                handleQueue(animationData, getCurrentPosition());
                if (animation.animationType == STROBE) {
                    //ESP_LOGI("LED", "Strobe. Hue: %d, Saturation: %d, Brightness: %d", animation.animationParams.strobe.hue, animation.animationParams.strobe.saturation, animation.animationParams.strobe.brightness);
                }
                else if (animation.animationType == BLINK) {
                    ESP_LOGI("LED", "sBlink. Hue: %d, Saturation: %d, Brightness: %d", animation.animationParams.blink.hue, animation.animationParams.blink.saturation, animation.animationParams.blink.brightness);
                }
            }
            continue;
        }
        else {
            if (xQueueReceive(ledQueue, &animationData, 0) == pdTRUE) 
            {   
                if (animationData.animationType == STROBE) {
                    ESP_LOGI("LED", "Strobe. Hue: %d, Saturation: %d, Brightness: %d", animation.animationParams.strobe.hue, animation.animationParams.strobe.saturation, animation.animationParams.strobe.brightness);
                }
                else if (animationData.animationType == BLINK) {
                    ESP_LOGI("LED", "bBlink. Hue: %d, Saturation: %d, Brightness: %d", animation.animationParams.blink.hue, animation.animationParams.blink.saturation, animation.animationParams.blink.brightness);
                }
                ESP_LOGI("LED", "received new animation %d current animation is %d", animationData.animationType, getCurrentAnimation() );
                //normal animation
                if (getCurrentAnimation() != MIDI && getCurrentAnimation() != BACKGROUND_SHIMMER) {
                    //delete old animation task if it exists
                    if (animationTaskHandle != NULL) {
                        vTaskDelete(animationTaskHandle);
                        animationTaskHandle = NULL;
                    }
                    //delete old midi task if it exists
                    if (midiTaskHandle != NULL) {
                        vTaskDelete(midiTaskHandle);
                        midiTaskHandle = NULL;
                    }
                    
                }
                //midi animation incoming and current animation is normal animatino
                if (animationData.animationType == MIDI && getCurrentAnimation() != MIDI && animationData.animationParams.midi.instrument != INSTRUMENT_CC) {
                    //delete old animation task if it exists
                    bool isInOctave = !(animation.animationParams.midi.note % OCTAVE != (getMidiNoteFromPosition(position)+animation.animationParams.midi.offset) % OCTAVE);

                    if (animationTaskHandle != NULL && isInOctave) {
                        vTaskDelete(animationTaskHandle);
                        animationTaskHandle = NULL;
                    }
                }
                //background shimmer animation incoming and current animation is normal animation
                if (animationData.animationType == BACKGROUND_SHIMMER && getCurrentAnimation() != MIDI && getCurrentAnimation() != BACKGROUND_SHIMMER) {
                    if (animationTaskHandle != NULL) {
                        vTaskDelete(animationTaskHandle);
                        animationTaskHandle = NULL;
                    }
                }
                handleQueue(animationData, getCurrentPosition());                
                if (animationTaskHandle != NULL) {
                }
                else {
                }
            }
        }    
         if (getCurrentAnimation() == MIDI){   
            if (midiTaskHandle == NULL || eTaskGetState(midiTaskHandle) == eDeleted) {
                xTaskCreatePinnedToCore(runMidiWrapper, "runMidi", 10000, this, 2, &midiTaskHandle, 1); // Create the runMidi task
            }

        }           
        else if (getCurrentAnimation() == STROBE)
            {   
                if (animationTaskHandle == NULL || eTaskGetState(animationTaskHandle) == eDeleted) {
                    xTaskCreatePinnedToCore(runStrobeWrapper, "runStrobe", 10000, this, 2, &animationTaskHandle, 1);
                }
            }
        else if (getCurrentAnimation() == BLINK)
            {   
                if (animationTaskHandle == NULL || eTaskGetState(animationTaskHandle) == eDeleted) {
                    xTaskCreatePinnedToCore(runBlinkWrapper, "runBlink", 10000, this, 2, &animationTaskHandle, 1);
                }
            }
        else if (getCurrentAnimation() == BACKGROUND_SHIMMER) {
            if (animationTaskHandle == NULL || eTaskGetState(animationTaskHandle) == eDeleted) {
                xTaskCreatePinnedToCore(runBackgroundShimmerWrapper, "runBackgroundShimmer", 10000, this, 2, &animationTaskHandle, 1);
            }
        }
        else if (getCurrentAnimation() == SYNC_ASYNC_BLINK) {
            if (animationTaskHandle == NULL || eTaskGetState(animationTaskHandle) == eDeleted) {
                xTaskCreatePinnedToCore(runSyncAsyncBlinkWrapper, "runSyncAsyncBlink", 10000, this, 2, &animationTaskHandle, 1);
            }
        }
        else {
            ledsOff();
            setCurrentAnimation(OFF);
        }
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}


void LedHandler::handleQueue(message_animation& animationData, int currentPosition) {
    if (animationData.animationType == MIDI) {
        if (animationData.animationParams.midi.instrument == INSTRUMENT_CC) {
            if (animationData.animationParams.midi.note != 64) {
                return;
            }
            else if (animationData.animationParams.midi.velocity == 127) {
                setSustain(true);
            }
            else if (animationData.animationParams.midi.velocity == 0) {
                setSustain(false);
                // Clear all midi note tables when sustain is released
                if (xSemaphoreTake(midiNoteTableMutex, portMAX_DELAY) == pdTRUE) {
                    for (int i = 0; i < OCTAVESONKEYBOARD; i++) {
                        midiNoteTableArray[i].velocity = 0;
                        midiNoteTableArray[i].note = 0;
                        midiNoteTableArray[i].startTime = 0;
                        micNoteTableArray[i].velocity = 0;
                        micNoteTableArray[i].note = 0;
                        micNoteTableArray[i].startTime = 0;
                    }
                    xSemaphoreGive(midiNoteTableMutex);
                }
            }
        }
        if (animationData.animationParams.midi.instrument == INSTRUMENT_MIC) {
            addToMidiTable(micNoteTableArray, animationData, currentPosition);
        }
        else if (animationData.animationParams.midi.instrument == INSTRUMENT_KEYBOARD) {
            
            addToMidiTable(midiNoteTableArray, animationData, currentPosition);
        }
    
    }
    else if (animationData.animationType == OFF) {
        ledsOff();
        setCurrentAnimation(OFF);
    }
    else if (animationData.animationType == BACKGROUND_SHIMMER) {
        // If a MIDI animation is running, ignore incoming background shimmer
        if (getCurrentAnimation() == MIDI) {
            return;
        }
        float hue = animationData.animationParams.backgroundShimmer.hue;
        float saturation = animationData.animationParams.backgroundShimmer.saturation;
        float value = animationData.animationParams.backgroundShimmer.value;   
    }
    setAnimation(animationData);
    setCurrentAnimation(animationData.animationType);

}

void LedHandler::runBlink() {
    message_animation animation = getAnimation();
    float hue = animation.animationParams.blink.hue;
    float saturation = animation.animationParams.blink.saturation;
    float value = animation.animationParams.blink.brightness;
    CRGB color = CHSV(hue, saturation, value);

    unsigned long long masterStartTime = animation.animationParams.blink.startTime;

    setMicrosUntilStart(masterStartTime);
    if (microsUntilStart < 0) {
        setMicrosUntilStart(100);
    }
    vTaskDelay(10/portTICK_PERIOD_MS);

    if (getMicrosUntilStart() > 0) {
        TickType_t ticksUntilStart = microsToTicks((unsigned long long)getMicrosUntilStart());
        TickType_t currentTicks = xTaskGetTickCount();
        vTaskDelayUntil(&currentTicks, ticksUntilStart);
    }

    for (int i = 0; i < animation.animationParams.blink.repetitions; i++) {
        writeLeds(color);
        ESP_LOGI("LED", "written leds");
        vTaskDelay(animation.animationParams.blink.duration);
        ledsOff();
        ESP_LOGI("LED", "LEDS OFF");
        vTaskDelay(animation.animationParams.blink.duration);
        ESP_LOGI("LED", "Blink %d/%d", i + 1, animation.animationParams.blink.repetitions);
    }
    ESP_LOGI("LED", "Blink Task Ended");
    setCurrentAnimation(OFF);
    animationTaskHandle = NULL;
    vTaskDelete(NULL);
}

void LedHandler::runStrobe() {
    message_animation animation = getAnimation();
    unsigned long long masterStartTime = animation.animationParams.strobe.startTime;
    setMicrosUntilStart(masterStartTime);
    if (getMicrosUntilStart() > 0) {
        TickType_t ticksUntilStart = microsToTicks((unsigned long long)getMicrosUntilStart());
        TickType_t currentTicks = xTaskGetTickCount();
        ledsOff();
        vTaskDelayUntil(&currentTicks, ticksUntilStart);
    }
    unsigned long long strobeDurationMicros = animation.animationParams.strobe.duration * 1000ULL;
    unsigned long long strobeEndTime = micros() + strobeDurationMicros;

    float rgb[3];
    while (micros() < strobeEndTime) {
        CRGB color = CHSV(animation.animationParams.strobe.hue, animation.animationParams.strobe.saturation, animation.animationParams.strobe.brightness);
        writeLeds(color);
        vTaskDelay(1000 / animation.animationParams.strobe.frequency);
        ledsOff();
        vTaskDelay(1000 / animation.animationParams.strobe.frequency);
    }
    ledsOff();
    setCurrentAnimation(OFF);
    animationTaskHandle = NULL;
    vTaskDelete(NULL); 
}

void LedHandler::runSyncAsyncBlink() {
    message_animation animation = getAnimation();
    float hue = animation.animationParams.syncAsyncBlink.hue;
    float saturation = animation.animationParams.syncAsyncBlink.saturation;
    float value = animation.animationParams.syncAsyncBlink.brightness;
    int repetitions = animation.animationParams.syncAsyncBlink.repetitions;
    int animationReps = animation.animationParams.syncAsyncBlink.animationReps;
    int spreadTime = animation.animationParams.syncAsyncBlink.spreadTime;
    int blinkDuration = animation.animationParams.syncAsyncBlink.blinkDuration;
    int pause = animation.animationParams.syncAsyncBlink.pause;
    uint8_t baseFraction = animation.animationParams.syncAsyncBlink.fraction;
    if (baseFraction < 1) baseFraction = 1; // Prevent division/modulo by zero
    unsigned long long masterStartTime = animation.animationParams.syncAsyncBlink.startTime;
    setMicrosUntilStart(masterStartTime);
    vTaskDelay(1000);
    // Calculate fraction based on lamp position and modulo of baseFraction
    int lampPosition = getCurrentPosition();
    uint8_t fraction = (lampPosition % baseFraction) + 1; // Ensure nonzero fraction
    if (getMicrosUntilStart() > 0) {
        TickType_t ticksUntilStart = microsToTicks((unsigned long long)microsUntilStart);
        TickType_t currentTicks = xTaskGetTickCount();
        ledsOff();
        vTaskDelayUntil(&currentTicks, ticksUntilStart);
    }

    float rgb[3];
    // Ensure repetitions is at least 2
    if (repetitions < 2) { repetitions = 2; }
    int divisor = repetitions / 2;
    if (divisor < 1) { divisor = 1; }
    if (animationReps < 1) {
        setCurrentAnimation(OFF);
        animationTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }
    for (int i = 0; i < animationReps; i++) {
        for (int j = 0; j < repetitions; j++) {
            if (i == 0) {
                // First repetition: all lamps blink in sync, no delay for any j
                float brightness = value;
                candleLight(blinkDuration, hue, saturation, brightness);
                ledsOff();
                vTaskDelay(0);
            } else {
                // Subsequent repetitions: wave effect
                if (j == 0) {
                    float brightness = value;
                    candleLight(blinkDuration, hue, saturation, brightness);
                    ledsOff();
                    vTaskDelay(0);
                } else if (j <= divisor) {
                    float brightness = value * (1 - (float)j / divisor);
                    candleLight(blinkDuration, hue, saturation, brightness);
                    ledsOff();
                    vTaskDelay(spreadTime / divisor * fraction * j);
                } else {
                    float brightness = value * (1 - (float)(repetitions - j) / divisor);
                    candleLight(blinkDuration, hue, saturation, brightness);
                    ledsOff();
                    vTaskDelay(spreadTime / divisor * fraction * (repetitions - j));
                }
            }
        }
        vTaskDelay(pause);
    }
}


// Calculates the maximum run time (in ms) for a sync async blink animation
unsigned long long LedHandler::calculateSyncAsyncBlink(message_animation& animationData) {
    const auto& params = animationData.animationParams.syncAsyncBlink;
    int repetitions = params.repetitions;
    int animationReps = params.animationReps;
    int spreadTime = params.spreadTime;
    int blinkDuration = params.blinkDuration;
    int pause = params.pause;
    uint8_t baseFraction = params.fraction;
    if (baseFraction < 1) baseFraction = 1;
    uint8_t fraction = baseFraction; // Use the base fraction for overall runtime
    unsigned long long totalTime = 0;
    int divisor = repetitions / 2;
    if (repetitions < 2) repetitions = 2;
    if (divisor < 1) divisor = 1;
    if (animationReps < 1) return 0;
    for (int i = 0; i < animationReps; i++) {
        for (int j = 0; j < repetitions; j++) {
            totalTime += blinkDuration;
            if (i == 0) {
                // First repetition: all lamps blink in sync, no delay
                totalTime += 0;
            } else {
                // Subsequent repetitions: wave effect
                if (j == 0) {
                    totalTime += 0;
                } else if (j <= divisor) {
                    totalTime += spreadTime / divisor * fraction * j;
                } else {
                    totalTime += spreadTime / divisor * fraction * (repetitions - j);
                }
            }
        }
        totalTime += pause;
    }
    totalTime = totalTime * 1000;
    if (totalTime > 60000000) { 
        totalTime = 60000000;
    }
    return totalTime;
}

unsigned long long LedHandler::calculateBlinkTime(message_animation& animationData) {
    const auto& params = animationData.animationParams.blink;
    unsigned long long microsUntilStart = params.startTime - micros();
    return (microsUntilStart + params.duration * 2 * params.repetitions);
}

unsigned long long LedHandler::calculateStrobeTime(message_animation& animationData) {
    const auto& params = animationData.animationParams.strobe;
    unsigned long long microsUntilStart = 0;
    if (micros() < params.startTime) {
        microsUntilStart = params.startTime - micros();
    }
    // strobeDurationMicros matches runStrobe logic
    unsigned long long strobeDurationMicros = params.duration * 1000ULL;
    return microsUntilStart + strobeDurationMicros;
}


void LedHandler::runBackgroundShimmer() {
    
    // Run the shimmer effect
    while (getCurrentAnimation() == BACKGROUND_SHIMMER) {
        message_animation animation = getAnimation();
        float hue = animation.animationParams.backgroundShimmer.hue;
        float saturation = animation.animationParams.backgroundShimmer.saturation;
        float value = animation.animationParams.backgroundShimmer.value;

        CRGB color = CHSV(hue, saturation, value);
        writeLeds(color);
        vTaskDelay(1000 / FPS);
        if ( value == 0) {
            // If the color is black, turn off the LEDs
            ledsOff();
            setCurrentAnimation(OFF);
        }

    }
    
    ledsOff();
    setCurrentAnimation(OFF);
    animationTaskHandle = NULL;
    vTaskDelete(NULL);
}



void LedHandler::pushToAnimationQueue(message_animation& animation)
{   
    if (xQueueSend(ledQueue, &animation, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE("LED", "Failed to send animation to queue");
    }
    else {
    }
}

void placeholder(int octave, int octaveDistance, float distanceFactor, float currentBrightness, float midiDecayFactor, float note) {
    ESP_LOGI("PLACEHOLDER", "octave: %d, octaveDistance: %d, distanceFactor: %.2f, currentBrightness: %.2f, midiDecayFactor: %.2f, note: %.2f", octave, octaveDistance, distanceFactor, currentBrightness, midiDecayFactor, note);
    return;
}


void LedHandler::runMidi()
{   
    float brightnessMidi = 0.0;
    float huemodMidi = 0;
    float satmodMidi = 0;
    float brightnessMic = 0.0;
    float huemodMic = 0;
    float satmodMic = 0;
    midiNoteTable localMidiNoteTableArray[OCTAVESONKEYBOARD];
    midiNoteTable localMicNoteTableArray[OCTAVESONKEYBOARD];
    while (true) {
        int numDevices = getNumDevices();
        if (numDevices <= 0) {
            ESP_LOGE("LED", "No devices found! Aborting runMidi to prevent division by zero.");
            ledsOff();
            setCurrentAnimation(OFF);
            vTaskDelete(NULL);
        }
        getMidiNoteTableArray(localMidiNoteTableArray, sizeof(localMidiNoteTableArray), INSTRUMENT_KEYBOARD);
        getMidiNoteTableArray(localMicNoteTableArray, sizeof(localMicNoteTableArray), INSTRUMENT_MIC);
        bool brightnessZeroMidi = true;
        bool brightnessZeroMic = true;
        brightnessMidi = 0.0;
        brightnessMic = 0.0;
        // Evaluate MIDI table
        for (int i = 0; i < OCTAVESONKEYBOARD; i++) {
            if (localMidiNoteTableArray[i].velocity == 0) {
                continue;
            }
            float midiDecayFactor = 0.0f;
            if (!getSustain()) {
                midiDecayFactor = calculateMidiDecay(localMidiNoteTableArray[i].startTime, localMidiNoteTableArray[i].velocity, localMidiNoteTableArray[i].note);
                if (midiDecayFactor == 0.0) {
                    continue;
                }
                if (midiDecayFactor >= 1.0) {
                    localMidiNoteTableArray[i].velocity = 0;
                    localMidiNoteTableArray[i].note = 0;
                    localMidiNoteTableArray[i].startTime = 0;
                    continue;
                }
            }
            int note = localMidiNoteTableArray[i].note;
            int velocity = localMidiNoteTableArray[i].velocity;
            int octave = (note / OCTAVE) - 1;
            int ledOctave = getOctaveFromPosition(position);
            int octaveDistance = abs((octave % numDevices) - ledOctave);
            float distanceFactor = 0.2 * octaveDistance;
            float currentBrightness;
            if (getSustain()) {
                currentBrightness = (int)(velocity * (1 - distanceFactor));
            } else {
                currentBrightness = (int)(velocity * (1 - midiDecayFactor) * (1 - distanceFactor));
            }
            if (currentBrightness > brightnessMidi) {
                brightnessMidi = currentBrightness;
                huemodMidi = -0.02 * octaveDistance;
                satmodMidi = 0.02 * octaveDistance;
            }
            brightnessZeroMidi = false;
        }
        // Evaluate MIC table
        /*for (int i = 0; i < OCTAVESONKEYBOARD; i++) {
            if (localMicNoteTableArray[i].velocity == 0) continue;
            ESP_LOGI("LED", "Evaluating MIC Note: %d, Velocity: %d, Start Time: %llu", 
                localMicNoteTableArray[i].note, 
                localMicNoteTableArray[i].velocity, 
                localMicNoteTableArray[i].startTime);
            ESP_LOGI("LED", "Index: %d", i);

            float micDecayFactor = calculateMidiDecay(localMicNoteTableArray[i].startTime, localMicNoteTableArray[i].velocity, localMicNoteTableArray[i].note);
            if (micDecayFactor == 0.0) continue;
            if (micDecayFactor >= 1.0) {
                localMicNoteTableArray[i].velocity = 0;
                localMicNoteTableArray[i].note = 0;
                localMicNoteTableArray[i].startTime = 0;
            } else {
                int note = localMicNoteTableArray[i].note;
                int velocity = localMicNoteTableArray[i].velocity;
                int octave = (note / OCTAVE) - 1;
                int ledOctave = getOctaveFromPosition(position);
                int octaveDistance = abs((octave % numDevices) - ledOctave);
                float distanceFactor = 0.2 * octaveDistance;
                float currentBrightness = (int)(velocity * (1-micDecayFactor) * (1 - distanceFactor));
                if (currentBrightness > brightnessMic) {
                    brightnessMic = currentBrightness;
                    huemodMic = -0.02 * octaveDistance;
                    satmodMic = 0.02 * octaveDistance;
                }
                brightnessZeroMic = false;
            }
        }*/
        // Choose the brighter source
        float finalBrightness = 0.0;
        float finalHueMod = 0.0;
        float finalSatMod = 0.0;
        if ((brightnessMidi == 0 && brightnessMic == 0) || (brightnessZeroMidi && brightnessZeroMic)) {
            ledsOff();
            setCurrentAnimation(OFF);
            midiTaskHandle = NULL;
            vTaskDelete(NULL);
        } else {
            if (brightnessMidi >= brightnessMic) {
                finalBrightness = brightnessMidi;
                finalHueMod = huemodMidi;
                finalSatMod = satmodMidi;
            } else {
                finalBrightness = brightnessMic;
                finalHueMod = huemodMic;
                finalSatMod = satmodMic;
            }
            CRGB color = CHSV(max(midiHue + finalHueMod, 0.0f) * 255, (midiSat + finalSatMod) * 255, finalBrightness*2);
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


void LedHandler::resetLedTask() {
    // Clear the animation queue
    xQueueReset(ledQueue);

    // Turn off all LEDs
    ledsOff();

    // Reset animation/task handles
    if (animationTaskHandle != NULL) {
        vTaskDelete(animationTaskHandle);
        animationTaskHandle = NULL;
    }
    if (midiTaskHandle != NULL) {
        vTaskDelete(midiTaskHandle);
        midiTaskHandle = NULL;
    }

    // Set current animation to OFF
    setCurrentAnimation(OFF);
    startLedTask();
    ESP_LOGI("LED", "LED task and state reset");
}

void LedHandler::candleLight(unsigned long long duration, float hue, float saturation, float value) {
    // Clamp hue and saturation for colorful candle effect
    hue = std::min(std::max(hue, 20.0f), 50.0f);         // Warm yellow/orange
    saturation = std::max(saturation, 220.0f);           // Ensure deeper, vivid color
    ESP_LOGI("LED", "Candle light for %llu", duration);
    unsigned long long fadeTime = duration * 0.35;
    unsigned long long steadyTime = duration - 2 * fadeTime;
    int steps = 24; // Fewer steps for smoother fade
    // Helper for random float between 0 and 1
    auto randFloat = []() { return (float)rand() / (float)0x7fff; };
    // Even gentler flicker: amplitude extremely close to 1.0
    float flickerMin = 0.999995f;
    float flickerMax = 1.00001f;
    int flickerBaseDelay = 280; // ms, even slower flicker
    int flickerVarDelay = 4;    // ms, very consistent timing
    // Fade in
    for (int i = 0; i < steps; i++) {
        float fadeVal = value * ((float)i / steps);
        float flicker = fadeVal * (flickerMin + (flickerMax - flickerMin) * randFloat());
        CRGB color = CHSV(hue, saturation, flicker);
        writeLeds(color);
        vTaskDelay(fadeTime / steps);
    }
    // Steady flicker
    unsigned long long startSteady = millis();
    float lastFlicker = value;
    while (millis() - startSteady < steadyTime) {
        float targetFlicker = value * (flickerMin + (flickerMax - flickerMin) * randFloat());
        // Even heavier interpolation for extremely gentle flicker
        float flicker = 0.9998f * lastFlicker + 0.0002f * targetFlicker;
        lastFlicker = flicker;
        CRGB color = CHSV(hue, saturation, flicker);
        writeLeds(color);
        vTaskDelay(flickerBaseDelay + rand() % flickerVarDelay); // Even slower, more gentle interval
    }
    // Fade out
    for (int i = steps; i >= 0; i--) {
        float fadeVal = value * ((float)i / steps);
        float flicker = fadeVal * (flickerMin + (flickerMax - flickerMin) * randFloat());
        CRGB color = CHSV(hue, saturation, flicker);
        writeLeds(color);
        vTaskDelay(fadeTime / steps);
    }
    ledsOff();
}
