#pragma once

#include <Arduino.h>

namespace Config {
constexpr uint32_t kSampleRate = 48000;
constexpr size_t kBlockSamples = 480;
constexpr uint32_t kBlockDurationMs = 10;
constexpr uint8_t kProtocolVersion = 1;
constexpr char kFirmwareVersion[] = "0.2.0";
}
