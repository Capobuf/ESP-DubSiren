// Android audio output: AAudio low-latency stream, mono 48 kHz, pulled from
// siren_render_mono() on the AAudio callback thread (the firmware audioTask).

#include <aaudio/AAudio.h>
#include <android/log.h>

#include <atomic>
#include <mutex>
#include <thread>

#include "Config.h"
#include "audio_output.h"
#include "siren_core.h"

#define LOG_TAG "DubSiren"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

std::mutex streamMutex;
AAudioStream *stream = nullptr;
bool audioWanted = false;
int32_t streamChannels = 1;
aaudio_format_t streamFormat = AAUDIO_FORMAT_PCM_I16;
std::atomic<int32_t> streamRate{0};
std::atomic<int32_t> streamBurst{0};
std::atomic<int32_t> streamBuffer{0};
int32_t lastXRuns = 0;

aaudio_data_callback_result_t dataCallback(AAudioStream *, void *, void *audioData, int32_t frames) {
    const int32_t channels = streamChannels;
    const bool floats = streamFormat == AAUDIO_FORMAT_PCM_FLOAT;
    float *outFloat = static_cast<float *>(audioData);
    int16_t *outShort = static_cast<int16_t *>(audioData);
    int16_t mono[256];
    for (int32_t done = 0; done < frames;) {
        const int32_t count = frames - done < 256 ? frames - done : 256;
        siren_render_mono(mono, count);
        for (int32_t i = 0; i < count; ++i) {
            for (int32_t c = 0; c < channels; ++c) {
                if (floats) *outFloat++ = mono[i] / 32768.0f;
                else *outShort++ = mono[i];
            }
        }
        done += count;
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void closeStreamLocked() {
    if (stream == nullptr) return;
    AAudioStream_requestStop(stream);
    AAudioStream_close(stream);
    stream = nullptr;
}

void errorCallback(AAudioStream *, void *, aaudio_result_t error);

bool openStreamLocked() {
    closeStreamLocked();
    for (int attempt = 0; attempt < 2 && stream == nullptr; ++attempt) {
        AAudioStreamBuilder *builder = nullptr;
        if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK) return false;
        AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
        AAudioStreamBuilder_setSampleRate(builder, static_cast<int32_t>(Config::kSampleRate));
        AAudioStreamBuilder_setChannelCount(builder, 1);  // mono, like the firmware PCM
        AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
        AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
        AAudioStreamBuilder_setSharingMode(builder, attempt == 0 ? AAUDIO_SHARING_MODE_EXCLUSIVE
                                                                 : AAUDIO_SHARING_MODE_SHARED);
        AAudioStreamBuilder_setDataCallback(builder, dataCallback, nullptr);
        AAudioStreamBuilder_setErrorCallback(builder, errorCallback, nullptr);
        const aaudio_result_t result = AAudioStreamBuilder_openStream(builder, &stream);
        AAudioStreamBuilder_delete(builder);
        if (result != AAUDIO_OK) {
            LOGE("openStream attempt %d failed: %s", attempt, AAudio_convertResultToText(result));
            stream = nullptr;
        }
    }
    if (stream == nullptr) return false;
    streamChannels = AAudioStream_getChannelCount(stream);
    streamFormat = AAudioStream_getFormat(stream);
    streamRate.store(AAudioStream_getSampleRate(stream));
    const int32_t burst = AAudioStream_getFramesPerBurst(stream);
    streamBurst.store(burst);
    streamBuffer.store(AAudioStream_setBufferSizeInFrames(stream, burst * 2));
    lastXRuns = 0;
    if (AAudioStream_requestStart(stream) != AAUDIO_OK) {
        closeStreamLocked();
        return false;
    }
    LOGI("AAudio %d Hz, %d ch, burst %d, buffer %d", streamRate.load(), streamChannels, burst,
         streamBuffer.load());
    return true;
}

void errorCallback(AAudioStream *, void *, aaudio_result_t error) {
    LOGE("AAudio error: %s", AAudio_convertResultToText(error));
    // Never reopen from the callback thread (AAudio documentation).
    std::thread([] {
        std::lock_guard<std::mutex> lock(streamMutex);
        if (audioWanted) openStreamLocked();
    }).detach();
}

}  // namespace

bool audioOutputStart() {
    std::lock_guard<std::mutex> lock(streamMutex);
    audioWanted = true;
    return openStreamLocked();
}

void audioOutputStop() {
    std::lock_guard<std::mutex> lock(streamMutex);
    audioWanted = false;
    closeStreamLocked();
}

// Underrun handling: grow the buffer by one burst per detected XRun.
void audioOutputTune() {
    std::unique_lock<std::mutex> lock(streamMutex, std::try_to_lock);
    if (!lock.owns_lock() || stream == nullptr) return;
    const int32_t xruns = AAudioStream_getXRunCount(stream);
    if (xruns > lastXRuns) {
        lastXRuns = xruns;
        const int32_t size = AAudioStream_getBufferSizeInFrames(stream) + streamBurst.load();
        if (size <= AAudioStream_getBufferCapacityInFrames(stream))
            streamBuffer.store(AAudioStream_setBufferSizeInFrames(stream, size));
    }
}

AudioOutputInfo audioOutputInfo() {
    AudioOutputInfo info;
    {
        std::unique_lock<std::mutex> lock(streamMutex, std::try_to_lock);
        if (lock.owns_lock() && stream != nullptr) {
            info.open = true;
            info.xruns = AAudioStream_getXRunCount(stream);
        }
    }
    info.sampleRate = streamRate.load();
    info.bufferFrames = streamBuffer.load();
    return info;
}
