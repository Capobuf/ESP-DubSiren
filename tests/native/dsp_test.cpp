// Zig release builds define NDEBUG; regressions must execute under -O2 too.
#ifdef NDEBUG
#undef NDEBUG
#endif
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
                        true, kRate, true);
    }

    double real[5] = {};
    double imaginary[5] = {};
    double energy = 0.0;
    double sum = 0.0;
    float peak = 0.0f;
    for (int i = 0; i < kMeasureSamples; ++i) {
        const float value = oscillator.next(
            mode, frequencyHz, modulation, 1.0f,
            true, kRate, true);
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
    assert(parser.parse("SET PROFILE CLASSIC"));
    assert(parser.parse("SET CLASSIC_PITCH HIGH"));
    assert(parser.parse("SET CLASSIC_MOD MANUAL"));
    auto state = store.snapshot();
    assert(state.profile == PerformanceProfile::Classic);
    assert(state.classicPitch == ClassicPitch::High);
    assert(state.classicModulation == ClassicModulation::Manual);
    assert(!parser.parse("SET CLASSIC_PITCH NOPE"));
    assert(!parser.parse("SET PROFILE NOPE"));
    assert(parser.parse("SET PROFILE EXTENDED"));
    state = store.snapshot();
    assert(state.profile == PerformanceProfile::Extended &&
           fabsf(state.tuneHz - PerformanceConfig::kClassicPitchHz[2]) < .001f);

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
    const auto validTransient = store.snapshot();
    assert(!parser.parse("SET SWEEP 1.1"));
    assert(!parser.parse("SET SWEEP_MS 49"));
    assert(!parser.parse("SET SYNDRUM_SWEEP -3.1"));
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
    assert(parser.parse("SET PROFILE EXTENDED"));
    assert(strcmp(parser.padSoundName(1), "CLASSIC") == 0);
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

    const unsigned menuBefore = parser.panelMenuRevision();
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("PAD 3 1"));
    assert(parser.parse("PAD 3 0"));
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(parser.panelMenuRevision() == menuBefore);
    assert(strcmp(parser.padVariantName(1), "SINE 2") == 0);
    assert(!store.snapshot().trigger);

    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("PAD 4 1"));
    assert(store.snapshot().echoCut);
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(!store.snapshot().echoCut);
    assert(parser.parse("PAD 4 0"));
    assert(!store.snapshot().trigger);

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
    assert(store.snapshot().mode == SirenMode::Sine2);
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
    assert(parser.parse("SET PROFILE EXTENDED"));
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
    engine->render(queued, block, 480);
    assert(blockRms(block, 480) > 100.0f);
}

void testPanelDisplayState() {
    ControlStore store;
    CommandParser parser(store);
    assert(parser.parse("SET PROFILE EXTENDED"));
    assert(parser.displayWidth() == 320 && parser.displayHeight() == 172);
    assert(strcmp(parser.displayModeName(), "HOME") == 0);
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(parser.padVariant(1) == 1);
    assert(store.snapshot().mode == SirenMode::Sine2);
    assert(store.snapshot().lfoDepthOctaves == 1.0f);

    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(strcmp(parser.displayModeName(), "HOME") == 0);
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(strcmp(parser.displayModeName(), "MENU") == 0);
    assert(parser.menuIndex() == 0 && !parser.menuEditing());
    assert(parser.menuItemCount() == 5);
    assert(strcmp(parser.menuTitle(), "PERFORMANCE") == 0);
    for (int i = 0; i < 3; ++i) assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(parser.parse("ENCODER_SWITCH 1")); assert(parser.parse("ENCODER_SWITCH 0"));
    assert(strcmp(parser.menuTitle(), "SETUP") == 0);
    assert(strcmp(parser.menuItemKey(0), "PROFILE") == 0);
    assert(strcmp(parser.menuItemLabel(1), "POTS") == 0);

    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(parser.menuIndex() == 1);
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(!parser.menuEditing());
    assert(strcmp(parser.menuTitle(), "POT CONTROLS") == 0);
    assert(strcmp(parser.menuItemKey(0), "BEHAVIOR") == 0);

    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(strcmp(parser.menuTitle(), "POT 1 - PITCH") == 0);
    assert(strcmp(parser.menuItemKey(0), "FUNCTION") == 0);
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(parser.menuEditing());
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(parser.panelSettings().profiles[1].pots[0].target == PotTarget::Rate);
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(!parser.menuEditing());
}

