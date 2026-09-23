#include "Envelope.h"

#include <math.h>

Envelope::Envelope()
    : level_(0.0f), lastDecayMs_(-1.0f), releaseMultiplier_(0.0f) {}

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
