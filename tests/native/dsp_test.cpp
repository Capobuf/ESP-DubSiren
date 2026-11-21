// Zig release builds define NDEBUG; regressions must execute under -O2 too.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <assert.h>
#include <math.h>
#include <memory>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "audio/AudioEngine.h"
#include "audio/Delay.h"
#include "audio/Envelope.h"
#include "audio/Lfo.h"
#include "audio/Oscillator.h"
#include "audio/PerformanceConfig.h"
#include "audio/SmoothedParameter.h"
#include "control/CommandParser.h"

constexpr float kRate = 48000.0f;

float blockRms(const int16_t *samples, size_t count) {
    double energy = 0.0;
    for (size_t i = 0; i < count; ++i) {
        energy += static_cast<double>(samples[i]) * samples[i];
    }
    return static_cast<float>(sqrt(energy / count));
}

float estimateFrequency(const int16_t *samples, size_t count) {
    unsigned crossings = 0;
    double first = 0.0, last = 0.0;
    for (size_t i = 1; i < count; ++i) {
        if (samples[i - 1] <= 0 && samples[i] > 0) {
            const double crossing = i - 1 + double(-samples[i - 1]) / (samples[i] - samples[i - 1]);
            if (crossings == 0) first = crossing;
            last = crossing;
            ++crossings;
        }
    }
    return crossings > 1 ? (crossings - 1) * kRate / (last - first) : 0.0f;
}

struct HarmonicSnapshot {
    float harmonic[5];
    float rms;
    float mean;
    float peak;
};

HarmonicSnapshot measureVoice(SirenMode mode, float modulation,
                              float frequencyHz = 375.0f) {
    constexpr int kWarmupSamples = 48000;
    constexpr int kMeasureSamples = 48000;
    Oscillator oscillator;
    for (int i = 0; i < kWarmupSamples; ++i) {
        oscillator.next(mode, frequencyHz, modulation, 1.0f,
                        true, kRate);
    }

    double real[5] = {};
    double imaginary[5] = {};
    double energy = 0.0;
    double sum = 0.0;
    float peak = 0.0f;
    for (int i = 0; i < kMeasureSamples; ++i) {
        const float value = oscillator.next(
            mode, frequencyHz, modulation, 1.0f,
            true, kRate);
        assert(isfinite(value));
        energy += static_cast<double>(value) * value;
        sum += value;
        if (fabsf(value) > peak) peak = fabsf(value);
        for (int harmonic = 1; harmonic <= 5; ++harmonic) {
            const double angle = TWO_PI * frequencyHz * harmonic * i / kRate;
            real[harmonic - 1] += value * cos(angle);
            imaginary[harmonic - 1] += value * sin(angle);
        }
    }

    HarmonicSnapshot result = {};
    for (int harmonic = 0; harmonic < 5; ++harmonic) {
        result.harmonic[harmonic] = static_cast<float>(
            2.0 * hypot(real[harmonic], imaginary[harmonic]) /
            kMeasureSamples);
    }
    result.rms = static_cast<float>(sqrt(energy / kMeasureSamples));
    result.mean = static_cast<float>(sum / kMeasureSamples);
    result.peak = peak;
    return result;
}

HarmonicSnapshot measureSine1(float modulation, float frequencyHz = 375.0f) {
    return measureVoice(SirenMode::Sine1, modulation, frequencyHz);
}

float harmonicRatio(const HarmonicSnapshot &spectrum, int harmonic) {
    return spectrum.harmonic[harmonic - 1] / spectrum.harmonic[0];
}

float harmonicEnergyRatio(const HarmonicSnapshot &spectrum) {
    double energy = 0.0;
    for (int harmonic = 1; harmonic < 5; ++harmonic) {
        energy += static_cast<double>(spectrum.harmonic[harmonic]) *
                  spectrum.harmonic[harmonic];
    }
    return static_cast<float>(sqrt(energy) / spectrum.harmonic[0]);
}

float sine2Duty(float modulation) {
    float duty = PerformanceConfig::kSine2Voicing.duty +
                 modulation * PerformanceConfig::kSine2Voicing.dutyModAmount;
    if (duty < PerformanceConfig::kSine2MinimumDuty) {
        return PerformanceConfig::kSine2MinimumDuty;
    }
    return duty > PerformanceConfig::kSine2MaximumDuty
               ? PerformanceConfig::kSine2MaximumDuty
               : duty;
}

// The former pad layout: four sound pads, continuous Pitch/Rate, 120 ms tail.
template <typename Parser> bool padLayout(Parser &parser) {
    static const char *sounds[] = {"SIREN", "TWO_TONE", "LICKSHOT", "PING"};
    bool ok = true;
    for (int i = 0; i < 4; ++i) {
        char command[64];
        snprintf(command, sizeof(command), "PANEL BUTTON %d ACTION PLAY_SOUND", i + 1);
        ok &= parser.parse(command);
        snprintf(command, sizeof(command), "PANEL BUTTON %d SOUND %s 0 0", i + 1, sounds[i]);
        ok &= parser.parse(command);
    }
    ok &= parser.parse("PANEL POT 1 MODE CONTINUOUS");
    ok &= parser.parse("PANEL POT 2 MODE CONTINUOUS");
    ok &= parser.parse("SET DECAY_MS 120");
    return ok;
}

// The former factory keys, still assignable: Trigger / Hold / Mod Up / Echo Cut.
template <typename Parser> bool classicKeys(Parser &parser) {
    return parser.parse("PANEL BUTTON 1 ACTION TRIGGER") &&
        parser.parse("PANEL BUTTON 2 ACTION HOLD") &&
        parser.parse("PANEL BUTTON 3 ACTION MOD_UP") &&
        parser.parse("PANEL BUTTON 4 ACTION ECHO_CUT");
}

// The menu way to change a sound, next to turning the encoder at Home: from
// Home, steps the SOUND row of the selected pad (VOICE for a Trigger) and
// returns Home.
template <typename Parser> void stepSound(Parser &parser, int steps) {
    auto click = [&]() {
        assert(parser.parse("ENCODER_SWITCH 1")); assert(parser.parse("ENCODER_SWITCH 0"));
    };
    auto at = [&](const char *key) {
        return strcmp(parser.menuItemKey(parser.menuIndex()), key) == 0;
    };
    click();
    while (!at("SOUND")) assert(parser.parse("ENCODER_ROTATE NEXT"));
    click();  // The SUONO page opens on its first row, never the sound row.
    assert(parser.parse("ENCODER_ROTATE PREV"));
    while (!at("SOUND") && !at("VOICE")) assert(parser.parse("ENCODER_ROTATE NEXT"));
    click();
    for (int i = 0; i < abs(steps); ++i)
        assert(parser.parse(steps < 0 ? "ENCODER_ROTATE PREV" : "ENCODER_ROTATE NEXT"));
    click();
    for (int level = 0; level < 2; ++level) {
        while (!at("BACK")) assert(parser.parse("ENCODER_ROTATE PREV"));
        click();
    }
    assert(strcmp(parser.displayModeName(), "HOME") == 0);
}

// Those keys with the factory zoned Pitch and Rate.
template <typename Parser> bool factoryLayout(Parser &parser) {
    return parser.parse("PANEL RESET_DEFAULTS") && classicKeys(parser) &&
        parser.parse("SET DECAY_MS 0");
}

// Raw state equivalent of the pad layout defaults.
inline void padState(ControlState &state) {
    state.pitchZonesActive = state.rateZonesActive = false;
    state.decayMs = PerformanceConfig::kExtendedDecayMs;
}

float renderFrequency(ControlState state) {
    auto engine = std::make_unique<AudioEngine>();
    engine->begin();
    state.hold = true;
    state.echoLevel = 0.0f;
    state.masterVolume = 0.7f;
    int16_t block[480];
    for (int i = 0; i < 150; ++i) engine->render(state, block, 480);
    int16_t capture[4800];
    for (int i = 0; i < 10; ++i) {
        engine->render(state, capture + i * 480, 480);
    }
    return estimateFrequency(capture, 4800);
}

void testSmoothing() {
    SmoothedParameter parameter(0.0f);
    parameter.configure(15.0f, kRate);
    parameter.setTarget(1.0f);
    assert(parameter.next() > 0.0f && parameter.current() < 0.01f);
    for (int i = 0; i < 10000; ++i) assert(isfinite(parameter.next()));
    assert(fabsf(parameter.current() - 1.0f) < 0.0001f);
}

void testOneShotEnvelope() {
    OneShotEnvelope envelope;
    assert(envelope.next(100.0f, kRate) == 0.0f);
    envelope.trigger();
    const float first = envelope.next(100.0f, kRate);
    assert(first > 0.99f && first <= 1.0f);
    float value = first;
    for (int i = 0; i < 96000; ++i) value = envelope.next(100.0f, kRate);
    assert(value == 0.0f);
    envelope.trigger();
    assert(envelope.next(100.0f, kRate) > 0.99f);
    envelope.reset();
    assert(envelope.next(100.0f, kRate) == 0.0f);
}

void testParser() {
    ControlStore store;
    CommandParser parser(store);
    assert(parser.parse("SET TUNE_HZ 333"));
    assert(factoryLayout(parser));
    assert(parser.parse("SET CLASSIC_PITCH HIGH"));
    assert(parser.parse("SET CLASSIC_MOD MANUAL"));
    auto state = store.snapshot();
    assert(state.classicPitch == ClassicPitch::High);
    assert(state.classicModulation == ClassicModulation::Manual);
    assert(!parser.parse("SET CLASSIC_PITCH NOPE"));
    assert(!parser.parse("SET PROFILE EXTENDED"));
    assert(padLayout(parser));
    state = store.snapshot();
    assert(fabsf(state.tuneHz - PerformanceConfig::kClassicPitchHz[2]) < .001f);

    assert(parser.parse("SET MODE LICKSHOT"));
    assert(store.snapshot().mode == SirenMode::Lickshot);
    assert(parser.parse("SET MODE SONAR"));
    assert(store.snapshot().mode == SirenMode::Sonar);
    assert(parser.parse("SET MODE SAW"));
    assert(store.snapshot().mode == SirenMode::Saw);
    assert(parser.parse("SET MODE TRIANGLE"));
    assert(store.snapshot().mode == SirenMode::Triangle);
    assert(parser.parse("SET MODE SYNDRUM"));
    assert(store.snapshot().mode == SirenMode::Syndrum);
    assert(parser.parse("SET SYNDRUM_WAVE TRIANGLE"));
    assert(parser.parse("SET SYNDRUM_SWEEP -2.5"));
    assert(parser.parse("SET SWEEP 0.75"));
    assert(parser.parse("SET SWEEP_MS 800"));
    state = store.snapshot();
    assert(state.syndrumWave == SyndrumWave::Triangle);
    assert(state.syndrumSweepOctaves == -2.5f);
    assert(state.sweepAmount == 0.75f && state.sweepMs == 800.0f);
    assert(parser.parse("SET MODE GUNSHOT"));
    assert(store.snapshot().mode == SirenMode::Gunshot);
    // Same +/-4 octave limit as the panel menu and stored patches.
    assert(parser.parse("SET SYNDRUM_SWEEP -3.5"));
    assert(store.snapshot().syndrumSweepOctaves == -3.5f);
    const auto validTransient = store.snapshot();
    assert(!parser.parse("SET SWEEP 1.1"));
    assert(!parser.parse("SET SWEEP_MS 49"));
    assert(!parser.parse("SET SYNDRUM_SWEEP -4.1"));
    assert(!parser.parse("SET SYNDRUM_WAVE SAW"));
    state = store.snapshot();
    assert(state.mode == validTransient.mode &&
           state.sweepAmount == validTransient.sweepAmount &&
           state.sweepMs == validTransient.sweepMs &&
           state.syndrumSweepOctaves == validTransient.syndrumSweepOctaves &&
           state.syndrumWave == validTransient.syndrumWave);
    assert(parser.parse("MODULATION 0"));
    assert(parser.parse("RETRIGGER 1"));
    assert(parser.parse("ECHO_SPIN 1"));
    assert(parser.parse("ECHO_LOOP 1"));
    state = store.snapshot();
    assert(!state.modulationEnabled && state.retrigger && state.echoSpin &&
           state.echoLoop);
    assert(!parser.parse("MODULATION 2"));
    assert(!parser.parse("SET MODE AIRHORN"));
    const auto unchanged = store.snapshot();
    assert(unchanged.mode == SirenMode::Gunshot && !unchanged.modulationEnabled &&
           unchanged.retrigger && unchanged.echoSpin && unchanged.echoLoop);
}

void testPadController() {
    ControlStore store;
    CommandParser parser(store);
    assert(padLayout(parser));
    assert(strcmp(parser.padSoundName(1), "SIREN") == 0);
    assert(strcmp(parser.padSoundName(2), "TWO TONE") == 0);
    assert(strcmp(parser.padSoundName(3), "LICKSHOT") == 0);
    assert(strcmp(parser.padSoundName(4), "PING") == 0);

    assert(parser.parse("SET TUNE_HZ 333"));
    assert(parser.parse("SET DELAY_MS 777"));
    assert(parser.parse("SET ECHO_LEVEL 0.73"));
    assert(parser.parse("PAD 1 1"));
    auto state = store.snapshot();
    assert(state.mode == SirenMode::Sine1 && state.trigger);
    assert(state.activationSequence == 1);
    assert(state.tuneHz == 333.0f && state.delayMs == 777.0f &&
           state.echoLevel == 0.73f);

    assert(parser.parse("PAD 2 1"));
    state = store.snapshot();
    assert(state.mode == SirenMode::Square && state.trigger);
    assert(state.activationSequence == 2);
    assert(parser.activePad() == 2 && parser.selectedPad() == 2);

    assert(parser.parse("PAD 2 0"));
    state = store.snapshot();
    assert(state.mode == SirenMode::Sine1 && state.trigger);
    assert(state.activationSequence == 2);
    assert(parser.activePad() == 1 && parser.selectedPad() == 1);
    assert(parser.parse("PAD 1 0"));
    assert(!store.snapshot().trigger && parser.activePad() == 0);

    // Hold gesture: a playing key together with the encoder push, in either
    // order. It toggles Hold and the encoder release does not open the menu.
    const unsigned menuBefore = parser.panelMenuRevision();
    assert(parser.parse("PAD 1 1"));
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(parser.parse("PAD 1 0"));
    assert(store.snapshot().hold && !store.snapshot().trigger);
    assert(parser.panelMenuRevision() == menuBefore);
    assert(parser.lockedButton() == 1 && strcmp(parser.overlayValue(), "BLOCCATO") == 0);
    // Hold survives a change of the pad's sound.
    assert(parser.parse("PANEL BUTTON 1 SOUND CLASSIC 1 0"));
    assert(strcmp(parser.padVoiceName(1), "SINE 2") == 0);
    assert(store.snapshot().hold && store.snapshot().mode == SirenMode::Sine2);
    assert(parser.parse("PANEL BUTTON 1 SOUND CLASSIC 0 0"));
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("PAD 1 1"));
    assert(parser.parse("PAD 1 0"));
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(!store.snapshot().hold && !store.snapshot().trigger);
    assert(parser.panelMenuRevision() == menuBefore);
    assert(parser.lockedButton() == 0);

    // One press of the locked key unlocks it, without playing.
    auto lock = [&](const char *down, const char *up) {
        assert(parser.parse(down));
        assert(parser.parse("ENCODER_SWITCH 1")); assert(parser.parse("ENCODER_SWITCH 0"));
        assert(parser.parse(up));
        assert(store.snapshot().hold);
    };
    lock("PAD 1 1", "PAD 1 0");
    const unsigned activations = store.snapshot().activationSequence;
    assert(parser.parse("PAD 1 1"));
    assert(!store.snapshot().hold && !store.snapshot().trigger);
    assert(store.snapshot().activationSequence == activations);
    assert(strcmp(parser.overlayValue(), "SBLOCCATO") == 0);
    assert(parser.parse("PAD 1 0"));
    assert(!store.snapshot().hold && parser.lockedButton() == 0);
    // Another playing key takes the lock over with its sound.
    lock("PAD 1 1", "PAD 1 0");
    assert(parser.parse("PAD 2 1")); assert(parser.parse("PAD 2 0"));
    assert(store.snapshot().hold && parser.lockedButton() == 2);
    // While locked the first encoder click only unlocks; the next opens MENU.
    assert(parser.parse("ENCODER_SWITCH 1")); assert(parser.parse("ENCODER_SWITCH 0"));
    assert(!store.snapshot().hold && parser.lockedButton() == 0);
    assert(parser.panelMenuRevision() == menuBefore);
    assert(parser.parse("PAD 1 1")); assert(parser.parse("PAD 1 0"));

    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(parser.panelMenuRevision() == menuBefore + 1);

    assert(parser.parse("PAD_SELECT 1"));
    assert(parser.parse("PAD_HOLD_TOGGLE"));
    assert(parser.padHold(1));
    assert(parser.parse("PAD_SELECT 3"));
    assert(parser.parse("PAD_ASSIGN NEXT"));
    assert(strcmp(parser.padSoundName(3), "PING") == 0);
    assert(parser.padHold(1));
    assert(store.snapshot().mode == SirenMode::Sine1);
    assert(store.snapshot().hold);
    assert(parser.parse("PAD 3 1"));
    assert(parser.padHold(1));
    assert(parser.parse("PAD 3 0"));

    assert(parser.parse("PAD_CONFIG 3 PING 0 0"));
    assert(strcmp(parser.padSoundName(3), "PING") == 0);
    assert(!parser.padHoldSupported(3));
    assert(!parser.parse("PAD_CONFIG 3 PING 0 1"));
}

