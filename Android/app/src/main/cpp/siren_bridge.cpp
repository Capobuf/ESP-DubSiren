// Android host for the shared DubSiren firmware core.
//
// Threads mirror the firmware split:
//   - UI thread  ~ firmware loop(): physical inputs, PanelDisplay::update(),
//     settings persistence. All CommandParser/PerformanceController calls.
//   - AAudio callback ~ audioTask(): ControlStore snapshot + AudioEngine render
//     in 480-sample blocks at 48 kHz. Nothing else touches AudioEngine.
// Only ControlStore is shared between them, guarded by its own mutex.

#include <aaudio/AAudio.h>
#include <android/log.h>
#include <jni.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "PanelLcdFrame.h"
#include "Config.h"
#include "audio/AudioEngine.h"
#include "control/CommandParser.h"
#include "display/PanelDisplay.h"

#define LOG_TAG "DubSiren"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

struct Siren {
    ControlStore controls;
    CommandParser parser{controls};
    PanelDisplay display{controls, parser};
    AudioEngine engine;
};

std::unique_ptr<Siren> siren;

// ---- Panel settings persistence (firmware main.cpp logic, file instead of NVS)

std::string settingsPath;
unsigned savedPanelSettingsRevision = 0;
unsigned pendingPanelSettingsRevision = 0;
uint32_t panelSettingsChangedMs = 0;
constexpr uint32_t kPanelSaveSettleMs = 2000;  // same as firmware

void loadPanelSettings() {
    CommandParser &parser = siren->parser;
    FILE *file = fopen(settingsPath.c_str(), "rb");
    if (file == nullptr) {
        savedPanelSettingsRevision = parser.panelSettingsRevision();
        return;
    }
    fseek(file, 0, SEEK_END);
    const long storedSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    PanelSettings stored;
    bool loaded = false;
    // Layouts stored by an earlier schema are dropped: factory settings apply.
    const bool current = storedSize == static_cast<long>(sizeof(stored));
    if (current) loaded = fread(&stored, 1, sizeof(stored), file) == sizeof(stored);
    fclose(file);
    if (!current || (loaded && stored.schemaVersion != PanelSettings::kSchemaVersion)) {
        savedPanelSettingsRevision = 0;
        return;
    }
    if (!loaded || !parser.applyPanelSettings(stored)) {
        parser.setPersistenceStatus("INVALID SAVED CONFIG");
    }
    savedPanelSettingsRevision = parser.panelSettingsRevision();
}

// immediate: the app is stopping and may be killed before another tick.
void savePanelSettingsIfNeeded(bool immediate = false) {
    CommandParser &parser = siren->parser;
    const unsigned revision = parser.panelSettingsRevision();
    if (revision == savedPanelSettingsRevision) return;
    const uint32_t now = millis();
    // The settle delay protects the ESP flash; a phone file needs none on stop.
    if (!immediate) {
        if (revision != pendingPanelSettingsRevision) {
            pendingPanelSettingsRevision = revision;
            panelSettingsChangedMs = now;
            return;
        }
        if (now - panelSettingsChangedMs < kPanelSaveSettleMs) return;
    }
    const PanelSettings &settings = parser.panelSettings();
    const std::string temporary = settingsPath + ".tmp";
    FILE *file = fopen(temporary.c_str(), "wb");
    bool ok = file != nullptr && fwrite(&settings, 1, sizeof(settings), file) == sizeof(settings);
    if (file != nullptr) ok = fclose(file) == 0 && ok;
    if (ok) ok = rename(temporary.c_str(), settingsPath.c_str()) == 0;
    if (!ok) {
        panelSettingsChangedMs = now;  // retry after another settle period
        parser.setPersistenceStatus("SAVE FAILED");
    } else {
        savedPanelSettingsRevision = revision;
        parser.setPersistenceStatus("");
    }
}

// ---- Physical inputs (PanelInputs.cpp after debounce/ADC)

// Nominal divider maxima from PanelInputs.cpp: 3.3 V * Rpot / (Rpot + 4.7k).
constexpr float kPotMaximumMv = 2672.1f;
constexpr float kMasterMaximumMv = 2244.9f;
constexpr int kAdcThresholdMv = 8;
int lastMillivolts[7] = {-1, -1, -1, -1, -1, -1, -1};

