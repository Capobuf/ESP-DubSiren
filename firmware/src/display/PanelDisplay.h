#pragma once

#include <Adafruit_ST7789.h>
#include <Arduino.h>

#include "control/CommandParser.h"
#include "control/ControlState.h"

class PanelDisplay {
public:
    PanelDisplay(ControlStore &controls, CommandParser &controller);

    bool begin();
    void update(bool linked, uint8_t audioPeak);
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

    static constexpr uint32_t kAnimationFrameMs = 100;
    static constexpr uint32_t kOverlayDurationMs = 1100;
    static constexpr uint32_t kBootDurationMs = 3300;

    void renderBoot(uint32_t elapsedMs);
    void renderHome(const ControlState &state, bool linked, uint32_t now,
                    uint8_t audioPeak);
    void renderMenu(const ControlState &state);
    void drawScene(const ControlState &state, bool linked, uint32_t now,
                   uint8_t audioPeak, Focus focus, const String &focusValue);
    void drawBackdrop();
    void drawStack(uint32_t now, uint8_t audioPeak, bool powered,
                   uint8_t buildStage);
    void drawRack(const ControlState &state, bool linked, uint32_t now,
                  uint8_t audioPeak, Focus focus, const String &focusValue,
                  bool powered, uint8_t buildStage);
    void drawSoundboy(uint32_t now, int16_t x, bool carrying, bool powered,
                      bool watchingRack);
    void drawCable(bool powered);
    void drawCabinet(int16_t x, int16_t y, int16_t width, int16_t height,
                     bool sub, uint8_t cone);
    void drawKnob(int16_t x, int16_t y, float normalized, uint16_t color);
    void drawVu(int16_t x, int16_t y, uint8_t level, uint16_t color);
    void drawFocusFrame(int16_t x, int16_t y, int16_t width, int16_t height,
                        bool active);
    void drawText(int16_t x, int16_t y, const String &text, uint8_t size,
                  uint16_t color);
    void drawCentered(int16_t y, const String &text, uint8_t size,
                      uint16_t color);
    String menuValue(const char *key, const ControlState &state) const;
    bool captureOverlay(const ControlState &before, const ControlState &after);
    static float normalized(float value, float minimum, float maximum);

    ControlStore &controls_;
    CommandParser &controller_;
    Adafruit_ST7789 tft_;
    bool ready_ = false;
    bool firstFrame_ = true;
    bool lastLinked_ = false;
    uint32_t bootStartedMs_ = 0;
    uint32_t lastFrameMs_ = 0;
    uint32_t overlayUntilMs_ = 0;
    uint32_t lastRenderUs_ = 0;
    uint32_t maximumRenderUs_ = 0;
    unsigned lastRevision_ = 0;
    ControlState lastState_;
    String overlayValue_;
    Focus focus_ = Focus::None;
};
