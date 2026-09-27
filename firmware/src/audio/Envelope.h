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

class OneShotEnvelope {
public:
    OneShotEnvelope();
    void trigger();
    float next(float decayMs, float sampleRate);
    void reset();

private:
    float level_;
    float lastDecayMs_;
    float multiplier_;
};
