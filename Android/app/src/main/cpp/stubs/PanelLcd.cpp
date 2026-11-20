// Framebuffer PanelLcd: takes the place of firmware/src/display/PanelLcd.cpp
// (esp_lcd, SPI DMA). The frame is copied at once, so the panel is never busy.

#include "display/PanelLcd.h"

#include <cstring>

#include "PanelLcdFrame.h"

namespace {
uint16_t frame[PanelLcd::kWidth * PanelLcd::kHeight] = {};
uint32_t frameCounter = 0;
}  // namespace

bool PanelLcd::begin() { return true; }

bool PanelLcd::send(uint16_t *pixels) {
    std::memcpy(frame, pixels, sizeof(frame));
    ++frameCounter;
    return true;
}

const uint16_t *panelLcdPixels() { return frame; }
uint32_t panelLcdFrameCounter() { return frameCounter; }
