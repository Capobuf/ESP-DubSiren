// Windows audio output: miniaudio (WASAPI), low-latency profile. The callback
// asks for mono s16 at 48 kHz, so the DSP keeps its rate; miniaudio converts
// to the device format, channels and rate.

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#include "miniaudio.h"

#include <mutex>

#include "audio_output.h"
#include "siren_core.h"

namespace {

std::mutex deviceMutex;
ma_device device;
bool deviceOpen = false;

void dataCallback(ma_device *, void *output, const void *, ma_uint32 frames) {
    siren_render_mono(static_cast<int16_t *>(output), static_cast<int32_t>(frames));
}

}  // namespace

bool audioOutputStart(uint32_t sampleRate) {
    std::lock_guard<std::mutex> lock(deviceMutex);
    if (deviceOpen) return true;
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_s16;
    config.playback.channels = 1;  // mono, like the firmware PCM
    config.sampleRate = sampleRate;
    config.performanceProfile = ma_performance_profile_low_latency;
    config.dataCallback = dataCallback;
    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS) return false;
    if (ma_device_start(&device) != MA_SUCCESS) {
        ma_device_uninit(&device);
        return false;
    }
    deviceOpen = true;
    return true;
}

void audioOutputStop() {
    std::lock_guard<std::mutex> lock(deviceMutex);
    if (!deviceOpen) return;
    ma_device_uninit(&device);
    deviceOpen = false;
}

// WASAPI buffer sizing is left to miniaudio's low-latency profile.
void audioOutputTune() {}

AudioOutputInfo audioOutputInfo() {
    AudioOutputInfo info;
    std::unique_lock<std::mutex> lock(deviceMutex, std::try_to_lock);
    if (!lock.owns_lock() || !deviceOpen) return info;
    info.open = true;
    // Rate the DSP is pulled at; a different device rate is resampled, pitch unchanged.
    info.sampleRate = static_cast<int32_t>(device.sampleRate);
    info.bufferFrames = static_cast<int32_t>(device.playback.internalPeriodSizeInFrames *
                                             device.playback.internalPeriods);
    info.xruns = -1;  // miniaudio does not count underruns
    return info;
}
