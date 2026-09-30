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
    assert(state.profile == PerformanceProfile::Extended && state.tuneHz == 333.0f);

    assert(parser.parse("SET MODE LICKSHOT"));
    assert(store.snapshot().mode == SirenMode::Lickshot);
    assert(parser.parse("SET MODE SONAR"));
    assert(store.snapshot().mode == SirenMode::Sonar);
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
    assert(!parser.padHold(1));
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
    assert(parser.displayWidth() == 320 && parser.displayHeight() == 170);
    assert(strcmp(parser.displayModeName(), "HOME") == 0);
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(fabsf(store.snapshot().lfoDepthOctaves - 1.05f) < 0.0001f);

    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(strcmp(parser.displayModeName(), "MENU") == 0);
    assert(parser.menuIndex() == 0 && !parser.menuEditing());
    assert(parser.menuItemCount() == 7);
    assert(strcmp(parser.menuItemKey(0), "PAD") == 0);

    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(parser.menuIndex() == 1);
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(parser.menuEditing());
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(strcmp(parser.padSoundName(1), "TWO TONE") == 0);
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(!parser.menuEditing());

    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(strcmp(parser.menuItemKey(parser.menuIndex()), "VARIANT") == 0);
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(strcmp(parser.padVariantName(1), "TRIANGLE") == 0);
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("ENCODER_SWITCH 0"));

    for (int i = 0; i < 4; ++i) assert(parser.parse("ENCODER_ROTATE NEXT"));
    assert(strcmp(parser.menuItemKey(parser.menuIndex()), "BACK") == 0);
    assert(parser.parse("ENCODER_SWITCH 1"));
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(strcmp(parser.displayModeName(), "HOME") == 0);
}

void testDynamicSweep() {
    ControlState state;
    state.profile = PerformanceProfile::Extended;
    state.mode = SirenMode::Sonar;
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
    for (SirenMode mode : {SirenMode::Lickshot, SirenMode::Sonar}) {
        Oscillator oscillator;
        double energy = 0.0;
        int crossings = 0;
        float previous = 0.0f;
        for (int i = 0; i < 48000; ++i) {
            const float value = oscillator.next(mode, 220.0f, 0.25f, 1.0f,
                                                true, kRate, true);
            assert(isfinite(value));
            energy += value * value;
            if (previous <= 0.0f && value > 0.0f) ++crossings;
            previous = value;
        }
        assert(energy > 100.0);
        if (mode == SirenMode::Sonar) {
            assert(crossings >= 219 && crossings <= 221);
        }
    }
}

void testPerformancePitch() {
    ControlState state;
    state.profile = PerformanceProfile::Extended;
    state.mode = SirenMode::Sonar;
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
    assert(parser.parse("PAD_CONFIG 1 LICKSHOT 0 0"));
    assert(store.snapshot().profile == PerformanceProfile::Classic);
    for (int i = 0; i < 4; ++i) {
        assert(static_cast<int>(store.snapshot().mode) == i);
        assert(parser.parse("ENCODER_ROTATE NEXT"));
        assert(store.snapshot().lfoDepthOctaves == 1.0f);
        assert(store.snapshot().tuneHz == 220.0f);
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
    assert(strcmp(parser.displayModeName(), "MENU") == 0);
    assert(strcmp(parser.menuItemKey(0), "PROFILE") == 0);
    assert(parser.parse("ENCODER_SWITCH 0"));
    assert(parser.parse("ENCODER_SWITCH 1"));
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
    assert(memcmp(left, right, sizeof(left)) == 0);
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
            const float value = osc.next(static_cast<SirenMode>(voice), 220, 0, 1, true, kRate, true);
            real += value * cos(TWO_PI * 440.0 * i / kRate);
            imaginary += value * sin(TWO_PI * 440.0 * i / kRate);
            energy += value * value;
        }
        assert(energy > 5000 && energy < 15000);
        secondHarmonic[voice] = hypot(real, imaginary) / 24000;
    }
    assert(secondHarmonic[0] < .001);
    assert(secondHarmonic[1] > .1 && secondHarmonic[1] < .13);
}

int main() {
    testClassicAutomaticVoices();
    testClassicControls();
    testClassicCalibration();
    testSelectors();
    testClassicDryRelease();
    testWetIsolation();
    testSingleRepeatAndFeedback();
    testManualAndClassicVoices();
    testSmoothing();
    testOneShotEnvelope();
    testParser();
    testPadController();
    testQueuedPadActivation();
    testPanelDisplayState();
    testDynamicSweep();
    testSyndrum();
    testGunshot();
    testLfoShape();
    testLfoReset();
    testNewVoices();
    testPerformancePitch();
    testModeTransitionStartsContinuously();
    testDelay();
    testEchoLoop();
    testEchoCutPreservesLoop();
    testEngine();
    testProfileKeepsTail();
    puts("native DSP tests passed");
}
