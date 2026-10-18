#pragma once

#include <Arduino.h>

#include "ControlState.h"
#include "PerformanceController.h"

class CommandParser {
public:
    explicit CommandParser(ControlStore &controls)
        : controls_(controls), performance_(controls) {}
    bool parse(const String &command);
    bool setPhysicalPad(unsigned pad, bool down) {
        if (pad < 1 || pad > 4) return false;
        physicalPads_[pad - 1] = down;
        return performance_.setPad(pad, down || remotePads_[pad - 1]);
    }
    bool setRemotePad(unsigned pad, bool down) {
        if (pad < 1 || pad > 4) return false;
        remotePads_[pad - 1] = down;
        return performance_.setPad(pad, down || physicalPads_[pad - 1]);
    }
    bool setPhysicalPot(unsigned pot, float position) { return performance_.setPot(pot, position); }
    void setPhysicalEncoderPressed(bool down) { physicalEncoder_ = down; performance_.setEncoderPressed(down || remoteEncoder_); }
    void setRemoteEncoderPressed(bool down) { remoteEncoder_ = down; performance_.setEncoderPressed(down || physicalEncoder_); }
    void resetRemoteInputs() {
        for (unsigned pad = 1; pad <= 4; ++pad) setRemotePad(pad, false);
        setRemoteEncoderPressed(false);
        bool physicalActive = physicalEncoder_;
        for (bool down : physicalPads_) physicalActive |= down;
        if (!physicalActive) performance_.resetMomentaries();
    }
    bool physicalTrigger() const {
        const auto profile = profileIndex(controls_.snapshot().profile);
        for (unsigned i = 0; i < 4; ++i) {
            const auto action = performance_.settings().profiles[profile].buttons[i].action;
            if (physicalPads_[i] && (action == ButtonAction::Trigger || action == ButtonAction::PlaySound)) return true;
        }
        return false;
    }
    void rotatePhysicalEncoder(int direction) { performance_.rotateEncoder(direction); }
    void notifyPhysicalMaster() { performance_.notifyControlsChanged(); }

    unsigned panelRevision() const { return performance_.revision(); }
    unsigned panelMenuRevision() const { return performance_.menuRevision(); }
    unsigned displayWidth() const { return PerformanceController::kDisplayWidth; }
    unsigned displayHeight() const { return PerformanceController::kDisplayHeight; }
    const char *displayModeName() const { return performance_.displayModeName(); }
    unsigned menuIndex() const { return performance_.menuIndex(); }
    bool menuEditing() const { return performance_.menuEditing(); }
    unsigned menuItemCount() const { return performance_.menuItemCount(); }
    const char *menuItemKey(unsigned index) const {
        return performance_.menuItemKey(index);
    }
    const char *menuItemLabel(unsigned index) const {
        return performance_.menuItemLabel(index);
    }
    const char *menuItemValue(unsigned index) const {
        return performance_.menuItemValue(index);
    }
    const char *menuTitle() const { return performance_.menuTitle(); }
    const char *menuFooter() const { return deviceStatus_[0] ? deviceStatus_ : performance_.menuFooter(); }
    const char *overlayTitle() const { return performance_.overlayTitle(); }
    const char *overlayValue() const { return performance_.overlayValue(); }
    unsigned selectedPad() const { return performance_.selectedPad(); }
    unsigned currentPad() const { return performance_.currentPad(); }
    int activePad() const { return performance_.activePad(); }
    const char *padSoundName(unsigned pad) const {
        return performance_.padSoundName(pad);
    }
    const char *padVariantName(unsigned pad) const {
        return performance_.padVariantName(pad);
    }
    unsigned padVariant(unsigned pad) const {
        return performance_.padVariant(pad);
    }
    bool padHold(unsigned pad) const { return performance_.padHold(pad); }
    bool padHoldSupported(unsigned pad) const {
        return performance_.padHoldSupported(pad);
    }
    PerformanceController::DisplayMode displayMode() const {
        return performance_.displayMode();
    }
    const PanelSettings &panelSettings() const { return performance_.settings(); }
    unsigned panelSettingsRevision() const { return performance_.settingsRevision(); }
    bool applyPanelSettings(const PanelSettings &settings) {
        return performance_.applySettings(settings);
    }
    void setPersistenceStatus(const char *status) {
        performance_.setPersistenceStatus(status);
    }
    void setDeviceStatus(const char *status) {
        if (strcmp(deviceStatus_, status) == 0) return;
        snprintf(deviceStatus_, sizeof(deviceStatus_), "%s", status);
        performance_.notifyControlsChanged();
    }
    const char *persistenceStatus() const {
        return deviceStatus_[0] ? deviceStatus_ : performance_.persistenceStatus();
    }
    const char *buttonActionNameFor(PerformanceProfile profile,
                                    unsigned button) const {
        return performance_.buttonActionNameFor(profile, button);
    }
    bool pickupWaiting(unsigned pot) const {
        return performance_.pickupWaiting(pot);
    }
    int pickupDirection(unsigned pot) const {
        return performance_.pickupDirection(pot);
    }
    void formatCurrentValue(PotTarget target, char *buffer,
                            unsigned size) const {
        performance_.formatCurrentValue(target, buffer, size);
    }

private:
    static bool parseFloat(const String &text, float &value);
    static bool parseBool(const String &text, bool &value);
    static bool parseMode(const String &text, SirenMode &mode);
    static bool parseLfoShape(const String &text, LfoShape &shape);
    static bool parseSyndrumWave(const String &text, SyndrumWave &wave);
    static bool parseProfileName(const char *text, PerformanceProfile &profile);
    static bool parsePotTargetName(const char *text, PotTarget &target);
    static bool parseButtonActionName(const char *text, ButtonAction &action);
    static bool parsePanelSoundName(const char *text, PanelSound &sound);
    bool parsePanelCommand(const String &command);

    bool physicalPads_[4] = {}, remotePads_[4] = {};
    bool physicalEncoder_ = false, remoteEncoder_ = false;
    char deviceStatus_[40] = {};
    ControlStore &controls_;
    PerformanceController performance_;
};
