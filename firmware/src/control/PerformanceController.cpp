#include "PerformanceController.h"

#include <stdio.h>

namespace {

int wrappedStep(int value, int direction, int count) {
    int result = (value + (direction < 0 ? -1 : 1)) % count;
    return result < 0 ? result + count : result;
}

float clampFloat(float value, float minimum, float maximum) {
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

bool crossed(float before, float after, float target) {
    return (before <= target && after >= target) ||
           (before >= target && after <= target);
}

float editingStep(const TargetDescriptor &descriptor, float value, int direction) {
    if (descriptor.mapping == MappingType::Logarithmic && value > 0.0f)
        return value * (direction < 0 ? 0.95f : 1.05f);
    const float step = (descriptor.absoluteMax - descriptor.absoluteMin) / 100.0f;
    return value + (direction < 0 ? -step : step);
}

void formatConfiguredValue(PotTarget target, float value,
                           char *buffer, unsigned size) {
    switch (target) {
        case PotTarget::Pitch:
            if (value >= 1000.0f) snprintf(buffer, size, "%.2f kHz", value / 1000.0f);
            else snprintf(buffer, size, "%.0f Hz", value);
            break;
        case PotTarget::Rate: snprintf(buffer, size, "%.2f Hz", value); break;
        case PotTarget::LfoDepth: snprintf(buffer, size, "%.2f oct", value); break;
        case PotTarget::EchoLevel:
        case PotTarget::EchoFeedback:
        case PotTarget::SweepAmount: snprintf(buffer, size, "%.0f%%", value * 100.0f); break;
        case PotTarget::EchoTime:
        case PotTarget::Decay:
        case PotTarget::SweepTime: snprintf(buffer, size, "%.0f ms", value); break;
        case PotTarget::EchoFilter: snprintf(buffer, size, "%+.0f%%", value * 100.0f); break;
        default: snprintf(buffer, size, "%.2f", value); break;
    }
}

}  // namespace

PerformanceController::PerformanceController(ControlStore &controls)
    : controls_(controls) {
    ControlState state = controls_.snapshot();
    state.tuneHz = settings_.profiles[0].pots[0].zones[1];
    state.lfoRateHz = settings_.profiles[0].pots[1].zones[1];
    state.classicPitchUsesTune = true;
    state.classicRateUsesRate = true;
    controls_.set(state);
}

void PerformanceController::changed() { ++revision_; }

void PerformanceController::setPersistenceStatus(const char *status) {
    snprintf(persistenceStatus_, sizeof(persistenceStatus_), "%s",
             status == nullptr ? "" : status);
    changed();
}

void PerformanceController::resetPickup() {
    for (PickupState &state : pickup_) state = PickupState{};
}

void PerformanceController::setProfile(PerformanceProfile profile) {
    ControlState state = controls_.snapshot();
    if (state.profile == profile) return;
    holdToggle_ = false;
    resetMomentaries();
    state = controls_.snapshot();
    state.hold = false;
    state.trigger = false;
    state.modUp = state.modDown = state.echoCut = state.echoSpin = false;
    state.liveTransposeSemitones = 0;
    state.profile = profile;
    if (profile == PerformanceProfile::Classic) {
        if (state.mode >= SirenMode::Lickshot) state.mode = SirenMode::Sine1;
        state.tuneHz = classicPitchHz(static_cast<unsigned>(state.classicPitch));
        state.classicPitchUsesTune = true;
        const unsigned rate = static_cast<unsigned>(state.classicModulation);
        if (rate < 3) state.lfoRateHz = classicRateHz(rate);
        state.classicRateUsesRate = true;
    }
    updateZoneFlags(state);
    controls_.set(state);
    resetPickup();
    menuIndex_ = 0;
    menuEditing_ = false;
    changed();
}

bool PerformanceController::parseSound(const String &text, PanelSound &sound) {
    if (text == "CLASSIC") sound = PanelSound::Classic;
    else if (text == "TWO_TONE") sound = PanelSound::TwoTone;
    else if (text == "LICKSHOT") sound = PanelSound::Lickshot;
    else if (text == "PING") sound = PanelSound::Ping;
    else if (text == "GUNSHOT") sound = PanelSound::Gunshot;
    else if (text == "TEST_TONE") sound = PanelSound::TestTone;
    else return false;
    return true;
}

void PerformanceController::applySound(ControlState &state, unsigned index,
                                       bool activate) {
    const bool samePad = currentPad_ == index;
    currentPad_ = index;
    const ControlState previous = state;
    const PerformanceProfile originalProfile = state.profile;
    const ButtonConfig config = settings_.profiles[profileIndex(state.profile)].buttons[index];
    const unsigned variant = config.variant;
    if (originalProfile == PerformanceProfile::Extended)
        state.profile = PerformanceProfile::Extended;
    state.modUp = state.modDown = false;
    switch (config.sound) {
        case PanelSound::Classic:
            state.mode = variant == 0 ? SirenMode::Sine1
                       : variant == 1 ? SirenMode::Sine2
                       : variant == 2 ? SirenMode::Sonar
                       : variant == 3 ? SirenMode::Saw
                                      : SirenMode::Triangle;
            state.lfoShape = LfoShape::Classic;
            state.modulationEnabled = true;
            state.retrigger = false;
            break;
        case PanelSound::TwoTone:
            state.mode = SirenMode::Square;
            state.lfoShape = variant == 0 ? LfoShape::Classic
                           : variant == 1 ? LfoShape::Triangle : LfoShape::Square;
            state.modulationEnabled = true;
            state.retrigger = true;
            break;
        case PanelSound::Lickshot:
            state.mode = SirenMode::Lickshot;
            state.lickshotArticulation = variant < 2 ? LickshotArticulation::Legacy
                : variant == 2 ? LickshotArticulation::Shot
                : variant == 3 ? LickshotArticulation::Roll : LickshotArticulation::Horn;
            state.lfoShape = variant == 0 ? LfoShape::SawDown : LfoShape::AsymDown;
            state.modulationEnabled = true;
            state.retrigger = true;
            break;
        case PanelSound::Ping:
            state.mode = SirenMode::Syndrum;
            state.syndrumWave = variant == 0 ? SyndrumWave::Sine
                               : variant == 1 ? SyndrumWave::Triangle
                                              : SyndrumWave::Square;
            state.syndrumSweepOctaves = 2.0f;
            state.decayMs = PerformanceConfig::kExtendedDecayMs;
            state.modulationEnabled = false;
            state.retrigger = false;
            break;
        case PanelSound::Gunshot: state.mode = SirenMode::Gunshot; break;
        case PanelSound::TestTone: state.mode = SirenMode::TestTone; break;
        case PanelSound::Count: break;
    }
    if (samePad && appliedSound_ == config.sound && appliedVariant_ == variant) {
        state.lfoShape = previous.lfoShape;
        state.modulationEnabled = previous.modulationEnabled;
        state.retrigger = previous.retrigger;
        state.decayMs = previous.decayMs;
        state.syndrumSweepOctaves = previous.syndrumSweepOctaves;
    }
    appliedSound_ = config.sound;
    appliedVariant_ = variant;
    if (activate) captureActivation(state);
}

void PerformanceController::captureActivation(ControlState &state) {
    recordActivation(state);
}

bool PerformanceController::isRelevantButton(unsigned index) const {
    const ControlState state = controls_.snapshot();
    const ButtonAction action = settings_.profiles[profileIndex(state.profile)]
        .buttons[index].action;
    return action == ButtonAction::Trigger || action == ButtonAction::PlaySound;
}

bool PerformanceController::anyRelevantButtonDown() const {
    for (unsigned index = 0; index < 4; ++index)
        if (padDown_[index] && !consumed_[index] && isRelevantButton(index)) return true;
    return false;
}

bool PerformanceController::pitchAvailable(const ControlState &state) const {
    return state.mode != SirenMode::Gunshot;
}

void PerformanceController::updateGate(ControlState &state) {
    const ProfilePanelSettings &profile = settings_.profiles[profileIndex(state.profile)];
    bool trigger = false;
    bool modUp = false;
    bool modDown = false;
    bool echoCut = false;
    bool echoSpin = false;
    for (unsigned index = 0; index < 4; ++index) {
        if (!padDown_[index] || consumed_[index]) continue;
        switch (profile.buttons[index].action) {
            case ButtonAction::Trigger: trigger = true; break;
            case ButtonAction::PlaySound: trigger |= activePad_ >= 0; break;
            case ButtonAction::ModUp: modUp = true; break;
            case ButtonAction::ModDown: modDown = true; break;
            case ButtonAction::EchoCut: echoCut = true; break;
            case ButtonAction::EchoSpin: echoSpin = true; break;
            default: break;
        }
    }
    state.trigger = trigger;
    state.modUp = modUp;
    state.modDown = modDown;
    state.echoCut = echoCut;
    state.echoSpin = echoSpin;
    const unsigned soundIndex = activePad_ >= 0
        ? static_cast<unsigned>(activePad_) : currentPad_;
    const ButtonConfig &button = profile.buttons[soundIndex];
    const bool soundLatch = button.action == ButtonAction::PlaySound &&
        button.latch && panelSoundSupportsLatch(button.sound);
    state.hold = holdToggle_ || soundLatch;
    if (state.profile == PerformanceProfile::Extended && encoderPressed_ &&
        padDown_[3] && consumed_[3]) state.echoCut = true;
}

int PerformanceController::latestPressedSoundButton() const {
    int latest = -1;
    unsigned order = 0;
    const ControlState state = controls_.snapshot();
    const auto &buttons = settings_.profiles[profileIndex(state.profile)].buttons;
    for (unsigned index = 0; index < 4; ++index) {
        if (padDown_[index] && !consumed_[index] &&
            buttons[index].action == ButtonAction::PlaySound &&
            pressOrder_[index] >= order) {
            order = pressOrder_[index];
            latest = static_cast<int>(index);
        }
    }
    return latest;
}

bool PerformanceController::setPad(unsigned padNumber, bool pressed) {
    if (padNumber < 1 || padNumber > 4) return false;
    const unsigned index = padNumber - 1;
    if (padDown_[index] == pressed) return true;
    overlayTitle_[0] = '\0';
    overlayValue_[0] = '\0';
    padDown_[index] = pressed;
    ControlState state = controls_.snapshot();
    ButtonConfig &button = settings_.profiles[profileIndex(state.profile)].buttons[index];

    // Extended's established encoder+button shortcuts take precedence over the
    // configured button action and suppress the release-click menu gesture.
    if (state.profile == PerformanceProfile::Extended && pressed && encoderPressed_) {
        consumed_[index] = true;
        modifierUsed_ = true;
        if (index == 0) toggleSelectedHold();
        else if (index == 1) stepVariant(-1);
        else if (index == 2) stepVariant(1);
        else state.echoCut = true;
        state = controls_.snapshot();
    } else if (pressed) {
        consumed_[index] = false;
        pressOrder_[index] = nextOrder_++;
        if (button.action == ButtonAction::Hold) holdToggle_ = !holdToggle_;
        else if (button.action == ButtonAction::Trigger) captureActivation(state);
        else if (button.action == ButtonAction::EchoLoop) state.echoLoop = !state.echoLoop;
        else if (button.action == ButtonAction::PlaySound) {
            selectedPad_ = index;
            activePad_ = static_cast<int>(index);
            applySound(state, index, true);
        }
    } else {
        pressOrder_[index] = 0;
        if (!consumed_[index] && button.action == ButtonAction::PlaySound &&
            activePad_ == static_cast<int>(index)) {
            activePad_ = latestPressedSoundButton();
            if (activePad_ >= 0) {
                selectedPad_ = static_cast<unsigned>(activePad_);
                applySound(state, selectedPad_, false);
            }
        }
        consumed_[index] = false;
    }
    if (!anyRelevantButtonDown()) state.liveTransposeSemitones = 0;
    updateGate(state);
    controls_.set(state);
    changed();
    return true;
}

float PerformanceController::mapPosition(const PotConfig &config,
                                         float position) const {
    position = clampFloat(position, 0.0f, 1.0f);
    const TargetDescriptor &descriptor = *targetDescriptor(config.target);
    if (descriptor.mapping == MappingType::Logarithmic)
        return config.minimum * powf(config.maximum / config.minimum, position);
    if (descriptor.mapping == MappingType::Power) {
        const float exponent = config.target == PotTarget::EchoFeedback ? 0.8f : 2.0f;
        return config.minimum + (config.maximum - config.minimum) * powf(position, exponent);
    }
    return config.minimum + (config.maximum - config.minimum) * position;
}

void PerformanceController::applyEchoFilter(ControlState &state, float value) {
    state.echoFilterPosition = value;
    const float amount = PerformanceConfig::echoFilterAmount(value);
    state.echoFilterFlat = amount == 0.0f;
    state.highPassHz = value > 0.0f
        ? PerformanceConfig::kEchoHighPassMinHz * powf(
            PerformanceConfig::kEchoHighPassMaxHz /
            PerformanceConfig::kEchoHighPassMinHz, amount)
        : PerformanceConfig::kEchoHighPassMinHz;
    state.lowPassHz = value < 0.0f
        ? PerformanceConfig::kEchoLowPassMaxHz * powf(
            PerformanceConfig::kEchoLowPassMinHz /
            PerformanceConfig::kEchoLowPassMaxHz, amount)
        : PerformanceConfig::kEchoLowPassMaxHz;
}

void PerformanceController::applyPotValue(ControlState &state,
                                          const PotConfig &config,
                                          float position) {
    if (config.target == PotTarget::Pitch) state.pitchPosition = position;
    if (config.target == PotTarget::Rate) state.ratePosition = position;
    if (config.target == PotTarget::Pitch)
        state.pitchZonesActive = config.mode == PotMode::Zones;
    if (config.target == PotTarget::Rate)
        state.rateZonesActive = config.mode == PotMode::Zones;
    if (config.mode == PotMode::Zones && config.target == PotTarget::Pitch) {
        const int zone = PerformanceConfig::selectorZone(position,
            static_cast<int>(state.classicPitch), 3);
        state.classicPitch = static_cast<ClassicPitch>(zone);
        state.tuneHz = config.zones[zone];
        state.classicPitchUsesTune = true;
        return;
    }
    if (config.mode == PotMode::Zones && config.target == PotTarget::Rate) {
        const int zone = PerformanceConfig::selectorZone(position,
            static_cast<int>(state.classicModulation), 4);
        state.classicModulation = static_cast<ClassicModulation>(zone);
        if (zone < 3) state.lfoRateHz = config.zones[zone];
        state.classicRateUsesRate = true;
        return;
    }
    const float value = mapPosition(config, position);
    switch (config.target) {
        case PotTarget::None: break;
        case PotTarget::Pitch: state.tuneHz = value; break;
        case PotTarget::Rate: state.lfoRateHz = value; break;
        case PotTarget::LfoDepth: state.lfoDepthOctaves = value; break;
        case PotTarget::EchoLevel: state.echoLevel = value; break;
        case PotTarget::EchoTime: state.delayMs = value; break;
        case PotTarget::EchoFeedback: state.feedback = value; break;
        case PotTarget::EchoFilter: applyEchoFilter(state, value); break;
        case PotTarget::Decay: state.decayMs = value; break;
        case PotTarget::SweepAmount: state.sweepAmount = value; break;
        case PotTarget::SweepTime: state.sweepMs = value; break;
        case PotTarget::Count: break;
    }
}

float PerformanceController::currentTargetNormalized(
        const PotConfig &config, const ControlState &state) const {
    if (config.mode == PotMode::Zones) {
        const unsigned zone = config.target == PotTarget::Pitch
            ? static_cast<unsigned>(state.classicPitch)
            : static_cast<unsigned>(state.classicModulation);
        const unsigned count = config.target == PotTarget::Pitch ? 3 : 4;
        return (zone + 0.5f) / count;
    }
    float value = config.minimum;
    switch (config.target) {
        case PotTarget::Pitch: value = state.tuneHz; break;
        case PotTarget::Rate: value = state.lfoRateHz; break;
        case PotTarget::LfoDepth: value = state.lfoDepthOctaves; break;
        case PotTarget::EchoLevel: value = state.echoLevel; break;
        case PotTarget::EchoTime: value = state.delayMs; break;
        case PotTarget::EchoFeedback: value = state.feedback; break;
        case PotTarget::EchoFilter: value = state.echoFilterPosition; break;
        case PotTarget::Decay: value = state.decayMs; break;
        case PotTarget::SweepAmount: value = state.sweepAmount; break;
        case PotTarget::SweepTime: value = state.sweepMs; break;
        default: return 0.0f;
    }
    const TargetDescriptor &descriptor = *targetDescriptor(config.target);
    value = clampFloat(value, config.minimum, config.maximum);
    if (descriptor.mapping == MappingType::Logarithmic)
        return logf(value / config.minimum) / logf(config.maximum / config.minimum);
    if (descriptor.mapping == MappingType::Power) {
        const float exponent = config.target == PotTarget::EchoFeedback ? 0.8f : 2.0f;
        return powf((value - config.minimum) / (config.maximum - config.minimum),
                    1.0f / exponent);
    }
    return (value - config.minimum) / (config.maximum - config.minimum);
}

bool PerformanceController::setPot(unsigned potNumber, float position) {
    if (potNumber < 1 || potNumber > 6 || !isfinite(position) ||
        position < 0.0f || position > 1.0f) return false;
    const unsigned index = potNumber - 1;
    ControlState state = controls_.snapshot();
    const PotConfig &config = settings_.profiles[profileIndex(state.profile)].pots[index];
    PickupState &pickup = pickup_[index];
    bool apply = settings_.potBehavior == PotBehavior::Jump || pickup.latched ||
                 config.target == PotTarget::None;
    if (!apply) {
        const float target = currentTargetNormalized(config, state);
        if (!pickup.seen) {
            pickup.seen = true;
            pickup.previous = position;
            pickup.direction = position < target ? 1 : position > target ? -1 : 0;
            apply = fabsf(position - target) <= 0.015f;
        } else {
            apply = crossed(pickup.previous, position, target) ||
                    fabsf(position - target) <= 0.015f;
            pickup.previous = position;
            pickup.direction = position < target ? 1 : position > target ? -1 : 0;
        }
        if (apply) pickup.latched = true;
    }
    snprintf(overlayTitle_, sizeof(overlayTitle_), "%s", potTargetName(config.target));
    if (apply) {
        applyPotValue(state, config, position);
        formatTargetValue(config.target, state, overlayValue_, sizeof(overlayValue_));
        if (config.mode == PotMode::Continuous && config.target == PotTarget::Pitch)
            snprintf(overlayValue_, sizeof(overlayValue_), "%.0f Hz", state.tuneHz);
        if (config.mode == PotMode::Continuous && config.target == PotTarget::Rate)
            snprintf(overlayValue_, sizeof(overlayValue_), "%.2f Hz", state.lfoRateHz);
        const bool transient = state.mode == SirenMode::Syndrum ||
                               state.mode == SirenMode::Gunshot;
        const bool inactive = (config.target == PotTarget::Pitch &&
                               state.mode == SirenMode::Gunshot) ||
            (transient && (config.target == PotTarget::Rate ||
                           config.target == PotTarget::LfoDepth ||
                           config.target == PotTarget::SweepAmount ||
                           config.target == PotTarget::SweepTime));
        if (inactive) snprintf(overlayValue_, sizeof(overlayValue_), "STORED - N/A");
        controls_.set(state);
    } else {
        char value[24];
        formatTargetValue(config.target, state, value, sizeof(value));
        snprintf(overlayValue_, sizeof(overlayValue_), "%s PICKUP - %s",
                 pickup.direction > 0 ? "UP" : "DOWN", value);
    }
    changed();
    return true;
}

void PerformanceController::setEncoderPressed(bool pressed) {
    if (encoderPressed_ == pressed) return;
    overlayTitle_[0] = '\0';
    overlayValue_[0] = '\0';
    encoderPressed_ = pressed;
    if (pressed) {
        modifierUsed_ = false;
    } else if (displayMode_ == DisplayMode::Menu) {
        if (!modifierUsed_) pressMenuItem();
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
        // Extended BTN1+turn edits Depth before the generic live-pitch gesture.
        // A BTN1 consumed by an encoder-click shortcut is not a held modifier.
        if (state.profile == PerformanceProfile::Extended &&
            padDown_[0] && !consumed_[0] && isRelevantButton(0)) {
            modifierUsed_ = true;
            state.lfoDepthOctaves = clampFloat(state.lfoDepthOctaves +
                (direction < 0 ? -0.05f : 0.05f), 0.0f, 2.0f);
            controls_.set(state);
            notifyExternalChange(PotTarget::LfoDepth);
            snprintf(overlayTitle_, sizeof(overlayTitle_), "DEPTH");
            if (state.mode == SirenMode::Syndrum || state.mode == SirenMode::Gunshot)
                snprintf(overlayValue_, sizeof(overlayValue_), "STORED - N/A");
            else
                snprintf(overlayValue_, sizeof(overlayValue_), "%.2f oct", state.lfoDepthOctaves);
            return;
        }
        if (anyRelevantButtonDown() &&
            settings_.encoderHoldAction == EncoderHoldAction::LivePitch) {
            modifierUsed_ = true;
            snprintf(overlayTitle_, sizeof(overlayTitle_), "LIVE PITCH");
            if (!pitchAvailable(state)) {
                snprintf(overlayValue_, sizeof(overlayValue_), "N/A");
            } else {
                const int amount = state.liveTransposeSemitones +
                    (direction < 0 ? -settings_.encoderStepSemitones
                                   : settings_.encoderStepSemitones);
                state.liveTransposeSemitones = static_cast<int8_t>(clampFloat(
                    static_cast<float>(amount),
                    -static_cast<float>(settings_.encoderRangeSemitones),
                    static_cast<float>(settings_.encoderRangeSemitones)));
                snprintf(overlayValue_, sizeof(overlayValue_), "%+d st - %.0f Hz",
                    state.liveTransposeSemitones,
                    state.tuneHz * powf(2.0f, state.liveTransposeSemitones / 12.0f));
                controls_.set(state);
            }
            changed();
            return;
        }
        if (state.profile == PerformanceProfile::Classic) {
            overlayTitle_[0] = '\0';
            overlayValue_[0] = '\0';
            state.mode = static_cast<SirenMode>(wrappedStep(
                static_cast<int>(state.mode), direction, 4));
        } else {
            overlayTitle_[0] = '\0';
            overlayValue_[0] = '\0';
            stepVariant(direction);
            return;
        }
        controls_.set(state);
        changed();
        return;
    }
    normalizeMenuIndex();
    rebuildMenuView();
    if (menuEditing_) editMenuItem(menuRows_[menuIndex_].row, direction);
    else {
        menuIndex_ = static_cast<uint8_t>(wrappedStep(
            menuIndex_, direction, static_cast<int>(menuRowCount_)));
        changed();
    }
}

void PerformanceController::resetMomentaries() {
    overlayTitle_[0] = '\0';
    overlayValue_[0] = '\0';
    for (unsigned index = 0; index < 4; ++index) {
        padDown_[index] = false;
        consumed_[index] = false;
        pressOrder_[index] = 0;
    }
    activePad_ = -1;
    encoderPressed_ = false;
    modifierUsed_ = false;
    ControlState state = controls_.snapshot();
    state.liveTransposeSemitones = 0;
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
    ButtonConfig &button = settings_.profiles[1].buttons[selectedPad_];
    if (button.action != ButtonAction::PlaySound) return;
    button.sound = static_cast<PanelSound>(wrappedStep(
        static_cast<int>(button.sound), direction,
        static_cast<int>(PanelSound::Count)));
    button.variant = 0;
    if (!panelSoundSupportsLatch(button.sound)) button.latch = false;
    ControlState state = controls_.snapshot();
    if (state.profile == PerformanceProfile::Extended && currentPad_ == selectedPad_)
        applySound(state, selectedPad_, false);
    controls_.set(state);
    commitSettingsChange();
}

void PerformanceController::stepVariant(int direction) {
    ButtonConfig &button = settings_.profiles[1].buttons[selectedPad_];
    if (button.action != ButtonAction::PlaySound) return;
    button.variant = static_cast<uint8_t>(wrappedStep(button.variant, direction,
        static_cast<int>(panelSoundVariantCount(button.sound))));
    ControlState state = controls_.snapshot();
    if (state.profile == PerformanceProfile::Extended && currentPad_ == selectedPad_)
        applySound(state, selectedPad_, false);
    controls_.set(state);
    commitSettingsChange();
}

void PerformanceController::toggleSelectedHold() {
    ButtonConfig &button = settings_.profiles[1].buttons[selectedPad_];
    if (button.action != ButtonAction::PlaySound ||
        !panelSoundSupportsLatch(button.sound)) return;
    button.latch = !button.latch;
    ControlState state = controls_.snapshot();
    if (state.profile == PerformanceProfile::Extended && currentPad_ == selectedPad_) {
        applySound(state, selectedPad_, false);
        updateGate(state);
        controls_.set(state);
    }
    commitSettingsChange();
}

bool PerformanceController::configurePad(unsigned padNumber, const String &sound,
                                         unsigned variant, bool hold) {
    PanelSound parsed;
    if (padNumber < 1 || padNumber > 4 || !parseSound(sound, parsed)) return false;
    return configureButtonSound(PerformanceProfile::Extended, padNumber,
                                parsed, variant, hold);
}

const char *PerformanceController::padSoundName(unsigned padNumber) const {
    if (padNumber < 1 || padNumber > 4) return "UNKNOWN";
    const ButtonConfig &button = settings_.profiles[1].buttons[padNumber - 1];
    return button.action == ButtonAction::PlaySound
        ? panelSoundName(button.sound) : buttonActionName(button.action);
}

const char *PerformanceController::padVariantName(unsigned padNumber) const {
    if (padNumber < 1 || padNumber > 4) return "UNKNOWN";
    const ButtonConfig &button = settings_.profiles[1].buttons[padNumber - 1];
    return button.action == ButtonAction::PlaySound
        ? panelSoundVariantName(button.sound, button.variant) : "N/A";
}

unsigned PerformanceController::padVariant(unsigned padNumber) const {
    return padNumber >= 1 && padNumber <= 4
        ? settings_.profiles[1].buttons[padNumber - 1].variant : 0;
}

bool PerformanceController::padHold(unsigned padNumber) const {
    return padNumber >= 1 && padNumber <= 4 &&
        settings_.profiles[1].buttons[padNumber - 1].latch;
}

bool PerformanceController::padHoldSupported(unsigned padNumber) const {
    if (padNumber < 1 || padNumber > 4) return false;
    const ButtonConfig &button = settings_.profiles[1].buttons[padNumber - 1];
    return button.action == ButtonAction::PlaySound &&
           panelSoundSupportsLatch(button.sound);
}

const char *PerformanceController::buttonActionNameFor(
        PerformanceProfile profile, unsigned buttonNumber) const {
    if (buttonNumber < 1 || buttonNumber > 4) return "UNKNOWN";
    return buttonActionName(settings_.profiles[profileIndex(profile)]
        .buttons[buttonNumber - 1].action);
}

bool PerformanceController::configurePotTarget(PerformanceProfile profile,
        unsigned potNumber, PotTarget target) {
    if (potNumber < 1 || potNumber > 6 || !targetSupported(profile, target)) return rejectSettingsChange();
    PanelSettings candidate = settingsTransaction_ ? stagedSettings_ : settings_;
    candidate.profiles[profileIndex(profile)].pots[potNumber - 1] = continuousPot(target);
    if (!validatePanelSettings(candidate)) return rejectSettingsChange();
    acceptSettingsCandidate(candidate, true);
    return true;
}

bool PerformanceController::configurePotMode(PerformanceProfile profile,
        unsigned potNumber, PotMode mode) {
    if (potNumber < 1 || potNumber > 6) return rejectSettingsChange();
    PanelSettings candidate = settingsTransaction_ ? stagedSettings_ : settings_;
    PotConfig &pot = candidate.profiles[profileIndex(profile)].pots[potNumber - 1];
    pot.mode = mode;
    if (mode == PotMode::Zones) {
        if (pot.target == PotTarget::Pitch) {
            for (unsigned i = 0; i < 3; ++i) pot.zones[i] = PerformanceConfig::kClassicPitchHz[i];
        } else if (pot.target == PotTarget::Rate) {
            for (unsigned i = 0; i < 3; ++i) pot.zones[i] = PerformanceConfig::kClassicRateHz[i];
        }
    }
    if (!validatePanelSettings(candidate)) return rejectSettingsChange();
    acceptSettingsCandidate(candidate, true);
    return true;
}

bool PerformanceController::configurePotRange(PerformanceProfile profile,
        unsigned potNumber, float minimum, float maximum) {
    if (potNumber < 1 || potNumber > 6) return rejectSettingsChange();
    PanelSettings candidate = settingsTransaction_ ? stagedSettings_ : settings_;
    PotConfig &pot = candidate.profiles[profileIndex(profile)].pots[potNumber - 1];
    pot.minimum = minimum;
    pot.maximum = maximum;
    if (!validatePanelSettings(candidate)) return rejectSettingsChange();
    acceptSettingsCandidate(candidate, true);
    return true;
}

bool PerformanceController::configurePotZones(PerformanceProfile profile,
        unsigned potNumber, float low, float middle, float high) {
    if (potNumber < 1 || potNumber > 6) return rejectSettingsChange();
    PanelSettings candidate = settingsTransaction_ ? stagedSettings_ : settings_;
    PotConfig &pot = candidate.profiles[profileIndex(profile)].pots[potNumber - 1];
    pot.zones[0] = low; pot.zones[1] = middle; pot.zones[2] = high;
    if (!validatePanelSettings(candidate)) return rejectSettingsChange();
    acceptSettingsCandidate(candidate, true);
    return true;
}

bool PerformanceController::configureButtonAction(PerformanceProfile profile,
        unsigned buttonNumber, ButtonAction action) {
    if (buttonNumber < 1 || buttonNumber > 4 || action >= ButtonAction::Count) return rejectSettingsChange();
    PanelSettings candidate = settingsTransaction_ ? stagedSettings_ : settings_;
    ButtonConfig &button = candidate.profiles[profileIndex(profile)].buttons[buttonNumber - 1];
    button.action = action;
    if (action == ButtonAction::PlaySound && button.sound >= PanelSound::Count)
        button.sound = PanelSound::Classic;
    if (!validatePanelSettings(candidate)) return rejectSettingsChange();
    acceptSettingsCandidate(candidate);
    return true;
}

bool PerformanceController::configureButtonSound(PerformanceProfile profile,
        unsigned buttonNumber, PanelSound sound, unsigned variant, bool latch) {
    if (buttonNumber < 1 || buttonNumber > 4) return rejectSettingsChange();
    PanelSettings candidate = settingsTransaction_ ? stagedSettings_ : settings_;
    ButtonConfig &button = candidate.profiles[profileIndex(profile)].buttons[buttonNumber - 1];
    if (button.action != ButtonAction::PlaySound) return rejectSettingsChange();
    button.sound = sound;
    button.variant = static_cast<uint8_t>(variant);
    button.latch = latch;
    if (!validatePanelSettings(candidate)) return rejectSettingsChange();
    acceptSettingsCandidate(candidate);
    ControlState state = controls_.snapshot();
    if (!settingsTransaction_ && profile == PerformanceProfile::Extended &&
        state.profile == profile && currentPad_ == buttonNumber - 1) {
        applySound(state, buttonNumber - 1, false);
        controls_.set(state);
    }
    return true;
}

bool PerformanceController::configurePotBehavior(PotBehavior behavior) {
    if (behavior > PotBehavior::Jump) return rejectSettingsChange();
    PanelSettings candidate = settingsTransaction_ ? stagedSettings_ : settings_;
    candidate.potBehavior = behavior;
    acceptSettingsCandidate(candidate, true);
    return true;
}

bool PerformanceController::configureEncoder(EncoderHoldAction action,
        unsigned step, unsigned range) {
    PanelSettings candidate = settingsTransaction_ ? stagedSettings_ : settings_;
    candidate.encoderHoldAction = action;
    candidate.encoderStepSemitones = static_cast<uint8_t>(step);
    candidate.encoderRangeSemitones = static_cast<uint8_t>(range);
    if (!validatePanelSettings(candidate)) return rejectSettingsChange();
    acceptSettingsCandidate(candidate);
    return true;
}

bool PerformanceController::applySettings(const PanelSettings &settings) {
    if (!validatePanelSettings(settings)) return false;
    settingsTransaction_ = false;
    settingsTransactionInvalid_ = false;
    settings_ = settings;
    resetPickup();
    ControlState state = controls_.snapshot();
    if (state.profile == PerformanceProfile::Classic) {
        state.tuneHz = classicPitchHz(static_cast<unsigned>(state.classicPitch));
        state.classicPitchUsesTune = true;
        const unsigned rate = static_cast<unsigned>(state.classicModulation);
        if (rate < 3) state.lfoRateHz = classicRateHz(rate);
        state.classicRateUsesRate = true;
    }
    updateZoneFlags(state);
    controls_.set(state);
    commitSettingsChange();
    return true;
}

void PerformanceController::resetDefaults() {
    settingsTransaction_ = false;
    settingsTransactionInvalid_ = false;
    settings_ = factoryPanelSettings();
    resetPickup();
    ControlState state = controls_.snapshot();
    if (state.profile == PerformanceProfile::Classic) {
        state.tuneHz = classicPitchHz(static_cast<unsigned>(state.classicPitch));
        const unsigned rate = static_cast<unsigned>(state.classicModulation);
        if (rate < 3) state.lfoRateHz = classicRateHz(rate);
        state.classicPitchUsesTune = true;
        state.classicRateUsesRate = true;
    }
    updateZoneFlags(state);
    controls_.set(state);
    commitSettingsChange();
}

void PerformanceController::commitSettingsChange() {
    ControlState state = controls_.snapshot();
    const ProfilePanelSettings &profile = settings_.profiles[profileIndex(state.profile)];
    bool hasHoldAction = false;
    for (const ButtonConfig &button : profile.buttons)
        hasHoldAction |= button.action == ButtonAction::Hold;
    if (!hasHoldAction) holdToggle_ = false;
    updateGate(state);
    updateZoneFlags(state);
    controls_.set(state);
    ++settingsRevision_;
    changed();
}

void PerformanceController::updateZoneFlags(ControlState &state) const {
    state.pitchZonesActive = false;
    state.rateZonesActive = false;
    const ProfilePanelSettings &profile = settings_.profiles[profileIndex(state.profile)];
    for (const PotConfig &pot : profile.pots) {
        state.pitchZonesActive |= pot.target == PotTarget::Pitch &&
                                  pot.mode == PotMode::Zones;
        state.rateZonesActive |= pot.target == PotTarget::Rate &&
                                 pot.mode == PotMode::Zones;
    }
}

void PerformanceController::acceptSettingsCandidate(
        const PanelSettings &candidate, bool resetPotPickup) {
    if (settingsTransaction_) {
        stagedSettings_ = candidate;
        return;
    }
    settings_ = candidate;
    if (resetPotPickup) resetPickup();
    commitSettingsChange();
}

bool PerformanceController::rejectSettingsChange() {
    if (settingsTransaction_) settingsTransactionInvalid_ = true;
    return false;
}

bool PerformanceController::beginSettingsTransaction() {
    if (settingsTransaction_) return false;
    stagedSettings_ = settings_;
    settingsTransaction_ = true;
    settingsTransactionInvalid_ = false;
    return true;
}

bool PerformanceController::commitSettingsTransaction() {
    if (!settingsTransaction_) return false;
    if (settingsTransactionInvalid_ || !validatePanelSettings(stagedSettings_)) {
        cancelSettingsTransaction();
        return false;
    }
    settingsTransaction_ = false;
    settings_ = stagedSettings_;
    resetPickup();
    ControlState state = controls_.snapshot();
    if (state.profile == PerformanceProfile::Classic) {
        state.tuneHz = classicPitchHz(static_cast<unsigned>(state.classicPitch));
        const unsigned rate = static_cast<unsigned>(state.classicModulation);
        if (rate < 3) state.lfoRateHz = classicRateHz(rate);
        state.classicPitchUsesTune = true;
        state.classicRateUsesRate = true;
        controls_.set(state);
    }
    commitSettingsChange();
    return true;
}

void PerformanceController::cancelSettingsTransaction() {
    settingsTransaction_ = false;
    settingsTransactionInvalid_ = false;
    stagedSettings_ = settings_;
}

void PerformanceController::notifyExternalChange(PotTarget target) {
    overlayTitle_[0] = '\0';
    overlayValue_[0] = '\0';
    ControlState state = controls_.snapshot();
    updateZoneFlags(state);
    controls_.set(state);
    const auto &pots = settings_.profiles[profileIndex(state.profile)].pots;
    for (unsigned i = 0; i < 6; ++i)
        if (pots[i].target == target) pickup_[i] = PickupState{};
    changed();
}

float PerformanceController::classicPitchHz(unsigned index) const {
    for (const PotConfig &pot : settings_.profiles[0].pots)
        if (pot.target == PotTarget::Pitch && pot.mode == PotMode::Zones)
            return pot.zones[index < 3 ? index : 1];
    return PerformanceConfig::kClassicPitchHz[index < 3 ? index : 1];
}

float PerformanceController::classicRateHz(unsigned index) const {
    for (const PotConfig &pot : settings_.profiles[0].pots)
        if (pot.target == PotTarget::Rate && pot.mode == PotMode::Zones)
            return pot.zones[index < 3 ? index : 1];
    return PerformanceConfig::kClassicRateHz[index < 3 ? index : 1];
}

bool PerformanceController::pickupWaiting(unsigned potNumber) const {
    return potNumber >= 1 && potNumber <= 6 && pickup_[potNumber - 1].seen &&
           !pickup_[potNumber - 1].latched &&
           settings_.potBehavior == PotBehavior::Pickup;
}

int PerformanceController::pickupDirection(unsigned potNumber) const {
    return potNumber >= 1 && potNumber <= 6 ? pickup_[potNumber - 1].direction : 0;
}

const char *PerformanceController::displayModeName() const {
    return displayMode_ == DisplayMode::Home ? "HOME" : "MENU";
}

void PerformanceController::openMenu() {
    displayMode_ = DisplayMode::Menu;
    menuPage_ = MenuPage::Root;
    menuIndex_ = 0;
    menuEditing_ = false;
    overlayTitle_[0] = '\0';
    overlayValue_[0] = '\0';
    ++menuRevision_;
}

void PerformanceController::setMenuPage(MenuPage page) {
    menuPage_ = page;
    menuIndex_ = 0;
    menuEditing_ = false;
    changed();
}

void PerformanceController::addMenuRow(Row row, const char *key,
        const char *label, const char *value) const {
    if (menuRowCount_ >= 24) return;
    MenuRowView &view = menuRows_[menuRowCount_++];
    view.row = row;
    view.key = key;
    view.label = label;
    snprintf(view.value, sizeof(view.value), "%s", value == nullptr ? "" : value);
}

void PerformanceController::rebuildMenuView() const {
    menuRowCount_ = 0;
    const ControlState state = controls_.snapshot();
    const unsigned profile = profileIndex(state.profile);
    char value[32];
    snprintf(menuFooterBuffer_, sizeof(menuFooterBuffer_), "%s",
        menuEditing_ ? "TURN CHANGE / PUSH DONE" : "TURN SELECT / PUSH OPEN");
    if (displayMode_ == DisplayMode::Menu && overlayTitle_[0])
        snprintf(menuFooterBuffer_, sizeof(menuFooterBuffer_), "%s %s",
                 overlayTitle_, overlayValue_);
    if (persistenceStatus_[0]) snprintf(menuFooterBuffer_, sizeof(menuFooterBuffer_),
                                       "%s", persistenceStatus_);
    switch (menuPage_) {
        case MenuPage::Root:
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "PERFORMANCE");
            addMenuRow(Row::SoundPage, "SOUND", "SOUND");
            addMenuRow(Row::ModPage, "MOD", "MOD");
            addMenuRow(Row::EchoPage, "ECHO", "ECHO");
            addMenuRow(Row::SetupPage, "SETUP", "SETUP");
            addMenuRow(Row::Back, "BACK", "BACK");
            break;
        case MenuPage::Setup:
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "SETUP");
            addMenuRow(Row::Profile, "PROFILE", "PROFILE",
                       state.profile == PerformanceProfile::Classic ? "CLASSIC" : "EXTENDED");
            addMenuRow(Row::Pots, "POTS", "POTS");
            addMenuRow(Row::Buttons, "BUTTONS", "BUTTONS");
            addMenuRow(Row::Encoder, "ENCODER", "ENCODER");
            addMenuRow(Row::ResetDefaults, "RESET_DEFAULTS", "RESET DEFAULTS");
            addMenuRow(Row::Back, "BACK", "BACK");
            break;
        case MenuPage::Sound: {
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "SOUND P%u", selectedPad_ + 1);
            if (state.profile == PerformanceProfile::Classic) {
                addMenuRow(Row::Voice, "VOICE", "VOICE", modeName(state.mode));
            } else {
                snprintf(value, sizeof(value), "%u", selectedPad_ + 1);
                addMenuRow(Row::SelectedPad, "PAD", "PAD", value);
                const auto &button = settings_.profiles[profile].buttons[selectedPad_];
                addMenuRow(Row::Sound, "SOUND", "FAMILY", panelSoundName(button.sound));
                addMenuRow(Row::Variant, "VARIANT", "VARIANT", panelSoundVariantName(button.sound, button.variant));
            }
            if (state.mode != SirenMode::Gunshot) addSoundValue(state, Row::Tune, "TUNE", "PITCH", "Hz");
            if (state.profile == PerformanceProfile::Extended) {
                const bool articulated = state.mode == SirenMode::Lickshot &&
                    (state.lickshotArticulation == LickshotArticulation::Shot || state.lickshotArticulation == LickshotArticulation::Roll);
                if (articulated) {
                    addSoundValue(state, Row::Length, "LENGTH", "LENGTH", "ms");
                    addSoundValue(state, Row::Punch, "PUNCH", "PUNCH", "oct");
                    addSoundValue(state, Row::PitchTime, "PITCH_TIME", "PITCH TAU", "ms");
                    if (state.lickshotArticulation == LickshotArticulation::Roll) addSoundValue(state, Row::Rate, "RATE", "ROLL RATE", "Hz");
                } else addSoundValue(state, Row::Decay, "DECAY", "DECAY TAU", "ms");
                if (state.mode == SirenMode::Syndrum) addSoundValue(state, Row::DrumSweep, "DRUM_SWEEP", "PITCH SWEEP", "oct");
                else if (state.mode != SirenMode::Gunshot) {
                    addSoundValue(state, Row::Sweep, "SWEEP", "DYNAMIC SWEEP", "");
                    addSoundValue(state, Row::SweepTime, "SWEEP_TIME", "SWEEP TAU", "ms");
                }
                addSoundValue(state, Row::Tone, "TONE", "DRY TONE", "Hz");
                addSoundValue(state, Row::Contour, "CONTOUR", "FILTER CONTOUR", "oct");
                addSoundValue(state, Row::ContourTime, "CONTOUR_TIME", "CONTOUR TAU", "ms");
            }
            addMenuRow(Row::Back, "BACK", "BACK");
            break;
        }
        case MenuPage::Mod:
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "MODULATION");
            if (state.profile == PerformanceProfile::Classic) {
                addMenuRow(Row::ClassicRate, "CLASSIC_RATE", "RATE", rateName(state.classicModulation));
            } else if (state.mode != SirenMode::Syndrum && state.mode != SirenMode::Gunshot &&
                       !(state.mode == SirenMode::Lickshot && state.lickshotArticulation != LickshotArticulation::Legacy)) {
                static const char *shapes[] = {"RC", "TRIANGLE", "SQUARE", "SAW UP", "SAW DOWN", "ASYM UP", "ASYM DOWN", "PULSE 25", "PULSE 75", "MANUAL"};
                addMenuRow(Row::Shape, "SHAPE", "SHAPE", shapes[static_cast<unsigned>(state.lfoShape)]);
                addMenuRow(Row::Modulation, "MODULATION", "MODULATION", state.modulationEnabled ? "ON" : "OFF");
                addSoundValue(state, Row::Rate, "RATE", "RATE", "Hz");
                if (state.mode != SirenMode::TestTone) addSoundValue(state, Row::Depth, "DEPTH", "DEPTH", "oct");
                addMenuRow(Row::Retrigger, "RETRIGGER", "RETRIGGER", state.retrigger ? "ON" : "OFF");
            }
            addMenuRow(Row::Back, "BACK", "BACK");
            break;
        case MenuPage::Echo:
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "ECHO");
            addSoundValue(state, Row::EchoLevel, "LEVEL", "LEVEL", "");
            addSoundValue(state, Row::EchoTime, "TIME", "TIME", "ms");
            addSoundValue(state, Row::Feedback, "FEEDBACK", "FEEDBACK", "");
            addSoundValue(state, Row::Filter, "FILTER", "FILTER LP / HP", "");
            if (state.profile == PerformanceProfile::Extended) {
                addSoundValue(state, Row::Color, "COLOR", "COLOR", "");
                addMenuRow(Row::Loop, "LOOP", "LOOP", state.echoLoop ? "ON" : "OFF");
                addMenuRow(Row::Spin, "SPIN", "SPIN (STATUS)", state.echoSpin ? "ON" : "OFF");
            }
            addMenuRow(Row::Cut, "CUT", "CUT (STATUS)", state.echoCut ? "ON" : "OFF");
            addMenuRow(Row::Back, "BACK", "BACK");
            break;
        case MenuPage::Pots:
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "POT CONTROLS");
            addMenuRow(Row::Behavior, "BEHAVIOR", "BEHAVIOR",
                       potBehaviorName(settings_.potBehavior));
            for (unsigned i = 0; i < 6; ++i) {
                snprintf(value, sizeof(value), "%s", potTargetName(settings_.profiles[profile].pots[i].target));
                static const Row rows[] = {Row::Pot1, Row::Pot2, Row::Pot3,
                    Row::Pot4, Row::Pot5, Row::Pot6};
                static const char *keys[] = {"POT_1", "POT_2", "POT_3", "POT_4", "POT_5", "POT_6"};
                static const char *labels[] = {"POT 1", "POT 2", "POT 3", "POT 4", "POT 5", "POT 6"};
                addMenuRow(rows[i], keys[i], labels[i], value);
            }
            addMenuRow(Row::Back, "BACK", "BACK");
            break;
        case MenuPage::Pot: {
            const PotConfig &pot = settings_.profiles[profile].pots[selectedPot_];
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "POT %u - %s",
                     selectedPot_ + 1, potTargetName(pot.target));
            addMenuRow(Row::Function, "FUNCTION", "FUNCTION", potTargetName(pot.target));
            const TargetDescriptor *descriptor = targetDescriptor(pot.target);
            if (descriptor != nullptr && descriptor->zones &&
                state.profile == PerformanceProfile::Classic)
                addMenuRow(Row::Mode, "MODE", "MODE", potModeName(pot.mode));
            if (pot.mode == PotMode::Zones) {
                formatConfiguredValue(pot.target, pot.zones[0], value, sizeof(value));
                addMenuRow(Row::Low, "LOW", pot.target == PotTarget::Pitch ? "LOW" : "SLOW", value);
                formatConfiguredValue(pot.target, pot.zones[1], value, sizeof(value));
                addMenuRow(Row::Middle, "MID", "MID", value);
                formatConfiguredValue(pot.target, pot.zones[2], value, sizeof(value));
                addMenuRow(Row::High, "HIGH", pot.target == PotTarget::Pitch ? "HIGH" : "FAST", value);
            } else if (pot.target != PotTarget::None) {
                formatConfiguredValue(pot.target, pot.minimum, value, sizeof(value));
                addMenuRow(Row::Minimum, "MIN", "MIN", value);
                formatConfiguredValue(pot.target, pot.maximum, value, sizeof(value));
                addMenuRow(Row::Maximum, "MAX", "MAX", value);
            }
            addMenuRow(Row::Back, "BACK", "BACK");
            break;
        }
        case MenuPage::Buttons:
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "BUTTONS");
            for (unsigned i = 0; i < 4; ++i) {
                static const Row rows[] = {Row::Button1, Row::Button2, Row::Button3, Row::Button4};
                static const char *keys[] = {"BTN_1", "BTN_2", "BTN_3", "BTN_4"};
                static const char *labels[] = {"BTN 1", "BTN 2", "BTN 3", "BTN 4"};
                addMenuRow(rows[i], keys[i], labels[i],
                    buttonActionName(settings_.profiles[profile].buttons[i].action));
            }
            addMenuRow(Row::Back, "BACK", "BACK");
            break;
        case MenuPage::Button: {
            const ButtonConfig &button = settings_.profiles[profile].buttons[selectedButton_];
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "BTN %u", selectedButton_ + 1);
            addMenuRow(Row::Action, "ACTION", "ACTION", buttonActionName(button.action));
            if (button.action == ButtonAction::PlaySound) {
                addMenuRow(Row::Sound, "SOUND", "SOUND", panelSoundName(button.sound));
                addMenuRow(Row::Variant, "VARIANT", "VARIANT",
                           panelSoundVariantName(button.sound, button.variant));
                if (panelSoundSupportsLatch(button.sound))
                    addMenuRow(Row::Latch, "LATCH", "LATCH", button.latch ? "ON" : "OFF");
            }
            addMenuRow(Row::Back, "BACK", "BACK");
            break;
        }
        case MenuPage::Encoder:
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "ENCODER");
            addMenuRow(Row::HoldTurn, "HOLD_TURN", "HOLD+TURN",
                settings_.encoderHoldAction == EncoderHoldAction::LivePitch ? "LIVE PITCH" : "OFF");
            snprintf(value, sizeof(value), "%u st", settings_.encoderStepSemitones);
            addMenuRow(Row::Step, "STEP", "STEP", value);
            snprintf(value, sizeof(value), "+/-%u st", settings_.encoderRangeSemitones);
            addMenuRow(Row::Range, "RANGE", "RANGE", value);
            addMenuRow(Row::Back, "BACK", "BACK");
            break;
        case MenuPage::Reset:
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "RESET DEFAULTS");
            addMenuRow(Row::Confirm, "CONFIRM", "CONFIRM", resetConfirmed_ ? "YES" : "NO");
            addMenuRow(Row::Back, "BACK", "BACK");
            break;
    }
}

