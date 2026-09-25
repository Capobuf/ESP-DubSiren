#include <assert.h>
#include <math.h>
#include <memory>
#include <stdint.h>
#include <stdio.h>

#include "audio/AudioEngine.h"
#include "audio/Delay.h"
#include "audio/Lfo.h"
#include "audio/Oscillator.h"
#include "audio/SmoothedParameter.h"
#include "control/CommandParser.h"

constexpr float kRate = 48000.0f;

void testSmoothing() {
    SmoothedParameter parameter(0.0f);
    parameter.configure(15.0f, kRate);
    parameter.setTarget(1.0f);
    assert(parameter.next() > 0.0f && parameter.current() < 0.01f);
    for (int i = 0; i < 10000; ++i) assert(isfinite(parameter.next()));
    assert(fabsf(parameter.current() - 1.0f) < 0.0001f);
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
            const float wet = delay->process(dry, feedback, kRate);
            assert(isfinite(wet) && fabsf(wet) < 8.0f);
        }
    }
    for (float target : {100.0f, 800.0f, 100.0f}) {
        delay->setParameters(target, 50.0f, 19000.0f, kRate);
        for (int i = 0; i < 48000; ++i) {
            if (i == 24000) delay->setParameters(target, 7000.0f, 200.0f, kRate);
            const float wet = delay->process(0.2f,
                i < 24000 ? 0.5f : 1.05f, kRate);
            assert(isfinite(wet) && fabsf(wet) < 8.0f);
        }
    }
}

void testEngine() {
    auto engine = std::make_unique<AudioEngine>();
    engine->begin();
    ControlState state;
    state.hold = true;
    state.feedback = 0.95f;
    int16_t block[480];
    for (int turn = 0; turn < 500; ++turn) {
        state.profile = turn % 80 < 40 ? PerformanceProfile::Classic
                                        : PerformanceProfile::Extended;
        state.classicPitch = static_cast<ClassicPitch>(turn % 3);
        state.classicModulation = static_cast<ClassicModulation>(turn % 4);
        state.mode = static_cast<SirenMode>(turn % 4);
        state.lfoShape = static_cast<LfoShape>(turn % 10);
        state.delayMs = turn % 2 ? 100.0f : 800.0f;
        state.highPassHz = turn % 2 ? 50.0f : 7000.0f;
        state.lowPassHz = turn % 2 ? 19000.0f : 200.0f;
        engine->render(state, block, 480);
        for (int16_t sample : block) assert(sample >= -32767 && sample <= 32767);
    }
}

void testProfileKeepsTail() {
    auto engine = std::make_unique<AudioEngine>();
    engine->begin();
    ControlState state;
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

int main() {
    testSmoothing();
    testParser();
    testLfoShape();
    testModeTransitionStartsContinuously();
    testDelay();
    testEngine();
    testProfileKeepsTail();
    puts("native DSP tests passed");
}
