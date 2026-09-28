#include "PanelDisplay.h"

#include <SPI.h>

#include "Config.h"

namespace {

// A deliberately small 16-bit palette: night, wood, steel and warm electronics.
constexpr uint16_t kNight = 0x0864;
constexpr uint16_t kNightHigh = 0x10C7;
constexpr uint16_t kGround = 0x18E4;
constexpr uint16_t kBlack = 0x0021;
constexpr uint16_t kCabinet = 0x2985;
constexpr uint16_t kCabinetEdge = 0x5A88;
constexpr uint16_t kSteel = 0x4228;
constexpr uint16_t kSteelLight = 0x7BCB;
constexpr uint16_t kCream = 0xE6F1;
constexpr uint16_t kAmber = 0xFD20;
constexpr uint16_t kAmberDim = 0x8320;
constexpr uint16_t kRed = 0xD1E4;
constexpr uint16_t kGreen = 0x4666;
constexpr uint16_t kGreenBright = 0x8EC9;
constexpr uint16_t kSkin = 0xD4CC;
constexpr uint16_t kShirt = 0xB1C6;
constexpr uint16_t kDenim = 0x3A49;
constexpr uint16_t kCable = 0xD343;

bool changed(float left, float right) {
    return fabsf(left - right) > 0.0001f;
}

String decimal(float value, unsigned digits) {
    return String(value, digits);
}

}  // namespace

PanelDisplay::PanelDisplay(ControlStore &controls, CommandParser &controller)
    : controls_(controls), controller_(controller),
      tft_(&SPI, Config::kDisplayCsPin, Config::kDisplayDcPin,
           Config::kDisplayResetPin) {}

bool PanelDisplay::begin() {
    pinMode(Config::kDisplayBacklightPin, OUTPUT);
    digitalWrite(Config::kDisplayBacklightPin, HIGH);
    SPI.begin(Config::kSpiSckPin, Config::kSpiMisoPin,
              Config::kSpiMosiPin, Config::kDisplayCsPin);
    tft_.init(170, 320);
    tft_.setRotation(1);
    ready_ = tft_.width() == PerformanceController::kDisplayWidth &&
             tft_.height() == PerformanceController::kDisplayHeight;
    if (ready_) {
        tft_.fillScreen(kNight);
        bootStartedMs_ = millis();
    }
    return ready_;
}

void PanelDisplay::update(bool linked, uint8_t audioPeak) {
    if (!ready_) return;
    const uint32_t now = millis();
    if (!firstFrame_ && now - lastFrameMs_ < kAnimationFrameMs) return;

    const ControlState state = controls_.snapshot();
    const unsigned revision = controller_.panelRevision();
    const bool stateChanged = firstFrame_ || captureOverlay(lastState_, state);
    const bool panelChanged = firstFrame_ || revision != lastRevision_;
    const bool linkChanged = firstFrame_ || linked != lastLinked_;
    const bool booting = now - bootStartedMs_ < kBootDurationMs;
    const bool overlayActive = static_cast<int32_t>(overlayUntilMs_ - now) > 0;
    const bool animated = booting ||
        controller_.displayMode() == PerformanceController::DisplayMode::Home;

    if (!firstFrame_ && !animated && !stateChanged && !panelChanged &&
        !linkChanged) return;

    lastFrameMs_ = now;
    const uint32_t renderStart = micros();
    if (booting) {
        renderBoot(now - bootStartedMs_);
    } else if (controller_.displayMode() ==
               PerformanceController::DisplayMode::Menu) {
        overlayUntilMs_ = 0;
        focus_ = Focus::None;
        renderMenu(state);
    } else {
        if (!overlayActive) {
            focus_ = Focus::None;
            overlayValue_ = "";
        }
        renderHome(state, linked, now, audioPeak);
    }
    lastRenderUs_ = micros() - renderStart;
    if (lastRenderUs_ > maximumRenderUs_) maximumRenderUs_ = lastRenderUs_;

    firstFrame_ = false;
    lastState_ = state;
    lastRevision_ = revision;
    lastLinked_ = linked;
}

