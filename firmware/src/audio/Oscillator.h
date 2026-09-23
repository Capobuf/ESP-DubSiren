#pragma once

#include "control/ControlState.h"

class Oscillator {
public:
    Oscillator() : phase_(0.0f) {}
    float next(SirenMode mode, float frequencyHz, float sampleRate);

private:
    static float polyBlep(float phase, float phaseIncrement);
    float phase_;
};
