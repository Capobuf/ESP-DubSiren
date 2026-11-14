#pragma once

// Framebuffer replacement for the subset of Adafruit_ST7789 / Adafruit_GFX used
// by firmware/src/display/PanelDisplay.cpp. Text uses the same classic 5x7
// glcdfont and the same cursor/advance rules as Adafruit GFX 1.12.6, so the
// Android display shows the pixels the firmware would send to the ST7789.

#include <Arduino.h>
#include <SPI.h>

class Adafruit_ST7789 {
public:
    static constexpr int kMaxPixels = 320 * 172;

    Adafruit_ST7789(SPIClass *, int8_t, int8_t, int8_t);

    void init(uint16_t width, uint16_t height, uint8_t = 0);
    void setRotation(uint8_t rotation);
    int16_t width() const { return width_; }
    int16_t height() const { return height_; }

    void fillScreen(uint16_t color) { fillRect(0, 0, width_, height_, color); }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void drawPixel(int16_t x, int16_t y, uint16_t color);

    void setTextWrap(bool wrap) { wrap_ = wrap; }
    void setTextSize(uint8_t size) { textSize_ = size > 0 ? size : 1; }
    void setTextColor(uint16_t color) { textColor_ = textBackground_ = color; }
    void setTextColor(uint16_t color, uint16_t background) {
        textColor_ = color; textBackground_ = background;
    }
    void setCursor(int16_t x, int16_t y) { cursorX_ = x; cursorY_ = y; }
    size_t print(const String &text) { return print(text.c_str()); }
    size_t print(const char *text);
    void getTextBounds(const String &text, int16_t x, int16_t y, int16_t *x1,
                       int16_t *y1, uint16_t *w, uint16_t *h);

    // Android side: last initialized display and a counter bumped on writes.
    static const Adafruit_ST7789 *active() { return active_; }
    const uint16_t *pixels() const { return pixels_; }
    uint32_t frameCounter() const { return frameCounter_; }

private:
    void write(char character);
    void drawChar(int16_t x, int16_t y, unsigned char character);

    static const Adafruit_ST7789 *active_;
    uint16_t pixels_[kMaxPixels] = {};
    uint16_t nativeWidth_ = 0, nativeHeight_ = 0;
    int16_t width_ = 0, height_ = 0;
    int16_t cursorX_ = 0, cursorY_ = 0;
    uint8_t textSize_ = 1;
    uint16_t textColor_ = 0xFFFF, textBackground_ = 0xFFFF;
    bool wrap_ = true;
    uint32_t frameCounter_ = 0;
};
