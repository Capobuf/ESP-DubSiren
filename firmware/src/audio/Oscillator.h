#pragma once

#include "AnalogDsp.h"
#include "Envelope.h"
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
    float nextSyndrum(SyndrumWave wave, float frequencyHz, float sampleRate);
    void resetSonarPulse();

private:
    float renderVoice(SirenMode mode, float frequencyHz, float modulation,
                      float envelopeLevel, bool lfoGate, float sampleRate,
                      bool voicingV2);
    void advance(float increment);
    struct VoiceState {
        OnePoleLowPass filters[1];
        DcBlocker dcBlocker;
        float smoothedCutoff = 1000.0f;
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
    float nextLickshot(float frequencyHz, float modulation,
                       float envelopeLevel, float phaseIncrement,
                       float sampleRate);
    float nextSonar(bool lfoGate, float sampleRate);

    Envelope testGate_;
    Envelope sonarGate_;
    OneShotEnvelope sonarPing_;
    bool previousSonarGate_ = false;
    float phase_;
    float syndrumTriangle_;
    VoiceState voices_[static_cast<int>(SirenMode::Count)];
};
