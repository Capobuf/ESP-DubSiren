#include <Arduino.h>
#include <Preferences.h>
#include <atomic>

#include "Config.h"
#include "audio/AudioEngine.h"
#include "control/CommandParser.h"
#include "control/PanelInputs.h"
#include "display/PanelDisplay.h"
#include "output/I2sPcm5102Sink.h"
#include "output/UsbPacketWriter.h"

namespace {

UsbPacketWriter usbLink;
I2sPcm5102Sink i2sSink;
AudioEngine engine;
ControlStore controls;
CommandParser commandParser(controls);
PanelDisplay panelDisplay(controls, commandParser);
PanelInputs panelInputs(controls, commandParser);
std::atomic<uint32_t> i2sWriteFailures{0};
std::atomic<uint32_t> lastRenderUs{0};
std::atomic<uint32_t> droppedActivations{0};
std::atomic<uint32_t> maximumRenderUs{0};
std::atomic<uint32_t> lastCycleUs{0};
std::atomic<uint32_t> maximumCycleUs{0};
std::atomic<uint32_t> maximumI2sWriteUs{0};
bool i2sReady = false;
bool hostLinked = false;
String commandBuffer;
String pendingCommandResult;
bool pendingCommandAccepted = false;
bool discardCommand = false;
Preferences panelPreferences;
bool panelPreferencesReady = false;
unsigned savedPanelSettingsRevision = 0;
unsigned pendingPanelSettingsRevision = 0;
uint32_t panelSettingsChangedMs = 0;
// Flash writes stall the cache (and the audio task): save only once settings
// have stopped changing, not on every encoder detent.
constexpr uint32_t kPanelSaveSettleMs = 2000;
// Without bytes from a linked host (the PC sends a keepalive every ~1 s),
// release remote inputs: HWCDC stays connected while the cable is plugged.
constexpr uint32_t kHostSilenceTimeoutMs = 3000;
uint32_t lastHostByteMs = 0;

String panelSettingsJson() {
    const PanelSettings &settings = commandParser.panelSettings();
    String json = String("{\"schema\":") + settings.schemaVersion +
        ",\"revision\":" + commandParser.panelSettingsRevision() +
        ",\"encoder\":{\"holdTurn\":\"" +
        (settings.encoderHoldAction == EncoderHoldAction::LivePitch
            ? "LIVE_PITCH" : "OFF") +
        "\",\"step\":" + static_cast<unsigned>(settings.encoderStepSemitones) +
        ",\"range\":" + static_cast<unsigned>(settings.encoderRangeSemitones) + "},\"pots\":[";
    for (unsigned pot = 0; pot < 6; ++pot) {
        if (pot) json += ",";
        const PotConfig &config = settings.pots[pot];
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
        const ButtonConfig &config = settings.buttons[button];
        json += String("{\"action\":\"") + buttonActionName(config.action) +
            "\",\"sound\":\"" + panelSoundName(config.sound) +
            "\",\"variant\":" + static_cast<unsigned>(config.variant) +
            ",\"latch\":" + (config.latch ? "true" : "false") + "}";
    }
    json += "],\"patches\":[";
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
    // Layouts stored by an earlier schema are dropped: factory settings apply.
    if (storedSize != sizeof(stored)) {
        savedPanelSettingsRevision = 0;
        return;
    }
    loaded = panelPreferences.getBytes("settings", &stored, sizeof(stored)) == sizeof(stored);
    if (loaded && stored.schemaVersion != PanelSettings::kSchemaVersion) {
        savedPanelSettingsRevision = 0;
        return;
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
    const uint32_t now = millis();
    if (revision != pendingPanelSettingsRevision) {
        pendingPanelSettingsRevision = revision;
        panelSettingsChangedMs = now;
        return;
    }
    if (now - panelSettingsChangedMs < kPanelSaveSettleMs) return;
    if (!panelPreferencesReady) {
        savedPanelSettingsRevision = revision;
        commandParser.setPersistenceStatus("SAVE FAILED - NVS");
        return;
    }
    const PanelSettings &settings = commandParser.panelSettings();
    if (panelPreferences.putBytes("settings", &settings, sizeof(settings)) != sizeof(settings)) {
        panelSettingsChangedMs = now;  // retry after another settle period
        commandParser.setPersistenceStatus("SAVE FAILED");
    } else {
        savedPanelSettingsRevision = revision;
        commandParser.setPersistenceStatus("");
    }
}

void updateMaximum(std::atomic<uint32_t> &maximum, uint32_t value) {
    uint32_t observed = maximum.load(std::memory_order_relaxed);
    while (value > observed &&
           !maximum.compare_exchange_weak(observed, value,
                                          std::memory_order_relaxed)) {
    }
}

bool sendStatus() {
    const ControlState displayState = controls.snapshot();
    const bool pitchZones = displayState.pitchZonesActive;
    const bool rateZones = displayState.rateZonesActive;
    char filterValue[24];
    char tempoJson[640];
    commandParser.formatTempoJson(tempoJson, sizeof(tempoJson));
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
        buttons += String("\"") + commandParser.buttonActionNameFor(button) + "\"";
    }
    buttons += "]";
    const String json =
        String("{\"name\":\"DubSiren\",\"protocol\":") + Config::kProtocolVersion +
        ",\"sampleRate\":" +
        Config::kSampleRate + ",\"blockSamples\":" + Config::kBlockSamples +
        ",\"firmware\":\"" + Config::kFirmwareVersion +
        "\",\"renderUs\":" + lastRenderUs.load(std::memory_order_relaxed) +
        ",\"maxRenderUs\":" +
        maximumRenderUs.load(std::memory_order_relaxed) +
        ",\"droppedActivations\":" + droppedActivations.load(std::memory_order_relaxed) +
        ",\"cycleUs\":" + lastCycleUs.load(std::memory_order_relaxed) +
        ",\"maxCycleUs\":" +
        maximumCycleUs.load(std::memory_order_relaxed) +
        ",\"maxI2sWriteUs\":" +
        maximumI2sWriteUs.load(std::memory_order_relaxed) +
        ",\"displayRenderUs\":" + panelDisplay.lastRenderUs() +
        ",\"maxDisplayRenderUs\":" + panelDisplay.maximumRenderUs() +
        ",\"i2sReady\":" + (i2sReady ? "true" : "false") +
        ",\"i2sWriteFailures\":" + i2sWriteFailures.load() +
        ",\"panelInputsReady\":" + (panelInputs.ready() ? "true" : "false") +
        ",\"maxInputPollGapUs\":" + panelInputs.maximumPollGapUs() +
        ",\"panel\":{\"revision\":" + commandParser.panelRevision() +
        ",\"settings\":" + panelSettingsJson() +
        ",\"persistenceStatus\":\"" + commandParser.persistenceStatus() + "\"" +
        ",\"deviceStatus\":\"" + commandParser.deviceStatus() + "\"" +
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
        ",\"padSelected\":" + (commandParser.selectedIsPad() ? "true" : "false") +
        ",\"voice\":\"" + commandParser.voiceName() +
        "\",\"pitch\":\"" + pitchName(displayState.classicPitch) +
        "\",\"rate\":\"" + rateName(displayState.classicModulation) + "\"" +
        ",\"pitchMode\":\"" + (pitchZones ? "ZONES" : "CONTINUOUS") + "\"" +
        ",\"rateMode\":\"" + (rateZones ? "ZONES" : "CONTINUOUS") + "\"" +
        ",\"hold\":" + (displayState.hold ? "true" : "false") +
        ",\"echoCut\":" + (displayState.echoCut ? "true" : "false") +
        ",\"tuneHz\":" + String(displayState.tuneHz, 4) +
        ",\"rateHz\":" + String(displayState.tempo.lfoSync ? displayState.tempo.lfoHz : displayState.lfoRateHz, 4) +
        ",\"tempo\":" + tempoJson +
        ",\"liveTranspose\":" + static_cast<int>(displayState.liveTransposeSemitones) +
        ",\"pitchPosition\":" + String(displayState.pitchPosition, 4) +
        ",\"ratePosition\":" + String(displayState.ratePosition, 4) +
        ",\"echoLevel\":" + String(displayState.echoLevel, 4) +
        ",\"delayMs\":" + String(displayState.tempo.echoSync ? displayState.tempo.echoMs : displayState.delayMs, 4) +
        ",\"feedback\":" + String(displayState.feedback, 4) +
        ",\"echoFilter\":" + String(displayState.echoFilterPosition, 4) +
        ",\"filter\":\"" + filterValue + "\"" +
        ",\"master\":" + String(displayState.masterVolume, 4) +
        ",\"depth\":" + String(displayState.lfoDepthOctaves, 4) +
        ",\"echoSpin\":" + (displayState.echoSpin ? "true" : "false") +
        ",\"echoLoop\":" + (displayState.echoLoop ? "true" : "false") +
        "}}}";
    return usbLink.writeStatus(json.c_str(), json.length());
}

void handleCommand(String command) {
    command.trim();
    if (command.isEmpty()) return;
    bool accepted = true;
    if (command == "HELLO") {
        hostLinked = true;
        sendStatus();
        return;
    } else {
        // STATUS follows from loop() at most every 200 ms: one per POT command
        // would crowd the shared TX buffer.
        accepted = commandParser.parse(command);
    }
    pendingCommandResult = command;
    pendingCommandAccepted = accepted;
    if (!accepted) sendStatus();
}

void pollCommands() {
    // Retain one reply until the non-blocking USB writer can accept it.
    // Do not consume another command and overwrite its result under load.
    if (!pendingCommandResult.isEmpty()) {
        if (!usbLink.writeCommandResult(pendingCommandResult.c_str(),
                                       pendingCommandResult.length(), pendingCommandAccepted)) return;
        pendingCommandResult = "";
    }
    unsigned budget = 64;
    while (budget-- && Serial.available() > 0) {
        const char value = static_cast<char>(Serial.read());
        lastHostByteMs = millis();
        if (value == '\n') {
            if (!discardCommand) handleCommand(commandBuffer);
            commandBuffer = "";
            discardCommand = false;
            if (!pendingCommandResult.isEmpty()) break;
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

    while (true) {
        const uint32_t cycleStartUs = micros();
        const ControlState snapshot = controls.snapshot();
        const uint32_t renderStartUs = micros();
        engine.render(snapshot, block, Config::kBlockSamples);
        droppedActivations.store(engine.droppedActivations(), std::memory_order_relaxed);
        const uint32_t renderUs = micros() - renderStartUs;
        lastRenderUs.store(renderUs, std::memory_order_relaxed);
        updateMaximum(maximumRenderUs, renderUs);
        if (i2sReady) {
            const uint32_t writeStartUs = micros();
            if (!i2sSink.write(block, Config::kBlockSamples)) ++i2sWriteFailures;
            updateMaximum(maximumI2sWriteUs, micros() - writeStartUs);
        }

        const uint32_t cycleUs = micros() - cycleStartUs;
        lastCycleUs.store(cycleUs, std::memory_order_relaxed);
        updateMaximum(maximumCycleUs, cycleUs);
        vTaskDelayUntil(&nextWake, pdMS_TO_TICKS(Config::kBlockDurationMs));
    }
}

}  // namespace

void setup() {
    // A STATUS packet is several kB; the driver's 256-byte default could not
    // hold one. Allocate the ring before CDC begins.
    Serial.setTxBufferSize(8192);
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    commandBuffer.reserve(512);
    if (!usbLink.begin()) commandParser.setDeviceStatus("USB LINK INIT FAILED");
    i2sReady = i2sSink.begin();
    if (!i2sReady) commandParser.setDeviceStatus("I2S INIT FAILED");
    engine.begin();
    loadPanelSettings();
#if defined(DUBSIREN_PHYSICAL_PANEL) && DUBSIREN_PHYSICAL_PANEL
    if (!panelInputs.begin()) commandParser.setDeviceStatus("PANEL INIT FAILED");
#endif
    if (xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr, 2, nullptr, 1) != pdPASS)
        commandParser.setDeviceStatus("AUDIO TASK FAILED");
    if (!panelDisplay.begin()) commandParser.setDeviceStatus("DISPLAY INIT FAILED");
}

void loop() {
    static bool serialWasConnected = false;
    panelInputs.poll();
    pollCommands();
    const bool serialConnected = Serial.isConnected();
    if (!serialConnected) {
        if (serialWasConnected || hostLinked) {
            commandParser.resetRemoteInputs();
            commandParser.parse("PANEL CANCEL");
        }
        hostLinked = false;
        pendingCommandResult = "";
        commandBuffer = "";
        discardCommand = false;
    }
    serialWasConnected = serialConnected;
    static bool hostSilenceHandled = false;
    if (hostLinked && millis() - lastHostByteMs > kHostSilenceTimeoutMs) {
        if (!hostSilenceHandled) {
            commandParser.resetRemoteInputs();
            commandParser.parse("PANEL CANCEL");
            hostSilenceHandled = true;
        }
    } else {
        hostSilenceHandled = false;
    }
    if (i2sWriteFailures.load()) commandParser.setDeviceStatus("I2S WRITE FAILED");
    static uint32_t lastStatusMs = 0;
    static uint32_t lastStatusAttemptMs = 0;
    static unsigned statusRevision = 0;
    if (hostLinked && millis() - lastStatusAttemptMs >= 200 &&
        (statusRevision != commandParser.panelRevision() || millis() - lastStatusMs >= 1000)) {
        lastStatusAttemptMs = millis();
        if (sendStatus()) {
            lastStatusMs = millis();
            statusRevision = commandParser.panelRevision();
        }
    }
    savePanelSettingsIfNeeded();
    panelDisplay.update(hostLinked);
    delay(1);
}
