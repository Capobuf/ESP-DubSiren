#include "CommandParser.h"

#include <errno.h>
#include <stdlib.h>

namespace {

template <typename T>
T clampValue(T value, T minimum, T maximum) {
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

}  // namespace

bool CommandParser::parse(const String &rawCommand) {
    String command = rawCommand;
    command.trim();
    ControlState state = controls_.snapshot();

    if (command.startsWith("SET ")) {
        const int separator = command.indexOf(' ', 4);
        if (separator < 0) {
            return false;
        }
        const String name = command.substring(4, separator);
        const String valueText = command.substring(separator + 1);
        float value = 0.0f;

        if (name == "MODE") {
            if (!parseMode(valueText, state.mode)) return false;
        } else if (name == "LFO_SHAPE") {
            if (!parseLfoShape(valueText, state.lfoShape)) return false;
        } else if (!parseFloat(valueText, value)) {
            return false;
        } else if (name == "TUNE_HZ") {
            state.tuneHz = clampValue(value, 30.0f, 9000.0f);
        } else if (name == "LFO_RATE_HZ") {
            state.lfoRateHz = clampValue(value, 0.05f, 20.0f);
        } else if (name == "LFO_DEPTH_OCT") {
            state.lfoDepthOctaves = clampValue(value, 0.0f, 2.0f);
        } else if (name == "DECAY_MS") {
            state.decayMs = clampValue(value, 0.0f, 3000.0f);
        } else if (name == "DELAY_MS") {
            state.delayMs = clampValue(value, 50.0f, 1000.0f);
        } else if (name == "FEEDBACK") {
            state.feedback = clampValue(value, 0.0f, 1.05f);
        } else if (name == "ECHO_LEVEL") {
            state.echoLevel = clampValue(value, 0.0f, 1.0f);
        } else if (name == "HPF_HZ") {
            state.highPassHz = clampValue(value, 50.0f, 7000.0f);
        } else if (name == "LPF_HZ") {
            state.lowPassHz = clampValue(value, 200.0f, 19000.0f);
        } else if (name == "MASTER") {
            state.masterVolume = clampValue(value, 0.0f, 1.0f);
        } else {
            return false;
        }
    } else {
        const int separator = command.indexOf(' ');
        if (separator < 0) {
            return false;
        }
        const String name = command.substring(0, separator);
        bool value = false;
        if (!parseBool(command.substring(separator + 1), value)) {
            return false;
        }
        if (name == "TRIGGER") {
            state.trigger = value;
        } else if (name == "HOLD") {
            state.hold = value;
        } else if (name == "MOD_UP") {
            state.modUp = value;
        } else if (name == "MOD_DOWN") {
            state.modDown = value;
        } else if (name == "ECHO_CUT") {
            state.echoCut = value;
        } else {
            return false;
        }
    }

    controls_.set(state);
    return true;
}

bool CommandParser::parseFloat(const String &text, float &value) {
    const char *start = text.c_str();
    char *end = nullptr;
    errno = 0;
    value = strtof(start, &end);
    return start != end && *end == '\0' && errno != ERANGE && isfinite(value);
}

bool CommandParser::parseBool(const String &text, bool &value) {
    if (text == "1") {
        value = true;
        return true;
    }
    if (text == "0") {
        value = false;
        return true;
    }
    return false;
}

bool CommandParser::parseMode(const String &text, SirenMode &mode) {
    if (text == "SINE1") mode = SirenMode::Sine1;
    else if (text == "SINE2") mode = SirenMode::Sine2;
    else if (text == "TEST_TONE") mode = SirenMode::TestTone;
    else if (text == "SQUARE") mode = SirenMode::Square;
    else return false;
    return true;
}

bool CommandParser::parseLfoShape(const String &text, LfoShape &shape) {
    if (text == "TRIANGLE") shape = LfoShape::Triangle;
    else if (text == "SQUARE") shape = LfoShape::Square;
    else if (text == "SAW_UP") shape = LfoShape::SawUp;
    else if (text == "SAW_DOWN") shape = LfoShape::SawDown;
    else if (text == "ASYM_UP") shape = LfoShape::AsymUp;
    else if (text == "ASYM_DOWN") shape = LfoShape::AsymDown;
    else if (text == "PULSE_25") shape = LfoShape::Pulse25;
    else if (text == "PULSE_75") shape = LfoShape::Pulse75;
    else if (text == "MANUAL") shape = LfoShape::Manual;
    else return false;
    return true;
}
