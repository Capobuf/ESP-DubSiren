#pragma once

#include <math.h>
#include <stdint.h>

// arduino-esp32 defines TWO_PI as a double literal; keep the audio path in float.
constexpr float kTwoPi = 6.28318531f;

inline int16_t toInt16(float value) {
    const float limited = value < -1.0f ? -1.0f : (value > 1.0f ? 1.0f : value);
    return static_cast<int16_t>(lrintf(limited * 32767.0f));
}

inline float softClip(float value) {
    if (value <= -1.0f) return -1.0f;
    if (value >= 1.0f) return 1.0f;
    return 1.5f * (value - value * value * value / 3.0f);
}

inline float fastTanh(float value) {
    // Bounded rational approximation. In the voicing ranges used here it
    // tracks tanh closely while avoiding several costly transcendentals per
    // sample on the ESP32-S3.
    if (value <= -3.0f) return -1.0f;
    if (value >= 3.0f) return 1.0f;
    const float squared = value * value;
    return value * (27.0f + squared) / (27.0f + 9.0f * squared);
}

inline float analogSaturate(float value, float drive, float asymmetry) {
    const float zero = fastTanh(drive * asymmetry);
    const float positiveSpan = fastTanh(drive * (1.0f + asymmetry)) - zero;
    const float normalization = positiveSpan > 0.0001f ? 1.0f / positiveSpan : 1.0f;
    return (fastTanh(drive * (value + asymmetry)) - zero) * normalization;
}
