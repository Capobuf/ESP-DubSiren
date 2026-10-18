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
enum class PanelSound : uint8_t { Classic, TwoTone, Lickshot, Ping, Gunshot, TestTone, Count };
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
    const char *valueFormat;
    float displayScale;
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

struct SoundPatch {
    uint8_t valid = 0;
    PanelSound sound = PanelSound::Classic;
    uint8_t variant = 0;
    SoundState state;
};

struct PanelSettingsV1 {
    static constexpr uint16_t kSchemaVersion = 1;
    uint16_t schemaVersion = kSchemaVersion;
    PotBehavior potBehavior = PotBehavior::Pickup;
    EncoderHoldAction encoderHoldAction = EncoderHoldAction::LivePitch;
    uint8_t encoderStepSemitones = 1;
    uint8_t encoderRangeSemitones = 12;
    ProfilePanelSettings profiles[2];
};

struct PanelSettings : PanelSettingsV1 {
    static constexpr uint16_t kSchemaVersion = 2;
    SoundPatch patches[4];
    PanelSettings() { schemaVersion = kSchemaVersion; }
};

inline unsigned profileIndex(PerformanceProfile profile) {
    return profile == PerformanceProfile::Classic ? 0u : 1u;
}

inline const TargetDescriptor *targetDescriptor(PotTarget target) {
    static constexpr TargetDescriptor descriptors[] = {
        {PotTarget::None, "NONE", 0.0f, 1.0f, 0.0f, 1.0f,
         MappingType::Linear, false, "%.2f", 1.0f},
        {PotTarget::Pitch, "PITCH", 30.0f, 9000.0f, 80.0f, 1600.0f,
         MappingType::Logarithmic, true, "%.0f Hz", 1.0f},
        {PotTarget::Rate, "RATE", 0.05f, 20.0f, 0.15f, 20.0f,
         MappingType::Logarithmic, true, "%.2f Hz", 1.0f},
        {PotTarget::LfoDepth, "LFO DEPTH", 0.0f, 2.0f, 0.0f, 2.0f,
         MappingType::Linear, false, "%.2f oct", 1.0f},
        {PotTarget::EchoLevel, "ECHO LEVEL", 0.0f, 1.0f, 0.0f, 1.0f,
         MappingType::Linear, false, "%.0f%%", 100.0f},
        {PotTarget::EchoTime, "ECHO TIME", 50.0f, 1000.0f, 50.0f, 1000.0f,
         MappingType::Logarithmic, false, "%.0f ms", 1.0f},
        {PotTarget::EchoFeedback, "ECHO FEEDBACK", 0.0f, 1.05f, 0.0f, 1.05f,
         MappingType::Power, false, "%.0f%%", 100.0f},
        {PotTarget::EchoFilter, "ECHO FILTER", -1.0f, 1.0f, -1.0f, 1.0f,
         MappingType::Bipolar, false, "%+.0f%%", 100.0f},
        {PotTarget::Decay, "DECAY", 0.0f, 3000.0f, 0.0f, 1500.0f,
         MappingType::Power, false, "%.0f ms", 1.0f},
        {PotTarget::SweepAmount, "SWEEP AMOUNT", 0.0f, 1.0f, 0.0f, 1.0f,
         MappingType::Linear, false, "%.0f%%", 100.0f},
        {PotTarget::SweepTime, "SWEEP TIME", 50.0f, 3000.0f, 50.0f, 3000.0f,
         MappingType::Power, false, "%.0f ms", 1.0f},
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
    static const char *names[] = {"CLASSIC", "TWO TONE", "LICKSHOT", "PING", "GUNSHOT", "TEST TONE"};
    const unsigned index = static_cast<unsigned>(sound);
    return index < static_cast<unsigned>(PanelSound::Count) ? names[index]
                                                             : "UNKNOWN";
}

inline unsigned panelSoundVariantCount(PanelSound sound) {
    switch (sound) {
        case PanelSound::Classic: return 5;
        case PanelSound::TwoTone: return 3;
        case PanelSound::Lickshot: return 5;
        case PanelSound::Ping: return 3;
        case PanelSound::Gunshot: return 1;
        case PanelSound::TestTone: return 1;
        case PanelSound::Count: break;
    }
    return 0;
}

inline bool panelSoundSupportsLatch(PanelSound sound) {
    return sound != PanelSound::Ping && sound != PanelSound::Gunshot && sound != PanelSound::Count;
}

inline const char *panelSoundVariantName(PanelSound sound, unsigned variant) {
    static const char *classic[] = {"SINE 1", "SINE 2", "SONAR", "SAW", "TRIANGLE"};
    static const char *twoTone[] = {"RC", "TRIANGLE", "SQUARE"};
    static const char *lickshot[] = {"SAW DOWN", "ASYM DOWN", "SHOT", "ROLL", "HORN"};
    static const char *ping[] = {"SINE", "TRIANGLE", "SQUARE"};
    const unsigned count = panelSoundVariantCount(sound);
    if (count == 0 || variant >= count) return "UNKNOWN";
    switch (sound) {
        case PanelSound::Classic: return classic[variant];
        case PanelSound::TwoTone: return twoTone[variant];
        case PanelSound::Lickshot: return lickshot[variant];
        case PanelSound::Ping: return ping[variant];
        case PanelSound::Gunshot: return "NOISE";
        case PanelSound::TestTone: return "TEST TONE";
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
    if (!targetSupported(profile, config.target) || !finiteRange(config) ||
        config.mode > PotMode::Zones) return false;
    for (float zone : config.zones) if (!isfinite(zone)) return false;
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
    if (config.action >= ButtonAction::Count || config.latch > 1 ||
        config.sound >= PanelSound::Count ||
        config.variant >= panelSoundVariantCount(config.sound)) return false;
    if (profile == PerformanceProfile::Classic &&
        (config.action == ButtonAction::EchoLoop || config.action == ButtonAction::EchoSpin)) return false;
    if (config.action != ButtonAction::PlaySound) return true;
    if (profile == PerformanceProfile::Classic &&
        !((config.sound == PanelSound::Classic && config.variant < 2) ||
          ((config.sound == PanelSound::TwoTone || config.sound == PanelSound::TestTone) && config.variant == 0))) return false;
    return config.sound < PanelSound::Count &&
        config.variant < panelSoundVariantCount(config.sound) &&
        (!config.latch || panelSoundSupportsLatch(config.sound));
}

inline bool validateSoundPatch(const SoundPatch &patch) {
    if (patch.valid > 1) return false;
    if (!patch.valid) return true;
    const SoundState &s = patch.state;
    if (s.modulationEnabled > 1 || s.retrigger > 1 || s.voicingV2 > 1) return false;
    if (patch.sound >= PanelSound::Count || patch.variant >= panelSoundVariantCount(patch.sound) ||
        static_cast<unsigned>(s.mode) >= static_cast<unsigned>(SirenMode::Count) ||
        static_cast<unsigned>(s.lfoShape) > static_cast<unsigned>(LfoShape::Manual) ||
        static_cast<unsigned>(s.syndrumWave) > 2 || static_cast<unsigned>(s.lickshotArticulation) > 3) return false;
    if (!isfinite(s.tuneHz) || s.tuneHz < 30.0f || s.tuneHz > 9000.0f) return false;
    if (!isfinite(s.lfoRateHz) || s.lfoRateHz < 0.05f || s.lfoRateHz > 20.0f) return false;
    if (!isfinite(s.lfoDepthOctaves) || s.lfoDepthOctaves < 0.0f || s.lfoDepthOctaves > 2.0f) return false;
    if (!isfinite(s.decayMs) || s.decayMs < 0.0f || s.decayMs > 3000.0f) return false;
    if (!isfinite(s.sweepAmount) || s.sweepAmount < 0.0f || s.sweepAmount > 1.0f) return false;
    if (!isfinite(s.sweepMs) || s.sweepMs < 50.0f || s.sweepMs > 3000.0f) return false;
    if (!isfinite(s.syndrumSweepOctaves) || s.syndrumSweepOctaves < -4.0f || s.syndrumSweepOctaves > 4.0f) return false;
    if (!isfinite(s.lickshotLengthMs) || s.lickshotLengthMs < 20.0f || s.lickshotLengthMs > 1000.0f) return false;
    if (!isfinite(s.lickshotPunchOctaves) || s.lickshotPunchOctaves < 0.0f || s.lickshotPunchOctaves > 3.0f) return false;
    if (!isfinite(s.lickshotPitchMs) || s.lickshotPitchMs < 10.0f || s.lickshotPitchMs > 300.0f) return false;
    if (!isfinite(s.dryToneHz) || s.dryToneHz < 200.0f || s.dryToneHz > 19000.0f) return false;
    if (!isfinite(s.filterContourOctaves) || s.filterContourOctaves < -4.0f || s.filterContourOctaves > 4.0f) return false;
    if (!isfinite(s.filterContourMs) || s.filterContourMs < 10.0f || s.filterContourMs > 3000.0f) return false;
    return true;
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
    for (const auto &patch : settings.patches) if (!validateSoundPatch(patch)) return false;
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

inline bool migratePanelSettings(const PanelSettingsV1 &legacy, PanelSettings &result) {
    if (legacy.schemaVersion != 1) return false;
    static_cast<PanelSettingsV1 &>(result) = legacy;
    result.schemaVersion = PanelSettings::kSchemaVersion;
    for (auto &patch : result.patches) patch = SoundPatch{};
    // These legacy Classic actions never affected audio; migration makes that explicit.
    for (auto &button : result.profiles[0].buttons)
        if (button.action == ButtonAction::EchoLoop || button.action == ButtonAction::EchoSpin)
            button.action = ButtonAction::None;
    return validatePanelSettings(result);
}

constexpr unsigned kSoundPatchValueCount = 23;
inline float soundPatchValue(const SoundPatch &patch, unsigned index) {
    switch (index) {
        case 0: return static_cast<float>(patch.valid);
        case 1: return static_cast<float>(patch.sound);
        case 2: return static_cast<float>(patch.variant);
        case 3: return static_cast<float>(patch.state.mode);
        case 4: return static_cast<float>(patch.state.lfoShape);
        case 5: return static_cast<float>(patch.state.syndrumWave);
        case 6: return static_cast<float>(patch.state.lickshotArticulation);
        case 7: return static_cast<float>(patch.state.modulationEnabled);
        case 8: return static_cast<float>(patch.state.retrigger);
        case 9: return static_cast<float>(patch.state.voicingV2);
        case 10: return static_cast<float>(patch.state.tuneHz);
        case 11: return static_cast<float>(patch.state.lfoRateHz);
        case 12: return static_cast<float>(patch.state.lfoDepthOctaves);
        case 13: return static_cast<float>(patch.state.decayMs);
        case 14: return static_cast<float>(patch.state.sweepAmount);
        case 15: return static_cast<float>(patch.state.sweepMs);
        case 16: return static_cast<float>(patch.state.syndrumSweepOctaves);
        case 17: return static_cast<float>(patch.state.lickshotLengthMs);
        case 18: return static_cast<float>(patch.state.lickshotPunchOctaves);
        case 19: return static_cast<float>(patch.state.lickshotPitchMs);
        case 20: return static_cast<float>(patch.state.dryToneHz);
        case 21: return static_cast<float>(patch.state.filterContourOctaves);
        case 22: return static_cast<float>(patch.state.filterContourMs);
        default: return 0;
    }
}

inline bool decodeSoundPatch(const float *values, SoundPatch &patch) {
    if (!isfinite(values[0])) return false;
    if (values[0] < 0 || values[0] > 1 || floorf(values[0]) != values[0]) return false;
    patch.valid = static_cast<uint8_t>(values[0]);
    if (!isfinite(values[1])) return false;
    if (values[1] < 0 || values[1] > 5 || floorf(values[1]) != values[1]) return false;
    patch.sound = static_cast<PanelSound>(values[1]);
    if (!isfinite(values[2])) return false;
    if (values[2] < 0 || values[2] > 4 || floorf(values[2]) != values[2]) return false;
    patch.variant = static_cast<uint8_t>(values[2]);
    if (!isfinite(values[3])) return false;
    if (values[3] < 0 || values[3] > 9 || floorf(values[3]) != values[3]) return false;
    patch.state.mode = static_cast<SirenMode>(values[3]);
    if (!isfinite(values[4])) return false;
    if (values[4] < 0 || values[4] > 9 || floorf(values[4]) != values[4]) return false;
    patch.state.lfoShape = static_cast<LfoShape>(values[4]);
    if (!isfinite(values[5])) return false;
    if (values[5] < 0 || values[5] > 2 || floorf(values[5]) != values[5]) return false;
    patch.state.syndrumWave = static_cast<SyndrumWave>(values[5]);
    if (!isfinite(values[6])) return false;
    if (values[6] < 0 || values[6] > 3 || floorf(values[6]) != values[6]) return false;
    patch.state.lickshotArticulation = static_cast<LickshotArticulation>(values[6]);
    if (!isfinite(values[7])) return false;
    if (values[7] < 0 || values[7] > 1 || floorf(values[7]) != values[7]) return false;
    patch.state.modulationEnabled = static_cast<bool>(values[7]);
    if (!isfinite(values[8])) return false;
    if (values[8] < 0 || values[8] > 1 || floorf(values[8]) != values[8]) return false;
    patch.state.retrigger = static_cast<bool>(values[8]);
    if (!isfinite(values[9])) return false;
    if (values[9] < 0 || values[9] > 1 || floorf(values[9]) != values[9]) return false;
    patch.state.voicingV2 = static_cast<bool>(values[9]);
    if (!isfinite(values[10])) return false;
    patch.state.tuneHz = static_cast<float>(values[10]);
    if (!isfinite(values[11])) return false;
    patch.state.lfoRateHz = static_cast<float>(values[11]);
    if (!isfinite(values[12])) return false;
    patch.state.lfoDepthOctaves = static_cast<float>(values[12]);
    if (!isfinite(values[13])) return false;
    patch.state.decayMs = static_cast<float>(values[13]);
    if (!isfinite(values[14])) return false;
    patch.state.sweepAmount = static_cast<float>(values[14]);
    if (!isfinite(values[15])) return false;
    patch.state.sweepMs = static_cast<float>(values[15]);
    if (!isfinite(values[16])) return false;
    patch.state.syndrumSweepOctaves = static_cast<float>(values[16]);
    if (!isfinite(values[17])) return false;
    patch.state.lickshotLengthMs = static_cast<float>(values[17]);
    if (!isfinite(values[18])) return false;
    patch.state.lickshotPunchOctaves = static_cast<float>(values[18]);
    if (!isfinite(values[19])) return false;
    patch.state.lickshotPitchMs = static_cast<float>(values[19]);
    if (!isfinite(values[20])) return false;
    patch.state.dryToneHz = static_cast<float>(values[20]);
    if (!isfinite(values[21])) return false;
    patch.state.filterContourOctaves = static_cast<float>(values[21]);
    if (!isfinite(values[22])) return false;
    patch.state.filterContourMs = static_cast<float>(values[22]);
    return validateSoundPatch(patch);
}
