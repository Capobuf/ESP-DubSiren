#pragma once

inline float softClip(float value) {
    if (value <= -1.0f) return -1.0f;
    if (value >= 1.0f) return 1.0f;
    return 1.5f * (value - value * value * value / 3.0f);
}
