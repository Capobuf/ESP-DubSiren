#pragma once

#include <Arduino.h>

#include "AudioSink.h"

class UsbPcmSink final : public AudioSink {
public:
    UsbPcmSink();
    ~UsbPcmSink() override;

    bool begin() override;
    bool write(const int16_t *samples, size_t count) override;
    bool writeStatus(const char *payload, size_t length);

private:
    bool writePacket(uint8_t type, const uint8_t *payload, size_t length,
                     uint32_t sequence);

    SemaphoreHandle_t mutex_;
    uint32_t audioSequence_;
    uint32_t statusSequence_;
};
