#include "LedHandler.h"

unsigned long long LedHandler::calculateAnimation(message_animation& animationData)
{
    
    if (animationData.animationType == MIDI)
    {
        return getDecayTime(animationData.animationParams.midi.note, animationData.animationParams.midi.velocity);
    }
    else if (animationData.animationType == STROBE)
    {
        return animationData.animationParams.strobe.duration * 1000;
    }
    else if (animationData.animationType == BLINK)
    {
        return animationData.animationParams.blink.duration * 1000 * animationData.animationParams.blink.repetitions;
    }
    else if (animationData.animationType == SYNC_ASYNC_BLINK)
    {
        return calculateSyncAsyncBlink(animationData);        
    }
    return 0;
}

unsigned long long LedHandler::calculateSyncAsyncBlink(message_animation& animationData)
{
    int spreadTime = animationData.animationParams.syncAsyncBlink.spreadTime;
    int blinkDuration = animationData.animationParams.syncAsyncBlink.blinkDuration;
    int pause = animationData.animationParams.syncAsyncBlink.pause;
    uint8_t repetitions = animationData.animationParams.syncAsyncBlink.repetitions;
    uint16_t animationReps = animationData.animationParams.syncAsyncBlink.animationReps;
    uint8_t fraction = animationData.animationParams.syncAsyncBlink.fraction;
    unsigned long long returnTime;
    returnTime += pause * repetitions * animationReps;
    returnTime += blinkDuration * repetitions * animationReps;
    for (int i = 1; i < animationReps+1; i++)
    {
        for (int j = 0; j < repetitions; j++)
        {
            if (j <= repetitions/2) {
                returnTime += (spreadTime/(repetitions/2))*fraction*j;
            }
            returnTime += (spreadTime/(repetitions/2))*fraction*(repetitions-j);
        }
    }

    return returnTime*1000;
}