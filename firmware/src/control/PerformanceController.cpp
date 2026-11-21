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

float editingStep(const TargetDescriptor &descriptor, float value, int direction) {
    if (descriptor.mapping == MappingType::Logarithmic && value > 0.0f)
        return value * (direction < 0 ? 0.95f : 1.05f);
    const float step = (descriptor.absoluteMax - descriptor.absoluteMin) / 100.0f;
    return value + (direction < 0 ? -step : step);
}

void formatConfiguredValue(PotTarget target, float value,
                           char *buffer, unsigned size) {
    const TargetDescriptor *descriptor = targetDescriptor(target);
    if (!descriptor) snprintf(buffer, size, "N/A");
    else if (target == PotTarget::Pitch && value >= 1000.0f)
        snprintf(buffer, size, "%.2f kHz", value / 1000.0f);
    else snprintf(buffer, size, descriptor->valueFormat, value * descriptor->displayScale);
}

// The one sound list of the panel, for the pads and for the Trigger voice.
// The other families and variants stay reachable over the serial protocol.
struct Voice { PanelSound sound; uint8_t variant; };
constexpr Voice kVoices[] = {
    {PanelSound::Classic, 0}, {PanelSound::Classic, 1}, {PanelSound::TwoTone, 0},
    {PanelSound::Classic, 2}, {PanelSound::Ping, 0}, {PanelSound::Gunshot, 0},
};
constexpr int kVoiceCount = sizeof(kVoices) / sizeof(kVoices[0]);

// A sound outside the list (set over serial) steps to the first voice.
void stepPadVoice(ButtonConfig &button, int direction) {
    int index = -1;
    for (int i = 0; i < kVoiceCount; ++i)
        if (kVoices[i].sound == button.sound && kVoices[i].variant == button.variant) index = i;
    index = index < 0 ? 0 : wrappedStep(index, direction, kVoiceCount);
    button.sound = kVoices[index].sound;
    button.variant = kVoices[index].variant;
    if (!panelSoundSupportsLatch(button.sound)) button.latch = false;
}

// What each choice does, one footer line (about 27 characters) in the order
// of its enum or of kVoices.
const char *potTargetHelp(PotTarget target) {
    static const char *help[] = {"NON FA NULLA", "NOTA: DA GRAVE AD ACUTO",
        "QUANTO VELOCE VA SU E GIU", "AMPIEZZA DEL SU E GIU", "VOLUME DELL'ECO",
        "RITARDO TRA LE RIPETIZIONI", "QUANTE VOLTE SI RIPETE", "SX ECO CUPA, DX ECO SOTTILE",
        "QUANTO DURA LA CODA", "QUANTO SALE ALL'ATTACCO", "DURATA DELLA SALITA"};
    static_assert(sizeof(help) / sizeof(help[0]) == static_cast<unsigned>(PotTarget::Count), "help");
    const unsigned index = static_cast<unsigned>(target);
    return index < static_cast<unsigned>(PotTarget::Count) ? help[index] : nullptr;
}

const char *buttonActionHelp(ButtonAction action) {
    static const char *help[] = {"NON FA NULLA", "SUONA FINCHE LO TIENI",
        "SUONA IL SUONO DEL TASTO", "PREMI: RESTA ACCESO", "IN MANUAL ALZA IL PITCH",
        "ABBASSA IL PITCH (NO SIREN)", "SPEGNE L'ECO MENTRE PREMI", "L'ECO GIRA ALL'INFINITO",
        "ECO CHE MONTA MENTRE PREMI", "BATTI A TEMPO PIU VOLTE"};
    static_assert(sizeof(help) / sizeof(help[0]) == static_cast<unsigned>(ButtonAction::Count), "help");
    const unsigned index = static_cast<unsigned>(action);
    return index < static_cast<unsigned>(ButtonAction::Count) ? help[index] : nullptr;
}

const char *shapeHelp(LfoShape shape) {
    static const char *help[] = {"SALE PIANO, CADE DI COLPO", "SALE E SCENDE REGOLARE",
        "SALTA TRA ALTO E BASSO", "SALE E RIPARTE DA SOTTO", "SCENDE E RIPARTE DA SOPRA",
        "SALE LENTO, SCENDE VELOCE", "SALE VELOCE, SCENDE LENTO", "ALTO PER 1/4 DEL CICLO",
        "ALTO PER 3/4 DEL CICLO", "SALE COL TASTO MOD UP"};
    const unsigned index = static_cast<unsigned>(shape);
    return index < sizeof(help) / sizeof(help[0]) ? help[index] : nullptr;
}

const char *voiceHelp(PanelSound sound, unsigned variant) {
    static const char *help[] = {"IMPULSO LARGO FILTRATO", "IMPULSO STRETTO, BRILLANTE",
        "ALTERNA DUE NOTE", "UN PING A OGNI CICLO LFO", "COLPO TIPO SYNDRUM",
        "SPARO DI RUMORE"};
    static_assert(sizeof(help) / sizeof(help[0]) == kVoiceCount, "help");
    for (int i = 0; i < kVoiceCount; ++i)
        if (kVoices[i].sound == sound && kVoices[i].variant == variant) return help[i];
    return nullptr;
}

}  // namespace

PerformanceController::PerformanceController(ControlStore &controls)
    : controls_(controls) {
    ControlState state = controls_.snapshot();
    state.tuneHz = settings_.pots[0].zones[1];
    state.lfoRateHz = settings_.pots[1].zones[1];
    controls_.set(state);
}

void PerformanceController::changed() { ++revision_; }

bool PerformanceController::tapTempo(uint32_t nowUs) {
    snprintf(overlayTitle_, sizeof(overlayTitle_), "TAP TEMPO");
    overlayPot_ = 0;
    const uint32_t interval = nowUs - lastTapUs_;
    if (!tapStarted_ || interval > 2000000u) {
        tapStarted_ = true;
        lastTapUs_ = nowUs;
        tapIntervalCount_ = tapIntervalIndex_ = 0;
        snprintf(overlayValue_, sizeof(overlayValue_), "TAP AGAIN");
        changed();
        return true;
    }
    if (interval < 200000u) {
        snprintf(overlayValue_, sizeof(overlayValue_), "TOO FAST - IGNORED");
        changed();
        return false;
    }
    lastTapUs_ = nowUs;
    tapIntervals_[tapIntervalIndex_] = interval;
    tapIntervalIndex_ = (tapIntervalIndex_ + 1) % 4;
    if (tapIntervalCount_ < 4) ++tapIntervalCount_;
    uint32_t total = 0;
    for (unsigned i = 0; i < tapIntervalCount_; ++i) total += tapIntervals_[i];
    ControlState state = controls_.snapshot();
    state.tempo.bpm = 60000000.0f * tapIntervalCount_ / total;
    state.tempo.known = true;
    updateTempoTiming(state.tempo);
    controls_.set(state);
    // Tapping is for the riddim: a free echo follows the tapped tempo.
    if (!state.tempo.echoSync) {
        setTempoSync(true, true);
        snprintf(overlayTitle_, sizeof(overlayTitle_), "TAP TEMPO");
        overlayPot_ = 0;
    }
    snprintf(overlayValue_, sizeof(overlayValue_), "%.1f BPM", state.tempo.bpm);
    changed();
    return true;
}

bool PerformanceController::setTempoBpm(float bpm) {
    if (!isfinite(bpm) || bpm < 30.0f || bpm > 300.0f) return false;
    ControlState state = controls_.snapshot();
    state.tempo.bpm = bpm;
    state.tempo.known = true;
    updateTempoTiming(state.tempo);
    controls_.set(state);
    tapStarted_ = false;
    snprintf(overlayTitle_, sizeof(overlayTitle_), "TEMPO");
    overlayPot_ = 0;
    snprintf(overlayValue_, sizeof(overlayValue_), "%.1f BPM", bpm);
    changed();
    return true;
}

bool PerformanceController::setTempoSync(bool echo, bool sync) {
    ControlState state = controls_.snapshot();
    bool &enabled = echo ? state.tempo.echoSync : state.tempo.lfoSync;
    if (enabled == sync) return true;
    if (sync) {
        if (echo) state.tempo.echoMs = state.delayMs;
        else state.tempo.lfoHz = state.lfoRateHz;
    } else {
        if (echo) state.delayMs = state.tempo.echoMs;
        else state.lfoRateHz = state.tempo.lfoHz;
    }
    enabled = sync;
    updateTempoTiming(state.tempo);
    controls_.set(state);
    overlayTitle_[0] = overlayValue_[0] = '\0';
    changed();
    return true;
}

bool PerformanceController::setTempoDivision(bool echo, TempoDivision division) {
    if (division >= TempoDivision::Count) return false;
    ControlState state = controls_.snapshot();
    (echo ? state.tempo.echoDivision : state.tempo.lfoDivision) = division;
    updateTempoTiming(state.tempo);
    controls_.set(state);
    notifyExternalChange();
    return true;
}

void PerformanceController::formatTempoStatus(char *buffer, unsigned size) const {
    const auto state = controls_.snapshot();
    const auto tempo = state.tempo;
    if (!tempo.echoSync && !tempo.lfoSync) { buffer[0] = '\0'; return; }
    char bpm[16];
    if (tempo.known) snprintf(bpm, sizeof(bpm), "%.1f BPM", tempo.bpm);
    else snprintf(bpm, sizeof(bpm), "TAP AGAIN");
    snprintf(buffer, size, "%s E:%s%s L:%s%s", bpm,
        tempo.echoSync ? tempoDivisionName(tempo.echoDivision) : "FREE",
        tempo.echoSync && !tempoEchoAvailable(tempo) ? " N/A" : "",
        tempo.lfoSync ? tempoDivisionName(tempo.lfoDivision) : "FREE",
        tempo.lfoSync && (!tempoLfoAvailable(tempo) || !tempoLfoActive(state)) ? " N/A" : "");
}

