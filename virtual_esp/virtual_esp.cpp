#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "Config.h"
#include "audio/AudioEngine.h"
#include "control/CommandParser.h"

#ifdef _WIN32
#define VIRTUAL_ESP_API __declspec(dllexport)
#else
#define VIRTUAL_ESP_API __attribute__((visibility("default")))
#endif

namespace {

std::unique_ptr<AudioEngine> engine;
std::unique_ptr<ControlStore> controls;
std::unique_ptr<CommandParser> parser;

bool ready() {
    return engine != nullptr && controls != nullptr && parser != nullptr;
}

}  // namespace

extern "C" {

VIRTUAL_ESP_API int virtual_esp_init() {
    parser.reset();
    controls.reset();
    engine.reset();

    controls = std::make_unique<ControlStore>();
    parser = std::make_unique<CommandParser>(*controls);
    engine = std::make_unique<AudioEngine>();
    engine->begin();
    return 1;
}

VIRTUAL_ESP_API int virtual_esp_command(const char *command) {
    if (!ready() || command == nullptr) {
        return 0;
    }
    return parser->parse(String(command)) ? 1 : 0;
}

VIRTUAL_ESP_API int virtual_esp_render(int16_t *output, size_t count) {
    if (!ready() || output == nullptr || count != Config::kBlockSamples) {
        return 0;
    }
    engine->render(controls->snapshot(), output, count);
    return 1;
}

VIRTUAL_ESP_API const char *virtual_esp_firmware_version() {
    return Config::kFirmwareVersion;
}

VIRTUAL_ESP_API int virtual_esp_protocol_version() {
    return Config::kProtocolVersion;
}

VIRTUAL_ESP_API int virtual_esp_sample_rate() {
    return static_cast<int>(Config::kSampleRate);
}

VIRTUAL_ESP_API int virtual_esp_block_samples() {
    return static_cast<int>(Config::kBlockSamples);
}

VIRTUAL_ESP_API unsigned virtual_esp_panel_revision() {
    return ready() ? parser->panelRevision() : 0;
}

VIRTUAL_ESP_API unsigned virtual_esp_panel_menu_revision() {
    return ready() ? parser->panelMenuRevision() : 0;
}

VIRTUAL_ESP_API unsigned virtual_esp_display_width() {
    return PerformanceController::kDisplayWidth;
}

VIRTUAL_ESP_API unsigned virtual_esp_display_height() {
    return PerformanceController::kDisplayHeight;
}

VIRTUAL_ESP_API const char *virtual_esp_display_mode() {
    return ready() ? parser->displayModeName() : "HOME";
}

VIRTUAL_ESP_API unsigned virtual_esp_menu_index() {
    return ready() ? parser->menuIndex() : 0;
}

VIRTUAL_ESP_API int virtual_esp_menu_editing() {
    return ready() && parser->menuEditing() ? 1 : 0;
}

VIRTUAL_ESP_API unsigned virtual_esp_menu_item_count() {
    return ready() ? parser->menuItemCount() : 0;
}

VIRTUAL_ESP_API const char *virtual_esp_menu_item_key(unsigned index) {
    return ready() ? parser->menuItemKey(index) : "BACK";
}

VIRTUAL_ESP_API float virtual_esp_depth() {
    return ready() ? controls->snapshot().lfoDepthOctaves : 0.0f;
}

VIRTUAL_ESP_API int virtual_esp_echo_loop() {
    return ready() && controls->snapshot().echoLoop ? 1 : 0;
}

VIRTUAL_ESP_API int virtual_esp_selected_pad() {
    return ready() ? static_cast<int>(parser->selectedPad()) : 0;
}

VIRTUAL_ESP_API int virtual_esp_current_pad() {
    return ready() ? static_cast<int>(parser->currentPad()) : 0;
}

VIRTUAL_ESP_API int virtual_esp_active_pad() {
    return ready() ? parser->activePad() : 0;
}

VIRTUAL_ESP_API const char *virtual_esp_pad_sound(unsigned pad) {
    return ready() ? parser->padSoundName(pad) : "UNKNOWN";
}

VIRTUAL_ESP_API const char *virtual_esp_pad_variant_name(unsigned pad) {
    return ready() ? parser->padVariantName(pad) : "UNKNOWN";
}

VIRTUAL_ESP_API unsigned virtual_esp_pad_variant(unsigned pad) {
    return ready() ? parser->padVariant(pad) : 0;
}

VIRTUAL_ESP_API int virtual_esp_pad_hold(unsigned pad) {
    return ready() && parser->padHold(pad) ? 1 : 0;
}

VIRTUAL_ESP_API int virtual_esp_pad_hold_supported(unsigned pad) {
    return ready() && parser->padHoldSupported(pad) ? 1 : 0;
}

}  // extern "C"
