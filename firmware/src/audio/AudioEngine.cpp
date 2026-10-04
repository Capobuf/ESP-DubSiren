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
      feedback_(0.62f), echoLevel_(0.45f), echoCutWet_(1.0f),
      masterVolume_(0.50f),
      performanceBend_(0.0f),
      oscillatorFrequencyHz_(220.0f), lastDry_(0.0f), transitionStartDry_(0.0f),
      noiseState_(0x6d2b79f5u), previousMode_(SirenMode::Sine1),
      currentMode_(SirenMode::Sine1), modeTransitionRemaining_(0),
      modeInitialized_(false), previousTrigger_(false), previousHold_(false),
      capturedActivationSequence_(0), pendingActivationRead_(0),
      pendingActivationWrite_(0), pendingActivationCount_(0),
      eventGateRemaining_(0) {}

void AudioEngine::begin() {
    delay_.begin(360.0f, 60.0f, 7000.0f, Config::kSampleRate, true);
    tuneHz_.configure(PerformanceConfig::kTuneMs, Config::kSampleRate);
    depthOctaves_.configure(PerformanceConfig::kDepthMs, Config::kSampleRate);
    rateHz_.configure(PerformanceConfig::kRateMs, Config::kSampleRate);
    feedback_.configure(PerformanceConfig::kFeedbackMs, Config::kSampleRate);
    echoLevel_.configure(PerformanceConfig::kEchoMs, Config::kSampleRate);
    echoCutWet_.configure(PerformanceConfig::kEchoCutMs, Config::kSampleRate);
    masterVolume_.configure(PerformanceConfig::kMasterMs, Config::kSampleRate);
    performanceBend_.configure(PerformanceConfig::kPerformanceBendMs,
                               Config::kSampleRate);
    gunshotFilter_.setCutoff(PerformanceConfig::kGunshotFilterHz,
                             Config::kSampleRate);
}