void testQueuedPadActivation() {
    ControlStore store;
    CommandParser parser(store);
    assert(padLayout(parser));
    auto engine = std::make_unique<AudioEngine>();
    engine->begin();
    int16_t block[480];
    engine->render(store.snapshot(), block, 480);
    assert(parser.parse("PAD 4 1"));
    assert(parser.parse("PAD 4 0"));
    const ControlState released = store.snapshot();
    const ActivationEvent &event = released.activationEvents[
        (released.activationSequence - 1) % kActivationEventCapacity];
    assert(!released.trigger && event.mode == SirenMode::Syndrum);
    engine->render(released, block, 480);
    assert(blockRms(block, 480) > 100.0f);

    assert(parser.parse("PAD 4 1"));
    assert(parser.parse("PAD 4 0"));
    assert(parser.parse("PAD 1 1"));
    assert(parser.parse("PAD 1 0"));
    const ControlState queued = store.snapshot();
    assert(queued.activationSequence == released.activationSequence + 2);
    const ActivationEvent &first = queued.activationEvents[
        released.activationSequence % kActivationEventCapacity];
    const ActivationEvent &second = queued.activationEvents[
        (released.activationSequence + 1) % kActivationEventCapacity];
    assert(first.mode == SirenMode::Syndrum);
    assert(second.mode == SirenMode::Sine1);
    engine->render(queued, block, 480);
    assert(blockRms(block, 480) > 100.0f);
}

void testPanelDisplayState() {
    ControlStore store;
    CommandParser parser(store);
    assert(padLayout(parser));
    assert(parser.displayWidth() == 320 && parser.displayHeight() == 172);
    assert(strcmp(parser.displayModeName(), "HOME") == 0);
    // Free rotation at Home steps the sound of the last pressed pad, and so
    // does the SOUND row of the menu.
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(parser.padVariant(1) == 1 && store.snapshot().mode == SirenMode::Sine2);
    assert(strcmp(parser.voiceName(), parser.padVoiceName(1)) == 0);
    stepSound(parser, -1);
    assert(parser.padVariant(1) == 0 && store.snapshot().mode == SirenMode::Sine1);
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(parser.padVariant(1) == 1);
    assert(store.snapshot().mode == SirenMode::Sine2);
    assert(store.snapshot().lfoDepthOctaves == 1.0f);

    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(strcmp(parser.displayModeName(), "HOME") == 0);
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(strcmp(parser.displayModeName(), "MENU") == 0);
    assert(parser.menuIndex() == 0 && !parser.menuEditing());
    assert(parser.menuItemCount() == 7);
    assert(strcmp(parser.menuTitle(), "MENU") == 0);
    // COMANDI comes first.
    assert(strcmp(parser.menuItemLabel(0), "COMANDI") == 0);
    auto click = [&]() {
        assert(parser.parse("ENCODER_SWITCH 1")); assert(parser.parse("ENCODER_SWITCH 0"));
    };
    auto is = [](const char *left, const char *right) { return strcmp(left, right) == 0; };
    click();
    // COMANDI is the map of the panel: six pots, four keys and ESCI. The
    // banner names the selected control and explains what it does.
    assert(parser.controlsMap() && parser.menuItemCount() == 11);
    assert(is(parser.menuItemKey(0), "POT_1") && is(parser.menuItemLabel(6), "TASTO 1"));
    assert(is(parser.menuItemValue(6), "SINE 2"));
    assert(parser.mapControl() == 1);
    assert(is(parser.menuTitle(), "POT 1 - PITCH"));
    assert(is(parser.menuFooter(), "NOTA: DA GRAVE AD ACUTO"));
    // ESCI, at the encoder's place, follows the keys and goes back to playing.
    assert(parser.parse("ENCODER_ROTATE PREV"));
    assert(parser.mapControl() == 11 && is(parser.menuTitle(), "ESCI"));
    click();
    assert(strcmp(parser.displayModeName(), "HOME") == 0);

    // VALORI POT, in MENU > ALTRO, switches every pot that has positions at
    // once; each pot keeps its own switch on its page.
    click();
    while (!is(parser.menuItemKey(parser.menuIndex()), "OTHER"))
        assert(parser.parse("ENCODER_ROTATE NEXT"));
    click();
    assert(is(parser.menuTitle(), "ALTRO") && is(parser.menuItemKey(0), "VALUES"));
    const unsigned values = 0;
    click();
    assert(parser.menuEditing());
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(is(parser.menuItemValue(values), "LOW/MID/HIGH"));
    assert(is(parser.menuFooter(), "PITCH E RATE A SCATTI"));
    assert(parser.panelSettings().pots[0].mode == PotMode::Zones);
    assert(parser.panelSettings().pots[1].mode == PotMode::Zones);
    assert(store.snapshot().pitchZonesActive && store.snapshot().rateZonesActive);
    assert(parser.parse("PANEL POT 2 MODE CONTINUOUS"));
    assert(is(parser.menuItemValue(values), "MISTI"));
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(is(parser.menuItemValue(values), "LOW/MID/HIGH"));
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(is(parser.menuItemValue(values), "LIBERI"));
    assert(parser.panelSettings().pots[0].mode == PotMode::Continuous);
    assert(!store.snapshot().pitchZonesActive && !store.snapshot().rateZonesActive);
    click();
    assert(!parser.menuEditing());
    assert(parser.setPhysicalPad(PerformanceController::kMenuBackButton, true));
    assert(parser.setPhysicalPad(PerformanceController::kMenuBackButton, false));
    assert(is(parser.menuTitle(), "MENU"));
    while (!is(parser.menuItemKey(parser.menuIndex()), "SETUP"))
        assert(parser.parse("ENCODER_ROTATE PREV"));
    click();
    assert(parser.controlsMap() && parser.mapControl() == 1);

    // A moved pot or a pressed key selects its cell; the key does not play.
    assert(parser.parse("POT 3 0.2"));  // Power-on scan: not a gesture.
    assert(parser.mapControl() == 1);
    assert(parser.parse("POT 3 0.4"));
    assert(parser.mapControl() == 3 && is(parser.menuTitle(), "POT 3 - ECHO LEVEL"));
    assert(is(parser.menuFooter(), "VOLUME DELL'ECO"));
    assert(parser.parse("POT 4 0.4"));  // Its power-on scan selects nothing.
    assert(parser.mapControl() == 3);
    assert(parser.parse("POT 4 0.5"));
    assert(parser.mapControl() == 4);
    const unsigned activations = store.snapshot().activationSequence;
    assert(parser.parse("PAD 2 1"));
    assert(!store.snapshot().trigger && store.snapshot().activationSequence == activations);
    assert(parser.parse("PAD 2 0"));
    assert(parser.mapControl() == 8 && is(parser.menuTitle(), "TASTO 2 - TWO TONE"));
    assert(is(parser.menuFooter(), "SUONA IL SUONO DEL TASTO"));

    // Push: the function is chosen in the cell, the banner explaining each
    // one; push again: its options open, BACK returns to the cell.
    while (parser.mapControl() != 1) assert(parser.parse("ENCODER_ROTATE PREV"));
    click();
    assert(parser.menuEditing() && is(parser.menuTitle(), "POT 1 - SCEGLI LA FUNZIONE"));
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(parser.panelSettings().pots[0].target == PotTarget::Rate);
    assert(is(parser.menuFooter(), "QUANTO VELOCE VA SU E GIU"));
    click();
    assert(!parser.controlsMap() && !parser.menuEditing());
    assert(is(parser.menuTitle(), "POT 1 - RATE") && is(parser.menuItemKey(0), "MODE"));

    // BTN4 is BACK in the menu: it ends editing, then climbs page by page to
    // Home, never playing or doing its own action.
    auto back = [&]() {
        assert(parser.setPhysicalPad(PerformanceController::kMenuBackButton, true));
        const ControlState held = store.snapshot();
        assert(!held.trigger && !held.echoCut && !held.hold);
        assert(parser.setPhysicalPad(PerformanceController::kMenuBackButton, false));
    };
    back();
    assert(parser.controlsMap() && parser.mapControl() == 1);
    // On the map BACK keeps the choice and skips the options.
    click();
    assert(parser.menuEditing());
    back();
    assert(parser.controlsMap() && !parser.menuEditing());
    assert(parser.panelSettings().pots[0].target == PotTarget::Rate);

    // A key: the action in its cell, then the sound of a pad on its page,
    // each choice explained.
    assert(parser.parse("PAD 1 1")); assert(parser.parse("PAD 1 0"));
    click();
    assert(is(parser.menuTitle(), "TASTO 1 - SCEGLI L'AZIONE"));
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(parser.panelSettings().buttons[0].action == ButtonAction::Hold);
    assert(is(parser.menuFooter(), "PREMI: RESTA ACCESO"));
    assert(parser.parse("ENCODER_ROTATE PREV"));
    click();
    assert(is(parser.menuTitle(), "TASTO 1 - PLAY SOUND") && is(parser.menuItemKey(0), "SOUND"));
    click();
    assert(is(parser.menuFooter(), "IMPULSO LARGO FILTRATO"));
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(is(parser.menuFooter(), "IMPULSO STRETTO, BRILLANTE"));
    click();
    back();
    assert(parser.controlsMap() && parser.mapControl() == 7);
    back();
    assert(is(parser.menuTitle(), "MENU"));
    back();
    assert(strcmp(parser.displayModeName(), "HOME") == 0);
}

void testPowerOnPotReading() {
    // Factory Rate is zoned and starts at MID: a pot already at SLOW when the
    // panel powers on takes over at once, without an overlay.
    ControlStore store;
    CommandParser parser(store);
    assert(store.snapshot().classicModulation == ClassicModulation::Medium);
    assert(parser.setPhysicalPot(2, 0.05f));
    assert(store.snapshot().classicModulation == ClassicModulation::Slow);
    assert(parser.overlayTitle()[0] == '\0');
    assert(parser.setPhysicalPot(2, 0.95f));
    assert(store.snapshot().classicModulation == ClassicModulation::Manual);

    // After a new voice the next turn applies at once and shows its value.
    assert(parser.setPhysicalPot(1, 0.5f));
    stepSound(parser, 1);
    assert(parser.setPhysicalPot(1, 0.95f));
    assert(store.snapshot().classicPitch == ClassicPitch::High);
    assert(strcmp(parser.overlayTitle(), "PITCH") == 0);
}

void testExtendedEncoderGestures() {
    ControlStore store;
    CommandParser parser(store);
    assert(padLayout(parser));
    // The SOUND row walks the selected pad through the one sound list and
    // wraps; a sound outside it (pad 3, set over serial) steps to the first.
    for (unsigned pad = 1; pad <= 4; ++pad) {
        char command[24];
        snprintf(command, sizeof(command), "PAD_SELECT %u", pad);
        assert(parser.parse(command));
        const float depth = store.snapshot().lfoDepthOctaves;
        const float tune = store.snapshot().tuneHz;
        const unsigned sequence = store.snapshot().activationSequence;
        stepSound(parser, 1);
        if (pad == 3) assert(strcmp(parser.padVoiceName(3), "SINE 1") == 0);
        const ButtonConfig first = parser.panelSettings().buttons[pad - 1];
        for (int step = 0; step < 6; ++step) {
            stepSound(parser, 1);
            const ButtonConfig &now = parser.panelSettings().buttons[pad - 1];
            assert((now.sound == first.sound && now.variant == first.variant) == (step == 5));
            assert(store.snapshot().lfoDepthOctaves == depth);
            assert(store.snapshot().tuneHz == tune);
            assert(!store.snapshot().modUp && !store.snapshot().modDown);
            assert(store.snapshot().activationSequence == sequence);
        }
    }

    assert(parser.parse("PAD 1 1"));
    const unsigned variant = parser.padVariant(1);
    const unsigned settingsRevision = parser.panelSettingsRevision();
    // A held key turns the encoder into a lever: with SIREN, one octave up.
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(parser.padVariant(1) == variant);
    assert(store.snapshot().modUp && !store.snapshot().modDown);
    assert(parser.panelSettingsRevision() == settingsRevision);
    assert(strcmp(parser.overlayTitle(), "LEVA") == 0);
    assert(strcmp(parser.overlayValue(), "+1 OTTAVA") == 0);
    assert(parser.parse("PAD 1 0"));
    assert(!store.snapshot().modUp);
    // With no key held the rotation steps the sound instead.
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(strcmp(parser.padVoiceName(1), "TWO TONE") == 0);

    // A latched sound is not a physically held button.
    assert(parser.parse("PANEL BUTTON 1 SOUND CLASSIC 0 1"));
    assert(store.snapshot().hold);
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(parser.padVariant(1) == 1 && !store.snapshot().modUp);

    // Other sound buttons have the lever too.
    assert(parser.parse("PAD 2 1"));
    assert(parser.parse("ENCODER_ROTATE PREV"));
    assert(store.snapshot().modDown && !store.snapshot().modUp);
    assert(parser.parse("PAD 2 0"));
    assert(!store.snapshot().modDown);
}