void PanelDisplay::renderBoot(uint32_t elapsedMs) {
    drawBackdrop();
    const uint8_t stage = elapsedMs < 650 ? 0
        : elapsedMs < 1250 ? 1
        : elapsedMs < 1850 ? 2
        : elapsedMs < 2450 ? 3
        : elapsedMs < 2850 ? 4 : 5;

    if (stage >= 1) drawStack(elapsedMs, 0, false, stage);
    if (stage >= 3) {
        const ControlState state = controls_.snapshot();
        drawRack(state, false, elapsedMs, 0, Focus::None, "", stage >= 5,
                 stage);
    }
    if (stage >= 4) drawCable(stage >= 5);

    const int16_t boyX = stage == 0
        ? static_cast<int16_t>(306 - elapsedMs / 13)
        : stage == 1 ? 171 : stage == 2 ? 228 : 274;
    drawSoundboy(elapsedMs, boyX, stage == 0, stage >= 5, stage >= 3);

    if (stage == 0) {
        drawCabinet(boyX - 42, 100, 39, 54, true, 0);
    }
    if (stage >= 5) {
        drawText(149, 148, "POWER UP", 1,
                 ((elapsedMs / 120) & 1U) ? kAmber : kGreenBright);
    }
}

void PanelDisplay::renderHome(const ControlState &state, bool linked,
                              uint32_t now, uint8_t audioPeak) {
    drawScene(state, linked, now, audioPeak, focus_, overlayValue_);
}

void PanelDisplay::drawScene(const ControlState &state, bool linked,
                             uint32_t now, uint8_t audioPeak, Focus focus,
                             const String &focusValue) {
    drawBackdrop();
    drawStack(now, audioPeak, true, 5);
    drawCable(true);
    drawRack(state, linked, now, audioPeak, focus, focusValue, true, 5);
    drawSoundboy(now, 276, false, true,
                 focus != Focus::None || controller_.activePad() != 0);
}

void PanelDisplay::drawBackdrop() {
    tft_.fillScreen(kNight);
    tft_.fillRect(0, 139, 320, 31, kGround);
    tft_.drawFastHLine(0, 138, 320, kCabinetEdge);
    for (int16_t x = 4; x < 320; x += 24) {
        tft_.drawPixel(x, 15 + ((x / 24) & 1) * 7, kNightHigh);
    }
    tft_.fillRect(5, 157, 310, 2, kBlack);
}

void PanelDisplay::drawStack(uint32_t now, uint8_t audioPeak, bool powered,
                             uint8_t buildStage) {
    if (buildStage < 1) return;
    const uint8_t cone = powered ? static_cast<uint8_t>(audioPeak / 48) : 0;
    drawCabinet(8, 95, 126, 61, true, cone);
    if (buildStage < 2) return;
    drawCabinet(14, 58, 114, 38, false, cone > 0 ? cone - 1 : 0);
    tft_.fillRect(21, 31, 100, 28, kCabinet);
    tft_.drawRect(21, 31, 100, 28, kCabinetEdge);
    tft_.fillTriangle(31, 38, 31, 51, 70, 45, kSteel);
    tft_.fillTriangle(111, 38, 111, 51, 72, 45, kSteel);
    tft_.fillRect(68, 40, 7, 10, kBlack);
    tft_.drawFastHLine(25, 55, 92, kBlack);
    const bool pulse = powered && ((now / 80) % 8) < 2;
    tft_.fillCircle(18, 36, 2, pulse ? kAmber : kAmberDim);
}

void PanelDisplay::drawCabinet(int16_t x, int16_t y, int16_t width,
                               int16_t height, bool sub, uint8_t cone) {
    tft_.fillRect(x, y, width, height, kBlack);
    tft_.drawRect(x, y, width, height, kCabinetEdge);
    tft_.drawRect(x + 3, y + 3, width - 6, height - 6, kCabinet);
    if (sub) {
        const int16_t radius = min<int16_t>(height / 2 - 7, 25);
        const int16_t cy = y + height / 2;
        for (int side = 0; side < 2; ++side) {
            const int16_t cx = x + width / 4 + side * width / 2;
            tft_.fillCircle(cx, cy, radius, kSteel);
            tft_.drawCircle(cx, cy, radius, kSteelLight);
            tft_.fillCircle(cx, cy, max<int16_t>(4, radius / 2 + cone), kBlack);
            tft_.fillCircle(cx - 3, cy - 3, 2, kCabinetEdge);
        }
    } else {
        const int16_t cy = y + height / 2;
        for (int side = 0; side < 3; ++side) {
            const int16_t cx = x + 21 + side * 36;
            tft_.fillCircle(cx, cy, 12, kSteel);
            tft_.drawCircle(cx, cy, 12, kSteelLight);
            tft_.fillCircle(cx, cy, 4 + cone / 2, kBlack);
        }
    }
    tft_.fillRect(x + 5, y + height - 4, 5, 2, kAmberDim);
}

