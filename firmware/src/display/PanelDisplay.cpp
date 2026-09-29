#include "PanelDisplay.h"

#include <SPI.h>
#include <esp_heap_caps.h>
#include <esp32-hal-psram.h>

#include "Config.h"
#include "assets/DisplayAssets.h"

namespace {

constexpr uint16_t kBlack = 0x0020;
constexpr uint16_t kPanel = 0x08A2;
constexpr uint16_t kChartreuse = 0xCFE6;
constexpr uint16_t kChartreuseDim = 0x7446;
constexpr uint16_t kOffWhite = 0xE73B;
constexpr uint16_t kMuted = 0x73EE;
constexpr uint16_t kAmber = 0xFD85;
constexpr uint16_t kRed = 0xFA67;
constexpr uint16_t kScanline = 0x2142;

namespace DisplayLayout {
constexpr int16_t kHeaderX = 159;
constexpr int16_t kHeaderY = 25;
constexpr int16_t kHeaderW = 80;
constexpr int16_t kHeaderH = 18;
constexpr int16_t kTuneX = 160;
constexpr int16_t kTuneY = 61;
constexpr int16_t kRateX = 207;
constexpr int16_t kRateY = 61;
constexpr int16_t kDepthX = 160;
constexpr int16_t kDepthY = 78;
constexpr int16_t kMasterX = 207;
constexpr int16_t kMasterY = 78;
constexpr int16_t kModuleW = 46;
constexpr int16_t kModuleH = 16;
constexpr int16_t kEchoX = 160;
constexpr int16_t kEchoY = 94;
constexpr int16_t kEchoW = 93;
constexpr int16_t kEchoH = 21;
constexpr int16_t kStatusX = 23;
constexpr int16_t kStatusY = 154;
constexpr int16_t kStatusW = 274;
constexpr int16_t kStatusH = 12;
constexpr int16_t kMenuTitleX = 83;
constexpr int16_t kMenuTitleY = 21;
constexpr int16_t kMenuTitleW = 155;
constexpr int16_t kMenuTitleH = 14;
constexpr int16_t kMenuRowsX = 63;
constexpr int16_t kMenuRowsY = 48;
constexpr int16_t kMenuRowsW = 196;
constexpr int16_t kMenuFooterX = 22;
constexpr int16_t kMenuFooterY = 155;
constexpr int16_t kMenuFooterW = 276;
constexpr int16_t kMenuFooterH = 11;
}  // namespace DisplayLayout

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

