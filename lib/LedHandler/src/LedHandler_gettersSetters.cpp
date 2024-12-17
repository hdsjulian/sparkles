#include <LedHandler.h>
void LedHandler::setTimerOffset(unsigned long long newOffset) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        timerOffset = newOffset;
        xSemaphoreGive(configMutex);
    }
}

// Thread-safe getter for offset
unsigned long long LedHandler::getTimerOffset() {
    int currentOffset;
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

void LedHandler::setAnimation(message_animation& animationData) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        memcpy(&animation, &animationData, sizeof(animationData));
        xSemaphoreGive(configMutex);
    }
}
message_animation LedHandler::getAnimation() {
    message_animation returnAnimation;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        returnAnimation = animation;
        xSemaphoreGive(configMutex);
    }
    return returnAnimation;
}