unsigned PerformanceController::menuItemCount() const {
    rebuildMenuView();
    return menuRowCount_;
}

const char *PerformanceController::menuItemKey(unsigned index) const {
    rebuildMenuView();
    return index < menuRowCount_ ? menuRows_[index].key : "BACK";
}

const char *PerformanceController::menuItemLabel(unsigned index) const {
    rebuildMenuView();
    return index < menuRowCount_ ? menuRows_[index].label : "BACK";
}

const char *PerformanceController::menuItemValue(unsigned index) const {
    rebuildMenuView();
    return index < menuRowCount_ ? menuRows_[index].value : "";
}

const char *PerformanceController::menuTitle() const {
    rebuildMenuView();
    return menuTitleBuffer_;
}

const char *PerformanceController::menuFooter() const {
    rebuildMenuView();
    return menuFooterBuffer_;
}

void PerformanceController::pressMenuItem() {
    normalizeMenuIndex();
    rebuildMenuView();
    const Row row = menuRows_[menuIndex_].row;
    if (menuEditing_) {
        menuEditing_ = false;
        if (row == Row::Confirm && resetConfirmed_) {
            resetDefaults();
            resetConfirmed_ = false;
            setMenuPage(MenuPage::Root);
            return;
        }
        if (row != Row::Profile && menuPage_ != MenuPage::Sound && menuPage_ != MenuPage::Mod && menuPage_ != MenuPage::Echo) ++settingsRevision_;
        changed();
        return;
    }
    if (row == Row::SoundPage) { selectedButton_ = selectedPad_; setMenuPage(MenuPage::Sound); }
    else if (row == Row::ModPage) setMenuPage(MenuPage::Mod);
    else if (row == Row::EchoPage) setMenuPage(MenuPage::Echo);
    else if (row == Row::SetupPage) setMenuPage(MenuPage::Setup);
    else if (row == Row::Spin || row == Row::Cut) return;
    else if (row >= Row::Pot1 && row <= Row::Pot6) {
        selectedPot_ = static_cast<unsigned>(row) - static_cast<unsigned>(Row::Pot1);
        setMenuPage(MenuPage::Pot);
    } else if (row >= Row::Button1 && row <= Row::Button4) {
        selectedButton_ = static_cast<unsigned>(row) - static_cast<unsigned>(Row::Button1);
        setMenuPage(MenuPage::Button);
    } else if (row == Row::Pots) setMenuPage(MenuPage::Pots);
    else if (row == Row::Buttons) setMenuPage(MenuPage::Buttons);
    else if (row == Row::Encoder) setMenuPage(MenuPage::Encoder);
    else if (row == Row::ResetDefaults) {
        resetConfirmed_ = false;
        setMenuPage(MenuPage::Reset);
    } else if (row == Row::Back) {
        if (menuPage_ == MenuPage::Root) {
            displayMode_ = DisplayMode::Home;
            changed();
        } else if (menuPage_ == MenuPage::Pot) setMenuPage(MenuPage::Pots);
        else if (menuPage_ == MenuPage::Button) setMenuPage(MenuPage::Buttons);
        else if (menuPage_ == MenuPage::Pots || menuPage_ == MenuPage::Buttons || menuPage_ == MenuPage::Encoder || menuPage_ == MenuPage::Reset) setMenuPage(MenuPage::Setup);
        else setMenuPage(MenuPage::Root);
    } else {
        menuEditing_ = true;
        changed();
    }
}

