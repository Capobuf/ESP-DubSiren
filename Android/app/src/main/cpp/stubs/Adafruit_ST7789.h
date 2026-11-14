#pragma once

// Framebuffer Adafruit_ST7789 built on the real Adafruit GFX 1.12.6, the
// version pinned in firmware/platformio.ini and fetched by CMakeLists.txt.
// Every primitive, font and canvas comes from the library itself; only the
// pixel sink differs, so the Android display shows the pixels the firmware
// would send to the ST7789.

#include <Adafruit_GFX.h>
#include <SPI.h>

class Adafruit_ST7789 : public Adafruit_GFX {
public:
    static constexpr int kMaxPixels = 320 * 172;

    Adafruit_ST7789(SPIClass *, int8_t, int8_t, int8_t) : Adafruit_GFX(0, 0) {}

    // Like the real driver: init() sets the native size, rotation 0.
    void init(uint16_t width, uint16_t height, uint8_t = 0);
    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;

    // Android side: last initialized display and a counter bumped on writes.
    static const Adafruit_ST7789 *active() { return active_; }
    const uint16_t *pixels() const { return pixels_; }
    uint32_t frameCounter() const { return frameCounter_; }

private:
    static const Adafruit_ST7789 *active_;
    // Stored in the current rotation: row-major, width() x height().
    uint16_t pixels_[kMaxPixels] = {};
    uint32_t frameCounter_ = 0;
};