void PerformanceController::formatTempoJson(char *buffer, unsigned size) const {
    const auto state = controls_.snapshot();
    const auto &tempo = state.tempo;
    char status[64], echo[40], lfo[40];
    formatTempoStatus(status, sizeof(status));
    formatTargetValue(PotTarget::EchoTime, state, echo, sizeof(echo));
    formatTargetValue(PotTarget::Rate, state, lfo, sizeof(lfo));
    snprintf(buffer, size,
        "{\"bpm\":%.4f,\"known\":%s,\"echoSync\":%s,\"lfoSync\":%s,"
        "\"echoDivision\":\"%s\",\"lfoDivision\":\"%s\","
        "\"echoAvailable\":%s,\"lfoAvailable\":%s,\"lfoActive\":%s,"
        "\"echoMs\":%.4f,\"lfoHz\":%.4f,\"status\":\"%s\","
        "\"echoText\":\"%s\",\"lfoText\":\"%s\"}",
        tempo.bpm, tempo.known ? "true" : "false",
        tempo.echoSync ? "true" : "false", tempo.lfoSync ? "true" : "false",
        tempoDivisionName(tempo.echoDivision), tempoDivisionName(tempo.lfoDivision),
        tempoEchoAvailable(tempo) ? "true" : "false",
        tempoLfoAvailable(tempo) ? "true" : "false", tempoLfoActive(state) ? "true" : "false",
        tempo.echoSync ? tempo.echoMs : state.delayMs,
        tempo.lfoSync ? tempo.lfoHz : state.lfoRateHz, status, echo, lfo);
}

void PerformanceController::setPersistenceStatus(const char *status) {
    snprintf(persistenceStatus_, sizeof(persistenceStatus_), "%s",
             status == nullptr ? "" : status);
    changed();
}

void PerformanceController::syncZoneValues(ControlState &state) const {
    updateZoneFlags(state);
    if (state.pitchZonesActive)
        state.tuneHz = classicPitchHz(static_cast<unsigned>(state.classicPitch));
    const unsigned rate = static_cast<unsigned>(state.classicModulation);
    if (state.rateZonesActive && rate < 3) state.lfoRateHz = classicRateHz(rate);
}

bool PerformanceController::parseSound(const String &text, PanelSound &sound) {
    if (text == "SIREN" || text == "CLASSIC") sound = PanelSound::Classic;
    else if (text == "TWO_TONE") sound = PanelSound::TwoTone;
    else if (text == "LICKSHOT") sound = PanelSound::Lickshot;
    else if (text == "PING") sound = PanelSound::Ping;
    else if (text == "GUNSHOT") sound = PanelSound::Gunshot;
    else if (text == "TEST_TONE") sound = PanelSound::TestTone;
    else return false;
    return true;
}

void PerformanceController::applyVoice(ControlState &state, PanelSound sound,
                                       unsigned variant) {
    // Sustained voices use the short gate; one-shots keep their own tail.
    state.decayMs = 0.0f;
    switch (sound) {
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
            state.decayMs = PerformanceConfig::kExtendedDecayMs;
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
        case PanelSound::Gunshot:
            state.mode = SirenMode::Gunshot;
            state.decayMs = PerformanceConfig::kExtendedDecayMs;
            break;
        case PanelSound::TestTone:
            state.mode = SirenMode::TestTone;
            state.lfoShape = LfoShape::Classic;
            state.modulationEnabled = true;
            break;
        case PanelSound::Count: break;
    }
}

void PerformanceController::stepVoice(ControlState &state, int direction) {
    voiceIndex_ = static_cast<uint8_t>(wrappedStep(voiceIndex_, direction, kVoiceCount));
    applyVoice(state, kVoices[voiceIndex_].sound, kVoices[voiceIndex_].variant);
    triggerVoiceApplied_ = true;
    reapplyRatePots(state);
}

// The one-shots (Ping, Gunshot) have no LFO: there a Rate pot sets the Decay,
// the length of the hit, instead of doing nothing.
PotConfig PerformanceController::effectivePot(const PotConfig &config,
                                              const ControlState &state) const {
    const bool oneShot = state.mode == SirenMode::Syndrum || state.mode == SirenMode::Gunshot;
    if (config.target != PotTarget::Rate || !oneShot) return config;
    // Never shorter than the one-shot default, wherever the Rate pot was left.
    PotConfig decay = continuousPot(PotTarget::Decay);
    decay.minimum = PerformanceConfig::kExtendedDecayMs;
    return decay;
}

PotConfig PerformanceController::potConfig(unsigned potNumber) const {
    if (potNumber < 1 || potNumber > 6) return PotConfig();
    return effectivePot(settings_.pots[potNumber - 1], controls_.snapshot());
}

// After a change of sound a Rate pot may have changed function: the knob
// still owns its value, so its position is applied to the new one.
void PerformanceController::reapplyRatePots(ControlState &state) {
    for (unsigned index = 0; index < 6; ++index)
        if (potReported_[index] && settings_.pots[index].target == PotTarget::Rate)
            applyPotValue(state, effectivePot(settings_.pots[index], state), potPosition_[index]);
}

const char *PerformanceController::voiceName() const {
    // Named from the live sound, so direct mode or shape commands show too.
    const ControlState state = controls_.snapshot();
    const char *variant = "";
    if (state.mode == SirenMode::Square && !sirenShape(state.lfoShape))
        variant = state.lfoShape == LfoShape::Triangle ? "TRIANGLE"
                : state.lfoShape == LfoShape::Square ? "SQUARE" : "LFO";
    else if (state.mode == SirenMode::Lickshot)
        variant = state.lickshotArticulation == LickshotArticulation::Legacy
            ? panelSoundVariantName(PanelSound::Lickshot, state.lfoShape == LfoShape::AsymDown ? 1 : 0)
            : panelSoundVariantName(PanelSound::Lickshot,
                                    static_cast<unsigned>(state.lickshotArticulation) + 1);
    else if (state.mode == SirenMode::Syndrum && state.syndrumWave != SyndrumWave::Sine)
        variant = panelSoundVariantName(PanelSound::Ping, static_cast<unsigned>(state.syndrumWave));
    // Named as the keys and the menu name them.
    const char *family = state.mode == SirenMode::Square ? "TWO TONE"
                       : state.mode == SirenMode::Syndrum ? "PING" : modeName(state.mode);
    snprintf(voiceName_, sizeof(voiceName_), variant[0] ? "%s %s" : "%s", family, variant);
    return voiceName_;
}

bool PerformanceController::selectedIsPad() const {
    return settings_.buttons[selectedPad_].action == ButtonAction::PlaySound;
}

void PerformanceController::applySound(ControlState &state, unsigned index,
                                       bool activate) {
    const bool samePad = currentPad_ == index;
    const SoundPatch &patch = settings_.patches[index];
    const bool restore = patch.valid &&
        (!samePad || appliedSound_ == PanelSound::Count);
    if (restore) {
        auto &button = settings_.buttons[index];
        button.sound = patch.sound; button.variant = patch.variant;
    }
    if (activate) soundLatchArmed_ = true;
    triggerVoiceApplied_ = false;
    currentPad_ = index;
    const ControlState previous = state;
    const ButtonConfig config = settings_.buttons[index];
    const unsigned variant = config.variant;
    state.modUp = state.modDown = false;
    applyVoice(state, config.sound, variant);
    if (samePad && appliedSound_ == config.sound && appliedVariant_ == variant) {
        state.lfoShape = previous.lfoShape;
        state.modulationEnabled = previous.modulationEnabled;
        state.retrigger = previous.retrigger;
        state.decayMs = previous.decayMs;
        state.syndrumSweepOctaves = previous.syndrumSweepOctaves;
    }
    if (restore) static_cast<SoundState &>(state) = patch.state;
    // A saved patch keeps its own values; any other new sound takes the pot's.
    else if (!samePad || appliedSound_ != config.sound || appliedVariant_ != variant)
        reapplyRatePots(state);
    appliedSound_ = config.sound;
    appliedVariant_ = variant;
    if (activate) recordActivation(state);
}

bool PerformanceController::savePad(unsigned padNumber) {
    if (padNumber < 1 || padNumber > 4) return false;
    const ControlState state = controls_.snapshot();
    const auto &button = settings_.buttons[padNumber - 1];
    if (button.action != ButtonAction::PlaySound) return false;
    SoundPatch patch;
    patch.valid = 1; patch.sound = button.sound; patch.variant = button.variant;
    switch (state.mode) {
        case SirenMode::Sine1: patch.sound = PanelSound::Classic; patch.variant = 0; break;
        case SirenMode::Sine2: patch.sound = PanelSound::Classic; patch.variant = 1; break;
        case SirenMode::Sonar: patch.sound = PanelSound::Classic; patch.variant = 2; break;
        case SirenMode::Saw: patch.sound = PanelSound::Classic; patch.variant = 3; break;
        case SirenMode::Triangle: patch.sound = PanelSound::Classic; patch.variant = 4; break;
        case SirenMode::Square: patch.sound = PanelSound::TwoTone; patch.variant = state.lfoShape == LfoShape::Classic ? 0 : state.lfoShape == LfoShape::Triangle ? 1 : 2; break;
        case SirenMode::Lickshot: patch.sound = PanelSound::Lickshot; patch.variant = state.lickshotArticulation == LickshotArticulation::Legacy ? (state.lfoShape == LfoShape::AsymDown ? 1 : 0) : static_cast<unsigned>(state.lickshotArticulation) + 1; break;
        case SirenMode::Syndrum: patch.sound = PanelSound::Ping; patch.variant = static_cast<unsigned>(state.syndrumWave); break;
        case SirenMode::Gunshot: patch.sound = PanelSound::Gunshot; patch.variant = 0; break;
        case SirenMode::TestTone: patch.sound = PanelSound::TestTone; patch.variant = 0; break;
        default: return false;
    }
    patch.state = state;
    if (!validateSoundPatch(patch)) return false;
    settings_.patches[padNumber - 1] = patch;
    settings_.buttons[padNumber - 1].sound = patch.sound;
    settings_.buttons[padNumber - 1].variant = patch.variant;
    if (currentPad_ == padNumber - 1) { appliedSound_ = patch.sound; appliedVariant_ = patch.variant; }
    ++settingsRevision_; changed(); return true;
}