void testDynamicSweep() {
    ControlState state;
    padState(state);
    state.mode = SirenMode::Sine1;
    state.tuneHz = 220.0f;
    state.lfoDepthOctaves = 0.0f;
    state.modulationEnabled = false;
    state.echoLevel = 0.0f;
    state.masterVolume = 0.8f;
    state.decayMs = 1000.0f;
    state.trigger = true;

    auto noSweep = std::make_unique<AudioEngine>();
    noSweep->begin();
    int16_t settled[4800];
    for (int block = 0; block < 10; ++block) {
        noSweep->render(state, settled + block * 480, 480);
    }
    const float noSweepFrequency = estimateFrequency(settled, 4800);
    assert(noSweepFrequency > 215.0f && noSweepFrequency < 225.0f);

    state.sweepAmount = 1.0f;
    state.sweepMs = 650.0f;
    auto swept = std::make_unique<AudioEngine>();
    swept->begin();
    int16_t early[4800];
    for (int block = 0; block < 10; ++block) {
        swept->render(state, early + block * 480, 480);
    }
    const float earlyFrequency = estimateFrequency(early, 4800);
    assert(earlyFrequency < noSweepFrequency * 0.8f);
    int16_t block[480];
    for (int i = 0; i < 300; ++i) swept->render(state, block, 480);
    int16_t late[4800];
    for (int i = 0; i < 10; ++i) {
        swept->render(state, late + i * 480, 480);
    }
    const float lateFrequency = estimateFrequency(late, 4800);
    assert(lateFrequency > 205.0f && lateFrequency < 225.0f);
    state.trigger = false;
    swept->render(state, block, 480);
    state.trigger = true;
    int16_t retriggered[4800];
    for (int i = 0; i < 10; ++i) {
        swept->render(state, retriggered + i * 480, 480);
    }
    assert(estimateFrequency(retriggered, 4800) < lateFrequency * 0.8f);

    OneShotEnvelope rateSweep;
    rateSweep.trigger();
    float level = rateSweep.next(650.0f, kRate);
    const float initialRate = 4.0f * exp2f(
        -level * PerformanceConfig::kSweepRateOctaves);
    for (int i = 0; i < 240000; ++i) {
        level = rateSweep.next(650.0f, kRate);
    }
    const float convergedRate = 4.0f * exp2f(
        -level * PerformanceConfig::kSweepRateOctaves);
    assert(initialRate < 0.6f && convergedRate > 3.95f);
    rateSweep.trigger();
    level = rateSweep.next(650.0f, kRate);
    assert(4.0f * exp2f(-level * PerformanceConfig::kSweepRateOctaves) < 0.6f);
}

void testSyndrum() {
    ControlState state;
    padState(state);
    state.mode = SirenMode::Syndrum;
    state.tuneHz = 220.0f;
    state.decayMs = 120.0f;
    state.echoLevel = 0.0f;
    state.masterVolume = 0.8f;
    auto engine = std::make_unique<AudioEngine>();
    engine->begin();
    int16_t block[480];
    engine->render(state, block, 480);
    assert(blockRms(block, 480) == 0.0f);
    state.hold = true;
    engine->render(state, block, 480);
    assert(blockRms(block, 480) == 0.0f);
    state.hold = false;
    state.trigger = true;
    engine->render(state, block, 480);
    assert(blockRms(block, 480) > 100.0f);
    for (int i = 0; i < 200; ++i) engine->render(state, block, 480);
    assert(blockRms(block, 480) < 2.0f);
    state.trigger = false;
    engine->render(state, block, 480);
    state.trigger = true;
    engine->render(state, block, 480);
    assert(blockRms(block, 480) > 100.0f);

    float sweptFrequency[3];
    float convergedFrequency[3];
    int sweepIndex = 0;
    for (float sweep : {2.0f, -2.0f, 0.0f}) {
        auto voice = std::make_unique<AudioEngine>();
        voice->begin();
        ControlState pitch = state;
        pitch.trigger = true;
        pitch.hold = false;
        pitch.decayMs = 1000.0f;
        pitch.syndrumWave = SyndrumWave::Sine;
        pitch.syndrumSweepOctaves = sweep;
        int16_t capture[4800];
        for (int i = 0; i < 10; ++i) {
            voice->render(pitch, capture + i * 480, 480);
        }
        sweptFrequency[sweepIndex] = estimateFrequency(capture, 4800);
        int16_t settle[480];
        for (int i = 0; i < 200; ++i) voice->render(pitch, settle, 480);
        for (int i = 0; i < 10; ++i) {
            voice->render(pitch, capture + i * 480, 480);
        }
        convergedFrequency[sweepIndex++] = estimateFrequency(capture, 4800);
    }
    assert(sweptFrequency[0] > 500.0f);
    assert(sweptFrequency[1] < 100.0f);
    assert(sweptFrequency[2] > 215.0f && sweptFrequency[2] < 225.0f);
    assert(sweptFrequency[0] > convergedFrequency[0]);
    assert(sweptFrequency[1] < convergedFrequency[1]);
    assert(convergedFrequency[2] > 215.0f && convergedFrequency[2] < 225.0f);

    int16_t waveforms[3][480];
    int waveIndex = 0;
    for (SyndrumWave wave : {SyndrumWave::Sine, SyndrumWave::Triangle,
                             SyndrumWave::Square}) {
        auto voice = std::make_unique<AudioEngine>();
        voice->begin();
        ControlState waveform = state;
        waveform.trigger = true;
        waveform.decayMs = 500.0f;
        waveform.syndrumSweepOctaves = 0.0f;
        waveform.syndrumWave = wave;
        voice->render(waveform, waveforms[waveIndex++], 480);
    }
    assert(memcmp(waveforms[0], waveforms[1], sizeof(waveforms[0])) != 0);
    assert(memcmp(waveforms[0], waveforms[2], sizeof(waveforms[0])) != 0);

    // DECAY 0 from the menu still yields a short hit, like Gunshot.
    auto shortHit = std::make_unique<AudioEngine>();
    shortHit->begin();
    ControlState zeroDecay = state;
    zeroDecay.trigger = true;
    zeroDecay.decayMs = 0.0f;
    shortHit->render(zeroDecay, block, 480);
    assert(blockRms(block, 480) > 100.0f);
}

void testGunshot() {
    ControlState state;
    padState(state);
    state.mode = SirenMode::Gunshot;
    state.decayMs = 100.0f;
    state.echoLevel = 0.0f;
    state.masterVolume = 0.8f;
    auto engine = std::make_unique<AudioEngine>();
    engine->begin();
    int16_t block[480];
    engine->render(state, block, 480);
    assert(blockRms(block, 480) == 0.0f);
    state.hold = true;
    engine->render(state, block, 480);
    assert(blockRms(block, 480) == 0.0f);
    state.hold = false;
    state.trigger = true;
    engine->render(state, block, 480);
    assert(blockRms(block, 480) > 100.0f);
    assert(memcmp(block, block + 240, 240 * sizeof(int16_t)) != 0);
    for (int i = 0; i < 200; ++i) engine->render(state, block, 480);
    assert(blockRms(block, 480) < 2.0f);
    state.trigger = false;
    engine->render(state, block, 480);
    state.trigger = true;
    engine->render(state, block, 480);
    assert(blockRms(block, 480) > 100.0f);
}

void testPotsFollowSound() {
    // A one-shot has no LFO: its Rate pot sets the Decay instead, and the
    // knob keeps owning its value across a change of sound.
    ControlStore store;
    CommandParser parser(store);
    assert(parser.setPhysicalPot(2, 0.05f));  // Power-on scan: SLOW.
    assert(store.snapshot().classicModulation == ClassicModulation::Slow);
    assert(parser.potConfig(2).target == PotTarget::Rate);
    assert(parser.parse("PAD 3 1")); assert(parser.parse("PAD 3 0"));
    assert(store.snapshot().mode == SirenMode::Gunshot);
    assert(parser.potConfig(2).target == PotTarget::Decay);
    // Taken from where the pot already is, never below the one-shot default.
    assert(store.snapshot().decayMs >= PerformanceConfig::kExtendedDecayMs);
    assert(store.snapshot().decayMs < 150.0f);
    assert(parser.setPhysicalPot(2, 0.8f));
    assert(strcmp(parser.overlayTitle(), "DECAY") == 0);
    assert(store.snapshot().decayMs > 900.0f);
    assert(store.snapshot().classicModulation == ClassicModulation::Slow);
    // Back on a siren the same pot is Rate again, at its new position.
    assert(parser.parse("PAD 1 1")); assert(parser.parse("PAD 1 0"));
    assert(parser.potConfig(2).target == PotTarget::Rate);
    assert(store.snapshot().classicModulation == ClassicModulation::Manual);
    assert(store.snapshot().decayMs == 0.0f);

    // Sonar has three Rate zones: the top of the travel is FAST, not MANUAL.
    assert(parser.parse("PANEL BUTTON 1 SOUND CLASSIC 2 0"));
    assert(store.snapshot().mode == SirenMode::Sonar);
    assert(store.snapshot().classicModulation == ClassicModulation::Fast);
    assert(parser.setPhysicalPot(2, 0.98f));
    assert(store.snapshot().classicModulation == ClassicModulation::Fast);
    assert(parser.setPhysicalPot(2, 0.5f));
    assert(store.snapshot().classicModulation == ClassicModulation::Medium);

    // Pitch sizes the gunshot instead of doing nothing.
    int16_t shots[2][480];
    unsigned index = 0;
    for (float tune : {330.0f, 1000.0f}) {
        ControlState state;
        state.mode = SirenMode::Gunshot;
        state.decayMs = 120.0f;
        state.tuneHz = tune;
        state.echoLevel = 0.0f;
        state.trigger = true;
        auto engine = std::make_unique<AudioEngine>();
        engine->begin();
        engine->render(state, shots[index++], 480);
    }
    assert(memcmp(shots[0], shots[1], sizeof(shots[0])) != 0);
}

void testSonarIsNeverManual() {
    // Sonar is its LFO: with Rate on MANUAL, or the MANUAL shape, it keeps
    // pinging at the last rate instead of going silent.
    ControlState state;
    state.mode = SirenMode::Sonar;
    state.classicModulation = ClassicModulation::Manual;
    state.rateZonesActive = true;
    assert(effectiveLfoShape(state) == LfoShape::Classic);
    state.lfoShape = LfoShape::Manual;
    assert(effectiveLfoShape(state) == LfoShape::Classic);
    state.lfoShape = LfoShape::Classic;
    state.lfoRateHz = 4.0f;
    state.echoLevel = 0.0f;
    state.masterVolume = 0.8f;
    state.hold = true;
    auto engine = std::make_unique<AudioEngine>();
    engine->begin();
    int16_t block[480];
    int pings = 0;
    bool sounding = false;
    for (int i = 0; i < 100; ++i) {  // One second: four cycles.
        engine->render(state, block, 480);
        const bool loud = blockRms(block, 480) > 100.0f;
        if (loud && !sounding) ++pings;
        sounding = loud;
    }
    assert(pings >= 3 && pings <= 5);
    // The other voices still go manual on that zone.
    state.mode = SirenMode::Sine1;
    assert(effectiveLfoShape(state) == LfoShape::Manual);
}

void testLfoShape() {
    Lfo lfo;
    float previous = 0.0f;
    for (int i = 0; i < 3000; ++i) previous = lfo.next(LfoShape::Classic, 0.7f, false, false, kRate);
    for (LfoShape shape : {LfoShape::Square, LfoShape::Triangle,
                           LfoShape::SawDown, LfoShape::Manual}) {
        const float first = lfo.next(shape, 0.7f, false, false, kRate);
        assert(isfinite(first) && fabsf(first - previous) < 0.02f);
        previous = first;
    }
}

void testLfoReset() {
    for (LfoShape shape : {LfoShape::Classic, LfoShape::Triangle,
                           LfoShape::SawDown, LfoShape::Manual}) {
        Lfo first;
        Lfo second;
        for (int i = 0; i < 1379; ++i) {
            first.next(LfoShape::AsymUp, 3.7f, true, false, kRate);
        }
        first.reset(shape);
        second.reset(shape);
        for (int i = 0; i < 2000; ++i) {
            const bool up = shape == LfoShape::Manual && i > 500;
            const float a = first.next(shape, 0.7f, up, false, kRate);
            const float b = second.next(shape, 0.7f, up, false, kRate);
            assert(isfinite(a) && isfinite(b));
            assert(fabsf(a - b) < 0.000001f);
        }
    }
}

void testNewVoices() {
    for (SirenMode mode : {SirenMode::Lickshot, SirenMode::Sonar,
                           SirenMode::Saw, SirenMode::Triangle}) {
        Oscillator oscillator;
        double energy = 0.0;
        int crossings = 0;
        float previous = 0.0f;
        for (int i = 0; i < 4800; ++i) {
            const float value = oscillator.next(mode, 220.0f, 0.25f, 1.0f,
                                                true, kRate);
            assert(isfinite(value));
            energy += value * value;
            if (previous <= 0.0f && value > 0.0f) ++crossings;
            previous = value;
        }
        assert(energy > 100.0);
        if (mode == SirenMode::Sonar || mode == SirenMode::Saw ||
            mode == SirenMode::Triangle) {
            assert(crossings >= 21 && crossings <= 23);
        }
    }

    const HarmonicSnapshot saw = measureVoice(SirenMode::Saw, 0.0f);
    const HarmonicSnapshot triangle = measureVoice(SirenMode::Triangle, 0.0f);
    assert(saw.rms > 0.30f && saw.rms < 0.50f);
    assert(triangle.rms > 0.30f && triangle.rms < 0.50f);
    assert(fabsf(saw.mean) < 0.001f);
    assert(fabsf(triangle.mean) < 0.001f);
    // Saw contains both even and odd harmonics; an ideal triangle suppresses
    // even harmonics and its third harmonic is approximately one ninth of H1.
    assert(harmonicRatio(saw, 2) > 0.35f);
    assert(harmonicRatio(saw, 3) > 0.20f);
    assert(harmonicRatio(triangle, 2) < 0.02f);
    assert(harmonicRatio(triangle, 3) > 0.07f);
    assert(harmonicRatio(triangle, 3) < 0.16f);
}

void testPerformancePitch() {
    ControlState state;
    padState(state);
    state.mode = SirenMode::Sine1;
    state.tuneHz = 220.0f;
    state.lfoShape = LfoShape::SawDown;
    state.lfoRateHz = 4.0f;
    state.lfoDepthOctaves = 1.0f;
    state.modulationEnabled = false;
    const float steady = renderFrequency(state);
    assert(steady > 218.0f && steady < 222.0f);
    {
        auto engine = std::make_unique<AudioEngine>();
        engine->begin();
        state.hold = true;
        state.echoLevel = 0.0f;
        int16_t block[480];
        for (int i = 0; i < 100; ++i) engine->render(state, block, 480);
        float observed[4];
        for (int segment = 0; segment < 4; ++segment) {
            int16_t capture[4800];
            for (int i = 0; i < 10; ++i) {
                engine->render(state, capture + i * 480, 480);
            }
            observed[segment] = estimateFrequency(capture, 4800);
        }
        for (float frequency : observed) {
            assert(frequency > 215.0f && frequency < 225.0f);
        }
    }

    state.lfoDepthOctaves = 0.0f;
    state.modulationEnabled = true;
    state.modUp = true;
    const float rise = renderFrequency(state);
    state.modUp = false;
    state.modDown = true;
    const float dive = renderFrequency(state);
    state.modUp = true;
    const float centered = renderFrequency(state);
    assert(rise > 435.0f && rise < 445.0f);
    assert(dive > 108.0f && dive < 112.0f);
    assert(centered > 218.0f && centered < 222.0f);

    state.lfoDepthOctaves = 1.0f;
    state.lfoShape = LfoShape::Manual;
    state.modDown = false;
    const float manualUp = renderFrequency(state);
    state.modUp = false;
    state.modDown = true;
    const float manualDown = renderFrequency(state);
    // MANUAL ignores Depth: Up holds the selected pitch, Down sinks to the floor.
    assert(manualUp > 218.0f && manualUp < 222.0f);
    assert(manualDown < 60.0f);
}

