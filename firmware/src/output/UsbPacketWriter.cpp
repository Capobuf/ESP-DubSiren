#include "UsbPacketWriter.h"

#include "Config.h"

namespace {

constexpr uint8_t kStatusPacket = 0x02;

#pragma pack(push, 1)
struct PacketHeader {
    uint8_t magic[2];
    uint8_t version;
    uint8_t type;
    uint16_t payloadLength;
    uint32_t sequence;
};
#pragma pack(pop)

static_assert(sizeof(PacketHeader) == 10, "Unexpected packet header size");

}  // namespace

bool UsbPacketWriter::writeStatus(const char *payload, size_t length) {
    if (length > UINT16_MAX) {
        return false;
    }
    return writePacket(kStatusPacket,
                       reinterpret_cast<const uint8_t *>(payload), length,
                       statusSequence_++);
}

bool UsbPacketWriter::writeCommandResult(const char *command, size_t length, bool accepted) {
    if (length == 0 || length > 511) return false;
    uint8_t payload[512];
    payload[0] = accepted ? 1 : 0;
    memcpy(payload + 1, command, length);
    return writePacket(0x03, payload, length + 1, statusSequence_++);
}

bool UsbPacketWriter::writePacket(uint8_t type, const uint8_t *payload,
                             size_t length, uint32_t sequence) {
    if (!Serial.isConnected()) return false;
    const PacketHeader header{{'D', 'S'}, Config::kProtocolVersion, type,
                              static_cast<uint16_t>(length), sequence};
    const size_t total = sizeof(header) + length;
    // Write a packet only when the driver has room for all of it.
    if (Serial.availableForWrite() < static_cast<int>(total)) return false;
    return Serial.write(reinterpret_cast<const uint8_t *>(&header), sizeof(header)) == sizeof(header) &&
        Serial.write(payload, length) == length;
}
