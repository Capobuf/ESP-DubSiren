#include <Arduino.h>
#include <Preferences.h>
#include <atomic>

#include "Config.h"
#include "audio/AudioEngine.h"
#include "control/CommandParser.h"
#include "control/PanelInputs.h"
#include "display/PanelDisplay.h"
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
PanelDisplay panelDisplay(controls, commandParser);
PanelInputs panelInputs(controls, commandParser);
std::atomic<bool> streamEnabled{false};
std::atomic<bool> localAudioEnabled{true};
std::atomic<uint32_t> usbDroppedPackets{0};
std::atomic<uint32_t> i2sWriteFailures{0};
std::atomic<OutputMode> outputMode{OutputMode::Both};
std::atomic<uint32_t> lastRenderUs{0};
std::atomic<uint32_t> droppedActivations{0};
std::atomic<uint32_t> maximumRenderUs{0};
std::atomic<uint32_t> lastCycleUs{0};
std::atomic<uint32_t> maximumCycleUs{0};
std::atomic<uint32_t> maximumUsbWriteUs{0};
std::atomic<uint32_t> maximumI2sWriteUs{0};
bool i2sReady = false;
bool hostLinked = false;
String commandBuffer;
bool discardCommand = false;
Preferences panelPreferences;
bool panelPreferencesReady = false;
unsigned savedPanelSettingsRevision = 0;

String panelSettingsJson() {
    const PanelSettings &settings = commandParser.panelSettings();
    String json = String("{\"schema\":") + settings.schemaVersion +
        ",\"revision\":" + commandParser.panelSettingsRevision() +
        ",\"potBehavior\":\"" + potBehaviorName(settings.potBehavior) +
        "\",\"encoder\":{\"holdTurn\":\"" +
        (settings.encoderHoldAction == EncoderHoldAction::LivePitch
            ? "LIVE_PITCH" : "OFF") +
        "\",\"step\":" + static_cast<unsigned>(settings.encoderStepSemitones) +
        ",\"range\":" + static_cast<unsigned>(settings.encoderRangeSemitones) + "},\"profiles\":{";
    for (unsigned profile = 0; profile < 2; ++profile) {
        if (profile) json += ",";
        json += profile == 0 ? "\"CLASSIC\":{" : "\"EXTENDED\":{";
        json += "\"pots\":[";
        for (unsigned pot = 0; pot < 6; ++pot) {
            if (pot) json += ",";
            const PotConfig &config = settings.profiles[profile].pots[pot];
            json += String("{\"target\":\"") + potTargetName(config.target) +
                "\",\"mode\":\"" + potModeName(config.mode) +
                "\",\"min\":" + String(config.minimum, 5) +
                ",\"max\":" + String(config.maximum, 5) +
                ",\"zones\":[" + String(config.zones[0], 5) + "," +
                String(config.zones[1], 5) + "," + String(config.zones[2], 5) + "]}";
        }
        json += "],\"buttons\":[";
        for (unsigned button = 0; button < 4; ++button) {
            if (button) json += ",";
            const ButtonConfig &config = settings.profiles[profile].buttons[button];
            json += String("{\"action\":\"") + buttonActionName(config.action) +
                "\",\"sound\":\"" + panelSoundName(config.sound) +
                "\",\"variant\":" + static_cast<unsigned>(config.variant) +
                ",\"latch\":" + (config.latch ? "true" : "false") + "}";
        }
        json += "]}";
    }
    json += "},\"patches\":[";
    for (unsigned pad = 0; pad < 4; ++pad) {
        if (pad) json += ",";
        json += "[";
        for (unsigned field = 0; field < kSoundPatchValueCount; ++field) {
            if (field) json += ",";
            json += String(soundPatchValue(settings.patches[pad], field), 6);
        }
        json += "]";
    }
    json += "]}";
    return json;
}