void testSonarPulses() {
    for (bool modulationEnabled : {false, true}) {
        for (float rate : {1.0f, 4.0f}) {
            auto engine = std::make_unique<AudioEngine>();
            engine->begin();
            ControlState state;
            padState(state);
            state.mode = SirenMode::Sonar;
            state.tuneHz = 375.0f;
            state.lfoDepthOctaves = 0.0f;
            state.lfoRateHz = rate;
            state.modulationEnabled = modulationEnabled;
            state.hold = true;
            state.echoLevel = 0.0f;
            int16_t block[480];
            int pings = 0, silentBlocks = 0;
            bool audible = false;
            float peak = 0.0f;
            for (int i = 0; i < 300; ++i) {
                engine->render(state, block, 480);
                const float rms = blockRms(block, 480);
                if (rms > 100.0f && !audible) ++pings;
                audible = rms > 100.0f;
                if (rms == 0.0f) ++silentBlocks;
                if (i < 10 && rms > peak) peak = rms;
                if (rate == 1.0f && i == 25) assert(rms < peak * 0.1f);
                for (int16_t sample : block) assert(abs(int(sample)) < 32767);
            }
            const int expected = static_cast<int>(3.0f * rate);
            assert(pings >= expected - 1 && pings <= expected + 1);
            assert(silentBlocks >= 100);
            printf("Sonar %.1f Hz, pitch modulation %s: %d pings, %d silent blocks\n",
                   rate, modulationEnabled ? "on" : "off", pings, silentBlocks);
            state.hold = false;
            state.decayMs = 0.0f;
            engine->render(state, block, 480);
            assert(blockRms(block, 480) == 0.0f);
        }
    }

    // A new trigger or queued tap in a silent part of the cycle starts a ping
    // immediately, even with the optional pitch retrigger disabled.
    auto engine = std::make_unique<AudioEngine>();
    engine->begin();
    ControlState state;
    padState(state);
    state.mode = SirenMode::Sonar;
    state.lfoRateHz = 1.0f;
    state.lfoDepthOctaves = 0.0f;
    state.echoLevel = 0.0f;
    state.trigger = true;
    int16_t block[480];
    for (int i = 0; i < 70; ++i) engine->render(state, block, 480);
    assert(blockRms(block, 480) == 0.0f);
    recordActivation(state);
    engine->render(state, block, 480);
    assert(blockRms(block, 480) > 1000.0f);
    for (int i = 0; i < 70; ++i) engine->render(state, block, 480);
    state.trigger = false;
    engine->render(state, block, 480);
    state.trigger = true;
    engine->render(state, block, 480);
    assert(blockRms(block, 480) > 1000.0f);

    // Selecting Sonar from a continuous voice also starts with an audible ping.
    state.mode = SirenMode::Sine1;
    for (int i = 0; i < 70; ++i) engine->render(state, block, 480);
    state.mode = SirenMode::Sonar;
    engine->render(state, block, 480);
    assert(blockRms(block, 480) > 1000.0f);
}

void testModeTransitionStartsContinuously() {
    Oscillator oldOnly;
    Oscillator transitioning;
    for (int i = 0; i < 1000; ++i) {
        oldOnly.next(SirenMode::Sine1, 220.0f, 0.0f, 1.0f,
                     true, kRate);
        transitioning.next(SirenMode::Sine1, 220.0f, 0.0f, 1.0f,
                           true, kRate);
    }
    const float expected = oldOnly.next(SirenMode::Sine1, 220.0f, 0.0f,
                                        1.0f, true, kRate);
    const float actual = transitioning.nextTransition(
        SirenMode::Sine1, SirenMode::Square, 0.0f, 220.0f, 0.0f,
        1.0f, true, kRate);
    assert(isfinite(actual) && fabsf(actual - expected) < 0.000001f);
}

void testSine1DynamicVoicing() {
    const HarmonicSnapshot low = measureSine1(-1.0f);
    const HarmonicSnapshot center = measureSine1(0.0f);
    const HarmonicSnapshot high = measureSine1(1.0f);
    const float lowH2 = harmonicRatio(low, 2);
    const float centerH2 = harmonicRatio(center, 2);
    const float highH2 = harmonicRatio(high, 2);

    assert(lowH2 > centerH2 && centerH2 > highH2);
    assert(lowH2 > 0.45f);
    assert(highH2 < 0.30f);
    assert(lowH2 - highH2 > 0.30f);

    const float thirdNotchModulation =
        (1.0f / 3.0f - PerformanceConfig::kSine1Voicing.duty) /
        PerformanceConfig::kSine1Voicing.dutyModAmount;
    const HarmonicSnapshot thirdNotch = measureSine1(thirdNotchModulation);
    const HarmonicSnapshot thirdBelow = measureSine1(
        (0.290f - PerformanceConfig::kSine1Voicing.duty) /
        PerformanceConfig::kSine1Voicing.dutyModAmount);
    const HarmonicSnapshot thirdAbove = measureSine1(
        (0.380f - PerformanceConfig::kSine1Voicing.duty) /
        PerformanceConfig::kSine1Voicing.dutyModAmount);
    assert(harmonicRatio(thirdNotch, 3) * 1.5f <
           harmonicRatio(thirdBelow, 3));
    assert(harmonicRatio(thirdNotch, 3) * 1.5f <
           harmonicRatio(thirdAbove, 3));

    // The configured 0.27 lower limit is the closest production state to the
    // ideal quarter-duty H4 notch; compare it with nearby reachable duties.
    const HarmonicSnapshot fourthNotch = measureSine1(-1.0f);
    const HarmonicSnapshot fourthAbove1 = measureSine1(
        (0.310f - PerformanceConfig::kSine1Voicing.duty) /
        PerformanceConfig::kSine1Voicing.dutyModAmount);
    const HarmonicSnapshot fourthAbove2 = measureSine1(
        (0.340f - PerformanceConfig::kSine1Voicing.duty) /
        PerformanceConfig::kSine1Voicing.dutyModAmount);
    assert(harmonicRatio(fourthNotch, 4) * 1.4f <
           harmonicRatio(fourthAbove1, 4));
    assert(harmonicRatio(fourthNotch, 4) * 1.4f <
           harmonicRatio(fourthAbove2, 4));
}

void testSine2DynamicVoicing() {
    const float modulations[] = {-1.0f, 0.0f, 1.0f};
    HarmonicSnapshot sine1[3];
    HarmonicSnapshot sine2[3];
    int moreExposedStates = 0;
    int thinnerStates = 0;

    puts("voice modulation duty H2/H1 H3/H1 H4/H1 H5/H1 THD2-5 RMS DC peak");
    for (int state = 0; state < 3; ++state) {
        sine1[state] = measureVoice(SirenMode::Sine1, modulations[state]);
        sine2[state] = measureVoice(SirenMode::Sine2, modulations[state]);
        for (int voice = 0; voice < 2; ++voice) {
            const HarmonicSnapshot &snapshot =
                voice == 0 ? sine1[state] : sine2[state];
            const float duty = voice == 0
                ? PerformanceConfig::kSine1Voicing.duty +
                      modulations[state] *
                          PerformanceConfig::kSine1Voicing.dutyModAmount
                : sine2Duty(modulations[state]);
            printf("SINE%d %+.0f %.3f %.4f %.4f %.4f %.4f %.4f %.4f %+.6f %.4f\n",
                   voice + 1, modulations[state], duty,
                   harmonicRatio(snapshot, 2), harmonicRatio(snapshot, 3),
                   harmonicRatio(snapshot, 4), harmonicRatio(snapshot, 5),
                   harmonicEnergyRatio(snapshot), snapshot.rms,
                   snapshot.mean, snapshot.peak);
        }

        if (harmonicEnergyRatio(sine2[state]) >
            harmonicEnergyRatio(sine1[state]) * 1.10f) {
            ++moreExposedStates;
        }
        if (sine2[state].rms < sine1[state].rms) ++thinnerStates;
        assert(sine2[state].rms > sine1[state].rms * 0.65f);
        assert(sine2[state].rms < sine1[state].rms * 1.10f);
        assert(fabsf(sine2[state].mean) < 0.001f);
        assert(sine2[state].peak < 1.5f);
    }
    assert(moreExposedStates >= 2);
    assert(thinnerStates >= 2);

    float largestRatioChange = 0.0f;
    for (int harmonic = 2; harmonic <= 4; ++harmonic) {
        const float change = fabsf(harmonicRatio(sine2[0], harmonic) -
                                   harmonicRatio(sine2[2], harmonic));
        if (change > largestRatioChange) largestRatioChange = change;
    }
    assert(largestRatioChange > 0.15f);
}

void testSine2Stability() {
    for (float frequency : PerformanceConfig::kClassicPitchHz) {
        for (float modulation : {-1.0f, 0.0f, 1.0f}) {
            Oscillator oscillator;
            for (int i = 0; i < 24000; ++i) {
                oscillator.next(SirenMode::Sine2, frequency, modulation, 1.0f,
                                true, kRate);
            }
            double sum = 0.0;
            float peak = 0.0f;
            for (int i = 0; i < 48000; ++i) {
                const float value = oscillator.next(
                    SirenMode::Sine2, frequency, modulation, 1.0f,
                    true, kRate);
                assert(isfinite(value));
                sum += value;
                if (fabsf(value) > peak) peak = fabsf(value);
            }
            assert(fabs(sum / 48000.0) < 0.001);
            assert(peak < 1.5f);
        }
    }
}

void testSine1SafetyDcAndGain() {
    for (float frequency : {PerformanceConfig::kClassicPitchHz[2], 587.33f}) {
        for (float modulation : {-1.0f, 1.0f}) {
            Oscillator oscillator;
            for (int i = 0; i < 96000; ++i) {
                const float value = oscillator.next(
                    SirenMode::Sine1, frequency, modulation, 1.0f,
                    true, kRate);
                assert(isfinite(value) && fabsf(value) < 2.0f);
            }
        }
    }

    Oscillator oscillator;
    for (int i = 0; i < 24000; ++i) {
        oscillator.next(SirenMode::Sine1, 375.0f, -1.0f, 1.0f,
                        true, kRate);
    }
    double mean = 0.0;
    for (int i = 0; i < 96000; ++i) {
        mean += oscillator.next(SirenMode::Sine1, 375.0f, -1.0f, 1.0f,
                                true, kRate);
    }
    assert(fabs(mean / 96000.0) < 0.001f);

    const HarmonicSnapshot sine1 = measureSine1(0.0f);
    const HarmonicSnapshot sine2 = measureVoice(SirenMode::Sine2, 0.0f);
    assert(sine1.rms > sine2.rms * 0.70f);
    assert(sine1.rms < sine2.rms * 1.35f);
}

void testDelay() {
    auto delay = std::make_unique<Delay>();
    delay->begin(360.0f, 60.0f, 7000.0f, kRate);
    for (float feedback : {0.0f, 0.5f, 0.9f, 1.0f, 1.05f}) {
        delay->setParameters(360.0f, 60.0f, 7000.0f, kRate);
        for (int i = 0; i < 96000; ++i) {
            const float dry = i % 1000 < 500 ? 0.4f : -0.4f;
            const float wet = delay->process(dry, feedback, false, kRate);
            assert(isfinite(wet) && fabsf(wet) < 8.0f);
        }
    }
    for (float target : {100.0f, 800.0f, 100.0f}) {
        delay->setParameters(target, 50.0f, 19000.0f, kRate);
        for (int i = 0; i < 48000; ++i) {
            if (i == 24000) delay->setParameters(target, 7000.0f, 200.0f, kRate);
            const float wet = delay->process(0.2f,
                i < 24000 ? 0.5f : 1.05f, false, kRate);
            assert(isfinite(wet) && fabsf(wet) < 8.0f);
        }
    }
}

void testEchoLoop() {
    auto delay = std::make_unique<Delay>();
    delay->begin(50.0f, 50.0f, 19000.0f, kRate);
    for (int i = 0; i < 2400; ++i) {
        const float dry = 0.35f * sinf(TWO_PI * 220.0f * i / kRate);
        delay->process(dry, 0.0f, false, kRate);
    }
    double energy = 0.0;
    for (int i = 0; i < 24000; ++i) {
        const float wet = delay->process(0.0f, 1.05f, true, kRate);
        assert(isfinite(wet) && fabsf(wet) < 2.0f);
        if (i >= 21600) energy += wet * wet;
    }
    assert(energy > 1.0);
}

void testFractionalEchoLoopKeepsHighs() {
    // 223.7 ms is not a whole number of samples: the frozen loop must not
    // be low-passed again on every pass.
    auto delay = std::make_unique<Delay>();
    delay->begin(223.7f, 50.0f, 19000.0f, kRate);
    for (int i = 0; i < 12000; ++i) {
        delay->process(0.35f * sinf(TWO_PI * 4000.0f * i / kRate), 0.0f, false, kRate);
    }
    double first = 0.0, last = 0.0;
    constexpr int kLoopSamples = 20 * 48000;
    for (int i = 0; i < kLoopSamples; ++i) {
        const float wet = delay->process(0.0f, 0.0f, true, kRate);
        if (i < 4800) first += wet * wet;
        if (i >= kLoopSamples - 4800) last += wet * wet;
    }
    const double changeDb = 10.0 * log10(last / first);
    assert(fabs(changeDb) < 1.0);
}

void testEchoCutPreservesLoop() {
    auto engine = std::make_unique<AudioEngine>();
    engine->begin();
    ControlState state;
    padState(state);
    state.mode = SirenMode::Sonar;
    state.hold = true;
    state.lfoRateHz = 0.7f;  // Keep a ping inside the 50 ms loop.
    state.lfoDepthOctaves = 0.0f;
    state.delayMs = 50.0f;
    state.feedback = 0.0f;
    state.echoLevel = 1.0f;
    state.highPassHz = 50.0f;
    state.lowPassHz = 19000.0f;
    int16_t block[480];
    for (int i = 0; i < 20; ++i) engine->render(state, block, 480);
    state.hold = false;
    state.decayMs = 0.0f;
    state.echoLoop = true;
    for (int i = 0; i < 10; ++i) engine->render(state, block, 480);
    const float beforeCut = blockRms(block, 480);
    state.echoCut = true;
    engine->render(state, block, 480);
    const float duringCut = blockRms(block, 480);
    for (int i = 96; i < 480; ++i) assert(block[i] == 0);
    assert(duringCut < beforeCut * 0.5f);
    state.echoCut = false;
    engine->render(state, block, 480);
    assert(blockRms(block, 480) > duringCut);
}

