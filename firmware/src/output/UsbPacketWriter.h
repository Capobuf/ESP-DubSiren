#pragma once

#include <Arduino.h>

#include <stddef.h>
#include <stdint.h>

// Framed STATUS and command-result packets over the USB serial link.
class UsbPacketWriter {
public:
    UsbPacketWriter();
    ~UsbPacketWriter();

    bool begin();
    bool writeStatus(const char *payload, size_t length);
    bool writeCommandResult(const char *command, size_t length, bool accepted);

private:
    bool writePacket(uint8_t type, const uint8_t *payload, size_t length,
                     uint32_t sequence);

    SemaphoreHandle_t mutex_;
    uint32_t statusSequence_;
    uint8_t packetBuffer_[8192];
};