bool PerformanceController::recallPad(unsigned padNumber) {
    if (padNumber < 1 || padNumber > 4) return false;
    const auto &patch = settings_.patches[padNumber - 1];
    ControlState state = controls_.snapshot();
    if (!patch.valid || !validateSoundPatch(patch)) return false;
    auto &button = settings_.buttons[padNumber - 1];
    if (button.action != ButtonAction::PlaySound) return false;
    button.sound = patch.sound; button.variant = patch.variant;
    static_cast<SoundState &>(state) = patch.state;
    currentPad_ = selectedPad_ = padNumber - 1;
    appliedSound_ = patch.sound; appliedVariant_ = patch.variant;
    controls_.set(state); changed(); return true;
}

bool PerformanceController::importPatch(unsigned padNumber, const SoundPatch &patch) {
    if (padNumber < 1 || padNumber > 4 || !validateSoundPatch(patch)) return rejectSettingsChange();
    PanelSettings candidate = settingsTransaction_ ? stagedSettings_ : settings_;
    candidate.patches[padNumber - 1] = patch;
    acceptSettingsCandidate(candidate); return true;
}

bool PerformanceController::patchDirty(unsigned padNumber) const {
    if (padNumber < 1 || padNumber > 4) return false;
    const auto &saved = settings_.patches[padNumber - 1];
    if (!saved.valid) return true;
    if (currentPad_ != padNumber - 1) return false;
    SoundPatch live = saved;
    live.state = controls_.snapshot();
    const auto &button = settings_.buttons[padNumber - 1];
    live.sound = button.sound; live.variant = button.variant;
    for (unsigned i = 0; i < kSoundPatchValueCount; ++i)
        if (soundPatchValue(live, i) != soundPatchValue(saved, i)) return true;
    return false;
}

bool PerformanceController::isRelevantButton(unsigned index) const {
    const ButtonAction action = settings_.buttons[index].action;
    return action == ButtonAction::Trigger || action == ButtonAction::PlaySound;
}

bool PerformanceController::anyRelevantButtonDown() const {
    for (unsigned index = 0; index < 4; ++index)
        if (padDown_[index] && !consumed_[index] && isRelevantButton(index)) return true;
    return false;
}

bool PerformanceController::modUpTaps() const {
    return effectiveLfoShape(controls_.snapshot()) != LfoShape::Manual;
}

bool PerformanceController::buttonDown(unsigned buttonNumber) const {
    return buttonNumber >= 1 && buttonNumber <= 4 && padDown_[buttonNumber - 1];
}

float PerformanceController::potValueNormalized(unsigned potNumber) const {
    if (potNumber < 1 || potNumber > 6) return 0.0f;
    const ControlState state = controls_.snapshot();
    return clampFloat(currentTargetNormalized(
        effectivePot(settings_.pots[potNumber - 1], state), state), 0.0f, 1.0f);
}

// The lever bends tonal voices only: the one-shots have no bend.
bool PerformanceController::pitchAvailable(const ControlState &state) const {
    return state.mode != SirenMode::Gunshot && state.mode != SirenMode::Syndrum;
}

void PerformanceController::setLock(bool on, int button) {
    holdToggle_ = on;
    lockedButton_ = on ? button : -1;
    snprintf(overlayTitle_, sizeof(overlayTitle_), "HOLD");
    snprintf(overlayValue_, sizeof(overlayValue_), on ? "BLOCCATO" : "SBLOCCATO");
    overlayPot_ = 0;
}

unsigned PerformanceController::lockedButton() const {
    return holdToggle_ && lockedButton_ >= 0 ? static_cast<unsigned>(lockedButton_) + 1 : 0;
}

int PerformanceController::latestPressedRelevantButton() const {
    int latest = -1;
    unsigned order = 0;
    for (unsigned index = 0; index < 4; ++index)
        if (padDown_[index] && !consumed_[index] && isRelevantButton(index) &&
            pressOrder_[index] >= order) {
            order = pressOrder_[index];
            latest = static_cast<int>(index);
        }
    return latest;
}

void PerformanceController::updateGate(ControlState &state) {
    const auto &buttons = settings_.buttons;
    bool trigger = false;
    bool modUp = false;
    bool modDown = false;
    bool echoCut = false;
    bool echoSpin = false;
    for (unsigned index = 0; index < 4; ++index) {
        if (!padDown_[index] || consumed_[index]) continue;
        switch (buttons[index].action) {
            case ButtonAction::Trigger: trigger = true; break;
            case ButtonAction::PlaySound: trigger |= activePad_ >= 0; break;
            case ButtonAction::ModUp: modUp |= !modUpTapping_[index]; break;
            case ButtonAction::ModDown: modDown = true; break;
            case ButtonAction::EchoCut: echoCut = true; break;
            case ButtonAction::EchoSpin: echoSpin = true; break;
            default: break;
        }
    }
    modUp |= leverPosition_ > 0;
    modDown |= leverPosition_ < 0;
    state.trigger = trigger || remoteGates_.trigger;
    state.modUp = modUp || remoteGates_.modUp;
    state.modDown = modDown || remoteGates_.modDown;
    state.echoCut = echoCut || remoteGates_.echoCut;
    state.echoSpin = echoSpin || remoteGates_.echoSpin;
    const unsigned soundIndex = activePad_ >= 0
        ? static_cast<unsigned>(activePad_) : currentPad_;
    const ButtonConfig &button = buttons[soundIndex];
    const bool soundLatch = soundLatchArmed_ && button.action == ButtonAction::PlaySound &&
        button.latch && panelSoundSupportsLatch(button.sound);
    state.hold = holdToggle_ || soundLatch || remoteGates_.hold;
}

