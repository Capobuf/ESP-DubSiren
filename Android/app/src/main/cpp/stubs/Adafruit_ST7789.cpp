#include "Adafruit_ST7789.h"

const Adafruit_ST7789 *Adafruit_ST7789::active_ = nullptr;

void Adafruit_ST7789::init(uint16_t width, uint16_t height, uint8_t) {
    if (static_cast<int>(width) * height > kMaxPixels) { width = height = 0; }
    WIDTH = static_cast<int16_t>(width);
    HEIGHT = static_cast<int16_t>(height);
    setRotation(0);
    active_ = this;
}

void Adafruit_ST7789::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || y < 0 || x >= _width || y >= _height) return;
    pixels_[y * _width + x] = color;
    ++frameCounter_;
}

void Adafruit_ST7789::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w < 0) { x += w + 1; w = -w; }
    if (h < 0) { y += h + 1; h = -h; }
    const int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    const int x1 = x + w > _width ? _width : x + w;
    const int y1 = y + h > _height ? _height : y + h;
    for (int row = y0; row < y1; ++row)
        for (int column = x0; column < x1; ++column) pixels_[row * _width + column] = color;
    ++frameCounter_;
}
