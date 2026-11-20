#pragma once

// Arduino Print, reduced to what Adafruit GFX and the display code call.

#include <Arduino.h>

class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size) {
        size_t count = 0;
        while (size--) count += write(*buffer++);
        return count;
    }
    size_t write(const char *text) {
        return text ? write(reinterpret_cast<const uint8_t *>(text), strlen(text)) : 0;
    }
    size_t print(const char *text) { return write(text); }
    size_t print(const String &text) { return write(text.c_str()); }
    size_t print(char character) { return write(static_cast<uint8_t>(character)); }
};
