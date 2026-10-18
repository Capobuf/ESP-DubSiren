#pragma once

#include <math.h>

// Finite articulation. Length is the time to silence, not an exponential tau.
class LickshotEnvelope {
public:
    void trigger() { age_ = 0; start_ = level_; active_ = true; }
    float ageMs(float sampleRate) const { return age_ * 1000.0f / sampleRate; }
    float next(float lengthMs, bool release, float sampleRate) {
        if (!active_) return 0.0f;
        const float age = ageMs(sampleRate);
        if (release) {
            level_ = fmaxf(0.0f, level_ - 1.0f / (.003f * sampleRate));
        } else if (age < 2.0f) {
            level_ = start_ + (1.0f - start_) * age / 2.0f;
        } else if (age < lengthMs) {
            const float progress = (age - 2.0f) / (lengthMs - 2.0f);
            level_ = (expf(-6.0f * progress) - expf(-6.0f)) / (1.0f - expf(-6.0f));
        } else {
            level_ = 0.0f;
        }
        ++age_;
        if ((release || age >= lengthMs) && level_ <= 0.0f) active_ = false;
        return level_;
    }
private:
    unsigned age_ = 0;
    float start_ = 0.0f;
    float level_ = 0.0f;
    bool active_ = false;
};
