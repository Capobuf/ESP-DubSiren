#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Delay.h"
#include "Envelope.h"
#include "Lfo.h"
#include "Oscillator.h"
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
    Delay delay_;
    float tuneHz_;
    float depthOctaves_;
    float echoLevel_;
    float masterVolume_;
    float oscillatorFrequencyHz_;
};
