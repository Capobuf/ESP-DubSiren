#pragma once

#include <Arduino.h>

enum class SirenMode {
    Sine1,
    Sine2,
    TestTone,
    Square,
};

enum class LfoShape {
    Classic,
    Triangle,
    Square,
    SawUp,
    SawDown,
    AsymUp,
    AsymDown,
    Pulse25,
    Pulse75,
    Manual,
};

enum class PerformanceProfile { Classic, Extended };
enum class ClassicPitch { Low, Mid, High };
enum class ClassicModulation { Slow, Medium, Fast, Manual };

struct ControlState {
    PerformanceProfile profile = PerformanceProfile::Extended;
    ClassicPitch classicPitch = ClassicPitch::Mid;
    ClassicModulation classicModulation = ClassicModulation::Medium;
    SirenMode mode = SirenMode::Sine1;
    float tuneHz = 220.0f;
    LfoShape lfoShape = LfoShape::Classic;
    float lfoRateHz = 0.70f;
    float lfoDepthOctaves = 1.0f;
    float decayMs = 120.0f;
    bool trigger = false;
    bool hold = false;
    bool modUp = false;
    bool modDown = false;
    float delayMs = 360.0f;
    float feedback = 0.62f;
    float echoLevel = 0.45f;
    float highPassHz = 60.0f;
    float lowPassHz = 7000.0f;
    bool echoCut = false;
    float masterVolume = 0.50f;
    bool voicingV2 = true;
};

class ControlStore {
public:
    ControlStore() : mutex_(xSemaphoreCreateMutex()) {}

    ~ControlStore() {
        if (mutex_ != nullptr) {
            vSemaphoreDelete(mutex_);
        }
    }

    ControlState snapshot() const {
        ControlState copy;
        if (mutex_ != nullptr &&
            xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
            copy = state_;
            xSemaphoreGive(mutex_);
        }
        return copy;
    }

    void set(const ControlState &state) {
        if (mutex_ != nullptr &&
            xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
            state_ = state;
            xSemaphoreGive(mutex_);
        }
    }

private:
    mutable SemaphoreHandle_t mutex_;
    ControlState state_;
};
