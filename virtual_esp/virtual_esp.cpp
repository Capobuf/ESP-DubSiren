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

VIRTUAL_ESP_API unsigned virtual_esp_dropped_activations() {
    return ready() ? engine->droppedActivations() : 0;
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

VIRTUAL_ESP_API unsigned virtual_esp_patch_value_count() { return kSoundPatchValueCount; }
VIRTUAL_ESP_API float virtual_esp_patch_value(unsigned pad, unsigned field) {
    return ready() && pad >= 1 && pad <= 4 ? soundPatchValue(parser->panelSettings().patches[pad-1], field) : 0;
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

VIRTUAL_ESP_API const char *virtual_esp_menu_item_label(unsigned index) {
    return ready() ? parser->menuItemLabel(index) : "BACK";
}

VIRTUAL_ESP_API const char *virtual_esp_menu_item_value(unsigned index) {
    return ready() ? parser->menuItemValue(index) : "";
}

VIRTUAL_ESP_API const char *virtual_esp_menu_title() {
    return ready() ? parser->menuTitle() : "SETTINGS";
}

VIRTUAL_ESP_API const char *virtual_esp_menu_footer() {
    return ready() ? parser->menuFooter() : "";
}

VIRTUAL_ESP_API const char *virtual_esp_overlay_title() {
    return ready() ? parser->overlayTitle() : "";
}

VIRTUAL_ESP_API const char *virtual_esp_overlay_value() {
    return ready() ? parser->overlayValue() : "";
}

VIRTUAL_ESP_API unsigned virtual_esp_settings_revision() {
    return ready() ? parser->panelSettingsRevision() : 0;
}

VIRTUAL_ESP_API unsigned virtual_esp_settings_schema() {
    return ready() ? parser->panelSettings().schemaVersion : 0;
}

VIRTUAL_ESP_API const char *virtual_esp_pot_behavior() {
    return ready() ? potBehaviorName(parser->panelSettings().potBehavior) : "PICKUP";
}

VIRTUAL_ESP_API const char *virtual_esp_encoder_action() {
    return ready() && parser->panelSettings().encoderHoldAction == EncoderHoldAction::LivePitch
        ? "LIVE_PITCH" : "OFF";
}

VIRTUAL_ESP_API unsigned virtual_esp_encoder_step() {
    return ready() ? parser->panelSettings().encoderStepSemitones : 1;
}

VIRTUAL_ESP_API unsigned virtual_esp_encoder_range() {
    return ready() ? parser->panelSettings().encoderRangeSemitones : 12;
}

VIRTUAL_ESP_API const char *virtual_esp_pot_target(unsigned profile, unsigned pot) {
    if (!ready() || profile > 1 || pot < 1 || pot > 6) return "UNKNOWN";
    return potTargetName(parser->panelSettings().profiles[profile].pots[pot - 1].target);
}

VIRTUAL_ESP_API const char *virtual_esp_pot_mode(unsigned profile, unsigned pot) {
    if (!ready() || profile > 1 || pot < 1 || pot > 6) return "CONTINUOUS";
    return potModeName(parser->panelSettings().profiles[profile].pots[pot - 1].mode);
}

VIRTUAL_ESP_API float virtual_esp_pot_min(unsigned profile, unsigned pot) {
    return ready() && profile < 2 && pot >= 1 && pot <= 6
        ? parser->panelSettings().profiles[profile].pots[pot - 1].minimum : 0.0f;
}

VIRTUAL_ESP_API float virtual_esp_pot_max(unsigned profile, unsigned pot) {
    return ready() && profile < 2 && pot >= 1 && pot <= 6
        ? parser->panelSettings().profiles[profile].pots[pot - 1].maximum : 1.0f;
}

VIRTUAL_ESP_API float virtual_esp_pot_zone(unsigned profile, unsigned pot, unsigned zone) {
    return ready() && profile < 2 && pot >= 1 && pot <= 6 && zone < 3
        ? parser->panelSettings().profiles[profile].pots[pot - 1].zones[zone] : 0.0f;
}

VIRTUAL_ESP_API const char *virtual_esp_button_action(unsigned profile, unsigned button) {
    if (!ready() || profile > 1 || button < 1 || button > 4) return "UNKNOWN";
    return buttonActionName(parser->panelSettings().profiles[profile].buttons[button - 1].action);
}

VIRTUAL_ESP_API const char *virtual_esp_button_sound(unsigned profile, unsigned button) {
    if (!ready() || profile > 1 || button < 1 || button > 4) return "UNKNOWN";
    return panelSoundName(parser->panelSettings().profiles[profile].buttons[button - 1].sound);
}

VIRTUAL_ESP_API unsigned virtual_esp_button_variant(unsigned profile, unsigned button) {
    return ready() && profile < 2 && button >= 1 && button <= 4
        ? parser->panelSettings().profiles[profile].buttons[button - 1].variant : 0;
}

VIRTUAL_ESP_API int virtual_esp_button_latch(unsigned profile, unsigned button) {
    return ready() && profile < 2 && button >= 1 && button <= 4 &&
        parser->panelSettings().profiles[profile].buttons[button - 1].latch;
}

VIRTUAL_ESP_API const char *virtual_esp_profile() {
    return controls->snapshot().profile == PerformanceProfile::Classic ? "CLASSIC" : "EXTENDED";
}
VIRTUAL_ESP_API const char *virtual_esp_voice() { return modeName(controls->snapshot().mode); }
VIRTUAL_ESP_API const char *virtual_esp_pitch() { return pitchName(controls->snapshot().classicPitch); }
VIRTUAL_ESP_API const char *virtual_esp_rate() { return rateName(controls->snapshot().classicModulation); }
VIRTUAL_ESP_API int virtual_esp_hold() { return controls->snapshot().hold; }
VIRTUAL_ESP_API int virtual_esp_echo_cut() { return controls->snapshot().echoCut; }

VIRTUAL_ESP_API float virtual_esp_tune() { return controls->snapshot().tuneHz; }
VIRTUAL_ESP_API float virtual_esp_rate_hz() {
    const auto state = controls->snapshot();
    return state.tempo.lfoSync ? state.tempo.lfoHz : state.lfoRateHz;
}
VIRTUAL_ESP_API float virtual_esp_pitch_position() { return controls->snapshot().pitchPosition; }
VIRTUAL_ESP_API float virtual_esp_rate_position() { return controls->snapshot().ratePosition; }
VIRTUAL_ESP_API float virtual_esp_echo_level() { return controls->snapshot().echoLevel; }
VIRTUAL_ESP_API float virtual_esp_delay_ms() {
    const auto state = controls->snapshot();
    return state.tempo.echoSync ? state.tempo.echoMs : state.delayMs;
}
VIRTUAL_ESP_API const char *virtual_esp_tempo_json() {
    static char json[640];
    parser->formatTempoJson(json, sizeof(json));
    return json;
}
VIRTUAL_ESP_API float virtual_esp_feedback() { return controls->snapshot().feedback; }
VIRTUAL_ESP_API float virtual_esp_echo_filter() { return controls->snapshot().echoFilterPosition; }
VIRTUAL_ESP_API float virtual_esp_master() { return controls->snapshot().masterVolume; }
VIRTUAL_ESP_API int virtual_esp_live_transpose() { return controls->snapshot().liveTransposeSemitones; }
VIRTUAL_ESP_API int virtual_esp_pitch_zones_active() { return controls->snapshot().pitchZonesActive; }
VIRTUAL_ESP_API int virtual_esp_rate_zones_active() { return controls->snapshot().rateZonesActive; }
VIRTUAL_ESP_API const char *virtual_esp_echo_filter_name() {
    static char value[24];
    if (ready()) parser->formatCurrentValue(PotTarget::EchoFilter, value, sizeof(value));
    else value[0] = '\0';
    return value;
}

VIRTUAL_ESP_API float virtual_esp_depth() {
    return ready() ? controls->snapshot().lfoDepthOctaves : 0.0f;
}

VIRTUAL_ESP_API int virtual_esp_echo_spin() { return ready() && controls->snapshot().echoSpin; }

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
