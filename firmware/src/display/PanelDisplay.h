#pragma once

#include <Adafruit_ST7789.h>
#include <Arduino.h>

#include <memory>

#include "control/CommandParser.h"
#include "control/ControlState.h"

// Home is a map of the front panel, which has no silkscreen: the top and
// bottom rows stand for the pots above and below the display, the middle row
// shows the voice and the last row stands for the four keys.
class PanelDisplay {
public:
    PanelDisplay(ControlStore &controls, CommandParser &controller);

    bool begin();
    void update(bool linked);
    uint32_t lastRenderUs() const { return lastRenderUs_; }
    uint32_t maximumRenderUs() const { return maximumRenderUs_; }

private:
    enum class Arrow : uint8_t { Up, Down, Left, Right };

    static constexpr uint32_t kRefreshMs = 100;
    static constexpr uint32_t kOverlayDurationMs = 1100;

    void renderHome(const ControlState &state, bool linked);
    void renderMenu();
    void drawPot(unsigned pot, int16_t x, int16_t y, const ControlState &state);
    void drawBar(unsigned pot, int16_t x, int16_t y, int16_t w,
                 const ControlState &state, uint16_t on, uint16_t off);
    void drawBanner(const ControlState &state, bool linked);
    void drawKey(unsigned button, int16_t x, int16_t y, const ControlState &state);
    // Text at a baseline, in one of the GFX fonts of PanelDisplay.cpp.
    void text(int16_t x, int16_t baseline, const char *value, const GFXfont *font,
              uint16_t color);
    void arrow(int16_t x, int16_t y, int16_t size, Arrow direction, uint16_t color);
    // Largest font of the list that fits; the text is cut to fit the last one.
    static const GFXfont *fitted(char *value, int16_t width, const GFXfont *const *fonts,
                                 unsigned count);
    static int16_t textWidth(const char *value, const GFXfont *font);
    static int16_t capHeight(const GFXfont *font);
    // "ECHO FEEDBACK" becomes "ECHO" and "FEEDBACK"; false without a space.
    static bool splitFirstWord(char *value, char *rest);
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