bool AudioEngine::isTonalMode(SirenMode mode) {
    return mode == SirenMode::Sine1 || mode == SirenMode::Sine2 ||
           mode == SirenMode::TestTone || mode == SirenMode::Square ||
           mode == SirenMode::Lickshot || mode == SirenMode::Sonar ||
           mode == SirenMode::Saw || mode == SirenMode::Triangle;
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
                         controls.lowPassHz, Config::kSampleRate, controls.echoFilterFlat);
    const bool classic = controls.profile == PerformanceProfile::Classic;
    const int pitchIndex = static_cast<int>(controls.classicPitch);
    const int modulationIndex = static_cast<int>(controls.classicModulation);
    tuneHz_.setTarget(classic && !controls.classicPitchUsesTune
                          ? PerformanceConfig::kClassicPitchHz[pitchIndex]
                          : controls.tuneHz);
    depthOctaves_.setTarget(classic ? PerformanceConfig::kClassicDepthOctaves
                                    : controls.lfoDepthOctaves);
    rateHz_.setTarget(classic && modulationIndex < 3 && !controls.classicRateUsesRate
                          ? PerformanceConfig::kClassicRateHz[modulationIndex]
                          : controls.lfoRateHz);
    const bool echoLoop = !classic && controls.echoLoop;
    feedback_.setTarget(echoLoop
                            ? controls.feedback
                            : (!classic && controls.echoSpin
                                   ? PerformanceConfig::kEchoSpinFeedback
                                   : controls.feedback));
    if (controls.echoLevel <= 0.0f) echoLevel_.reset(0.0f);
    echoLevel_.setTarget(controls.echoLevel);
    if (controls.echoCut) echoCutWet_.reset(0.0f);
    echoCutWet_.setTarget(controls.echoCut ? 0.0f : 1.0f);
    masterVolume_.setTarget(controls.masterVolume);
    uint32_t unseenActivations =
        controls.activationSequence - capturedActivationSequence_;
    if (unseenActivations > kActivationEventCapacity) {
        capturedActivationSequence_ =
            controls.activationSequence - kActivationEventCapacity;
        unseenActivations = kActivationEventCapacity;
    }
    for (uint32_t offset = 1; offset <= unseenActivations; ++offset) {
        if (pendingActivationCount_ == kPendingActivationCapacity) break;
        const uint32_t sequence = capturedActivationSequence_ + offset;
        pendingActivations_[pendingActivationWrite_] = controls.activationEvents[
            (sequence - 1) % kActivationEventCapacity];
        pendingActivationWrite_ =
            (pendingActivationWrite_ + 1) % kPendingActivationCapacity;
        ++pendingActivationCount_;
    }
    capturedActivationSequence_ = controls.activationSequence;
    ActivationEvent activation;
    const bool queuedActivation = pendingActivationCount_ > 0;
    if (queuedActivation) {
        activation = pendingActivations_[pendingActivationRead_];
        pendingActivationRead_ =
            (pendingActivationRead_ + 1) % kPendingActivationCapacity;
        --pendingActivationCount_;
    }
    const SirenMode requestedMode = classic
        ? (controls.mode < SirenMode::Lickshot ? controls.mode : SirenMode::Sine1)
        : (queuedActivation ? activation.mode : controls.mode);
    const LfoShape shape = classic
        ? (controls.classicModulation == ClassicModulation::Manual
               ? LfoShape::Manual : LfoShape::Classic)
        : (queuedActivation ? activation.lfoShape : controls.lfoShape);
    const float decayMs = queuedActivation
        ? activation.decayMs
        : controls.decayMs;
    const bool manual = shape == LfoShape::Manual;
    const float bendTarget = classic || manual || controls.modUp == controls.modDown
                                 ? 0.0f
                                 : (controls.modUp ? 1.0f : -1.0f);
    performanceBend_.setTarget(bendTarget);
    const bool modulationEnabled = classic || (queuedActivation
        ? activation.modulationEnabled : controls.modulationEnabled);
    const bool triggerEdge = !previousTrigger_ && controls.trigger;
    const bool triggerEvent = triggerEdge ||
        queuedActivation;
    const bool holdEdge = !previousHold_ && controls.hold;
    const bool activationEdge = triggerEvent || holdEdge;
    if (activationEdge) sweepEnvelope_.trigger();
    // Only Extended preserves sub-block taps as queued notes. Classic follows the gate.
    if (classic) {
        eventGateRemaining_ = 0;
        pendingActivationCount_ = 0;
        pendingActivationRead_ = pendingActivationWrite_;
    } else if (triggerEvent) eventGateRemaining_ = Config::kBlockSamples;
    if (triggerEvent && requestedMode == SirenMode::Syndrum) {
        syndrumEnvelope_.trigger();
    }
    if (triggerEvent && requestedMode == SirenMode::Gunshot) {
        gunshotEnvelope_.trigger();
    }
    const bool retrigger = queuedActivation
        ? activation.retrigger : controls.retrigger;
    if (!classic && retrigger && activationEdge) {
        lfo_.reset(shape);
    }
    previousTrigger_ = controls.trigger;
    previousHold_ = controls.hold;
    if (!modeInitialized_) {
        currentMode_ = previousMode_ = requestedMode;
        modeInitialized_ = true;
    } else if (requestedMode != currentMode_) {
        previousMode_ = currentMode_;
        currentMode_ = requestedMode;
        transitionStartDry_ = lastDry_;
        modeTransitionRemaining_ = static_cast<unsigned>(
            PerformanceConfig::kModeTransitionMs * Config::kSampleRate * 0.001f);
    }

    for (size_t i = 0; i < count; ++i) {
        const float tune = tuneHz_.next();
        const float depth = depthOctaves_.next();
        const float rate = rateHz_.next();
        const float smoothedFeedback = feedback_.next();
        const float feedback = controls.feedback == 0.0f && !(controls.echoSpin && !classic)
            ? 0.0f : smoothedFeedback;
        const float echoLevel = echoLevel_.next();
        const float echoCutWet = echoCutWet_.next();
        const float master = masterVolume_.next();
        const float performanceBend = performanceBend_.next();
        const float livePitch = controls.liveTransposeSemitones / 12.0f;

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
        if (!classic && tonal && sweepAmount > 0.0f) {
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
        if ((classic || controls.voicingV2) && currentMode_ == SirenMode::Square) {
            oscillatorModulation = modulationEnabled
                                      ? (lfo_.gate() ? 1.0f : -1.0f)
                                      : 0.0f;
            frequencyTarget = clampFrequency(
                tune * exp2f(depth * oscillatorModulation + performanceBend + livePitch +
                             sweepPitch));
            frequencySmoothing = 0.00829871f;  // 2.5 ms anti-click slew.
        } else if ((classic || controls.voicingV2) &&
                   currentMode_ == SirenMode::TestTone) {
            frequencyTarget = clampFrequency(
                tune * exp2f(performanceBend + livePitch + sweepPitch));
        } else if (tonal) {
            frequencyTarget = clampFrequency(
                tune * exp2f(depth * modulation + performanceBend + livePitch + sweepPitch));
        }
        if (tonal) {
            oscillatorFrequencyHz_ +=
                (frequencyTarget - oscillatorFrequencyHz_) * frequencySmoothing;
        }
        const bool eventGate = eventGateRemaining_ > 0;
        if (eventGateRemaining_ > 0) --eventGateRemaining_;
        const float envelopeLevel = tonal
            ? (classic ? envelope_.nextGate(controls.trigger || controls.hold, Config::kSampleRate)
                       : envelope_.next(controls.trigger || controls.hold || eventGate, decayMs,
                                        Config::kSampleRate))
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
                Config::kSampleRate, classic || controls.voicingV2,
                classic) * envelopeLevel;
        } else {
            if (tonal) {
                dry = oscillator_.next(currentMode_, oscillatorFrequencyHz_,
                    oscillatorModulation, envelopeLevel,
                    modulationEnabled ? lfo_.gate() : true,
                    Config::kSampleRate, classic || controls.voicingV2,
                    classic) * envelopeLevel;
            } else if (currentMode_ == SirenMode::Syndrum) {
                const float frequency = clampFrequency(
                    tune * exp2f(livePitch + (queuedActivation
                        ? activation.syndrumSweepOctaves
                        : controls.syndrumSweepOctaves) * syndrumLevel));
                dry = oscillator_.nextSyndrum(
                    queuedActivation ? activation.syndrumWave
                                     : controls.syndrumWave, frequency,
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
        if (classic && envelopeLevel == 0.0f) dry = 0.0f;
        lastDry_ = dry;

        const float wet = delay_.process(dry, feedback, echoLoop,
                                         Config::kSampleRate);
        const float mixed = dry + wet * echoLevel * echoCutWet;
        output[i] = toInt16(softClip(mixed * master));
    }
}
