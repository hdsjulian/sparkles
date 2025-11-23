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

