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

float blockRms(const int16_t *samples, size_t count) {
    double energy = 0.0;
    for (size_t i = 0; i < count; ++i) {
        energy += static_cast<double>(samples[i]) * samples[i];
    }
    return static_cast<float>(sqrt(energy / count));
}

float estimateFrequency(const int16_t *samples, size_t count) {
    unsigned crossings = 0;
    for (size_t i = 1; i < count; ++i) {
        if (samples[i - 1] <= 0 && samples[i] > 0) ++crossings;
    }
    return crossings * kRate / static_cast<float>(count);
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
    assert(unchanged.mode == SirenMode::Sonar && !unchanged.modulationEnabled &&
           unchanged.retrigger && unchanged.echoSpin && unchanged.echoLoop);
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
    state.echoCut = true;
    engine->render(state, block, 480);
    assert(blockRms(block, 480) == 0.0f);
    state.echoCut = false;
    engine->render(state, block, 480);
    assert(blockRms(block, 480) > 100.0f);
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