void testEngine() {
    auto engine = std::make_unique<AudioEngine>();
    engine->begin();
    ControlState state;
    padState(state);
    state.hold = true;
    state.feedback = 0.95f;
    int16_t block[480];
    for (int turn = 0; turn < 500; ++turn) {
        state.rateZonesActive = turn % 80 < 40;
        state.decayMs = turn % 80 < 40 ? 0.0f : 120.0f;
        state.classicPitch = static_cast<ClassicPitch>(turn % 3);
        state.classicModulation = static_cast<ClassicModulation>(turn % 4);
        state.mode = static_cast<SirenMode>(
            turn % static_cast<int>(SirenMode::Count));
        state.lfoShape = static_cast<LfoShape>(turn % 10);
        state.delayMs = turn % 2 ? 100.0f : 800.0f;
        state.highPassHz = turn % 2 ? 50.0f : 7000.0f;
        state.lowPassHz = turn % 2 ? 19000.0f : 200.0f;
        state.echoSpin = turn % 7 == 0;
        state.echoLoop = turn % 11 == 0;
        engine->render(state, block, 480);
        for (int16_t sample : block) assert(sample >= -32767 && sample <= 32767);
    }
}

void testModeChangeKeepsTail() {
    auto engine = std::make_unique<AudioEngine>();
    engine->begin();
    ControlState state;
    padState(state);
    state.hold = true;
    state.decayMs = 0.0f;
    state.feedback = 0.8f;
    int16_t block[480];
    for (int i = 0; i < 100; ++i) engine->render(state, block, 480);
    state.hold = false;
    for (int i = 0; i < 20; ++i) engine->render(state, block, 480);
    state.mode = SirenMode::Square;
    bool heardTail = false;
    for (int i = 0; i < 30; ++i) {
        engine->render(state, block, 480);
        for (int16_t sample : block) heardTail |= sample != 0;
    }
    assert(heardTail);
}


void testClassicControls() {
    ControlStore store;
    CommandParser parser(store);
    assert(classicKeys(parser));
    // BTN1 is a Trigger here, not a configurable sound pad.
    assert(!parser.parse("PAD_CONFIG 1 LICKSHOT 0 0"));
    // The Trigger voice walks the one sound list of the panel and wraps.
    for (SirenMode voice : {SirenMode::Sine1, SirenMode::Sine2, SirenMode::Square,
                            SirenMode::Sonar, SirenMode::Syndrum, SirenMode::Gunshot}) {
        assert(store.snapshot().mode == voice);
        if (voice == SirenMode::Sonar) assert(strcmp(parser.voiceName(), "SONAR") == 0);
        // The banner names the sound as the keys and the menu do.
        if (voice == SirenMode::Square) assert(strcmp(parser.voiceName(), "TWO TONE") == 0);
        if (voice == SirenMode::Syndrum) assert(strcmp(parser.voiceName(), "PING") == 0);
        stepSound(parser, 1);
        assert(store.snapshot().lfoDepthOctaves == 1.0f);
        assert(fabsf(store.snapshot().tuneHz - PerformanceConfig::kClassicPitchHz[1]) < .001f);
    }
    assert(store.snapshot().mode == SirenMode::Sine1);
    stepSound(parser, -1);
    assert(store.snapshot().mode == SirenMode::Gunshot);
    assert(store.snapshot().decayMs == PerformanceConfig::kExtendedDecayMs);
    stepSound(parser, 1);
    assert(store.snapshot().mode == SirenMode::Sine1 && store.snapshot().decayMs == 0.0f);
    assert(parser.parse("PAD 1 1"));
    assert(store.snapshot().trigger);
    assert(parser.parse("PAD 2 1"));
    assert(parser.parse("PAD 2 1")); // Autorepeat must not toggle twice.
    assert(store.snapshot().hold);
    assert(parser.parse("PAD 1 0"));
    assert(!store.snapshot().trigger && store.snapshot().hold);
    assert(parser.parse("PAD 2 0"));
    assert(parser.parse("PAD 2 1"));
    assert(!store.snapshot().hold);
    // BTN3 bends only in MANUAL; with the SIREN shape it taps the tempo.
    assert(parser.modUpTaps());
    assert(parser.parse("PAD 3 1"));
    assert(!store.snapshot().modUp);
    assert(strcmp(parser.overlayTitle(), "TAP TEMPO") == 0);
    assert(parser.parse("PAD 3 0"));
    assert(parser.parse("SET CLASSIC_MOD MANUAL"));
    assert(!parser.modUpTaps());
    assert(parser.parse("PAD 3 1"));
    assert(store.snapshot().modUp);
    assert(parser.parse("PAD 4 1"));
    assert(store.snapshot().echoCut);
    assert(parser.parse("PANEL_RESET"));
    assert(!store.snapshot().modUp && !store.snapshot().echoCut);
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(strcmp(parser.displayModeName(), "HOME") == 0);
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(strcmp(parser.displayModeName(), "MENU") == 0);
    assert(parser.parse("ENCODER_SWITCH 1")); assert(parser.parse("ENCODER_SWITCH 0"));
    assert(strcmp(parser.menuItemKey(0), "POT_1") == 0);
}

void testClassicCalibration() {
    constexpr float expected[] = {330.0f, 680.0f, 1000.0f};
    constexpr float tolerance[] = {3.0f, 5.0f, 8.0f};
    float observed[3];
    ControlState state;
    state.mode = SirenMode::Sine1;
    state.classicModulation = ClassicModulation::Manual;
    state.modUp = true;  // The selected pitch is the top of the manual sweep.

    for (int i = 0; i < 3; ++i) {
        assert(fabsf(PerformanceConfig::kClassicPitchHz[i] - expected[i]) <
               0.001f);
        state.tuneHz = PerformanceConfig::kClassicPitchHz[i];
        observed[i] = renderFrequency(state);
        assert(fabsf(observed[i] - expected[i]) <= tolerance[i]);
    }
    assert(fabsf(observed[1] / observed[0] - 2.061f) <= 0.02061f);
    assert(fabsf(observed[2] / observed[0] - 3.030f) <= 0.03030f);

    assert(PerformanceConfig::kClassicRateHz[0] == 0.70f);
    assert(PerformanceConfig::kClassicRateHz[1] == 3.00f);
    assert(PerformanceConfig::kClassicRateHz[2] == 6.50f);
    assert(PerformanceConfig::kEchoTimeMinMs == 50.0f);
    assert(PerformanceConfig::kEchoTimeMaxMs == 1000.0f);
}

void testSelectors() {
    for (int zones : {3, 4}) {
        for (int boundary = 1; boundary < zones; ++boundary) {
            int zone = boundary - 1;
            const float edge = float(boundary) / zones;
            for (int i = 0; i < 100; ++i) {
                zone = PerformanceConfig::selectorZone(edge + (i % 2 ? .01f : -.01f), zone, zones);
                assert(zone == boundary - 1);
            }
            zone = PerformanceConfig::selectorZone(edge + .02f, zone, zones);
            assert(zone == boundary);
            for (int i = 0; i < 100; ++i) {
                zone = PerformanceConfig::selectorZone(edge + (i % 2 ? .01f : -.01f), zone, zones);
                assert(zone == boundary);
            }
            assert(PerformanceConfig::selectorZone(edge - .02f, zone, zones) == boundary - 1);
        }
    }
    ControlStore store;
    CommandParser parser(store);
    for (int adc = 0; adc <= 4095; ++adc) {
        const float normalized = adc / 4095.0f;
        char command[64];
        snprintf(command, sizeof(command), "SET PITCH_POSITION %.7f", normalized);
        assert(parser.parse(command));
        const int pitch = static_cast<int>(store.snapshot().classicPitch);
        assert(pitch >= 0 && pitch < 3);
        snprintf(command, sizeof(command), "SET RATE_POSITION %.7f", normalized);
        assert(parser.parse(command));
        const int rate = static_cast<int>(store.snapshot().classicModulation);
        assert(rate >= 0 && rate < 4);
    }
    assert(store.snapshot().classicPitch == ClassicPitch::High);
    assert(store.snapshot().classicModulation == ClassicModulation::Manual);
    assert(!parser.parse("SET PITCH_POSITION nan"));
    assert(!parser.parse("SET RATE_POSITION 1.1"));
    assert(parser.parse("SET ECHO_FILTER 0.049"));
    assert(store.snapshot().echoFilterFlat);
    assert(parser.parse("SET ECHO_FILTER -1"));
    assert(!store.snapshot().echoFilterFlat && fabsf(store.snapshot().lowPassHz - 400) < .01f);
    assert(parser.parse("SET ECHO_FILTER 1"));
    assert(fabsf(store.snapshot().highPassHz - 4000) < .01f);
}

void testConfigurablePanel() {
    ControlStore store;
    CommandParser parser(store);
    const PanelSettings &factory = parser.panelSettings();
    assert(factory.schemaVersion == PanelSettings::kSchemaVersion);
    assert(factory.pots[0].target == PotTarget::Pitch);
    assert(factory.pots[0].mode == PotMode::Zones);
    assert(factory.pots[1].target == PotTarget::Rate);
    // Three sound pads, then the tempo tap that is Mod Up in MANUAL.
    for (unsigned i = 0; i < 3; ++i)
        assert(factory.buttons[i].action == ButtonAction::PlaySound);
    assert(factory.buttons[0].sound == PanelSound::Classic);
    assert(factory.buttons[1].sound == PanelSound::TwoTone);
    assert(factory.buttons[2].sound == PanelSound::Gunshot);
    assert(factory.buttons[3].action == ButtonAction::ModUp);

    // A knob applies at once, also after an external parameter change.
    assert(parser.parse("POT 1 0.5"));
    assert(parser.parse("SET CLASSIC_PITCH MID"));
    assert(parser.parse("POT 1 0"));
    assert(store.snapshot().classicPitch == ClassicPitch::Low);
    assert(fabsf(store.snapshot().tuneHz - PerformanceConfig::kClassicPitchHz[0]) < .001f);
    assert(parser.parse("POT 2 1"));
    assert(store.snapshot().classicModulation == ClassicModulation::Manual);

    // Continuous endpoints and validated ranges are owned by the C++ descriptor.
    assert(parser.parse("PANEL POT 1 MODE CONTINUOUS"));
    assert(!store.snapshot().pitchZonesActive);
    assert(parser.parse("PANEL POT 1 RANGE 100 800"));
    assert(parser.parse("POT 1 0"));
    assert(fabsf(store.snapshot().tuneHz - 100.0f) < .001f);
    assert(parser.parse("POT 1 1"));
    assert(fabsf(store.snapshot().tuneHz - 800.0f) < .01f);
    const PotConfig validRange = parser.panelSettings().pots[0];
    assert(!parser.parse("PANEL POT 1 RANGE 900 100"));
    assert(parser.panelSettings().pots[0].minimum == validRange.minimum);
    assert(parser.panelSettings().pots[0].maximum == validRange.maximum);
    assert(!parser.parse("PANEL POT 1 RANGE 20 100"));

    assert(parser.parse("PANEL POT 1 MODE ZONES"));
    assert(store.snapshot().pitchZonesActive);
    assert(parser.parse("PANEL POT 1 ZONES 220 440 880"));
    assert(!parser.parse("PANEL POT 1 ZONES 440 220 880"));
    assert(parser.panelSettings().pots[0].zones[1] == 440.0f);

    // Every target is assignable; layouts switch the zone flags with them.
    assert(parser.parse("PANEL POT 3 TARGET NONE"));
    assert(parser.panelSettings().pots[2].target == PotTarget::None);
    assert(parser.parse("PANEL POT 3 TARGET DECAY"));
    assert(parser.panelSettings().pots[2].target == PotTarget::Decay);
    assert(padLayout(parser));
    assert(!store.snapshot().pitchZonesActive);
    assert(!store.snapshot().rateZonesActive);
    assert(factoryLayout(parser));
    assert(store.snapshot().pitchZonesActive);
    assert(store.snapshot().rateZonesActive);

    // Configured momentary and toggle button semantics remain finite and explicit.
    assert(parser.parse("PANEL BUTTON 1 ACTION ECHO_LOOP"));
    assert(parser.parse("PANEL BUTTON 1 ACTION TRIGGER"));
    assert(parser.parse("PANEL BUTTON 4 ACTION ECHO_CUT"));
    assert(parser.parse("PAD 4 1"));
    assert(store.snapshot().echoCut);
    assert(parser.parse("PAD 4 0"));
    assert(!store.snapshot().echoCut);
    assert(!parser.parse("PANEL BUTTON 2 ACTION SCRIPT"));

    assert(padLayout(parser));
    assert(parser.parse("PANEL BUTTON 1 SOUND CLASSIC 3 0"));
    assert(parser.parse("PAD 1 1"));
    assert(store.snapshot().mode == SirenMode::Saw);
    assert(parser.parse("PAD 1 0"));
    assert(parser.parse("PANEL BUTTON 1 SOUND CLASSIC 4 0"));
    assert(parser.parse("PAD 1 1"));
    assert(store.snapshot().mode == SirenMode::Triangle);
    assert(parser.parse("PAD 1 0"));
    assert(!parser.parse("PANEL BUTTON 1 SOUND CLASSIC 5 0"));
    assert(parser.parse("PANEL BUTTON 2 SOUND TWO_TONE 2 1"));
    assert(parser.parse("PANEL BUTTON 3 ACTION HOLD"));
    assert(parser.parse("PAD 2 1"));
    assert(store.snapshot().mode == SirenMode::Square && store.snapshot().hold);
    assert(parser.parse("PAD 2 0"));
    assert(store.snapshot().hold);
    assert(parser.parse("PAD 3 1"));
    assert(parser.parse("PAD 3 0"));
    assert(store.snapshot().hold);
    assert(parser.parse("PAD 3 1"));
    assert(parser.parse("PAD 3 0"));
    assert(store.snapshot().hold);
    assert(!parser.parse("PANEL BUTTON 4 SOUND PING 0 1"));

    // The lever has three positions, springs back on release and never
    // alters the stored base tune.
    assert(parser.parse("PANEL RESET_DEFAULTS"));
    assert(factoryLayout(parser));
    const float storedTune = store.snapshot().tuneHz;
    assert(parser.parse("PAD 1 1"));
    for (int detent = 0; detent < 5; ++detent)
        assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(store.snapshot().modUp && !store.snapshot().modDown);
    assert(parser.parse("ENCODER_ROTATE PREV"));
    assert(!store.snapshot().modUp && !store.snapshot().modDown);
    assert(strcmp(parser.overlayValue(), "CENTRO") == 0);
    assert(parser.parse("ENCODER_ROTATE PREV"));
    assert(parser.parse("ENCODER_ROTATE PREV"));
    assert(store.snapshot().modDown && !store.snapshot().modUp);
    assert(store.snapshot().tuneHz == storedTune);
    assert(strcmp(parser.displayModeName(), "HOME") == 0);
    assert(parser.parse("PAD 1 0"));
    assert(!store.snapshot().modUp && !store.snapshot().modDown);
    // In MANUAL it is Mod Up / Mod Down.
    assert(parser.parse("SET CLASSIC_MOD MANUAL"));
    assert(parser.parse("PAD 1 1"));
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(strcmp(parser.overlayValue(), "MOD UP") == 0 && store.snapshot().modUp);
    assert(parser.parse("PAD 1 0"));

    assert(padLayout(parser));
    assert(parser.parse("PANEL BUTTON 2 SOUND GUNSHOT 0 0"));
    assert(parser.parse("PAD 2 1"));
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(!store.snapshot().modUp);
    assert(strcmp(parser.overlayValue(), "N/A") == 0);
    assert(parser.parse("PAD 2 0"));

    // Rear Master is direct.
    assert(parser.parse("SET MASTER 0.23"));
    assert(fabsf(store.snapshot().masterVolume - .23f) < .0001f);

    assert(parser.parse("PANEL RESET_DEFAULTS"));
    assert(parser.panelSettings().pots[0].mode == PotMode::Zones);
    assert(parser.panelSettings().buttons[3].action == ButtonAction::ModUp);

    assert(parser.parse("PANEL BEGIN"));
    assert(parser.parse("PANEL POT 1 TARGET NONE"));
    assert(parser.panelSettings().pots[0].target == PotTarget::Pitch);
    assert(!parser.parse("PANEL POT 2 RANGE 100 1"));
    assert(!parser.parse("PANEL COMMIT"));
    assert(parser.panelSettings().pots[0].target == PotTarget::Pitch);
    assert(parser.parse("PANEL BEGIN"));
    assert(parser.parse("PANEL POT 1 TARGET NONE"));
    assert(parser.parse("PANEL COMMIT"));
    assert(parser.panelSettings().pots[0].target == PotTarget::None);
    assert(parser.parse("PANEL RESET_DEFAULTS"));

    // The lever bends SIREN by one octave too; MANUAL keeps Mod Up for its sweep.
    ControlState bendState;
    bendState.mode = SirenMode::Sine1;
    bendState.lfoShape = LfoShape::Classic;
    bendState.modulationEnabled = false;
    bendState.tuneHz = 392.0f;
    const float straight = renderFrequency(bendState);
    bendState.modDown = true;
    const float bent = renderFrequency(bendState);
    assert(fabsf(straight / bent - 2.0f) < 0.02f);
    assert(bendState.tuneHz == 392.0f);
}

