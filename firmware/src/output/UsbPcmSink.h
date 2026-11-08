#pragma once

#include <Arduino.h>

#include <stddef.h>
#include <stdint.h>

class UsbPcmSink {
public:
    UsbPcmSink();
    ~UsbPcmSink();

    bool begin();
    bool write(const int16_t *samples, size_t count);
    bool writeStatus(const char *payload, size_t length);
    bool writeCommandResult(const char *command, size_t length, bool accepted);

private:
    bool writePacket(uint8_t type, const uint8_t *payload, size_t length,
                     uint32_t sequence);

    SemaphoreHandle_t mutex_;
    uint32_t audioSequence_;
    uint32_t statusSequence_;
    uint8_t packetBuffer_[8192];
};
