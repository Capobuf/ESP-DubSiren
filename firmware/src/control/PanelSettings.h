#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "audio/PerformanceConfig.h"
#include "ControlState.h"

enum class PotTarget : uint8_t {
    None,
    Pitch,
    Rate,
    LfoDepth,
    EchoLevel,
    EchoTime,
    EchoFeedback,
    EchoFilter,
    Decay,
    SweepAmount,
    SweepTime,
    Count,
};

enum class PotMode : uint8_t { Continuous, Zones };
enum class PotBehavior : uint8_t { Pickup, Jump };
enum class ButtonAction : uint8_t {
    None,
    Trigger,
    PlaySound,
    Hold,
    ModUp,
    ModDown,
    EchoCut,
    EchoLoop,
    EchoSpin,
    Count,
};
enum class PanelSound : uint8_t { Classic, TwoTone, Lickshot, Ping, Count };
enum class EncoderHoldAction : uint8_t { Off, LivePitch };
enum class MappingType : uint8_t { Linear, Logarithmic, Power, Bipolar };

struct TargetDescriptor {
    PotTarget target;
    const char *name;
    float absoluteMin;
    float absoluteMax;
    float defaultMin;
    float defaultMax;
    MappingType mapping;
    bool zones;
};

struct PotConfig {
    PotTarget target = PotTarget::None;
    PotMode mode = PotMode::Continuous;
    float minimum = 0.0f;
    float maximum = 1.0f;
    float zones[3] = {};
};

struct ButtonConfig {
    ButtonAction action = ButtonAction::None;
    PanelSound sound = PanelSound::Classic;
    uint8_t variant = 0;
    uint8_t latch = 0;
};

struct ProfilePanelSettings {
    PotConfig pots[6];
    ButtonConfig buttons[4];
};

struct PanelSettings {
    static constexpr uint16_t kSchemaVersion = 1;
    uint16_t schemaVersion = kSchemaVersion;
    PotBehavior potBehavior = PotBehavior::Pickup;
    EncoderHoldAction encoderHoldAction = EncoderHoldAction::LivePitch;
    uint8_t encoderStepSemitones = 1;
    uint8_t encoderRangeSemitones = 12;
    ProfilePanelSettings profiles[2];
};

inline unsigned profileIndex(PerformanceProfile profile) {
    return profile == PerformanceProfile::Classic ? 0u : 1u;
}

inline const TargetDescriptor *targetDescriptor(PotTarget target) {
    static constexpr TargetDescriptor descriptors[] = {
        {PotTarget::None, "NONE", 0.0f, 1.0f, 0.0f, 1.0f,
         MappingType::Linear, false},
        {PotTarget::Pitch, "PITCH", 30.0f, 9000.0f, 80.0f, 1600.0f,
         MappingType::Logarithmic, true},
        {PotTarget::Rate, "RATE", 0.05f, 20.0f, 0.15f, 20.0f,
         MappingType::Logarithmic, true},
        {PotTarget::LfoDepth, "LFO DEPTH", 0.0f, 2.0f, 0.0f, 2.0f,
         MappingType::Linear, false},
        {PotTarget::EchoLevel, "ECHO LEVEL", 0.0f, 1.0f, 0.0f, 1.0f,
         MappingType::Linear, false},
        {PotTarget::EchoTime, "ECHO TIME", 50.0f, 1000.0f, 50.0f, 1000.0f,
         MappingType::Logarithmic, false},
        {PotTarget::EchoFeedback, "ECHO FEEDBACK", 0.0f, 1.05f, 0.0f, 1.05f,
         MappingType::Power, false},
        {PotTarget::EchoFilter, "ECHO FILTER", -1.0f, 1.0f, -1.0f, 1.0f,
         MappingType::Bipolar, false},
        {PotTarget::Decay, "DECAY", 0.0f, 3000.0f, 0.0f, 1500.0f,
         MappingType::Power, false},
        {PotTarget::SweepAmount, "SWEEP AMOUNT", 0.0f, 1.0f, 0.0f, 1.0f,
         MappingType::Linear, false},
        {PotTarget::SweepTime, "SWEEP TIME", 50.0f, 3000.0f, 50.0f, 3000.0f,
         MappingType::Power, false},
    };
    const unsigned index = static_cast<unsigned>(target);
    return index < static_cast<unsigned>(PotTarget::Count)
        ? &descriptors[index] : nullptr;
}