// channel 0..5 = POT1..6 (mux A0..A5), 6 = Master (A6). The position is the
// wiper travel 0..1; it is quantized to integer millivolts with the firmware's
// 8 mV change threshold before entering the shared controller.
void physicalPot(unsigned channel, float position) {
    if (channel > 6) return;
    const float maximum = channel == 6 ? kMasterMaximumMv : kPotMaximumMv;
    const float travel = fminf(1.0f, fmaxf(0.0f, position));
    const int millivolts = static_cast<int>(lroundf(travel * maximum));
    if (lastMillivolts[channel] >= 0 && abs(millivolts - lastMillivolts[channel]) < kAdcThresholdMv) return;
    lastMillivolts[channel] = millivolts;
    const float normalized = fminf(1.0f, fmaxf(0.0f, millivolts / maximum));
    if (channel == 6) {
        siren->parser.setPhysicalMaster(normalized);
    } else {
        siren->parser.setPhysicalPot(channel + 1, normalized);
    }
}

// ---- AAudio output

std::mutex streamMutex;
AAudioStream *stream = nullptr;
bool audioWanted = false;
int32_t streamChannels = 1;
aaudio_format_t streamFormat = AAUDIO_FORMAT_PCM_I16;
std::atomic<int32_t> streamRate{0};
std::atomic<int32_t> streamBurst{0};
std::atomic<int32_t> streamBuffer{0};
std::atomic<uint32_t> lastRenderUs{0};
std::atomic<uint32_t> maximumRenderUs{0};
int32_t lastXRuns = 0;

int16_t block[Config::kBlockSamples];
size_t blockPosition = Config::kBlockSamples;

void renderBlock() {
    const ControlState snapshot = siren->controls.snapshot();
    const uint32_t start = micros();
    siren->engine.render(snapshot, block, Config::kBlockSamples);
    const uint32_t elapsed = micros() - start;
    lastRenderUs.store(elapsed, std::memory_order_relaxed);
    if (elapsed > maximumRenderUs.load(std::memory_order_relaxed))
        maximumRenderUs.store(elapsed, std::memory_order_relaxed);
    blockPosition = 0;
}

