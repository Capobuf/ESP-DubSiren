#include "Lfo.h"

#include <math.h>
#include "DspUtils.h"
#include "PerformanceConfig.h"

Lfo::Lfo()
    : phase_(0.0f), sirenLevel_(PerformanceConfig::kSirenRestartLevel),
      currentShape_(LfoShape::Classic), lastOutput_(-1.0f), shapeOffset_(0.0f),
      shapeTransitionRemaining_(0) {}

IRAM_ATTR void Lfo::reset(LfoShape shape) {
    phase_ = 0.0f;
    manualGate_ = false;
    sirenLevel_ = shape == LfoShape::Manual ? PerformanceConfig::kSirenManualTriggerLevel
                                            : PerformanceConfig::kSirenRestartLevel;
    currentShape_ = shape;
    lastOutput_ = sirenShape(shape) ? sirenLevel_ * 2.0f - 1.0f
                                    : automaticValue(shape, phase_);
    shapeOffset_ = 0.0f;
    shapeTransitionRemaining_ = 0;
}

IRAM_ATTR void Lfo::kickManual() {
    if (sirenLevel_ < PerformanceConfig::kSirenManualTriggerLevel)
        sirenLevel_ = PerformanceConfig::kSirenManualTriggerLevel;
}

IRAM_ATTR float Lfo::next(LfoShape shape, float rateHz, bool modUp, bool modDown,
                float sampleRate) {
    if (sirenShape(shape))
        return transition(shape, nextSiren(shape == LfoShape::Manual, rateHz, modUp,
                                           modDown, sampleRate), sampleRate);
    const float value = automaticValue(shape, phase_);
    phase_ += rateHz / sampleRate;
    if (phase_ >= 1.0f) {
        phase_ -= floorf(phase_);
    }
    return transition(shape, value, sampleRate);
}

IRAM_ATTR float Lfo::nextSiren(bool manual, float rateHz, bool modUp, bool modDown,
                     float sampleRate) {
    using namespace PerformanceConfig;
    float target, tauSeconds;
    if (manual) {
        // Up pulls the pitch to the top, Down pushes it to the floor; with
        // neither held it sinks slowly.
        manualGate_ = modUp && !modDown;
        target = manualGate_ ? 1.0f : 0.0f;
        tauSeconds = (modUp != modDown ? kSirenManualAttackMs : kSirenManualDecayMs) * 0.001f;
    } else {
        const float rate = rateHz < 0.01f ? 0.01f : rateHz;
        const bool charging = phase_ < kSirenChargePhase;
        target = charging ? 1.0f : 0.0f;
        tauSeconds = (charging ? kSirenChargeTauCycles : kSirenDischargeTauCycles) / rate;
        phase_ += rate / sampleRate;
        if (phase_ >= 1.0f) phase_ -= floorf(phase_);
    }
    // 1 - exp(-x) ~= x: every time constant here spans thousands of samples.
    float coefficient = 1.0f / (tauSeconds * sampleRate);
    if (coefficient > 1.0f) coefficient = 1.0f;
    sirenLevel_ += (target - sirenLevel_) * coefficient;
    return sirenLevel_ * 2.0f - 1.0f;
}

IRAM_ATTR float Lfo::transition(LfoShape shape, float value, float sampleRate) {
    const unsigned duration = static_cast<unsigned>(
        PerformanceConfig::kLfoShapeTransitionMs * sampleRate * 0.001f);
    if (shape != currentShape_) {
        currentShape_ = shape;
        shapeOffset_ = lastOutput_ - value;
        shapeTransitionRemaining_ = duration;
    }
    if (shapeTransitionRemaining_ > 0) {
        value += shapeOffset_ * static_cast<float>(shapeTransitionRemaining_) /
                 static_cast<float>(duration);
        --shapeTransitionRemaining_;
    }
    lastOutput_ = value;
    return value;
}

IRAM_ATTR float Lfo::automaticValue(LfoShape shape, float phase) {
    switch (shape) {
        case LfoShape::Classic:
            return 0.0f;  // Rendered by nextSiren.
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