void testExtendedEncoderGestures() {
    ControlStore store;
    CommandParser parser(store);
    assert(parser.parse("SET PROFILE EXTENDED"));
    for (unsigned pad = 1; pad <= 4; ++pad) {
        char command[24];
        snprintf(command, sizeof(command), "PAD_SELECT %u", pad);
        assert(parser.parse(command));
        const float depth = store.snapshot().lfoDepthOctaves;
        const float tune = store.snapshot().tuneHz;
        const unsigned sequence = store.snapshot().activationSequence;
        const unsigned count = panelSoundVariantCount(
            parser.panelSettings().profiles[1].buttons[pad - 1].sound);
        assert(parser.parse("ENCODER_ROTATE PREV"));
        assert(parser.padVariant(pad) == count - 1);
        assert(parser.parse("ENCODER_ROTATE NEXT"));
        assert(parser.padVariant(pad) == 0);
        for (unsigned variant = 1; variant <= count; ++variant) {
            assert(parser.parse("ENCODER_ROTATE NEXT"));
            assert(parser.padVariant(pad) == variant % count);
            assert(store.snapshot().lfoDepthOctaves == depth);
            assert(store.snapshot().tuneHz == tune);
            assert(store.snapshot().liveTransposeSemitones == 0);
            assert(store.snapshot().activationSequence == sequence);
        }
    }

    assert(parser.parse("PAD 1 1"));
    const float tune = store.snapshot().tuneHz;
    const unsigned variant = parser.padVariant(1);
    const unsigned settingsRevision = parser.panelSettingsRevision();
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(fabsf(store.snapshot().lfoDepthOctaves - 1.05f) < .0001f);
    assert(parser.padVariant(1) == variant);
    assert(store.snapshot().liveTransposeSemitones == 0);
    assert(store.snapshot().tuneHz == tune);
    assert(parser.panelSettingsRevision() == settingsRevision);
    assert(strcmp(parser.overlayTitle(), "DEPTH") == 0);
    for (int i = 0; i < 50; ++i) assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(store.snapshot().lfoDepthOctaves == 2.0f);
    for (int i = 0; i < 50; ++i) assert(parser.parse("ENCODER_ROTATE PREV"));
    assert(store.snapshot().lfoDepthOctaves == 0.0f);
    assert(parser.parse("PAD 1 0"));
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(parser.padVariant(1) == (variant + 1) % 3);
    assert(store.snapshot().lfoDepthOctaves == 0.0f);

    // A latched sound is not a physically held BTN1 modifier.
    assert(parser.parse("PANEL BUTTON EXTENDED 1 SOUND CLASSIC 0 1"));
    assert(store.snapshot().hold);
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(parser.padVariant(1) == 1);
    assert(store.snapshot().lfoDepthOctaves == 0.0f);

    // Other sound buttons keep the configured transient pitch gesture.
    assert(parser.parse("PAD 2 1"));
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(store.snapshot().liveTransposeSemitones == 1);
    assert(store.snapshot().lfoDepthOctaves == 0.0f);
    assert(parser.parse("PAD 2 0"));
    assert(store.snapshot().liveTransposeSemitones == 0);
}

void testDynamicSweep() {
    ControlState state;
    state.profile = PerformanceProfile::Extended;
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
    state.profile = PerformanceProfile::Extended;
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
}

void testGunshot() {
    ControlState state;
    state.profile = PerformanceProfile::Extended;
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
                                                true, kRate, true);
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
    state.profile = PerformanceProfile::Extended;
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
    assert(manualUp > 435.0f && manualUp < 445.0f);
    assert(manualDown > 108.0f && manualDown < 112.0f);
}

