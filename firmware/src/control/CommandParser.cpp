#include "CommandParser.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

template <typename T>
T clampValue(T value, T minimum, T maximum) {
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

}  // namespace

bool CommandParser::parse(const String &rawCommand) {
    String command = rawCommand;
    command.trim();
    if (command.startsWith("POT ") || command.startsWith("PANEL ") ||
        command.startsWith("PAD ") || command.startsWith("PAD_SELECT ") ||
        command.startsWith("PAD_ASSIGN ") ||
        command.startsWith("PAD_VARIANT ") || command == "PAD_HOLD_TOGGLE" ||
        command.startsWith("PAD_CONFIG ") ||
        command.startsWith("ENCODER_SWITCH ") ||
        command.startsWith("ENCODER_ROTATE ") || command == "PANEL_RESET") {
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
            if (state.mode >= SirenMode::Lickshot && state.profile == PerformanceProfile::Classic) return false;
        } else if (name == "PROFILE") {
            if (valueText == "CLASSIC") performance_.setProfile(PerformanceProfile::Classic);
            else if (valueText == "EXTENDED") performance_.setProfile(PerformanceProfile::Extended);
            else return false;
            return true;
        } else if (name == "CLASSIC_PITCH") {
            if (valueText == "LOW") state.classicPitch = ClassicPitch::Low;
            else if (valueText == "MID") state.classicPitch = ClassicPitch::Mid;
            else if (valueText == "HIGH") state.classicPitch = ClassicPitch::High;
            else return false;
            state.tuneHz = performance_.classicPitchHz(
                static_cast<unsigned>(state.classicPitch));
            state.classicPitchUsesTune = true;
            state.pitchZonesActive = true;
        } else if (name == "CLASSIC_MOD") {
            if (valueText == "SLOW") state.classicModulation = ClassicModulation::Slow;
            else if (valueText == "MEDIUM" || valueText == "MID") state.classicModulation = ClassicModulation::Medium;
            else if (valueText == "FAST") state.classicModulation = ClassicModulation::Fast;
            else if (valueText == "MANUAL") state.classicModulation = ClassicModulation::Manual;
            else return false;
            if (state.classicModulation != ClassicModulation::Manual)
                state.lfoRateHz = performance_.classicRateHz(
                    static_cast<unsigned>(state.classicModulation));
            state.classicRateUsesRate = true;
            state.rateZonesActive = true;
        } else if (name == "LFO_SHAPE") {
            if (!parseLfoShape(valueText, state.lfoShape)) return false;
        } else if (name == "LICKSHOT_MODE") {
            if (state.profile != PerformanceProfile::Extended) return false;
            if (valueText == "LEGACY") state.lickshotArticulation = LickshotArticulation::Legacy;
            else if (valueText == "SHOT") state.lickshotArticulation = LickshotArticulation::Shot;
            else if (valueText == "ROLL") state.lickshotArticulation = LickshotArticulation::Roll;
            else if (valueText == "HORN") state.lickshotArticulation = LickshotArticulation::Horn;
            else return false;
        } else if (name == "SYNDRUM_WAVE") {
            if (!parseSyndrumWave(valueText, state.syndrumWave)) return false;
        } else if (name == "VOICING") {
            if (valueText == "V2") state.voicingV2 = true;
            else if (valueText == "LEGACY") state.voicingV2 = false;
            else return false;
        } else if (!parseFloat(valueText, value)) {
            return false;
        } else if (name == "PITCH_POSITION" || name == "RATE_POSITION") {
            if (value < 0.0f || value > 1.0f) return false;
            if (name == "PITCH_POSITION") {
                state.pitchPosition = value;
                state.classicPitch = static_cast<ClassicPitch>(PerformanceConfig::selectorZone(
                    value, static_cast<int>(state.classicPitch), 3));
                state.tuneHz = state.profile == PerformanceProfile::Extended
                    ? 80.0f * powf(20.0f, value)
                    : performance_.classicPitchHz(
                        static_cast<unsigned>(state.classicPitch));
                state.classicPitchUsesTune = true;
                state.pitchZonesActive = state.profile == PerformanceProfile::Classic;
            } else {
                state.ratePosition = value;
                state.classicModulation = static_cast<ClassicModulation>(PerformanceConfig::selectorZone(
                    value, static_cast<int>(state.classicModulation), 4));
                if (state.profile == PerformanceProfile::Extended)
                    state.lfoRateHz = 0.15f * powf(20.0f / 0.15f, value);
                else if (state.classicModulation != ClassicModulation::Manual)
                    state.lfoRateHz = performance_.classicRateHz(
                        static_cast<unsigned>(state.classicModulation));
                state.classicRateUsesRate = true;
                state.rateZonesActive = state.profile == PerformanceProfile::Classic;
            }
        } else if (name == "ECHO_FILTER") {
            if (value < -1.0f || value > 1.0f) return false;
            state.echoFilterPosition = value;
            using namespace PerformanceConfig;
            const float amount = echoFilterAmount(value);
            state.echoFilterFlat = amount == 0.0f;
            state.highPassHz = value > 0.0f
                ? kEchoHighPassMinHz * powf(kEchoHighPassMaxHz / kEchoHighPassMinHz, amount)
                : kEchoHighPassMinHz;
            state.lowPassHz = value < 0.0f
                ? kEchoLowPassMaxHz * powf(kEchoLowPassMinHz / kEchoLowPassMaxHz, amount)
                : kEchoLowPassMaxHz;
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
            state.delayMs = clampValue(value, PerformanceConfig::kEchoTimeMinMs,
                                       PerformanceConfig::kEchoTimeMaxMs);
        } else if (name == "FEEDBACK") {
            state.feedback = clampValue(value, 0.0f, 1.05f);
        } else if (name == "ECHO_LEVEL") {
            state.echoLevel = clampValue(value, 0.0f, 1.0f);
        } else if (name == "HPF_HZ") {
            state.echoFilterFlat = false;
            state.highPassHz = clampValue(value, 50.0f, 7000.0f);
        } else if (name == "LPF_HZ") {
            state.echoFilterFlat = false;
            state.lowPassHz = clampValue(value, 200.0f, 19000.0f);
        } else if (name == "LICKSHOT_LENGTH_MS" || name == "LICKSHOT_PUNCH_OCT" || name == "LICKSHOT_PITCH_MS") {
            if (state.profile != PerformanceProfile::Extended) return false;
            if (name == "LICKSHOT_LENGTH_MS") state.lickshotLengthMs = clampValue(value, 20.0f, 1000.0f);
            else if (name == "LICKSHOT_PUNCH_OCT") state.lickshotPunchOctaves = clampValue(value, 0.0f, 3.0f);
            else state.lickshotPitchMs = clampValue(value, 10.0f, 300.0f);
        } else if (name == "DRY_TONE_HZ" || name == "FILTER_CONTOUR_OCT" || name == "FILTER_CONTOUR_MS") {
            if (state.profile != PerformanceProfile::Extended) return false;
            if (name == "DRY_TONE_HZ") state.dryToneHz = clampValue(value, 200.0f, 19000.0f);
            else if (name == "FILTER_CONTOUR_OCT") state.filterContourOctaves = clampValue(value, -4.0f, 4.0f);
            else state.filterContourMs = clampValue(value, 10.0f, 3000.0f);
        } else if (name == "ECHO_COLOR") {
            if (state.profile != PerformanceProfile::Extended) return false;
            state.echoColor = clampValue(value, 0.0f, 1.0f);
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
    PotTarget changedTarget = PotTarget::None;
    if (command.indexOf(' ') >= 0) {
        if (command.startsWith("SET PITCH_POSITION") || command.startsWith("SET TUNE_HZ") ||
            command.startsWith("SET CLASSIC_PITCH")) changedTarget = PotTarget::Pitch;
        else if (command.startsWith("SET RATE_POSITION") || command.startsWith("SET LFO_RATE_HZ") ||
                 command.startsWith("SET CLASSIC_MOD")) changedTarget = PotTarget::Rate;
        else if (command.startsWith("SET LFO_DEPTH_OCT")) changedTarget = PotTarget::LfoDepth;
        else if (command.startsWith("SET ECHO_LEVEL")) changedTarget = PotTarget::EchoLevel;
        else if (command.startsWith("SET DELAY_MS")) changedTarget = PotTarget::EchoTime;
        else if (command.startsWith("SET FEEDBACK")) changedTarget = PotTarget::EchoFeedback;
        else if (command.startsWith("SET ECHO_FILTER")) changedTarget = PotTarget::EchoFilter;
        else if (command.startsWith("SET DECAY_MS")) changedTarget = PotTarget::Decay;
        else if (command.startsWith("SET SWEEP ")) changedTarget = PotTarget::SweepAmount;
        else if (command.startsWith("SET SWEEP_MS")) changedTarget = PotTarget::SweepTime;
    }
    if (changedTarget == PotTarget::None) performance_.notifyControlsChanged();
    else performance_.notifyExternalChange(changedTarget);
    return true;
}

bool CommandParser::parsePanelCommand(const String &command) {
    if (command.startsWith("POT ")) {
        unsigned pot = 0;
        float position = 0.0f;
        char extra = 0;
        if (sscanf(command.c_str(), "POT %u %f %c", &pot, &position, &extra) != 2)
            return false;
        return performance_.setPot(pot, position);
    }
    if (command == "PANEL RESET_DEFAULTS") {
        performance_.resetDefaults();
        return true;
    }
    if (command == "PANEL BEGIN") return performance_.beginSettingsTransaction();
    if (command == "PANEL COMMIT") return performance_.commitSettingsTransaction();
    if (command == "PANEL CANCEL") {
        performance_.cancelSettingsTransaction();
        return true;
    }
    if (command.startsWith("PANEL BEHAVIOR ")) {
        const String value = command.substring(15);
        if (value == "PICKUP") return performance_.configurePotBehavior(PotBehavior::Pickup);
        if (value == "JUMP") return performance_.configurePotBehavior(PotBehavior::Jump);
        return false;
    }
    if (command.startsWith("PANEL ENCODER ")) {
        char actionText[20] = {};
        unsigned step = 0, range = 0;
        char extra = 0;
        if (sscanf(command.c_str(), "PANEL ENCODER %19s %u %u %c",
                   actionText, &step, &range, &extra) != 3) return false;
        EncoderHoldAction action;
        if (strcmp(actionText, "OFF") == 0) action = EncoderHoldAction::Off;
        else if (strcmp(actionText, "LIVE_PITCH") == 0) action = EncoderHoldAction::LivePitch;
        else return false;
        return performance_.configureEncoder(action, step, range);
    }
    if (command.startsWith("PANEL POT ")) {
        char profileText[12] = {}, field[12] = {}, valueText[24] = {};
        unsigned pot = 0;
        PerformanceProfile profile;
        if (sscanf(command.c_str(), "PANEL POT %11s %u %11s",
                   profileText, &pot, field) != 3 ||
            !parseProfileName(profileText, profile)) return false;
        if (strcmp(field, "TARGET") == 0) {
            char extra = 0;
            if (sscanf(command.c_str(), "PANEL POT %11s %u TARGET %23s %c",
                       profileText, &pot, valueText, &extra) != 3) return false;
            PotTarget target;
            return parsePotTargetName(valueText, target) &&
                performance_.configurePotTarget(profile, pot, target);
        }
        if (strcmp(field, "MODE") == 0) {
            char extra = 0;
            if (sscanf(command.c_str(), "PANEL POT %11s %u MODE %23s %c",
                       profileText, &pot, valueText, &extra) != 3) return false;
            if (strcmp(valueText, "CONTINUOUS") == 0)
                return performance_.configurePotMode(profile, pot, PotMode::Continuous);
            if (strcmp(valueText, "ZONES") == 0)
                return performance_.configurePotMode(profile, pot, PotMode::Zones);
            return false;
        }
        if (strcmp(field, "RANGE") == 0) {
            float minimum = 0.0f, maximum = 0.0f;
            char extra = 0;
            if (sscanf(command.c_str(), "PANEL POT %11s %u RANGE %f %f %c",
                       profileText, &pot, &minimum, &maximum, &extra) != 4) return false;
            return performance_.configurePotRange(profile, pot, minimum, maximum);
        }
        if (strcmp(field, "ZONES") == 0) {
            float low = 0.0f, middle = 0.0f, high = 0.0f;
            char extra = 0;
            if (sscanf(command.c_str(), "PANEL POT %11s %u ZONES %f %f %f %c",
                       profileText, &pot, &low, &middle, &high, &extra) != 5) return false;
            return performance_.configurePotZones(profile, pot, low, middle, high);
        }
        return false;
    }
    if (command.startsWith("PANEL BUTTON ")) {
        char profileText[12] = {}, field[12] = {}, valueText[24] = {};
        unsigned button = 0;
        PerformanceProfile profile;
        if (sscanf(command.c_str(), "PANEL BUTTON %11s %u %11s",
                   profileText, &button, field) != 3 ||
            !parseProfileName(profileText, profile)) return false;
        if (strcmp(field, "ACTION") == 0) {
            char extra = 0;
            if (sscanf(command.c_str(), "PANEL BUTTON %11s %u ACTION %23s %c",
                       profileText, &button, valueText, &extra) != 3) return false;
            ButtonAction action;
            return parseButtonActionName(valueText, action) &&
                performance_.configureButtonAction(profile, button, action);
        }
        if (strcmp(field, "SOUND") == 0) {
            unsigned variant = 0, latch = 0;
            char extra = 0;
            if (sscanf(command.c_str(), "PANEL BUTTON %11s %u SOUND %23s %u %u %c",
                       profileText, &button, valueText, &variant, &latch, &extra) != 5 ||
                latch > 1) return false;
            PanelSound sound;
            return parsePanelSoundName(valueText, sound) &&
                performance_.configureButtonSound(profile, button, sound,
                                                   variant, latch != 0);
        }
        return false;
    }
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
    if (command.startsWith("ENCODER_ROTATE ")) {
        const String direction = command.substring(15);
        if (direction == "PREV") performance_.rotateEncoder(-1);
        else if (direction == "NEXT") performance_.rotateEncoder(1);
        else return false;
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
    else if (text == "SAW") mode = SirenMode::Saw;
    else if (text == "TRIANGLE") mode = SirenMode::Triangle;
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

bool CommandParser::parseProfileName(const char *text,
                                     PerformanceProfile &profile) {
    if (strcmp(text, "CLASSIC") == 0) profile = PerformanceProfile::Classic;
    else if (strcmp(text, "EXTENDED") == 0) profile = PerformanceProfile::Extended;
    else return false;
    return true;
}

bool CommandParser::parsePotTargetName(const char *text, PotTarget &target) {
    struct Entry { const char *name; PotTarget target; };
    static constexpr Entry entries[] = {
        {"NONE", PotTarget::None}, {"PITCH", PotTarget::Pitch},
        {"RATE", PotTarget::Rate}, {"LFO_DEPTH", PotTarget::LfoDepth},
        {"ECHO_LEVEL", PotTarget::EchoLevel}, {"ECHO_TIME", PotTarget::EchoTime},
        {"ECHO_FEEDBACK", PotTarget::EchoFeedback},
        {"ECHO_FILTER", PotTarget::EchoFilter}, {"DECAY", PotTarget::Decay},
        {"SWEEP_AMOUNT", PotTarget::SweepAmount},
        {"SWEEP_TIME", PotTarget::SweepTime},
    };
    for (const Entry &entry : entries) {
        if (strcmp(text, entry.name) == 0) {
            target = entry.target;
            return true;
        }
    }
    return false;
}

bool CommandParser::parseButtonActionName(const char *text,
                                          ButtonAction &action) {
    struct Entry { const char *name; ButtonAction action; };
    static constexpr Entry entries[] = {
        {"NONE", ButtonAction::None}, {"TRIGGER", ButtonAction::Trigger},
        {"PLAY_SOUND", ButtonAction::PlaySound}, {"HOLD", ButtonAction::Hold},
        {"MOD_UP", ButtonAction::ModUp}, {"MOD_DOWN", ButtonAction::ModDown},
        {"ECHO_CUT", ButtonAction::EchoCut}, {"ECHO_LOOP", ButtonAction::EchoLoop},
        {"ECHO_SPIN", ButtonAction::EchoSpin},
    };
    for (const Entry &entry : entries) {
        if (strcmp(text, entry.name) == 0) {
            action = entry.action;
            return true;
        }
    }
    return false;
}

bool CommandParser::parsePanelSoundName(const char *text, PanelSound &sound) {
    if (strcmp(text, "CLASSIC") == 0) sound = PanelSound::Classic;
    else if (strcmp(text, "TWO_TONE") == 0) sound = PanelSound::TwoTone;
    else if (strcmp(text, "LICKSHOT") == 0) sound = PanelSound::Lickshot;
    else if (strcmp(text, "PING") == 0) sound = PanelSound::Ping;
    else return false;
    return true;
}
