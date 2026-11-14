#pragma once

#include "control/ControlState.h"

class Lfo {
public:
    Lfo();
    // SIREN and MANUAL return the capacitor-like pitch level mapped to -1..1.
    float next(LfoShape shape, float rateHz, bool modUp, bool modDown,
               float sampleRate);
    void reset(LfoShape shape);
    // A new trigger in MANUAL starts near the top without lowering the level.
    void kickManual();
    bool gate() const { return currentShape_ == LfoShape::Manual ? manualGate_ : phase_ < 0.5f; }

private:
    float nextSiren(bool manual, float rateHz, bool modUp, bool modDown,
                    float sampleRate);
    float transition(LfoShape shape, float value, float sampleRate);
    static float automaticValue(LfoShape shape, float phase);
    bool manualGate_ = false;
    float phase_;
    float sirenLevel_;
    LfoShape currentShape_;
    float lastOutput_;
    float shapeOffset_;
    unsigned shapeTransitionRemaining_;
};