void loadPanelSettings() {
    panelPreferencesReady = panelPreferences.begin("panel", false);
    if (!panelPreferencesReady) {
        commandParser.setPersistenceStatus("NVS UNAVAILABLE");
        return;
    }
    const size_t storedSize = panelPreferences.getBytesLength("settings");
    if (storedSize == 0) {
        savedPanelSettingsRevision = commandParser.panelSettingsRevision();
        return;
    }
    PanelSettings stored;
    bool loaded = false;
    if (storedSize == sizeof(PanelSettingsV1)) {
        PanelSettingsV1 legacy;
        loaded = panelPreferences.getBytes("settings", &legacy, sizeof(legacy)) == sizeof(legacy) && migratePanelSettings(legacy, stored);
    } else if (storedSize == sizeof(stored)) {
        loaded = panelPreferences.getBytes("settings", &stored, sizeof(stored)) == sizeof(stored);
    }
    if (!loaded || !commandParser.applyPanelSettings(stored)) {
        commandParser.setPersistenceStatus("INVALID SAVED CONFIG");
        savedPanelSettingsRevision = commandParser.panelSettingsRevision();
        return;
    }
    savedPanelSettingsRevision = commandParser.panelSettingsRevision();
}

void savePanelSettingsIfNeeded() {
    const unsigned revision = commandParser.panelSettingsRevision();
    if (revision == savedPanelSettingsRevision) return;
    savedPanelSettingsRevision = revision;
    if (!panelPreferencesReady) {
        commandParser.setPersistenceStatus("SAVE FAILED - NVS");
        return;
    }
    const PanelSettings &settings = commandParser.panelSettings();
    if (panelPreferences.putBytes("settings", &settings, sizeof(settings)) != sizeof(settings))
        commandParser.setPersistenceStatus("SAVE FAILED");
    else
        commandParser.setPersistenceStatus("");
}

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
    const ControlState displayState = controls.snapshot();
    const bool pitchZones = displayState.pitchZonesActive;
    const bool rateZones = displayState.rateZonesActive;
    char filterValue[24];
    commandParser.formatCurrentValue(PotTarget::EchoFilter,
                                     filterValue, sizeof(filterValue));
    String pads = "[";
    for (unsigned pad = 1; pad <= 4; ++pad) {
        if (pad > 1) pads += ",";
        pads += String("{\"sound\":\"") + commandParser.padSoundName(pad) +
            "\",\"variant\":\"" + commandParser.padVariantName(pad) +
            "\",\"variantIndex\":" + commandParser.padVariant(pad) +
            ",\"hold\":" + (commandParser.padHold(pad) ? "true" : "false") +
            ",\"holdSupported\":" +
            (commandParser.padHoldSupported(pad) ? "true" : "false") + "}";
    }
    pads += "]";
    String menuItems = "[";
    String menuRows = "[";
    for (unsigned index = 0; index < commandParser.menuItemCount(); ++index) {
        if (index > 0) { menuItems += ","; menuRows += ","; }
        menuItems += String("\"") + commandParser.menuItemKey(index) + "\"";
        menuRows += String("{\"key\":\"") + commandParser.menuItemKey(index) +
            "\",\"label\":\"" + commandParser.menuItemLabel(index) +
            "\",\"value\":\"" + commandParser.menuItemValue(index) + "\"}";
    }
    menuItems += "]";
    menuRows += "]";
    String buttons = "[";
    for (unsigned button = 1; button <= 4; ++button) {
        if (button > 1) buttons += ",";
        buttons += String("\"") + commandParser.buttonActionNameFor(
            displayState.profile, button) + "\"";
    }
    buttons += "]";
    const String json =
        String("{\"name\":\"DubSiren\",\"protocol\":1,\"sampleRate\":") +
        Config::kSampleRate + ",\"blockSamples\":" + Config::kBlockSamples +
        ",\"firmware\":\"" + Config::kFirmwareVersion +
        "\",\"renderUs\":" + lastRenderUs.load(std::memory_order_relaxed) +
        ",\"maxRenderUs\":" +
        maximumRenderUs.load(std::memory_order_relaxed) +
        ",\"droppedActivations\":" + droppedActivations.load(std::memory_order_relaxed) +
        ",\"cycleUs\":" + lastCycleUs.load(std::memory_order_relaxed) +
        ",\"maxCycleUs\":" +
        maximumCycleUs.load(std::memory_order_relaxed) +
        ",\"maxUsbWriteUs\":" +
        maximumUsbWriteUs.load(std::memory_order_relaxed) +
        ",\"maxI2sWriteUs\":" +
        maximumI2sWriteUs.load(std::memory_order_relaxed) +
        ",\"displayRenderUs\":" + panelDisplay.lastRenderUs() +
        ",\"maxDisplayRenderUs\":" + panelDisplay.maximumRenderUs() +
        ",\"output\":\"" + outputModeName(mode) +
        "\",\"i2sReady\":" + (i2sReady ? "true" : "false") +
        ",\"localAudio\":" + (localAudioEnabled.load() ? "true" : "false") +
        ",\"usbStreaming\":" + (streamEnabled.load() ? "true" : "false") +
        ",\"usbDroppedPackets\":" + usbDroppedPackets.load() +
        ",\"i2sWriteFailures\":" + i2sWriteFailures.load() +
        ",\"panelInputsReady\":" + (panelInputs.ready() ? "true" : "false") +
        ",\"maxInputPollGapUs\":" + panelInputs.maximumPollGapUs() +
        ",\"panel\":{\"revision\":" + commandParser.panelRevision() +
        ",\"settings\":" + panelSettingsJson() +
        ",\"persistenceStatus\":\"" + commandParser.persistenceStatus() + "\"" +
        ",\"menuRevision\":" + commandParser.panelMenuRevision() +
        ",\"selectedPad\":" + commandParser.selectedPad() +
        ",\"currentPad\":" + commandParser.currentPad() +
        ",\"activePad\":" + commandParser.activePad() +
        ",\"pads\":" + pads +
        ",\"display\":{\"width\":" + commandParser.displayWidth() +
        ",\"height\":" + commandParser.displayHeight() +
        ",\"mode\":\"" + commandParser.displayModeName() +
        "\",\"menuIndex\":" + commandParser.menuIndex() +
        ",\"editing\":" + (commandParser.menuEditing() ? "true" : "false") +
        ",\"menuItems\":" + menuItems +
        ",\"title\":\"" + commandParser.menuTitle() +
        "\",\"footer\":\"" + commandParser.menuFooter() +
        "\",\"rows\":" + menuRows +
        ",\"overlay\":{\"title\":\"" + commandParser.overlayTitle() +
        "\",\"value\":\"" + commandParser.overlayValue() + "\"}" +
        ",\"buttons\":" + buttons +
        ",\"profile\":\"" + (displayState.profile == PerformanceProfile::Classic ? "CLASSIC" : "EXTENDED") +
        "\",\"voice\":\"" + modeName(displayState.mode) +
        "\",\"pitch\":\"" + pitchName(displayState.classicPitch) +
        "\",\"rate\":\"" + rateName(displayState.classicModulation) + "\"" +
        ",\"pitchMode\":\"" + (pitchZones ? "ZONES" : "CONTINUOUS") + "\"" +
        ",\"rateMode\":\"" + (rateZones ? "ZONES" : "CONTINUOUS") + "\"" +
        ",\"hold\":" + (displayState.hold ? "true" : "false") +
        ",\"echoCut\":" + (displayState.echoCut ? "true" : "false") +
        ",\"tuneHz\":" + String(displayState.tuneHz, 4) +
        ",\"rateHz\":" + String(displayState.lfoRateHz, 4) +
        ",\"liveTranspose\":" + static_cast<int>(displayState.liveTransposeSemitones) +
        ",\"pitchPosition\":" + String(displayState.pitchPosition, 4) +
        ",\"ratePosition\":" + String(displayState.ratePosition, 4) +
        ",\"echoLevel\":" + String(displayState.echoLevel, 4) +
        ",\"delayMs\":" + String(displayState.delayMs, 4) +
        ",\"feedback\":" + String(displayState.feedback, 4) +
        ",\"echoFilter\":" + String(displayState.echoFilterPosition, 4) +
        ",\"filter\":\"" + filterValue + "\"" +
        ",\"master\":" + String(displayState.masterVolume, 4) +
        ",\"depth\":" + String(displayState.lfoDepthOctaves, 4) +
        ",\"echoSpin\":" + (displayState.echoSpin ? "true" : "false") +
        ",\"echoLoop\":" + (displayState.echoLoop ? "true" : "false") +
        "}}}";
    usbSink.writeStatus(json.c_str(), json.length());
}