inline bool targetSupported(PerformanceProfile profile, PotTarget target) {
    if (target == PotTarget::None) return true;
    if (profile == PerformanceProfile::Extended) return target < PotTarget::Count;
    return target == PotTarget::Pitch || target == PotTarget::Rate ||
        target == PotTarget::EchoLevel || target == PotTarget::EchoTime ||
        target == PotTarget::EchoFeedback || target == PotTarget::EchoFilter;
}

inline const char *potTargetName(PotTarget target) {
    const TargetDescriptor *descriptor = targetDescriptor(target);
    return descriptor == nullptr ? "UNKNOWN" : descriptor->name;
}

inline const char *potModeName(PotMode mode) {
    return mode == PotMode::Zones ? "ZONES" : "CONTINUOUS";
}

inline const char *potBehaviorName(PotBehavior behavior) {
    return behavior == PotBehavior::Pickup ? "PICKUP" : "JUMP";
}

inline const char *buttonActionName(ButtonAction action) {
    static const char *names[] = {"NONE", "TRIGGER", "PLAY SOUND", "HOLD",
        "MOD UP", "MOD DOWN", "ECHO CUT", "ECHO LOOP", "ECHO SPIN"};
    const unsigned index = static_cast<unsigned>(action);
    return index < static_cast<unsigned>(ButtonAction::Count) ? names[index]
                                                               : "UNKNOWN";
}

inline const char *panelSoundName(PanelSound sound) {
    static const char *names[] = {"CLASSIC", "TWO TONE", "LICKSHOT", "PING"};
    const unsigned index = static_cast<unsigned>(sound);
    return index < static_cast<unsigned>(PanelSound::Count) ? names[index]
                                                             : "UNKNOWN";
}

inline unsigned panelSoundVariantCount(PanelSound sound) {
    switch (sound) {
        case PanelSound::Classic: return 3;
        case PanelSound::TwoTone: return 3;
        case PanelSound::Lickshot: return 2;
        case PanelSound::Ping: return 3;
        case PanelSound::Count: break;
    }
    return 0;
}

inline bool panelSoundSupportsLatch(PanelSound sound) {
    return sound != PanelSound::Ping && sound != PanelSound::Count;
}

inline const char *panelSoundVariantName(PanelSound sound, unsigned variant) {
    static const char *classic[] = {"SINE 1", "SINE 2", "SONAR"};
    static const char *twoTone[] = {"RC", "TRIANGLE", "SQUARE"};
    static const char *lickshot[] = {"SAW DOWN", "ASYM DOWN"};
    static const char *ping[] = {"SINE", "TRIANGLE", "SQUARE"};
    const unsigned count = panelSoundVariantCount(sound);
    if (count == 0 || variant >= count) return "UNKNOWN";
    switch (sound) {
        case PanelSound::Classic: return classic[variant];
        case PanelSound::TwoTone: return twoTone[variant];
        case PanelSound::Lickshot: return lickshot[variant];
        case PanelSound::Ping: return ping[variant];
        case PanelSound::Count: break;
    }
    return "UNKNOWN";
}

inline bool finiteRange(const PotConfig &config) {
    const TargetDescriptor *descriptor = targetDescriptor(config.target);
    return descriptor != nullptr && isfinite(config.minimum) &&
        isfinite(config.maximum) && config.minimum < config.maximum &&
        config.minimum >= descriptor->absoluteMin &&
        config.maximum <= descriptor->absoluteMax;
}

inline bool validatePotConfig(PerformanceProfile profile,
                              const PotConfig &config) {
    if (!targetSupported(profile, config.target) || !finiteRange(config)) return false;
    const TargetDescriptor *descriptor = targetDescriptor(config.target);
    if (config.mode == PotMode::Zones) {
        if (!descriptor->zones ||
            (profile != PerformanceProfile::Classic) ||
            (config.target != PotTarget::Pitch && config.target != PotTarget::Rate)) {
            return false;
        }
        for (unsigned index = 0; index < 3; ++index) {
            if (!isfinite(config.zones[index]) ||
                config.zones[index] < descriptor->absoluteMin ||
                config.zones[index] > descriptor->absoluteMax ||
                (index > 0 && config.zones[index] <= config.zones[index - 1])) {
                return false;
            }
        }
    }
    return true;
}

