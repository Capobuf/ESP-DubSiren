#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Delay.h"
#include "Envelope.h"
#include "Lfo.h"
#include "Oscillator.h"
#include "SmoothedParameter.h"
#include "control/ControlState.h"

class AudioEngine {
public:
    AudioEngine();
    void begin();
    void render(const ControlState &controls, int16_t *output, size_t count);

private:
    Oscillator oscillator_;
    Lfo lfo_;
    Envelope envelope_;
    Delay delay_;
    SmoothedParameter tuneHz_;
    SmoothedParameter depthOctaves_;
    SmoothedParameter rateHz_;
    SmoothedParameter feedback_;
    SmoothedParameter echoLevel_;
    SmoothedParameter masterVolume_;
    float oscillatorFrequencyHz_;
    SirenMode previousMode_;
    SirenMode currentMode_;
    unsigned modeTransitionRemaining_;
    bool modeInitialized_;
};
