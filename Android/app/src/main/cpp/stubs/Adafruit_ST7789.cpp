#include "Adafruit_ST7789.h"

// Adafruit GFX 1.12.6 classic font (BSD license, see Adafruit_GFX_license.txt).
#include "glcdfont.c"

const Adafruit_ST7789 *Adafruit_ST7789::active_ = nullptr;

Adafruit_ST7789::Adafruit_ST7789(SPIClass *, int8_t, int8_t, int8_t) {}

void Adafruit_ST7789::init(uint16_t width, uint16_t height, uint8_t) {
    if (static_cast<int>(width) * height > kMaxPixels) { width = height = 0; }
    nativeWidth_ = width;
    nativeHeight_ = height;
    setRotation(0);
    active_ = this;
}

void Adafruit_ST7789::setRotation(uint8_t rotation) {
    const bool swap = rotation & 1;
    width_ = static_cast<int16_t>(swap ? nativeHeight_ : nativeWidth_);
    height_ = static_cast<int16_t>(swap ? nativeWidth_ : nativeHeight_);
}

void Adafruit_ST7789::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
    pixels_[y * width_ + x] = color;
}

void Adafruit_ST7789::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w < 0) { x += w + 1; w = -w; }
    if (h < 0) { y += h + 1; h = -h; }
    const int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    const int x1 = x + w > width_ ? width_ : x + w;
    const int y1 = y + h > height_ ? height_ : y + h;
    for (int row = y0; row < y1; ++row)
        for (int column = x0; column < x1; ++column) pixels_[row * width_ + column] = color;
    ++frameCounter_;
}

void Adafruit_ST7789::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    fillRect(x, y, w, 1, color);
    fillRect(x, y + h - 1, w, 1, color);
    fillRect(x, y, 1, h, color);
    fillRect(x + w - 1, y, 1, h, color);
}

void Adafruit_ST7789::drawChar(int16_t x, int16_t y, unsigned char character) {
    const int size = textSize_;
    if (x >= width_ || y >= height_ || x + 6 * size - 1 < 0 || y + 8 * size - 1 < 0) return;
    if (character >= 176) ++character;  // Adafruit GFX default: _cp437 == false
    for (int column = 0; column < 5; ++column) {
        uint8_t line = font[character * 5 + column];
        for (int row = 0; row < 8; ++row, line >>= 1) {
            if (line & 1) {
                fillRect(x + column * size, y + row * size, size, size, textColor_);
            } else if (textBackground_ != textColor_) {
                fillRect(x + column * size, y + row * size, size, size, textBackground_);
            }
        }
    }
    if (textBackground_ != textColor_) fillRect(x + 5 * size, y, size, 8 * size, textBackground_);
}

void Adafruit_ST7789::write(char character) {
    if (character == '\n') {
        cursorX_ = 0;
        cursorY_ += textSize_ * 8;
        return;
    }
    if (character == '\r') return;
    if (wrap_ && cursorX_ + textSize_ * 6 > width_) {
        cursorX_ = 0;
        cursorY_ += textSize_ * 8;
    }
    drawChar(cursorX_, cursorY_, static_cast<unsigned char>(character));
    cursorX_ += textSize_ * 6;
}

size_t Adafruit_ST7789::print(const char *text) {
    size_t count = 0;
    for (; text && *text; ++text, ++count) write(*text);
    return count;
}

void Adafruit_ST7789::getTextBounds(const String &text, int16_t x, int16_t y, int16_t *x1,
                                    int16_t *y1, uint16_t *w, uint16_t *h) {
    // Classic-font branch of Adafruit_GFX::charBounds / getTextBounds.
    int minX = 0x7FFF, minY = 0x7FFF, maxX = -1, maxY = -1;
    int cursorX = x, cursorY = y;
    for (const char *c = text.c_str(); *c; ++c) {
        if (*c == '\n') { cursorX = 0; cursorY += textSize_ * 8; continue; }
        if (*c == '\r') continue;
        if (wrap_ && cursorX + textSize_ * 6 > width_) { cursorX = 0; cursorY += textSize_ * 8; }
        const int right = cursorX + textSize_ * 6 - 1, bottom = cursorY + textSize_ * 8 - 1;
        if (right > maxX) maxX = right;
        if (bottom > maxY) maxY = bottom;
        if (cursorX < minX) minX = cursorX;
        if (cursorY < minY) minY = cursorY;
        cursorX += textSize_ * 6;
    }
    *x1 = static_cast<int16_t>(x);
    *y1 = static_cast<int16_t>(y);
    *w = *h = 0;
    if (maxX >= minX) { *x1 = static_cast<int16_t>(minX); *w = static_cast<uint16_t>(maxX - minX + 1); }
    if (maxY >= minY) { *y1 = static_cast<int16_t>(minY); *h = static_cast<uint16_t>(maxY - minY + 1); }
}
