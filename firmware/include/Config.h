#pragma once

#include <Arduino.h>

namespace Config {
constexpr uint32_t kSampleRate = 48000;
constexpr size_t kBlockSamples = 480;
constexpr uint32_t kBlockDurationMs = 10;
constexpr uint8_t kProtocolVersion = 1;
constexpr char kFirmwareVersion[] = "0.8.0";
constexpr int kI2sBclkPin = 16;
constexpr int kI2sLrckPin = 17;
constexpr int kI2sDataPin = 18;
constexpr int kDisplayResetPin = 7;
constexpr int kDisplayDcPin = 8;
constexpr int kDisplayCsPin = 9;
constexpr int kSpiMosiPin = 11;
constexpr int kSpiSckPin = 12;
constexpr int kSpiMisoPin = 13;
constexpr int kDisplayBacklightPin = 14;
}