void testClassicDryRelease() {
    for (SirenMode mode : {SirenMode::Sine1, SirenMode::Sine2, SirenMode::TestTone, SirenMode::Square}) {
        auto engine = std::make_unique<AudioEngine>();
        engine->begin();
        ControlState state;
        state.mode = mode;
        state.echoLevel = 0;
        state.classicModulation = ClassicModulation::Manual;
        state.modUp = true;
        state.trigger = true;
        int16_t block[480];
        engine->render(state, block, 480);
        assert(blockRms(block, 96) > 50);
        for (int i = 0; i < 100; ++i) engine->render(state, block, 480);
        state.trigger = false;
        engine->render(state, block, 480);
        assert(blockRms(block + 146, 480 - 146) == 0);
        for (int i = 0; i < 120; ++i) {
            engine->render(state, block, 480);
            assert(blockRms(block, 480) == 0);
        }
        state.hold = true;
        engine->render(state, block, 480);
        assert(blockRms(block, 480) > 100);
        state.hold = false;
        engine->render(state, block, 480);
        assert(blockRms(block + 146, 334) == 0);
    }
}

void testWetIsolation() {
    auto audible = std::make_unique<AudioEngine>();
    auto dry = std::make_unique<AudioEngine>();
    audible->begin(); dry->begin();
    ControlState a, b;
    a.trigger = b.trigger = true;
    a.echoLevel = 1; b.echoLevel = 0;
    a.delayMs = 50; b.delayMs = 700;
    a.feedback = 1.05f; b.feedback = 0;
    int16_t left[480], right[480];
    for (int i = 0; i < 100; ++i) {
        audible->render(a, left, 480); dry->render(b, right, 480);
    }
    a.echoLevel = 0;
    audible->render(a, left, 480); dry->render(b, right, 480);
    assert(memcmp(left, right, sizeof(left)) == 0);
    a.echoLevel = 1; a.echoCut = true;
    audible->render(a, left, 480); dry->render(b, right, 480);
    assert(memcmp(left + 96, right + 96, (480-96) * sizeof(int16_t)) == 0);
    a.echoCut = false;
    audible->render(a, left, 480); dry->render(b, right, 480);
    assert(memcmp(left, right, sizeof(left)) != 0); // Buffer evolved underneath cut.
}

void testSingleRepeatAndFeedback() {
    double previousTail = -1;
    for (float feedback : {0.0f, 0.2f, 0.5f, 0.8f}) {
        auto delay = std::make_unique<Delay>();
        delay->begin(100, 60, 16000, kRate);
        delay->setParameters(100, 60, 16000, kRate, true);
        for (int i = 0; i < 48000; ++i) delay->process(0, feedback, false, kRate);
        double first = 0, tail = 0;
        for (int i = 0; i < 48000; ++i) {
            const float value = delay->process(i == 0 ? .25f : 0.0f, feedback, false, kRate);
            if (i >= 4800 && i < 9600) first += value * value;
            if (i >= 9600) tail += value * value;
        }
        assert(first > .01);
        if (feedback == 0) assert(tail < 1e-9);
        assert(tail > previousTail);
        previousTail = tail;
    }
}

void testFeedbackTailDecays() {
    for (bool flat : {true, false}) {
        for (float feedback : {0.0f, 0.2f, 0.5f, 0.62f, 0.8f, 0.9f, 0.95f, 0.99f}) {
            auto delay = std::make_unique<Delay>();
            delay->begin(100, 400, 7000, kRate, flat);
            for (int i = 0; i < 4800; ++i) {
                const float dry = .3f * sinf(TWO_PI * 600 * i / kRate);
                delay->process(dry, feedback, false, kRate);
            }
            double first = 0, tail = 0;
            const int samples = feedback > .98f ? 4800000 : feedback > .9f ? 960000 : 480000;
            for (int i = 0; i < samples; ++i) {
                const float wet = delay->process(0, feedback, false, kRate);
                assert(isfinite(wet));
                if (i < 4800) first += wet * wet;
                if (i >= samples - 48000) tail += wet * wet;
            }
            assert(first > 1);
            assert(tail < first * 1e-6);
        }
    }
}

void testReducingFeedbackStopsSustainedTail() {
    auto delay = std::make_unique<Delay>();
    delay->begin(50, 60, 7000, kRate, true);
    for (int i = 0; i < 2400; ++i) {
        delay->process(.05f * sinf(TWO_PI * 600 * i / kRate), 1.05f, false, kRate);
    }
    double sustained = 0, cleared = 0;
    for (int i = 0; i < 96000; ++i) {
        const float wet = delay->process(0, 1.05f, false, kRate);
        if (i >= 93600) sustained += wet * wet;
    }
    assert(sustained > 1); // Above-unity feedback still permits self-oscillation.
    for (int i = 0; i < 48000; ++i) {
        const float wet = delay->process(0, .5f, false, kRate);
        if (i >= 45600) cleared += wet * wet;
    }
    assert(cleared == 0); // Lowering feedback also drains an already loud buffer.
}

void testManualAndClassicVoices() {
    // MANUAL ignores Rate: the button pulls the level to the top, release
    // lets it sink slowly instead of snapping back.
    Lfo slow, fast;
    slow.reset(LfoShape::Manual); fast.reset(LfoShape::Manual);
    float held = 0, released = 0;
    for (int i = 0; i < 48000; ++i) {
        const bool pressed = i > 1000 && i < 12000;
        const float a = slow.next(LfoShape::Manual, .35f, pressed, false, kRate);
        const float b = fast.next(LfoShape::Manual, 20.0f, pressed, false, kRate);
        assert(a == b && slow.gate() == pressed && fast.gate() == pressed);
        if (i == 11999) held = a;
        released = a;
    }
    assert(held > 0.9f);
    assert(released < held - 0.2f && released > 0.0f);
    ControlState state;
    state.classicModulation = ClassicModulation::Manual;
    state.mode = SirenMode::Square;
    const float low = renderFrequency(state);
    state.modUp = true;
    const float high = renderFrequency(state);
    assert(low > 417 && low < 426 && high > 675 && high < 685);
    state.mode = SirenMode::TestTone;
    assert(fabsf(renderFrequency(state) - 680) < 5);
    state.modUp = false;
    assert(renderFrequency(state) == 0);
}


void testClassicAutomaticVoices() {
    for (SirenMode mode : {SirenMode::TestTone, SirenMode::Square}) {
        auto engine = std::make_unique<AudioEngine>(); engine->begin();
        ControlState state;
        state.mode = mode; state.hold = true; state.echoLevel = 0;
        // Slow keeps each half-cycle longer than one analysis block.
        state.classicModulation = ClassicModulation::Slow;
        int16_t block[4800];
        bool heardLow = false, heardHigh = false;
        for (int i = 0; i < 40; ++i) {
            engine->render(state, block, 4800);
            if (mode == SirenMode::TestTone) {
                heardLow |= blockRms(block, 4800) < 1;
                heardHigh |= blockRms(block, 4800) > 1000;
            } else {
                const float frequency = estimateFrequency(block, 4800);
                heardLow |= frequency > 417 && frequency < 426;
                heardHigh |= frequency > 675 && frequency < 685;
            }
        }
        assert(heardLow && heardHigh);
    }
    double secondHarmonic[2] = {};
    for (int voice = 0; voice < 2; ++voice) {
        Oscillator osc;
        double real = 0, imaginary = 0, energy = 0;
        for (int i = 0; i < 48000; ++i) {
            const float value = osc.next(static_cast<SirenMode>(voice), 220, 0,
                                         1, true, kRate);
            real += value * cos(TWO_PI * 440.0 * i / kRate);
            imaginary += value * sin(TWO_PI * 440.0 * i / kRate);
            energy += value * value;
        }
        assert(energy > 5000 && energy < 15000);
        secondHarmonic[voice] = hypot(real, imaginary) / 24000;
    }
    assert(secondHarmonic[0] > .1);
    assert(secondHarmonic[1] > .13);
}

void testShortTriggerEvents() {
    for (bool pads : {false, true}) {
        for (int route = 0; route < 3; ++route) {
            ControlStore store; CommandParser parser(store);
            assert(pads ? padLayout(parser) : factoryLayout(parser));
            assert(parser.parse("SET ECHO_LEVEL 0"));
            if (route == 2) assert(parser.parse("PANEL BUTTON 3 ACTION TRIGGER"));
            auto engine = std::make_unique<AudioEngine>(); engine->begin();
            int16_t block[480];
            engine->render(store.snapshot(), block, 480);
            const char *press = route == 0 ? "TRIGGER 1" : route == 1 ? "PAD 1 1" : "PAD 3 1";
            const char *release = route == 0 ? "TRIGGER 0" : route == 1 ? "PAD 1 0" : "PAD 3 0";
            assert(parser.parse(press)); assert(parser.parse(release));
            engine->render(store.snapshot(), block, 480);
            assert(blockRms(block, 480) > 10);
            assert(engine->renderedActivations() == 1);
        }
    }
    for (unsigned interval = 1000; interval <= 9000; interval += 1000) {
        auto engine = std::make_unique<AudioEngine>(); engine->begin();
        ControlState state; state.echoLevel = 0;
        recordActivation(state, 100000); recordActivation(state, 100000 + interval);
        int16_t block[480]; engine->render(state, block, 480);
        assert(engine->renderedActivations() == 2);
        engine->render(state, block, 480);
        assert(engine->renderedActivations() == 2);
    }
    auto engine = std::make_unique<AudioEngine>(); engine->begin();
    ControlState state;
    for (unsigned i = 0; i < 12; ++i) recordActivation(state, 100000 + i * 1000);
    int16_t block[480]; engine->render(state, block, 480);
    assert(engine->droppedActivations() == 4);
    assert(engine->renderedActivations() == 8);
    for (unsigned i = 0; i < 8; ++i) recordActivation(state, 200000 + i * 10000);
    engine->render(state, block, 480);
    assert(engine->renderedActivations() == 9 && engine->droppedActivations() == 11);
    engine->render(state, block, 480);
    assert(engine->renderedActivations() == 9);
}

void testTwoTonePitchTrajectories() {
    float frequency[3][10] = {};
    const char *shapes[] = {"SET LFO_SHAPE CLASSIC", "SET LFO_SHAPE TRIANGLE", "SET LFO_SHAPE SQUARE"};
    for (int route = 0; route < 2; ++route) {
        for (unsigned variant = 0; variant < 3; ++variant) {
            ControlStore store; CommandParser parser(store);
            assert(padLayout(parser));
            assert(parser.parse("SET TUNE_HZ 220"));
            assert(parser.parse("SET LFO_RATE_HZ 2"));
            assert(parser.parse("SET ECHO_LEVEL 0"));
            auto engine = std::make_unique<AudioEngine>(); engine->begin();
            int16_t block[2400];
            for (int i = 0; i < 20; ++i) engine->render(store.snapshot(), block, 2400);
            if (route == 0) {
                assert(parser.parse("SET MODE SQUARE")); assert(parser.parse(shapes[variant]));
                assert(parser.parse("RETRIGGER 1")); assert(parser.parse("TRIGGER 1"));
            } else {
                char command[64]; snprintf(command, sizeof(command), "PAD_CONFIG 2 TWO_TONE %u 0", variant);
                assert(parser.parse(command)); assert(parser.parse("PAD 2 1"));
            }
            for (int i = 0; i < 20; ++i) {
                engine->render(store.snapshot(), block, 2400);
                if (i >= 10) frequency[variant][i-10] = estimateFrequency(block, 2400);
            }
            if (variant == 0) {
                // SIREN two-tone: only the selected pitch and 0.62 of it.
                int high = 0, low = 0;
                for (float value : frequency[0]) {
                    high += fabsf(value - 220.0f) < 3.0f;
                    low += fabsf(value - 136.4f) < 3.0f;
                }
                assert(high >= 2 && low >= 2 && high + low >= 7);
            } else {
                assert(frequency[variant][4] > 350);
                assert(frequency[variant][9] < 160);
            }
        }
        // Triangle continues rising; square plateaus.
        assert(frequency[1][4] > frequency[1][1] + 150);
        assert(fabsf(frequency[2][4] - frequency[2][1]) < 10);
    }
}

void testLickshotArticulations() {
    for (unsigned variant = 2; variant <= 4; ++variant) {
        ControlStore store; CommandParser parser(store);
        assert(padLayout(parser));
        assert(parser.parse("SET ECHO_LEVEL 0"));
        assert(parser.parse("SET LFO_RATE_HZ 4"));
        char command[64]; snprintf(command, sizeof(command), "PAD_CONFIG 3 LICKSHOT %u 0", variant);
        assert(parser.parse(command));
        auto engine = std::make_unique<AudioEngine>(); engine->begin();
        int16_t block[480];
        for (int i = 0; i < 100; ++i) engine->render(store.snapshot(), block, 480);
        assert(parser.parse("PAD 3 1"));
        unsigned attacks = 0; bool previous = false;
        for (int i = 0; i < 200; ++i) {
            engine->render(store.snapshot(), block, 480);
            const bool sounding = blockRms(block, 480) > 10;
            if (sounding && !previous) ++attacks;
            previous = sounding;
            if (variant == 2 && i > 16) assert(!sounding);
            if (variant == 4 && i > 10) assert(sounding);
        }
        assert(attacks == (variant == 3 ? 8 : 1));
        assert(parser.parse("PAD 3 0"));
        for (int i = 0; i < 150; ++i) engine->render(store.snapshot(), block, 480);
        assert(parser.parse("PAD 3 1")); assert(parser.parse("PAD 3 0"));
        engine->render(store.snapshot(), block, 480);
        assert(blockRms(block, 480) > 10);
        assert(static_cast<unsigned>(store.snapshot().lickshotArticulation) == variant-1);
    }
}

