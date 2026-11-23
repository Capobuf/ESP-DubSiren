#pragma once

#include <cstdint>

// Android side of PanelLcd: the last frame the firmware sent, PanelLcd::kWidth
// x kHeight RGB565 in CPU order, and a counter bumped on every frame.
const uint16_t *panelLcdPixels();
uint32_t panelLcdFrameCounter();
