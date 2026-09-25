#pragma once

#include "control/ControlState.h"

class Lfo {
public:
    Lfo();
    float next(LfoShape shape, float rateHz, bool modUp, bool modDown,
               float sampleRate);
    bool gate() const { return phase_ < 0.5f; }

private:
    void updateCoefficients(float rateHz, float sampleRate);
    float transition(LfoShape shape, float value, float sampleRate);
    static float automaticValue(LfoShape shape, float phase);
    float phase_;
    float manualValue_;
    float classicValue_;
    float lastRateHz_;
    float lastSampleRate_;
    float chargeCoefficient_;
    float fallCoefficient_;
    float manualChargeCoefficient_;
    float manualFallCoefficient_;
    LfoShape currentShape_;
    float lastOutput_;
    float shapeOffset_;
    unsigned shapeTransitionRemaining_;
    unsigned coefficientCountdown_;
};