int PerformanceController::latestPressedSoundButton() const {
    int latest = -1;
    unsigned order = 0;
    const auto &buttons = settings_.buttons;
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
    padDown_[index] = pressed;
    ControlState state = controls_.snapshot();
    ButtonConfig &button = settings_.buttons[index];
    // A quick tap can press and release between display refreshes; so can
    // the Hold gesture, whose message outlives the release of its key.
    const bool lockMessage = strcmp(overlayTitle_, "HOLD") == 0;
    if (pressed || (button.action != ButtonAction::TapTempo && !modUpTapping_[index] &&
                    !lockMessage)) {
        overlayTitle_[0] = '\0';
        overlayValue_[0] = '\0';
    }

    // With the menu open, the key under the encoder steps back one level
    // instead of doing its own action.
    if (pressed && displayMode_ == DisplayMode::Menu && !encoderPressed_ &&
        padNumber == kMenuBackButton) {
        consumed_[index] = true;
        menuBack();
        state = controls_.snapshot();
    // On the controls map any other key selects its own cell instead of acting.
    } else if (pressed && displayMode_ == DisplayMode::Menu && menuPage_ == MenuPage::Setup) {
        consumed_[index] = true;
        if (!menuEditing_) menuIndex_ = static_cast<uint8_t>(6 + index);
    // A locked key unlocks with one press of its own, and does nothing else.
    } else if (pressed && displayMode_ == DisplayMode::Home && !encoderPressed_ &&
               lockedButton() == padNumber) {
        consumed_[index] = true;
        setLock(false, -1);
    } else if (pressed) {
        consumed_[index] = false;
        pressOrder_[index] = nextOrder_++;
        // The Hold gesture in the other order: encoder first, then the key.
        if (encoderPressed_ && displayMode_ == DisplayMode::Home && isRelevantButton(index)) {
            setLock(!holdToggle_, static_cast<int>(index));
            modifierUsed_ = true;
        // Another playing key takes the lock over with its sound.
        } else if (lockedButton() != 0 && isRelevantButton(index)) {
            lockedButton_ = static_cast<int>(index);
        }
        if (button.action == ButtonAction::Hold) {
            holdToggle_ = !holdToggle_;
            lockedButton_ = -1;  // A HOLD key is red itself: no lock on a sound key.
        }
        else if (button.action == ButtonAction::Trigger) {
            // The Trigger always plays its own voice, also after a sound pad.
            if (!triggerVoiceApplied_) {
                applyVoice(state, kVoices[voiceIndex_].sound, kVoices[voiceIndex_].variant);
                triggerVoiceApplied_ = true;
                reapplyRatePots(state);
            }
            selectedPad_ = index;
            recordActivation(state);
        }
        else if (button.action == ButtonAction::EchoLoop) state.echoLoop = !state.echoLoop;
        else if (button.action == ButtonAction::TapTempo) {
            tapTempo();
            state = controls_.snapshot();
        } else if (button.action == ButtonAction::ModUp && modUpTaps()) {
            modUpTapping_[index] = true;
            tapTempo();
            state = controls_.snapshot();
        } else if (button.action == ButtonAction::PlaySound) {
            selectedPad_ = index;
            activePad_ = static_cast<int>(index);
            applySound(state, index, true);
        }
    } else {
        pressOrder_[index] = 0;
        modUpTapping_[index] = false;
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
    // The lever springs back to the centre when no playing key is held.
    if (!anyRelevantButtonDown()) leverPosition_ = 0;
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
    if ((config.target == PotTarget::EchoTime && state.tempo.echoSync) ||
        (config.target == PotTarget::Rate && state.tempo.lfoSync)) {
        auto &division = config.target == PotTarget::EchoTime
            ? state.tempo.echoDivision : state.tempo.lfoDivision;
        division = static_cast<TempoDivision>(PerformanceConfig::selectorZone(
            position, static_cast<int>(division), 4));
        updateTempoTiming(state.tempo);
        return;
    }
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
        return;
    }
    if (config.mode == PotMode::Zones && config.target == PotTarget::Rate) {
        const int count = rateZoneCount(state);
        int current = static_cast<int>(state.classicModulation);
        if (current >= count) current = count - 1;
        const int zone = PerformanceConfig::selectorZone(position, current, count);
        state.classicModulation = static_cast<ClassicModulation>(zone);
        if (zone < 3) state.lfoRateHz = config.zones[zone];
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
    if ((config.target == PotTarget::EchoTime && state.tempo.echoSync) ||
        (config.target == PotTarget::Rate && state.tempo.lfoSync)) {
        const auto division = config.target == PotTarget::EchoTime
            ? state.tempo.echoDivision : state.tempo.lfoDivision;
        return (static_cast<unsigned>(division) + 0.5f) / 4;
    }
    if (config.mode == PotMode::Zones) {
        const unsigned zone = config.target == PotTarget::Pitch
            ? static_cast<unsigned>(state.classicPitch)
            : static_cast<unsigned>(state.classicModulation);
        const unsigned count = config.target == PotTarget::Pitch ? 3 : rateZoneCount(state);
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
    const PotConfig config = effectivePot(settings_.pots[index], state);
    // Every reading applies at once: the knob always owns its value.
    const bool firstReading = !potReported_[index];
    potReported_[index] = true;
    potPosition_[index] = position;
    snprintf(overlayTitle_, sizeof(overlayTitle_), "%s", potTargetName(config.target));
    overlayPot_ = potNumber;
    applyPotValue(state, config, position);
    formatTargetValue(config.target, state, overlayValue_, sizeof(overlayValue_));
    if (config.mode == PotMode::Continuous && config.target == PotTarget::Pitch)
        snprintf(overlayValue_, sizeof(overlayValue_), "%.0f Hz", state.tuneHz);
    if (config.mode == PotMode::Continuous && config.target == PotTarget::Rate && !state.tempo.lfoSync)
        snprintf(overlayValue_, sizeof(overlayValue_), "%.2f Hz", state.lfoRateHz);
    const bool transient = state.mode == SirenMode::Syndrum ||
                           state.mode == SirenMode::Gunshot;
    const bool inactive = transient && (config.target == PotTarget::LfoDepth ||
                                        config.target == PotTarget::SweepAmount ||
                                        config.target == PotTarget::SweepTime);
    if (inactive) snprintf(overlayValue_, sizeof(overlayValue_), "STORED - N/A");
    controls_.set(state);
    // The power-on scan is not a gesture: no overlay for it.
    if (firstReading) overlayTitle_[0] = overlayValue_[0] = '\0';
    // On the controls map a moved pot selects its own cell.
    else if (displayMode_ == DisplayMode::Menu && menuPage_ == MenuPage::Setup && !menuEditing_)
        menuIndex_ = static_cast<uint8_t>(index);
    changed();
    return true;
}

void PerformanceController::setMaster(float position) {
    if (!isfinite(position)) return;
    position = clampFloat(position, 0.0f, 1.0f);
    ControlState state = controls_.snapshot();
    state.masterVolume = position;
    controls_.set(state);
    // As for the pots, the power-on reading is not a gesture.
    if (masterReported_) {
        snprintf(overlayTitle_, sizeof(overlayTitle_), "MASTER");
        snprintf(overlayValue_, sizeof(overlayValue_), "%d%%",
                 static_cast<int>(lroundf(position * 100.0f)));
        overlayPot_ = 0;
    }
    masterReported_ = true;
    changed();
}

void PerformanceController::setEncoderPressed(bool pressed, bool cancelClick) {
    if (encoderPressed_ == pressed) return;
    if (cancelClick && !pressed) modifierUsed_ = true;
    // The release keeps what the press showed (the Hold gesture message).
    if (pressed) {
        overlayTitle_[0] = '\0';
        overlayValue_[0] = '\0';
    }
    encoderPressed_ = pressed;
    if (pressed) {
        modifierUsed_ = false;
        // Hold gesture: a playing key together with the encoder push.
        if (displayMode_ == DisplayMode::Home && anyRelevantButtonDown()) {
            setLock(!holdToggle_, latestPressedRelevantButton());
            modifierUsed_ = true;
        }
    } else if (displayMode_ == DisplayMode::Menu) {
        if (!modifierUsed_) pressMenuItem();
    } else if (!modifierUsed_ && holdToggle_) {
        setLock(false, -1);  // While locked, the first click only unlocks.
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
        if (anyRelevantButtonDown()) {
            // A three-position lever, DOWN / CENTRE / UP, one detent per step:
            // Mod Up / Mod Down in MANUAL, a one-octave bend with other shapes.
            modifierUsed_ = true;
            snprintf(overlayTitle_, sizeof(overlayTitle_), "LEVA");
            overlayPot_ = 0;
            if (!pitchAvailable(state)) {
                snprintf(overlayValue_, sizeof(overlayValue_), "N/A");
            } else {
                leverPosition_ = static_cast<int8_t>(clampFloat(
                    static_cast<float>(leverPosition_ + (direction < 0 ? -1 : 1)), -1.0f, 1.0f));
                const bool manual = effectiveLfoShape(state) == LfoShape::Manual;
                snprintf(overlayValue_, sizeof(overlayValue_), "%s",
                    leverPosition_ == 0 ? "CENTRO"
                    : manual ? (leverPosition_ > 0 ? "MOD UP" : "MOD DOWN")
                             : (leverPosition_ > 0 ? "+1 OTTAVA" : "-1 OTTAVA"));
                updateGate(state);
                controls_.set(state);
            }
            changed();
            return;
        }
        overlayTitle_[0] = '\0';
        overlayValue_[0] = '\0';
        // Free rotation steps the sound of the last pressed pad, shown on its
        // key and in the banner; with a Trigger selected, the Trigger voice.
        if (selectedIsPad()) {
            stepPadVoice(settings_.buttons[selectedPad_], direction);
            if (currentPad_ == selectedPad_) applySound(state, selectedPad_, false);
            controls_.set(state);
            commitSettingsChange();
            return;
        }
        stepVoice(state, direction);
        controls_.set(state);
        changed();
        return;
    }
    overlayTitle_[0] = '\0';
    overlayValue_[0] = '\0';
    normalizeMenuIndex();
    rebuildMenuView();
    if (menuEditing_) editMenuItem(menuRows_[menuIndex_].row, direction);
    else {
        menuIndex_ = static_cast<uint8_t>(wrappedStep(
            menuIndex_, direction, static_cast<int>(menuRowCount_)));
        changed();
    }
}

void PerformanceController::clearRemoteGates() {
    remoteGates_ = {};
    ControlState state = controls_.snapshot();
    updateGate(state);
    controls_.set(state);
    changed();
}

void PerformanceController::resetMomentaries() {
    soundLatchArmed_ = false;
    overlayTitle_[0] = '\0';
    overlayValue_[0] = '\0';
    for (unsigned index = 0; index < 4; ++index) {
        padDown_[index] = false;
        consumed_[index] = false;
        pressOrder_[index] = 0;
        modUpTapping_[index] = false;
    }
    activePad_ = -1;
    encoderPressed_ = false;
    modifierUsed_ = false;
    leverPosition_ = 0;
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
    ButtonConfig &button = settings_.buttons[selectedPad_];
    if (button.action != ButtonAction::PlaySound) return;
    button.sound = static_cast<PanelSound>(wrappedStep(
        static_cast<int>(button.sound), direction,
        static_cast<int>(PanelSound::Count)));
    button.variant = 0;
    if (!panelSoundSupportsLatch(button.sound)) button.latch = false;
    ControlState state = controls_.snapshot();
    if (currentPad_ == selectedPad_)
        applySound(state, selectedPad_, false);
    controls_.set(state);
    commitSettingsChange();
}

void PerformanceController::stepVariant(int direction) {
    ButtonConfig &button = settings_.buttons[selectedPad_];
    if (button.action != ButtonAction::PlaySound) return;
    button.variant = static_cast<uint8_t>(wrappedStep(button.variant, direction,
        static_cast<int>(panelSoundVariantCount(button.sound))));
    ControlState state = controls_.snapshot();
    if (currentPad_ == selectedPad_)
        applySound(state, selectedPad_, false);
    controls_.set(state);
    commitSettingsChange();
}

void PerformanceController::toggleSelectedHold() {
    ButtonConfig &button = settings_.buttons[selectedPad_];
    if (button.action != ButtonAction::PlaySound ||
        !panelSoundSupportsLatch(button.sound)) return;
    button.latch = !button.latch;
    ControlState state = controls_.snapshot();
    if (currentPad_ == selectedPad_) {
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
    return configureButtonSound(padNumber,
                                parsed, variant, hold);
}

const char *PerformanceController::padSoundName(unsigned padNumber) const {
    if (padNumber < 1 || padNumber > 4) return "UNKNOWN";
    const ButtonConfig &button = settings_.buttons[padNumber - 1];
    return button.action == ButtonAction::PlaySound
        ? panelSoundName(button.sound) : buttonActionName(button.action);
}

const char *PerformanceController::padVariantName(unsigned padNumber) const {
    if (padNumber < 1 || padNumber > 4) return "UNKNOWN";
    const ButtonConfig &button = settings_.buttons[padNumber - 1];
    return button.action == ButtonAction::PlaySound
        ? panelSoundVariantName(button.sound, button.variant) : "N/A";
}

const char *PerformanceController::padVoiceName(unsigned padNumber) const {
    if (padNumber < 1 || padNumber > 4) return "UNKNOWN";
    const ButtonConfig &button = settings_.buttons[padNumber - 1];
    // The siren family is named by its variant, the others by the family.
    return button.sound == PanelSound::Classic
        ? panelSoundVariantName(button.sound, button.variant) : panelSoundName(button.sound);
}

unsigned PerformanceController::padVariant(unsigned padNumber) const {
    return padNumber >= 1 && padNumber <= 4
        ? settings_.buttons[padNumber - 1].variant : 0;
}

bool PerformanceController::padHold(unsigned padNumber) const {
    return padNumber >= 1 && padNumber <= 4 &&
        settings_.buttons[padNumber - 1].latch;
}

bool PerformanceController::padHoldSupported(unsigned padNumber) const {
    if (padNumber < 1 || padNumber > 4) return false;
    const ButtonConfig &button = settings_.buttons[padNumber - 1];
    return button.action == ButtonAction::PlaySound &&
           panelSoundSupportsLatch(button.sound);
}

const char *PerformanceController::buttonActionNameFor(unsigned buttonNumber) const {
    if (buttonNumber < 1 || buttonNumber > 4) return "UNKNOWN";
    return buttonActionName(settings_.buttons[buttonNumber - 1].action);
}

bool PerformanceController::configurePotTarget(unsigned potNumber, PotTarget target) {
    if (potNumber < 1 || potNumber > 6 || target >= PotTarget::Count) return rejectSettingsChange();
    PanelSettings candidate = settingsTransaction_ ? stagedSettings_ : settings_;
    candidate.pots[potNumber - 1] = continuousPot(target);
    if (!validatePanelSettings(candidate)) return rejectSettingsChange();
    acceptSettingsCandidate(candidate);
    return true;
}

bool PerformanceController::configurePotMode(unsigned potNumber, PotMode mode) {
    if (potNumber < 1 || potNumber > 6) return rejectSettingsChange();
    PanelSettings candidate = settingsTransaction_ ? stagedSettings_ : settings_;
    PotConfig &pot = candidate.pots[potNumber - 1];
    pot.mode = mode;
    if (mode == PotMode::Zones) {
        if (pot.target == PotTarget::Pitch) {
            for (unsigned i = 0; i < 3; ++i) pot.zones[i] = PerformanceConfig::kClassicPitchHz[i];
        } else if (pot.target == PotTarget::Rate) {
            for (unsigned i = 0; i < 3; ++i) pot.zones[i] = PerformanceConfig::kClassicRateHz[i];
        }
    }
    if (!validatePanelSettings(candidate)) return rejectSettingsChange();
    acceptSettingsCandidate(candidate);
    return true;
}

bool PerformanceController::configurePotRange(unsigned potNumber, float minimum, float maximum) {
    if (potNumber < 1 || potNumber > 6) return rejectSettingsChange();
    PanelSettings candidate = settingsTransaction_ ? stagedSettings_ : settings_;
    PotConfig &pot = candidate.pots[potNumber - 1];
    pot.minimum = minimum;
    pot.maximum = maximum;
    if (!validatePanelSettings(candidate)) return rejectSettingsChange();
    acceptSettingsCandidate(candidate);
    return true;
}

bool PerformanceController::configurePotZones(unsigned potNumber, float low, float middle, float high) {
    if (potNumber < 1 || potNumber > 6) return rejectSettingsChange();
    PanelSettings candidate = settingsTransaction_ ? stagedSettings_ : settings_;
    PotConfig &pot = candidate.pots[potNumber - 1];
    pot.zones[0] = low; pot.zones[1] = middle; pot.zones[2] = high;
    if (!validatePanelSettings(candidate)) return rejectSettingsChange();
    acceptSettingsCandidate(candidate);
    return true;
}

bool PerformanceController::configureButtonAction(unsigned buttonNumber, ButtonAction action) {
    if (buttonNumber < 1 || buttonNumber > 4 || action >= ButtonAction::Count) return rejectSettingsChange();
    PanelSettings candidate = settingsTransaction_ ? stagedSettings_ : settings_;
    ButtonConfig &button = candidate.buttons[buttonNumber - 1];
    button.action = action;
    if (action == ButtonAction::PlaySound && button.sound >= PanelSound::Count)
        button.sound = PanelSound::Classic;
    if (!validatePanelSettings(candidate)) return rejectSettingsChange();
    acceptSettingsCandidate(candidate);
    return true;
}

bool PerformanceController::configureButtonSound(unsigned buttonNumber, PanelSound sound, unsigned variant, bool latch) {
    if (buttonNumber < 1 || buttonNumber > 4 ||
        variant >= panelSoundVariantCount(sound)) return rejectSettingsChange();
    PanelSettings candidate = settingsTransaction_ ? stagedSettings_ : settings_;
    ButtonConfig &button = candidate.buttons[buttonNumber - 1];
    if (button.action != ButtonAction::PlaySound) return rejectSettingsChange();
    button.sound = sound;
    button.variant = static_cast<uint8_t>(variant);
    button.latch = latch;
    if (!validatePanelSettings(candidate)) return rejectSettingsChange();
    acceptSettingsCandidate(candidate);
    ControlState state = controls_.snapshot();
    if (!settingsTransaction_ && currentPad_ == buttonNumber - 1) {
        applySound(state, buttonNumber - 1, false);
        controls_.set(state);
    }
    return true;
}

bool PerformanceController::applySettings(const PanelSettings &settings) {
    if (!validatePanelSettings(settings)) return false;
    settingsTransaction_ = false;
    settingsTransactionInvalid_ = false;
    settings_ = settings;
    ControlState state = controls_.snapshot();
    syncZoneValues(state);
    controls_.set(state);
    commitSettingsChange();
    return true;
}

void PerformanceController::resetDefaults() {
    settingsTransaction_ = false;
    settingsTransactionInvalid_ = false;
    settings_ = factoryPanelSettings();
    holdToggle_ = false;
    lockedButton_ = -1;
    ControlState state = controls_.snapshot();
    state.tempo = TempoState{};
    tapStarted_ = false;
    syncZoneValues(state);
    controls_.set(state);
    commitSettingsChange();
}

void PerformanceController::commitSettingsChange() {
    // A HOLD latch (not a key lock) ends when no key can release it any more.
    if (holdToggle_ && lockedButton_ < 0) {
        bool holdKey = false;
        for (const ButtonConfig &button : settings_.buttons)
            holdKey |= button.action == ButtonAction::Hold;
        holdToggle_ = holdKey;
    }
    ControlState state = controls_.snapshot();
    updateGate(state);
    updateZoneFlags(state);
    controls_.set(state);
    ++settingsRevision_;
    changed();
}

void PerformanceController::updateZoneFlags(ControlState &state) const {
    state.pitchZonesActive = false;
    state.rateZonesActive = false;
    for (const PotConfig &pot : settings_.pots) {
        state.pitchZonesActive |= pot.target == PotTarget::Pitch &&
                                  pot.mode == PotMode::Zones;
        state.rateZonesActive |= pot.target == PotTarget::Rate &&
                                 pot.mode == PotMode::Zones;
    }
}

void PerformanceController::acceptSettingsCandidate(
        const PanelSettings &candidate) {
    if (settingsTransaction_) {
        stagedSettings_ = candidate;
        return;
    }
    settings_ = candidate;
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
    ControlState state = controls_.snapshot();
    syncZoneValues(state);
    controls_.set(state);
    commitSettingsChange();
    return true;
}

void PerformanceController::cancelSettingsTransaction() {
    settingsTransaction_ = false;
    settingsTransactionInvalid_ = false;
    stagedSettings_ = settings_;
}

void PerformanceController::notifyExternalChange() {
    overlayTitle_[0] = '\0';
    overlayValue_[0] = '\0';
    ControlState state = controls_.snapshot();
    updateZoneFlags(state);
    controls_.set(state);
    changed();
}

float PerformanceController::classicPitchHz(unsigned index) const {
    for (const PotConfig &pot : settings_.pots)
        if (pot.target == PotTarget::Pitch && pot.mode == PotMode::Zones)
            return pot.zones[index < 3 ? index : 1];
    return PerformanceConfig::kClassicPitchHz[index < 3 ? index : 1];
}

float PerformanceController::classicRateHz(unsigned index) const {
    for (const PotConfig &pot : settings_.pots)
        if (pot.target == PotTarget::Rate && pot.mode == PotMode::Zones)
            return pot.zones[index < 3 ? index : 1];
    return PerformanceConfig::kClassicRateHz[index < 3 ? index : 1];
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

// Fixed positions or a free sweep for one pot; the knob takes over at once
// in its new way of moving. Only Pitch and Rate have positions.
void PerformanceController::setPotMode(ControlState &state, unsigned index, PotMode mode) {
    PotConfig &pot = settings_.pots[index];
    const TargetDescriptor *descriptor = targetDescriptor(pot.target);
    if (descriptor == nullptr || !descriptor->zones) return;
    pot.mode = mode;
    if (mode == PotMode::Zones) {
        const float *defaults = pot.target == PotTarget::Pitch
            ? PerformanceConfig::kClassicPitchHz : PerformanceConfig::kClassicRateHz;
        for (unsigned i = 0; i < 3; ++i) pot.zones[i] = defaults[i];
    }
    if (potReported_[index])
        applyPotValue(state, effectivePot(pot, state), potPosition_[index]);
    updateZoneFlags(state);
}

// 1 when every pot that has positions uses them, 0 when none does, -1 if mixed
// or if no pot has any.
int PerformanceController::allPotsZoned() const {
    int zoned = 0, total = 0;
    for (const PotConfig &pot : settings_.pots) {
        const TargetDescriptor *descriptor = targetDescriptor(pot.target);
        if (descriptor == nullptr || !descriptor->zones) continue;
        ++total;
        zoned += pot.mode == PotMode::Zones;
    }
    return total == 0 ? -1 : zoned == total ? 1 : zoned == 0 ? 0 : -1;
}

void PerformanceController::rebuildMenuView() const {
    // Labels and titles are Italian; the names of sounds, pot functions,
    // button actions and LFO shapes stay as the Home screen shows them.
    menuRowCount_ = 0;
    const ControlState state = controls_.snapshot();
    char value[32];
    snprintf(menuFooterBuffer_, sizeof(menuFooterBuffer_), "%s",
        menuEditing_ ? "RUOTA CAMBIA / PREMI OK" : "RUOTA SCEGLI / PREMI APRI");
    switch (menuPage_) {
        case MenuPage::Root:
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "MENU");
            addMenuRow(Row::SetupPage, "SETUP", "COMANDI");
            addMenuRow(Row::SoundPage, "SOUND", "SUONO");
            addMenuRow(Row::ModPage, "MOD", "MODULAZIONE");
            addMenuRow(Row::EchoPage, "ECHO", "ECO");
            addMenuRow(Row::TempoPage, "TEMPO", "TEMPO");
            addMenuRow(Row::OtherPage, "OTHER", "ALTRO");
            addMenuRow(Row::Back, "BACK", "INDIETRO");
            break;
        case MenuPage::Setup: {
            // The map of the panel: every control in panel order, then ESCI at
            // the encoder's place. Moving a pot or pressing a key selects it:
            // setPot and setPad count on pots at rows 0-5 and keys at rows 6-9.
            static const Row potRows[] = {Row::Pot1, Row::Pot2, Row::Pot3,
                Row::Pot4, Row::Pot5, Row::Pot6};
            static const char *potKeys[] = {"POT_1", "POT_2", "POT_3", "POT_4", "POT_5", "POT_6"};
            static const char *potLabels[] = {"POT 1", "POT 2", "POT 3", "POT 4", "POT 5", "POT 6"};
            for (unsigned i = 0; i < 6; ++i)
                addMenuRow(potRows[i], potKeys[i], potLabels[i],
                           potTargetName(settings_.pots[i].target));
            static const Row buttonRows[] = {Row::Button1, Row::Button2, Row::Button3, Row::Button4};
            static const char *buttonKeys[] = {"BTN_1", "BTN_2", "BTN_3", "BTN_4"};
            static const char *buttonLabels[] = {"TASTO 1", "TASTO 2", "TASTO 3", "TASTO 4"};
            for (unsigned i = 0; i < 4; ++i)
                addMenuRow(buttonRows[i], buttonKeys[i], buttonLabels[i],
                    settings_.buttons[i].action == ButtonAction::PlaySound
                        ? padVoiceName(i + 1) : buttonActionName(settings_.buttons[i].action));
            addMenuRow(Row::Back, "BACK", "ESCI");
            // The banner of the map: the selected control and what it does.
            const MenuRowView &selected = menuRows_[menuIndex_ < menuRowCount_ ? menuIndex_ : 0];
            const bool pot = selected.row >= Row::Pot1 && selected.row <= Row::Pot6;
            const bool key = selected.row >= Row::Button1 && selected.row <= Row::Button4;
            if (menuEditing_ && (pot || key))
                snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "%s - %s", selected.label,
                         pot ? "SCEGLI LA FUNZIONE" : "SCEGLI L'AZIONE");
            else if (selected.value[0])
                snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "%s - %s",
                         selected.label, selected.value);
            else snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "%s", selected.label);
            break;
        }
        case MenuPage::Other: {
            // Settings without a control on the panel.
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "ALTRO");
            const int zoned = allPotsZoned();
            addMenuRow(Row::AllModes, "VALUES", "VALORI POT",
                       zoned == 1 ? "LOW/MID/HIGH" : zoned == 0 ? "LIBERI" : "MISTI");
            addMenuRow(Row::ResetDefaults, "RESET_DEFAULTS", "RIPRISTINA");
            addMenuRow(Row::Back, "BACK", "INDIETRO");
            break;
        }
        case MenuPage::Sound: {
            const bool pad = selectedIsPad();
            if (pad) {
                snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "SUONO TASTO %u%s", selectedPad_ + 1, patchDirty(selectedPad_ + 1) ? " *" : "");
                snprintf(value, sizeof(value), "%u", selectedPad_ + 1);
                addMenuRow(Row::SelectedPad, "PAD", "TASTO", value);
                addMenuRow(Row::Sound, "SOUND", "SUONO", padVoiceName(selectedPad_ + 1));
            } else {
                snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "SUONO");
                addMenuRow(Row::Voice, "VOICE", "SUONO", voiceName());
            }
            if (state.mode != SirenMode::Gunshot) addSoundValue(state, Row::Tune, "TUNE", "PITCH", "Hz");
            const bool articulated = state.mode == SirenMode::Lickshot &&
                (state.lickshotArticulation == LickshotArticulation::Shot || state.lickshotArticulation == LickshotArticulation::Roll);
            if (articulated) {
                addSoundValue(state, Row::Length, "LENGTH", "DURATA", "ms");
                addSoundValue(state, Row::Punch, "PUNCH", "PUNCH", "oct");
                addSoundValue(state, Row::PitchTime, "PITCH_TIME", "TEMPO PITCH", "ms");
                if (state.lickshotArticulation == LickshotArticulation::Roll) addSoundValue(state, Row::Rate, "RATE", "RATE", "Hz");
            } else addSoundValue(state, Row::Decay, "DECAY", "DECAY", "ms");
            if (state.mode == SirenMode::Syndrum) addSoundValue(state, Row::DrumSweep, "DRUM_SWEEP", "SWEEP PITCH", "oct");
            else if (state.mode != SirenMode::Gunshot) {
                addSoundValue(state, Row::Sweep, "SWEEP", "SWEEP", "");
                addSoundValue(state, Row::SweepTime, "SWEEP_TIME", "TEMPO SWEEP", "ms");
            }
            addSoundValue(state, Row::Tone, "TONE", "TONO", "Hz");
            addSoundValue(state, Row::Contour, "CONTOUR", "CONTOUR", "oct");
            addSoundValue(state, Row::ContourTime, "CONTOUR_TIME", "TEMPO CONTOUR", "ms");
            if (pad) {
                addMenuRow(Row::SavePatch, "SAVE", "SALVA");
                addMenuRow(Row::RecallPatch, "RECALL", "RICHIAMA", settings_.patches[selectedPad_].valid ? "" : "VUOTA");
            }
            addMenuRow(Row::Back, "BACK", "INDIETRO");
            break;
        }
        case MenuPage::Mod:
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "MODULAZIONE");
            if (state.tempo.lfoSync) {
                formatTargetValue(PotTarget::Rate, state, value, sizeof(value));
                addMenuRow(Row::LfoDivision, "LFO_DIV", "DIVISIONE", value);
            }
            if (state.rateZonesActive && !state.tempo.lfoSync)
                addMenuRow(Row::ClassicRate, "RATE_ZONE", "RATE", rateName(state.classicModulation));
            if (state.mode != SirenMode::Syndrum && state.mode != SirenMode::Gunshot &&
                !(state.mode == SirenMode::Lickshot && state.lickshotArticulation != LickshotArticulation::Legacy)) {
                static const char *shapes[] = {"SIREN", "TRIANGLE", "SQUARE", "SAW UP", "SAW DOWN", "ASYM UP", "ASYM DOWN", "PULSE 25", "PULSE 75", "MANUAL"};
                addMenuRow(Row::Shape, "SHAPE", "FORMA", shapes[static_cast<unsigned>(state.lfoShape)]);
                addMenuRow(Row::Modulation, "MODULATION", "MODULAZIONE", state.modulationEnabled ? "ON" : "OFF");
                if (!state.tempo.lfoSync && !state.rateZonesActive) addSoundValue(state, Row::Rate, "RATE", "RATE", "Hz");
                // SIREN and MANUAL have a fixed sweep and always restart on trigger.
                if (!sirenShape(effectiveLfoShape(state))) {
                    if (state.mode != SirenMode::TestTone) addSoundValue(state, Row::Depth, "DEPTH", "AMPIEZZA", "oct");
                    if (!state.tempo.lfoSync) addMenuRow(Row::Retrigger, "RETRIGGER", "RETRIGGER", state.retrigger ? "ON" : "OFF");
                }
            }
            addMenuRow(Row::Back, "BACK", "INDIETRO");
            break;
        case MenuPage::Echo:
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "ECO");
            addSoundValue(state, Row::EchoLevel, "LEVEL", "LIVELLO", "");
            if (state.tempo.echoSync) {
                formatTargetValue(PotTarget::EchoTime, state, value, sizeof(value));
                addMenuRow(Row::EchoDivision, "ECHO_DIV", "DIVISIONE", value);
            } else addSoundValue(state, Row::EchoTime, "TIME", "RITARDO", "ms");
            addSoundValue(state, Row::Feedback, "FEEDBACK", "FEEDBACK", "");
            addSoundValue(state, Row::Filter, "FILTER", "FILTRO LP / HP", "");
            addSoundValue(state, Row::Color, "COLOR", "COLORE", "");
            addMenuRow(Row::Loop, "LOOP", "LOOP", state.echoLoop ? "ON" : "OFF");
            addMenuRow(Row::Back, "BACK", "INDIETRO");
            break;
        case MenuPage::Tempo:
            // The tempo is tapped on its key; here it is typed and routed.
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "TEMPO");
            snprintf(value, sizeof(value), state.tempo.known ? "%.1f" : "%.1f (IMPOSTA)", state.tempo.bpm);
            addMenuRow(Row::Bpm, "BPM", "BPM", value);
            addMenuRow(Row::LfoSync, "LFO_SYNC", "LFO A TEMPO", state.tempo.lfoSync ? "ON" : "OFF");
            addMenuRow(Row::EchoSync, "ECHO_SYNC", "ECO A TEMPO", state.tempo.echoSync ? "ON" : "OFF");
            addMenuRow(Row::Back, "BACK", "INDIETRO");
            break;
        case MenuPage::Pot: {
            const PotConfig &pot = settings_.pots[selectedPot_];
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "POT %u - %s",
                     selectedPot_ + 1, potTargetName(pot.target));
            // The options of the function chosen on the map, in its cell.
            const TargetDescriptor *descriptor = targetDescriptor(pot.target);
            if (descriptor != nullptr && descriptor->zones)
                addMenuRow(Row::Mode, "MODE", "VALORI", pot.mode != PotMode::Zones ? "LIBERI"
                    : pot.target == PotTarget::Pitch ? "LOW/MID/HIGH" : "SLOW/MID/FAST");
            if (pot.mode == PotMode::Zones) {
                formatConfiguredValue(pot.target, pot.zones[0], value, sizeof(value));
                addMenuRow(Row::Low, "LOW", pot.target == PotTarget::Pitch ? "LOW" : "SLOW", value);
                formatConfiguredValue(pot.target, pot.zones[1], value, sizeof(value));
                addMenuRow(Row::Middle, "MID", "MID", value);
                formatConfiguredValue(pot.target, pot.zones[2], value, sizeof(value));
                addMenuRow(Row::High, "HIGH", pot.target == PotTarget::Pitch ? "HIGH" : "FAST", value);
            } else if (pot.target != PotTarget::None) {
                formatConfiguredValue(pot.target, pot.minimum, value, sizeof(value));
                addMenuRow(Row::Minimum, "MIN", "MINIMO", value);
                formatConfiguredValue(pot.target, pot.maximum, value, sizeof(value));
                addMenuRow(Row::Maximum, "MAX", "MASSIMO", value);
            }
            addMenuRow(Row::Back, "BACK", "INDIETRO");
            break;
        }
        case MenuPage::Button: {
            const ButtonConfig &button = settings_.buttons[selectedButton_];
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "TASTO %u - %s", selectedButton_ + 1,
                     buttonActionName(button.action));
            if (button.action == ButtonAction::PlaySound) {
                addMenuRow(Row::Sound, "SOUND", "SUONO", padVoiceName(selectedButton_ + 1));
                if (panelSoundSupportsLatch(button.sound))
                    addMenuRow(Row::Latch, "LATCH", "LATCH", button.latch ? "ON" : "OFF");
            }
            addMenuRow(Row::Back, "BACK", "INDIETRO");
            break;
        }
        case MenuPage::Reset:
            snprintf(menuTitleBuffer_, sizeof(menuTitleBuffer_), "RIPRISTINA");
            addMenuRow(Row::Confirm, "CONFIRM", "CONFERMA", resetConfirmed_ ? "SI" : "NO");
            addMenuRow(Row::Back, "BACK", "INDIETRO");
            break;
    }
    // The footer describes the selected row; a moved pot or an error replaces it.
    const char *help = menuIndex_ < menuRowCount_ ? rowHelp(menuRows_[menuIndex_].row, state) : nullptr;
    if (help) snprintf(menuFooterBuffer_, sizeof(menuFooterBuffer_), "%s", help);
    // On the map a moved pot only selects its cell: the banner keeps explaining.
    if (displayMode_ == DisplayMode::Menu && menuPage_ != MenuPage::Setup && overlayTitle_[0])
        snprintf(menuFooterBuffer_, sizeof(menuFooterBuffer_), "%s %s",
                 overlayTitle_, overlayValue_);
    if (persistenceStatus_[0]) snprintf(menuFooterBuffer_, sizeof(menuFooterBuffer_),
                                       "%s", persistenceStatus_);
}

