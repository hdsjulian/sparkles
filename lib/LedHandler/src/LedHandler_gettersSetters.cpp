#include <LedHandler.h>
void LedHandler::setTimerOffset(long long newOffset) {
    ESP_LOGI("LED", "Setting LED Timer Offset %lld", newOffset);
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
        ESP_LOGI("LED", "Failed to get current animation");
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
        ESP_LOGI("LED", "Failed to get current position");
        return -1;
    }
}
void LedHandler::setCurrentPosition(int newPosition) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        position = newPosition;
        xSemaphoreGive(configMutex);
    }
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
    unsigned long long now = micros();
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
    unsigned long long clientNow = micros();
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        if (timerOffset < 0) {
        microsUntilStart = masterStartTime - ((long long)clientNow - timerOffset);
    }
    else {
      microsUntilStart = masterStartTime - ((long long)clientNow + timerOffset);
    }
        xSemaphoreGive(configMutex);
    }
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
        ESP_LOGI("LED", "Setting number of devices: %d", numDevices);
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
        ESP_LOGI("LED", "Setting background shimmer fadeout to %s", fadeout ? "true" : "false");
        xSemaphoreGive(configMutex);        
    }
}