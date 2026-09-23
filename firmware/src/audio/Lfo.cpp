#include "Lfo.h"

#include <math.h>

float Lfo::next(LfoShape shape, float rateHz, bool modUp, bool modDown,
                float sampleRate) {
    if (shape == LfoShape::Manual) {
        const float target = modUp == modDown ? 0.0f : (modUp ? 1.0f : -1.0f);
        const float coefficient = 1.0f / (0.04f * sampleRate);
        manualValue_ += (target - manualValue_) * coefficient;
        return manualValue_;
    }

    const float value = automaticValue(shape, phase_);
    phase_ += rateHz / sampleRate;
    if (phase_ >= 1.0f) {
        phase_ -= floorf(phase_);
    }
    return value;
}

float Lfo::automaticValue(LfoShape shape, float phase) {
    switch (shape) {
        case LfoShape::Triangle:
            return 1.0f - 4.0f * fabsf(phase - 0.5f);
        case LfoShape::Square:
            return phase < 0.5f ? 1.0f : -1.0f;
        case LfoShape::SawUp:
            return phase * 2.0f - 1.0f;
        case LfoShape::SawDown:
            return 1.0f - phase * 2.0f;
        case LfoShape::AsymUp:
            return phase < 0.75f ? -1.0f + phase * (2.0f / 0.75f)
                                 : 1.0f - (phase - 0.75f) * 8.0f;
        case LfoShape::AsymDown:
            return phase < 0.25f ? -1.0f + phase * 8.0f
                                 : 1.0f - (phase - 0.25f) * (2.0f / 0.75f);
        case LfoShape::Pulse25:
            return phase < 0.25f ? 1.0f : -1.0f;
        case LfoShape::Pulse75:
            return phase < 0.75f ? 1.0f : -1.0f;
        case LfoShape::Manual:
            return 0.0f;
    }
    return 0.0f;
}