// One short line per row, null where the label says it all. The footer is
// 184 px wide in the 16 px font: about 27 characters.
const char *PerformanceController::rowHelp(Row row, const ControlState &state) const {
    // On the map the help is what the control does; elsewhere, while a choice
    // is being changed, what that choice does.
    if (row >= Row::Pot1 && row <= Row::Pot6)
        return potTargetHelp(settings_.pots[static_cast<unsigned>(row) - static_cast<unsigned>(Row::Pot1)].target);
    if (row >= Row::Button1 && row <= Row::Button4)
        return buttonActionHelp(settings_.buttons[static_cast<unsigned>(row) - static_cast<unsigned>(Row::Button1)].action);
    if (menuEditing_ && row == Row::Shape) return shapeHelp(state.lfoShape);
    if (menuEditing_ && row == Row::Voice)
        return voiceHelp(kVoices[voiceIndex_].sound, kVoices[voiceIndex_].variant);
    if (menuEditing_ && row == Row::Sound) {
        const ButtonConfig &button = settings_.buttons[menuPage_ == MenuPage::Sound ? selectedPad_ : selectedButton_];
        return voiceHelp(button.sound, button.variant);
    }
    switch (row) {
        case Row::SoundPage: return "SUONO E SUOI PARAMETRI";
        case Row::ModPage: return "MOVIMENTO DEL PITCH (LFO)";
        case Row::EchoPage: return "LIVELLO, RITARDO, FILTRO";
        case Row::TempoPage: return "BPM, LFO ED ECO A TEMPO";
        case Row::SetupPage: return "ASSEGNA POT E TASTI";
        case Row::OtherPage: return "VALORI DEI POT, RIPRISTINA";
        case Row::ResetDefaults:
        case Row::Confirm: return "RIPORTA TUTTO DI FABBRICA";
        case Row::Bpm: return "SI BATTE SUL TASTO TAP";
        case Row::LfoSync: return state.tempo.lfoSync ? "IL RATE SEGUE IL BPM" : "RATE LIBERO, IN HZ";
        case Row::EchoSync: return state.tempo.echoSync ? "IL RITARDO SEGUE IL BPM" : "RITARDO LIBERO, IN MS";
        case Row::LfoDivision:
        case Row::EchoDivision: return "FRAZIONE DELLA BATTUTA";
        case Row::Decay: return "CODA DOPO IL RILASCIO";
        case Row::Sweep: return "PARTE BASSO E POI SALE";
        case Row::SweepTime: return "DURATA DELLO SWEEP";
        case Row::DrumSweep: return "CADUTA DI PITCH DEL COLPO";
        case Row::Tone: return "TAGLIA GLI ACUTI DEL SUONO";
        case Row::Contour: return "MUOVE IL FILTRO AL TRIGGER";
        case Row::ContourTime: return "DURATA DEL CONTOUR";
        case Row::SavePatch: return "MEMORIZZA SUONO NEL TASTO";
        case Row::RecallPatch: return "TORNA AL SUONO SALVATO";
        case Row::Shape: return "ANDAMENTO DEL PITCH";
        case Row::Modulation: return "ON: IL PITCH SI MUOVE";
        case Row::Depth: return "QUANTO SI MUOVE IL PITCH";
        case Row::Retrigger: return "ON: RIPARTE A OGNI TRIGGER";
        case Row::Feedback: return "QUANTE RIPETIZIONI";
        case Row::Filter: return "SINISTRA LP, DESTRA HP";
        case Row::Color: return "SCURISCE E SATURA LE CODE";
        case Row::Loop: return "ON: L'ECO GIRA SENZA FINE";
        case Row::Mode:
            return settings_.pots[selectedPot_].mode == PotMode::Zones
                ? "ORA A POSIZIONI FISSE" : "ORA SCORRE DA MIN A MAX";
        case Row::AllModes: {
            const int zoned = allPotsZoned();
            return zoned == 1 ? "PITCH E RATE A SCATTI" : zoned == 0 ? "PITCH E RATE LIBERI"
                                                                    : "MISTI: OGNI POT A MODO SUO";
        }
        case Row::Back: return menuPage_ == MenuPage::Setup ? "TORNA A SUONARE" : nullptr;
        case Row::Low:
        case Row::Middle:
        case Row::High: return "VALORE DI QUESTA POSIZIONE";
        case Row::Minimum: return "VALORE A POT TUTTO GIU";
        case Row::Maximum: return "VALORE A POT TUTTO SU";
        case Row::Latch: return "ON: RESTA ACCESO DA SOLO";
        default: return nullptr;
    }
}

