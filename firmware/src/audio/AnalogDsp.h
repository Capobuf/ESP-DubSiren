#pragma once

#include <Arduino.h>
#include <math.h>

class OnePoleLowPass {
public:
    OnePoleLowPass() : state_(0.0f), cutoffHz_(-1.0f), coefficient_(1.0f) {}

    void setCutoff(float cutoffHz, float sampleRate) {
        const float maximum = sampleRate * 0.45f;
        const float clamped =
            cutoffHz < 5.0f ? 5.0f : (cutoffHz > maximum ? maximum : cutoffHz);
        // Tracking changes continuously, but small changes can reuse the cached
        // coefficient and avoid evaluating expf() for every audio sample.
        if (cutoffHz_ > 0.0f && fabsf(clamped - cutoffHz_) < cutoffHz_ * 0.002f) {
            return;
        }
        cutoffHz_ = clamped;
        coefficient_ = 1.0f - expf(-TWO_PI * cutoffHz_ / sampleRate);
    }

    float process(float input) {
        state_ += coefficient_ * (input - state_);
        return state_;
    }

    void reset() { state_ = 0.0f; }

private:
    float state_;
    float cutoffHz_;
    float coefficient_;
};

class DcBlocker {
public:
    DcBlocker() : previousInput_(0.0f), previousOutput_(0.0f) {}

    float process(float input) {
        // At 48 kHz, R=0.9995 puts the corner near 3.8 Hz: DC and very slow
        // bias movement are removed without thinning the audible siren range.
        constexpr float kR = 0.9995f;
        const float output = input - previousInput_ + kR * previousOutput_;
        previousInput_ = input;
        previousOutput_ = output;
        return output;
    }

    void reset() {
        previousInput_ = 0.0f;
        previousOutput_ = 0.0f;
    }

private:
    float previousInput_;
    float previousOutput_;
};

inline float polyBlep(float phase, float phaseIncrement) {
    if (phaseIncrement <= 0.0f) return 0.0f;
    if (phase < phaseIncrement) {
        phase /= phaseIncrement;
        return phase + phase - phase * phase - 1.0f;
    }
    if (phase > 1.0f - phaseIncrement) {
        phase = (phase - 1.0f) / phaseIncrement;
        return phase * phase + phase + phase + 1.0f;
    }
    return 0.0f;
}

inline float bandLimitedPulse(float phase, float phaseIncrement, float duty) {
    const float minimumDuty = phaseIncrement < 0.49f ? phaseIncrement : 0.49f;
    const float maximumDuty = 1.0f - minimumDuty;
    duty = duty < minimumDuty ? minimumDuty
                              : (duty > maximumDuty ? maximumDuty : duty);

    float output = phase < duty ? 1.0f : -1.0f;
    output += polyBlep(phase, phaseIncrement);

    float fallingPhase = phase - duty;
    if (fallingPhase < 0.0f) fallingPhase += 1.0f;
    output -= polyBlep(fallingPhase, phaseIncrement);
    return output;
}
