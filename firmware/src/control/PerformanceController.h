#pragma once

#include <Arduino.h>

#include "ControlState.h"

class PerformanceController {
public:
    static constexpr unsigned kDisplayWidth = 320;
    static constexpr unsigned kDisplayHeight = 170;

    enum class DisplayMode : uint8_t { Home, Menu };
    enum class MenuItem : uint8_t {
        Pad,
        Assignment,
        Variant,
        Hold,
        EchoLoop,
        Back,
    };

    explicit PerformanceController(ControlStore &controls);

    bool setPad(unsigned padNumber, bool pressed);
    void setEncoderPressed(bool pressed);
    void rotateEncoder(int direction);
    void resetMomentaries();
    bool selectPad(unsigned padNumber);
    void stepAssignment(int direction);
    void stepVariant(int direction);
    void toggleSelectedHold();
    bool configurePad(unsigned padNumber, const String &sound,
                      unsigned variant, bool hold);

    unsigned revision() const { return revision_; }
    unsigned menuRevision() const { return menuRevision_; }
    unsigned selectedPad() const { return selectedPad_ + 1; }
    unsigned currentPad() const { return currentPad_ + 1; }
    int activePad() const { return activePad_ < 0 ? 0 : activePad_ + 1; }
    const char *padSoundName(unsigned padNumber) const;
    const char *padVariantName(unsigned padNumber) const;
    unsigned padVariant(unsigned padNumber) const;
    bool padHold(unsigned padNumber) const;
    bool padHoldSupported(unsigned padNumber) const;
    DisplayMode displayMode() const { return displayMode_; }
    const char *displayModeName() const;
    unsigned menuIndex() const { return menuIndex_; }
    bool menuEditing() const { return menuEditing_; }
    unsigned menuItemCount() const;
    MenuItem menuItem(unsigned index) const;
    const char *menuItemKey(unsigned index) const;

private:
    enum class Sound : uint8_t { Classic, TwoTone, Lickshot, Ping, Count };
    struct PadConfig {
        Sound sound;
        uint8_t variant;
        bool hold;
    };

    static unsigned variantCount(Sound sound);
    static const char *soundName(Sound sound);
    static const char *variantName(Sound sound, unsigned variant);
    static bool supportsHold(Sound sound);
    static bool parseSound(const String &text, Sound &sound);
    void applyConfig(ControlState &state, unsigned padIndex, bool activate);
    static void captureActivation(ControlState &state);
    void updateGate(ControlState &state);
    void clearHiddenHolds();
    int latestPressedPad() const;
    void openMenu();
    void pressMenuItem();
    void editMenuItem(MenuItem item, int direction);
    void normalizeMenuIndex();
    void changed();

    ControlStore &controls_;
    PadConfig pads_[4];
    bool padDown_[4] = {};
    bool consumed_[4] = {};
    unsigned pressOrder_[4] = {};
    unsigned nextOrder_ = 1;
    unsigned selectedPad_ = 0;
    unsigned currentPad_ = 0;
    int activePad_ = -1;
    bool encoderPressed_ = false;
    bool modifierUsed_ = false;
    DisplayMode displayMode_ = DisplayMode::Home;
    uint8_t menuIndex_ = 0;
    bool menuEditing_ = false;
    unsigned revision_ = 1;
    unsigned menuRevision_ = 0;
};
