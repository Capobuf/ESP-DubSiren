#include "Oscillator.h"

#include <Arduino.h>
#include <math.h>

#include "AnalogDsp.h"
#include "DspUtils.h"
#include "PerformanceConfig.h"

namespace {

using namespace PerformanceConfig;

float clampCutoff(float cutoff, float sampleRate) {
    const float maximum = sampleRate * 0.40f;
    return cutoff < 70.0f ? 70.0f : (cutoff > maximum ? maximum : cutoff);
}

}  // namespace

Oscillator::Oscillator() : phase_(0.0f), syndrumTriangle_(0.0f) {}

float Oscillator::next(SirenMode mode, float frequencyHz, float modulation,
                       float envelopeLevel, bool lfoGate, float sampleRate,
                       bool voicingV2) {
    const float output = renderVoice(mode, frequencyHz, modulation,
                                     envelopeLevel, lfoGate, sampleRate, voicingV2);
    advance(frequencyHz / sampleRate);
    return output;
}

float Oscillator::nextTransition(SirenMode from, SirenMode to, float mix,
                                 float frequencyHz, float modulation,
                                 float envelopeLevel, bool lfoGate,
                                 float sampleRate, bool voicingV2) {
    const float oldVoice = renderVoice(from, frequencyHz, modulation,
                                      envelopeLevel, lfoGate, sampleRate, voicingV2);
    const float newVoice = renderVoice(to, frequencyHz, modulation,
                                      envelopeLevel, lfoGate, sampleRate, voicingV2);
    advance(frequencyHz / sampleRate);
    return oldVoice + (newVoice - oldVoice) * mix;
}

float Oscillator::nextSyndrum(SyndrumWave wave, float frequencyHz,
                              float sampleRate) {
    const float increment = frequencyHz / sampleRate;
    float output = 0.0f;
    if (wave == SyndrumWave::Sine) {
        output = sinf(phase_ * TWO_PI);
    } else if (wave == SyndrumWave::Square) {
        output = bandLimitedPulse(phase_, increment, 0.5f) * 0.72f;
    } else {
        // Integrating the PolyBLEP square produces a low-alias triangle while
        // retaining the shared oscillator phase used by every carrier.
        const float square = bandLimitedPulse(phase_, increment, 0.5f);
        syndrumTriangle_ += square * 4.0f * increment;
        syndrumTriangle_ *= 0.99995f;
        if (syndrumTriangle_ > 1.0f) syndrumTriangle_ = 1.0f;
        if (syndrumTriangle_ < -1.0f) syndrumTriangle_ = -1.0f;
        output = syndrumTriangle_ * 0.88f;
    }
    advance(increment);
    return output;
}

float Oscillator::renderVoice(SirenMode mode, float frequencyHz,
                              float modulation, float envelopeLevel,
                              bool lfoGate, float sampleRate, bool voicingV2) {
    const float increment = frequencyHz / sampleRate;
    float output;
    if (mode == SirenMode::Lickshot) {
        output = nextLickshot(frequencyHz, modulation, envelopeLevel, increment,
                              sampleRate);
    } else if (mode == SirenMode::Sonar) {
        output = nextSonar();
    } else if (voicingV2 && mode == SirenMode::Sine1) {
        output = nextSine1(frequencyHz, modulation, envelopeLevel, increment,
                           sampleRate);
    } else if (voicingV2 && mode == SirenMode::Sine2) {
        output = nextSine2(frequencyHz, modulation, envelopeLevel, increment,
                           sampleRate);
    } else if (voicingV2 && mode == SirenMode::TestTone) {
        output = nextTestTone(frequencyHz, envelopeLevel, lfoGate, increment,
                              sampleRate);
    } else if (voicingV2 && mode == SirenMode::Square) {
        output = nextSquare(frequencyHz, modulation, envelopeLevel, increment,
                            sampleRate);
    } else {
        output = nextLegacy(mode, increment);
        if (mode == SirenMode::TestTone && !lfoGate) output = 0.0f;
    }

    return output;
}

void Oscillator::advance(float increment) {
    phase_ += increment;
    if (phase_ >= 1.0f) {
        phase_ -= floorf(phase_);
    }
}

