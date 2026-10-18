#pragma once

#include <Arduino.h>
#include "CommandParser.h"

// Native GPIO/ADC/PCNT adapter for the saved schematic, enabled explicitly.
class PanelInputs {
public:
    PanelInputs(ControlStore &controls, CommandParser &parser)
        : controls_(controls), parser_(parser) {}
    bool begin();
    void poll();
    bool ready() const { return ready_; }
    uint32_t maximumPollGapUs() const { return maximumPollGapUs_; }
private:
    struct Button { bool raw = false, stable = false, armed = false; uint32_t changed = 0; };
    ControlStore &controls_;
    CommandParser &parser_;
    Button buttons_[5];
    bool ready_ = false;
    unsigned channel_ = 6, nextPot_ = 0;
    uint32_t selectedAt_ = 0, lastPoll_ = 0, maximumPollGapUs_ = 0;
    int lastMillivolts_[7] = {-1, -1, -1, -1, -1, -1, -1};
    int16_t previousEncoderCount_ = 0;
    int encoderRemainder_ = 0;
    void selectChannel(unsigned channel);
};
