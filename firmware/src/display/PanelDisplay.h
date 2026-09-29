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
    bool usesPsramFramebuffer() const { return framebuffer_ != nullptr; }
    uint32_t lastRenderUs() const { return lastRenderUs_; }
    uint32_t maximumRenderUs() const { return maximumRenderUs_; }

private:
    enum class DisplayScene : uint8_t {
        BootInit,
        BootRollIn,
        BootConnect,
        BootPower,
        Home,
        Menu,
    };

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

    static constexpr uint32_t kFrameMs = 100;
    static constexpr uint32_t kOverlayDurationMs = 1100;
    static constexpr uint32_t kTransitionMs = 120;
    static constexpr uint32_t kBootDurationMs = 3350;

    void renderBoot(uint32_t elapsedMs);
    void renderHome(const ControlState &state, bool linked, uint32_t now,
                    uint8_t audioPeak);
    void renderMenu(const ControlState &state, bool glitch);
    void beginBitmapFrame(const uint16_t *background);
    void drawMaskedBitmap(int16_t x, int16_t y, const uint16_t *pixels,
                          const uint8_t *mask, uint16_t width,
                          uint16_t height);
    void presentBitmapFrame();
    void drawHomeUi(const ControlState &state, bool linked, uint8_t audioPeak,
                    Focus focus, const String &focusValue);
    void drawModule(int16_t x, int16_t y, int16_t width, int16_t height,
                    const char *label, const String &value, bool focus);
    void drawGlitch();
    void drawVu(int16_t x, int16_t y, uint8_t level, uint16_t color);
    void drawText(int16_t x, int16_t y, const String &text, uint8_t size,
                  uint16_t color);
    String menuValue(const char *key, const ControlState &state) const;
    bool captureOverlay(const ControlState &before, const ControlState &after);
    uint8_t speakerLevel(uint8_t peak);
    static float normalized(float value, float minimum, float maximum);

    ControlStore &controls_;
    CommandParser &controller_;
    Adafruit_ST7789 tft_;
    uint16_t *framebuffer_ = nullptr;
    bool ready_ = false;
    bool firstFrame_ = true;
    bool lastLinked_ = false;
    PerformanceController::DisplayMode lastMode_ =
        PerformanceController::DisplayMode::Home;
    DisplayScene scene_ = DisplayScene::BootInit;
    uint8_t speakerLevel_ = 0;
    uint32_t bootStartedMs_ = 0;
    uint32_t lastFrameMs_ = 0;
    uint32_t overlayUntilMs_ = 0;
    uint32_t transitionUntilMs_ = 0;
    uint32_t interactUntilMs_ = 0;
    uint32_t lastRenderUs_ = 0;
    uint32_t maximumRenderUs_ = 0;
    unsigned lastRevision_ = 0;
    ControlState lastState_;
    String overlayValue_;
    Focus focus_ = Focus::None;
};
