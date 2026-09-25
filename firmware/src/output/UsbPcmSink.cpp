#include "UsbPcmSink.h"

#include "Config.h"

namespace {

constexpr uint8_t kAudioPacket = 0x01;
constexpr uint8_t kStatusPacket = 0x02;

bool writeAll(const uint8_t *bytes, size_t length) {
    const uint32_t deadline = millis() + 500;
    size_t written = 0;
    while (written < length) {
        written += Serial.write(bytes + written, length - written);
        if (written == length) break;
        if (static_cast<int32_t>(millis() - deadline) >= 0) return false;
        vTaskDelay(1);
    }
    return true;
}

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

UsbPcmSink::UsbPcmSink()
    : mutex_(nullptr), audioSequence_(0), statusSequence_(0) {}

UsbPcmSink::~UsbPcmSink() {
    if (mutex_ != nullptr) {
        vSemaphoreDelete(mutex_);
    }
}

bool UsbPcmSink::begin() {
    mutex_ = xSemaphoreCreateMutex();
    return mutex_ != nullptr;
}

bool UsbPcmSink::write(const int16_t *samples, size_t count) {
    const size_t byteCount = count * sizeof(int16_t);
    if (samples == nullptr || byteCount > UINT16_MAX) return false;
    return writePacket(kAudioPacket,
                       reinterpret_cast<const uint8_t *>(samples), byteCount,
                       audioSequence_++);
}

bool UsbPcmSink::writeStatus(const char *payload, size_t length) {
    if (length > UINT16_MAX) {
        return false;
    }
    return writePacket(kStatusPacket,
                       reinterpret_cast<const uint8_t *>(payload), length,
                       statusSequence_++);
}

bool UsbPcmSink::writePacket(uint8_t type, const uint8_t *payload,
                             size_t length, uint32_t sequence) {
    if (mutex_ == nullptr ||
        xSemaphoreTake(mutex_, pdMS_TO_TICKS(500)) != pdTRUE) {
        return false;
    }

    const PacketHeader header{{'D', 'S'}, Config::kProtocolVersion, type,
                              static_cast<uint16_t>(length), sequence};
    const bool headerWritten = writeAll(
        reinterpret_cast<const uint8_t *>(&header), sizeof(header));
    const bool payloadWritten = headerWritten && writeAll(payload, length);
    xSemaphoreGive(mutex_);
    return headerWritten && payloadWritten;
}
