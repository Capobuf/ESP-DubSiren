#pragma once

#include <Adafruit_ST7789.h>
#include <Arduino.h>

#include <memory>

#include "control/CommandParser.h"
#include "control/ControlState.h"

// Home is a map of the front panel, which has no silkscreen: the top and
// bottom rows stand for the pots above and below the display, the right cell
// for the encoder beside it and the last row for the four keys.
class PanelDisplay {
public:
    PanelDisplay(ControlStore &controls, CommandParser &controller);

    bool begin();
    void update(bool linked);
    uint32_t lastRenderUs() const { return lastRenderUs_; }
    uint32_t maximumRenderUs() const { return maximumRenderUs_; }

private:
    // Classic GFX font scaled per axis; bold draws it twice, one pixel apart.
    struct Style {
        uint8_t sx, sy;
        bool bold;
    };

    static constexpr uint32_t kRefreshMs = 100;
    static constexpr uint32_t kOverlayDurationMs = 1100;

    void renderHome(const ControlState &state, bool linked);
    void renderMenu();
    void drawPot(unsigned pot, int16_t x, int16_t y, const ControlState &state);
    void drawBar(unsigned pot, int16_t x, int16_t y, int16_t w,
                 const ControlState &state, uint16_t on, uint16_t off);
    void drawBanner(const ControlState &state, bool linked);
    void drawEncoder(int16_t x, int16_t y);
    void drawKey(unsigned button, int16_t x, int16_t y, const ControlState &state);
    void text(int16_t x, int16_t y, const char *value, Style style, uint16_t color);
    // Largest style of the list that fits; the last one is cut to fit.
    Style fitted(char *value, int16_t width, const Style *styles, unsigned count) const;
    static int16_t textWidth(const char *value, Style style);
    static bool displayedStateChanged(const ControlState &before,
                                      const ControlState &after);

    ControlStore &controls_;
    CommandParser &controller_;
    Adafruit_ST7789 tft_;
    // Full frame drawn off screen, then sent at once: no visible clear.
    std::unique_ptr<GFXcanvas16> canvas_;
    Adafruit_GFX *gfx_ = nullptr;
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
    bool overlayActive_ = false;
    unsigned overlayPot_ = 0;
    char overlayTitle_[24] = "";
    char overlayValue_[40] = "";
};
