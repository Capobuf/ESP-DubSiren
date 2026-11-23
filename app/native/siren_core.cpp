// Host core for the shared DubSiren firmware sources: what firmware main.cpp
// does around the controller, display and AudioEngine, without the hardware.

#include "siren_core.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include "PanelLcdFrame.h"
#include "Config.h"
#include "audio/AudioEngine.h"
#include "audio_output.h"
#include "control/CommandParser.h"
#include "display/PanelDisplay.h"

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

// Replaces an existing file on every platform (Windows rename() refuses to).
bool replaceFile(const std::string &from, const std::string &to) {
    std::error_code error;
    std::filesystem::rename(from, to, error);
    return !error;
}

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
    // The settle delay protects the ESP flash; a host file needs none on stop.
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
    if (ok) ok = replaceFile(temporary, settingsPath);
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

// ---- Audio rendering (audio thread only)

std::atomic<uint32_t> lastRenderUs{0};
std::atomic<uint32_t> maximumRenderUs{0};

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

inline uint32_t rgb565ToArgb(uint16_t color) {
    const uint32_t r = (((color >> 11) & 0x1F) * 527 + 23) >> 6;
    const uint32_t g = (((color >> 5) & 0x3F) * 259 + 33) >> 6;
    const uint32_t b = ((color & 0x1F) * 527 + 23) >> 6;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

}  // namespace

extern "C" {

bool siren_init(const char *settings_path) {
    if (siren) return true;  // process-wide, survives Activity recreation
    siren = std::make_unique<Siren>();
    settingsPath = settings_path ? settings_path : "";
    siren->engine.begin();
    loadPanelSettings();
    // PanelInputs::begin(): Master stays at zero until its first conversion.
    ControlState state = siren->controls.snapshot();
    state.masterVolume = 0;
    siren->controls.set(state);
    if (!siren->display.begin()) siren->parser.setDeviceStatus("DISPLAY INIT FAILED");
    return true;
}

bool siren_start_audio(void) {
    if (!siren) return false;
    const bool ok = audioOutputStart(Config::kSampleRate);
    siren->parser.setDeviceStatus(ok ? "" : "AUDIO OUTPUT FAILED");
    return ok;
}

void siren_stop_audio(void) {
    if (!siren) return;
    audioOutputStop();
    savePanelSettingsIfNeeded(true);
}

void siren_set_pot(int32_t channel, float position) {
    if (siren && channel >= 0) physicalPot(static_cast<unsigned>(channel), position);
}

void siren_set_button(int32_t button, bool down) {
    if (siren) siren->parser.setPhysicalPad(static_cast<unsigned>(button), down);
}

void siren_set_encoder_pressed(bool down) {
    if (siren) siren->parser.setPhysicalEncoderPressed(down);
}

void siren_rotate_encoder(int32_t direction) {
    if (siren && direction != 0) siren->parser.rotatePhysicalEncoder(direction > 0 ? 1 : -1);
}

int32_t siren_tick(void) {
    if (!siren) return 0;
    audioOutputTune();
    savePanelSettingsIfNeeded();
    siren->display.update(false);
    return static_cast<int32_t>(panelLcdFrameCounter());
}

void siren_copy_display(uint32_t *argb) {
    const int count = PanelLcd::kWidth * PanelLcd::kHeight;
    const uint16_t *source = panelLcdPixels();
    for (int i = 0; i < count; ++i) argb[i] = rgb565ToArgb(source[i]);
}

int32_t siren_display_width(void) { return PerformanceController::kDisplayWidth; }
int32_t siren_display_height(void) { return PerformanceController::kDisplayHeight; }

void siren_render_mono(int16_t *out, int32_t frames) {
    for (int32_t frame = 0; frame < frames; ++frame) {
        if (blockPosition >= Config::kBlockSamples) renderBlock();
        out[frame] = block[blockPosition++];
    }
}

void siren_audio_status(char *text, size_t size) {
    const AudioOutputInfo info = audioOutputInfo();
    const int32_t rate = info.sampleRate;
    snprintf(text, size, "fw %s %s %dHz%s buf %d xrun %d render %u/%uus",
             Config::kFirmwareVersion, info.open ? "ON" : "OFF", rate,
             rate != 0 && rate != static_cast<int32_t>(Config::kSampleRate) ? " (!48k)" : "",
             info.bufferFrames, info.xruns, lastRenderUs.load(), maximumRenderUs.load());
}

}  // extern "C"