void PanelDisplay::drawRack(const ControlState &state, bool linked,
                            uint32_t now, uint8_t audioPeak, Focus focus,
                            const String &focusValue, bool powered,
                            uint8_t buildStage) {
    if (buildStage < 3) return;
    constexpr int16_t x = 143;
    constexpr int16_t width = 122;
    tft_.fillRect(x, 28, width, 128, kBlack);
    tft_.drawRect(x, 28, width, 128, kSteelLight);
    tft_.fillRect(x + 4, 32, width - 8, 20, kSteel);
    tft_.fillRect(x + 8, 35, width - 34, 14, kNightHigh);
    drawText(x + 11, 34, controller_.padSoundName(controller_.selectedPad()),
             1, powered ? kGreenBright : kSteelLight);
    drawText(x + 11, 43, controller_.padVariantName(controller_.selectedPad()),
             1, kGreen);
    tft_.fillRect(x + width - 22, 36, 14, 12, kBlack);
    drawText(x + width - 19, 38, String("P") + controller_.selectedPad(), 1,
             kAmber);

    tft_.fillRect(x + 4, 55, width - 8, 26, kCabinet);
    drawFocusFrame(x + 5, 56, 55, 24, focus == Focus::Tune);
    drawFocusFrame(x + 61, 56, 55, 24, focus == Focus::Rate);
    drawText(x + 9, 58, "TUNE", 1, kSteelLight);
    drawText(x + 65, 58, "RATE", 1, kSteelLight);
    const String tune = focus == Focus::Tune ? focusValue
        : decimal(state.tuneHz, 0) + "H";
    const String rate = focus == Focus::Rate ? focusValue
        : decimal(state.lfoRateHz, 1) + "H";
    drawText(x + 9, 69, tune, 1, kAmber);
    drawText(x + 65, 69, rate, 1, kGreenBright);

    tft_.fillRect(x + 4, 84, width - 8, 25, kSteel);
    drawFocusFrame(x + 5, 85, width - 10, 23, focus == Focus::Depth);
    drawText(x + 9, 88, "DEPTH", 1, kSteelLight);
    drawKnob(x + 91, 96, state.lfoDepthOctaves / 2.0f,
             focus == Focus::Depth ? kAmber : kCream);
    const String depth = focus == Focus::Depth ? focusValue
        : decimal(state.lfoDepthOctaves, 2);
    drawText(x + 9, 98, depth, 1, kCream);
    if (controller_.padHold(controller_.currentPad())) {
        tft_.fillCircle(x + 72, 91, 3, kRed);
        drawText(x + 61, 99, "HOLD", 1, kRed);
    }

    tft_.fillRect(x + 4, 112, width - 8, 28, kCabinet);
    const Focus echoFocus = focus == Focus::EchoLevel ||
        focus == Focus::EchoTime || focus == Focus::EchoFeedback ||
        focus == Focus::EchoTone ? focus : Focus::None;
    drawFocusFrame(x + 5, 113, width - 10, 26, echoFocus != Focus::None);
    drawText(x + 9, 115, "T", 1,
             focus == Focus::EchoTime ? kAmber : kSteelLight);
    drawText(x + 36, 115, "FB", 1,
             focus == Focus::EchoFeedback ? kAmber : kSteelLight);
    drawText(x + 68, 115, "LV", 1,
             focus == Focus::EchoLevel ? kAmber : kSteelLight);
    drawText(x + 98, 115, "EQ", 1,
             focus == Focus::EchoTone ? kAmber : kSteelLight);
    drawKnob(x + 14, 131, normalized(state.delayMs, 20.0f, 1000.0f), kCream);
    drawKnob(x + 45, 131, state.feedback / 1.05f, kCream);
    drawKnob(x + 76, 131, state.echoLevel, kCream);
    const float tone = normalized(logf(state.highPassHz / 60.0f) -
                                  logf(state.lowPassHz / 16000.0f),
                                  0.0f, 4.2f);
    drawKnob(x + 107, 131, tone, kCream);
    if (echoFocus != Focus::None && focusValue.length() > 0) {
        tft_.fillRect(x + 7, 126, 104, 11, kNightHigh);
        drawText(x + 10, 128, focusValue, 1, kAmber);
    }

    tft_.fillRect(x + 4, 143, width - 8, 10, kSteel);
    drawFocusFrame(x + 5, 143, width - 10, 10, focus == Focus::Master);
    drawVu(x + 8, 146, powered ? audioPeak : 0,
           focus == Focus::Master ? kAmber : kGreenBright);
    drawText(x + 79, 145, state.echoLoop ? "LOOP" : "AMP", 1,
             state.echoLoop ? kRed : kSteelLight);
    tft_.fillCircle(x + 109, 148, 2,
                    powered ? (((now / 160) & 1U) ? kGreenBright : kGreen)
                            : kSteel);
    if (linked) tft_.drawPixel(x + 113, 148, kAmber);
}

