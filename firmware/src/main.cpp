#include <Arduino.h>
#include "Config.h"
#include "audio/AudioEngine.h"
#include "control/CommandParser.h"
#include "output/UsbPcmSink.h"

namespace {

UsbPcmSink sink;
AudioEngine engine;
ControlStore controls;
CommandParser commandParser(controls);
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
    } else {
        commandParser.parse(command);
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
    TickType_t nextWake = xTaskGetTickCount();

    while (true) {
        const ControlState snapshot = controls.snapshot();
        engine.render(snapshot, block, Config::kBlockSamples);
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
    engine.begin();
    xTaskCreatePinnedToCore(audioTask, "audio", 6144, nullptr, 2, nullptr, 1);
}

void loop() {
    pollCommands();
    delay(1);
}