float Oscillator::nextLegacy(SirenMode mode, float phaseIncrement) const {
    const float fundamental = sinf(phase_ * TWO_PI);
    switch (mode) {
        case SirenMode::Sine1:
        case SirenMode::TestTone:
            return fundamental;
        case SirenMode::Sine2: {
            const float third = sinf(phase_ * TWO_PI * 3.0f);
            return softClip((0.78f * fundamental + 0.22f * third) * 1.35f);
        }
        case SirenMode::Square:
            return bandLimitedPulse(phase_, phaseIncrement, 0.5f);
        case SirenMode::Lickshot:
        case SirenMode::Sonar:
        case SirenMode::Syndrum:
        case SirenMode::Gunshot:
        case SirenMode::Count:
            return 0.0f;
    }
    return 0.0f;
}

float Oscillator::nextSine1(float, float, float, float, float) {
    return sinf(phase_ * TWO_PI) * PerformanceConfig::kSine1Gain;
}

float Oscillator::nextSine2(float frequencyHz, float, float, float, float sampleRate) {
    // Explicit low-order harmonics keep the richer voice predictable and bounded.
    const float phase = phase_ * TWO_PI;
    float value = sinf(phase);
    if (frequencyHz * 2.0f < sampleRate * 0.45f)
        value += PerformanceConfig::kSine2Second * sinf(phase * 2.0f);
    if (frequencyHz * 3.0f < sampleRate * 0.45f)
        value += PerformanceConfig::kSine2Third * sinf(phase * 3.0f);
    return value * PerformanceConfig::kSine2Gain;
}

float Oscillator::nextTestTone(float, float, bool lfoGate, float, float sampleRate) {
    return sinf(phase_ * TWO_PI) * PerformanceConfig::kTestGain *
           testGate_.nextGate(lfoGate, sampleRate);
}

float Oscillator::nextSquare(float frequencyHz, float modulation,
                             float envelopeLevel, float phaseIncrement,
                             float sampleRate) {
    VoiceState &state = voices_[static_cast<int>(SirenMode::Square)];
    const float duty =
        kSquareVoicing.duty + modulation * kSquareVoicing.dutyModAmount;
    float value = bandLimitedPulse(phase_, phaseIncrement, duty);
    const float envelopeFactor = 0.97f + 0.03f * envelopeLevel;
    const float targetCutoff = clampCutoff(
        frequencyHz * kSquareVoicing.harmonicFactor * envelopeFactor,
        sampleRate);
    const float cutoffSmoothing = 1.0f / (0.003f * sampleRate);
    state.smoothedCutoff +=
        (targetCutoff - state.smoothedCutoff) * cutoffSmoothing;
    if (state.cutoffUpdateCountdown == 0) {
        state.filters[0].setCutoff(state.smoothedCutoff, sampleRate);
        state.cutoffUpdateCountdown = 15;
    } else {
        --state.cutoffUpdateCountdown;
    }
    value = state.filters[0].process(value);
    value = analogSaturate(value, kSquareVoicing.drive,
                           kSquareVoicing.asymmetry);
    return state.dcBlocker.process(value) * kSquareVoicing.outputGain;
}

float Oscillator::nextLickshot(float frequencyHz, float modulation,
                               float envelopeLevel, float phaseIncrement,
                               float sampleRate) {
    VoiceState &state = voices_[static_cast<int>(SirenMode::Lickshot)];
    const float duty = kLickshotVoicing.duty +
                       modulation * kLickshotVoicing.dutyModAmount;
    float value = bandLimitedPulse(phase_, phaseIncrement, duty);
    const float envelopeFactor = 0.96f + 0.04f * envelopeLevel;
    const float targetCutoff = clampCutoff(
        frequencyHz * kLickshotVoicing.harmonicFactor * envelopeFactor,
        sampleRate);
    const float cutoffSmoothing = 1.0f / (0.004f * sampleRate);
    state.smoothedCutoff +=
        (targetCutoff - state.smoothedCutoff) * cutoffSmoothing;
    if (state.cutoffUpdateCountdown == 0) {
        state.filters[0].setCutoff(state.smoothedCutoff, sampleRate);
        state.cutoffUpdateCountdown = 15;
    } else {
        --state.cutoffUpdateCountdown;
    }
    value = state.filters[0].process(value);
    value = analogSaturate(value, kLickshotVoicing.drive,
                           kLickshotVoicing.asymmetry);
    return state.dcBlocker.process(value) * kLickshotVoicing.outputGain;
}

float Oscillator::nextSonar() const {
    return sinf(phase_ * TWO_PI) * kSonarOutputGain;
}
