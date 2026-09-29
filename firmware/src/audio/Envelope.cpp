#include "Envelope.h"

#include <math.h>
#include "PerformanceConfig.h"

Envelope::Envelope()
    : level_(0.0f), lastDecayMs_(-1.0f), releaseMultiplier_(0.0f) {}

float Envelope::nextGate(bool gate, float sampleRate) {
    const float duration = gate ? PerformanceConfig::kGateAttackMs
                                : PerformanceConfig::kGateReleaseMs;
    const float step = 1.0f / (duration * 0.001f * sampleRate);
    level_ = gate ? fminf(1.0f, level_ + step) : fmaxf(0.0f, level_ - step);
    return level_;
}

float Envelope::next(bool gate, float decayMs, float sampleRate) {
    if (gate) {
        const float coefficient = 1.0f / (0.004f * sampleRate);
        level_ += (1.0f - level_) * coefficient;
    } else if (decayMs <= 0.0f) {
        level_ = 0.0f;
    } else {
        if (decayMs != lastDecayMs_) {
            lastDecayMs_ = decayMs;
            releaseMultiplier_ = expf(-1.0f / (decayMs * 0.001f * sampleRate));
        }
        level_ *= releaseMultiplier_;
        if (level_ < 0.00001f) {
            level_ = 0.0f;
        }
    }
    return level_;
}

OneShotEnvelope::OneShotEnvelope()
    : level_(0.0f), lastDecayMs_(-1.0f), multiplier_(0.0f) {}

void OneShotEnvelope::trigger() {
    level_ = 1.0f;
}

float OneShotEnvelope::next(float decayMs, float sampleRate) {
    if (decayMs <= 0.0f || sampleRate <= 0.0f) {
        level_ = 0.0f;
        return level_;
    }
    if (decayMs != lastDecayMs_) {
        lastDecayMs_ = decayMs;
        multiplier_ = expf(-1.0f / (decayMs * 0.001f * sampleRate));
    }
    level_ *= multiplier_;
    if (level_ < 0.00001f) level_ = 0.0f;
    if (level_ > 1.0f) level_ = 1.0f;
    return level_;
}

void OneShotEnvelope::reset() {
    level_ = 0.0f;
}
