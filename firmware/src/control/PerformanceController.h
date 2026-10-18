#pragma once

#include <Arduino.h>

#include "ControlState.h"
#include "PanelSettings.h"

class PerformanceController {
public:
    static constexpr unsigned kDisplayWidth = 320;
    static constexpr unsigned kDisplayHeight = 172;

    enum class DisplayMode : uint8_t { Home, Menu };
    enum class MenuPage : uint8_t { Root, Setup, Sound, Mod, Echo, Pots, Pot, Buttons, Button, Encoder, Reset };

    explicit PerformanceController(ControlStore &controls);

    void setProfile(PerformanceProfile profile);
    void notifyControlsChanged() { changed(); }
    void notifyExternalChange(PotTarget target);
    bool setPad(unsigned padNumber, bool pressed);
    bool setPot(unsigned potNumber, float position);
    void setEncoderPressed(bool pressed);
    void rotateEncoder(int direction);
    void resetMomentaries();

    bool selectPad(unsigned padNumber);
    void stepAssignment(int direction);
    void stepVariant(int direction);
    void toggleSelectedHold();
    bool configurePad(unsigned padNumber, const String &sound,
                      unsigned variant, bool hold);

    bool configurePotTarget(PerformanceProfile profile, unsigned potNumber,
                            PotTarget target);
    bool configurePotMode(PerformanceProfile profile, unsigned potNumber,
                          PotMode mode);
    bool configurePotRange(PerformanceProfile profile, unsigned potNumber,
                           float minimum, float maximum);
    bool configurePotZones(PerformanceProfile profile, unsigned potNumber,
                           float low, float middle, float high);
    bool configureButtonAction(PerformanceProfile profile, unsigned buttonNumber,
                               ButtonAction action);
    bool configureButtonSound(PerformanceProfile profile, unsigned buttonNumber,
                              PanelSound sound, unsigned variant, bool latch);
    bool configurePotBehavior(PotBehavior behavior);
    bool configureEncoder(EncoderHoldAction action, unsigned step,
                          unsigned range);
    bool beginSettingsTransaction();
    bool commitSettingsTransaction();
    void cancelSettingsTransaction();
    bool applySettings(const PanelSettings &settings);
    void resetDefaults();

    const PanelSettings &settings() const { return settings_; }
    unsigned settingsRevision() const { return settingsRevision_; }
    void setPersistenceStatus(const char *status);
    const char *persistenceStatus() const { return persistenceStatus_; }

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
    const char *buttonActionNameFor(PerformanceProfile profile,
                                    unsigned buttonNumber) const;
    DisplayMode displayMode() const { return displayMode_; }
    const char *displayModeName() const;
    const char *menuTitle() const;
    const char *menuFooter() const;
    unsigned menuIndex() const { return menuIndex_; }
    bool menuEditing() const { return menuEditing_; }
    unsigned menuItemCount() const;
    const char *menuItemKey(unsigned index) const;
    const char *menuItemLabel(unsigned index) const;
    const char *menuItemValue(unsigned index) const;
    const char *overlayTitle() const { return overlayTitle_; }
    const char *overlayValue() const { return overlayValue_; }
    bool pickupWaiting(unsigned potNumber) const;
    int pickupDirection(unsigned potNumber) const;
    float classicPitchHz(unsigned index) const;
    float classicRateHz(unsigned index) const;
    void formatCurrentValue(PotTarget target, char *buffer,
                            unsigned size) const;

private:
    enum class Row : uint8_t {
        Profile, Pots, Buttons, Encoder, ResetDefaults, Back, Behavior,
        Pot1, Pot2, Pot3, Pot4, Pot5, Pot6, Function, Mode,
        Low, Middle, High, Minimum, Maximum,
        Button1, Button2, Button3, Button4, Action, Sound, Variant, Latch,
        HoldTurn, Step, Range, Confirm,
        SoundPage, ModPage, EchoPage, SetupPage, SelectedPad, Voice,
        Tune, Rate, Depth, Decay, Sweep, SweepTime, DrumSweep,
        Length, Punch, PitchTime, Tone, Contour, ContourTime,
        Shape, Modulation, Retrigger, ClassicRate,
        EchoLevel, EchoTime, Feedback, Filter, Color, Loop, Spin, Cut,

    };
    struct MenuRowView {
        Row row;
        const char *key;
        const char *label;
        char value[32];
    };
    struct PickupState {
        bool seen = false;
        bool latched = false;
        float previous = 0.0f;
        int direction = 0;
    };

    static bool parseSound(const String &text, PanelSound &sound);
    void applySound(ControlState &state, unsigned buttonIndex, bool activate);
    static void captureActivation(ControlState &state);
    void updateGate(ControlState &state);
    int latestPressedSoundButton() const;
    bool isRelevantButton(unsigned index) const;
    bool anyRelevantButtonDown() const;
    bool pitchAvailable(const ControlState &state) const;
    void openMenu();
    void pressMenuItem();
    void editMenuItem(Row item, int direction);
    static float *soundValue(ControlState &state, Row row, float &minimum, float &maximum, float &step);
    void addSoundValue(const ControlState &state, Row row, const char *key, const char *label, const char *unit) const;
    void normalizeMenuIndex();
    void setMenuPage(MenuPage page);
    void rebuildMenuView() const;
    void addMenuRow(Row row, const char *key, const char *label,
                    const char *value = "") const;
    void commitSettingsChange();
    void updateZoneFlags(ControlState &state) const;
    void acceptSettingsCandidate(const PanelSettings &candidate,
                                 bool resetPotPickup = false);
    bool rejectSettingsChange();
    void resetPickup();
    float currentTargetNormalized(const PotConfig &config,
                                  const ControlState &state) const;
    void applyPotValue(ControlState &state, const PotConfig &config,
                       float position);
    float mapPosition(const PotConfig &config, float position) const;
    void formatTargetValue(PotTarget target, const ControlState &state,
                           char *buffer, unsigned size) const;
    static void applyEchoFilter(ControlState &state, float value);
    void changed();

    ControlStore &controls_;
    PanelSettings settings_ = factoryPanelSettings();
    PanelSettings stagedSettings_ = factoryPanelSettings();
    bool settingsTransaction_ = false;
    bool settingsTransactionInvalid_ = false;
    bool padDown_[4] = {};
    bool consumed_[4] = {};
    unsigned pressOrder_[4] = {};
    unsigned nextOrder_ = 1;
    unsigned selectedPad_ = 0;
    unsigned selectedPot_ = 0;
    unsigned selectedButton_ = 0;
    unsigned currentPad_ = 0;
    PanelSound appliedSound_ = PanelSound::Count;
    unsigned appliedVariant_ = 255;
    int activePad_ = -1;
    bool encoderPressed_ = false;
    bool modifierUsed_ = false;
    bool holdToggle_ = false;
    PickupState pickup_[6];
    DisplayMode displayMode_ = DisplayMode::Home;
    MenuPage menuPage_ = MenuPage::Root;
    uint8_t menuIndex_ = 0;
    bool menuEditing_ = false;
    bool resetConfirmed_ = false;
    mutable MenuRowView menuRows_[24] = {};
    mutable unsigned menuRowCount_ = 0;
    mutable char menuTitleBuffer_[32] = "SETTINGS";
    mutable char menuFooterBuffer_[48] = "TURN SELECT / PUSH OPEN";
    char overlayTitle_[24] = "";
    char overlayValue_[40] = "";
    char persistenceStatus_[40] = "";
    unsigned revision_ = 1;
    unsigned menuRevision_ = 0;
    unsigned settingsRevision_ = 1;
};
