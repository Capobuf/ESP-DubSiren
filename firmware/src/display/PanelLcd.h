#pragma once

#include <atomic>
#include <cstdint>

struct esp_lcd_panel_t;

// The ST7789 of the panel (U5, 172x320 glass) in landscape, fed by SPI DMA:
// send() starts a full frame and returns at once, so loop() never waits for
// the bus. The Android app replaces this file's .cpp with a framebuffer.
class PanelLcd {
public:
    static constexpr int16_t kWidth = 320;
    static constexpr int16_t kHeight = 172;

    bool begin();
    // False while the previous frame is still being sent.
    bool idle() const { return !busy_.load(); }
    // kWidth x kHeight RGB565 pixels, byte-swapped in place for the panel:
    // they belong to the DMA until idle() and must be redrawn before reuse.
    bool send(uint16_t *pixels);

private:
    esp_lcd_panel_t *panel_ = nullptr;
    std::atomic<bool> busy_{false};
};