unsigned PerformanceController::mapControl() const {
    if (!controlsMap()) return 0;
    rebuildMenuView();
    if (menuIndex_ >= menuRowCount_) return 0;
    const unsigned row = static_cast<unsigned>(menuRows_[menuIndex_].row);
    if (row >= static_cast<unsigned>(Row::Pot1) && row <= static_cast<unsigned>(Row::Pot6))
        return 1 + row - static_cast<unsigned>(Row::Pot1);
    if (row >= static_cast<unsigned>(Row::Button1) && row <= static_cast<unsigned>(Row::Button4))
        return 7 + row - static_cast<unsigned>(Row::Button1);
    return row == static_cast<unsigned>(Row::Back) ? 11 : 0;
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
        if (row == Row::Bpm && !controls_.snapshot().tempo.known)
            setTempoBpm(controls_.snapshot().tempo.bpm);
        if (row == Row::Confirm && resetConfirmed_) {
            resetDefaults();
            resetConfirmed_ = false;
            setMenuPage(MenuPage::Root);
            return;
        }
        // Pad family/variant are button assignments and persist like the Home
        // encoder path; sound, MOD and ECHO values stay live until SAVE PATCH.
        const bool assignment = row == Row::Sound;
        if ((assignment || (menuPage_ != MenuPage::Sound && menuPage_ != MenuPage::Mod &&
            menuPage_ != MenuPage::Echo && menuPage_ != MenuPage::Tempo))) commitSettingsChange();
        // On the map the chosen function opens its options, when it has any.
        if (menuPage_ == MenuPage::Setup && row >= Row::Pot1 && row <= Row::Pot6 &&
            settings_.pots[selectedPot_].target != PotTarget::None) {
            setMenuPage(MenuPage::Pot);
            return;
        }
        if (menuPage_ == MenuPage::Setup && row >= Row::Button1 && row <= Row::Button4 &&
            settings_.buttons[selectedButton_].action == ButtonAction::PlaySound) {
            setMenuPage(MenuPage::Button);
            return;
        }
        changed();
        return;
    }
    if (row == Row::SavePatch) { savePad(selectedPad_ + 1); return; }
    if (row == Row::RecallPatch) { recallPad(selectedPad_ + 1); return; }
    if (row == Row::SoundPage) { selectedButton_ = selectedPad_; setMenuPage(MenuPage::Sound); }
    else if (row == Row::ModPage) setMenuPage(MenuPage::Mod);
    else if (row == Row::EchoPage) setMenuPage(MenuPage::Echo);
    else if (row == Row::TempoPage) setMenuPage(MenuPage::Tempo);
    else if (row == Row::SetupPage) setMenuPage(MenuPage::Setup);
    else if (row == Row::OtherPage) setMenuPage(MenuPage::Other);
    // On the map a control's function is chosen in its own cell.
    else if (row >= Row::Pot1 && row <= Row::Pot6) {
        selectedPot_ = static_cast<unsigned>(row) - static_cast<unsigned>(Row::Pot1);
        menuEditing_ = true;
        changed();
    } else if (row >= Row::Button1 && row <= Row::Button4) {
        selectedButton_ = static_cast<unsigned>(row) - static_cast<unsigned>(Row::Button1);
        menuEditing_ = true;
        changed();
    } else if (row == Row::ResetDefaults) {
        resetConfirmed_ = false;
        setMenuPage(MenuPage::Reset);
    } else if (row == Row::Back && menuPage_ == MenuPage::Setup) {
        // ESCI on the map: straight back to playing; settings are saved anyway.
        displayMode_ = DisplayMode::Home;
        changed();
    } else if (row == Row::Back) {
        leaveMenuPage();
    } else {
        menuEditing_ = true;
        changed();
    }
}

