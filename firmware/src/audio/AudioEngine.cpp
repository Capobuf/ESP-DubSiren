#include "AudioEngine.h"

#include <math.h>

#include "Config.h"
#include "DspUtils.h"
#include "PerformanceConfig.h"

namespace {

float clampFrequency(float frequency) {
    const float maximum = Config::kSampleRate * 0.45f;
    return frequency < 10.0f ? 10.0f : (frequency > maximum ? maximum : frequency);
}

int16_t toInt16(float value) {
    const float limited = value < -1.0f ? -1.0f : (value > 1.0f ? 1.0f : value);
    return static_cast<int16_t>(lrintf(limited * 32767.0f));
}

}  // namespace

AudioEngine::AudioEngine()
    : tuneHz_(220.0f), depthOctaves_(1.0f), rateHz_(0.70f),
      feedback_(0.62f), echoLevel_(0.45f), masterVolume_(0.50f),
      oscillatorFrequencyHz_(220.0f), previousMode_(SirenMode::Sine1),
      currentMode_(SirenMode::Sine1), modeTransitionRemaining_(0),
      modeInitialized_(false) {}

void AudioEngine::begin() {
    delay_.begin(360.0f, 60.0f, 7000.0f, Config::kSampleRate);
    tuneHz_.configure(PerformanceConfig::kTuneMs, Config::kSampleRate);
    depthOctaves_.configure(PerformanceConfig::kDepthMs, Config::kSampleRate);
    rateHz_.configure(PerformanceConfig::kRateMs, Config::kSampleRate);
    feedback_.configure(PerformanceConfig::kFeedbackMs, Config::kSampleRate);
    echoLevel_.configure(PerformanceConfig::kEchoMs, Config::kSampleRate);
    masterVolume_.configure(PerformanceConfig::kMasterMs, Config::kSampleRate);
}

void AudioEngine::render(const ControlState &controls, int16_t *output,
                         size_t count) {
    delay_.setParameters(controls.delayMs, controls.highPassHz,
                         controls.lowPassHz, Config::kSampleRate);
    const bool classic = controls.profile == PerformanceProfile::Classic;
    const int pitchIndex = static_cast<int>(controls.classicPitch);
    const int modulationIndex = static_cast<int>(controls.classicModulation);
    tuneHz_.setTarget(classic ? PerformanceConfig::kClassicPitchHz[pitchIndex]
                              : controls.tuneHz);
    depthOctaves_.setTarget(classic ? PerformanceConfig::kClassicDepthOctaves
                                    : controls.lfoDepthOctaves);
    rateHz_.setTarget(classic && modulationIndex < 3
                          ? PerformanceConfig::kClassicRateHz[modulationIndex]
                          : controls.lfoRateHz);
    feedback_.setTarget(controls.feedback);
    echoLevel_.setTarget(controls.echoLevel);
    masterVolume_.setTarget(controls.masterVolume);
    const LfoShape shape = classic
        ? (controls.classicModulation == ClassicModulation::Manual
               ? LfoShape::Manual : LfoShape::Classic)
        : controls.lfoShape;
    const float decayMs = classic ? PerformanceConfig::kClassicDecayMs
                                  : controls.decayMs;
    if (!modeInitialized_) {
        currentMode_ = previousMode_ = controls.mode;
        modeInitialized_ = true;
    } else if (controls.mode != currentMode_) {
        previousMode_ = currentMode_;
        currentMode_ = controls.mode;
        modeTransitionRemaining_ = static_cast<unsigned>(
            PerformanceConfig::kModeTransitionMs * Config::kSampleRate * 0.001f);
    }

    for (size_t i = 0; i < count; ++i) {
        const float tune = tuneHz_.next();
        const float depth = depthOctaves_.next();
        const float rate = rateHz_.next();
        const float feedback = feedback_.next();
        const float echoLevel = echoLevel_.next();
        const float master = masterVolume_.next();

        const float modulation =
            lfo_.next(shape, rate, controls.modUp,
                      controls.modDown, Config::kSampleRate);
        float frequencyTarget;
        float oscillatorModulation = modulation;
        float frequencySmoothing = 0.0137929f;  // 1.5 ms RC at 48 kHz.
        if (controls.voicingV2 && controls.mode == SirenMode::Square) {
            oscillatorModulation = lfo_.gate() ? 1.0f : -1.0f;
            frequencyTarget = clampFrequency(
                tune * exp2f(depth * oscillatorModulation));
            frequencySmoothing = 0.00829871f;  // 2.5 ms anti-click slew.
        } else if (controls.voicingV2 &&
                   controls.mode == SirenMode::TestTone) {
            frequencyTarget = clampFrequency(tune);
        } else {
            frequencyTarget = clampFrequency(
                tune * exp2f(depth * modulation));
        }
        oscillatorFrequencyHz_ +=
            (frequencyTarget - oscillatorFrequencyHz_) * frequencySmoothing;
        const float envelopeLevel = envelope_.next(
            controls.trigger || controls.hold, decayMs,
            Config::kSampleRate);
        float dry;
        if (modeTransitionRemaining_ > 0) {
            const float mix = 1.0f - static_cast<float>(modeTransitionRemaining_) /
                (PerformanceConfig::kModeTransitionMs * Config::kSampleRate * 0.001f);
            dry = oscillator_.nextTransition(previousMode_, currentMode_, mix,
                oscillatorFrequencyHz_, oscillatorModulation, envelopeLevel,
                lfo_.gate(), Config::kSampleRate, controls.voicingV2);
            --modeTransitionRemaining_;
        } else {
            dry = oscillator_.next(currentMode_, oscillatorFrequencyHz_,
                oscillatorModulation, envelopeLevel, lfo_.gate(),
                Config::kSampleRate, controls.voicingV2);
        }
        dry *= envelopeLevel;

        const float wet = delay_.process(dry, feedback,
                                         Config::kSampleRate);
        const float mixed = dry + (controls.echoCut ? 0.0f : wet * echoLevel);
        output[i] = toInt16(softClip(mixed * master));
    }
}
