#include <LedHandler.h>
void LedHandler::setTimerOffset(long long newOffset) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        timerOffset = newOffset;
        xSemaphoreGive(configMutex);
    }
}

// Thread-safe getter for offset
long long LedHandler::getTimerOffset() {
    long long currentOffset;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        currentOffset = timerOffset;
        xSemaphoreGive(configMutex);
    }
    return currentOffset;
}


animationEnum LedHandler::getCurrentAnimation() {
    animationEnum animation;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        animation = currentAnimation;
        xSemaphoreGive(configMutex);
    }
    else {
    }
    return animation;
}
void LedHandler::setCurrentAnimation(animationEnum animation) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        currentAnimation = animation;
        xSemaphoreGive(configMutex);
    }
}

int LedHandler::getCurrentPosition() {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        int currentPosition = position;
        xSemaphoreGive(configMutex);
        return currentPosition;
    }
    else {
        return -1;
    }
}
void LedHandler::setCurrentPosition(int newPosition) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        position = newPosition;
        xSemaphoreGive(configMutex);
    }
}

float LedHandler::getDistanceFromCenter() {
    float distance;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        distance = distanceFromCenter;
        xSemaphoreGive(configMutex);
    }
    return distance;
}
void LedHandler::setDistanceFromCenter(float distance) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        distanceFromCenter = distance;
        xSemaphoreGive(configMutex);
    }
}

void LedHandler::setLocation(float x, float y) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        xLocation = x;
        yLocation = y;
        xSemaphoreGive(configMutex);
    }
}
float LedHandler::getXLocation() {
    float xLoc;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        xLoc = xLocation;
        xSemaphoreGive(configMutex);
    }
    return xLoc;
}
float LedHandler::getYLocation() {
    float yLoc;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        yLoc = yLocation;
        xSemaphoreGive(configMutex);
    }
    return yLoc;
}
#if DEVICE_MODE == CLIENT
void LedHandler::setAnimation(message_animation& animationData) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        memcpy(&animation, &animationData, sizeof(animationData));
        xSemaphoreGive(configMutex);
    }
}
#elif DEVICE_MODE == MASTER
void LedHandler::setAnimation(message_animation& animationData) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
    unsigned long long now = esp_timer_get_time();
    switch (animationData.animationType) {
        case BACKGROUND_SHIMMER:
        case MIDI:
            microsUntilEnd = 0;
            break;
        case STROBE:
            microsUntilEnd = now + calculateStrobeTime(animationData);
            break;
        case BLINK:
            microsUntilEnd = now + calculateBlinkTime(animationData);
            break;
        case SYNC_ASYNC_BLINK:
            microsUntilEnd = now + calculateSyncAsyncBlink(animationData);
            break;
        default:
            microsUntilEnd = 0;
            break;
    }
    xSemaphoreGive(configMutex);
    }
}
#endif

message_animation LedHandler::getAnimation() {
    message_animation returnAnimation;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnAnimation = animation;
        xSemaphoreGive(configMutex);
    }
    return returnAnimation;
}

void LedHandler::setMicrosUntilStart(unsigned long long masterStartTime) {
    unsigned long long clientNow = esp_timer_get_time();
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        // offset = master - client, master time is always clientNow + offset
        microsUntilStart = masterStartTime - ((long long)clientNow + timerOffset);
        // see calculateMicrosUntilStart: a stale/bad offset must not turn into
        // an hours-long vTaskDelayUntil in runBlink()/runStrobe()
        if (microsUntilStart > 5000000LL) {
            ESP_LOGW("LED", "Absurd microsUntilStart %lld (offset %lld) — sync must be stale, starting now instead", microsUntilStart, timerOffset);
            microsUntilStart = 0;
        }
        xSemaphoreGive(configMutex);
    }
}

unsigned long long LedHandler::calculateMicrosUntilStart(unsigned long long masterStartTime) {
    unsigned long long clientNow = esp_timer_get_time();
    long long microsUntilStartCalc;
    microsUntilStartCalc = masterStartTime - ((long long)clientNow + timerOffset);
    // Clamp to zero if negative
    if (microsUntilStartCalc < 0) {
        microsUntilStartCalc = 0;
    }
    // Every scheduled animation starts well under 2s in the future (see
    // sendAnimation/runAnimationLoop) — this is called from ledTask()'s own
    // loop via a blocking vTaskDelayUntil, so a stale/bad timerOffset (e.g.
    // right after a resync) could otherwise freeze ALL animation dispatch
    // for as long as the bogus wait, making the client look completely dead
    // to every subsequent broadcast. Treat an absurd wait as a sync error.
    const long long MAX_SANE_WAIT_US = 5000000; // 5s
    if (microsUntilStartCalc > MAX_SANE_WAIT_US) {
        ESP_LOGW("LED", "Absurd microsUntilStart %lld (offset %lld) — sync must be stale, starting now instead", microsUntilStartCalc, timerOffset);
        microsUntilStartCalc = 0;
    }
    return (unsigned long long)microsUntilStartCalc;
}
long long LedHandler::getMicrosUntilStart() {
    long long returnMicros;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnMicros = microsUntilStart;
        xSemaphoreGive(configMutex);
    }
    return returnMicros;
}

void LedHandler::setNumDevices(int num) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        numDevices = num;
        xSemaphoreGive(configMutex);
    }
}
int LedHandler::getNumDevices() {
    int returnNumDevices;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnNumDevices = numDevices;
        xSemaphoreGive(configMutex);
    }
    return returnNumDevices;
}


void LedHandler::setSustain(bool sustain) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        this->sustain = sustain;
        xSemaphoreGive(configMutex);
    }
}
bool LedHandler::getSustain() {
    bool returnSustain;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnSustain = sustain;     
        xSemaphoreGive(configMutex);
    }
    return returnSustain;  
}

bool LedHandler::getBackgroundShimmerFadeout() {
    bool fadeout;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        fadeout = backgroundShimmerFadeout;
        xSemaphoreGive(configMutex);
    }
    return fadeout;
}
void LedHandler::setBackgroundShimmerFadeout(bool fadeout) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        backgroundShimmerFadeout = fadeout;
        xSemaphoreGive(configMutex);        
    }
}

void LedHandler::setMaxDistanceFromCenter(int distance) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        maxDistanceFromCenter = distance;
        xSemaphoreGive(configMutex);
    }
}

int LedHandler::getMaxDistanceFromCenter() {
    int returnDistance;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnDistance = maxDistanceFromCenter;
        xSemaphoreGive(configMutex);
    }
    return returnDistance;
}

bool LedHandler::getUseDistanceSwitch() {
    bool returnUse;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnUse = useDistanceSwitch;
        xSemaphoreGive(configMutex);
    }
    return returnUse;
}
void LedHandler::setUseDistanceSwitch(bool use) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        useDistanceSwitch = use;
        xSemaphoreGive(configMutex);
    }
}

int LedHandler::getDistanceMode() {
    int m;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        m = distanceMode;
        xSemaphoreGive(configMutex);
    }
    return m;
}

void LedHandler::setDistanceMode(int mode) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        distanceMode = mode;
        xSemaphoreGive(configMutex);
    }
}   

void LedHandler::setMidiParams(message_midi_params& params) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        memcpy(&midiParams, &params, sizeof(params));
        xSemaphoreGive(configMutex);
    }
    midiHue = (float)midiParams.hue / 360.0f;
    midiSat = (float)midiParams.saturation / 100.0f;
}