void PanelDisplay::drawSoundboy(uint32_t now, int16_t x, bool carrying,
                                bool powered, bool watchingRack) {
    const int16_t bob = powered ? static_cast<int16_t>((now / 240) & 1U) : 0;
    const int16_t y = 76 + bob;
    tft_.fillRect(x - 4, y - 12, 18, 5, kBlack);
    tft_.fillRect(x - 7, y - 8, 5, 20, kBlack);
    tft_.fillRect(x + 11, y - 8, 5, 18, kBlack);
    tft_.fillRect(x, y - 9, 12, 13, kSkin);
    tft_.drawPixel(x + (watchingRack ? 1 : 9), y - 4, kBlack);
    tft_.fillRect(x - 2, y + 4, 16, 31, kShirt);
    tft_.fillRect(x + 1, y + 35, 6, 29, kDenim);
    tft_.fillRect(x + 9, y + 35, 6, 29, kDenim);
    tft_.fillRect(x - 2, y + 63, 10, 4, kBlack);
    tft_.fillRect(x + 9, y + 63, 10, 4, kBlack);
    if (carrying) {
        tft_.drawLine(x - 2, y + 12, x - 18, y + 28, kSkin);
        tft_.drawLine(x + 13, y + 13, x + 17, y + 28, kSkin);
    } else if (watchingRack) {
        tft_.drawLine(x - 2, y + 12, x - 14, y + 23, kSkin);
        tft_.fillCircle(x - 15, y + 23, 2, kSkin);
    } else {
        const int16_t hand = ((now / 480) & 1U) ? 1 : 0;
        tft_.drawLine(x - 1, y + 12, x - 7, y + 30 - hand, kSkin);
        tft_.drawLine(x + 13, y + 12, x + 18, y + 29 + hand, kSkin);
    }
}

void PanelDisplay::drawCable(bool powered) {
    const uint16_t color = powered ? kCable : kSteel;
    tft_.drawLine(132, 148, 139, 148, color);
    tft_.drawLine(139, 148, 139, 160, color);
    tft_.drawLine(139, 160, 270, 160, color);
    tft_.drawLine(270, 160, 270, 132, color);
}

void PanelDisplay::drawKnob(int16_t x, int16_t y, float amount,
                            uint16_t color) {
    amount = max(0.0f, min(1.0f, amount));
    tft_.fillCircle(x, y, 7, kBlack);
    tft_.drawCircle(x, y, 7, kSteelLight);
    const float angle = (225.0f - amount * 270.0f) * PI / 180.0f;
    tft_.drawLine(x, y, x + cosf(angle) * 5.0f,
                  y - sinf(angle) * 5.0f, color);
}

void PanelDisplay::drawVu(int16_t x, int16_t y, uint8_t level,
                          uint16_t color) {
    const uint8_t bars = min<uint8_t>(10, level / 24);
    for (uint8_t index = 0; index < 10; ++index) {
        tft_.fillRect(x + index * 6, y, 4, 4,
                      index < bars ? (index > 7 ? kRed : color) : kNightHigh);
    }
}

void PanelDisplay::drawFocusFrame(int16_t x, int16_t y, int16_t width,
                                  int16_t height, bool active) {
    if (active) {
        tft_.drawRect(x, y, width, height, kAmber);
        tft_.drawPixel(x + 1, y + 1, kCream);
    }
}

void PanelDisplay::renderMenu(const ControlState &state) {
    drawBackdrop();
    tft_.fillRect(16, 12, 288, 146, kBlack);
    tft_.drawRect(16, 12, 288, 146, kSteelLight);
    tft_.drawRect(20, 16, 280, 138, kCabinetEdge);
    for (const int16_t x : {20, 294}) {
        tft_.fillRect(x, 16, 6, 6, kSteelLight);
        tft_.fillRect(x, 148, 6, 6, kSteelLight);
    }
    drawText(31, 23, controller_.menuEditing() ? "RACK LABEL / EDIT"
                                               : "RACK LABEL / SELECT",
             1, kAmber);
    const unsigned count = controller_.menuItemCount();
    const unsigned index = controller_.menuIndex();
    unsigned start = index > 0 ? index - 1 : 0;
    if (count > 3 && start > count - 3) start = count - 3;
    for (unsigned row = 0; row < 3 && start + row < count; ++row) {
        const unsigned absolute = start + row;
        const int16_t y = 49 + static_cast<int16_t>(row) * 30;
        const bool active = absolute == index;
        if (active) tft_.fillRect(27, y - 5, 266, 23, kNightHigh);
        const char *key = controller_.menuItemKey(absolute);
        String label = strcmp(key, "PAD") == 0 ? "CONFIGURE PAD" : key;
        label.replace('_', ' ');
        drawText(34, y, String(active ? "> " : "  ") + label, 1,
                 active ? kCream : kSteelLight);
        const String value = menuValue(key, state);
        if (value.length() > 0) {
            int16_t x1, y1;
            uint16_t textWidth, textHeight;
            tft_.setTextSize(1);
            tft_.getTextBounds(value, 0, 0, &x1, &y1, &textWidth, &textHeight);
            drawText(286 - textWidth, y, value, 1,
                     active ? kAmber : kGreen);
        }
    }
    drawText(34, 140, controller_.menuEditing() ? "TURN CHANGE  PUSH SAVE"
                                                : "TURN SELECT  PUSH OPEN",
             1, kSteelLight);
}

