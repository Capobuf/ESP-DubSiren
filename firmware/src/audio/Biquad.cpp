#include "Biquad.h"

#include <Arduino.h>
#include <math.h>

Biquad::Biquad()
    : b0_(1.0f), b1_(0.0f), b2_(0.0f), a1_(0.0f), a2_(0.0f), z1_(0.0f),
      z2_(0.0f) {}

void Biquad::setHighPass(float cutoffHz, float sampleRate, float q) {
    const float omega = TWO_PI * cutoffHz / sampleRate;
    const float cosine = cosf(omega);
    const float alpha = sinf(omega) / (2.0f * q);
    setCoefficients((1.0f + cosine) * 0.5f, -(1.0f + cosine),
                    (1.0f + cosine) * 0.5f, 1.0f + alpha,
                    -2.0f * cosine, 1.0f - alpha);
}

void Biquad::setLowPass(float cutoffHz, float sampleRate, float q) {
    const float omega = TWO_PI * cutoffHz / sampleRate;
    const float cosine = cosf(omega);
    const float alpha = sinf(omega) / (2.0f * q);
    setCoefficients((1.0f - cosine) * 0.5f, 1.0f - cosine,
                    (1.0f - cosine) * 0.5f, 1.0f + alpha,
                    -2.0f * cosine, 1.0f - alpha);
}

float Biquad::process(float input) {
    const float output = b0_ * input + z1_;
    z1_ = b1_ * input - a1_ * output + z2_;
    z2_ = b2_ * input - a2_ * output;
    return output;
}

void Biquad::reset() {
    z1_ = 0.0f;
    z2_ = 0.0f;
}

void Biquad::setCoefficients(float b0, float b1, float b2, float a0,
                             float a1, float a2) {
    b0_ = b0 / a0;
    b1_ = b1 / a0;
    b2_ = b2 / a0;
    a1_ = a1 / a0;
    a2_ = a2 / a0;
}
