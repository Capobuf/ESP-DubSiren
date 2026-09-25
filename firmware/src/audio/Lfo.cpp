#include "Lfo.h"

#include <math.h>
#include "PerformanceConfig.h"

namespace {

// Internal RC proportions are intentionally slightly different. They are
// grouped here so they can become controls later without changing the model.
constexpr float kChargeRatio = 0.18f;
constexpr float kFallRatio = 0.24f;
constexpr float kManualReferenceCycleSeconds = 0.22f;

float rcCoefficient(float tauSeconds, float sampleRate) {
    const float safeTau = tauSeconds < 0.001f ? 0.001f : tauSeconds;
    return 1.0f - expf(-1.0f / (safeTau * sampleRate));
}

}  // namespace

Lfo::Lfo()
    : phase_(0.0f), manualValue_(0.0f), classicValue_(-1.0f),
      lastRateHz_(-1.0f), lastSampleRate_(-1.0f), chargeCoefficient_(0.0f),
      fallCoefficient_(0.0f), manualChargeCoefficient_(0.0f),
      manualFallCoefficient_(0.0f), currentShape_(LfoShape::Classic),
      lastOutput_(-1.0f), shapeOffset_(0.0f), shapeTransitionRemaining_(0),
      coefficientCountdown_(0) {}

float Lfo::next(LfoShape shape, float rateHz, bool modUp, bool modDown,
                float sampleRate) {
    if (coefficientCountdown_ == 0) {
        updateCoefficients(rateHz, sampleRate);
        coefficientCountdown_ = PerformanceConfig::kLfoCoefficientUpdateSamples - 1;
    } else {
        --coefficientCountdown_;
    }

    if (shape == LfoShape::Manual) {
        const float target = modUp == modDown ? 0.0f : (modUp ? 1.0f : -1.0f);
        const float coefficient = target > manualValue_
                                      ? manualChargeCoefficient_
                                      : manualFallCoefficient_;
        manualValue_ += (target - manualValue_) * coefficient;
        return transition(shape, manualValue_, sampleRate);
    }

    float value;
    if (shape == LfoShape::Classic) {
        const float target = phase_ < 0.5f ? 1.0f : -1.0f;
        const float coefficient = target > classicValue_ ? chargeCoefficient_
                                                          : fallCoefficient_;
        classicValue_ += (target - classicValue_) * coefficient;
        value = classicValue_;
    } else {
        value = automaticValue(shape, phase_);
    }
    phase_ += rateHz / sampleRate;
    if (phase_ >= 1.0f) {
        phase_ -= floorf(phase_);
    }
    return transition(shape, value, sampleRate);
}

float Lfo::transition(LfoShape shape, float value, float sampleRate) {
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

void Lfo::updateCoefficients(float rateHz, float sampleRate) {
    if (rateHz == lastRateHz_ && sampleRate == lastSampleRate_) return;
    lastRateHz_ = rateHz;
    lastSampleRate_ = sampleRate;

    const float halfCycleSeconds = 0.5f / rateHz;
    chargeCoefficient_ =
        rcCoefficient(halfCycleSeconds * kChargeRatio, sampleRate);
    fallCoefficient_ =
        rcCoefficient(halfCycleSeconds * kFallRatio, sampleRate);
    manualChargeCoefficient_ = rcCoefficient(
        kManualReferenceCycleSeconds * kChargeRatio, sampleRate);
    manualFallCoefficient_ =
        rcCoefficient(kManualReferenceCycleSeconds * kFallRatio, sampleRate);
}

float Lfo::automaticValue(LfoShape shape, float phase) {
    switch (shape) {
        case LfoShape::Classic:
            return 0.0f;
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