void testDryToneAndContour() {
    float highEnergy[3] = {};
    for (int version = 0; version < 3; ++version) {
        auto engine = std::make_unique<AudioEngine>(); engine->begin();
        ControlState state; padState(state);
        state.mode = SirenMode::Square; state.modulationEnabled = false;
        state.tuneHz = 220; state.echoLevel = 0;
        state.dryToneHz = version == 0 ? 19000 : 500;
        state.filterContourOctaves = version == 2 ? 4 : 0;
        state.filterContourMs = 3000;
        int16_t block[4800];
        for (int i = 0; i < 10; ++i) engine->render(state, block, 4800);
        state.trigger = true;
        engine->render(state, block, 4800);
        engine->render(state, block, 4800);
        assert(fabsf(estimateFrequency(block, 4800) - 220) < 4);
        double dc = 0, real = 0, imag = 0, fundamentalReal = 0, fundamentalImag = 0;
        for (int i = 0; i < 4800; ++i) {
            real += block[i] * cos(TWO_PI * 1100 * i / kRate);
            imag += block[i] * sin(TWO_PI * 1100 * i / kRate);
            fundamentalReal += block[i] * cos(TWO_PI * 220 * i / kRate);
            fundamentalImag += block[i] * sin(TWO_PI * 220 * i / kRate);
            dc += block[i];
        }
        highEnergy[version] = hypot(real, imag) / hypot(fundamentalReal, fundamentalImag);
        assert(fabs(dc / 4799) < 200);
    }
    assert(highEnergy[1] < highEnergy[0] * .5f);
    assert(highEnergy[2] > highEnergy[1] * 3);
}

void testToneBypassFadesOut() {
    // Turning the Tone filter off must fade it out (5 ms), not drop it at
    // once. Switch at several waveform phases and keep the worst step.
    int steadyJump = 0, switchJump = 0;
    for (int offset = 0; offset < 480; offset += 24) {
        auto engine = std::make_unique<AudioEngine>();
        engine->begin();
        ControlState state;
        padState(state);
        state.mode = SirenMode::Sine1;
        state.hold = true;
        state.dryToneHz = 500.0f;
        state.echoLevel = 0.0f;
        int16_t block[480];
        int previous = 0;
        for (int n = 0; n < 100; ++n) {
            const size_t count = n == 0 ? 480 - offset : 480;
            engine->render(state, block, count);
            for (size_t i = 0; i < count; ++i) {
                if (n >= 80) steadyJump = std::max(steadyJump, abs(block[i] - previous));
                previous = block[i];
            }
        }
        state.dryToneHz = 19000.0f;
        engine->render(state, block, 8);
        for (int i = 0; i < 8; ++i) {
            switchJump = std::max(switchJump, abs(block[i] - previous));
            previous = block[i];
        }
    }
    assert(switchJump <= steadyJump * 1.2f);
}

void testColoredEchoDecay() {
    for (float feedback : {0.0f, .5f, .8f, .99f}) {
        auto delay = std::make_unique<Delay>(); delay->begin(50, 60, 19000, kRate, true);
        delay->setColor(1);
        for (int i = 0; i < 4800; ++i) delay->process(0, feedback, false, kRate);
        double early = 0, late = 0;
        for (int i = 0; i < 480000; ++i) {
            const float input = i < 480 ? .4f * sinf(TWO_PI * 440 * i / kRate) : 0;
            const float wet = delay->process(input, feedback, false, kRate);
            assert(isfinite(wet) && fabsf(wet) < 1);
            if (i >= 2400 && i < 4800) early += wet * wet;
            if (i >= 477600) late += wet * wet;
        }
        assert(early > .01 && late < early * .001);
    }
}

void testPerformanceMenu() {
    ControlStore store; CommandParser parser(store);
    assert(padLayout(parser));
    auto click = [&]() { assert(parser.parse("ENCODER_SWITCH 1")); assert(parser.parse("ENCODER_SWITCH 0")); };
    auto go = [&](const char *key) {
        unsigned steps = 0;
        while (strcmp(parser.menuItemKey(parser.menuIndex()), key) != 0) {
            assert(++steps <= parser.menuItemCount()); assert(parser.parse("ENCODER_ROTATE NEXT"));
        }
        click();
    };
    // TEMPO holds the BPM and both clocks; there is no tap row in the menu.
    click(); go("TEMPO");
    assert(strcmp(parser.menuTitle(), "TEMPO") == 0 && parser.menuItemCount() == 4);
    go("LFO_SYNC"); assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(strcmp(parser.menuFooter(), "IL RATE SEGUE IL BPM") == 0);
    click();
    assert(store.snapshot().tempo.lfoSync);
    go("LFO_SYNC"); assert(parser.parse("ENCODER_ROTATE NEXT")); click();
    assert(!store.snapshot().tempo.lfoSync);
    go("BACK"); go("SOUND"); go("SOUND");
    assert(parser.parse("ENCODER_ROTATE NEXT")); assert(parser.parse("ENCODER_ROTATE NEXT")); click();
    assert(store.snapshot().mode == SirenMode::Square);
    go("BACK"); go("MOD"); go("MODULATION");
    assert(parser.parse("ENCODER_ROTATE NEXT")); click();
    assert(!store.snapshot().modulationEnabled);
    assert(parser.parse("PAD 1 1")); assert(parser.parse("PAD 1 0"));
    assert(!store.snapshot().modulationEnabled);
    go("BACK"); go("SOUND"); go("DECAY");
    const float before = store.snapshot().decayMs;
    assert(parser.parse("ENCODER_ROTATE NEXT")); click();
    assert(store.snapshot().decayMs > before);
    assert(parser.parse("PAD 1 1")); assert(parser.parse("PAD 1 0"));
    assert(store.snapshot().decayMs > before);
    go("SOUND"); for (int i = 0; i < 3; ++i) assert(parser.parse("ENCODER_ROTATE NEXT"));
    click();
    assert(store.snapshot().mode == SirenMode::Gunshot);
    assert(parser.parse("PAD 1 1")); assert(parser.parse("PAD 1 0"));
    assert(store.snapshot().mode == SirenMode::Gunshot);
    assert(parser.parse("PANEL BUTTON 1 ACTION ECHO_CUT"));
    go("BACK"); go("BACK");
    const float depth = store.snapshot().lfoDepthOctaves;
    assert(parser.parse("PAD 1 1")); assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(store.snapshot().lfoDepthOctaves == depth);
}

void testPadMemories() {
    ControlStore store; CommandParser parser(store);
    assert(padLayout(parser));
    for (unsigned pad = 1; pad <= 4; ++pad) {
        char command[64];
        snprintf(command, sizeof(command), "PAD %u 1", pad); assert(parser.parse(command));
        snprintf(command, sizeof(command), "PAD %u 0", pad); assert(parser.parse(command));
        snprintf(command, sizeof(command), "SET TUNE_HZ %u", pad * 220); assert(parser.parse(command));
        snprintf(command, sizeof(command), "SET DECAY_MS %u", pad * 150); assert(parser.parse(command));
        assert(parser.parse("SET SYNDRUM_SWEEP -1.5"));
        assert(parser.parse("MODULATION 0"));
        snprintf(command, sizeof(command), "PANEL SAVE %u", pad); assert(parser.parse(command));
    }
    const PanelSettings saved = parser.panelSettings();
    ControlStore reboot; CommandParser restored(reboot);
    assert(restored.applyPanelSettings(saved));
    assert(padLayout(restored));
    assert(!reboot.snapshot().trigger && !reboot.snapshot().hold && !reboot.snapshot().echoLoop);
    // Power-on scan of POT1; each recall below still restores its patch.
    assert(restored.parse("POT 1 0.5"));
    assert(restored.parse("SET ECHO_LEVEL 0.7")); assert(restored.parse("SET MASTER 0.3"));
    for (unsigned pad = 1; pad <= 4; ++pad) {
        char command[64]; snprintf(command, sizeof(command), "PANEL RECALL %u", pad);
        assert(restored.parse(command));
        assert(reboot.snapshot().tuneHz == pad * 220);
        assert(reboot.snapshot().decayMs == pad * 150);
        snprintf(command, sizeof(command), "PAD %u 1", pad); assert(restored.parse(command));
        assert(reboot.snapshot().decayMs == pad * 150);
        assert(reboot.snapshot().syndrumSweepOctaves == -1.5f);
        assert(reboot.snapshot().echoLevel == .7f && reboot.snapshot().masterVolume == .3f);
        const auto state = reboot.snapshot();
        const auto &event = state.activationEvents[(state.activationSequence-1) % kActivationEventCapacity];
        assert(event.tuneHz == state.tuneHz && event.decayMs == state.decayMs);
        snprintf(command, sizeof(command), "PAD %u 0", pad); assert(restored.parse(command));
        assert(restored.parse("POT 1 0"));
    }
    PanelSettings outdated = saved; outdated.schemaVersion = 2;
    assert(!restored.applyPanelSettings(outdated));
    PanelSettings corrupt = saved; corrupt.patches[0].state.tuneHz = NAN;
    assert(!restored.applyPanelSettings(corrupt));
    assert(restored.panelSettings().patches[0].state.tuneHz == 220);
}

void testPhysicalAndRemoteOwnership() {
    ControlStore store; CommandParser parser(store);
    assert(parser.setPhysicalPad(1, true));
    const auto sequence = store.snapshot().activationSequence;
    assert(parser.parse("PAD 1 1")); assert(parser.parse("PAD 1 0"));
    assert(store.snapshot().trigger);
    assert(parser.parse("PANEL_RESET")); assert(parser.parse("TRIGGER 0"));
    assert(store.snapshot().trigger && store.snapshot().activationSequence == sequence);
    assert(parser.setPhysicalPad(1, false)); assert(!store.snapshot().trigger);
    assert(parser.parse("PAD 1 1")); assert(parser.setPhysicalPad(1, true));
    assert(parser.setPhysicalPad(1, false)); assert(store.snapshot().trigger);
    assert(parser.parse("PAD 1 0")); assert(!store.snapshot().trigger);
    // USB loss releases remote input without cancelling a physical trigger.
    assert(parser.parse("PAD 1 1"));
    assert(parser.parse("ENCODER_SWITCH 1"));
    parser.resetRemoteInputs();
    assert(!store.snapshot().trigger);
    assert(strcmp(parser.displayModeName(), "HOME") == 0);
    // That key + push was the Hold gesture: a click unlocks it, as intended
    // Hold survives the reset.
    assert(store.snapshot().hold);
    assert(parser.parse("ENCODER_SWITCH 1")); assert(parser.parse("ENCODER_SWITCH 0"));
    assert(!store.snapshot().hold);
    assert(parser.setPhysicalPad(1, true));
    assert(parser.parse("PAD 1 1"));
    parser.resetRemoteInputs();
    assert(store.snapshot().trigger);
    assert(parser.setPhysicalPad(1, false));
    assert(!store.snapshot().trigger);
}

void testPanelValidationAndCancelledClicks() {
    ControlStore store;
    CommandParser parser(store);
    const char *invalid[] = {
        "PANEL ENCODER LIVE_PITCH 1 12",  // Gone with live pitch.
        "PANEL BUTTON 1 SOUND CLASSIC 256 0",
        "PANEL BUTTON 4294967297 SOUND CLASSIC 0 0",
        "PANEL POT 1 RANGE 100", "PANEL POT 1 MODE UNKNOWN",
        "PANEL POT 1 TARGET UNKNOWN",
        "PANEL BUTTON 1 ACTION UNKNOWN", "PANEL UNKNOWN"
    };
    for (const char *command : invalid) {
        const unsigned revision = parser.panelSettingsRevision();
        assert(parser.parse("PANEL BEGIN"));
        assert(parser.parse("PANEL POT 1 TARGET NONE"));
        assert(!parser.parse(command));
        assert(!parser.parse("PANEL COMMIT"));
        assert(parser.panelSettingsRevision() == revision);
        assert(parser.panelSettings().pots[0].target == PotTarget::Pitch);
    }
    PanelSettings corrupt = factoryPanelSettings();
    corrupt.pots[0].mode = static_cast<PotMode>(255);
    assert(!parser.applyPanelSettings(corrupt));
    corrupt = factoryPanelSettings();
    corrupt.pots[0].zones[0] = NAN;
    assert(!parser.applyPanelSettings(corrupt));
    corrupt = factoryPanelSettings();
    corrupt.buttons[0].sound = static_cast<PanelSound>(255);
    assert(!parser.applyPanelSettings(corrupt));

    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("PANEL_RESET"));
    assert(strcmp(parser.displayModeName(), "HOME") == 0);
    assert(parser.setPhysicalPad(1, true));
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("PANEL_RESET"));
    assert(store.snapshot().trigger);
    assert(strcmp(parser.displayModeName(), "HOME") == 0);
    assert(parser.setPhysicalPad(1, false));
    // That push with the key held locked the sound: the next click unlocks it.
    assert(store.snapshot().hold);
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(!store.snapshot().hold && strcmp(parser.displayModeName(), "HOME") == 0);
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(strcmp(parser.displayModeName(), "MENU") == 0);
    // The power-on reading is silent; the next movement shows in the footer.
    // Until then the footer describes the selected row.
    assert(parser.parse("POT 3 0.5"));
    assert(strcmp(parser.menuFooter(), "ASSEGNA POT E TASTI") == 0);
    assert(parser.parse("POT 3 0"));
    assert(strstr(parser.menuFooter(), "ECHO LEVEL") != nullptr);
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(strcmp(parser.menuFooter(), "SUONO E SUOI PARAMETRI") == 0);
}

void testPatchTransactionAndRemoteGates() {
    ControlStore store;
    CommandParser parser(store);
    assert(padLayout(parser));
    assert(parser.parse("PANEL SAVE 1"));
    char patch[512] = "PANEL PATCH 2";
    for (unsigned i = 0; i < kSoundPatchValueCount; ++i) {
        char value[32];
        snprintf(value, sizeof(value), " %.9g", soundPatchValue(parser.panelSettings().patches[0], i));
        strcat(patch, value);
    }
    // A malformed PATCH invalidates the staged restore; later valid lines
    // must not be applied on their own.
    const unsigned revision = parser.panelSettingsRevision();
    assert(parser.parse("PANEL BEGIN"));
    assert(!parser.parse("PANEL PATCH 1 1 2 x"));
    assert(parser.parse(patch));
    assert(!parser.parse("PANEL COMMIT"));
    assert(!parser.panelSettings().patches[1].valid);
    assert(parser.panelSettingsRevision() == revision);
    assert(parser.parse("PANEL BEGIN"));
    assert(parser.parse(patch));
    assert(parser.parse("PANEL COMMIT"));
    assert(parser.panelSettings().patches[1].valid);
    for (unsigned i = 0; i < kSoundPatchValueCount; ++i)
        assert(soundPatchValue(parser.panelSettings().patches[1], i) ==
               soundPatchValue(parser.panelSettings().patches[0], i));
    float bad[kSoundPatchValueCount];
    SoundPatch decoded;
    for (unsigned i = 0; i < kSoundPatchValueCount; ++i) bad[i] = soundPatchValue(parser.panelSettings().patches[0], i);
    bad[3] = 200;  // no such mode
    assert(!decodeSoundPatch(bad, decoded));
    bad[3] = 0; bad[7] = 2;  // flags are 0 or 1
    assert(!decodeSoundPatch(bad, decoded));

    // Legacy remote gates survive gate recomputation from panel events.
    assert(parser.parse("HOLD 1"));
    assert(parser.parse("MOD_UP 1"));
    assert(parser.parse("ECHO_SPIN 1"));
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(parser.setPhysicalPad(4, true));
    assert(parser.setPhysicalPad(4, false));
    ControlState state = store.snapshot();
    assert(state.hold && state.modUp && state.echoSpin);
    assert(parser.parse("HOLD 0"));
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(!store.snapshot().hold && store.snapshot().modUp);
    // A physically held pad must not keep remote gates alive on USB loss.
    assert(parser.parse("HOLD 1"));
    assert(parser.setPhysicalPad(1, true));
    parser.resetRemoteInputs();
    state = store.snapshot();
    assert(!state.hold && !state.modUp && !state.echoSpin);
    assert(parser.setPhysicalPad(1, false));

    // Device errors no longer hide NVS errors or the menu footer.
    parser.setDeviceStatus("I2S WRITE FAILED");
    parser.setPersistenceStatus("SAVE FAILED");
    assert(strcmp(parser.persistenceStatus(), "SAVE FAILED") == 0);
    assert(strcmp(parser.deviceStatus(), "I2S WRITE FAILED") == 0);
    parser.setPersistenceStatus("");
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(strcmp(parser.menuFooter(), "ASSEGNA POT E TASTI") == 0);
}

