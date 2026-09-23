#pragma once

class Envelope {
public:
    Envelope();
    float next(bool gate, float decayMs, float sampleRate);

private:
    float level_;
    float lastDecayMs_;
    float releaseMultiplier_;
};
