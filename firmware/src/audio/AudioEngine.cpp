#include "AudioEngine.h"

#include <math.h>

#include "Config.h"
#include "DspUtils.h"
#include "PerformanceConfig.h"

static_assert(Config::kSampleRate == 48000,
              "Delay::kBufferSamples and fixed smoothing coefficients assume 48 kHz");

namespace {

float clampFrequency(float frequency) {
    const float maximum = Config::kSampleRate * 0.45f;
    return frequency < 10.0f ? 10.0f : (frequency > maximum ? maximum : frequency);
}

}  // namespace

AudioEngine::AudioEngine()
    : tuneHz_(220.0f), depthOctaves_(1.0f), rateHz_(0.70f),
      feedback_(0.62f), echoLevel_(0.45f),
      masterVolume_(0.50f),
      performanceBend_(0.0f),
      oscillatorFrequencyHz_(220.0f), lastDry_(0.0f), transitionStartDry_(0.0f),
      noiseState_(0x6d2b79f5u), previousMode_(SirenMode::Sine1),
      currentMode_(SirenMode::Sine1), modeTransitionRemaining_(0),
      modeInitialized_(false), previousTrigger_(false), previousHold_(false),
      eventGateRemaining_(0) {}

IRAM_ATTR void AudioEngine::begin() {
    dryToneHz_.configure(10.0f, Config::kSampleRate);
    contourOctaves_.configure(10.0f, Config::kSampleRate);
    dryFilterMix_.configure(5.0f, Config::kSampleRate);
    dryFilter_.setLowPass(19000.0f, Config::kSampleRate, .7071f);
    delay_.begin(360.0f, 60.0f, 7000.0f, Config::kSampleRate, true);
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

IRAM_ATTR bool AudioEngine::isTonalMode(SirenMode mode) {
    return mode == SirenMode::Sine1 || mode == SirenMode::Sine2 ||
           mode == SirenMode::TestTone || mode == SirenMode::Square ||
           mode == SirenMode::Lickshot || mode == SirenMode::Sonar ||
           mode == SirenMode::Saw || mode == SirenMode::Triangle;
}

IRAM_ATTR float AudioEngine::nextGunshot(float envelopeLevel, float tuneHz) {
    noiseState_ ^= noiseState_ << 13;
    noiseState_ ^= noiseState_ >> 17;
    noiseState_ ^= noiseState_ << 5;
    const float raw = static_cast<float>(noiseState_ & 0x00ffffffu) *
                          (2.0f / 16777215.0f) -
                      1.0f;
    // Pitch sizes the shot: it scales the thump and the darkness of the tail.
    float size = tuneHz / PerformanceConfig::kGunshotReferenceHz;
    size = size < 0.5f ? 0.5f : (size > 2.5f ? 2.5f : size);
    gunshotFilter_.setCutoff(PerformanceConfig::kGunshotFilterHz * size, Config::kSampleRate);
    // Full-band noise at the hit, low-passed in the tail.
    const float bright = envelopeLevel * envelopeLevel;
    const float dark = gunshotFilter_.process(raw);
    const float noise = dark + (raw - dark) * bright;
    gunshotPhase_ += size * (PerformanceConfig::kGunshotBodyLowHz +
                             PerformanceConfig::kGunshotBodySweepHz * bright) / Config::kSampleRate;
    if (gunshotPhase_ >= 1.0f) gunshotPhase_ -= 1.0f;
    const float body = sinf(kTwoPi * gunshotPhase_);
    return fastTanh(PerformanceConfig::kGunshotDrive * envelopeLevel *
                    (PerformanceConfig::kGunshotNoiseMix * noise +
                     PerformanceConfig::kGunshotBodyMix * body)) *
           PerformanceConfig::kGunshotOutputGain;
}

IRAM_ATTR void AudioEngine::render(const ControlState &controls, int16_t *output, size_t count) {
    if (count == 0) return;
    uint32_t unseen = controls.activationSequence - capturedActivationSequence_;
    if (unseen > kActivationEventCapacity) {
        droppedActivations_ += unseen - kActivationEventCapacity;
        unseen = kActivationEventCapacity;
    }
    uint32_t firstSequence = controls.activationSequence - unseen;
    capturedActivationSequence_ = controls.activationSequence;
    if (unseen == 0) {
        renderSegment(controls, nullptr, output, count);
        return;
    }
    const uint32_t newestTime = controls.activationEvents[(controls.activationSequence - 1) % kActivationEventCapacity].timestampUs;
    const uint32_t windowUs = static_cast<uint32_t>(count * 1000000ull / Config::kSampleRate);
    while (unseen > 1 && newestTime - controls.activationEvents[firstSequence % kActivationEventCapacity].timestampUs >= windowUs) {
        ++firstSequence; --unseen; ++droppedActivations_;
    }
    // Play the captured batch in this block, preserving relative press intervals.
    // Never turn a stalled controller/overflow into a delayed multi-block roll.
    const uint32_t firstTime = controls.activationEvents[firstSequence % kActivationEventCapacity].timestampUs;
    size_t cursor = 0;
    for (uint32_t n = 0; n < unseen; ++n) {
        const ActivationEvent &event = controls.activationEvents[(firstSequence + n) % kActivationEventCapacity];
        size_t end = count;
        if (n + 1 < unseen) {
            const auto &next = controls.activationEvents[(firstSequence + n + 1) % kActivationEventCapacity];
            end = static_cast<size_t>((uint64_t(next.timestampUs - firstTime) * Config::kSampleRate) / 1000000);
            if (end > count) end = count;
        }
        // Simultaneous events follow monophonic last-note priority.
        if (end <= cursor) { ++droppedActivations_; continue; }
        renderSegment(controls, &event, output + cursor, end - cursor);
        ++renderedActivations_;
        cursor = end;
    }
}

IRAM_ATTR void AudioEngine::renderSegment(const ControlState &liveControls, const ActivationEvent *event,
                                int16_t *output, size_t count) {
    ControlState controls = liveControls;
    if (event) static_cast<SoundState &>(controls) = *event;
    if (controls.tempo.echoSync) controls.delayMs = controls.tempo.echoMs;
    delay_.setParameters(controls.delayMs, controls.highPassHz,
                         controls.lowPassHz, Config::kSampleRate, controls.echoFilterFlat);
    if (controls.tempo.lfoSync) controls.lfoRateHz = controls.tempo.lfoHz;
    dryToneHz_.setTarget(controls.dryToneHz);
    contourOctaves_.setTarget(controls.filterContourOctaves);
    dryFilterMix_.setTarget(controls.dryToneHz < 19000.0f || controls.filterContourOctaves != 0.0f ? 1.0f : 0.0f);
    tuneHz_.setTarget(controls.tuneHz);
    depthOctaves_.setTarget(controls.lfoDepthOctaves);
    rateHz_.setTarget(controls.lfoRateHz);
    const bool echoLoop = controls.echoLoop;
    feedback_.setTarget(!echoLoop && controls.echoSpin ? PerformanceConfig::kEchoSpinFeedback
                                                       : controls.feedback);
    if (controls.echoLevel <= 0.0f) echoLevel_.reset(0.0f);
    echoLevel_.setTarget(controls.echoLevel);
    delay_.setColor(controls.echoColor);
    masterVolume_.setTarget(controls.masterVolume);
    const bool queuedActivation = event != nullptr;
    const SirenMode requestedMode = controls.mode;
    const LfoShape shape = effectiveLfoShape(controls);
    // SIREN and MANUAL replace Depth with their own fixed sweep.
    const bool siren = sirenShape(shape);
    const float decayMs = controls.decayMs;
    // Zero decay selects the finite anti-click gate instead of a tail.
    const bool gateEnvelope = decayMs <= 0.0f;
    const auto articulation = controls.lickshotArticulation;
    const bool articulatedLickshot = requestedMode == SirenMode::Lickshot && articulation != LickshotArticulation::Legacy;
    const bool lickshotTransient = articulatedLickshot && articulation != LickshotArticulation::Horn;
    const float lickshotLength = controls.lickshotLengthMs;
    const float lickshotPunch = controls.lickshotPunchOctaves;
    const float lickshotPitchMs = controls.lickshotPitchMs;
    const bool manual = shape == LfoShape::Manual;
    // MANUAL drives its sweep with Mod Up/Down; every other shape bends.
    const float bendTarget = manual || controls.modUp == controls.modDown
                                 ? 0.0f
                                 : (controls.modUp ? 1.0f : -1.0f);
    performanceBend_.setTarget(bendTarget);
    const bool modulationEnabled = controls.modulationEnabled;
    const bool triggerEdge = !previousTrigger_ && controls.trigger;
    const bool triggerEvent = triggerEdge ||
        queuedActivation;
    const bool holdEdge = !previousHold_ && controls.hold;
    const bool activationEdge = triggerEvent || holdEdge;
    if (activationEdge) { sweepEnvelope_.trigger(); filterContour_.trigger(); }
    if (activationEdge && lickshotTransient) {
        lickshotEnvelope_.trigger();
        lickshotRollPhase_ = 0.0f;
    }
    // A zero-duration tap has a finite minimum gate; gate releases still take 3 ms.
    if (triggerEvent) eventGateRemaining_ = gateEnvelope
        ? static_cast<unsigned>(PerformanceConfig::kGateAttackMs * Config::kSampleRate * .001f)
        : Config::kBlockSamples;
    if (triggerEvent && requestedMode == SirenMode::Syndrum) {
        syndrumEnvelope_.trigger();
    }
    if (triggerEvent && requestedMode == SirenMode::Gunshot) {
        gunshotEnvelope_.trigger();
        gunshotPhase_ = 0.0f;
    }
    const bool sonarRestart = requestedMode == SirenMode::Sonar &&
        (activationEdge || !modeInitialized_ || requestedMode != currentMode_);
    if (sonarRestart) oscillator_.resetSonarPulse();
    if (siren) {
        // The sweep always restarts from the bottom; tempo sync only sets its rate.
        if (manual) { if (activationEdge) lfo_.kickManual(); }
        else if (activationEdge || sonarRestart) lfo_.reset(shape);
    } else if (!controls.tempo.lfoSync &&
               (sonarRestart || (controls.retrigger && activationEdge))) {
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
        const float feedback = controls.feedback == 0.0f && !controls.echoSpin
            ? 0.0f : smoothedFeedback;
        const float echoLevel = echoLevel_.next();
        const float cutStep = 1.0f / (PerformanceConfig::kEchoCutMs * .001f * Config::kSampleRate);
        echoCutWet_ = controls.echoCut ? fmaxf(0.0f, echoCutWet_ - cutStep)
                                       : fminf(1.0f, echoCutWet_ + cutStep);
        if (echoCutWet_ < 0.00001f) echoCutWet_ = 0.0f;
        const float echoCutWet = echoCutWet_;
        const float master = masterVolume_.next();
        const float performanceBend = performanceBend_.next();

        const float sweepLevel = sweepEnvelope_.next(
            controls.sweepMs, Config::kSampleRate);
        const float syndrumLevel = syndrumEnvelope_.next(
            fmaxf(decayMs, PerformanceConfig::kGunshotMinDecayMs), Config::kSampleRate);
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
            if (!controls.tempo.lfoSync)
                effectiveRate = rate * exp2f(
                    -sweepLevel * sweepAmount *
                    PerformanceConfig::kSweepRateOctaves);
        }
        const float lfoValue = tonal
            ? lfo_.next(shape, effectiveRate, controls.modUp,
                        controls.modDown, Config::kSampleRate)
            : 0.0f;
        if (lickshotTransient && articulation == LickshotArticulation::Roll &&
            (controls.trigger || controls.hold)) {
            lickshotRollPhase_ += rate / Config::kSampleRate;
            if (lickshotRollPhase_ >= 1.0f) {
                lickshotRollPhase_ -= 1.0f;
                lickshotEnvelope_.trigger();
                filterContour_.trigger();
            }
        }
        const float lickshotPitch = lickshotTransient
            ? lickshotPunch * expf(-lickshotEnvelope_.ageMs(Config::kSampleRate) / lickshotPitchMs) : 0.0f;
        const float modulation = tonal && modulationEnabled && !articulatedLickshot ? lfoValue : 0.0f;
        float frequencyTarget = tune;
        float oscillatorModulation = modulation;
        float frequencySmoothing = 0.0137929f;  // 1.5 ms RC at 48 kHz.
        const bool sirenSweep = siren && modulationEnabled && !articulatedLickshot;
        if (sirenSweep && currentMode_ == SirenMode::Square) {
            // Two fixed notes switched by the LFO gate.
            oscillatorModulation = lfo_.gate() ? 1.0f : -1.0f;
            frequencyTarget = clampFrequency(
                tune * (lfo_.gate() ? 1.0f : PerformanceConfig::kClassicSquareLowRatio) *
                exp2f(performanceBend + sweepPitch));
            frequencySmoothing = 0.00829871f;  // 2.5 ms anti-click slew.
        } else if (controls.voicingV2 && currentMode_ == SirenMode::TestTone) {
            frequencyTarget = clampFrequency(
                tune * exp2f(performanceBend + sweepPitch));
        } else if (tonal && sirenSweep) {
            frequencyTarget = clampFrequency(
                tune * (PerformanceConfig::kSirenFloorRatio +
                        (1.0f - PerformanceConfig::kSirenFloorRatio) * (lfoValue + 1.0f) * 0.5f) *
                exp2f(performanceBend + sweepPitch));
        } else if (tonal) {
            frequencyTarget = clampFrequency(
                tune * exp2f(depth * modulation + performanceBend + sweepPitch + lickshotPitch));
        }
        if (tonal) {
            oscillatorFrequencyHz_ +=
                (frequencyTarget - oscillatorFrequencyHz_) * frequencySmoothing;
        }
        const bool eventGate = eventGateRemaining_ > 0;
        if (eventGateRemaining_ > 0) --eventGateRemaining_;
        float envelopeLevel = tonal
            ? (gateEnvelope ? envelope_.nextGate(controls.trigger || controls.hold || eventGate, Config::kSampleRate)
                            : envelope_.next(controls.trigger || controls.hold || eventGate, decayMs,
                                        Config::kSampleRate))
            : 0.0f;
        if (lickshotTransient) envelopeLevel = lickshotEnvelope_.next(lickshotLength,
            articulation == LickshotArticulation::Roll && !(controls.trigger || controls.hold || eventGate),
            Config::kSampleRate);
        float dry;
        const bool tonalTransition = modeTransitionRemaining_ > 0 && tonal &&
                                     isTonalMode(previousMode_);
        // Sonar's amplitude pulses keep running when pitch modulation is off.
        const bool voiceGate = modulationEnabled || currentMode_ == SirenMode::Sonar ||
            (tonalTransition && previousMode_ == SirenMode::Sonar)
                ? lfo_.gate() : true;
        if (tonalTransition) {
            const float mix = 1.0f - static_cast<float>(modeTransitionRemaining_) /
                (PerformanceConfig::kModeTransitionMs * Config::kSampleRate * 0.001f);
            dry = oscillator_.nextTransition(previousMode_, currentMode_, mix,
                oscillatorFrequencyHz_, oscillatorModulation, envelopeLevel,
                voiceGate, Config::kSampleRate,
                controls.voicingV2) * envelopeLevel;
        } else {
            if (tonal) {
                dry = oscillator_.next(currentMode_, oscillatorFrequencyHz_,
                    oscillatorModulation, envelopeLevel,
                    voiceGate, Config::kSampleRate,
                    controls.voicingV2) * envelopeLevel;
            } else if (currentMode_ == SirenMode::Syndrum) {
                const float frequency = clampFrequency(
                    tune * exp2f(controls.syndrumSweepOctaves * syndrumLevel));
                dry = oscillator_.nextSyndrum(
                    controls.syndrumWave, frequency, Config::kSampleRate) *
                      syndrumLevel;
            } else {
                dry = nextGunshot(gunshotLevel, tune);
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
        if (tonal && gateEnvelope && envelopeLevel == 0.0f) dry = 0.0f;
        const float contour = filterContour_.next(controls.filterContourMs, Config::kSampleRate);
        const float tone = dryToneHz_.next();
        const float contourAmount = contourOctaves_.next();
        if (dryFilterCountdown_-- == 0) {
            const float cutoff = fmaxf(50.0f, fminf(19000.0f, tone * exp2f(contourAmount * contour)));
            dryFilter_.setLowPass(cutoff, Config::kSampleRate, .7071f);
            dryFilterCountdown_ = 47;
        }
        float filterMix = dryFilterMix_.next();
        if (dryFilterMix_.target() == 0.0f && filterMix < .00001f) dryFilterMix_.reset(filterMix = 0.0f);
        const float filteredDry = dryFilter_.process(dry);
        if (filterMix > 0.0f) dry += (filteredDry - dry) * filterMix;
        lastDry_ = dry;

        const float wet = delay_.process(dry, feedback, echoLoop,
                                         Config::kSampleRate);
        const float mixed = dry + wet * echoLevel * echoCutWet;
        output[i] = toInt16(softClip(mixed * master));
    }
}