void PerformanceController::leaveMenuPage() {
    if (menuPage_ == MenuPage::Root) {
        displayMode_ = DisplayMode::Home;
        changed();
    } else if (menuPage_ == MenuPage::Pot || menuPage_ == MenuPage::Button ||
               menuPage_ == MenuPage::Reset) {
        // Back where the page was opened: its cell on the map, or its row.
        const Row from = menuPage_ == MenuPage::Pot
            ? static_cast<Row>(static_cast<unsigned>(Row::Pot1) + selectedPot_)
            : menuPage_ == MenuPage::Button
            ? static_cast<Row>(static_cast<unsigned>(Row::Button1) + selectedButton_)
            : Row::ResetDefaults;
        setMenuPage(menuPage_ == MenuPage::Pot || menuPage_ == MenuPage::Button
                    ? MenuPage::Setup : MenuPage::Other);
        rebuildMenuView();
        for (unsigned i = 0; i < menuRowCount_; ++i)
            if (menuRows_[i].row == from) menuIndex_ = static_cast<uint8_t>(i);
    } else setMenuPage(MenuPage::Root);
}

void PerformanceController::menuBack() {
    normalizeMenuIndex();
    rebuildMenuView();
    if (!menuEditing_) {
        leaveMenuPage();
        return;
    }
    // Ends editing like the push (DONE), but never confirms a Reset.
    if (menuRowCount_ > 0 && menuRows_[menuIndex_].row == Row::Confirm) {
        menuEditing_ = false;
        resetConfirmed_ = false;
        changed();
        return;
    }
    // On the map BACK keeps the choice and stays there, without the options.
    if (menuPage_ == MenuPage::Setup) {
        menuEditing_ = false;
        commitSettingsChange();
        return;
    }
    pressMenuItem();
}

