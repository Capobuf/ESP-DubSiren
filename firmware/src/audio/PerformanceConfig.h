#pragma once

// Shared performance calibration and project tuning points.
namespace PerformanceConfig {
// Pitch and rate zone calibration; all tuning points are project values.
// The selected pitch is the top of the sweep, not its centre.
constexpr float kClassicPitchHz[] = {330.0f, 680.0f, 1000.0f};
constexpr float kClassicRateHz[] = {0.70f, 3.00f, 6.50f};
// The SIREN and MANUAL shapes follow one capacitor-like level in 0..1, linear in Hz:
// frequency = pitch * (floor + (1 - floor) * level).
constexpr float kSirenFloorRatio = 0.12f;
// Automatic: charge for most of the cycle, then a short discharge.
constexpr float kSirenChargePhase = 0.82f;
constexpr float kSirenChargeTauCycles = 0.40f;
constexpr float kSirenDischargeTauCycles = 0.257f;
// A new trigger restarts the sweep from the bottom.
constexpr float kSirenRestartLevel = 0.15f;
// Manual: the button pulls the pitch up quickly, release lets it sink slowly.
constexpr float kSirenManualAttackMs = 120.0f;
constexpr float kSirenManualDecayMs = 3500.0f;
constexpr float kSirenManualTriggerLevel = 0.85f;
// Two-tone with the SIREN shape: the low note as a fraction of the selected pitch.
constexpr float kClassicSquareLowRatio = 0.62f;
constexpr float kSelectorHysteresis = 0.015f;
inline int selectorZone(float position, int current, int zones) {
    while (current < zones - 1 &&
           position > float(current + 1) / zones + kSelectorHysteresis) ++current;
    while (current > 0 &&
           position < float(current) / zones - kSelectorHysteresis) --current;
    return current;
}
constexpr float kTestGain = 0.72f;
// Canonical saw/triangle oscillator levels. Keep the waveform itself neutral;
// character can be added downstream without baking an arbitrary filter into it.
constexpr float kSawGain = 0.72f;
constexpr float kTriangleGain = 0.72f;
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
    6.00f, 0.360f, 0.090f, 1.03f, 0.008f, 0.56f,
};
constexpr float kSine1MinimumDuty = 0.270f;
constexpr float kSine1MaximumDuty = 0.450f;
constexpr SirenVoicing kSine2Voicing = {
    8.00f, 0.240f, 0.050f, 1.04f, 0.012f, 0.54f,
};
constexpr float kSine2MinimumDuty = 0.190f;
constexpr float kSine2MaximumDuty = 0.290f;
constexpr SirenVoicing kSquareVoicing = {
    7.50f, 0.470f, 0.006f, 1.06f, 0.018f, 0.56f,
};
// Lickshot and Sonar voicing, one-shot tail default.
constexpr SirenVoicing kLickshotVoicing = {
    5.00f, 0.495f, 0.004f, 1.12f, 0.020f, 0.62f,
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
// Gunshot: a noise crack that darkens to kGunshotFilterHz as it decays, over a
// thump falling from (low + sweep) to low, driven into saturation.
// These hold at kGunshotReferenceHz (the MID pitch zone) and scale with Pitch.
constexpr float kGunshotReferenceHz = 680.0f;
constexpr float kGunshotFilterHz = 1800.0f;
constexpr float kGunshotBodyLowHz = 45.0f;
constexpr float kGunshotBodySweepHz = 180.0f;
constexpr float kGunshotNoiseMix = 0.6f;
constexpr float kGunshotBodyMix = 0.9f;
constexpr float kGunshotDrive = 3.0f;
constexpr float kGunshotOutputGain = 0.75f;
constexpr unsigned kFilterUpdateSamples = 64;

// Non-resonant HP/LP filters avoid adding gain inside the feedback loop.
constexpr float kEchoFilterQ = 0.70710678f;
constexpr float kFeedbackSaturationDrive = 1.35f;
constexpr float kFeedbackSaturationAsymmetry = 0.018f;
constexpr float kEchoSpinFeedback = 1.05f;
}  // namespace PerformanceConfig