void PerformanceController::editMenuItem(Row row, int direction) {
    ControlState state = controls_.snapshot();
    const unsigned profile = profileIndex(state.profile);
    PotConfig &pot = settings_.profiles[profile].pots[selectedPot_];
    ButtonConfig &button = settings_.profiles[profile].buttons[menuPage_ == MenuPage::Sound ? selectedPad_ : selectedButton_];
    float minimum, maximum, step;
    if (float *value = soundValue(state, row, minimum, maximum, step)) {
        *value = clampFloat(*value + direction * step, minimum, maximum);
        if (row == Row::Tune) { state.classicPitchUsesTune = true; state.pitchZonesActive = false; }
        if (row == Row::Filter) applyEchoFilter(state, *value);
        controls_.set(state); resetPickup(); changed(); return;
    }
    switch (row) {
        case Row::SelectedPad:
            selectedPad_ = wrappedStep(selectedPad_, direction, 4);
            applySound(state, selectedPad_, false); controls_.set(state); resetPickup(); break;
        case Row::Voice:
            state.mode = static_cast<SirenMode>(wrappedStep(static_cast<int>(state.mode), direction, 4)); controls_.set(state); break;
        case Row::Shape:
            state.lfoShape = static_cast<LfoShape>(wrappedStep(static_cast<int>(state.lfoShape), direction, 10)); controls_.set(state); break;
        case Row::Modulation: state.modulationEnabled = !state.modulationEnabled; controls_.set(state); break;
        case Row::Retrigger: state.retrigger = !state.retrigger; controls_.set(state); break;
        case Row::Loop: state.echoLoop = !state.echoLoop; controls_.set(state); break;
        case Row::ClassicRate:
            state.classicModulation = static_cast<ClassicModulation>(wrappedStep(static_cast<int>(state.classicModulation), direction, 4));
            if (state.classicModulation != ClassicModulation::Manual) state.lfoRateHz = classicRateHz(static_cast<unsigned>(state.classicModulation));
            controls_.set(state); resetPickup(); break;
        case Row::Profile:
            setProfile(state.profile == PerformanceProfile::Classic
                ? PerformanceProfile::Extended : PerformanceProfile::Classic);
            menuEditing_ = true;
            break;
        case Row::Behavior:
            settings_.potBehavior = settings_.potBehavior == PotBehavior::Pickup
                ? PotBehavior::Jump : PotBehavior::Pickup;
            resetPickup();
            break;
        case Row::Function: {
            int candidate = static_cast<int>(pot.target);
            do candidate = wrappedStep(candidate, direction, static_cast<int>(PotTarget::Count));
            while (!targetSupported(state.profile, static_cast<PotTarget>(candidate)));
            pot = continuousPot(static_cast<PotTarget>(candidate));
            resetPickup();
            break;
        }
        case Row::Mode:
            pot.mode = pot.mode == PotMode::Zones ? PotMode::Continuous : PotMode::Zones;
            if (pot.mode == PotMode::Zones) {
                const float *defaults = pot.target == PotTarget::Pitch
                    ? PerformanceConfig::kClassicPitchHz : PerformanceConfig::kClassicRateHz;
                for (unsigned i = 0; i < 3; ++i) pot.zones[i] = defaults[i];
            }
            resetPickup();
            break;
        case Row::Minimum:
        case Row::Maximum: {
            const TargetDescriptor &descriptor = *targetDescriptor(pot.target);
            float &value = row == Row::Minimum ? pot.minimum : pot.maximum;
            const float edited = clampFloat(editingStep(descriptor, value, direction),
                                             descriptor.absoluteMin, descriptor.absoluteMax);
            if ((row == Row::Minimum && edited < pot.maximum) ||
                (row == Row::Maximum && edited > pot.minimum)) value = edited;
            resetPickup();
            break;
        }
        case Row::Low:
        case Row::Middle:
        case Row::High: {
            const unsigned zone = row == Row::Low ? 0 : row == Row::Middle ? 1 : 2;
            const TargetDescriptor &descriptor = *targetDescriptor(pot.target);
            const float edited = clampFloat(editingStep(descriptor, pot.zones[zone], direction),
                                             descriptor.absoluteMin, descriptor.absoluteMax);
            if ((zone == 0 || edited > pot.zones[zone - 1]) &&
                (zone == 2 || edited < pot.zones[zone + 1])) pot.zones[zone] = edited;
            break;
        }
        case Row::Action:
            do { button.action = static_cast<ButtonAction>(wrappedStep(
                static_cast<int>(button.action), direction,
                static_cast<int>(ButtonAction::Count)));
            } while (state.profile == PerformanceProfile::Classic && (button.action == ButtonAction::EchoLoop || button.action == ButtonAction::EchoSpin));
            if (button.action == ButtonAction::PlaySound) {
                button.sound = PanelSound::Classic;
                button.variant = 0;
                button.latch = false;
            }
            break;
        case Row::Sound:
            do { button.sound = static_cast<PanelSound>(wrappedStep(
                    static_cast<int>(button.sound), direction,
                    static_cast<int>(PanelSound::Count)));
            } while (state.profile == PerformanceProfile::Classic && button.sound != PanelSound::Classic && button.sound != PanelSound::TwoTone && button.sound != PanelSound::TestTone);
            button.variant = 0;
            if (!panelSoundSupportsLatch(button.sound)) button.latch = false;
            break;
        case Row::Variant:
            button.variant = static_cast<uint8_t>(wrappedStep(button.variant, direction,
                static_cast<int>(state.profile == PerformanceProfile::Classic
                    ? (button.sound == PanelSound::Classic ? 2 : 1) : panelSoundVariantCount(button.sound))));
            break;
        case Row::Latch: button.latch = !button.latch; break;
        case Row::HoldTurn:
            settings_.encoderHoldAction = settings_.encoderHoldAction == EncoderHoldAction::Off
                ? EncoderHoldAction::LivePitch : EncoderHoldAction::Off;
            break;
        case Row::Step:
            settings_.encoderStepSemitones = static_cast<uint8_t>(clampFloat(
                settings_.encoderStepSemitones + (direction < 0 ? -1 : 1), 1, 12));
            if (settings_.encoderRangeSemitones < settings_.encoderStepSemitones)
                settings_.encoderRangeSemitones = settings_.encoderStepSemitones;
            break;
        case Row::Range:
            settings_.encoderRangeSemitones = static_cast<uint8_t>(clampFloat(
                settings_.encoderRangeSemitones + (direction < 0 ? -1 : 1),
                settings_.encoderStepSemitones, 24));
            break;
        case Row::Confirm: resetConfirmed_ = !resetConfirmed_; break;
        default: break;
    }
    if (menuPage_ == MenuPage::Sound && (row == Row::Sound || row == Row::Variant)) {
        button.action = ButtonAction::PlaySound;
        applySound(state, selectedPad_, false);
        controls_.set(state); resetPickup();
    }
    changed();
}

