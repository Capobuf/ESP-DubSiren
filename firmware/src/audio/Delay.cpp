#include "Delay.h"

#include <math.h>
#include <string.h>

#include "DspUtils.h"

namespace {

constexpr float kFilterQ = 0.707f;

int16_t floatToInt16(float value) {
    const float limited = value < -1.0f ? -1.0f : (value > 1.0f ? 1.0f : value);
    return static_cast<int16_t>(lrintf(limited * 32767.0f));
}

}  // namespace

Delay::Delay()
    : writeIndex_(0), delayMs_(360.0f), targetDelayMs_(360.0f),
      highPassHz_(-1.0f), lowPassHz_(-1.0f) {
    memset(buffer_, 0, sizeof(buffer_));
}

void Delay::begin(float initialDelayMs, float highPassHz, float lowPassHz,
                  float sampleRate) {
    delayMs_ = initialDelayMs;
    targetDelayMs_ = initialDelayMs;
    setParameters(initialDelayMs, highPassHz, lowPassHz, sampleRate);
}

void Delay::setParameters(float delayMs, float highPassHz, float lowPassHz,
                          float sampleRate) {
    targetDelayMs_ = delayMs;
    if (highPassHz != highPassHz_) {
        highPassHz_ = highPassHz;
        highPass_.setHighPass(highPassHz_, sampleRate, kFilterQ);
    }
    if (lowPassHz != lowPassHz_) {
        lowPassHz_ = lowPassHz;
        lowPass_.setLowPass(lowPassHz_, sampleRate, kFilterQ);
    }
}

float Delay::process(float dry, float feedback, float sampleRate) {
    const float smoothing = 1.0f / (0.025f * sampleRate);
    delayMs_ += (targetDelayMs_ - delayMs_) * smoothing;
    const float delaySamples = delayMs_ * sampleRate * 0.001f;
    float readPosition = static_cast<float>(writeIndex_) - delaySamples;
    while (readPosition < 0.0f) {
        readPosition += kBufferSamples;
    }
    while (readPosition >= kBufferSamples) {
        readPosition -= kBufferSamples;
    }

    const size_t i0 = static_cast<size_t>(floorf(readPosition));
    const size_t i1 = (i0 + 1) % kBufferSamples;
    const float fraction = readPosition - static_cast<float>(i0);
    const float sample0 = buffer_[i0] / 32768.0f;
    const float sample1 = buffer_[i1] / 32768.0f;
    const float delayed = sample0 + (sample1 - sample0) * fraction;
    const float filtered = lowPass_.process(highPass_.process(delayed));
    const float feedbackInput = softClip(dry + filtered * feedback);
    buffer_[writeIndex_] = floatToInt16(feedbackInput);
    writeIndex_ = (writeIndex_ + 1) % kBufferSamples;
    return filtered;
}
