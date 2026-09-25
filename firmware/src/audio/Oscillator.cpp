#include "Oscillator.h"

#include <Arduino.h>
#include <math.h>

#include "AnalogDsp.h"
#include "DspUtils.h"

namespace {

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
    3, 1.65f, 0.480f, 0.006f, 1.08f, 0.015f, 0.82f,
};
constexpr SirenVoicing kSine2Voicing = {
    2, 2.55f, 0.455f, 0.012f, 1.28f, 0.045f, 0.70f,
};
constexpr SirenVoicing kTestVoicing = {
    2, 2.00f, 0.500f, 0.000f, 1.02f, 0.000f, 0.78f,
};
constexpr SirenVoicing kSquareVoicing = {
    1, 7.50f, 0.470f, 0.006f, 1.06f, 0.018f, 0.56f,
};

float clampCutoff(float cutoff, float sampleRate) {
    const float maximum = sampleRate * 0.40f;
    return cutoff < 70.0f ? 70.0f : (cutoff > maximum ? maximum : cutoff);
}

}  // namespace

Oscillator::Oscillator() : phase_(0.0f) {}

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

float Oscillator::renderVoice(SirenMode mode, float frequencyHz,
                              float modulation, float envelopeLevel,
                              bool lfoGate, float sampleRate, bool voicingV2) {
    const float increment = frequencyHz / sampleRate;
    float output;
    if (voicingV2 && mode == SirenMode::Sine1) {
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
    }
    return 0.0f;
}

float Oscillator::nextSine1(float frequencyHz, float modulation,
                            float envelopeLevel, float phaseIncrement,
                            float sampleRate) {
    VoiceState &state = voices_[static_cast<int>(SirenMode::Sine1)];
    const float duty =
        kSine1Voicing.duty + modulation * kSine1Voicing.dutyModAmount;
    float value = bandLimitedPulse(phase_, phaseIncrement, duty);

    constexpr float kTone = 0.5f;
    const float toneFactor = 0.88f + 0.24f * kTone;
    const float envelopeFactor = 0.94f + 0.06f * envelopeLevel;
    const float targetCutoff = clampCutoff(
        frequencyHz * kSine1Voicing.harmonicFactor * toneFactor *
            envelopeFactor,
        sampleRate);
    const float cutoffSmoothing = 1.0f / (0.006f * sampleRate);
    state.smoothedCutoff +=
        (targetCutoff - state.smoothedCutoff) * cutoffSmoothing;

    if (state.cutoffUpdateCountdown == 0) {
        for (int stage = 0; stage < kSine1Voicing.filterStages; ++stage) {
            state.filters[stage].setCutoff(state.smoothedCutoff, sampleRate);
        }
        state.cutoffUpdateCountdown = 15;
    } else {
        --state.cutoffUpdateCountdown;
    }
    for (int stage = 0; stage < kSine1Voicing.filterStages; ++stage) {
        value = state.filters[stage].process(value);
    }
    value = analogSaturate(value, kSine1Voicing.drive,
                           kSine1Voicing.asymmetry);
    return state.dcBlocker.process(value) * kSine1Voicing.outputGain;
}

float Oscillator::nextSine2(float frequencyHz, float modulation,
                            float envelopeLevel, float phaseIncrement,
                            float sampleRate) {
    VoiceState &state = voices_[static_cast<int>(SirenMode::Sine2)];
    const float duty =
        kSine2Voicing.duty + modulation * kSine2Voicing.dutyModAmount;
    float value = bandLimitedPulse(phase_, phaseIncrement, duty);

    constexpr float kTone = 0.5f;
    const float toneFactor = 0.86f + 0.28f * kTone;
    const float envelopeFactor = 0.93f + 0.07f * envelopeLevel;
    const float targetCutoff = clampCutoff(
        frequencyHz * kSine2Voicing.harmonicFactor * toneFactor *
            envelopeFactor,
        sampleRate);
    const float cutoffSmoothing = 1.0f / (0.005f * sampleRate);
    state.smoothedCutoff +=
        (targetCutoff - state.smoothedCutoff) * cutoffSmoothing;

    if (state.cutoffUpdateCountdown == 0) {
        for (int stage = 0; stage < kSine2Voicing.filterStages; ++stage) {
            state.filters[stage].setCutoff(state.smoothedCutoff, sampleRate);
        }
        state.cutoffUpdateCountdown = 15;
    } else {
        --state.cutoffUpdateCountdown;
    }
    for (int stage = 0; stage < kSine2Voicing.filterStages; ++stage) {
        value = state.filters[stage].process(value);
    }
    value = analogSaturate(value, kSine2Voicing.drive,
                           kSine2Voicing.asymmetry);
    return state.dcBlocker.process(value) * kSine2Voicing.outputGain;
}

float Oscillator::nextTestTone(float frequencyHz, float envelopeLevel,
                               bool lfoGate, float phaseIncrement,
                               float sampleRate) {
    VoiceState &state = voices_[static_cast<int>(SirenMode::TestTone)];
    float value = bandLimitedPulse(phase_, phaseIncrement, kTestVoicing.duty);
    const float envelopeFactor = 0.96f + 0.04f * envelopeLevel;
    const float targetCutoff = clampCutoff(
        frequencyHz * kTestVoicing.harmonicFactor * envelopeFactor, sampleRate);
    const float cutoffSmoothing = 1.0f / (0.005f * sampleRate);
    state.smoothedCutoff +=
        (targetCutoff - state.smoothedCutoff) * cutoffSmoothing;
    if (state.cutoffUpdateCountdown == 0) {
        for (int stage = 0; stage < kTestVoicing.filterStages; ++stage) {
            state.filters[stage].setCutoff(state.smoothedCutoff, sampleRate);
        }
        state.cutoffUpdateCountdown = 15;
    } else {
        --state.cutoffUpdateCountdown;
    }
    for (int stage = 0; stage < kTestVoicing.filterStages; ++stage) {
        value = state.filters[stage].process(value);
    }
    value = analogSaturate(value, kTestVoicing.drive,
                           kTestVoicing.asymmetry);
    value = state.dcBlocker.process(value) * kTestVoicing.outputGain;

    const float gateTarget = lfoGate ? 1.0f : 0.0f;
    const float gateCoefficient = gateTarget > state.gateLevel
                                      ? 0.0103626f   // 2 ms attack.
                                      : 0.00519481f; // 4 ms release.
    state.gateLevel += (gateTarget - state.gateLevel) * gateCoefficient;
    return value * state.gateLevel;
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