float *PerformanceController::soundValue(ControlState &state, Row row, float &minimum, float &maximum, float &step) {
    switch (row) {
        case Row::Tune: minimum = 30.0f; maximum = 9000.0f; step = 5.0f; return &state.tuneHz;
        case Row::Rate: minimum = 0.05f; maximum = 20.0f; step = 0.05f; return &state.lfoRateHz;
        case Row::Depth: minimum = 0.0f; maximum = 2.0f; step = 0.05f; return &state.lfoDepthOctaves;
        case Row::Decay: minimum = 0.0f; maximum = 3000.0f; step = 10.0f; return &state.decayMs;
        case Row::Sweep: minimum = 0.0f; maximum = 1.0f; step = 0.05f; return &state.sweepAmount;
        case Row::SweepTime: minimum = 50.0f; maximum = 3000.0f; step = 10.0f; return &state.sweepMs;
        case Row::DrumSweep: minimum = -4.0f; maximum = 4.0f; step = 0.1f; return &state.syndrumSweepOctaves;
        case Row::Length: minimum = 20.0f; maximum = 1000.0f; step = 5.0f; return &state.lickshotLengthMs;
        case Row::Punch: minimum = 0.0f; maximum = 3.0f; step = 0.1f; return &state.lickshotPunchOctaves;
        case Row::PitchTime: minimum = 10.0f; maximum = 300.0f; step = 5.0f; return &state.lickshotPitchMs;
        case Row::Tone: minimum = 200.0f; maximum = 19000.0f; step = 100.0f; return &state.dryToneHz;
        case Row::Contour: minimum = -4.0f; maximum = 4.0f; step = 0.1f; return &state.filterContourOctaves;
        case Row::ContourTime: minimum = 10.0f; maximum = 3000.0f; step = 10.0f; return &state.filterContourMs;
        case Row::EchoLevel: minimum = 0.0f; maximum = 1.0f; step = 0.01f; return &state.echoLevel;
        case Row::EchoTime: minimum = 50.0f; maximum = 1000.0f; step = 5.0f; return &state.delayMs;
        case Row::Feedback: minimum = 0.0f; maximum = 1.05f; step = 0.01f; return &state.feedback;
        case Row::Filter: minimum = -1.0f; maximum = 1.0f; step = 0.05f; return &state.echoFilterPosition;
        case Row::Color: minimum = 0.0f; maximum = 1.0f; step = 0.05f; return &state.echoColor;
        default: return nullptr;
    }
}

