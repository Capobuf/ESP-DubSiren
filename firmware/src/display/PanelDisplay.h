#pragma once

#include <Adafruit_ST7789.h>
#include <Arduino.h>

#include "control/CommandParser.h"
#include "control/ControlState.h"

class PanelDisplay {
public:
    PanelDisplay(ControlStore &controls, CommandParser &controller);

    bool begin();
    void update(bool linked);
    bool ready() const { return ready_; }
    uint32_t lastRenderUs() const { return lastRenderUs_; }
    uint32_t maximumRenderUs() const { return maximumRenderUs_; }

private:
    enum class Focus : uint8_t {
        None,
        Tune,
        Rate,
        Depth,
        EchoLevel,
        EchoTime,
        EchoFeedback,
        EchoTone,
        Master,
    };

    static constexpr uint32_t kRefreshMs = 100;
    static constexpr uint32_t kOverlayDurationMs = 1100;

    void renderHome(const ControlState &state, bool linked);
    void renderMenu(const ControlState &state);
    void drawText(int16_t x, int16_t y, const String &text, uint8_t size,
                  uint16_t color);
    bool captureOverlay(const ControlState &before, const ControlState &after);

    ControlStore &controls_;
    CommandParser &controller_;
    Adafruit_ST7789 tft_;
    bool ready_ = false;
    bool firstRender_ = true;
    bool lastLinked_ = false;
    PerformanceController::DisplayMode lastMode_ =
        PerformanceController::DisplayMode::Home;
    uint32_t lastRefreshMs_ = 0;
    uint32_t overlayUntilMs_ = 0;
    uint32_t lastRenderUs_ = 0;
    uint32_t maximumRenderUs_ = 0;
    unsigned lastRevision_ = 0;
    ControlState lastState_;
    String overlayValue_;
    String overlayLabel_;
    Focus focus_ = Focus::None;
};
