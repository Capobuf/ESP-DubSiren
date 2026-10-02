#include "PanelDisplay.h"

#include <SPI.h>

#include "Config.h"

namespace {

constexpr uint16_t kBlack = 0x0020;
constexpr uint16_t kChartreuse = 0xCFE6;
constexpr uint16_t kChartreuseDim = 0x7446;
constexpr uint16_t kOffWhite = 0xE73B;
constexpr uint16_t kMuted = 0x73EE;
constexpr uint16_t kAmber = 0xFD85;
constexpr uint16_t kRed = 0xFA67;

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
    if (!ready_) return false;

    tft_.fillScreen(kBlack);
    return true;
}

void PanelDisplay::update(bool linked) {
    if (!ready_) return;
    const uint32_t now = millis();
    if (!firstRender_ && now - lastRefreshMs_ < kRefreshMs) return;

    const ControlState state = controls_.snapshot();
    const unsigned revision = controller_.panelRevision();
    const auto mode = controller_.displayMode();
    const bool stateChanged = firstRender_ || captureOverlay(lastState_, state);
    const bool overlayExpired = focus_ != Focus::None &&
        static_cast<int32_t>(overlayUntilMs_ - now) <= 0;
    if (overlayExpired || mode == PerformanceController::DisplayMode::Menu) {
        focus_ = Focus::None;
        overlayValue_ = "";
    }
    if (!firstRender_ && !stateChanged && !overlayExpired &&
        revision == lastRevision_ && linked == lastLinked_ && mode == lastMode_) {
        return;
    }

    lastRefreshMs_ = now;
    const uint32_t renderStart = micros();
    tft_.fillScreen(kBlack);
    if (mode == PerformanceController::DisplayMode::Menu) {
        renderMenu(state);
    } else {
        renderHome(state, linked);
    }
    lastRenderUs_ = micros() - renderStart;
    if (lastRenderUs_ > maximumRenderUs_) maximumRenderUs_ = lastRenderUs_;
    firstRender_ = false;
    lastState_ = state;
    lastRevision_ = revision;
    lastLinked_ = linked;
    lastMode_ = mode;
}

void PanelDisplay::renderHome(const ControlState &state, bool linked) {
    if (state.profile == PerformanceProfile::Classic) {
        drawText(16, 10, "DS01E / CLASSIC", 1, kMuted);
        drawText(16, 31, modeName(state.mode), 3, kChartreuse);
        drawText(16, 73, String("PITCH  ") + pitchName(state.classicPitch), 2, kOffWhite);
        drawText(16, 99, String("RATE   ") + rateName(state.classicModulation), 2, kOffWhite);
        String status = state.hold ? "HOLD  " : "";
        status += state.echoCut ? "ECHO CUT" : state.echoLevel > 0.0f ? "ECHO" : "DRY";
        drawText(16, 139, status, 2, kAmber);
        if (focus_ != Focus::None) {
            const char *labels[] = {"", "PITCH", "RATE", "DEPTH", "ECHO LEVEL",
                "ECHO TIME", "ECHO FEEDBACK", "FILTER", "MASTER"};
            tft_.fillRect(10, 61, 300, 72, kBlack);
            tft_.drawRect(10, 61, 300, 72, kChartreuseDim);
            drawText(22, 68, labels[static_cast<unsigned>(focus_)], 1, kMuted);
            drawText(22, 92, overlayValue_, 3, kOffWhite);
        }
        return;
    }
    drawText(16, 10, String("P") + controller_.selectedPad() + " " +
             controller_.padSoundName(controller_.selectedPad()), 2, kChartreuse);
    drawText(16, 32, controller_.padVariantName(controller_.selectedPad()), 1, kOffWhite);
    drawText(16, 51, "TUNE " + decimal(state.tuneHz, 0) + " Hz  RATE " +
             decimal(state.lfoRateHz, 1) + " Hz", 1, kOffWhite);
    drawText(16, 69, "DEPTH " + decimal(state.lfoDepthOctaves, 2) +
             " oct  MASTER " + decimal(state.masterVolume * 100, 0) + "%", 1, kOffWhite);
    drawText(16, 87, "ECHO " + decimal(state.delayMs, 0) + "ms  FB " +
             decimal(state.feedback * 100, 0) + "%  LV " +
             decimal(state.echoLevel * 100, 0) + "%", 1, kOffWhite);
    if (focus_ != Focus::None) {
        const char *labels[] = {"", "TUNE", "RATE", "DEPTH", "ECHO LEVEL",
            "ECHO TIME", "ECHO FEEDBACK", "FILTER", "MASTER"};
        drawText(16, 111, String(labels[static_cast<unsigned>(focus_)]) +
                 " " + overlayValue_, 2, kAmber);
    }
    String status;
    if (controller_.padHold(controller_.currentPad())) status += "HOLD ";
    if (state.echoLoop) status += "LOOP ";
    if (linked) status += "LINK";
    if (!status.length()) status = "SYSTEM READY";
    drawText(16, 149, status, 1, state.echoLoop ? kRed : kChartreuse);
}

