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
      performanceBend_(0.0f),
      oscillatorFrequencyHz_(220.0f), lastDry_(0.0f), transitionStartDry_(0.0f),
      noiseState_(0x6d2b79f5u), previousMode_(SirenMode::Sine1),
      currentMode_(SirenMode::Sine1), modeTransitionRemaining_(0),
      modeInitialized_(false), previousTrigger_(false), previousHold_(false) {}

void AudioEngine::begin() {
    delay_.begin(360.0f, 60.0f, 7000.0f, Config::kSampleRate);
    tuneHz_.configure(PerformanceConfig::kTuneMs, Config::kSampleRate);
    depthOctaves_.configure(PerformanceConfig::kDepthMs, Config::kSampleRate);
    rateHz_.configure(PerformanceConfig::kRateMs, Config::kSampleRate);
    feedback_.configure(PerformanceConfig::kFeedbackMs, Config::kSampleRate);
    echoLevel_.configure(PerformanceConfig::kEchoMs, Config::kSampleRate);
    masterVolume_.configure(PerformanceConfig::kMasterMs, Config::kSampleRate);
    performanceBend_.configure(PerformanceConfig::kPerformanceBendMs,
                               Config::kSampleRate);
    gunshotFilter_.setCutoff(PerformanceConfig::kGunshotFilterHz,
                             Config::kSampleRate);
}

bool AudioEngine::isTonalMode(SirenMode mode) {
    return mode == SirenMode::Sine1 || mode == SirenMode::Sine2 ||
           mode == SirenMode::TestTone || mode == SirenMode::Square ||
           mode == SirenMode::Lickshot || mode == SirenMode::Sonar;
}

