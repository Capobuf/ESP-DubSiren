#include <Arduino.h>
#include <atomic>

#include "Config.h"
#include "audio/AudioEngine.h"
#include "control/CommandParser.h"
#include "output/I2sPcm5102Sink.h"
#include "output/UsbPcmSink.h"

namespace {

enum class OutputMode : uint8_t {
    Pc,
    Gpio,
    Both,
};

UsbPcmSink usbSink;
I2sPcm5102Sink i2sSink;
AudioEngine engine;
ControlStore controls;
CommandParser commandParser(controls);
std::atomic<bool> streamEnabled{false};
std::atomic<OutputMode> outputMode{OutputMode::Both};
std::atomic<uint32_t> lastRenderUs{0};
std::atomic<uint32_t> maximumRenderUs{0};
std::atomic<uint32_t> lastCycleUs{0};
std::atomic<uint32_t> maximumCycleUs{0};
std::atomic<uint32_t> maximumUsbWriteUs{0};
std::atomic<uint32_t> maximumI2sWriteUs{0};
bool i2sReady = false;
String commandBuffer;

const char *outputModeName(OutputMode mode) {
    switch (mode) {
        case OutputMode::Pc:
            return "PC";
        case OutputMode::Gpio:
            return "GPIO";
        case OutputMode::Both:
            return "BOTH";
    }
    return "PC";
}

bool parseOutputMode(const String &value, OutputMode &mode) {
    if (value == "PC") {
        mode = OutputMode::Pc;
    } else if (value == "GPIO") {
        mode = OutputMode::Gpio;
    } else if (value == "BOTH") {
        mode = OutputMode::Both;
    } else {
        return false;
    }
    return true;
}

void updateMaximum(std::atomic<uint32_t> &maximum, uint32_t value) {
    uint32_t observed = maximum.load(std::memory_order_relaxed);
    while (value > observed &&
           !maximum.compare_exchange_weak(observed, value,
                                          std::memory_order_relaxed)) {
    }
}

void sendStatus() {
    const OutputMode mode = outputMode.load(std::memory_order_relaxed);
    const String json =
        String("{\"name\":\"DubSiren\",\"protocol\":1,\"sampleRate\":") +
        Config::kSampleRate + ",\"blockSamples\":" + Config::kBlockSamples +
        ",\"firmware\":\"" + Config::kFirmwareVersion +
        "\",\"renderUs\":" + lastRenderUs.load(std::memory_order_relaxed) +
        ",\"maxRenderUs\":" +
        maximumRenderUs.load(std::memory_order_relaxed) +
        ",\"cycleUs\":" + lastCycleUs.load(std::memory_order_relaxed) +
        ",\"maxCycleUs\":" +
        maximumCycleUs.load(std::memory_order_relaxed) +
        ",\"maxUsbWriteUs\":" +
        maximumUsbWriteUs.load(std::memory_order_relaxed) +
        ",\"maxI2sWriteUs\":" +
        maximumI2sWriteUs.load(std::memory_order_relaxed) +
        ",\"output\":\"" + outputModeName(mode) +
        "\",\"i2sReady\":" + (i2sReady ? "true" : "false") + "}";
    usbSink.writeStatus(json.c_str(), json.length());
}

void handleCommand(String command) {
    command.trim();
    if (command == "HELLO") {
        sendStatus();
    } else if (command == "STREAM 1") {
        streamEnabled.store(true, std::memory_order_release);
    } else if (command == "STREAM 0") {
        streamEnabled.store(false, std::memory_order_release);
    } else if (command.startsWith("SET OUTPUT ")) {
        OutputMode requested;
        if (parseOutputMode(command.substring(11), requested) &&
            (requested == OutputMode::Pc || i2sReady)) {
            outputMode.store(requested, std::memory_order_release);
        }
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
    bool i2sWasActive = false;

    while (true) {
        const uint32_t cycleStartUs = micros();
        const ControlState snapshot = controls.snapshot();
        const uint32_t renderStartUs = micros();
        engine.render(snapshot, block, Config::kBlockSamples);
        const uint32_t renderUs = micros() - renderStartUs;
        lastRenderUs.store(renderUs, std::memory_order_relaxed);
        updateMaximum(maximumRenderUs, renderUs);

        const bool enabled = streamEnabled.load(std::memory_order_acquire);
        const OutputMode mode = outputMode.load(std::memory_order_acquire);
        const bool pcActive =
            enabled && (mode == OutputMode::Pc || mode == OutputMode::Both);
        const bool i2sActive =
            enabled && i2sReady &&
            (mode == OutputMode::Gpio || mode == OutputMode::Both);
        if (pcActive) {
            const uint32_t writeStartUs = micros();
            usbSink.write(block, Config::kBlockSamples);
            updateMaximum(maximumUsbWriteUs, micros() - writeStartUs);
        }
        if (i2sActive) {
            const uint32_t writeStartUs = micros();
            i2sSink.write(block, Config::kBlockSamples);
            updateMaximum(maximumI2sWriteUs, micros() - writeStartUs);
        } else if (i2sWasActive) {
            i2sSink.silence();
        }
        i2sWasActive = i2sActive;

        const uint32_t cycleUs = micros() - cycleStartUs;
        lastCycleUs.store(cycleUs, std::memory_order_relaxed);
        updateMaximum(maximumCycleUs, cycleUs);
        vTaskDelayUntil(&nextWake, pdMS_TO_TICKS(Config::kBlockDurationMs));
    }
}

}  // namespace

void setup() {
    // One packet is 970 bytes; the driver's 256-byte default causes writes to
    // wait for USB during the audio task. Allocate the ring before CDC begins.
    Serial.setTxBufferSize(8192);
    Serial.begin(115200);
    commandBuffer.reserve(128);
    usbSink.begin();
    i2sReady = i2sSink.begin();
    if (!i2sReady) {
        outputMode.store(OutputMode::Pc, std::memory_order_relaxed);
    }
    engine.begin();
    xTaskCreatePinnedToCore(audioTask, "audio", 6144, nullptr, 2, nullptr, 1);
}

void loop() {
    pollCommands();
    delay(1);
}
