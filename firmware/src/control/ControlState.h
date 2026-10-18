#pragma once

#include <Arduino.h>
#include "audio/PerformanceConfig.h"

enum class SirenMode {
    Sine1,
    Sine2,
    TestTone,
    Square,
    Lickshot,
    Sonar,
    Syndrum,
    Gunshot,
    Saw,
    Triangle,
    Count,
};

enum class LickshotArticulation { Legacy, Shot, Roll, Horn };

enum class SyndrumWave {
    Sine,
    Triangle,
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

struct ActivationEvent {
    uint32_t timestampUs = 0;
    SirenMode mode = SirenMode::Sine1;
    LfoShape lfoShape = LfoShape::Classic;
    SyndrumWave syndrumWave = SyndrumWave::Sine;
    LickshotArticulation lickshotArticulation = LickshotArticulation::Legacy;
    float lickshotLengthMs = 150.0f;
    float lickshotPunchOctaves = 2.0f;
    float lickshotPitchMs = 70.0f;
    float decayMs = PerformanceConfig::kExtendedDecayMs;
    float syndrumSweepOctaves = 2.0f;
    bool modulationEnabled = true;
    bool retrigger = false;
};

constexpr unsigned kActivationEventCapacity = 8;

struct ControlState {
    PerformanceProfile profile = PerformanceProfile::Classic;
    ClassicPitch classicPitch = ClassicPitch::Mid;
    ClassicModulation classicModulation = ClassicModulation::Medium;
    float pitchPosition = 0.5f;
    float ratePosition = 0.375f;
    bool classicPitchUsesTune = false;
    bool classicRateUsesRate = false;
    bool pitchZonesActive = true;
    bool rateZonesActive = true;
    SirenMode mode = SirenMode::Sine1;
    float tuneHz = PerformanceConfig::kClassicPitchHz[1];
    LfoShape lfoShape = LfoShape::Classic;
    float lfoRateHz = 0.70f;
    float lfoDepthOctaves = 1.0f;
    LickshotArticulation lickshotArticulation = LickshotArticulation::Legacy;
    float lickshotLengthMs = 150.0f;
    float lickshotPunchOctaves = 2.0f;
    float lickshotPitchMs = 70.0f;
    float decayMs = PerformanceConfig::kExtendedDecayMs;
    float dryToneHz = 19000.0f;
    float filterContourOctaves = 0.0f;
    float filterContourMs = 150.0f;
    float sweepAmount = 0.0f;
    float sweepMs = 650.0f;
    SyndrumWave syndrumWave = SyndrumWave::Sine;
    float syndrumSweepOctaves = 2.0f;
    bool trigger = false;
    uint32_t activationSequence = 0;
    ActivationEvent activationEvents[kActivationEventCapacity];
    bool hold = false;
    bool modUp = false;
    bool modDown = false;
    int8_t liveTransposeSemitones = 0;
    bool modulationEnabled = true;
    bool retrigger = false;
    float delayMs = 360.0f;
    float feedback = 0.62f;
    float echoLevel = 0.45f;
    float echoColor = 0.0f;
    float highPassHz = 60.0f;
    float lowPassHz = 7000.0f;
    float echoFilterPosition = 0.0f;
    bool echoFilterFlat = true;
    bool echoCut = false;
    bool echoSpin = false;
    bool echoLoop = false;
    float masterVolume = 0.50f;
    bool voicingV2 = true;
};

inline const char *pitchName(ClassicPitch pitch) {
    static const char *names[] = {"LOW", "MID", "HIGH"};
    return names[static_cast<unsigned>(pitch)];
}

inline const char *rateName(ClassicModulation rate) {
    static const char *names[] = {"SLOW", "MID", "FAST", "MANUAL"};
    return names[static_cast<unsigned>(rate)];
}

inline const char *modeName(SirenMode mode) {
    static const char *names[] = {"SINE 1", "SINE 2", "TEST TONE", "SQUARE",
        "LICKSHOT", "SONAR", "SYNDRUM", "GUNSHOT", "SAW", "TRIANGLE"};
    return names[static_cast<unsigned>(mode)];
}

inline void recordActivation(ControlState &state, uint32_t timestampUs = micros()) {
    ActivationEvent &event = state.activationEvents[
        state.activationSequence % kActivationEventCapacity];
    event.mode = state.mode;
    event.timestampUs = timestampUs;
    event.lfoShape = state.lfoShape;
    event.syndrumWave = state.syndrumWave;
    event.lickshotArticulation = state.lickshotArticulation;
    event.lickshotLengthMs = state.lickshotLengthMs;
    event.lickshotPunchOctaves = state.lickshotPunchOctaves;
    event.lickshotPitchMs = state.lickshotPitchMs;
    event.decayMs = state.decayMs;
    event.syndrumSweepOctaves = state.syndrumSweepOctaves;
    event.modulationEnabled = state.modulationEnabled;
    event.retrigger = state.retrigger;
    ++state.activationSequence;
}

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