void PerformanceController::editMenuItem(Row row, int direction) {
    ControlState state = controls_.snapshot();
    // A map cell edits the function of its pot or the action of its key.
    if (row >= Row::Pot1 && row <= Row::Pot6) row = Row::Function;
    else if (row >= Row::Button1 && row <= Row::Button4) row = Row::Action;
    if (row == Row::Bpm) {
        setTempoBpm(clampFloat(state.tempo.bpm + direction, 30.0f, 300.0f)); return;
    }
    if (row == Row::EchoSync || row == Row::LfoSync) {
        const bool echo = row == Row::EchoSync;
        setTempoSync(echo, !(echo ? state.tempo.echoSync : state.tempo.lfoSync)); return;
    }
    if (row == Row::EchoDivision || row == Row::LfoDivision ||
        (row == Row::Rate && state.tempo.lfoSync)) {
        const bool echo = row == Row::EchoDivision;
        const auto division = echo ? state.tempo.echoDivision : state.tempo.lfoDivision;
        setTempoDivision(echo, static_cast<TempoDivision>(wrappedStep(
            static_cast<int>(division), direction, 4))); return;
    }
    PotConfig &pot = settings_.pots[selectedPot_];
    ButtonConfig &button = settings_.buttons[menuPage_ == MenuPage::Sound ? selectedPad_ : selectedButton_];
    float minimum, maximum, step;
    if (float *value = soundValue(state, row, minimum, maximum, step)) {
        *value = clampFloat(*value + direction * step, minimum, maximum);
        if (row == Row::Tune) state.pitchZonesActive = false;
        if (row == Row::Filter) applyEchoFilter(state, *value);
        controls_.set(state); changed(); return;
    }
    switch (row) {
        case Row::SelectedPad:
            do selectedPad_ = wrappedStep(selectedPad_, direction, 4);
            while (!selectedIsPad());
            applySound(state, selectedPad_, false); controls_.set(state); break;
        case Row::Voice:
            stepVoice(state, direction); controls_.set(state); break;
        case Row::Shape:
            state.lfoShape = static_cast<LfoShape>(wrappedStep(static_cast<int>(state.lfoShape), direction, 10)); controls_.set(state); break;
        case Row::Modulation: state.modulationEnabled = !state.modulationEnabled; controls_.set(state); break;
        case Row::Retrigger: state.retrigger = !state.retrigger; controls_.set(state); break;
        case Row::Loop: state.echoLoop = !state.echoLoop; controls_.set(state); break;
        case Row::ClassicRate:
            state.classicModulation = static_cast<ClassicModulation>(wrappedStep(static_cast<int>(state.classicModulation), direction, rateZoneCount(state)));
            if (state.classicModulation != ClassicModulation::Manual) state.lfoRateHz = classicRateHz(static_cast<unsigned>(state.classicModulation));
            controls_.set(state); break;
        case Row::Function: {
            pot = continuousPot(static_cast<PotTarget>(wrappedStep(
                static_cast<int>(pot.target), direction, static_cast<int>(PotTarget::Count))));
            break;
        }
        case Row::Mode:
            setPotMode(state, selectedPot_,
                       pot.mode == PotMode::Zones ? PotMode::Continuous : PotMode::Zones);
            controls_.set(state);
            break;
        case Row::AllModes: {
            // One switch for every pot; each keeps its own row on its page.
            const PotMode mode = allPotsZoned() == 1 ? PotMode::Continuous : PotMode::Zones;
            for (unsigned i = 0; i < 6; ++i) setPotMode(state, i, mode);
            controls_.set(state);
            break;
        }
        case Row::Minimum:
        case Row::Maximum: {
            const TargetDescriptor &descriptor = *targetDescriptor(pot.target);
            float &value = row == Row::Minimum ? pot.minimum : pot.maximum;
            const float edited = clampFloat(editingStep(descriptor, value, direction),
                                             descriptor.absoluteMin, descriptor.absoluteMax);
            if ((row == Row::Minimum && edited < pot.maximum) ||
                (row == Row::Maximum && edited > pot.minimum)) value = edited;
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
            button.action = static_cast<ButtonAction>(wrappedStep(
                static_cast<int>(button.action), direction,
                static_cast<int>(ButtonAction::Count)));
            if (button.action == ButtonAction::PlaySound) {
                button.sound = PanelSound::Classic;
                button.variant = 0;
                button.latch = false;
            }
            break;
        case Row::Sound: stepPadVoice(button, direction); break;
        case Row::Latch: button.latch = !button.latch; break;
        case Row::Confirm: resetConfirmed_ = !resetConfirmed_; break;
        default: break;
    }
    if (menuPage_ == MenuPage::Sound && row == Row::Sound) {
        button.action = ButtonAction::PlaySound;
        applySound(state, selectedPad_, false);
        controls_.set(state);
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
    if (row == Row::Rate && state.tempo.lfoSync) {
        char value[32];
        formatTargetValue(PotTarget::Rate, state, value, sizeof(value));
        addMenuRow(row, key, label, value);
        return;
    }
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
    if ((target == PotTarget::EchoTime && state.tempo.echoSync) ||
        (target == PotTarget::Rate && state.tempo.lfoSync)) {
        const bool echo = target == PotTarget::EchoTime;
        if (!echo && !tempoLfoActive(state)) {
            snprintf(buffer, size, "%s N/A VOICE", tempoDivisionName(state.tempo.lfoDivision));
            return;
        }
        const bool available = echo ? tempoEchoAvailable(state.tempo) : tempoLfoAvailable(state.tempo);
        snprintf(buffer, size, echo ? "%s %s%.1f ms" : "%s %s%.2f Hz",
            tempoDivisionName(echo ? state.tempo.echoDivision : state.tempo.lfoDivision),
            available ? "" : "N/A HOLD ",
            echo ? state.tempo.echoMs : state.tempo.lfoHz);
        return;
    }
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
        case PotTarget::LfoDepth: formatConfiguredValue(target, state.lfoDepthOctaves, buffer, size); break;
        case PotTarget::EchoLevel: formatConfiguredValue(target, state.echoLevel, buffer, size); break;
        case PotTarget::EchoTime: formatConfiguredValue(target, state.delayMs, buffer, size); break;
        case PotTarget::EchoFeedback: formatConfiguredValue(target, state.feedback, buffer, size); break;
        case PotTarget::EchoFilter: {
            const float amount = PerformanceConfig::echoFilterAmount(state.echoFilterPosition);
            if (amount == 0) snprintf(buffer, size, "FLAT");
            else snprintf(buffer, size, "%s %.0f%%", state.echoFilterPosition < 0 ? "LP" : "HP", amount * 100);
            break;
        }
        case PotTarget::Decay: formatConfiguredValue(target, state.decayMs, buffer, size); break;
        case PotTarget::SweepAmount: formatConfiguredValue(target, state.sweepAmount, buffer, size); break;
        case PotTarget::SweepTime: formatConfiguredValue(target, state.sweepMs, buffer, size); break;
        case PotTarget::None: snprintf(buffer, size, "OFF"); break;
        case PotTarget::Count: snprintf(buffer, size, "N/A"); break;
    }
}

void PerformanceController::formatCurrentValue(PotTarget target, char *buffer,
                                               unsigned size) const {
    formatTargetValue(target, controls_.snapshot(), buffer, size);
}
