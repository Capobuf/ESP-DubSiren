#pragma once

#include <Arduino.h>

#include "ControlState.h"

class CommandParser {
public:
    explicit CommandParser(ControlStore &controls) : controls_(controls) {}
    bool parse(const String &command);

private:
    static bool parseFloat(const String &text, float &value);
    static bool parseBool(const String &text, bool &value);
    static bool parseMode(const String &text, SirenMode &mode);
    static bool parseLfoShape(const String &text, LfoShape &shape);

    ControlStore &controls_;
};
