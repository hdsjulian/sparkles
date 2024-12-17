#include <LedHandler.h>

void LedHandler::addToMidiTable(midiNoteTable midiNoteTableArray[OCTAVESONKEYBOARD], message_animation animation, int position)
{
    if (animation.animationParams.midi.note % OCTAVE != getMidiNoteFromPosition(position) % OCTAVE)
    {
        return;
    }

    int note = animation.animationParams.midi.note;
    int velocity = animation.animationParams.midi.velocity;
    int octave = (note / OCTAVE) - 1;
    if (xSemaphoreTake(midiNoteTableMutex, portMAX_DELAY) == pdTRUE) {
        if (midiNoteTableArray[octave].velocity < velocity)
        {
            midiNoteTableArray[octave].velocity = velocity;
            midiNoteTableArray[octave].note = note;
            midiNoteTableArray[octave].startTime = velocity == 0 ? 0 : micros();
            //ESP_LOGI("LED", "Added: Note: %d, Velocity: %d, Octave: %d", note, velocity, octave);
            //ESP_LOGI("LED", "Start time: %llu", midiNoteTableArray[octave].startTime);
        }
        else if (velocity == 0)
        {
            midiNoteTableArray[octave].velocity = 0;
            midiNoteTableArray[octave].note = 0;
            midiNoteTableArray[octave].startTime = 0;
        }
        xSemaphoreGive(midiNoteTableMutex);
    }
}

float LedHandler::calculateMidiDecay(unsigned long long startTime, int velocity, int note)
{
    if (startTime == 0)
    {
        return 0;
    }
    unsigned long long currentTime = micros();
    unsigned long long timeElapsed = currentTime - startTime;
    int decay = getDecayTime(note, velocity);
    //ESP_LOGI("LED", "Decay time: %d", decay);
    //ESP_LOGI("LED", "Time elapsed: %llu", timeElapsed);
    if (timeElapsed == 0)
    {
        return 1;
    }
    else if (timeElapsed > decay)
    {
        return 1;
    }
    else
    {
        float decayFactor = (float) timeElapsed / float(decay);
        return decayFactor;
    }
}

int LedHandler::getDecayTime(int midiNote, int velocity)
{
    int minNote = 21;     // Lowest note on an 88-key keyboard (A0)
    int maxNote = 108;    // Highest note on an 88-key keyboard (C8)
    float minDecay = 8.0; // Decay time for the lowest note in seconds
    float maxDecay = 2.0; // Decay time for the highest note in seconds

    // Linear interpolation formula
    float decayTime = (minDecay - ((midiNote - minNote) * (minDecay - maxDecay) / (maxNote - minNote))) * velocity / 127;
    return (int)decayTime * 1000000;
}

int LedHandler::getMidiNoteFromPosition(int position)
{
    return position + 21;
}

int LedHandler::getOctaveFromPosition(int position)
{
    return (getMidiNoteFromPosition(position) / OCTAVE) - 1;
}


void LedHandler::setMidiNoteTable(int index, midiNoteTable tableElement) {
    if (xSemaphoreTake(midiNoteTableMutex, portMAX_DELAY) == pdTRUE) {
        midiNoteTableArray[index] = tableElement;
        xSemaphoreGive(midiNoteTableMutex);
    }
}

// Thread-safe getter for midiNoteTableArray
midiNoteTable LedHandler::getMidiNoteTable(int index) {
    midiNoteTable tableElement;
    if (xSemaphoreTake(midiNoteTableMutex, portMAX_DELAY) == pdTRUE) {
        tableElement = midiNoteTableArray[index];
        xSemaphoreGive(midiNoteTableMutex);
    }
    return tableElement;
}

void LedHandler::getMidiNoteTableArray(midiNoteTable* buffer, size_t size) {
    if (size < sizeof(midiNoteTableArray)) {
        // Handle error: buffer is too small
        return;
    }
    if (xSemaphoreTake(midiNoteTableMutex, portMAX_DELAY) == pdTRUE) {
        memcpy(buffer, midiNoteTableArray, sizeof(midiNoteTableArray));
        xSemaphoreGive(midiNoteTableMutex);
    }
}