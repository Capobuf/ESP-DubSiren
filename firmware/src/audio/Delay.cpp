#include "Delay.h"

#include <math.h>
#include <string.h>

#include "DspUtils.h"
#include "PerformanceConfig.h"

namespace {

int16_t floatToInt16(float value) {
    const float limited = value < -1.0f ? -1.0f : (value > 1.0f ? 1.0f : value);
    return static_cast<int16_t>(lrintf(limited * 32767.0f));
}

}  // namespace

Delay::Delay()
    : writeIndex_(0), delayMs_(360.0f), highPassHz_(60.0f),
      lowPassHz_(7000.0f), filterUpdateCountdown_(0),
      appliedHighPassHz_(-1.0f), appliedLowPassHz_(-1.0f) {
    memset(buffer_, 0, sizeof(buffer_));
}

void Delay::begin(float initialDelayMs, float highPassHz, float lowPassHz,
                  float sampleRate) {
    delayMs_.configure(PerformanceConfig::kDelayMs, sampleRate);
    highPassHz_.configure(PerformanceConfig::kFilterMs, sampleRate);
    lowPassHz_.configure(PerformanceConfig::kFilterMs, sampleRate);
    delayMs_.reset(initialDelayMs);
    highPassHz_.reset(highPassHz);
    lowPassHz_.reset(lowPassHz);
    highPass_.setHighPass(highPassHz, sampleRate, PerformanceConfig::kEchoFilterQ);
    lowPass_.setLowPass(lowPassHz, sampleRate, PerformanceConfig::kEchoFilterQ);
    appliedHighPassHz_ = highPassHz;
    appliedLowPassHz_ = lowPassHz;
    setParameters(initialDelayMs, highPassHz, lowPassHz, sampleRate);
}

void Delay::setParameters(float delayMs, float highPassHz, float lowPassHz,
                          float sampleRate) {
    (void)sampleRate;
    delayMs_.setTarget(delayMs);
    highPassHz_.setTarget(highPassHz);
    lowPassHz_.setTarget(lowPassHz);
}

float Delay::process(float dry, float feedback, float sampleRate) {
    const float oldDelay = delayMs_.current();
    const float proposedDelay = delayMs_.next();
    const float maxStep = PerformanceConfig::kDelayMaxSlewMsPerSecond / sampleRate;
    if (proposedDelay > oldDelay + maxStep) delayMs_.setCurrent(oldDelay + maxStep);
    else if (proposedDelay < oldDelay - maxStep) delayMs_.setCurrent(oldDelay - maxStep);
    const float delaySamples = delayMs_.current() * sampleRate * 0.001f;
    const float highPass = highPassHz_.next();
    const float lowPass = lowPassHz_.next();
    if (filterUpdateCountdown_ == 0) {
        if (fabsf(highPass - appliedHighPassHz_) > appliedHighPassHz_ * 0.002f) {
            highPass_.setHighPass(highPass, sampleRate, PerformanceConfig::kEchoFilterQ);
            appliedHighPassHz_ = highPass;
        }
        if (fabsf(lowPass - appliedLowPassHz_) > appliedLowPassHz_ * 0.002f) {
            lowPass_.setLowPass(lowPass, sampleRate, PerformanceConfig::kEchoFilterQ);
            appliedLowPassHz_ = lowPass;
        }
        filterUpdateCountdown_ = PerformanceConfig::kFilterUpdateSamples - 1;
    } else {
        --filterUpdateCountdown_;
    }
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
    const float saturatedFeedback =
        analogSaturate(filtered * feedback,
                       PerformanceConfig::kFeedbackSaturationDrive,
                       PerformanceConfig::kFeedbackSaturationAsymmetry) *
        PerformanceConfig::kFeedbackSaturationGain;
    const float feedbackInput = softClip(dry + saturatedFeedback);
    buffer_[writeIndex_] = floatToInt16(feedbackInput);
    writeIndex_ = (writeIndex_ + 1) % kBufferSamples;
    return filtered;
}
