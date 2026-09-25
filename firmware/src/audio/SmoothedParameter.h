#pragma once

#include <math.h>

class SmoothedParameter {
public:
    explicit SmoothedParameter(float initial = 0.0f)
        : current_(initial), target_(initial), coefficient_(1.0f) {}

    void configure(float milliseconds, float sampleRate) {
        coefficient_ = milliseconds <= 0.0f ? 1.0f
            : 1.0f - expf(-1000.0f / (milliseconds * sampleRate));
    }
    void reset(float value) { current_ = target_ = value; }
    void setCurrent(float value) { current_ = value; }
    void setTarget(float value) { target_ = value; }
    float next() {
        current_ += (target_ - current_) * coefficient_;
        return current_;
    }
    float current() const { return current_; }
    float target() const { return target_; }

private:
    float current_;
    float target_;
    float coefficient_;
};