void handleCommand(String command) {
    command.trim();
    if (command == "HELLO") {
        hostLinked = true;
        sendStatus();
    } else if (command == "STREAM 1") {
        streamEnabled.store(true, std::memory_order_release);
    } else if (command == "STREAM 0") {
        streamEnabled.store(false, std::memory_order_release);
    } else if (command == "LOCAL_AUDIO 0" || command == "LOCAL_AUDIO 1") {
        localAudioEnabled.store(command == "LOCAL_AUDIO 1", std::memory_order_release);
    } else if (command.startsWith("SET OUTPUT ")) {
        OutputMode requested;
        if (parseOutputMode(command.substring(11), requested) &&
            (requested == OutputMode::Pc || i2sReady)) {
            outputMode.store(requested, std::memory_order_release);
        }
    } else {
        const unsigned panelRevision = commandParser.panelRevision();
        if (commandParser.parse(command) &&
            commandParser.panelRevision() != panelRevision) {
            savePanelSettingsIfNeeded();
            sendStatus();
        }
    }
}

void pollCommands() {
    unsigned budget = 64;
    while (budget-- && Serial.available() > 0) {
        const char value = static_cast<char>(Serial.read());
        if (value == '\n') {
            if (!discardCommand) handleCommand(commandBuffer);
            commandBuffer = "";
            discardCommand = false;
        } else if (value != '\r' && !discardCommand) {
            if (commandBuffer.length() < 511) {
                commandBuffer += value;
            } else {
                discardCommand = true;
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
        droppedActivations.store(engine.droppedActivations(), std::memory_order_relaxed);
        const uint32_t renderUs = micros() - renderStartUs;
        lastRenderUs.store(renderUs, std::memory_order_relaxed);
        updateMaximum(maximumRenderUs, renderUs);
        const bool enabled = streamEnabled.load(std::memory_order_acquire);
        const OutputMode mode = outputMode.load(std::memory_order_acquire);
        const bool pcActive =
            enabled && (mode == OutputMode::Pc || mode == OutputMode::Both);
        const bool i2sActive =
            localAudioEnabled.load(std::memory_order_acquire) && i2sReady &&
            (mode == OutputMode::Gpio || mode == OutputMode::Both);
        if (i2sActive) {
            const uint32_t writeStartUs = micros();
            if (!i2sSink.write(block, Config::kBlockSamples)) ++i2sWriteFailures;
            updateMaximum(maximumI2sWriteUs, micros() - writeStartUs);
        } else if (i2sWasActive) {
            i2sSink.silence();
        }
        i2sWasActive = i2sActive;
        if (pcActive) {
            const uint32_t writeStartUs = micros();
            if (!usbSink.write(block, Config::kBlockSamples)) ++usbDroppedPackets;
            updateMaximum(maximumUsbWriteUs, micros() - writeStartUs);
        }

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
    Serial.setTxTimeoutMs(0);
    commandBuffer.reserve(512);
    usbSink.begin();
    i2sReady = i2sSink.begin();
    if (!i2sReady) {
        outputMode.store(OutputMode::Pc, std::memory_order_relaxed);
        commandParser.setDeviceStatus("I2S INIT FAILED");
    }
    engine.begin();
    loadPanelSettings();
#if defined(DUBSIREN_PHYSICAL_PANEL) && DUBSIREN_PHYSICAL_PANEL
    if (!panelInputs.begin()) commandParser.setDeviceStatus("PANEL INIT FAILED");
#endif
    if (xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr, 2, nullptr, 1) != pdPASS)
        commandParser.setDeviceStatus("AUDIO TASK FAILED");
    panelDisplay.begin();
}

void loop() {
    panelInputs.poll();
    pollCommands();
    if (!Serial.isConnected()) { hostLinked = false; streamEnabled.store(false); }
    if (i2sWriteFailures.load()) commandParser.setDeviceStatus("I2S WRITE FAILED");
    static uint32_t lastStatusMs = 0;
    static unsigned statusRevision = 0;
    if (hostLinked && millis() - lastStatusMs >= 200 && statusRevision != commandParser.panelRevision()) {
        lastStatusMs = millis(); statusRevision = commandParser.panelRevision(); sendStatus();
    }
    savePanelSettingsIfNeeded();
    panelDisplay.update(hostLinked);
    delay(1);
}
