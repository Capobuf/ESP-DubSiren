#pragma once

class Biquad {
public:
    Biquad();
    void setHighPass(float cutoffHz, float sampleRate, float q);
    void setLowPass(float cutoffHz, float sampleRate, float q);
    float process(float input);
    void reset();

private:
    void setCoefficients(float b0, float b1, float b2, float a0, float a1,
                         float a2);
    float b0_, b1_, b2_, a1_, a2_;
    float z1_, z2_;
};