void PanelDisplay::renderMenu(const ControlState &state) {
    drawText(16, 12, controller_.menuEditing() ? "EDIT" : "SELECT", 2, kChartreuse);
    const unsigned count = controller_.menuItemCount();
    const unsigned index = controller_.menuIndex();
    unsigned start = index > 0 ? index - 1 : 0;
    if (count > 3 && start > count - 3) start = count - 3;
    for (unsigned row = 0; row < 3 && start + row < count; ++row) {
        const unsigned absolute = start + row;
        const int16_t y = 48 + static_cast<int16_t>(row) * 28;
        const bool active = absolute == index;
        const char *key = controller_.menuItemKey(absolute);
        String label = strcmp(key, "PAD") == 0 ? "CONFIGURE PAD" : key;
        label.replace('_', ' ');
        drawText(16, y, String(active ? "> " : "  ") + label,
                 1, active ? kOffWhite : kMuted);
        const String value = menuValue(key, state);
        if (value.length()) {
            int16_t x1, y1;
            uint16_t width, height;
            tft_.setTextSize(1);
            tft_.getTextBounds(value, 0, 0, &x1, &y1, &width, &height);
            drawText(304 - width, y, value, 1, active ? kAmber : kMuted);
        }
    }
    drawText(16, 149, controller_.menuEditing() ? "TURN CHANGE / PUSH SAVE"
                                              : "TURN SELECT / PUSH OPEN",
             1, kOffWhite);
}

bool PanelDisplay::captureOverlay(const ControlState &before,
                                  const ControlState &after) {
    if (firstRender_) return true;
    Focus focus = Focus::None;
    String value;
    if ((before.classicPitch != after.classicPitch || changed(before.pitchPosition, after.pitchPosition)) && after.profile == PerformanceProfile::Classic) {
        focus = Focus::Tune;
        value = pitchName(after.classicPitch);
    } else if ((before.classicModulation != after.classicModulation || changed(before.ratePosition, after.ratePosition)) && after.profile == PerformanceProfile::Classic) {
        focus = Focus::Rate;
        value = rateName(after.classicModulation);
    } else if (changed(before.echoFilterPosition, after.echoFilterPosition)) {
        focus = Focus::EchoTone;
        const float amount = PerformanceConfig::echoFilterAmount(after.echoFilterPosition);
        value = amount == 0.0f ? String("FLAT") : String(after.echoFilterPosition < 0 ? "LP " : "HP ") + decimal(amount * 100, 0) + "%";
    } else if (changed(before.tuneHz, after.tuneHz)) {
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
        value = decimal(after.echoLevel * 100.0f, 0) + "%";
    } else if (changed(before.delayMs, after.delayMs)) {
        focus = Focus::EchoTime;
        value = decimal(after.delayMs, 0) + "ms";
    } else if (changed(before.feedback, after.feedback)) {
        focus = Focus::EchoFeedback;
        value = decimal(after.feedback * 100.0f, 0) + "%";
    } else if (changed(before.highPassHz, after.highPassHz) ||
               changed(before.lowPassHz, after.lowPassHz)) {
        focus = Focus::EchoTone;
        value = decimal(after.highPassHz, 0) + "H";
    } else if (changed(before.masterVolume, after.masterVolume)) {
        focus = Focus::Master;
        value = decimal(after.masterVolume * 100.0f, 0) + "%";
    }
    const bool flagChanged = before.echoLoop != after.echoLoop ||
        before.profile != after.profile || before.mode != after.mode ||
        before.hold != after.hold || before.echoCut != after.echoCut;
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
    if (strcmp(key, "PROFILE") == 0) return state.profile == PerformanceProfile::Classic ? "EXTENDED" : "DS01E";
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

void PanelDisplay::drawText(int16_t x, int16_t y, const String &text,
                            uint8_t size, uint16_t color) {
    tft_.setTextWrap(false);
    tft_.setTextSize(size);
    tft_.setTextColor(color);
    tft_.setCursor(x, y);
    tft_.print(text);
}
