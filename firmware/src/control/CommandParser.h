#pragma once

#include <Arduino.h>

#include "ControlState.h"
#include "PerformanceController.h"

class CommandParser {
public:
    explicit CommandParser(ControlStore &controls)
        : controls_(controls), performance_(controls) {}
    bool parse(const String &command);

    unsigned panelRevision() const { return performance_.revision(); }
    unsigned panelMenuRevision() const { return performance_.menuRevision(); }
    unsigned selectedPad() const { return performance_.selectedPad(); }
    unsigned currentPad() const { return performance_.currentPad(); }
    int activePad() const { return performance_.activePad(); }
    const char *padSoundName(unsigned pad) const {
        return performance_.padSoundName(pad);
    }
    const char *padVariantName(unsigned pad) const {
        return performance_.padVariantName(pad);
    }
    unsigned padVariant(unsigned pad) const {
        return performance_.padVariant(pad);
    }
    bool padHold(unsigned pad) const { return performance_.padHold(pad); }
    bool padHoldSupported(unsigned pad) const {
        return performance_.padHoldSupported(pad);
    }

private:
    static bool parseFloat(const String &text, float &value);
    static bool parseBool(const String &text, bool &value);
    static bool parseMode(const String &text, SirenMode &mode);
    static bool parseLfoShape(const String &text, LfoShape &shape);
    static bool parseSyndrumWave(const String &text, SyndrumWave &wave);
    bool parsePanelCommand(const String &command);

    ControlStore &controls_;
    PerformanceController performance_;
};
