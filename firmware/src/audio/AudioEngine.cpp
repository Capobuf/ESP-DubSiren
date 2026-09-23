#include "AudioEngine.h"

#include <math.h>

#include "Config.h"
#include "DspUtils.h"

namespace {

float clampFrequency(float frequency) {
    const float maximum = Config::kSampleRate * 0.45f;
    return frequency < 10.0f ? 10.0f : (frequency > maximum ? maximum : frequency);
}

int16_t toInt16(float value) {
    const float limited = value < -1.0f ? -1.0f : (value > 1.0f ? 1.0f : value);
    return static_cast<int16_t>(lrintf(limited * 32767.0f));
}

}  // namespace

AudioEngine::AudioEngine()
    : tuneHz_(220.0f), depthOctaves_(1.0f), echoLevel_(0.45f),
      masterVolume_(0.50f) {}

void AudioEngine::begin() {
    delay_.begin(360.0f, 60.0f, 7000.0f, Config::kSampleRate);
}

void AudioEngine::render(const ControlState &controls, int16_t *output,
                         size_t count) {
    delay_.setParameters(controls.delayMs, controls.highPassHz,
                         controls.lowPassHz, Config::kSampleRate);
    constexpr float smoothing = 1.0f / (0.012f * Config::kSampleRate);

    for (size_t i = 0; i < count; ++i) {
        tuneHz_ += (controls.tuneHz - tuneHz_) * smoothing;
        depthOctaves_ +=
            (controls.lfoDepthOctaves - depthOctaves_) * smoothing;
        echoLevel_ += (controls.echoLevel - echoLevel_) * smoothing;
        masterVolume_ += (controls.masterVolume - masterVolume_) * smoothing;

        const float modulation =
            lfo_.next(controls.lfoShape, controls.lfoRateHz, controls.modUp,
                      controls.modDown, Config::kSampleRate);
        const float frequency =
            clampFrequency(tuneHz_ * exp2f(depthOctaves_ * modulation));
        float dry = oscillator_.next(controls.mode, frequency,
                                     Config::kSampleRate);
        if (controls.mode == SirenMode::TestTone && !lfo_.gate()) {
            dry = 0.0f;
        }
        dry *= envelope_.next(controls.trigger || controls.hold,
                              controls.decayMs, Config::kSampleRate);

        const float wet = delay_.process(dry, controls.feedback,
                                         Config::kSampleRate);
        const float mixed = dry + (controls.echoCut ? 0.0f : wet * echoLevel_);
        output[i] = toInt16(softClip(mixed * masterVolume_));
    }
}
