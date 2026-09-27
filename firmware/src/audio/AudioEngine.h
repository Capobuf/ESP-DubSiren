#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Delay.h"
#include "Envelope.h"
#include "Lfo.h"
#include "Oscillator.h"
#include "AnalogDsp.h"
#include "SmoothedParameter.h"
#include "control/ControlState.h"

class AudioEngine {
public:
    AudioEngine();
    void begin();
    void render(const ControlState &controls, int16_t *output, size_t count);

private:
    Oscillator oscillator_;
    Lfo lfo_;
    Envelope envelope_;
    OneShotEnvelope sweepEnvelope_;
    OneShotEnvelope syndrumEnvelope_;
    OneShotEnvelope gunshotEnvelope_;
    OnePoleLowPass gunshotFilter_;
    Delay delay_;
    SmoothedParameter tuneHz_;
    SmoothedParameter depthOctaves_;
    SmoothedParameter rateHz_;
    SmoothedParameter feedback_;
    SmoothedParameter echoLevel_;
    SmoothedParameter echoCutWet_;
    SmoothedParameter masterVolume_;
    SmoothedParameter performanceBend_;
    float oscillatorFrequencyHz_;
    float lastDry_;
    float transitionStartDry_;
    uint32_t noiseState_;
    SirenMode previousMode_;
    SirenMode currentMode_;
    unsigned modeTransitionRemaining_;
    bool modeInitialized_;
    bool previousTrigger_;
    bool previousHold_;
    static constexpr unsigned kPendingActivationCapacity = 32;
    ActivationEvent pendingActivations_[kPendingActivationCapacity];
    uint32_t capturedActivationSequence_;
    unsigned pendingActivationRead_;
    unsigned pendingActivationWrite_;
    unsigned pendingActivationCount_;
    unsigned eventGateRemaining_;

    static bool isTonalMode(SirenMode mode);
    float nextGunshot(float envelopeLevel);
};