bool PanelDisplay::captureOverlay(const ControlState &before,
                                  const ControlState &after) {
    if (firstFrame_) return true;
    Focus focus = Focus::None;
    String value;
    if (changed(before.tuneHz, after.tuneHz)) {
        focus = Focus::Tune;
        value = decimal(after.tuneHz, 0) + " Hz";
    } else if (changed(before.lfoRateHz, after.lfoRateHz)) {
        focus = Focus::Rate;
        value = decimal(after.lfoRateHz, 2) + " Hz";
    } else if (changed(before.lfoDepthOctaves, after.lfoDepthOctaves)) {
        focus = Focus::Depth;
        value = decimal(after.lfoDepthOctaves, 2) + " oct";
    } else if (changed(before.echoLevel, after.echoLevel)) {
        focus = Focus::EchoLevel;
        value = decimal(after.echoLevel * 100.0f, 0) + "% WET";
    } else if (changed(before.delayMs, after.delayMs)) {
        focus = Focus::EchoTime;
        value = decimal(after.delayMs, 0) + " ms";
    } else if (changed(before.feedback, after.feedback)) {
        focus = Focus::EchoFeedback;
        value = decimal(after.feedback * 100.0f, 0) + "% FB";
    } else if (changed(before.highPassHz, after.highPassHz) ||
               changed(before.lowPassHz, after.lowPassHz)) {
        focus = Focus::EchoTone;
        value = decimal(after.highPassHz, 0) + "-" +
                decimal(after.lowPassHz, 0) + "H";
    } else if (changed(before.masterVolume, after.masterVolume)) {
        focus = Focus::Master;
        value = decimal(after.masterVolume * 100.0f, 0) + "%";
    }
    const bool flagChanged = before.echoLoop != after.echoLoop;
    if (focus != Focus::None && controller_.displayMode() ==
                                  PerformanceController::DisplayMode::Home) {
        focus_ = focus;
        overlayValue_ = value;
        overlayUntilMs_ = millis() + kOverlayDurationMs;
    }
    return focus != Focus::None || flagChanged;
}

String PanelDisplay::menuValue(const char *key,
                               const ControlState &state) const {
    if (strcmp(key, "PAD") == 0) return String(controller_.selectedPad());
    if (strcmp(key, "ASSIGNMENT") == 0) {
        return controller_.padSoundName(controller_.selectedPad());
    }
    if (strcmp(key, "VARIANT") == 0) {
        return controller_.padVariantName(controller_.selectedPad());
    }
    if (strcmp(key, "HOLD") == 0) {
        return controller_.padHold(controller_.selectedPad()) ? "ON" : "OFF";
    }
    if (strcmp(key, "ECHO_LOOP") == 0) return state.echoLoop ? "ON" : "OFF";
    return "";
}

float PanelDisplay::normalized(float value, float minimum, float maximum) {
    if (maximum <= minimum) return 0.0f;
    return max(0.0f, min(1.0f, (value - minimum) / (maximum - minimum)));
}

void PanelDisplay::drawText(int16_t x, int16_t y, const String &text,
                            uint8_t size, uint16_t color) {
    tft_.setTextWrap(false);
    tft_.setTextSize(size);
    tft_.setTextColor(color);
    tft_.setCursor(x, y);
    tft_.print(text);
}

void PanelDisplay::drawCentered(int16_t y, const String &text, uint8_t size,
                                uint16_t color) {
    int16_t x1, y1;
    uint16_t width, height;
    tft_.setTextSize(size);
    tft_.getTextBounds(text, 0, 0, &x1, &y1, &width, &height);
    drawText((PerformanceController::kDisplayWidth - width) / 2, y, text,
             size, color);
}
