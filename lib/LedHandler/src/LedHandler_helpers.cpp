#include <LedHandler.h>


float LedHandler::fract(float x) {
    return x - floor(x);
}

float LedHandler::mix(float a, float b, float t) { return a + (b - a) * t; }

float LedHandler::step(float e, float x) { return x < e ? 0.0 : 1.0; }

float LedHandler::intRGBToFloat(int val) { return val / 255.0; }

float LedHandler::float_to_sRGB(float val)
{
    if (val < 0.0031308)
        val *= 12.92;
    else
        val = 1.055 * pow(val, 1.0 / 2.4) - 0.055;
    return val;
}
float *LedHandler::hsv2rgb(float h, float s, float b, float *rgb)
{
    rgb[0] = float_to_sRGB(b * mix(1.0, constrain(abs(fract(h + 1.0) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s) * 255);
    rgb[1] = float_to_sRGB(b * mix(1.0, constrain(abs(fract(h + 0.6666666) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s) * 255);
    rgb[2] = float_to_sRGB(b * mix(1.0, constrain(abs(fract(h + 0.3333333) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s) * 255);
    return rgb;
}

float LedHandler::sRGB_to_float(float val)
{
    if (val < 0.04045)
        val /= 12.92;
    else
        val = pow((val + 0.055) / 1.055, 2.4);
    return val;
}





TickType_t LedHandler::microsToTicks(unsigned long long micros) {
    return (TickType_t)(micros / (1000000 / configTICK_RATE_HZ));
}

message_animation LedHandler::createAnimation(animationEnum animationType ) {
    message_animation animation;
    animation.animationType = animationType;

    return animation;

}

message_animation LedHandler::createFlash(unsigned long long startTime, unsigned long long duration, int repetitions, int hue, int saturation, int brightness) {
    message_animation animation;
    animation.animationType = BLINK;
    animation.animationParams.blink.startTime = startTime;
    animation.animationParams.blink.duration = duration;
    animation.animationParams.blink.repetitions = repetitions;
    animation.animationParams.blink.hue = hue;
    animation.animationParams.blink.saturation = saturation;
    animation.animationParams.blink.brightness = brightness;
    return animation;
}

message_animation LedHandler::createCandle(unsigned long long startTime, int duration, int hue, int saturation, int value) {
    message_animation animation;
    animation.animationType = CANDLE;
    animation.animationParams.candle.startTime = startTime;
    animation.animationParams.candle.duration = duration;
    animation.animationParams.candle.hue = hue;
    animation.animationParams.candle.saturation = saturation;
    animation.animationParams.candle.value = value;
    return animation;
}

message_animation LedHandler::createSyncAsyncBlinkRandom() {
    message_animation animation;
    animation.animationType = SYNC_ASYNC_BLINK;
    animation_sync_async_blink sabAnimation;
    sabAnimation.blinkDuration = random(syncAsyncMinDuration, syncAsyncMaxDuration);
    sabAnimation.pause = random(syncAsyncMinPause, syncAsyncMaxPause);
    sabAnimation.repetitions = random(syncAsyncMinReps, syncAsyncMaxReps);
    sabAnimation.spreadTime = random(syncAsyncMinSpread, syncAsyncMaxSpread); 
    sabAnimation.fraction = random (10, 20);
    sabAnimation.animationReps = random(syncAsyncMinAniReps, syncAsyncMaxAniReps);
    sabAnimation.hue = random(0, 40);
    sabAnimation.saturation = random(20, 127);
    sabAnimation.brightness = random(80, 180);
    animation.animationParams.syncAsyncBlink = sabAnimation;
    return animation;

}

void LedHandler::stopAnimationTask() {
    if (animationTaskHandle != NULL) {
        vTaskDelete(animationTaskHandle);
        animationTaskHandle = NULL;
    }
    setCurrentAnimation(OFF);
    ESP_LOGI("LED", "Animation task stopped");
}

void LedHandler::turnOff() {
    ledsOff();
    setCurrentAnimation(OFF);
    if (animationTaskHandle != NULL) {
        vTaskDelete(animationTaskHandle);
        animationTaskHandle = NULL;
    }
    ESP_LOGI("LED", "LEDs turned off and animation task stopped");
}

void LedHandler::blink(unsigned long long startTime, unsigned long long duration, int repetitions, int hue, int saturation, int brightness) {
    message_animation animation = createFlash(startTime, duration, repetitions, hue, saturation, brightness);
    pushToAnimationQueue(animation);
    //ESP_LOGI("LED", "Blinking with hue: %d, saturation: %d, brightness: %d", hue, saturation, brightness);
}

void LedHandler::batteryBlink(float batteryPercentage) {
    int hue, sat, val = 255;
    if (batteryPercentage >= 95.0f) {
        // White to green
        float frac = (batteryPercentage - 95.0f) / 5.0f; // 95-100%
        hue = 96 * (1.0f - frac); // 0 (white) to 96 (green)
        sat = (int)(255 * (1.0f - frac)); // 0 (white) to 255 (green)
    } else if (batteryPercentage >= 70.0f) {
        // Green to yellow
        float frac = (batteryPercentage - 70.0f) / 25.0f; // 70-95%
        hue = (int)(96 * frac + 32 * (1.0f - frac)); // 96 (green) to 32 (yellow)
        sat = 255;
    } else if (batteryPercentage >= 30.0f) {
        // Yellow to red
        float frac = (batteryPercentage - 30.0f) / 40.0f; // 30-70%
        hue = (int)(32 * frac); // 32 (yellow) to 0 (red)
        sat = 255;
    } else {
        // Red
        hue = 0; sat = 255;
    }
    blink(micros(), 100, 1, hue, sat, val);
}

TickType_t LedHandler::getNextAnimationTicks() {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        TickType_t nextTicks = microsToTicks((unsigned long long)microsUntilEnd);
        xSemaphoreGive(configMutex);
        return nextTicks;
    }
    else {
        ESP_LOGI("LED", "Failed to get next animation ticks");
        return 0;
    }
}

void LedHandler::setMicrosUntilEnd(message_animation& animationData) {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        unsigned long long endTime = calculateAnimation(animationData);
        microsUntilEnd = endTime - micros();
        xSemaphoreGive(configMutex);
    }
    else {
        ESP_LOGI("LED", "Failed to set micros until end");
    }
}

void LedHandler::resetMicrosUntilEnd() {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE) {
        microsUntilEnd = micros()+1000000; // Set to 1 second in the future
        ESP_LOGI("LED", "Reset micros until end to %llu", microsUntilEnd);
        xSemaphoreGive(configMutex);
    }
    else {
        ESP_LOGI("LED", "Failed to reset micros until end");
    }
}

bool LedHandler::isTimedAnimation(animationEnum type) {
    if (type == STROBE || type == SYNC_ASYNC_BLINK || type == BLINK) {
        ESP_LOGI("LED", "Animation %d is timed", type);
        return true;
    }
    else {
        ESP_LOGI("LED", "Animation %d is not timed", type);
        return false;
    }
}