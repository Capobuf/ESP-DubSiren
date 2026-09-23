#pragma once

#include "control/ControlState.h"

class Lfo {
public:
    Lfo() : phase_(0.0f), manualValue_(0.0f) {}
    float next(LfoShape shape, float rateHz, bool modUp, bool modDown,
               float sampleRate);
    bool gate() const { return phase_ < 0.5f; }

private:
    static float automaticValue(LfoShape shape, float phase);
    float phase_;
    float manualValue_;
};