aaudio_data_callback_result_t dataCallback(AAudioStream *, void *, void *audioData, int32_t frames) {
    const int32_t channels = streamChannels;
    if (streamFormat == AAUDIO_FORMAT_PCM_FLOAT) {
        float *out = static_cast<float *>(audioData);
        for (int32_t frame = 0; frame < frames; ++frame) {
            if (blockPosition >= Config::kBlockSamples) renderBlock();
            const float sample = block[blockPosition++] / 32768.0f;
            for (int32_t c = 0; c < channels; ++c) *out++ = sample;
        }
    } else {
        int16_t *out = static_cast<int16_t *>(audioData);
        for (int32_t frame = 0; frame < frames; ++frame) {
            if (blockPosition >= Config::kBlockSamples) renderBlock();
            const int16_t sample = block[blockPosition++];
            for (int32_t c = 0; c < channels; ++c) *out++ = sample;
        }
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

// Underrun handling: grow the buffer by one burst per detected XRun.
void tuneLatency() {
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

std::string jstringToString(JNIEnv *env, jstring value) {
    if (value == nullptr) return {};
    const char *chars = env->GetStringUTFChars(value, nullptr);
    std::string result(chars ? chars : "");
    if (chars) env->ReleaseStringUTFChars(value, chars);
    return result;
}

inline uint32_t rgb565ToArgb(uint16_t color) {
    const uint32_t r = (((color >> 11) & 0x1F) * 527 + 23) >> 6;
    const uint32_t g = (((color >> 5) & 0x3F) * 259 + 33) >> 6;
    const uint32_t b = ((color & 0x1F) * 527 + 23) >> 6;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

}  // namespace

#define JNI_FN(name) Java_com_capobuf_dubsiren_NativeSiren_##name

extern "C" {

JNIEXPORT jboolean JNICALL JNI_FN(nativeInit)(JNIEnv *env, jobject, jstring path) {
    if (siren) return JNI_TRUE;  // process-wide, survives Activity recreation
    siren = std::make_unique<Siren>();
    settingsPath = jstringToString(env, path);
    siren->engine.begin();
    loadPanelSettings();
    // PanelInputs::begin(): Master stays at zero until its first conversion.
    ControlState state = siren->controls.snapshot();
    state.masterVolume = 0;
    siren->controls.set(state);
    if (!siren->display.begin()) siren->parser.setDeviceStatus("DISPLAY INIT FAILED");
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL JNI_FN(nativeStartAudio)(JNIEnv *, jobject) {
    std::lock_guard<std::mutex> lock(streamMutex);
    audioWanted = true;
    const bool ok = openStreamLocked();
    siren->parser.setDeviceStatus(ok ? "" : "AUDIO OUTPUT FAILED");
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL JNI_FN(nativeStopAudio)(JNIEnv *, jobject) {
    std::lock_guard<std::mutex> lock(streamMutex);
    audioWanted = false;
    closeStreamLocked();
    savePanelSettingsIfNeeded(true);  // onStop: no more ticks to save later
}

JNIEXPORT void JNICALL JNI_FN(nativeSetPot)(JNIEnv *, jobject, jint channel, jfloat position) {
    if (siren) physicalPot(static_cast<unsigned>(channel), position);
}

JNIEXPORT void JNICALL JNI_FN(nativeSetButton)(JNIEnv *, jobject, jint button, jboolean down) {
    if (siren) siren->parser.setPhysicalPad(static_cast<unsigned>(button), down == JNI_TRUE);
}

JNIEXPORT void JNICALL JNI_FN(nativeSetEncoderPressed)(JNIEnv *, jobject, jboolean down) {
    if (siren) siren->parser.setPhysicalEncoderPressed(down == JNI_TRUE);
}

JNIEXPORT void JNICALL JNI_FN(nativeRotateEncoder)(JNIEnv *, jobject, jint direction) {
    if (siren && direction != 0) siren->parser.rotatePhysicalEncoder(direction > 0 ? 1 : -1);
}

// One loop() pass: persistence + display. Copies the framebuffer into
// `pixels` (ARGB, 320x172) and returns true when it changed since `since`.
JNIEXPORT jint JNICALL JNI_FN(nativeTick)(JNIEnv *env, jobject, jintArray pixels, jint since) {
    if (!siren) return since;
    tuneLatency();
    savePanelSettingsIfNeeded();
    siren->display.update(false);
    const jint frame = static_cast<jint>(panelLcdFrameCounter());
    if (frame == since) return since;
    const int count = PanelLcd::kWidth * PanelLcd::kHeight;
    if (env->GetArrayLength(pixels) < count) return since;
    jint *out = env->GetIntArrayElements(pixels, nullptr);
    const uint16_t *source = panelLcdPixels();
    for (int i = 0; i < count; ++i) out[i] = static_cast<jint>(rgb565ToArgb(source[i]));
    env->ReleaseIntArrayElements(pixels, out, 0);
    return frame;
}

JNIEXPORT jint JNICALL JNI_FN(nativeDisplayWidth)(JNIEnv *, jobject) {
    return PerformanceController::kDisplayWidth;
}

JNIEXPORT jint JNICALL JNI_FN(nativeDisplayHeight)(JNIEnv *, jobject) {
    return PerformanceController::kDisplayHeight;
}

JNIEXPORT jstring JNICALL JNI_FN(nativeAudioStatus)(JNIEnv *env, jobject) {
    char text[160];
    int32_t xruns = 0;
    bool open = false;
    {
        std::unique_lock<std::mutex> lock(streamMutex, std::try_to_lock);
        if (lock.owns_lock() && stream != nullptr) {
            open = true;
            xruns = AAudioStream_getXRunCount(stream);
        }
    }
    const int32_t rate = streamRate.load();
    snprintf(text, sizeof(text), "fw %s %s %dHz%s buf %d xrun %d render %u/%uus",
             Config::kFirmwareVersion, open ? "ON" : "OFF", rate,
             rate != 0 && rate != static_cast<int32_t>(Config::kSampleRate) ? " (!48k)" : "",
             streamBuffer.load(), xruns, lastRenderUs.load(), maximumRenderUs.load());
    return env->NewStringUTF(text);
}

}  // extern "C"