void PerformanceController::addSoundValue(const ControlState &state, Row row, const char *key, const char *label, const char *unit) const {
    ControlState copy = state;
    float minimum, maximum, step;
    const float *number = soundValue(copy, row, minimum, maximum, step);
    char value[32];
    snprintf(value, sizeof(value), step < 1.0f ? "%.2f %s" : "%.0f %s", *number, unit);
    addMenuRow(row, key, label, value);
}

void PerformanceController::normalizeMenuIndex() {
    rebuildMenuView();
    if (menuRowCount_ == 0) menuIndex_ = 0;
    else if (menuIndex_ >= menuRowCount_) menuIndex_ = menuRowCount_ - 1;
}

void PerformanceController::formatTargetValue(PotTarget target,
        const ControlState &state, char *buffer, unsigned size) const {
    switch (target) {
        case PotTarget::Pitch:
            if (state.pitchZonesActive)
                snprintf(buffer, size, "%s - %.0f Hz", pitchName(state.classicPitch), state.tuneHz);
            else snprintf(buffer, size, "%.0f Hz", state.tuneHz);
            break;
        case PotTarget::Rate:
            if (state.rateZonesActive) {
                if (state.classicModulation == ClassicModulation::Manual)
                    snprintf(buffer, size, "MANUAL");
                else snprintf(buffer, size, "%s - %.2f Hz", rateName(state.classicModulation), state.lfoRateHz);
            } else snprintf(buffer, size, "%.2f Hz", state.lfoRateHz);
            break;
        case PotTarget::LfoDepth: snprintf(buffer, size, "%.2f oct", state.lfoDepthOctaves); break;
        case PotTarget::EchoLevel: snprintf(buffer, size, "%.0f%%", state.echoLevel * 100); break;
        case PotTarget::EchoTime: snprintf(buffer, size, "%.0f ms", state.delayMs); break;
        case PotTarget::EchoFeedback: snprintf(buffer, size, "%.0f%%", state.feedback * 100); break;
        case PotTarget::EchoFilter: {
            const float amount = PerformanceConfig::echoFilterAmount(state.echoFilterPosition);
            if (amount == 0) snprintf(buffer, size, "FLAT");
            else snprintf(buffer, size, "%s %.0f%%", state.echoFilterPosition < 0 ? "LP" : "HP", amount * 100);
            break;
        }
        case PotTarget::Decay: snprintf(buffer, size, "%.0f ms", state.decayMs); break;
        case PotTarget::SweepAmount: snprintf(buffer, size, "%.0f%%", state.sweepAmount * 100); break;
        case PotTarget::SweepTime: snprintf(buffer, size, "%.0f ms", state.sweepMs); break;
        case PotTarget::None: snprintf(buffer, size, "OFF"); break;
        case PotTarget::Count: snprintf(buffer, size, "N/A"); break;
    }
}

void PerformanceController::formatCurrentValue(PotTarget target, char *buffer,
                                               unsigned size) const {
    formatTargetValue(target, controls_.snapshot(), buffer, size);
}