inline bool validateButtonConfig(PerformanceProfile profile,
                                 const ButtonConfig &config) {
    if (config.action >= ButtonAction::Count || config.latch > 1) return false;
    if (config.action != ButtonAction::PlaySound) return true;
    if (profile == PerformanceProfile::Classic &&
        (config.sound != PanelSound::Classic || config.variant >= 2)) return false;
    return config.sound < PanelSound::Count &&
        config.variant < panelSoundVariantCount(config.sound) &&
        (!config.latch || panelSoundSupportsLatch(config.sound));
}

inline bool validatePanelSettings(const PanelSettings &settings) {
    if (settings.schemaVersion != PanelSettings::kSchemaVersion ||
        settings.potBehavior > PotBehavior::Jump ||
        settings.encoderHoldAction > EncoderHoldAction::LivePitch ||
        settings.encoderStepSemitones < 1 || settings.encoderStepSemitones > 12 ||
        settings.encoderRangeSemitones < settings.encoderStepSemitones ||
        settings.encoderRangeSemitones > 24) return false;
    for (unsigned profile = 0; profile < 2; ++profile) {
        const PerformanceProfile value = profile == 0
            ? PerformanceProfile::Classic : PerformanceProfile::Extended;
        for (const PotConfig &pot : settings.profiles[profile].pots)
            if (!validatePotConfig(value, pot)) return false;
        for (const ButtonConfig &button : settings.profiles[profile].buttons)
            if (!validateButtonConfig(value, button)) return false;
    }
    return true;
}

inline PotConfig continuousPot(PotTarget target) {
    const TargetDescriptor *descriptor = targetDescriptor(target);
    PotConfig result;
    result.target = target;
    result.mode = PotMode::Continuous;
    result.minimum = descriptor->defaultMin;
    result.maximum = descriptor->defaultMax;
    return result;
}

inline PanelSettings factoryPanelSettings() {
    PanelSettings settings;
    auto &classic = settings.profiles[0];
    classic.pots[0] = continuousPot(PotTarget::Pitch);
    classic.pots[0].mode = PotMode::Zones;
    classic.pots[0].zones[0] = PerformanceConfig::kClassicPitchHz[0];
    classic.pots[0].zones[1] = PerformanceConfig::kClassicPitchHz[1];
    classic.pots[0].zones[2] = PerformanceConfig::kClassicPitchHz[2];
    classic.pots[1] = continuousPot(PotTarget::Rate);
    classic.pots[1].mode = PotMode::Zones;
    classic.pots[1].zones[0] = PerformanceConfig::kClassicRateHz[0];
    classic.pots[1].zones[1] = PerformanceConfig::kClassicRateHz[1];
    classic.pots[1].zones[2] = PerformanceConfig::kClassicRateHz[2];
    classic.pots[2] = continuousPot(PotTarget::EchoLevel);
    classic.pots[3] = continuousPot(PotTarget::EchoTime);
    classic.pots[4] = continuousPot(PotTarget::EchoFeedback);
    classic.pots[5] = continuousPot(PotTarget::EchoFilter);
    classic.buttons[0].action = ButtonAction::Trigger;
    classic.buttons[1].action = ButtonAction::Hold;
    classic.buttons[2].action = ButtonAction::ModUp;
    classic.buttons[3].action = ButtonAction::EchoCut;

    auto &extended = settings.profiles[1];
    extended.pots[0] = continuousPot(PotTarget::Pitch);
    extended.pots[1] = continuousPot(PotTarget::Rate);
    extended.pots[2] = continuousPot(PotTarget::EchoLevel);
    extended.pots[3] = continuousPot(PotTarget::EchoTime);
    extended.pots[4] = continuousPot(PotTarget::EchoFeedback);
    extended.pots[5] = continuousPot(PotTarget::EchoFilter);
    const PanelSound sounds[] = {PanelSound::Classic, PanelSound::TwoTone,
        PanelSound::Lickshot, PanelSound::Ping};
    for (unsigned index = 0; index < 4; ++index) {
        extended.buttons[index].action = ButtonAction::PlaySound;
        extended.buttons[index].sound = sounds[index];
    }
    return settings;
}