void testSonarPulses() {
    for (bool modulationEnabled : {false, true}) {
        for (float rate : {1.0f, 4.0f}) {
            auto engine = std::make_unique<AudioEngine>();
            engine->begin();
            ControlState state;
            state.profile = PerformanceProfile::Extended;
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
    state.profile = PerformanceProfile::Extended;
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
                     true, kRate, true);
        transitioning.next(SirenMode::Sine1, 220.0f, 0.0f, 1.0f,
                           true, kRate, true);
    }
    const float expected = oldOnly.next(SirenMode::Sine1, 220.0f, 0.0f,
                                        1.0f, true, kRate, true);
    const float actual = transitioning.nextTransition(
        SirenMode::Sine1, SirenMode::Square, 0.0f, 220.0f, 0.0f,
        1.0f, true, kRate, true);
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

void testSharedSineVoicing() {
    // Compare the complete Classic and Extended render paths at equal effective
    // controls, including automatic/manual modulation and the legacy flag.
    for (SirenMode mode : {SirenMode::Sine1, SirenMode::Sine2}) {
        for (ClassicModulation modulation : {ClassicModulation::Medium,
                                              ClassicModulation::Manual}) {
            for (bool voicingV2 : {false, true}) {
                auto classic = std::make_unique<AudioEngine>();
                auto extended = std::make_unique<AudioEngine>();
                classic->begin();
                extended->begin();
                ControlState a;
                a.mode = mode;
                a.classicPitchUsesTune = true;
                a.classicRateUsesRate = true;
                a.tuneHz = 375.0f;
                a.lfoRateHz = 2.8f;
                a.classicModulation = modulation;
                a.lfoShape = modulation == ClassicModulation::Manual
                    ? LfoShape::Manual : LfoShape::Classic;
                a.modUp = modulation == ClassicModulation::Manual;
                a.hold = true;
                a.echoLevel = 0.0f;
                a.voicingV2 = voicingV2;
                ControlState b = a;
                b.profile = PerformanceProfile::Extended;
                int16_t first[480], second[480];
                for (int block = 0; block < 200; ++block) {
                    classic->render(a, first, 480);
                    extended->render(b, second, 480);
                    if (block < 100) continue;  // Different gate attacks settle.
                    assert(blockRms(first, 480) > 1000.0f);
                    for (int i = 0; i < 480; ++i) {
                        assert(abs(int(first[i]) - int(second[i])) <= 1);
                    }
                }
            }
        }
    }
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
                                true, kRate, true);
            }
            double sum = 0.0;
            float peak = 0.0f;
            for (int i = 0; i < 48000; ++i) {
                const float value = oscillator.next(
                    SirenMode::Sine2, frequency, modulation, 1.0f,
                    true, kRate, true);
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
                    true, kRate, true);
                assert(isfinite(value) && fabsf(value) < 2.0f);
            }
        }
    }

    Oscillator oscillator;
    for (int i = 0; i < 24000; ++i) {
        oscillator.next(SirenMode::Sine1, 375.0f, -1.0f, 1.0f,
                        true, kRate, true);
    }
    double mean = 0.0;
    for (int i = 0; i < 96000; ++i) {
        mean += oscillator.next(SirenMode::Sine1, 375.0f, -1.0f, 1.0f,
                                true, kRate, true);
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

void testEchoCutPreservesLoop() {
    auto engine = std::make_unique<AudioEngine>();
    engine->begin();
    ControlState state;
    state.profile = PerformanceProfile::Extended;
    state.mode = SirenMode::Sonar;
    state.hold = true;
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
    state.profile = PerformanceProfile::Extended;
    state.hold = true;
    state.feedback = 0.95f;
    int16_t block[480];
    for (int turn = 0; turn < 500; ++turn) {
        state.profile = turn % 80 < 40 ? PerformanceProfile::Classic
                                        : PerformanceProfile::Extended;
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

void testProfileKeepsTail() {
    auto engine = std::make_unique<AudioEngine>();
    engine->begin();
    ControlState state;
    state.profile = PerformanceProfile::Extended;
    state.hold = true;
    state.decayMs = 0.0f;
    state.feedback = 0.8f;
    int16_t block[480];
    for (int i = 0; i < 100; ++i) engine->render(state, block, 480);
    state.hold = false;
    for (int i = 0; i < 20; ++i) engine->render(state, block, 480);
    state.profile = PerformanceProfile::Classic;
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
    assert(store.snapshot().profile == PerformanceProfile::Classic);
    assert(!parser.parse("SET MODE GUNSHOT"));
    assert(!parser.parse("SET MODE SAW"));
    assert(!parser.parse("SET MODE TRIANGLE"));
    assert(store.snapshot().mode == SirenMode::Sine1);
    assert(parser.parse("PAD_CONFIG 1 LICKSHOT 0 0"));
    assert(store.snapshot().profile == PerformanceProfile::Classic);
    for (int i = 0; i < 4; ++i) {
        assert(static_cast<int>(store.snapshot().mode) == i);
        assert(parser.parse("ENCODER_ROTATE NEXT"));
        assert(store.snapshot().lfoDepthOctaves == 1.0f);
        assert(fabsf(store.snapshot().tuneHz - PerformanceConfig::kClassicPitchHz[1]) < .001f);
    }
    assert(store.snapshot().mode == SirenMode::Sine1);
    assert(parser.parse("ENCODER_ROTATE PREV"));
    assert(store.snapshot().mode == SirenMode::Square);
    assert(parser.parse("SET PROFILE EXTENDED"));
    assert(store.snapshot().mode == SirenMode::Square);
    assert(parser.parse("SET PROFILE CLASSIC"));
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
    for (int i = 0; i < 3; ++i) assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(parser.parse("ENCODER_SWITCH 1")); assert(parser.parse("ENCODER_SWITCH 0"));
    assert(strcmp(parser.menuItemKey(0), "PROFILE") == 0);
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(parser.menuEditing());
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(store.snapshot().profile == PerformanceProfile::Extended);
}

void testClassicCalibration() {
    constexpr float expected[] = {261.63f, 392.00f, 587.33f};
    constexpr float tolerance[] = {2.0f, 3.0f, 5.0f};
    float observed[3];
    ControlState state;
    state.mode = SirenMode::Sine1;
    state.classicModulation = ClassicModulation::Manual;

    for (int i = 0; i < 3; ++i) {
        assert(fabsf(PerformanceConfig::kClassicPitchHz[i] - expected[i]) <
               0.001f);
        state.classicPitch = static_cast<ClassicPitch>(i);
        observed[i] = renderFrequency(state);
        assert(fabsf(observed[i] - expected[i]) <= tolerance[i]);
    }
    assert(fabsf(observed[1] / observed[0] - 1.498f) <= 0.01498f);
    assert(fabsf(observed[2] / observed[0] - 2.245f) <= 0.02245f);

    assert(PerformanceConfig::kClassicRateHz[0] == 0.35f);
    assert(PerformanceConfig::kClassicRateHz[1] == 0.70f);
    assert(PerformanceConfig::kClassicRateHz[2] == 2.80f);
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
    assert(factory.potBehavior == PotBehavior::Pickup);
    assert(factory.profiles[0].pots[0].target == PotTarget::Pitch);
    assert(factory.profiles[0].pots[0].mode == PotMode::Zones);
    assert(factory.profiles[0].pots[1].target == PotTarget::Rate);
    assert(factory.profiles[0].buttons[0].action == ButtonAction::Trigger);
    assert(factory.profiles[0].buttons[1].action == ButtonAction::Hold);
    assert(factory.profiles[0].buttons[2].action == ButtonAction::ModUp);
    assert(factory.profiles[0].buttons[3].action == ButtonAction::EchoCut);
    for (unsigned button = 0; button < 4; ++button)
        assert(factory.profiles[1].buttons[button].action == ButtonAction::PlaySound);

    // Pickup waits for a crossing after an external parameter change.
    assert(parser.parse("SET CLASSIC_PITCH MID"));
    const float baseTune = store.snapshot().tuneHz;
    assert(parser.parse("POT 1 0"));
    assert(store.snapshot().classicPitch == ClassicPitch::Mid);
    assert(store.snapshot().tuneHz == baseTune);
    assert(parser.pickupWaiting(1) && parser.pickupDirection(1) > 0);
    assert(parser.parse("POT 1 0.9"));
    assert(store.snapshot().classicPitch == ClassicPitch::High);
    assert(!parser.pickupWaiting(1));

    assert(parser.parse("PANEL BEHAVIOR JUMP"));
    assert(parser.parse("POT 1 0"));
    assert(store.snapshot().classicPitch == ClassicPitch::Low);
    assert(fabsf(store.snapshot().tuneHz - PerformanceConfig::kClassicPitchHz[0]) < .001f);
    assert(parser.parse("POT 2 1"));
    assert(store.snapshot().classicModulation == ClassicModulation::Manual);

    // Continuous endpoints and validated ranges are owned by the C++ descriptor.
    assert(parser.parse("PANEL POT CLASSIC 1 MODE CONTINUOUS"));
    assert(!store.snapshot().pitchZonesActive);
    assert(parser.parse("PANEL POT CLASSIC 1 RANGE 100 800"));
    assert(parser.parse("POT 1 0"));
    assert(fabsf(store.snapshot().tuneHz - 100.0f) < .001f);
    assert(parser.parse("POT 1 1"));
    assert(fabsf(store.snapshot().tuneHz - 800.0f) < .01f);
    const PotConfig validRange = parser.panelSettings().profiles[0].pots[0];
    assert(!parser.parse("PANEL POT CLASSIC 1 RANGE 900 100"));
    assert(parser.panelSettings().profiles[0].pots[0].minimum == validRange.minimum);
    assert(parser.panelSettings().profiles[0].pots[0].maximum == validRange.maximum);
    assert(!parser.parse("PANEL POT CLASSIC 1 RANGE 20 100"));

    assert(parser.parse("PANEL POT CLASSIC 1 MODE ZONES"));
    assert(store.snapshot().pitchZonesActive);
    assert(parser.parse("PANEL POT CLASSIC 1 ZONES 220 440 880"));
    assert(!parser.parse("PANEL POT CLASSIC 1 ZONES 440 220 880"));
    assert(parser.panelSettings().profiles[0].pots[0].zones[1] == 440.0f);

    // Settings are independent per profile and unsupported targets are rejected.
    assert(parser.parse("PANEL POT CLASSIC 3 TARGET NONE"));
    assert(parser.parse("PANEL POT EXTENDED 3 TARGET DECAY"));
    assert(!parser.parse("PANEL POT CLASSIC 3 TARGET DECAY"));
    assert(parser.panelSettings().profiles[0].pots[2].target == PotTarget::None);
    assert(parser.panelSettings().profiles[1].pots[2].target == PotTarget::Decay);
    assert(parser.parse("SET PROFILE EXTENDED"));
    assert(!store.snapshot().pitchZonesActive);
    assert(!store.snapshot().rateZonesActive);
    assert(parser.parse("SET PROFILE CLASSIC"));
    assert(store.snapshot().pitchZonesActive);
    assert(store.snapshot().rateZonesActive);
    assert(parser.panelSettings().profiles[1].pots[2].target == PotTarget::Decay);

    // Configured momentary and toggle button semantics remain finite and explicit.
    assert(!parser.parse("PANEL BUTTON CLASSIC 1 ACTION ECHO_LOOP"));
    assert(parser.parse("PANEL BUTTON CLASSIC 4 ACTION ECHO_CUT"));
    assert(parser.parse("PAD 4 1"));
    assert(store.snapshot().echoCut);
    assert(parser.parse("PAD 4 0"));
    assert(!store.snapshot().echoCut);
    assert(!parser.parse("PANEL BUTTON CLASSIC 2 ACTION SCRIPT"));

    assert(parser.parse("SET PROFILE EXTENDED"));
    assert(parser.parse("PANEL BUTTON EXTENDED 1 SOUND CLASSIC 3 0"));
    assert(parser.parse("PAD 1 1"));
    assert(store.snapshot().mode == SirenMode::Saw);
    assert(parser.parse("PAD 1 0"));
    assert(parser.parse("PANEL BUTTON EXTENDED 1 SOUND CLASSIC 4 0"));
    assert(parser.parse("PAD 1 1"));
    assert(store.snapshot().mode == SirenMode::Triangle);
    assert(parser.parse("PAD 1 0"));
    assert(!parser.parse("PANEL BUTTON EXTENDED 1 SOUND CLASSIC 5 0"));
    assert(parser.parse("PANEL BUTTON EXTENDED 2 SOUND TWO_TONE 2 1"));
    assert(parser.parse("PANEL BUTTON EXTENDED 3 ACTION HOLD"));
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
    assert(!parser.parse("PANEL BUTTON EXTENDED 4 SOUND PING 0 1"));

    // Live pitch is transient, clamped and does not alter the stored base tune.
    assert(parser.parse("PANEL RESET_DEFAULTS"));
    assert(parser.parse("SET PROFILE CLASSIC"));
    const float storedTune = store.snapshot().tuneHz;
    assert(parser.parse("PANEL ENCODER LIVE_PITCH 1 2"));
    assert(parser.parse("PAD 1 1"));
    for (int detent = 0; detent < 5; ++detent)
        assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(store.snapshot().liveTransposeSemitones == 2);
    assert(store.snapshot().tuneHz == storedTune);
    assert(strcmp(parser.displayModeName(), "HOME") == 0);
    assert(parser.parse("PAD 1 0"));
    assert(store.snapshot().liveTransposeSemitones == 0);

    assert(parser.parse("SET PROFILE EXTENDED"));
    assert(parser.parse("PANEL BUTTON EXTENDED 2 ACTION TRIGGER"));
    assert(parser.parse("SET MODE GUNSHOT"));
    assert(parser.parse("PAD 2 1"));
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(store.snapshot().liveTransposeSemitones == 0);
    assert(strcmp(parser.overlayValue(), "N/A") == 0);
    assert(parser.parse("PAD 2 0"));

    // Rear Master remains direct and is never subject to Pickup.
    assert(parser.parse("SET MASTER 0.23"));
    assert(fabsf(store.snapshot().masterVolume - .23f) < .0001f);

    assert(parser.parse("PANEL RESET_DEFAULTS"));
    assert(parser.panelSettings().potBehavior == PotBehavior::Pickup);
    assert(parser.panelSettings().profiles[0].pots[0].mode == PotMode::Zones);
    assert(parser.panelSettings().profiles[1].buttons[3].sound == PanelSound::Ping);

    assert(parser.parse("PANEL BEGIN"));
    assert(parser.parse("PANEL POT CLASSIC 1 TARGET NONE"));
    assert(parser.panelSettings().profiles[0].pots[0].target == PotTarget::Pitch);
    assert(!parser.parse("PANEL POT CLASSIC 2 RANGE 100 1"));
    assert(!parser.parse("PANEL COMMIT"));
    assert(parser.panelSettings().profiles[0].pots[0].target == PotTarget::Pitch);
    assert(parser.parse("PANEL BEGIN"));
    assert(parser.parse("PANEL POT CLASSIC 1 TARGET NONE"));
    assert(parser.parse("PANEL COMMIT"));
    assert(parser.panelSettings().profiles[0].pots[0].target == PotTarget::None);
    assert(parser.parse("PANEL RESET_DEFAULTS"));

    ControlState transposeState;
    transposeState.mode = SirenMode::TestTone;
    transposeState.classicModulation = ClassicModulation::Manual;
    transposeState.classicPitchUsesTune = true;
    transposeState.tuneHz = 392.0f;
    transposeState.modUp = true;
    transposeState.liveTransposeSemitones = 12;
    const float transposed = renderFrequency(transposeState);
    assert(transposed > 780.0f && transposed < 788.0f);
    assert(transposeState.tuneHz == 392.0f);
}

void testClassicDryRelease() {
    for (SirenMode mode : {SirenMode::Sine1, SirenMode::Sine2, SirenMode::TestTone, SirenMode::Square}) {
        auto engine = std::make_unique<AudioEngine>();
        engine->begin();
        ControlState state;
        state.mode = mode;
        state.echoLevel = 0;
        state.decayMs = 3000; // Extended decay must have no effect.
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
    Lfo slow, fast;
    slow.reset(LfoShape::Manual); fast.reset(LfoShape::Manual);
    for (int i = 0; i < 48000; ++i) {
        const bool pressed = i > 1000 && i < 12000;
        const float a = slow.next(LfoShape::Manual, .35f, pressed, false, kRate);
        const float b = fast.next(LfoShape::Manual, 20.0f, pressed, false, kRate);
        assert(a == b && slow.gate() == pressed && fast.gate() == pressed);
        if (i > 24000) assert(fabsf(a) < 1e-6f);
    }
    ControlState state;
    state.classicModulation = ClassicModulation::Manual;
    state.mode = SirenMode::Square;
    const float low = renderFrequency(state);
    state.modUp = true;
    const float high = renderFrequency(state);
    assert(low > 194 && low < 198 && high > 779 && high < 789);
    state.mode = SirenMode::TestTone;
    assert(fabsf(renderFrequency(state) - 392) < 3);
    state.modUp = false;
    assert(renderFrequency(state) == 0);
    // Classic ignores advanced sweep, legacy switch and automatic bend.
    auto a = std::make_unique<AudioEngine>(); auto b = std::make_unique<AudioEngine>();
    a->begin(); b->begin();
    ControlState plain, advanced;
    plain.trigger = advanced.trigger = true;
    plain.echoLevel = advanced.echoLevel = 0;
    advanced.sweepAmount = 1; advanced.modUp = true;
    advanced.voicingV2 = false; advanced.modulationEnabled = false;
    int16_t left[480], right[480];
    for (int i = 0; i < 200; ++i) {
        a->render(plain, left, 480); b->render(advanced, right, 480);
        assert(memcmp(left, right, sizeof(left)) == 0);
    }
}


void testClassicAutomaticVoices() {
    for (SirenMode mode : {SirenMode::TestTone, SirenMode::Square}) {
        auto engine = std::make_unique<AudioEngine>(); engine->begin();
        ControlState state;
        state.mode = mode; state.hold = true; state.echoLevel = 0;
        state.classicModulation = ClassicModulation::Fast;
        int16_t block[4800];
        bool heardLow = false, heardHigh = false;
        for (int i = 0; i < 40; ++i) {
            engine->render(state, block, 4800);
            if (mode == SirenMode::TestTone) {
                heardLow |= blockRms(block, 4800) < 1;
                heardHigh |= blockRms(block, 4800) > 1000;
            } else {
                const float frequency = estimateFrequency(block, 4800);
                heardLow |= frequency > 194 && frequency < 198;
                heardHigh |= frequency > 779 && frequency < 789;
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
                                         1, true, kRate, true);
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
    for (const char *profile : {"SET PROFILE CLASSIC", "SET PROFILE EXTENDED"}) {
        for (int route = 0; route < 3; ++route) {
            ControlStore store; CommandParser parser(store);
            assert(parser.parse(profile));
            assert(parser.parse("SET ECHO_LEVEL 0"));
            if (route == 2) assert(parser.parse("PANEL BUTTON CLASSIC 3 ACTION TRIGGER"));
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
            assert(parser.parse("SET PROFILE EXTENDED"));
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
            assert(frequency[variant][4] > 350);
            assert(frequency[variant][9] < 160);
        }
        // RC rises quickly and rounds off; triangle continues rising; square plateaus.
        assert(frequency[0][1] > frequency[1][1] + 70);
        assert(frequency[2][1] > frequency[0][1] + 30);
        assert(frequency[1][4] > frequency[1][1] + 150);
        assert(fabsf(frequency[2][4] - frequency[2][1]) < 10);
    }
}

void testLickshotArticulations() {
    for (unsigned variant = 2; variant <= 4; ++variant) {
        ControlStore store; CommandParser parser(store);
        assert(parser.parse("SET PROFILE EXTENDED"));
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
        ControlState state; state.profile = PerformanceProfile::Extended;
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
    auto a = std::make_unique<AudioEngine>(), b = std::make_unique<AudioEngine>();
    a->begin(); b->begin();
    ControlState neutral, edited; neutral.trigger = edited.trigger = true;
    edited.dryToneHz = 200; edited.filterContourOctaves = 4;
    int16_t left[480], right[480];
    for (int i = 0; i < 100; ++i) {
        a->render(neutral, left, 480); b->render(edited, right, 480);
        assert(memcmp(left, right, sizeof(left)) == 0);
    }
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
    assert(parser.parse("SET PROFILE EXTENDED"));
    auto click = [&]() { assert(parser.parse("ENCODER_SWITCH 1")); assert(parser.parse("ENCODER_SWITCH 0")); };
    auto go = [&](const char *key) {
        unsigned steps = 0;
        while (strcmp(parser.menuItemKey(parser.menuIndex()), key) != 0) {
            assert(++steps <= parser.menuItemCount()); assert(parser.parse("ENCODER_ROTATE NEXT"));
        }
        click();
    };
    click(); go("SOUND"); go("SOUND");
    assert(parser.parse("ENCODER_ROTATE NEXT")); assert(parser.parse("ENCODER_ROTATE NEXT")); click();
    assert(store.snapshot().mode == SirenMode::Lickshot);
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
    go("SOUND"); assert(parser.parse("ENCODER_ROTATE NEXT")); assert(parser.parse("ENCODER_ROTATE NEXT")); click();
    assert(store.snapshot().mode == SirenMode::Gunshot);
    assert(parser.parse("PAD 1 1")); assert(parser.parse("PAD 1 0"));
    assert(store.snapshot().mode == SirenMode::Gunshot);
    assert(!parser.parse("PANEL BUTTON CLASSIC 1 ACTION ECHO_SPIN"));
    assert(!parser.parse("PANEL BUTTON CLASSIC 1 ACTION ECHO_LOOP"));
    assert(parser.parse("PANEL BUTTON EXTENDED 1 ACTION ECHO_CUT"));
    go("BACK"); go("BACK");
    const float depth = store.snapshot().lfoDepthOctaves;
    assert(parser.parse("PAD 1 1")); assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(store.snapshot().lfoDepthOctaves == depth);
}

void testPadMemories() {
    ControlStore store; CommandParser parser(store);
    assert(parser.parse("SET PROFILE EXTENDED"));
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
    assert(restored.parse("SET PROFILE EXTENDED"));
    assert(!reboot.snapshot().trigger && !reboot.snapshot().hold && !reboot.snapshot().echoLoop);
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
        assert(restored.parse("POT 1 0")); assert(restored.pickupWaiting(1));
    }
    PanelSettingsV1 legacy = static_cast<const PanelSettingsV1 &>(saved); legacy.schemaVersion = 1;
    legacy.profiles[0].buttons[0].action = ButtonAction::EchoLoop;
    PanelSettings migrated; assert(migratePanelSettings(legacy, migrated));
    assert(migrated.schemaVersion == 2 && !migrated.patches[0].valid);
    assert(migrated.profiles[0].buttons[0].action == ButtonAction::None);
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
}

int main() {
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
    testSharedSineVoicing();
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
    testExtendedEncoderGestures();
    testDynamicSweep();
    testSyndrum();
    testGunshot();
    testLfoShape();
    testLfoReset();
    testNewVoices();
    testSonarPulses();
    testPerformancePitch();
    testModeTransitionStartsContinuously();
    testDelay();
    testEchoLoop();
    testEchoCutPreservesLoop();
    testEngine();
    testProfileKeepsTail();
    puts("native DSP tests passed");
}
