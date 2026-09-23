#include <Arduino.h>
#include <math.h>

#include "Config.h"
#include "output/UsbPcmSink.h"

namespace {

UsbPcmSink sink;
volatile bool streamEnabled = false;
String commandBuffer;

void sendStatus() {
    const String json =
        String("{\"name\":\"DubSiren\",\"protocol\":1,\"sampleRate\":") +
        Config::kSampleRate + ",\"blockSamples\":" + Config::kBlockSamples +
        ",\"firmware\":\"" + Config::kFirmwareVersion + "\"}";
    sink.writeStatus(json.c_str(), json.length());
}

void handleCommand(String command) {
    command.trim();
    if (command == "HELLO") {
        sendStatus();
    } else if (command == "STREAM 1") {
        streamEnabled = true;
    } else if (command == "STREAM 0") {
        streamEnabled = false;
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

void audioTask(void *) {
    int16_t block[Config::kBlockSamples];
    float phase = 0.0f;
    constexpr float phaseIncrement = 440.0f / Config::kSampleRate;
    TickType_t nextWake = xTaskGetTickCount();

    while (true) {
        for (size_t i = 0; i < Config::kBlockSamples; ++i) {
            block[i] = static_cast<int16_t>(sinf(phase * TWO_PI) * 8192.0f);
            phase += phaseIncrement;
            if (phase >= 1.0f) {
                phase -= 1.0f;
            }
        }
        if (streamEnabled) {
            sink.write(block, Config::kBlockSamples);
        }
        vTaskDelayUntil(&nextWake, pdMS_TO_TICKS(Config::kBlockDurationMs));
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    commandBuffer.reserve(128);
    sink.begin();
    xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 2, nullptr, 1);
}

void loop() {
    pollCommands();
    delay(1);
}
