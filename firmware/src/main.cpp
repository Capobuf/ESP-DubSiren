#include <Arduino.h>

#include "Config.h"

namespace {

enum class PacketType : uint8_t {
    Audio = 0x01,
    Status = 0x02,
};

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

uint32_t statusSequence = 0;
String commandBuffer;

void sendStatus() {
    const String json =
        String("{\"name\":\"DubSiren\",\"protocol\":1,\"sampleRate\":") +
        Config::kSampleRate + ",\"blockSamples\":" + Config::kBlockSamples +
        ",\"firmware\":\"" + Config::kFirmwareVersion + "\"}";
    const PacketHeader header{{'D', 'S'}, Config::kProtocolVersion,
                              static_cast<uint8_t>(PacketType::Status),
                              static_cast<uint16_t>(json.length()), statusSequence++};
    Serial.write(reinterpret_cast<const uint8_t *>(&header), sizeof(header));
    Serial.write(reinterpret_cast<const uint8_t *>(json.c_str()), json.length());
}

void handleCommand(String command) {
    command.trim();
    if (command == "HELLO") {
        sendStatus();
    }
}

void pollCommands() {
    while (Serial.available() > 0) {
        const char value = static_cast<char>(Serial.read());
        if (value == '\n') {
            handleCommand(commandBuffer);
            commandBuffer = "";
        } else if (value != '\r') {
            if (commandBuffer.length() < 127) {
                commandBuffer += value;
            } else {
                commandBuffer = "";
            }
        }
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    commandBuffer.reserve(128);
}

void loop() {
    pollCommands();
    delay(1);
}
