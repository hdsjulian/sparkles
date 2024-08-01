#include <Arduino.h>
#include <ledHandler.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef DEVICE
#define V1 1
#define V2 2 
#define D1 3
#define DEVICE V2
#endif 

typedef struct {
    int ledPinRed1, ledPinGreen1, ledPinBlue1;
    int ledPinRed2, ledPinGreen2, ledPinBlue2;
    int r, g, b, duration, reps, pause;
} FlashParams;

ledHandler::ledHandler() {

};

void ledHandler::setup() {
  ledcAttach(ledPinRed1, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttach(ledPinGreen1, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttach(ledPinBlue1, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttach(ledPinRed2, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttach(ledPinGreen2, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledcAttach(ledPinBlue2, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);
  ledsOff();
}


/*
void ledHandler::setAnimation(int animationType) {
  switch (animationType) {
    case 1:
      concentric();
      break;
    case 2:
      blink();
      break;
    case 3:
      candle(1000, 5, 100, 0, 0);
      break;
    case 4:
      flash(255, 0, 0, 50, 2, 50);
      break;
    case 5:
      ledOn(255, 0, 0, 1000, false);
      break;
    case 6:
      ledOn(255, 0, 0, 1000, true);
      break;
    default:
      break;
  }
}
*/
float ledHandler::fract(float x) { return x - int(x); }

float ledHandler::mix(float a, float b, float t) { return a + (b - a) * t; }

float ledHandler::step(float e, float x) { return x < e ? 0.0 : 1.0; }

float* ledHandler::hsv2rgb(float h, float s, float b, float* rgb) {
  rgb[0] = b * mix(1.0, constrain(abs(fract(h + 1.0) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s);
  Serial.println(rgb[0]); 
  rgb[1] = b * mix(1.0, constrain(abs(fract(h + 0.6666666) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s);
  rgb[2] = b * mix(1.0, constrain(abs(fract(h + 0.3333333) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s);
  return rgb;
}
void ledHandler::ledsOff() {
      ledcWrite(ledPinRed2, 0);
  ledcWrite(ledPinGreen2, 0);
  ledcWrite(ledPinBlue2, 0);
  ledcWrite(ledPinRed1, 0);
  ledcWrite(ledPinGreen1, 0);
  ledcWrite(ledPinBlue1, 0);

}
void ledHandler::flash(int r, int g, int b, int duration, int reps, int pause) {
  for (int i = 0; i < reps; i++ ){
    ledcFade(ledPinRed1, 0, r, duration);
    ledcFade(ledPinGreen1, 0, g, duration);
    ledcFade(ledPinBlue1, 0, b, duration);
    ledcFade(ledPinRed2, 0, r, duration);
    ledcFade(ledPinGreen2, 0, g, duration);
    ledcFade(ledPinBlue2, 0, b, duration);
    delay(duration);
    ledcFade(ledPinRed1, r, 0, duration);
    ledcFade(ledPinGreen1, g, 0, duration);
    ledcFade(ledPinBlue1, b, 0, duration);
    ledcFade(ledPinRed2, r, 0, duration);
    ledcFade(ledPinGreen2, g, 0, duration);
    ledcFade(ledPinBlue2, b, 0, duration);
    delay(pause);
    ledsOff(); 
  }
}  

/*
void ledHandler::startFlashTask() {
    FlashParams *flashParams = new FlashParams;

    // Initialize flashParams with your values...
    
    xTaskCreate(flashTask, "FlashLEDs", 2048, (void *)flashParams, 5, NULL);
}

void ledHandler::flashTask(void *parameters) {
    // Assuming ledcFade is adapted for RTOS and non-blocking
    // Structure to hold parameters, cast from void* to the actual type expected
    FlashParams *params = (FlashParams *)parameters;

    for (int i = 0; i < params->reps; i++) {
        ledcFade(params->ledPinRed1, 0, params->r, params->duration);
        ledcFade(params->ledPinGreen1, 0, params->g, params->duration);
        ledcFade(params->ledPinBlue1, 0, params->b, params->duration);
        ledcFade(params->ledPinRed2, 0, params->r, params->duration);
        ledcFade(params->ledPinGreen2, 0, params->g, params->duration);
        ledcFade(params->ledPinBlue2, 0, params->b, params->duration);
        vTaskDelay(pdMS_TO_TICKS(params->duration));
        
        ledcFade(params->ledPinRed1, params->r, 0, params->duration);
        ledcFade(params->ledPinGreen1, params->g, 0, params->duration);
        ledcFade(params->ledPinBlue1, params->b, 0, params->duration);
        ledcFade(params->ledPinRed2, params->r, 0, params->duration);
        ledcFade(params->ledPinGreen2, params->g, 0, params->duration);
        ledcFade(params->ledPinBlue2, params->b, 0, params->duration);
        vTaskDelay(pdMS_TO_TICKS(params->pause));
        
        ledsOff(); // Ensure this is also RTOS friendly
    }

    // Optionally delete the task if it should only run once
    vTaskDelete(NULL);
}*/

void ledHandler::ledOn(int r, int g, int b, int duration, int  frontback) {
  if (DEVICE == D1) {
    return;
  }
  if (frontback == 0 or frontback == 2) {
  ledcWrite(ledPinRed1, r);
  ledcWrite(ledPinGreen1, g);
  ledcWrite(ledPinBlue1, b);
    
  }
  if (frontback == 1 or frontback == 2) {
  ledcWrite(ledPinRed2, r);
  ledcWrite(ledPinGreen2, g);
  ledcWrite(ledPinBlue2, b);
  }
    delay(duration);
}    

/*
void ledHandler::delayLoop(int duration) {
  int start = millis();
  while (true) {
    if (millis()-start > duration) {
      break;
    }
  }
}
*/


void ledHandler::setupAnimation(message_animate *animationSetupMessage) {
  memcpy(&animationMessage, animationSetupMessage, sizeof(animationMessage));
  Serial.println("setupanimation");
  if (currentAnimation != OFF && false) {
    Serial.println("isn't off");
    return;
  }
  if (animationMessage.animationType == OFF) {
    Serial.println("turn off");
    ledsOff();
    return;
  }
  else if (animationMessage.animationType == SYNC_ASYNC_BLINK) {
    Serial.println("Setting up anim");
    setupSyncAsyncBlink();
    return;
  }
  else if (animationMessage.animationType == SLOW_STARTUP) {
    Serial.println("setting up slow startup");
    setupSlowStartup();
    return;
  }
  else if (animationMessage.animationType == SYNC_END) {
    Serial.println("setting up sync end");
    setupSyncEnd();
    return;
  }
  else {
    return;
  }
 
}


void ledHandler::setupSlowStartup() {
  repeatCounter = 0;
  localAnimationStart = 0;
  animationNextStep = 0;
  currentAnimation = animationMessage.animationType;
  unsigned long localAnimationStartMicros = animationMessage.startTime-(timeOffset*offsetMultiplier);
  //calculate start of first round
  //was will ich hier
  localAnimationStart = localAnimationStartMicros/1000;
  globalAnimationStart = localAnimationStart;
  if (micros() > localAnimationStartMicros) {
    Serial.println("not today");
    return;
  }
  animationNextStep = localAnimationStart;
  globalAnimationTimeframe = animationMessage.speed+animationMessage.pause;

  //first round of cycle is just this. speed+pause
  localAnimationTimeframe = globalAnimationTimeframe;
  /* calculate length of the entire animation. not needed for now.
  for (int i = 0; i < animationMessage.reps/2;i++) {
    cycleTotalRuntime += 2*((animationMessage.spread_time/(animationMessage.reps/2))*animationMessage.num_devices*i+animationMessage.speed+animationMessage.pause);
  }*/

}

void ledHandler::setupSyncEnd() {
  repeatCounter = 0;
  localAnimationStart = 0;
  animationNextStep = 0;
  currentAnimation = animationMessage.animationType;
  unsigned long localAnimationStartMicros = animationMessage.startTime-(timeOffset*offsetMultiplier);
  //calculate start of first round
  //was will ich hier
  localAnimationStart = localAnimationStartMicros/1000;
  globalAnimationStart = localAnimationStart;
  if (micros() > localAnimationStartMicros) {
    Serial.println("not today");
    return;
  }
  animationNextStep = localAnimationStart;
  globalAnimationTimeframe = animationMessage.speed+animationMessage.pause;

  localAnimationTimeframe = globalAnimationTimeframe;

}


void ledHandler::setupSyncAsyncBlink() {
  repeatCounter = 0;
  localAnimationStart = 0;
  animationNextStep = 0;
  currentAnimation = animationMessage.animationType;
  unsigned long localAnimationStartMicros = animationMessage.startTime-(timeOffset*offsetMultiplier);
  fraction = animationMessage.num_devices/FRACTION == 0 ? 1 : animationMessage.num_devices/FRACTION;
  //calculate start of first round
  //was will ich hier
  localAnimationStart = localAnimationStartMicros/1000;
  globalAnimationStart = localAnimationStart;
  if (micros() > localAnimationStartMicros) {
    Serial.println("not today");
    return;
  }
  animationNextStep = localAnimationStart;
  globalAnimationTimeframe = animationMessage.speed+animationMessage.pause;

  //first round of cycle is just this. speed+pause
  localAnimationTimeframe = globalAnimationTimeframe;
  /* calculate length of the entire animation. not needed for now.
  for (int i = 0; i < animationMessage.reps/2;i++) {
    cycleTotalRuntime += 2*((animationMessage.spread_time/(animationMessage.reps/2))*animationMessage.num_devices*i+animationMessage.speed+animationMessage.pause);
  }*/

}
/*
void ledHandler::setupRowBlink() {
  repeatCounter = 0;
  localAnimationStart = 0;
  animationNextStep = 0;
  currentAnimation = animationMessage.animationType;
  unsigned long localAnimationStartMicros = animationMessage.startTime-(timeOffset*offsetMultiplier);
  //calculate start of first round
  //was will ich hier
  float distance = 0;
  //calculate distance as a function of spread time (in centimeters per second) and the distance from the axis
  distance = (xPos/animationMessage.spread_time)*1000;
  localAnimationStart = localAnimationStartMicros/1000+distance*speed;
  globalAnimationStart = localAnimationStart;
  if (micros() > localAnimationStartMicros) {
    Serial.println("not today");
    return;
  }
  animationNextStep = localAnimationStart;
  globalAnimationTimeframe = animationMessage.speed+animationMessage.pause;

  //first round of cycle is just this. speed+pause
  localAnimationTimeframe = globalAnimationTimeframe;
  *//* calculate length of the entire animation. not needed for now.
  for (int i = 0; i < animationMessage.reps/2;i++) {
    cycleTotalRuntime += 2*((animationMessage.spread_time/(animationMessage.reps/2))*animationMessage.num_devices*i+animationMessage.speed+animationMessage.pause);
  }
}*/
void ledHandler::turnOff() {
  currentAnimation = OFF;
  ledsOff();
}
void ledHandler::run() {

  switch (currentAnimation) {
    case OFF:
      ledsOff();
      break;
    case SYNC_ASYNC_BLINK:
      syncAsyncBlink();
      break;
    default:
      break;
  }
}

unsigned long ledHandler::calculate(message_animate *animationMessage) {
  Serial.println("calculating for ");
  switch(animationMessage->animationType) {
    case OFF:
    Serial.println("Off");
      return millis();
    case SYNC_ASYNC_BLINK:
      Serial.println("syncasync");
      return calculateSyncAsyncBlink(animationMessage);
    break;
    case SLOW_STARTUP:
      Serial.println("slow startup");
      return calculateSlowStartup(animationMessage);
    case SYNC_END: 
      Serial.println("sync end");
      return calculateSyncEnd(animationMessage);
    default:  
    return 0;

  }

}

unsigned long ledHandler::calculateSyncAsyncBlink(message_animate *animationMessage) {
  unsigned long base_time = (animationMessage->pause+animationMessage->speed)*(animationMessage->reps+1)*(animationMessage->animationreps+1);
  Serial.println("base time "+String(base_time));
  Serial.println("animation reps "+String(animationMessage->animationreps));
  Serial.println("reps "+String(animationMessage->reps));
  Serial.println("Speed "+String(animationMessage->speed));
  Serial.println("Pause "+String(animationMessage->pause));
  Serial.println("spread time "+String(animationMessage->spread_time));
  Serial.println("num devices "+String(animationMessage->num_devices));
  for (int i = 0; i<animationMessage->animationreps;i++) {
    for (int j = 0; j < animationMessage->reps;j++) {
      if (j <= animationMessage->reps/2) {
        if (i == 0) {
        }
        base_time += (animationMessage->spread_time/(animationMessage->reps/2))*fraction*j;
      }
      else {
        base_time += (animationMessage->spread_time/(animationMessage->reps/2))*fraction*(animationMessage->reps-j);
        if (i == 0) {
        }

      }
    }
  }
  Serial.println("base time "+String(base_time));
  Serial.println("millis "+String(millis()));
  return base_time;
}

unsigned long ledHandler::calculateSlowStartup(message_animate *animationMessage) {
  unsigned long base_time = 0;
  Serial.println("animation reps "+String(animationMessage->animationreps));
  Serial.println("reps "+String(animationMessage->reps));
  Serial.println("Speed "+String(animationMessage->speed));
  Serial.println("Pause "+String(animationMessage->pause));
  Serial.println("spread time "+String(animationMessage->spread_time));
  Serial.println("num devices "+String(animationMessage->num_devices));
  for (int i = 0; i<animationMessage->animationreps;i++) {
    for (int j = 0; j < animationMessage->reps;j++) {
      base_time += animationMessage->speed/j+ animationMessage->pause/j;
      }
    }
  Serial.println("base time "+String(base_time));
  Serial.println("millis "+String(millis()));
  return base_time;
}

unsigned long ledHandler::calculateSyncEnd(message_animate *animationMessage) {
  unsigned long base_time = animationMessage->speed;
  return base_time;
}

void ledHandler::syncAsyncBlink() {
  if (millis() < animationNextStep) {
    return;
  }

    if (millis() > animationNextStep and millis() < localAnimationStart+animationMessage.speed) {
    //redsteps? and backwards?
    //cyclestart berechnen auch abhängig vom spread und ansonsten einfach runterrattern dat ding

    int elapsedTime = millis()-localAnimationStart;
    redfloat  = calculateFlash(animationMessage.rgb1[0], elapsedTime);
    greenfloat = calculateFlash(animationMessage.rgb1[1], elapsedTime);
    bluefloat = calculateFlash(animationMessage.rgb1[2], elapsedTime);  
    writeLeds();
    // how to calculate?
    animationNextStep = millis()+animationMessage.speed/256;
  }

  //if a repeat should happen...
  if (millis()  > localAnimationStart+animationMessage.speed) {
    repeatCounter++;
    Serial.println("repeat counter is now "+String(repeatCounter)+" of "+animationMessage.reps);
    globalAnimationStart = globalAnimationStart+globalAnimationTimeframe;

    //cycle start noch timen
    //and figure out the start of next cycle
    if (repeatCounter <= animationMessage.reps/2) {
      localAnimationStart = globalAnimationStart +(animationMessage.spread_time/(animationMessage.reps/2))*(position%fraction)*repeatCounter; 
      globalAnimationTimeframe = animationMessage.speed+animationMessage.pause+(animationMessage.spread_time/(animationMessage.reps/2))*fraction*repeatCounter;
    }
    else {
      localAnimationStart = globalAnimationStart + (animationMessage.spread_time/(animationMessage.reps/2))*(position%fraction)*(animationMessage.reps-repeatCounter);
      globalAnimationTimeframe = animationMessage.speed+animationMessage.pause+(animationMessage.spread_time/(animationMessage.reps/2))*fraction*(animationMessage.reps-repeatCounter);
    }

    //globalAnimationStart = globalAnimationStart+globalAnimationTimeframe;

  }

  
  //if all repetitions have happened
  if (repeatCounter == animationMessage.reps) {
    if (animationRepeatCounter == animationMessage.animationreps) {
      //either turn off
      ledsOff();
      currentAnimation = OFF;
      return;
    }
    else {
      // or repeat 
      animationRepeatCounter++;
       
      repeatCounter = 0;
    }
  }
  //hier kommt die tatsächliche animation rein

}

/*
void ledHandler::syncAsyncBlink() {

 
  //wait until next step. if all repeats done: done. 

 
  if (millis() > animationNextStep and millis() < localAnimationStart+animationMessage.speed) {
    //redsteps? and backwards?
    //cyclestart berechnen auch abhängig vom spread und ansonsten einfach runterrattern dat ding

    int elapsedTime = millis()-localAnimationStart;
    redfloat  = calculateFlash(animationMessage.rgb1[0], elapsedTime);
    greenfloat = calculateFlash(animationMessage.rgb1[1], elapsedTime);
    bluefloat = calculateFlash(animationMessage.rgb1[2], elapsedTime);  
    writeLeds();
    // how to calculate?
    animationNextStep = millis()+animationMessage.speed/256;
  }

  //if a repeat should happen...
  if (millis()  > localAnimationStart+animationMessage.speed) {
    repeatCounter++;
    Serial.println("repeat counter is now "+String(repeatCounter)+" of "+animationMessage.reps);
    globalAnimationStart = globalAnimationStart+globalAnimationTimeframe;

    //cycle start noch timen
    //and figure out the start of next cycle
    if (repeatCounter <= animationMessage.reps/2) {
      localAnimationStart = globalAnimationStart +(animationMessage.spread_time/(animationMessage.reps/2))*position*repeatCounter; 
      globalAnimationTimeframe = animationMessage.speed+animationMessage.pause+(animationMessage.spread_time/(animationMessage.reps/2))*animationMessage.num_devices*repeatCounter;
    }
    else {
      localAnimationStart = globalAnimationStart + (animationMessage.spread_time/(animationMessage.reps/2))*position*(animationMessage.reps-repeatCounter);
      globalAnimationTimeframe = animationMessage.speed+animationMessage.pause+(animationMessage.spread_time/(animationMessage.reps/2))*animationMessage.num_devices*(animationMessage.reps-repeatCounter);
    }

    //globalAnimationStart = globalAnimationStart+globalAnimationTimeframe;

  }

  
  //if all repetitions have happened
  if (repeatCounter == animationMessage.reps) {
    if (animationRepeatCounter == animationMessage.animationreps) {
      //either turn off
      ledsOff();
      currentAnimation = OFF;
      return;
    }
    else {
      // or repeat 
      animationRepeatCounter++;
       
      repeatCounter = 0;
    }
  }
  //hier kommt die tatsächliche animation rein


}*/

void ledHandler::slowStartup() {
 
  //wait until next step. if all repeats done: done. 
  if (millis() < animationNextStep) {
    return;
  }
 
  if (millis() > animationNextStep and millis() < localAnimationStart+animationMessage.speed) {
    //redsteps? and backwards?
    //cyclestart berechnen auch abhängig vom spread und ansonsten einfach runterrattern dat ding

    int elapsedTime = millis()-localAnimationStart;
    redfloat  = calculateFlash(animationMessage.rgb1[0], elapsedTime, repeatCounter);
    greenfloat = calculateFlash(animationMessage.rgb1[1], elapsedTime, repeatCounter);
    bluefloat = calculateFlash(animationMessage.rgb1[2], elapsedTime, repeatCounter);  
    writeLeds();
    // how to calculate?
    animationNextStep = millis()+(animationMessage.speed/repeatCounter)/256;
  }

  //if a repeat should happen...
  if (millis()  > localAnimationStart+animationMessage.speed/repeatCounter) {
    repeatCounter++;
    Serial.println("repeat counter is now "+String(repeatCounter)+" of "+animationMessage.reps);
    globalAnimationStart = globalAnimationStart+globalAnimationTimeframe;

    //cycle start noch timen
    //and figure out the start of next cycle
    localAnimationStart = globalAnimationStart;
    globalAnimationTimeframe = animationMessage.speed/repeatCounter+animationMessage.pause/repeatCounter;

    //globalAnimationStart = globalAnimationStart+globalAnimationTimeframe;

  }

  
  //if all repetitions have happened
  if (repeatCounter == animationMessage.reps) {
    if (animationRepeatCounter == animationMessage.animationreps) {
      //either turn off
      ledsOff();
      currentAnimation = OFF;
      return;
    }
    else {
      // or repeat 
      animationRepeatCounter++;
       
      repeatCounter = 0;
    }
  }
  //hier kommt die tatsächliche animation rein


}

void ledHandler::rowBlink() {

  if (millis() < animationNextStep) {
    return;
  }
  if (millis() > animationNextStep and millis() < localAnimationStart+animationMessage.speed) {
    int elapsedTime = millis()-localAnimationStart;
    redfloat  = calculateFlash(animationMessage.rgb1[0], elapsedTime);
    greenfloat = calculateFlash(animationMessage.rgb1[1], elapsedTime);
    bluefloat = calculateFlash(animationMessage.rgb1[2], elapsedTime);  
    writeLeds();
    animationNextStep = millis()+animationMessage.speed/256;

  
}
}
/*
void ledHandler::rowBlink() {

 
  //wait until next step. if all repeats done: done. 
  if (millis() < animationNextStep) {
    return;
  }
 
  if (millis() > animationNextStep and millis() < localAnimationStart+animationMessage.speed) {
    //redsteps? and backwards?
    //cyclestart berechnen auch abhängig vom spread und ansonsten einfach runterrattern dat ding

    int elapsedTime = millis()-localAnimationStart;
    redfloat  = calculateFlash(animationMessage.rgb1[0], elapsedTime);
    greenfloat = calculateFlash(animationMessage.rgb1[1], elapsedTime);
    bluefloat = calculateFlash(animationMessage.rgb1[2], elapsedTime);  
    writeLeds();
    // how to calculate?
    animationNextStep = millis()+animationMessage.speed/256;
  }

  //if a repeat should happen...
  if (millis()  > localAnimationStart+animationMessage.speed) {
    repeatCounter++;
    globalAnimationStart = globalAnimationStart+globalAnimationTimeframe;

    //cycle start noch timen
    //and figure out the start of next cycle
    if (repeatCounter % 2 == 0) {
      //spread time in meters per second"
      localAnimationStart = globalAnimationStart +(xPos/animationMessage.spread_time)*1000; 
      globalAnimationTimeframe = animationMessage.maxPos/animationMessage.spread_time;
    }
    else {
      localAnimationStart = globalAnimationStart + (animationMessage.maxPos-xPos/animationMessage.spread_time)*1000; 
      globalAnimationTimeframe = animationMessage.maxPos/animationMessage.spread_time;
    }

    //globalAnimationStart = globalAnimationStart+globalAnimationTimeframe;

  }
 //if all repetitions have happened
  if (repeatCounter == animationMessage.reps) {
    if (animationRepeatCounter == animationMessage.animationreps) {
      //either turn off
      ledsOff();
      currentAnimation = OFF;
      return;
    }
    else {
      // or repeat 
      animationRepeatCounter++;
      repeatCounter = 0;
    }
  }
  //hier kommt die tatsächliche animation rein


}
 */

float ledHandler::calculateFlash(int targetVal, unsigned long timeElapsed, int speedfactor){
  if (timeElapsed < 0) {
    timeElapsed = 0;
  }  else if (timeElapsed > animationMessage.speed) {
    timeElapsed = animationMessage.speed;
  }
  float normalizedTime = (float)timeElapsed/((float)animationMessage.speed/speedfactor);
  float factor;  
  float colorValue;
  if (normalizedTime <= 0.5) {
    factor = pow(normalizedTime * 2, animationMessage.exponent);
    colorValue = (targetVal * factor);
  }
  else {
    factor = pow(abs(normalizedTime - 1) *2, animationMessage.exponent);
    colorValue = (targetVal*factor);
  }

  if (colorValue < 0) {
    return 0;
  }
  else if (colorValue > 255) {
    return 255;
  }
  else {
    return colorValue;
  }

}

void ledHandler::writeLeds() {
  ledcWrite(ledPinRed1, (int)floor(redfloat));
  ledcWrite(ledPinGreen1, (int)floor(greenfloat));
  ledcWrite(ledPinBlue1, (int)floor(bluefloat));
  ledcWrite(ledPinRed2, (int)floor(redfloat));
  ledcWrite(ledPinGreen2, (int)floor(greenfloat));
  ledcWrite(ledPinBlue2, (int)floor(bluefloat));
}

void ledHandler::candle(int duration, int reps, int pause, unsigned long startTime, unsigned long timeOffset) {
  Serial.println("should blink");
  //entfernung einbauen
  uint32_t currentTime = micros();
  uint32_t difference = currentTime-timeOffset;
  if (startTime-difference > 0) {
    Serial.println("time now: "+String(micros()));
    Serial.println("Delaying for"+String(startTime-difference+((distance/34300)*1000000*1500)));
    //todo turn this into sleep
    delayMicroseconds(startTime-difference+((distance/34300)*1000000*1500));
    Serial.println("delay done");
  }
  bool heating = true;
  float redsteps = 255.0/(duration/3);
  float greensteps = 255.0/(duration/3);
  float bluesteps = 80.0/(duration/3);
  redfloat = 0.0;
  greenfloat = 0.0;
  bluefloat = 0.0;
  
for (int i = 0; i < reps; i++ ){
  redfloat = 0.0;
  greenfloat = 0.0;
  bluefloat = 0.0;
  for (int j = 0; j <=duration; j++ ) 
    {
    if (redfloat <255) {
      redfloat += redsteps;
    }
    else if (greenfloat <255) {
      greenfloat += greensteps;
    }
    else {
      bluefloat+=bluesteps;
    }
    ledcWrite(ledPinRed2, (int)floor(redfloat));
    ledcWrite(ledPinGreen2, (int)floor(greenfloat));
    ledcWrite(ledPinBlue2, (int)floor(bluefloat));
    ledcWrite(ledPinRed1, (int)floor(redfloat));
    ledcWrite(ledPinGreen1, (int)floor(greenfloat));
    ledcWrite(ledPinBlue1, (int)floor(bluefloat));
    int start = millis();
    while (true) {
      if (millis()-start > 100) {
        break;
      }

    }
    delay(1);
  }
  for (int j = 0; j <=duration; j++ ) 
    {
    if (bluefloat > 0) {
      bluefloat -= bluesteps;
      if (bluefloat <= 0) {
        bluefloat = 0;
      }
    }
    else if (greenfloat > 0) {
      greenfloat -= greensteps;
      if (greenfloat <= 0) {
        greenfloat = 0;
      }
    }
    else {
      redfloat-=redsteps;
      if (redfloat <= 0) {
        redfloat = 0;
      }
    }
    ledcWrite(ledPinRed2, (int)floor(redfloat));
    ledcWrite(ledPinGreen2, (int)floor(greenfloat));
    ledcWrite(ledPinBlue2, (int)floor(bluefloat));
    ledcWrite(ledPinRed1, (int)floor(redfloat));
    ledcWrite(ledPinGreen1, (int)floor(greenfloat));
    ledcWrite(ledPinBlue1, (int)floor(bluefloat));
    delay(1);
  }
  ledsOff(); 
  delay(1);
  }
}





  void ledHandler::blink() {
    Serial.println("should blink");
  uint32_t currentTime = micros();
  //uint32_t difference = currentTime-timeOffset;
  /*
  if (startTime-difference > 0) {
    delayMicroseconds(startTime-difference);
    flash(animationMessage.rgb1[0], animationMessage.rgb1[1], animationMessage.rgb1[2], animationMessage.speed, animationMessage.reps, animationMessage.delay);
  }
  else {
    Serial.println("too late, need to wait to chime in");
    //find algorithm that calculates length of animation and finds later point in time to jump in
  }  */
return;

}


void ledHandler::concentric() {


}

void ledHandler::setDistance(float dist) {
  distance = dist;
}


void ledHandler::setTimeOffset(unsigned long setOffset, int setOffsetMultiplier) {
  timeOffset = setOffset;
  offsetMultiplier = setOffsetMultiplier;
}

void ledHandler::setPosition(int id) {
  position = id;
}

void ledHandler::setLocation(int xposition, int yposition, int zposition) {
  xPos = xposition;
  yPos = yposition;
  zPos = zposition;
  Serial.println("positions set to x:"+String(xPos)+", y: "+String(yPos)+" z: "+String(zPos));
  delay(10000);
}
// IF BOARD == V2

void ledHandler::getNextAnimation(message_animate *animationMessage) {
  switch (animationMessage->animationType) {
    case OFF:
      return;
    case SYNC_ASYNC_BLINK:
      createSyncAsyncBlink(animationMessage);
      return;
    case SLOW_STARTUP:
    createSlowStartup(animationMessage);
    return;
    case SYNC_END:
    createSyncEnd(animationMessage);
    return;
    default:
      return;
  }
}
  void ledHandler::createSyncAsyncBlink(message_animate *animationMessage) {
    
    int red = random(0,256);
    int blue = random(0,256);
    int green = random(0,256);
    while (red <100 and blue < 100 and green <100) {
      red = red+1 > 255? red : red+1;
      blue = blue+1 > 255? blue : blue+1;
      green = green+1 > 255? green : green+1;
    }
    animationMessage->rgb1[0] =red;
    animationMessage->rgb1[1] =  green;
    animationMessage->rgb1[2] = blue;
    animationMessage->speed = random(500, 2000);
    animationMessage->pause = random(0, 200);
    animationMessage->spread_time = random(1000, 5000);
    animationMessage->reps = random(5, 10);
    animationMessage->animationreps = random(5, 20);
  }
/* old
  void ledHandler::createSyncAsyncBlink(message_animate *animationMessage) {
    
    int red = random(0,256);
    int blue = random(0,256);
    int green = random(0,256);
    while (red <100 and blue < 100 and green <100) {
      red = red+1 > 255? red : red+1;
      blue = blue+1 > 255? blue : blue+1;
      green = green+1 > 255? green : green+1;
    }
    animationMessage->rgb1[0] =red;
    animationMessage->rgb1[1] =  green;
    animationMessage->rgb1[2] = blue;
    animationMessage->speed = random(100, 400);
    animationMessage->pause = random(100, 1000);
    animationMessage->spread_time = random(100, 300);
    animationMessage->reps = random(10, 50);
    animationMessage->animationreps = random(5, 20);
  }
*/
    void ledHandler::createSlowStartup(message_animate *animationMessage) {
    
    int red = 128;
    int blue = 128;
    int green = 0;
    animationMessage->rgb1[0] =red;
    animationMessage->rgb1[1] =  green;
    animationMessage->rgb1[2] = blue;
    animationMessage->speed = 1000;
    animationMessage->pause = 2000;
    animationMessage->spread_time = 0;
    animationMessage->reps = 20;
    animationMessage->animationreps = 0;
  }

    void ledHandler::createRowBlink(message_animate *animationMessage) {
    
    int red = random(125,256);
    int blue = random(125,256);
    int green = random(125,256);
    animationMessage->rgb1[0] =red;
    animationMessage->rgb1[1] =  green;
    animationMessage->rgb1[2] = blue;
    animationMessage->speed = random(100, 400);
    animationMessage->pause = random(100, 1000);
    animationMessage->spread_time = random(1, 5);
    animationMessage->reps = random(10, 50);
    animationMessage->animationreps = random(5, 20);
  }

    void ledHandler::createSyncEnd(message_animate *animationMessage) {
    
    int red = random(125,256);
    int blue = random(125,256);
    int green = random(125,256);
    animationMessage->rgb1[0] =red;
    animationMessage->rgb1[1] =  green;
    animationMessage->rgb1[2] = blue;
    animationMessage->speed = random(100, 400);
    animationMessage->pause = random(100, 1000);
    animationMessage->spread_time = random(1, 5);
    animationMessage->reps = random(10, 50);
    animationMessage->animationreps = random(5, 20);
  }




void ledHandler::printStatus() {
  Serial.println("Status of LEDHandler");
  Serial.println("Current Animation "+String(currentAnimation));
  Serial.println("Animation next step "+String(animationNextStep));
  Serial.println("Animation Spread Time"+String(animationMessage.spread_time));
  Serial.println("Current millis time "+String(millis()));
  Serial.println("Current Micros time "+String(micros()));
  Serial.println("Local animation start "+String(localAnimationStart));
  Serial.println("Global animation start "+String(globalAnimationStart));
  Serial.println("localAnimationTimeframe "+String(localAnimationTimeframe));
  Serial.println("globalAnimationTimeframe "+String(globalAnimationTimeframe));
  Serial.println("repeatCounter "+String(repeatCounter));
  Serial.println("animationRepeatCounter "+String(animationRepeatCounter));
  Serial.println("Position"+String(position));
  Serial.println("timerOffset "+String(timeOffset));
  Serial.println("animationMessage.startTime "+String(animationMessage.startTime));
  Serial.println("xPos "+String(xPos));
  Serial.println("yPos "+String(xPos));
  Serial.println("zPos "+String(xPos));
  
};

void ledHandler::printAnimationMessage(const message_animate &animationMessage) {
    Serial.print("messageType: ");
    Serial.println(animationMessage.messageType);
    
    Serial.print("animationType: ");
    Serial.println(animationMessage.animationType);
    
    Serial.print("speed: ");
    Serial.println(animationMessage.speed);
    
    Serial.print("delay: ");
    Serial.println(animationMessage.delay);
    
    Serial.print("pause: ");
    Serial.println(animationMessage.pause);
    
    Serial.print("reps: ");
    Serial.println(animationMessage.reps);
    
    Serial.print("rgb1: ");
    Serial.print(animationMessage.rgb1[0]);
    Serial.print(", ");
    Serial.print(animationMessage.rgb1[1]);
    Serial.print(", ");
    Serial.println(animationMessage.rgb1[2]);
    
    Serial.print("rgb2: ");
    Serial.print(animationMessage.rgb2[0]);
    Serial.print(", ");
    Serial.print(animationMessage.rgb2[1]);
    Serial.print(", ");
    Serial.println(animationMessage.rgb2[2]);
    
    Serial.print("startTime: ");
    Serial.println(animationMessage.startTime);
    
    Serial.print("num_devices: ");
    Serial.println(animationMessage.num_devices);
    
    Serial.print("spread_time: ");
    Serial.println(animationMessage.spread_time);
    
    Serial.print("exponent: ");
    Serial.println(animationMessage.exponent);
    
    Serial.print("animationreps: ");
    Serial.println(animationMessage.animationreps);
}


