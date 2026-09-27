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

}  // extern "C"
