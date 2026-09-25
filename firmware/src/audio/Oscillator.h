#pragma once

#include "AnalogDsp.h"
#include "control/ControlState.h"

class Oscillator {
public:
    Oscillator();
    float next(SirenMode mode, float frequencyHz, float modulation,
               float envelopeLevel, bool lfoGate, float sampleRate,
               bool voicingV2);
    float nextTransition(SirenMode from, SirenMode to, float mix,
                         float frequencyHz, float modulation,
                         float envelopeLevel, bool lfoGate, float sampleRate,
                         bool voicingV2);

private:
    float renderVoice(SirenMode mode, float frequencyHz, float modulation,
                      float envelopeLevel, bool lfoGate, float sampleRate,
                      bool voicingV2);
    void advance(float increment);
    struct VoiceState {
        OnePoleLowPass filters[3];
        DcBlocker dcBlocker;
        float smoothedCutoff = 1000.0f;
        float gateLevel = 0.0f;
        uint8_t cutoffUpdateCountdown = 0;
    };

    float nextLegacy(SirenMode mode, float phaseIncrement) const;
    float nextSine1(float frequencyHz, float modulation,
                    float envelopeLevel, float phaseIncrement,
                    float sampleRate);
    float nextSine2(float frequencyHz, float modulation,
                    float envelopeLevel, float phaseIncrement,
                    float sampleRate);
    float nextTestTone(float frequencyHz, float envelopeLevel, bool lfoGate,
                       float phaseIncrement, float sampleRate);
    float nextSquare(float frequencyHz, float modulation, float envelopeLevel,
                     float phaseIncrement, float sampleRate);

    float phase_;
    VoiceState voices_[4];
};
