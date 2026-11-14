#include "PanelDisplay.h"

#include <SPI.h>

#include <new>

#include "Config.h"

namespace {

// Gold on black, sound system flyer style. Red and green mark state only:
// red = latched or waiting for pickup, green = selected pad or shortcut.
constexpr uint16_t kInk = 0x0000;
constexpr uint16_t kGold = 0xFDE5;      // 255, 190, 40
constexpr uint16_t kGoldDim = 0x7AA1;   // 120, 85, 15
constexpr uint16_t kGoldDeep = 0xBC23;  // 190, 132, 24: fader track on gold
constexpr uint16_t kCell = 0x2102;      // 36, 32, 16
constexpr uint16_t kTrack = 0x41A3;     // 64, 52, 24
constexpr uint16_t kCream = 0xFF79;     // 250, 236, 200
constexpr uint16_t kMuted = 0x838B;     // 130, 115, 90
constexpr uint16_t kRed = 0xE143;       // 230, 40, 30
constexpr uint16_t kGreen = 0x2DE8;     // 40, 190, 70

constexpr int16_t kWidth = PerformanceController::kDisplayWidth;
constexpr int16_t kHeight = PerformanceController::kDisplayHeight;

// Panel map: three columns for RV1-RV3 / RV4-RV6, encoder at the right of
// the middle row, the four keys below. The glass has 3 mm (about 30 px)
// rounded corners: side margins and inset text keep everything visible.
constexpr int16_t kMargin = 8;
constexpr int16_t kColumnX[3] = {8, 111, 214};
constexpr int16_t kColumnW = 98;
constexpr int16_t kRowH = 40;
constexpr int16_t kTopY = 0;
constexpr int16_t kMiddleY = 44;
constexpr int16_t kBottomY = 88;
constexpr int16_t kKeyY = 132;
constexpr int16_t kKeyX[4] = {8, 85, 162, 239};
constexpr int16_t kKeyW = 73;
constexpr int16_t kBannerX = kMargin;
constexpr int16_t kBannerW = 201;
constexpr int16_t kRadius = 5;

// Menu.
constexpr int16_t kTitleH = 26;
constexpr int16_t kMenuRowY = 30;
constexpr int16_t kMenuRowPitch = 36;
constexpr int16_t kMenuRowH = 34;
constexpr unsigned kMenuRows = 3;
constexpr int16_t kFooterY = 140;

constexpr char kArrowRight = 0x10;
constexpr char kArrowLeft = 0x11;
constexpr char kBullet = 0x07;

// Display copy of a controller value: "MID - 680 Hz" becomes "MID 680 Hz".
void compact(char *value) {
    char *dash;
    while ((dash = strstr(value, " - ")) != nullptr)
        memmove(dash + 1, dash + 3, strlen(dash + 3) + 1);
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
    // Adafruit Product 5393: 172x320 native, 320x172 at rotation 1.
    tft_.init(172, 320);
    tft_.setRotation(1);
    ready_ = tft_.width() == kWidth && tft_.height() == kHeight;
    if (!ready_) return false;

    // 110 kB frame buffer; without it the frame is drawn on the panel itself.
    canvas_.reset(new (std::nothrow) GFXcanvas16(kWidth, kHeight));
    if (canvas_ && canvas_->getBuffer() == nullptr) canvas_.reset();
    gfx_ = canvas_ ? static_cast<Adafruit_GFX *>(canvas_.get()) : &tft_;
    tft_.fillScreen(kInk);
    return true;
}

void PanelDisplay::update(bool linked) {
    if (!ready_) return;
    const uint32_t now = millis();
    if (!firstRender_ && now - lastRefreshMs_ < kRefreshMs) return;

    const ControlState state = controls_.snapshot();
    const unsigned revision = controller_.panelRevision();
    const auto mode = controller_.displayMode();
    bool dirty = firstRender_ || revision != lastRevision_ || mode != lastMode_ ||
                 linked != lastLinked_ || displayedStateChanged(lastState_, state);

    if (!firstRender_ && revision != lastRevision_ &&
        mode == PerformanceController::DisplayMode::Home) {
        const char *title = controller_.overlayTitle();
        const char *value = controller_.overlayValue();
        const unsigned pot = controller_.overlayPot();
        if (!title[0]) {
            overlayActive_ = false;
        } else if (overlayActive_ || pot != overlayPot_ ||
                   strcmp(title, overlayTitle_) != 0 ||
                   strcmp(value, overlayValue_) != 0) {
            // An unchanged message from an older event is not shown again.
            snprintf(overlayTitle_, sizeof(overlayTitle_), "%s", title);
            snprintf(overlayValue_, sizeof(overlayValue_), "%s", value);
            overlayPot_ = pot;
            overlayActive_ = true;
            overlayUntilMs_ = now + kOverlayDurationMs;
        }
    }
    if (overlayActive_ && (mode == PerformanceController::DisplayMode::Menu ||
                           static_cast<int32_t>(overlayUntilMs_ - now) <= 0)) {
        overlayActive_ = false;
        dirty = true;
    }
    if (!dirty) return;

    lastRefreshMs_ = now;
    const uint32_t renderStart = micros();
    gfx_->fillScreen(kInk);
    if (mode == PerformanceController::DisplayMode::Menu) renderMenu();
    else renderHome(state, linked);
    if (canvas_) tft_.drawRGBBitmap(0, 0, canvas_->getBuffer(), kWidth, kHeight);
    lastRenderUs_ = micros() - renderStart;
    if (lastRenderUs_ > maximumRenderUs_) maximumRenderUs_ = lastRenderUs_;
    firstRender_ = false;
    lastState_ = state;
    lastRevision_ = revision;
    lastLinked_ = linked;
    lastMode_ = mode;
}

// ---- Home --------------------------------------------------------------

void PanelDisplay::renderHome(const ControlState &state, bool linked) {
    for (unsigned column = 0; column < 3; ++column) {
        drawPot(column + 1, kColumnX[column], kTopY, state);
        drawPot(column + 4, kColumnX[column], kBottomY, state);
    }
    drawBanner(state, linked);
    drawEncoder(kColumnX[2], kMiddleY);
    for (unsigned button = 1; button <= 4; ++button)
        drawKey(button, kKeyX[button - 1], kKeyY, state);
}

void PanelDisplay::drawPot(unsigned pot, int16_t x, int16_t y,
                           const ControlState &state) {
    const PotConfig &config = controller_.panelSettings().pots[pot - 1];
    const bool focus = overlayActive_ && overlayPot_ == pot;
    const bool unused = config.target == PotTarget::None;
    gfx_->fillRoundRect(x, y, kColumnW, kRowH, kRadius, focus ? kGold : kCell);

    // Function only: the value appears in the banner while the pot moves.
    static constexpr Style labelStyles[] = {{1, 3, true}, {1, 3, false}, {1, 2, false}};
    char label[24];
    snprintf(label, sizeof(label), "%s", potTargetName(config.target));
    text(x + 6, y + 4, label, fitted(label, kColumnW - 12, labelStyles, 3),
         focus ? kInk : unused ? kMuted : kGold);
    if (unused) return;

    // While Pickup waits the bar is red, with an arrow to turn the pot by.
    const bool pickup = controller_.pickupWaiting(pot);
    const int16_t barY = y + kRowH - 9, barW = kColumnW - 12 - (pickup ? 10 : 0);
    drawBar(pot, x + 6, barY, barW, state, focus ? kInk : pickup ? kRed : kGold,
            focus ? kGoldDeep : kTrack);
    if (pickup) {
        const int16_t ax = x + kColumnW - 11;
        const uint16_t color = focus ? kInk : kRed;
        if (controller_.pickupDirection(pot) > 0)
            gfx_->fillTriangle(ax, barY + 5, ax + 6, barY + 5, ax + 3, barY - 1, color);
        else
            gfx_->fillTriangle(ax, barY - 1, ax + 6, barY - 1, ax + 3, barY + 5, color);
    }
}

void PanelDisplay::drawBar(unsigned pot, int16_t x, int16_t y, int16_t w,
                           const ControlState &state, uint16_t on, uint16_t off) {
    constexpr int16_t h = 5;
    const PotConfig &config = controller_.panelSettings().pots[pot - 1];
    const bool pitch = config.target == PotTarget::Pitch;
    const bool rate = config.target == PotTarget::Rate;
    if (config.mode == PotMode::Zones && (pitch || (rate && !state.tempo.lfoSync))) {
        // One segment per zone, left to right; the current one lit.
        const int16_t count = pitch ? 3 : 4;
        const int16_t zone = pitch ? static_cast<int16_t>(state.classicPitch)
                                   : static_cast<int16_t>(state.classicModulation);
        const int16_t segment = (w - (count - 1) * 3) / count;
        for (int16_t index = 0; index < count; ++index)
            gfx_->fillRect(x + index * (segment + 3), y, segment, h, index == zone ? on : off);
        return;
    }
    const float value = controller_.potValueNormalized(pot);
    gfx_->fillRect(x, y, w, h, off);
    if (config.target == PotTarget::EchoFilter) {
        // Bipolar: LP left of the centre, HP right of it.
        const int16_t middle = x + w / 2;
        const int16_t level = x + static_cast<int16_t>(value * w + 0.5f);
        gfx_->fillRect(level < middle ? level : middle, y, abs(level - middle) + 1, h, on);
        gfx_->drawFastVLine(middle, y - 2, h + 4, on);
        return;
    }
    gfx_->fillRect(x, y, static_cast<int16_t>(value * w + 0.5f), h, on);
}

void PanelDisplay::drawBanner(const ControlState &state, bool linked) {
    const int16_t y = kMiddleY;
    static constexpr Style nameStyles[] = {{2, 3, true}, {1, 3, true}, {1, 2, true}};
    static constexpr Style valueStyles[] = {{2, 3, true}, {1, 3, true}, {1, 2, true}, {1, 1, false}};
    static constexpr Style tagStyles[] = {{1, 1, true}, {1, 1, false}};
    const char *alert = controller_.persistenceStatus()[0]
        ? controller_.persistenceStatus() : controller_.deviceStatus();
    char line[48];

    if (alert[0]) {
        gfx_->fillRoundRect(kBannerX, y, kBannerW, kRowH, kRadius, kRed);
        snprintf(line, sizeof(line), "%s", alert);
        const Style style = fitted(line, kBannerW - 12, valueStyles + 2, 2);
        text(kBannerX + (kBannerW - textWidth(line, style)) / 2, y + (kRowH - 8 * style.sy) / 2,
             line, style, kCream);
        return;
    }
    gfx_->fillRoundRect(kBannerX, y, kBannerW, kRowH, kRadius, kGold);
    if (overlayActive_) {
        // What just moved: name above, value below, the cell is lit too.
        snprintf(line, sizeof(line), "%s", overlayTitle_);
        text(kBannerX + 8, y + 4, line, fitted(line, kBannerW - 16, tagStyles, 2), kInk);
        snprintf(line, sizeof(line), "%s", overlayValue_);
        compact(line);
        const Style style = fitted(line, kBannerW - 16, valueStyles, 4);
        text(kBannerX + 8, y + 14 + (24 - 8 * style.sy) / 2, line, style, kInk);
        return;
    }
    snprintf(line, sizeof(line), "%s", controller_.voiceName());
    const Style style = fitted(line, kBannerW - 16, nameStyles, 3);
    text(kBannerX + (kBannerW - textWidth(line, style)) / 2, y + (kRowH - 8 * style.sy) / 2 + 1,
         line, style, kInk);
    if (state.liveTransposeSemitones != 0) {
        snprintf(line, sizeof(line), "%+d ST", state.liveTransposeSemitones);
        text(kBannerX + 5, y + kRowH - 10, line, {1, 1, true}, kInk);
    }
    if (linked) text(kBannerX + kBannerW - 23, y + kRowH - 10, "USB", {1, 1, true}, kInk);
}

void PanelDisplay::drawEncoder(int16_t x, int16_t y) {
    const bool pressed = controller_.encoderPressed();
    const uint16_t background = pressed ? kGold : kCell;
    const uint16_t color = pressed ? kInk : kGold;
    gfx_->fillRoundRect(x, y, kColumnW, kRowH, kRadius, background);

    // The knob seen from above: ring, pointer and the push face.
    const int16_t cx = x + 16, cy = y + kRowH / 2;
    gfx_->fillCircle(cx, cy, 12, pressed ? kGoldDeep : kTrack);
    gfx_->drawCircle(cx, cy, 12, color);
    gfx_->drawCircle(cx, cy, 11, color);
    gfx_->fillCircle(cx, cy, 4, color);
    gfx_->drawFastVLine(cx, cy - 11, 5, color);

    static constexpr Style styles[] = {{1, 2, true}, {1, 2, false}, {1, 1, false}};
    const char *turn = controller_.livePitchArmed() ? "PITCH"
                     : controller_.selectedIsPad() ? "VARIANT" : "VOICE";
    char line[24];
    snprintf(line, sizeof(line), "%c%c %s", kArrowLeft, kArrowRight, turn);
    text(x + 33, y + 4, line, fitted(line, kColumnW - 37, styles, 3),
         pressed ? kInk : kCream);
    snprintf(line, sizeof(line), "%c MENU", kBullet);
    text(x + 33, y + 21, line, fitted(line, kColumnW - 37, styles, 3),
         pressed ? kInk : kCream);
}

void PanelDisplay::drawKey(unsigned button, int16_t x, int16_t y,
                           const ControlState &state) {
    const ButtonConfig &config = controller_.panelSettings().buttons[button - 1];
    const bool pad = config.action == ButtonAction::PlaySound;
    const bool down = controller_.buttonDown(button);
    const bool shortcut = pad && controller_.encoderPressed();
    char label[24] = "", detail[24] = "";
    bool latched = false;
    if (shortcut) {
        // Encoder held: a pad key does its shortcut instead of playing.
        static const char *shortcuts[] = {"LATCH", "VARIANT -", "VARIANT +", "ECHO CUT"};
        snprintf(label, sizeof(label), "%s", shortcuts[button - 1]);
    } else {
        switch (config.action) {
            case ButtonAction::PlaySound:
                snprintf(label, sizeof(label), "%s", controller_.padSoundName(button));
                snprintf(detail, sizeof(detail), "%s", controller_.padVariantName(button));
                latched = state.hold && controller_.padHold(button) &&
                          controller_.currentPad() == button;
                break;
            case ButtonAction::TapTempo:
                snprintf(label, sizeof(label), "TAP TEMPO");
                if (state.tempo.known) snprintf(detail, sizeof(detail), "%.0f BPM", state.tempo.bpm);
                break;
            case ButtonAction::Hold:
                snprintf(label, sizeof(label), "HOLD");
                latched = state.hold;
                break;
            case ButtonAction::EchoLoop:
                snprintf(label, sizeof(label), "ECHO LOOP");
                latched = state.echoLoop;
                break;
            default:
                snprintf(label, sizeof(label), "%s", controller_.buttonActionNameFor(button));
                break;
        }
    }
    const bool unused = config.action == ButtonAction::None && !shortcut;
    const uint16_t background = down ? kGold : latched ? kRed : kCell;
    const uint16_t color = down ? kInk : latched ? kCream
                         : shortcut ? kGreen : unused ? kMuted : kGold;
    gfx_->fillRoundRect(x, y, kKeyW, kRowH, kRadius, background);
    if (pad && !down && !shortcut && controller_.selectedPad() == button) {
        gfx_->drawRoundRect(x, y, kKeyW, kRowH, kRadius, kGreen);
        gfx_->drawRoundRect(x + 1, y + 1, kKeyW - 2, kRowH - 2, kRadius - 1, kGreen);
    }

    static constexpr Style styles[] = {{1, 2, true}, {1, 2, false}, {1, 1, false}};
    const int16_t width = kKeyW - 8;
    const Style labelStyle = fitted(label, width, styles, 3);
    if (!detail[0]) {
        text(x + (kKeyW - textWidth(label, labelStyle)) / 2,
             y + (kRowH - 8 * labelStyle.sy) / 2, label, labelStyle, color);
        return;
    }
    text(x + (kKeyW - textWidth(label, labelStyle)) / 2, y + 4, label, labelStyle, color);
    const Style detailStyle = fitted(detail, width, styles + 1, 2);
    text(x + (kKeyW - textWidth(detail, detailStyle)) / 2, y + 21, detail, detailStyle,
         down ? kInk : kCream);
}

// ---- Menu --------------------------------------------------------------

void PanelDisplay::renderMenu() {
    static constexpr Style titleStyles[] = {{2, 2, true}, {1, 2, true}, {1, 2, false}};
    static constexpr Style rowStyles[] = {{2, 3, true}, {1, 3, true}, {1, 2, true}, {1, 2, false}};
    char line[48];
    gfx_->fillRect(0, 0, kWidth, kTitleH, kGold);
    snprintf(line, sizeof(line), "%s", controller_.menuTitle());
    text(18, 5, line, fitted(line, kWidth - 36, titleStyles, 3), kInk);

    const unsigned count = controller_.menuItemCount();
    const unsigned index = controller_.menuIndex();
    unsigned start = index > 0 ? index - 1 : 0;
    if (count > kMenuRows && start > count - kMenuRows) start = count - kMenuRows;
    if (count <= kMenuRows) start = 0;
    const int16_t rowW = count > kMenuRows ? kWidth - 8 : kWidth;
    for (unsigned row = 0; row < kMenuRows && start + row < count; ++row) {
        const unsigned item = start + row;
        const int16_t y = kMenuRowY + static_cast<int16_t>(row) * kMenuRowPitch;
        const bool active = item == index;
        const bool editing = active && controller_.menuEditing();
        if (active) {
            gfx_->fillRoundRect(0, y, rowW, kMenuRowH, kRadius, kCell);
            gfx_->fillRect(0, y, 4, kMenuRowH, kGold);
        }
        // Value first: the label takes the room it leaves.
        int16_t valueX = rowW - 6;
        snprintf(line, sizeof(line), "%s", controller_.menuItemValue(item));
        if (line[0]) {
            if (editing) {
                char boxed[48];
                snprintf(boxed, sizeof(boxed), "%c %s %c", kArrowLeft, line, kArrowRight);
                snprintf(line, sizeof(line), "%s", boxed);
            }
            const Style style = fitted(line, rowW / 2, rowStyles, 4);
            const int16_t width = textWidth(line, style);
            valueX = rowW - 8 - width;
            if (editing) gfx_->fillRoundRect(valueX - 4, y + 2, width + 8, kMenuRowH - 4, 3, kGold);
            text(valueX, y + (kMenuRowH - 8 * style.sy) / 2, line, style,
                 editing ? kInk : active ? kGold : kGoldDim);
        }
        snprintf(line, sizeof(line), "%s", controller_.menuItemLabel(item));
        const Style style = fitted(line, valueX - 22, rowStyles, 4);
        text(12, y + (kMenuRowH - 8 * style.sy) / 2, line, style, active ? kCream : kMuted);
    }
    if (count > kMenuRows) {
        // Where the visible rows sit in the page.
        const int16_t trackY = kMenuRowY, trackH = kMenuRowPitch * kMenuRows - 2;
        const int16_t thumbH = trackH * kMenuRows / count;
        const int16_t thumbY = trackY + (trackH - thumbH) * start / (count - kMenuRows);
        gfx_->fillRect(kWidth - 4, trackY, 3, trackH, kCell);
        gfx_->fillRect(kWidth - 4, thumbY, 3, thumbH, kGold);
    }

    // Footer hint, then the key that steps back: BTN4, under the encoder.
    gfx_->fillRect(0, kFooterY, kWidth, kHeight - kFooterY, kCell);
    static constexpr Style footerStyles[] = {{1, 2, true}, {1, 2, false}, {1, 1, false}};
    const Style backStyle{1, 2, true};
    char back[12];
    snprintf(back, sizeof(back), "%c BACK", kArrowLeft);
    const int16_t backW = textWidth(back, backStyle) + 12;
    const int16_t backX = kWidth - 14 - backW, backY = kFooterY + 4;
    gfx_->fillRoundRect(backX, backY, backW, 22, kRadius, kGold);
    text(backX + 6, backY + 3, back, backStyle, kInk);
    snprintf(line, sizeof(line), "%s", controller_.menuFooter());
    const Style style = fitted(line, backX - 28, footerStyles, 3);
    text(20, backY + (22 - 8 * style.sy) / 2, line, style, kCream);
}

// ---- Text --------------------------------------------------------------

int16_t PanelDisplay::textWidth(const char *value, Style style) {
    const int16_t count = static_cast<int16_t>(strlen(value));
    if (count == 0) return 0;
    // Six columns per glyph, the last one blank; bold adds one.
    return count * (6 * style.sx + style.bold) - style.sx;
}

PanelDisplay::Style PanelDisplay::fitted(char *value, int16_t width,
                                         const Style *styles, unsigned count) const {
    for (unsigned index = 0; index + 1 < count; ++index)
        if (textWidth(value, styles[index]) <= width) return styles[index];
    const Style last = styles[count - 1];
    const int16_t advance = 6 * last.sx + last.bold;
    const size_t capacity = width > 0 ? static_cast<size_t>((width + last.sx) / advance) : 0;
    if (strlen(value) > capacity) value[capacity] = '\0';
    return last;
}

void PanelDisplay::text(int16_t x, int16_t y, const char *value, Style style,
                        uint16_t color) {
    const int16_t advance = 6 * style.sx + style.bold;
    for (; *value; ++value, x += advance) {
        const unsigned char character = static_cast<unsigned char>(*value);
        gfx_->drawChar(x, y, character, color, color, style.sx, style.sy);
        if (style.bold) gfx_->drawChar(x + 1, y, character, color, color, style.sx, style.sy);
    }
}

bool PanelDisplay::displayedStateChanged(const ControlState &before,
                                         const ControlState &after) {
    auto differs = [](float left, float right) { return fabsf(left - right) > 0.0001f; };
    const TempoState &a = before.tempo, &b = after.tempo;
    return before.mode != after.mode || before.lfoShape != after.lfoShape ||
        before.lickshotArticulation != after.lickshotArticulation ||
        before.syndrumWave != after.syndrumWave ||
        before.classicPitch != after.classicPitch ||
        before.classicModulation != after.classicModulation ||
        before.pitchZonesActive != after.pitchZonesActive ||
        before.rateZonesActive != after.rateZonesActive ||
        before.hold != after.hold || before.echoCut != after.echoCut ||
        before.echoSpin != after.echoSpin || before.echoLoop != after.echoLoop ||
        before.liveTransposeSemitones != after.liveTransposeSemitones ||
        differs(before.tuneHz, after.tuneHz) || differs(before.lfoRateHz, after.lfoRateHz) ||
        differs(before.lfoDepthOctaves, after.lfoDepthOctaves) ||
        differs(before.decayMs, after.decayMs) || differs(before.sweepAmount, after.sweepAmount) ||
        differs(before.sweepMs, after.sweepMs) || differs(before.delayMs, after.delayMs) ||
        differs(before.feedback, after.feedback) || differs(before.echoLevel, after.echoLevel) ||
        differs(before.echoFilterPosition, after.echoFilterPosition) ||
        differs(a.bpm, b.bpm) || a.known != b.known || a.echoSync != b.echoSync ||
        a.lfoSync != b.lfoSync || a.echoDivision != b.echoDivision ||
        a.lfoDivision != b.lfoDivision || differs(a.echoMs, b.echoMs) ||
        differs(a.lfoHz, b.lfoHz);
}