    if (psramFound()) {
        constexpr size_t bytes = PerformanceController::kDisplayWidth *
                                 PerformanceController::kDisplayHeight *
                                 sizeof(uint16_t);
        framebuffer_ = static_cast<uint16_t *>(
            heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    tft_.fillScreen(kBlack);
    bootStartedMs_ = millis();
    return true;
}

void PanelDisplay::update(bool linked, uint8_t audioPeak) {
    if (!ready_) return;
    const uint32_t now = millis();
    if (!firstFrame_ && now - lastFrameMs_ < kFrameMs) return;

    const ControlState state = controls_.snapshot();
    const unsigned revision = controller_.panelRevision();
    const auto mode = controller_.displayMode();
    const bool stateChanged = firstFrame_ || captureOverlay(lastState_, state);
    const bool panelChanged = firstFrame_ || revision != lastRevision_;
    const bool linkChanged = firstFrame_ || linked != lastLinked_;
    const bool modeChanged = !firstFrame_ && mode != lastMode_;
    const bool booting = now - bootStartedMs_ < kBootDurationMs;

    if (panelChanged && !firstFrame_) interactUntilMs_ = now + 650;
    if (modeChanged) transitionUntilMs_ = now + kTransitionMs;
    if (!booting && scene_ == DisplayScene::BootPower) {
        transitionUntilMs_ = now + kTransitionMs;
    }
    const bool transitionActive =
        static_cast<int32_t>(transitionUntilMs_ - now) > 0;
    const bool animated = booting || transitionActive ||
        mode == PerformanceController::DisplayMode::Home;
    if (!firstFrame_ && !animated && !stateChanged && !panelChanged &&
        !linkChanged && !modeChanged) return;

    lastFrameMs_ = now;
    const uint32_t renderStart = micros();
    if (booting) {
        renderBoot(now - bootStartedMs_);
    } else if (mode == PerformanceController::DisplayMode::Menu) {
        scene_ = DisplayScene::Menu;
        overlayUntilMs_ = 0;
        focus_ = Focus::None;
        renderMenu(state, transitionActive);
    } else {
        scene_ = DisplayScene::Home;
        if (static_cast<int32_t>(overlayUntilMs_ - now) <= 0) {
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
    lastMode_ = mode;
}

void PanelDisplay::renderBoot(uint32_t elapsedMs) {
    const uint16_t *background = DisplayAssets::kBootInit;
    const char *label = "SYS/INIT";
    uint8_t shot = 0;
    if (elapsedMs >= 2250) {
        scene_ = DisplayScene::BootPower;
        background = DisplayAssets::kBootPower;
        label = "POWER/ONLINE";
        shot = 3;
    } else if (elapsedMs >= 1500) {
        scene_ = DisplayScene::BootConnect;
        background = DisplayAssets::kBootConnect;
        label = "SIGNAL/CONNECT";
        shot = 2;
    } else if (elapsedMs >= 650) {
        scene_ = DisplayScene::BootRollIn;
        background = DisplayAssets::kBootRollin;
        label = "RIG/ROLL-IN";
        shot = 1;
    } else {
        scene_ = DisplayScene::BootInit;
    }

    beginBitmapFrame(background);
    const uint16_t phase = shot == 0 ? elapsedMs
        : shot == 1 ? elapsedMs - 650
        : shot == 2 ? elapsedMs - 1500 : elapsedMs - 2250;
    if (phase < 120 || phase > 610) drawGlitch();
    presentBitmapFrame();

    tft_.fillRect(7, 7, 86, 12, kBlack);
    drawText(10, 9, label, 1, kChartreuse);
    tft_.drawFastHLine(10, 20, 58 + shot * 13, kChartreuse);
    for (int16_t y = (phase / 17) % 9; y < 170; y += 12) {
        for (int16_t x = 0; x < 320; x += 2) {
            tft_.drawPixel(x, y, kScanline);
        }
    }
    if (shot == 2) {
        const uint8_t radius = 3 + ((elapsedMs / 70) % 5);
        tft_.drawCircle(207, 112, radius, kAmber);
    } else if (shot == 3) {
        const uint8_t bars = 4 + ((elapsedMs / 90) % 9);
        for (uint8_t index = 0; index < bars; ++index) {
            tft_.fillRect(170 + index * 5, 144, 4, 4, kChartreuse);
        }
    }
}

void PanelDisplay::renderHome(const ControlState &state, bool linked,
                              uint32_t now, uint8_t audioPeak) {
    if (state.profile == PerformanceProfile::Classic) {
        tft_.fillScreen(kBlack);
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
    beginBitmapFrame(DisplayAssets::kHome);
    const uint8_t level = speakerLevel(audioPeak);
    const uint16_t *speakerPixels[] = {
        DisplayAssets::kSpeakerReactiveFrame0,
        DisplayAssets::kSpeakerReactiveFrame1,
        DisplayAssets::kSpeakerReactiveFrame2,
    };
    const uint8_t *speakerMasks[] = {
        DisplayAssets::kSpeakerReactiveFrame0Mask,
        DisplayAssets::kSpeakerReactiveFrame1Mask,
        DisplayAssets::kSpeakerReactiveFrame2Mask,
    };
    drawMaskedBitmap(8, 21, speakerPixels[level], speakerMasks[level], 126, 132);

    const bool interacting = focus_ != Focus::None ||
        controller_.activePad() != 0 ||
        static_cast<int32_t>(interactUntilMs_ - now) > 0;
    if (interacting) {
        const uint8_t frame = (now / 220) % 3;
        const uint16_t *pixels[] = {
            DisplayAssets::kSoundboyInteractFrame0,
            DisplayAssets::kSoundboyInteractFrame1,
            DisplayAssets::kSoundboyInteractFrame2,
        };
        const uint8_t *masks[] = {
            DisplayAssets::kSoundboyInteractFrame0Mask,
            DisplayAssets::kSoundboyInteractFrame1Mask,
            DisplayAssets::kSoundboyInteractFrame2Mask,
        };
        drawMaskedBitmap(248, 77, pixels[frame], masks[frame], 72, 76);
    } else {
        constexpr uint8_t cadence[] = {0, 0, 1, 0, 2, 0};
        const uint8_t frame = cadence[(now / 480) % 6];
        const uint16_t *pixels[] = {
            DisplayAssets::kSoundboyIdleFrame0,
            DisplayAssets::kSoundboyIdleFrame1,
            DisplayAssets::kSoundboyIdleFrame2,
        };
        const uint8_t *masks[] = {
            DisplayAssets::kSoundboyIdleFrame0Mask,
            DisplayAssets::kSoundboyIdleFrame1Mask,
            DisplayAssets::kSoundboyIdleFrame2Mask,
        };
        drawMaskedBitmap(271, 68, pixels[frame], masks[frame], 46, 88);
    }
    if (static_cast<int32_t>(transitionUntilMs_ - now) > 0) drawGlitch();
    presentBitmapFrame();
    drawHomeUi(state, linked, audioPeak, focus_, overlayValue_);
}

void PanelDisplay::renderMenu(const ControlState &state, bool glitch) {
    beginBitmapFrame(DisplayAssets::kMenu);
    if (glitch) drawGlitch();
    presentBitmapFrame();

    using namespace DisplayLayout;
    tft_.fillRect(kMenuTitleX, kMenuTitleY, kMenuTitleW, kMenuTitleH, kBlack);
    drawText(kMenuTitleX + 4, kMenuTitleY + 3,
             controller_.menuEditing() ? "RACK/EDIT" : "RACK/SELECT", 1,
             controller_.menuEditing() ? kAmber : kChartreuse);
    const unsigned count = controller_.menuItemCount();
    const unsigned index = controller_.menuIndex();
    unsigned start = index > 0 ? index - 1 : 0;
    if (count > 3 && start > count - 3) start = count - 3;
    for (unsigned row = 0; row < 3 && start + row < count; ++row) {
        const unsigned absolute = start + row;
        const int16_t y = kMenuRowsY + static_cast<int16_t>(row) * 26;
        const bool active = absolute == index;
        tft_.fillRect(kMenuRowsX, y, kMenuRowsW, 22, active ? kPanel : kBlack);
        tft_.drawRect(kMenuRowsX, y, kMenuRowsW, 22,
                      active ? kChartreuse : kChartreuseDim);
        const char *key = controller_.menuItemKey(absolute);
        String label = strcmp(key, "PAD") == 0 ? "CONFIGURE PAD" : key;
        label.replace('_', ' ');
        drawText(kMenuRowsX + 5, y + 7, String(active ? "> " : "  ") + label,
                 1, active ? kOffWhite : kMuted);
        const String value = menuValue(key, state);
        if (value.length()) {
            int16_t x1, y1;
            uint16_t width, height;
            tft_.setTextSize(1);
            tft_.getTextBounds(value, 0, 0, &x1, &y1, &width, &height);
            drawText(kMenuRowsX + kMenuRowsW - width - 5, y + 7, value, 1,
                     active ? kAmber : kChartreuseDim);
        }
    }
    tft_.fillRect(kMenuFooterX, kMenuFooterY, kMenuFooterW, kMenuFooterH, kBlack);
    drawText(kMenuFooterX + 5, kMenuFooterY + 2,
             controller_.menuEditing() ? "TURN CHANGE / PUSH SAVE"
                                       : "TURN SELECT / PUSH OPEN",
             1, kOffWhite);
}

void PanelDisplay::beginBitmapFrame(const uint16_t *background) {
    constexpr size_t pixelCount = PerformanceController::kDisplayWidth *
                                  PerformanceController::kDisplayHeight;
    if (framebuffer_) {
        memcpy(framebuffer_, background, pixelCount * sizeof(uint16_t));
    } else {
        tft_.drawRGBBitmap(0, 0, background, PerformanceController::kDisplayWidth,
                           PerformanceController::kDisplayHeight);
    }
}

void PanelDisplay::drawMaskedBitmap(int16_t x, int16_t y,
                                    const uint16_t *pixels,
                                    const uint8_t *mask, uint16_t width,
                                    uint16_t height) {
    if (framebuffer_) {
        for (uint16_t row = 0; row < height; ++row) {
            for (uint16_t column = 0; column < width; ++column) {
                const uint32_t source = static_cast<uint32_t>(row) * width + column;
                if ((mask[source >> 3] & (0x80 >> (source & 7))) == 0) continue;
                const int16_t targetX = x + column;
                const int16_t targetY = y + row;
                if (targetX >= 0 && targetX < 320 && targetY >= 0 && targetY < 170) {
                    framebuffer_[targetY * 320 + targetX] = pixels[source];
                }
            }
        }
        return;
    }
    for (uint16_t row = 0; row < height; ++row) {
        uint16_t column = 0;
        while (column < width) {
            while (column < width) {
                const uint32_t source = static_cast<uint32_t>(row) * width + column;
                if (mask[source >> 3] & (0x80 >> (source & 7))) break;
                ++column;
            }
            const uint16_t start = column;
            while (column < width) {
                const uint32_t source = static_cast<uint32_t>(row) * width + column;
                if ((mask[source >> 3] & (0x80 >> (source & 7))) == 0) break;
                ++column;
            }
            if (column > start) {
                const uint32_t source = static_cast<uint32_t>(row) * width + start;
                tft_.drawRGBBitmap(x + start, y + row, pixels + source,
                                   column - start, 1);
            }
        }
    }
}

void PanelDisplay::presentBitmapFrame() {
    if (framebuffer_) {
        tft_.drawRGBBitmap(0, 0, framebuffer_,
                           PerformanceController::kDisplayWidth,
                           PerformanceController::kDisplayHeight);
    }
}

void PanelDisplay::drawHomeUi(const ControlState &state, bool linked,
                              uint8_t audioPeak, Focus focus,
                              const String &focusValue) {
    using namespace DisplayLayout;
    tft_.fillRect(kHeaderX, kHeaderY, kHeaderW, kHeaderH, kBlack);
    drawText(kHeaderX + 3, kHeaderY + 2,
             controller_.padSoundName(controller_.selectedPad()), 1,
             kChartreuse);
    drawText(kHeaderX + 3, kHeaderY + 10,
             controller_.padVariantName(controller_.selectedPad()), 1,
             kOffWhite);
    drawText(kHeaderX + kHeaderW - 16, kHeaderY + 5,
             String("P") + controller_.selectedPad(), 1, kAmber);

    const String tune = focus == Focus::Tune ? focusValue
        : decimal(state.tuneHz, 0) + "H";
    const String rate = focus == Focus::Rate ? focusValue
        : decimal(state.lfoRateHz, 1) + "H";
    const String depth = focus == Focus::Depth ? focusValue
        : decimal(state.lfoDepthOctaves, 2);
    const String master = focus == Focus::Master ? focusValue
        : decimal(state.masterVolume * 100.0f, 0) + "%";
    drawModule(kTuneX, kTuneY, kModuleW, kModuleH, "TUNE", tune,
               focus == Focus::Tune);
    drawModule(kRateX, kRateY, kModuleW, kModuleH, "RATE", rate,
               focus == Focus::Rate);
    drawModule(kDepthX, kDepthY, kModuleW, kModuleH - 1, "DEPTH", depth,
               focus == Focus::Depth);
    drawModule(kMasterX, kMasterY, kModuleW, kModuleH - 1, "MASTER", master,
               focus == Focus::Master);

    const bool echoFocus = focus == Focus::EchoTime ||
        focus == Focus::EchoFeedback || focus == Focus::EchoLevel ||
        focus == Focus::EchoTone;
    tft_.fillRect(kEchoX, kEchoY, kEchoW, kEchoH, kBlack);
    tft_.drawRect(kEchoX, kEchoY, kEchoW, kEchoH,
                  echoFocus ? kAmber : kChartreuseDim);
    const char *labels[] = {"T", "FB", "LV", "EQ"};
    const Focus focuses[] = {Focus::EchoTime, Focus::EchoFeedback,
                             Focus::EchoLevel, Focus::EchoTone};
    String values[] = {
        decimal(state.delayMs, 0),
        decimal(state.feedback * 100.0f, 0),
        decimal(state.echoLevel * 100.0f, 0),
        "CUS",
    };
    for (uint8_t column = 0; column < 4; ++column) {
        const int16_t left = kEchoX + 3 + column * 23;
        drawText(left, kEchoY + 2, labels[column], 1,
                 focus == focuses[column] ? kAmber : kMuted);
        drawText(left, kEchoY + 11,
                 focus == focuses[column] ? focusValue.substring(0, 6)
                                           : values[column],
                 1, kOffWhite);
    }

    tft_.fillRect(kStatusX, kStatusY, kStatusW, kStatusH, kBlack);
    String status;
    if (controller_.padHold(controller_.currentPad())) status += "HOLD";
    if (state.echoLoop) status += (status.length() ? "/LOOP" : "LOOP");
    if (linked) status += (status.length() ? "/LINK" : "LINK");
    if (!status.length()) status = "SYSTEM READY";
    drawText(kStatusX + 4, kStatusY + 2, status, 1,
             state.echoLoop ? kRed : kChartreuse);
    drawVu(kStatusX + 168, kStatusY + 4, audioPeak, kChartreuse);
}

void PanelDisplay::drawModule(int16_t x, int16_t y, int16_t width,
                              int16_t height, const char *label,
                              const String &value, bool focus) {
    tft_.fillRect(x, y, width, height, kBlack);
    tft_.drawRect(x, y, width, height, focus ? kAmber : kChartreuseDim);
    drawText(x + 2, y + 1, label, 1, focus ? kAmber : kMuted);
    drawText(x + 2, y + 8, value.substring(0, 9), 1,
             focus ? kAmber : kOffWhite);
}

void PanelDisplay::drawGlitch() {
    drawMaskedBitmap(0, 0, DisplayAssets::kGlitch,
                     DisplayAssets::kGlitchMask, 320, 170);
}

void PanelDisplay::drawVu(int16_t x, int16_t y, uint8_t level,
                          uint16_t color) {
    const uint8_t bars = min<uint8_t>(16, (level + 8) / 16);
    for (uint8_t index = 0; index < 16; ++index) {
        const uint16_t active = index >= 13 ? kRed : index >= 10 ? kAmber : color;
        tft_.fillRect(x + index * 6, y, 4, 4,
                      index < bars ? active : kPanel);
    }
}

uint8_t PanelDisplay::speakerLevel(uint8_t peak) {
    if (peak >= 166) speakerLevel_ = 2;
    else if (speakerLevel_ == 2 && peak >= 133) speakerLevel_ = 2;
    else if (peak >= 77) speakerLevel_ = 1;
    else if (speakerLevel_ == 1 && peak >= 56) speakerLevel_ = 1;
    else speakerLevel_ = 0;
    return speakerLevel_;
}

bool PanelDisplay::captureOverlay(const ControlState &before,
                                  const ControlState &after) {
    if (firstFrame_) return true;
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
