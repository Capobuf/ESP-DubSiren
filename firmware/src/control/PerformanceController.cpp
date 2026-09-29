#include "PerformanceController.h"

namespace {

int wrappedStep(int value, int direction, int count) {
    int result = (value + (direction < 0 ? -1 : 1)) % count;
    return result < 0 ? result + count : result;
}

float clampFloat(float value, float minimum, float maximum) {
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

}  // namespace

PerformanceController::PerformanceController(ControlStore &controls)
    : controls_(controls),
      pads_{{Sound::Classic, 0, false},
            {Sound::TwoTone, 0, false},
            {Sound::Lickshot, 0, false},
            {Sound::Ping, 0, false}} {
    ControlState state = controls_.snapshot();
    if (state.profile == PerformanceProfile::Extended) applyConfig(state, selectedPad_, false);
    controls_.set(state);
}

void PerformanceController::setProfile(PerformanceProfile profile) {
    ControlState state = controls_.snapshot();
    if (state.profile == profile) return;
    resetMomentaries();
    state = controls_.snapshot();
    clearHiddenHolds();
    state.hold = false;
    state.trigger = false;
    state.modUp = state.modDown = state.echoCut = false;
    state.profile = profile;
    if (profile == PerformanceProfile::Classic) {
        if (state.mode >= SirenMode::Lickshot) state.mode = SirenMode::Sine1;
    }
    controls_.set(state);
    menuIndex_ = 0;
    menuEditing_ = false;
    changed();
}

unsigned PerformanceController::variantCount(Sound sound) {
    switch (sound) {
        case Sound::Classic: return 3;
        case Sound::TwoTone: return 3;
        case Sound::Lickshot: return 2;
        case Sound::Ping: return 3;
        case Sound::Count: break;
    }
    return 1;
}

const char *PerformanceController::soundName(Sound sound) {
    switch (sound) {
        case Sound::Classic: return "CLASSIC";
        case Sound::TwoTone: return "TWO TONE";
        case Sound::Lickshot: return "LICKSHOT";
        case Sound::Ping: return "PING";
        case Sound::Count: break;
    }
    return "UNKNOWN";
}

const char *PerformanceController::variantName(Sound sound, unsigned variant) {
    static const char *classic[] = {"SINE 1", "SINE 2", "SONAR"};
    static const char *twoTone[] = {"RC", "TRIANGLE", "SQUARE"};
    static const char *lickshot[] = {"SAW DOWN", "ASYM DOWN"};
    static const char *ping[] = {"SINE", "TRIANGLE", "SQUARE"};
    variant %= variantCount(sound);
    switch (sound) {
        case Sound::Classic: return classic[variant];
        case Sound::TwoTone: return twoTone[variant];
        case Sound::Lickshot: return lickshot[variant];
        case Sound::Ping: return ping[variant];
        case Sound::Count: break;
    }
    return "UNKNOWN";
}

bool PerformanceController::supportsHold(Sound sound) {
    return sound != Sound::Ping;
}

bool PerformanceController::parseSound(const String &text, Sound &sound) {
    if (text == "CLASSIC") sound = Sound::Classic;
    else if (text == "TWO_TONE") sound = Sound::TwoTone;
    else if (text == "LICKSHOT") sound = Sound::Lickshot;
    else if (text == "PING") sound = Sound::Ping;
    else return false;
    return true;
}

void PerformanceController::applyConfig(ControlState &state, unsigned padIndex,
                                        bool activate) {
    currentPad_ = padIndex;
    const PadConfig &config = pads_[padIndex];
    const unsigned variant = config.variant % variantCount(config.sound);
    state.profile = PerformanceProfile::Extended;
    state.modUp = false;
    state.modDown = false;
    switch (config.sound) {
        case Sound::Classic:
            state.mode = variant == 0 ? SirenMode::Sine1
                       : variant == 1 ? SirenMode::Sine2
                                      : SirenMode::Sonar;
            state.lfoShape = LfoShape::Classic;
            state.modulationEnabled = true;
            state.retrigger = false;
            break;
        case Sound::TwoTone:
            state.mode = SirenMode::Square;
            state.lfoShape = variant == 0 ? LfoShape::Classic
                           : variant == 1 ? LfoShape::Triangle
                                          : LfoShape::Square;
            state.modulationEnabled = true;
            state.retrigger = true;
            break;
        case Sound::Lickshot:
            state.mode = SirenMode::Lickshot;
            state.lfoShape = variant == 0 ? LfoShape::SawDown
                                          : LfoShape::AsymDown;
            state.modulationEnabled = true;
            state.retrigger = true;
            break;
        case Sound::Ping:
            state.mode = SirenMode::Syndrum;
            state.syndrumWave = variant == 0 ? SyndrumWave::Sine
                               : variant == 1 ? SyndrumWave::Triangle
                                              : SyndrumWave::Square;
            state.syndrumSweepOctaves = 2.0f;
            state.decayMs = PerformanceConfig::kExtendedDecayMs;
            state.modulationEnabled = false;
            state.retrigger = false;
            break;
        case Sound::Count:
            break;
    }
    if (activate) captureActivation(state);
}

void PerformanceController::captureActivation(ControlState &state) {
    recordActivation(state);
}

void PerformanceController::updateGate(ControlState &state) {
    if (state.profile == PerformanceProfile::Classic) {
        state.trigger = padDown_[0];
        state.modUp = padDown_[2];
        state.modDown = false;
        state.echoCut = padDown_[3];
        return;
    }
    state.trigger = activePad_ >= 0;
    const unsigned holdPad = activePad_ >= 0
        ? static_cast<unsigned>(activePad_) : currentPad_;
    state.hold = pads_[holdPad].hold && supportsHold(pads_[holdPad].sound);
    state.echoCut = encoderPressed_ && padDown_[3] && consumed_[3];
}

void PerformanceController::clearHiddenHolds() {
    for (PadConfig &pad : pads_) pad.hold = false;
}

int PerformanceController::latestPressedPad() const {
    int latest = -1;
    unsigned order = 0;
    for (unsigned index = 0; index < 4; ++index) {
        if (padDown_[index] && !consumed_[index] && pressOrder_[index] >= order) {
            order = pressOrder_[index];
            latest = static_cast<int>(index);
        }
    }
    return latest;
}

void PerformanceController::changed() {
    ++revision_;
}

bool PerformanceController::setPad(unsigned padNumber, bool pressed) {
    if (padNumber < 1 || padNumber > 4) return false;
    const unsigned index = padNumber - 1;
    if (padDown_[index] == pressed) return true;
    padDown_[index] = pressed;
    ControlState state = controls_.snapshot();
    if (state.profile == PerformanceProfile::Classic) {
        if (index == 1 && pressed) state.hold = !state.hold;
        activePad_ = padDown_[0] ? 0 : -1;
        updateGate(state);
        controls_.set(state);
        changed();
        return true;
    }

    if (pressed) {
        if (encoderPressed_) {
            consumed_[index] = true;
            modifierUsed_ = true;
            if (index == 0) toggleSelectedHold();
            else if (index == 1) stepVariant(-1);
            else if (index == 2) stepVariant(1);
            state = controls_.snapshot();
        } else {
            consumed_[index] = false;
            pressOrder_[index] = nextOrder_++;
            selectedPad_ = index;
            activePad_ = static_cast<int>(index);
            if (!supportsHold(pads_[index].sound)) clearHiddenHolds();
            applyConfig(state, index, true);
        }
    } else {
        pressOrder_[index] = 0;
        if (!consumed_[index] && activePad_ == static_cast<int>(index)) {
            activePad_ = latestPressedPad();
            if (activePad_ >= 0) {
                selectedPad_ = static_cast<unsigned>(activePad_);
                applyConfig(state, selectedPad_, false);
            }
        }
        consumed_[index] = false;
    }
    updateGate(state);
    controls_.set(state);
    changed();
    return true;
}

void PerformanceController::setEncoderPressed(bool pressed) {
    if (controls_.snapshot().profile == PerformanceProfile::Classic) {
        if (encoderPressed_ == pressed) return;
        encoderPressed_ = pressed;
        if (pressed) {
            if (displayMode_ == DisplayMode::Home) openMenu();
            else pressMenuItem();
            changed();
        }
        return;
    }
    if (displayMode_ == DisplayMode::Menu) {
        if (pressed) pressMenuItem();
        return;
    }
    if (encoderPressed_ == pressed) return;
    encoderPressed_ = pressed;
    if (pressed) {
        modifierUsed_ = false;
    } else if (!modifierUsed_) {
        openMenu();
    }
    ControlState state = controls_.snapshot();
    updateGate(state);
    controls_.set(state);
    changed();
}

void PerformanceController::rotateEncoder(int direction) {
    if (direction == 0) return;
    if (displayMode_ == DisplayMode::Home) {
        ControlState state = controls_.snapshot();
        if (state.profile == PerformanceProfile::Classic) {
            state.mode = static_cast<SirenMode>(wrappedStep(static_cast<int>(state.mode), direction, 4));
            controls_.set(state);
            changed();
            return;
        }
        state.lfoDepthOctaves = clampFloat(
            state.lfoDepthOctaves + (direction < 0 ? -0.05f : 0.05f),
            0.0f, 2.0f);
        controls_.set(state);
        changed();
        return;
    }

    normalizeMenuIndex();
    if (menuEditing_) {
        editMenuItem(menuItem(menuIndex_), direction);
    } else {
        menuIndex_ = static_cast<uint8_t>(wrappedStep(
            menuIndex_, direction, static_cast<int>(menuItemCount())));
        changed();
    }
}

void PerformanceController::resetMomentaries() {
    for (unsigned index = 0; index < 4; ++index) {
        padDown_[index] = false;
        consumed_[index] = false;
        pressOrder_[index] = 0;
    }
    activePad_ = -1;
    encoderPressed_ = false;
    modifierUsed_ = false;
    ControlState state = controls_.snapshot();
    updateGate(state);
    controls_.set(state);
    changed();
}

bool PerformanceController::selectPad(unsigned padNumber) {
    if (padNumber < 1 || padNumber > 4) return false;
    selectedPad_ = padNumber - 1;
    normalizeMenuIndex();
    changed();
    return true;
}

void PerformanceController::stepAssignment(int direction) {
    PadConfig &pad = pads_[selectedPad_];
    pad.sound = static_cast<Sound>(wrappedStep(
        static_cast<int>(pad.sound), direction, static_cast<int>(Sound::Count)));
    pad.variant = 0;
    if (!supportsHold(pad.sound)) {
        pad.hold = false;
    }
    ControlState state = controls_.snapshot();
    if (state.profile == PerformanceProfile::Extended && currentPad_ == selectedPad_) {
        applyConfig(state, selectedPad_, false);
    }
    updateGate(state);
    controls_.set(state);
    normalizeMenuIndex();
    changed();
}

void PerformanceController::stepVariant(int direction) {
    PadConfig &pad = pads_[selectedPad_];
    pad.variant = static_cast<uint8_t>(wrappedStep(
        pad.variant, direction, static_cast<int>(variantCount(pad.sound))));
    ControlState state = controls_.snapshot();
    if (state.profile == PerformanceProfile::Extended && currentPad_ == selectedPad_) {
        applyConfig(state, selectedPad_, false);
        updateGate(state);
        controls_.set(state);
    }
    changed();
}

void PerformanceController::toggleSelectedHold() {
    ControlState baseline = controls_.snapshot();
    if (baseline.profile == PerformanceProfile::Classic) {
        baseline.hold = !baseline.hold;
        controls_.set(baseline);
        changed();
        return;
    }
    PadConfig &pad = pads_[selectedPad_];
    if (!supportsHold(pad.sound)) return;
    pad.hold = !pad.hold;
    ControlState state = controls_.snapshot();
    if (currentPad_ == selectedPad_ || (activePad_ < 0 && pad.hold)) {
        applyConfig(state, selectedPad_, false);
        updateGate(state);
        controls_.set(state);
    }
    changed();
}

bool PerformanceController::configurePad(unsigned padNumber, const String &sound,
                                         unsigned variant, bool hold) {
    if (padNumber < 1 || padNumber > 4) return false;
    Sound parsed;
    if (!parseSound(sound, parsed) || variant >= variantCount(parsed) ||
        (hold && !supportsHold(parsed))) return false;
    PadConfig &pad = pads_[padNumber - 1];
    pad = {parsed, static_cast<uint8_t>(variant), hold};
    ControlState state = controls_.snapshot();
    if (state.profile == PerformanceProfile::Extended && currentPad_ == padNumber - 1) {
        applyConfig(state, padNumber - 1, false);
        updateGate(state);
        controls_.set(state);
    }
    changed();
    return true;
}

const char *PerformanceController::padSoundName(unsigned padNumber) const {
    if (padNumber < 1 || padNumber > 4) return "UNKNOWN";
    return soundName(pads_[padNumber - 1].sound);
}

const char *PerformanceController::padVariantName(unsigned padNumber) const {
    if (padNumber < 1 || padNumber > 4) return "UNKNOWN";
    const PadConfig &pad = pads_[padNumber - 1];
    return variantName(pad.sound, pad.variant);
}

unsigned PerformanceController::padVariant(unsigned padNumber) const {
    return padNumber >= 1 && padNumber <= 4 ? pads_[padNumber - 1].variant : 0;
}

bool PerformanceController::padHold(unsigned padNumber) const {
    return padNumber >= 1 && padNumber <= 4 && pads_[padNumber - 1].hold;
}

bool PerformanceController::padHoldSupported(unsigned padNumber) const {
    return padNumber >= 1 && padNumber <= 4 &&
           supportsHold(pads_[padNumber - 1].sound);
}

const char *PerformanceController::displayModeName() const {
    return displayMode_ == DisplayMode::Home ? "HOME" : "MENU";
}

unsigned PerformanceController::menuItemCount() const {
    if (controls_.snapshot().profile == PerformanceProfile::Classic) return 2;
    return padHoldSupported(selectedPad()) ? 7 : 6;
}

PerformanceController::MenuItem PerformanceController::menuItem(
        unsigned index) const {
    if (controls_.snapshot().profile == PerformanceProfile::Classic)
        return index % 2 == 0 ? MenuItem::Profile : MenuItem::Back;
    static constexpr MenuItem withHold[] = {
        MenuItem::Pad, MenuItem::Assignment, MenuItem::Variant,
        MenuItem::Hold, MenuItem::EchoLoop, MenuItem::Profile, MenuItem::Back,
    };
    static constexpr MenuItem withoutHold[] = {
        MenuItem::Pad, MenuItem::Assignment, MenuItem::Variant,
        MenuItem::EchoLoop, MenuItem::Profile, MenuItem::Back,
    };
    const unsigned count = menuItemCount();
    index = count == 0 ? 0 : index % count;
    return padHoldSupported(selectedPad()) ? withHold[index]
                                           : withoutHold[index];
}

const char *PerformanceController::menuItemKey(unsigned index) const {
    switch (menuItem(index)) {
        case MenuItem::Profile: return "PROFILE";
        case MenuItem::Pad: return "PAD";
        case MenuItem::Assignment: return "ASSIGNMENT";
        case MenuItem::Variant: return "VARIANT";
        case MenuItem::Hold: return "HOLD";
        case MenuItem::EchoLoop: return "ECHO_LOOP";
        case MenuItem::Back: return "BACK";
    }
    return "BACK";
}

void PerformanceController::openMenu() {
    displayMode_ = DisplayMode::Menu;
    menuIndex_ = 0;
    menuEditing_ = false;
    ++menuRevision_;
}

void PerformanceController::pressMenuItem() {
    normalizeMenuIndex();
    const MenuItem item = menuItem(menuIndex_);
    if (item == MenuItem::Profile) {
        setProfile(controls_.snapshot().profile == PerformanceProfile::Classic
            ? PerformanceProfile::Extended : PerformanceProfile::Classic);
        ControlState state = controls_.snapshot();
        if (state.profile == PerformanceProfile::Extended) {
            applyConfig(state, selectedPad_, false);
            controls_.set(state);
        }
        return;
    }
    if (item == MenuItem::Back) {
        displayMode_ = DisplayMode::Home;
        menuEditing_ = false;
    } else if (item == MenuItem::Hold || item == MenuItem::EchoLoop) {
        editMenuItem(item, 1);
        return;
    } else {
        menuEditing_ = !menuEditing_;
    }
    changed();
}

void PerformanceController::editMenuItem(MenuItem item, int direction) {
    switch (item) {
        case MenuItem::Profile: break;
        case MenuItem::Pad:
            selectPad(static_cast<unsigned>(wrappedStep(
                static_cast<int>(selectedPad_), direction, 4)) + 1);
            break;
        case MenuItem::Assignment:
            stepAssignment(direction);
            break;
        case MenuItem::Variant:
            stepVariant(direction);
            break;
        case MenuItem::Hold:
            toggleSelectedHold();
            break;
        case MenuItem::EchoLoop: {
            ControlState state = controls_.snapshot();
            state.echoLoop = !state.echoLoop;
            controls_.set(state);
            changed();
            break;
        }
        case MenuItem::Back:
            break;
    }
}

void PerformanceController::normalizeMenuIndex() {
    const unsigned count = menuItemCount();
    if (menuIndex_ >= count) menuIndex_ = static_cast<uint8_t>(count - 1);
}
