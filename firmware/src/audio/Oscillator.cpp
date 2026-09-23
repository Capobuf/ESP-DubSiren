#include "Oscillator.h"

#include <Arduino.h>
#include <math.h>

#include "DspUtils.h"

float Oscillator::next(SirenMode mode, float frequencyHz, float sampleRate) {
    const float increment = frequencyHz / sampleRate;
    float output = 0.0f;
    const float fundamental = sinf(phase_ * TWO_PI);

    switch (mode) {
        case SirenMode::Sine1:
        case SirenMode::TestTone:
            output = fundamental;
            break;
        case SirenMode::Sine2: {
            const float third = sinf(phase_ * TWO_PI * 3.0f);
            output = softClip((0.78f * fundamental + 0.22f * third) * 1.35f);
            break;
        }
        case SirenMode::Square:
            output = phase_ < 0.5f ? 1.0f : -1.0f;
            output += polyBlep(phase_, increment);
            output -= polyBlep(fmodf(phase_ + 0.5f, 1.0f), increment);
            break;
    }

    phase_ += increment;
    if (phase_ >= 1.0f) {
        phase_ -= floorf(phase_);
    }
    return output;
}

float Oscillator::polyBlep(float phase, float increment) {
    if (phase < increment) {
        phase /= increment;
        return phase + phase - phase * phase - 1.0f;
    }
    if (phase > 1.0f - increment) {
        phase = (phase - 1.0f) / increment;
        return phase * phase + phase + phase + 1.0f;
    }
    return 0.0f;
}
