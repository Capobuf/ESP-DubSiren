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
    if (command.startsWith("PAD ") || command.startsWith("PAD_SELECT ") ||
        command.startsWith("PAD_ASSIGN ") ||
        command.startsWith("PAD_VARIANT ") || command == "PAD_HOLD_TOGGLE" ||
        command.startsWith("PAD_CONFIG ") ||
        command.startsWith("ENCODER_SWITCH ") || command == "PANEL_RESET") {
        return parsePanelCommand(command);
    }
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
        } else if (name == "PROFILE") {
            if (valueText == "CLASSIC") state.profile = PerformanceProfile::Classic;
            else if (valueText == "EXTENDED") state.profile = PerformanceProfile::Extended;
            else return false;
        } else if (name == "CLASSIC_PITCH") {
            if (valueText == "LOW") state.classicPitch = ClassicPitch::Low;
            else if (valueText == "MID") state.classicPitch = ClassicPitch::Mid;
            else if (valueText == "HIGH") state.classicPitch = ClassicPitch::High;
            else return false;
        } else if (name == "CLASSIC_MOD") {
            if (valueText == "SLOW") state.classicModulation = ClassicModulation::Slow;
            else if (valueText == "MEDIUM") state.classicModulation = ClassicModulation::Medium;
            else if (valueText == "FAST") state.classicModulation = ClassicModulation::Fast;
            else if (valueText == "MANUAL") state.classicModulation = ClassicModulation::Manual;
            else return false;
        } else if (name == "LFO_SHAPE") {
            if (!parseLfoShape(valueText, state.lfoShape)) return false;
        } else if (name == "SYNDRUM_WAVE") {
            if (!parseSyndrumWave(valueText, state.syndrumWave)) return false;
        } else if (name == "VOICING") {
            if (valueText == "V2") state.voicingV2 = true;
            else if (valueText == "LEGACY") state.voicingV2 = false;
            else return false;
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
        } else if (name == "SWEEP") {
            if (value < 0.0f || value > 1.0f) return false;
            state.sweepAmount = value;
        } else if (name == "SWEEP_MS") {
            if (value < 50.0f || value > 3000.0f) return false;
            state.sweepMs = value;
        } else if (name == "SYNDRUM_SWEEP") {
            if (value < -3.0f || value > 3.0f) return false;
            state.syndrumSweepOctaves = value;
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
            if (value && !state.trigger) {
                recordActivation(state);
            }
            state.trigger = value;
        } else if (name == "HOLD") {
            state.hold = value;
        } else if (name == "MOD_UP") {
            state.modUp = value;
        } else if (name == "MOD_DOWN") {
            state.modDown = value;
        } else if (name == "ECHO_CUT") {
            state.echoCut = value;
        } else if (name == "MODULATION") {
            state.modulationEnabled = value;
        } else if (name == "RETRIGGER") {
            state.retrigger = value;
        } else if (name == "ECHO_SPIN") {
            state.echoSpin = value;
        } else if (name == "ECHO_LOOP") {
            state.echoLoop = value;
        } else {
            return false;
        }
    }

    controls_.set(state);
    return true;
}

bool CommandParser::parsePanelCommand(const String &command) {
    if (command == "PANEL_RESET") {
        performance_.resetMomentaries();
        return true;
    }
    if (command == "PAD_HOLD_TOGGLE") {
        performance_.toggleSelectedHold();
        return true;
    }
    if (command.startsWith("ENCODER_SWITCH ")) {
        bool pressed;
        if (!parseBool(command.substring(15), pressed)) return false;
        performance_.setEncoderPressed(pressed);
        return true;
    }
    if (command.startsWith("PAD_SELECT ")) {
        float pad;
        if (!parseFloat(command.substring(11), pad) || pad != static_cast<int>(pad)) {
            return false;
        }
        return performance_.selectPad(static_cast<unsigned>(pad));
    }
    if (command.startsWith("PAD_ASSIGN ")) {
        const String direction = command.substring(11);
        if (direction == "PREV") performance_.stepAssignment(-1);
        else if (direction == "NEXT") performance_.stepAssignment(1);
        else return false;
        return true;
    }
    if (command.startsWith("PAD_VARIANT ")) {
        const String direction = command.substring(12);
        if (direction == "PREV") performance_.stepVariant(-1);
        else if (direction == "NEXT") performance_.stepVariant(1);
        else return false;
        return true;
    }
    if (command.startsWith("PAD_CONFIG ")) {
        const String arguments = command.substring(11);
        const int first = arguments.indexOf(' ');
        const int second = first < 0 ? -1 : arguments.indexOf(' ', first + 1);
        const int third = second < 0 ? -1 : arguments.indexOf(' ', second + 1);
        if (first < 0 || second < 0 || third < 0) return false;
        float pad;
        float variant;
        bool hold;
        if (!parseFloat(arguments.substring(0, first), pad) ||
            pad != static_cast<int>(pad) ||
            !parseFloat(arguments.substring(second + 1, third), variant) ||
            variant != static_cast<int>(variant) ||
            !parseBool(arguments.substring(third + 1), hold)) return false;
        return performance_.configurePad(
            static_cast<unsigned>(pad), arguments.substring(first + 1, second),
            static_cast<unsigned>(variant), hold);
    }
    if (command.startsWith("PAD ")) {
        const String arguments = command.substring(4);
        const int separator = arguments.indexOf(' ');
        if (separator < 0) return false;
        float pad;
        bool pressed;
        if (!parseFloat(arguments.substring(0, separator), pad) ||
            pad != static_cast<int>(pad) ||
            !parseBool(arguments.substring(separator + 1), pressed)) return false;
        return performance_.setPad(static_cast<unsigned>(pad), pressed);
    }
    return false;
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
    else if (text == "LICKSHOT") mode = SirenMode::Lickshot;
    else if (text == "SONAR") mode = SirenMode::Sonar;
    else if (text == "SYNDRUM") mode = SirenMode::Syndrum;
    else if (text == "GUNSHOT") mode = SirenMode::Gunshot;
    else return false;
    return true;
}

bool CommandParser::parseSyndrumWave(const String &text, SyndrumWave &wave) {
    if (text == "SINE") wave = SyndrumWave::Sine;
    else if (text == "TRIANGLE") wave = SyndrumWave::Triangle;
    else if (text == "SQUARE") wave = SyndrumWave::Square;
    else return false;
    return true;
}

bool CommandParser::parseLfoShape(const String &text, LfoShape &shape) {
    if (text == "CLASSIC") shape = LfoShape::Classic;
    else if (text == "TRIANGLE") shape = LfoShape::Triangle;
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
