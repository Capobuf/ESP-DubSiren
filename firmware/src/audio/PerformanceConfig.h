#pragma once

// Project tuning points, not measured values from another instrument.
namespace PerformanceConfig {
constexpr float kClassicPitchHz[] = {110.0f, 220.0f, 440.0f};
constexpr float kClassicRateHz[] = {0.35f, 0.70f, 2.80f};
constexpr float kClassicDepthOctaves = 1.0f;
constexpr float kClassicDecayMs = 120.0f;

constexpr float kTuneMs = 12.0f;
constexpr float kDepthMs = 15.0f;
constexpr float kRateMs = 20.0f;
constexpr float kFeedbackMs = 18.0f;
constexpr float kEchoMs = 15.0f;
constexpr float kEchoCutMs = 3.0f;
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

constexpr float kEchoFilterQ = 1.0f;
constexpr float kFeedbackSaturationDrive = 1.35f;
constexpr float kFeedbackSaturationAsymmetry = 0.018f;
constexpr float kFeedbackSaturationGain = 0.72f;
constexpr float kEchoSpinFeedback = 1.05f;
}  // namespace PerformanceConfig
