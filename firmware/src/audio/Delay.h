#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Biquad.h"

class Delay {
public:
    static constexpr size_t kBufferSamples = 48000;

    Delay();
    void begin(float initialDelayMs, float highPassHz, float lowPassHz,
               float sampleRate);
    void setParameters(float delayMs, float highPassHz, float lowPassHz,
                       float sampleRate);
    float process(float dry, float feedback, float sampleRate);

private:
    int16_t buffer_[kBufferSamples];
    size_t writeIndex_;
    float delayMs_;
    float targetDelayMs_;
    float highPassHz_;
    float lowPassHz_;
    Biquad highPass_;
    Biquad lowPass_;
};
