#pragma once

// Shared performance calibration and project tuning points.
namespace PerformanceConfig {
// Classic pitch calibration; all tuning points are project values.
constexpr float kClassicPitchHz[] = {261.63f, 392.00f, 587.33f};
constexpr float kClassicRateHz[] = {0.35f, 0.70f, 2.80f};
constexpr float kClassicDepthOctaves = 1.0f;
constexpr float kLfoChargeRatio = 0.18f;
constexpr float kLfoFallRatio = 0.24f;
constexpr float kSelectorHysteresis = 0.015f;
inline int selectorZone(float position, int current, int zones) {
    while (current < zones - 1 &&
           position > float(current + 1) / zones + kSelectorHysteresis) ++current;
    while (current > 0 &&
           position < float(current) / zones - kSelectorHysteresis) --current;
    return current;
}
constexpr float kTestGain = 0.72f;
// Canonical Extended oscillator levels. Keep the waveform itself neutral;
// character can be added downstream without baking an arbitrary filter into it.
constexpr float kSawGain = 0.72f;
constexpr float kTriangleGain = 0.72f;
constexpr float kManualAttackMs = 8.0f;
constexpr float kManualReleaseMs = 12.0f;
constexpr float kEchoTimeMinMs = 50.0f;
constexpr float kEchoTimeMaxMs = 1000.0f;
constexpr float kEchoFilterDeadband = 0.05f;
constexpr float kEchoHighPassMinHz = 60.0f;
constexpr float kEchoHighPassMaxHz = 4000.0f;
constexpr float kEchoLowPassMinHz = 400.0f;
constexpr float kEchoLowPassMaxHz = 16000.0f;

inline float echoFilterAmount(float position) {
    const float distance = position < 0.0f ? -position : position;
    return distance <= kEchoFilterDeadband ? 0.0f
        : (distance - kEchoFilterDeadband) / (1.0f - kEchoFilterDeadband);
}

struct SirenVoicing {
    int filterStages;
    float harmonicFactor;
    float duty;
    float dutyModAmount;
    float drive;
    float asymmetry;
    float outputGain;
};

// Internal tuning values: they describe this project's voicing, not any
// third-party circuit. Keep them together so listening tests stay simple.
constexpr SirenVoicing kSine1Voicing = {
    1, 6.00f, 0.360f, 0.090f, 1.03f, 0.008f, 0.56f,
};
constexpr float kSine1MinimumDuty = 0.270f;
constexpr float kSine1MaximumDuty = 0.450f;
constexpr SirenVoicing kSine2Voicing = {
    1, 8.00f, 0.240f, 0.050f, 1.04f, 0.012f, 0.54f,
};
constexpr float kSine2MinimumDuty = 0.190f;
constexpr float kSine2MaximumDuty = 0.290f;
constexpr SirenVoicing kSquareVoicing = {
    1, 7.50f, 0.470f, 0.006f, 1.06f, 0.018f, 0.56f,
};
// Extended voicing / envelope defaults.
constexpr SirenVoicing kLickshotVoicing = {
    1, 5.00f, 0.495f, 0.004f, 1.12f, 0.020f, 0.62f,
};
constexpr float kSonarOutputGain = 0.82f;
constexpr float kSonarPulseDecayMs = 80.0f;

constexpr float kExtendedDecayMs = 120.0f;

// DSP safety / anti-click times (finite linear gate, not RC time constants).
constexpr float kGateAttackMs = 2.0f;
constexpr float kGateReleaseMs = 3.0f;

constexpr float kTuneMs = 12.0f;
constexpr float kDepthMs = 15.0f;
constexpr float kRateMs = 20.0f;
constexpr float kFeedbackMs = 18.0f;
constexpr float kEchoMs = 15.0f;
constexpr float kEchoCutMs = 2.0f;
constexpr float kMasterMs = 15.0f;
constexpr float kPerformanceBendMs = 30.0f;
constexpr float kFilterMs = 18.0f;
constexpr float kDelayMs = 25.0f;
constexpr float kDelayMaxSlewMsPerSecond = 2000.0f;
constexpr float kLfoShapeTransitionMs = 10.0f;
constexpr float kModeTransitionMs = 6.0f;
constexpr float kSweepPitchOctaves = 2.0f;
constexpr float kSweepRateOctaves = 3.0f;
constexpr float kGunshotMinDecayMs = 20.0f;
constexpr float kGunshotMaxDecayMs = 1500.0f;
constexpr float kGunshotFilterHz = 1800.0f;
constexpr float kGunshotOutputGain = 0.42f;
constexpr unsigned kFilterUpdateSamples = 64;
constexpr unsigned kLfoCoefficientUpdateSamples = 64;

// Non-resonant HP/LP filters avoid adding gain inside the feedback loop.
constexpr float kEchoFilterQ = 0.70710678f;
constexpr float kFeedbackSaturationDrive = 1.35f;
constexpr float kFeedbackSaturationAsymmetry = 0.018f;
constexpr float kEchoSpinFeedback = 1.05f;
}  // namespace PerformanceConfig