void testVariantSaveRule() {
    // Pad family/variant persist from both Home and the SOUND menu; numeric
    // sound edits stay live until SAVE PATCH.
    ControlStore store;
    CommandParser parser(store);
    assert(padLayout(parser));
    auto click = [&]() { assert(parser.parse("ENCODER_SWITCH 1")); assert(parser.parse("ENCODER_SWITCH 0")); };
    auto go = [&](const char *key) {
        unsigned steps = 0;
        while (strcmp(parser.menuItemKey(parser.menuIndex()), key) != 0) {
            assert(++steps <= parser.menuItemCount()); assert(parser.parse("ENCODER_ROTATE NEXT"));
        }
        click();
    };
    unsigned revision = parser.panelSettingsRevision();
    stepSound(parser, 1);
    assert(parser.panelSettingsRevision() > revision);
    click(); go("SOUND");
    revision = parser.panelSettingsRevision();
    go("SOUND"); assert(parser.parse("ENCODER_ROTATE NEXT")); click();
    assert(parser.panelSettingsRevision() > revision);
    revision = parser.panelSettingsRevision();
    go("TUNE"); assert(parser.parse("ENCODER_ROTATE NEXT")); click();
    assert(parser.panelSettingsRevision() == revision);
}

void testRemapClearsHold() {
    // Remapping the HOLD key from the SETUP map must release the latch it set.
    ControlStore store;
    CommandParser parser(store);
    assert(factoryLayout(parser));
    auto click = [&]() { assert(parser.parse("ENCODER_SWITCH 1")); assert(parser.parse("ENCODER_SWITCH 0")); };
    auto go = [&](const char *key) {
        unsigned steps = 0;
        while (strcmp(parser.menuItemKey(parser.menuIndex()), key) != 0) {
            assert(++steps <= parser.menuItemCount()); assert(parser.parse("ENCODER_ROTATE NEXT"));
        }
        click();
    };
    // At Home the encoder click only unlocks: latch HOLD with the menu open.
    click();
    assert(parser.parse("PAD 2 1")); assert(parser.parse("PAD 2 0"));
    assert(store.snapshot().hold);
    go("SETUP"); go("BTN_2");
    assert(parser.parse("ENCODER_ROTATE NEXT"));  // HOLD -> MOD UP
    assert(parser.panelSettings().buttons[1].action == ButtonAction::ModUp);
    click();
    assert(!store.snapshot().hold);
    // Same path for a zoned Pitch pot given another function.
    assert(store.snapshot().pitchZonesActive);
    unsigned pitch = 0;
    while (parser.panelSettings().pots[pitch].target != PotTarget::Pitch) assert(++pitch < 6);
    char key[8]; snprintf(key, sizeof(key), "POT_%u", pitch + 1);
    while (strcmp(parser.menuItemKey(parser.menuIndex()), key) != 0) assert(parser.parse("ENCODER_ROTATE PREV"));
    click(); assert(parser.parse("ENCODER_ROTATE NEXT")); click();
    assert(parser.panelSettings().pots[pitch].target != PotTarget::Pitch);
    assert(!store.snapshot().pitchZonesActive);
}

void testTapTempo() {
    ControlStore store;
    PerformanceController panel(store);
    assert(panel.tapTempo(100000));
    assert(!store.snapshot().tempo.known);
    assert(!store.snapshot().tempo.echoSync);
    assert(!panel.tapTempo(150000));
    assert(strstr(panel.overlayValue(), "IGNORED"));
    assert(panel.tapTempo(600000));
    assert(fabsf(store.snapshot().tempo.bpm - 120.0f) < .001f);
    // A free echo follows the tapped tempo; the LFO keeps its own Rate.
    assert(store.snapshot().tempo.echoSync && !store.snapshot().tempo.lfoSync);
    assert(fabsf(store.snapshot().tempo.echoMs - 500.0f) < .001f);
    assert(strcmp(panel.overlayTitle(), "TAP TEMPO") == 0);
    assert(panel.tapTempo(1110000));
    assert(fabsf(store.snapshot().tempo.bpm - 60000000.0f / 505000) < .001f);
    assert(panel.tapTempo(1600000));
    assert(fabsf(store.snapshot().tempo.bpm - 120.0f) < .001f);
    assert(panel.tapTempo(2100000));
    assert(panel.tapTempo(2500000));
    assert(fabsf(store.snapshot().tempo.bpm - 60000000.0f / 475000) < .001f);
    assert(panel.tapTempo(5000001));  // Timeout starts a fresh sequence without changing tempo.
    assert(strstr(panel.overlayValue(), "AGAIN"));
    assert(panel.tapTempo(5400001));
    assert(fabsf(store.snapshot().tempo.bpm - 150.0f) < .001f);
    assert(panel.setTempoBpm(120));
    assert(panel.tapTempo(UINT32_MAX - 249999u));
    assert(panel.tapTempo(250000u));  // Unsigned timer rollover, exactly 500 ms.
    assert(fabsf(store.snapshot().tempo.bpm - 120.0f) < .001f);
    assert(!panel.setTempoBpm(NAN));
    assert(!panel.setTempoBpm(29));
    assert(!panel.setTempoBpm(301));
    assert(store.snapshot().tempo.bpm == 120);
    assert(panel.setTempoBpm(30));
    assert(panel.tapTempo(0));
    assert(panel.tapTempo(2000000));
    assert(store.snapshot().tempo.bpm == 30);
    assert(panel.setTempoBpm(300));
    assert(panel.tapTempo(0));
    assert(panel.tapTempo(200000));
    assert(store.snapshot().tempo.bpm == 300);
}

void testTempoControls() {
    ControlStore store;
    CommandParser parser(store);
    assert(parser.parse("TEMPO ECHO SYNC"));
    assert(!tempoEchoAvailable(store.snapshot().tempo));
    char value[64];
    parser.formatCurrentValue(PotTarget::EchoTime, value, sizeof(value));
    assert(strstr(value, "N/A HOLD"));
    assert(parser.parse("TEMPO BPM 120"));
    assert(parser.parse("TEMPO LFO SYNC"));
    const char *divisions[] = {"1/4", "1/8", "1/8D", "1/8T"};
    const float periods[] = {500, 250, 375, 500.0f / 3};
    for (unsigned i = 0; i < 4; ++i) {
        char command[48];
        snprintf(command, sizeof(command), "TEMPO ECHO_DIV %s", divisions[i]);
        assert(parser.parse(command));
        snprintf(command, sizeof(command), "TEMPO LFO_DIV %s", divisions[i]);
        assert(parser.parse(command));
        const auto tempo = store.snapshot().tempo;
        assert(fabsf(tempo.echoMs - periods[i]) < .001f);
        assert(fabsf(tempo.lfoHz - 1000.0f / periods[i]) < .001f);
    }
    const auto before = store.snapshot();
    for (const char *command : {"TEMPO BPM nan", "TEMPO BPM 120 extra", "TEMPO BPM 1e100",
            "TEMPO BPM 0", "TEMPO BPM 301", "TEMPO ECHO ON", "TEMPO LFO_DIV 1/16",
            "TEMPO ECHO_DIV 1/4 extra", "TEMPO UNKNOWN SYNC",
            "SET DELAY_MS 100", "SET LFO_RATE_HZ 3", "SET CLASSIC_MOD MANUAL"}) {
        assert(!parser.parse(command));
        const auto after = store.snapshot();
        assert(after.tempo.bpm == before.tempo.bpm);
        assert(after.tempo.echoDivision == before.tempo.echoDivision);
        assert(after.tempo.lfoDivision == before.tempo.lfoDivision);
        assert(after.delayMs == before.delayMs && after.lfoRateHz == before.lfoRateHz);
    }
    assert(parser.parse("TEMPO BPM 30"));
    const float previous = store.snapshot().tempo.echoMs;
    assert(parser.parse("TEMPO ECHO_DIV 1/4"));
    assert(!tempoEchoAvailable(store.snapshot().tempo));
    assert(store.snapshot().tempo.echoMs == previous);
    parser.formatCurrentValue(PotTarget::EchoTime, value, sizeof(value));
    assert(strstr(value, "N/A HOLD"));
    assert(parser.parse("TEMPO ECHO_DIV 1/8"));
    assert(tempoEchoAvailable(store.snapshot().tempo));
    assert(store.snapshot().tempo.echoMs == 1000);
    assert(parser.parse("TEMPO BPM 120"));
    assert(parser.parse("POT 4 0"));
    assert(store.snapshot().tempo.echoDivision == TempoDivision::Quarter);
    assert(parser.parse("POT 4 0.26"));
    assert(store.snapshot().tempo.echoDivision == TempoDivision::Quarter);
    assert(parser.parse("POT 4 0.30"));
    assert(store.snapshot().tempo.echoDivision == TempoDivision::Eighth);
    assert(parser.parse("POT 4 0.24"));
    assert(store.snapshot().tempo.echoDivision == TempoDivision::Eighth);
    assert(parser.parse("POT 2 1"));
    assert(store.snapshot().tempo.lfoDivision == TempoDivision::TripletEighth);
    assert(parser.parse("TEMPO ECHO FREE"));
    assert(store.snapshot().tempo.lfoSync);
    assert(store.snapshot().delayMs == 250);
    assert(parser.parse("POT 4 1"));
    assert(store.snapshot().delayMs == 1000);
    assert(parser.parse("SET DELAY_MS 375"));

    assert(parser.parse("PANEL BUTTON 4 ACTION TAP_TEMPO"));
    const unsigned sequence = store.snapshot().activationSequence;
    assert(parser.parse("PAD 4 1"));
    const unsigned revision = parser.panelRevision();
    assert(parser.parse("PAD 4 1"));
    assert(parser.panelRevision() == revision);
    assert(parser.parse("PAD 4 0"));
    assert(strcmp(parser.overlayTitle(), "TAP TEMPO") == 0);
    assert(strstr(parser.overlayValue(), "TAP AGAIN"));
    assert(!store.snapshot().trigger && !store.snapshot().echoCut);
    assert(store.snapshot().activationSequence == sequence);
    ControlStore reboot;
    CommandParser restored(reboot);
    assert(restored.applyPanelSettings(parser.panelSettings()));
    assert(restored.panelSettings().buttons[3].action == ButtonAction::TapTempo);
    assert(!reboot.snapshot().tempo.known && !reboot.snapshot().tempo.lfoSync);

    assert(padLayout(parser));
    assert(parser.parse("PANEL SAVE 1"));
    assert(parser.parse("PANEL RECALL 1"));
    assert(store.snapshot().tempo.lfoSync);
    assert(parser.parse("TEMPO LFO FREE"));
    assert(parser.parse("POT 2 1"));
    assert(store.snapshot().lfoRateHz == 20);
    assert(parser.parse("PANEL RESET_DEFAULTS"));
    assert(!store.snapshot().tempo.echoSync && !store.snapshot().tempo.lfoSync);
}

void testTempoAudioContinuity() {
    auto synced = std::make_unique<AudioEngine>();
    auto reference = std::make_unique<AudioEngine>();
    synced->begin(); reference->begin();
    ControlState state;
    state.hold = true;
    state.tempo.known = state.tempo.echoSync = state.tempo.lfoSync = true;
    int16_t actual[480], expected[480];
    bool tail = false;
    for (unsigned block = 0; block < 260; ++block) {
        if (block == 60) state.tempo.bpm = 180;
        if (block == 100) {
            state.tempo.echoDivision = TempoDivision::DottedEighth;
            state.tempo.lfoDivision = TempoDivision::TripletEighth;
        }
        if (block == 160) state.hold = false;
        updateTempoTiming(state.tempo);
        auto free = state;
        free.tempo.echoSync = free.tempo.lfoSync = false;
        free.delayMs = state.tempo.echoMs;
        free.lfoRateHz = state.tempo.lfoHz;
        synced->render(state, actual, 480);
        reference->render(free, expected, 480);
        assert(memcmp(actual, expected, sizeof(actual)) == 0);
        if (block > 165 && blockRms(actual, 480) > 10) tail = true;
    }
    assert(tail);

    synced = std::make_unique<AudioEngine>();
    reference = std::make_unique<AudioEngine>();
    synced->begin(); reference->begin();
    state = ControlState{};
    padState(state);
    state.lfoShape = LfoShape::Triangle;
    state.tempo.known = state.tempo.lfoSync = true;
    state.retrigger = true;
    state.echoLevel = 0;
    for (unsigned block = 0; block < 120; ++block) {
        state.trigger = block % 30 < 20;
        updateTempoTiming(state.tempo);
        auto free = state;
        free.tempo.lfoSync = false;
        free.lfoRateHz = state.tempo.lfoHz;
        free.retrigger = false;
        synced->render(state, actual, 480);
        reference->render(free, expected, 480);
        assert(memcmp(actual, expected, sizeof(actual)) == 0);
    }
}

int main() {
    testTapTempo();
    testTempoControls();
    testTempoAudioContinuity();
    testPanelValidationAndCancelledClicks();
    testPatchTransactionAndRemoteGates();
    testVariantSaveRule();
    testRemapClearsHold();
    testPhysicalAndRemoteOwnership();
    testPadMemories();
    testPerformanceMenu();
    testColoredEchoDecay();
    testDryToneAndContour();
    testLickshotArticulations();
    testTwoTonePitchTrajectories();
    testShortTriggerEvents();
    testClassicAutomaticVoices();
    testSine1DynamicVoicing();
    testSine2DynamicVoicing();
    testSine2Stability();
    testSine1SafetyDcAndGain();
    testClassicControls();
    testClassicCalibration();
    testSelectors();
    testConfigurablePanel();
    testClassicDryRelease();
    testWetIsolation();
    testSingleRepeatAndFeedback();
    testFeedbackTailDecays();
    testReducingFeedbackStopsSustainedTail();
    testManualAndClassicVoices();
    testSmoothing();
    testOneShotEnvelope();
    testParser();
    testPadController();
    testQueuedPadActivation();
    testPanelDisplayState();
    testPowerOnPotReading();
    testExtendedEncoderGestures();
    testDynamicSweep();
    testSyndrum();
    testGunshot();
    testPotsFollowSound();
    testSonarIsNeverManual();
    testLfoShape();
    testLfoReset();
    testNewVoices();
    testSonarPulses();
    testPerformancePitch();
    testModeTransitionStartsContinuously();
    testDelay();
    testEchoLoop();
    testFractionalEchoLoopKeepsHighs();
    testEchoCutPreservesLoop();
    testToneBypassFadesOut();
    testEngine();
    testModeChangeKeepsTail();
    puts("native DSP tests passed");
}
