#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Biquad.h"
#include "SmoothedParameter.h"

class Delay {
public:
    static constexpr size_t kBufferSamples = 48000;

    Delay();
    void begin(float initialDelayMs, float highPassHz, float lowPassHz,
               float sampleRate, bool flat = false);
    void setParameters(float delayMs, float highPassHz, float lowPassHz,
                       float sampleRate, bool flat = false);
    void setColor(float amount) { color_.setTarget(amount); }
    float process(float dry, float feedback, bool echoLoop, float sampleRate);

private:
    int16_t buffer_[kBufferSamples];
    SmoothedParameter color_{0.0f};
    size_t writeIndex_;
    SmoothedParameter flatMix_{0.0f};
    SmoothedParameter delayMs_;
    SmoothedParameter highPassHz_;
    SmoothedParameter lowPassHz_;
    unsigned filterUpdateCountdown_;
    float appliedHighPassHz_;
    float appliedLowPassHz_;
    Biquad highPass_;
    Biquad lowPass_;
};
