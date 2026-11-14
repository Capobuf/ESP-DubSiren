#pragma once

// The display renders into a framebuffer on Android: no SPI bus exists.
class SPIClass {
public:
    void begin(int = -1, int = -1, int = -1, int = -1) {}
};

inline SPIClass SPI;