float AudioEngine::nextGunshot(float envelopeLevel) {
    noiseState_ ^= noiseState_ << 13;
    noiseState_ ^= noiseState_ >> 17;
    noiseState_ ^= noiseState_ << 5;
    const float raw = static_cast<float>(noiseState_ & 0x00ffffffu) *
                          (2.0f / 16777215.0f) -
                      1.0f;
    const float shaped = 0.68f * raw + 0.32f * gunshotFilter_.process(raw);
    return shaped * envelopeLevel * PerformanceConfig::kGunshotOutputGain;
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
    feedback_.setTarget(controls.echoLoop
                            ? controls.feedback
                            : (controls.echoSpin
                                   ? PerformanceConfig::kEchoSpinFeedback
                                   : controls.feedback));
    echoLevel_.setTarget(controls.echoLevel);
    masterVolume_.setTarget(controls.masterVolume);
    const LfoShape shape = classic
        ? (controls.classicModulation == ClassicModulation::Manual
               ? LfoShape::Manual : LfoShape::Classic)
        : controls.lfoShape;
    const float decayMs = classic ? PerformanceConfig::kClassicDecayMs
                                  : controls.decayMs;
    const bool manual = shape == LfoShape::Manual;
    const float bendTarget = manual || controls.modUp == controls.modDown
                                 ? 0.0f
                                 : (controls.modUp ? 1.0f : -1.0f);
    performanceBend_.setTarget(bendTarget);
    const bool modulationEnabled = classic || controls.modulationEnabled;
    const bool triggerEdge = !previousTrigger_ && controls.trigger;
    const bool holdEdge = !previousHold_ && controls.hold;
    const bool activationEdge = triggerEdge || holdEdge;
    if (activationEdge) sweepEnvelope_.trigger();
    if (triggerEdge && controls.mode == SirenMode::Syndrum) {
        syndrumEnvelope_.trigger();
    }
    if (triggerEdge && controls.mode == SirenMode::Gunshot) {
        gunshotEnvelope_.trigger();
    }
    if (controls.retrigger && activationEdge) {
        lfo_.reset(shape);
    }
    previousTrigger_ = controls.trigger;
    previousHold_ = controls.hold;
    if (!modeInitialized_) {
        currentMode_ = previousMode_ = controls.mode;
        modeInitialized_ = true;
    } else if (controls.mode != currentMode_) {
        previousMode_ = currentMode_;
        currentMode_ = controls.mode;
        transitionStartDry_ = lastDry_;
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
        const float performanceBend = performanceBend_.next();

        const float sweepLevel = sweepEnvelope_.next(
            controls.sweepMs, Config::kSampleRate);
        const float syndrumLevel = syndrumEnvelope_.next(
            decayMs, Config::kSampleRate);
        float gunshotDecay = decayMs;
        if (gunshotDecay < PerformanceConfig::kGunshotMinDecayMs) {
            gunshotDecay = PerformanceConfig::kGunshotMinDecayMs;
        } else if (gunshotDecay > PerformanceConfig::kGunshotMaxDecayMs) {
            gunshotDecay = PerformanceConfig::kGunshotMaxDecayMs;
        }
        const float gunshotLevel = gunshotEnvelope_.next(
            gunshotDecay, Config::kSampleRate);
        const bool tonal = isTonalMode(currentMode_);
        const float sweepAmount = controls.sweepAmount < 0.0f
                                      ? 0.0f
                                      : (controls.sweepAmount > 1.0f
                                             ? 1.0f
                                             : controls.sweepAmount);
        float sweepPitch = 0.0f;
        float effectiveRate = rate;
        if (tonal && sweepAmount > 0.0f) {
            sweepPitch = -sweepLevel * sweepAmount *
                         PerformanceConfig::kSweepPitchOctaves;
            effectiveRate = rate * exp2f(
                -sweepLevel * sweepAmount *
                PerformanceConfig::kSweepRateOctaves);
        }
        const float lfoValue = tonal
            ? lfo_.next(shape, effectiveRate, controls.modUp,
                        controls.modDown, Config::kSampleRate)
            : 0.0f;
        const float modulation = tonal && modulationEnabled ? lfoValue : 0.0f;
        float frequencyTarget = tune;
        float oscillatorModulation = modulation;
        float frequencySmoothing = 0.0137929f;  // 1.5 ms RC at 48 kHz.
        if (controls.voicingV2 && currentMode_ == SirenMode::Square) {
            oscillatorModulation = modulationEnabled
                                      ? (lfo_.gate() ? 1.0f : -1.0f)
                                      : 0.0f;
            frequencyTarget = clampFrequency(
                tune * exp2f(depth * oscillatorModulation + performanceBend +
                             sweepPitch));
            frequencySmoothing = 0.00829871f;  // 2.5 ms anti-click slew.
        } else if (controls.voicingV2 &&
                   currentMode_ == SirenMode::TestTone) {
            frequencyTarget = clampFrequency(
                tune * exp2f(performanceBend + sweepPitch));
        } else if (tonal) {
            frequencyTarget = clampFrequency(
                tune * exp2f(depth * modulation + performanceBend + sweepPitch));
        }
        if (tonal) {
            oscillatorFrequencyHz_ +=
                (frequencyTarget - oscillatorFrequencyHz_) * frequencySmoothing;
        }
        const float envelopeLevel = tonal
            ? envelope_.next(controls.trigger || controls.hold, decayMs,
                             Config::kSampleRate)
            : 0.0f;
        float dry;
        const bool tonalTransition = modeTransitionRemaining_ > 0 && tonal &&
                                     isTonalMode(previousMode_);
        if (tonalTransition) {
            const float mix = 1.0f - static_cast<float>(modeTransitionRemaining_) /
                (PerformanceConfig::kModeTransitionMs * Config::kSampleRate * 0.001f);
            dry = oscillator_.nextTransition(previousMode_, currentMode_, mix,
                oscillatorFrequencyHz_, oscillatorModulation, envelopeLevel,
                modulationEnabled ? lfo_.gate() : true,
                Config::kSampleRate, controls.voicingV2) * envelopeLevel;
        } else {
            if (tonal) {
                dry = oscillator_.next(currentMode_, oscillatorFrequencyHz_,
                    oscillatorModulation, envelopeLevel,
                    modulationEnabled ? lfo_.gate() : true,
                    Config::kSampleRate, controls.voicingV2) * envelopeLevel;
            } else if (currentMode_ == SirenMode::Syndrum) {
                const float frequency = clampFrequency(
                    tune * exp2f(controls.syndrumSweepOctaves * syndrumLevel));
                dry = oscillator_.nextSyndrum(controls.syndrumWave, frequency,
                                              Config::kSampleRate) *
                      syndrumLevel;
            } else {
                dry = nextGunshot(gunshotLevel);
            }
            if (modeTransitionRemaining_ > 0) {
                const float mix = 1.0f -
                    static_cast<float>(modeTransitionRemaining_) /
                    (PerformanceConfig::kModeTransitionMs *
                     Config::kSampleRate * 0.001f);
                dry = transitionStartDry_ + (dry - transitionStartDry_) * mix;
            }
        }
        if (modeTransitionRemaining_ > 0) --modeTransitionRemaining_;
        lastDry_ = dry;

        const float wet = delay_.process(dry, feedback, controls.echoLoop,
                                         Config::kSampleRate);
        const float mixed = dry + (controls.echoCut ? 0.0f : wet * echoLevel);
        output[i] = toInt16(softClip(mixed * master));
    }
}
