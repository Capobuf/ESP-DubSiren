#pragma once

// Minimal Arduino/FreeRTOS surface needed to compile the shared firmware
// sources (audio, control, display) on Android. It extends the host stub in
// tests/native/stubs with a real mutex (UI thread and AAudio thread share the
// ControlStore) and the String operations used by display/PanelDisplay.cpp.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>
#include <mutex>
#include <string>
#include <type_traits>

inline uint32_t micros() {
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

inline uint32_t millis() {
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

inline void delay(uint32_t) {}

constexpr float TWO_PI = 6.2831853071795864769f;
inline double radians(double degrees) { return degrees * 0.017453292519943295; }

// Declared only: Adafruit GFX names it in signatures the firmware never calls.
class __FlashStringHelper;

// Flash placement on the ESP32 (fonts); ordinary memory here.
#ifndef PROGMEM
#define PROGMEM
#endif

constexpr uint8_t LOW = 0;
constexpr uint8_t HIGH = 1;
constexpr uint8_t INPUT = 0;
constexpr uint8_t OUTPUT = 1;
inline void pinMode(int, uint8_t) {}
inline void digitalWrite(int, uint8_t) {}

// FreeRTOS mutex mapped to std::mutex: ControlStore::snapshot() runs on the
// audio callback thread while the UI thread writes through the controller.
using SemaphoreHandle_t = std::mutex *;
constexpr int portMAX_DELAY = 0;
constexpr int pdTRUE = 1;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return new std::mutex(); }
inline void vSemaphoreDelete(SemaphoreHandle_t mutex) { delete mutex; }
inline int xSemaphoreTake(SemaphoreHandle_t mutex, int) { mutex->lock(); return pdTRUE; }
inline void xSemaphoreGive(SemaphoreHandle_t mutex) { mutex->unlock(); }

class String {
public:
    String() = default;
    String(const char *value) : value_(value ? value : "") {}
    String(const String &) = default;
    String &operator=(const String &) = default;
    explicit String(char value) : value_(1, value) {}
    explicit String(int value) : value_(std::to_string(value)) {}
    explicit String(unsigned int value) : value_(std::to_string(value)) {}
    explicit String(long value) : value_(std::to_string(value)) {}
    explicit String(unsigned long value) : value_(std::to_string(value)) {}
    explicit String(float value, unsigned decimals = 2) : value_(format(value, decimals)) {}
    explicit String(double value, unsigned decimals = 2) : value_(format(value, decimals)) {}

    void trim() {
        const auto begin = value_.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) { value_.clear(); return; }
        const auto end = value_.find_last_not_of(" \t\r\n");
        value_ = value_.substr(begin, end - begin + 1);
    }
    bool startsWith(const char *prefix) const { return value_.rfind(prefix, 0) == 0; }
    int indexOf(char character, int start = 0) const {
        const auto at = value_.find(character, static_cast<size_t>(start));
        return at == std::string::npos ? -1 : static_cast<int>(at);
    }
    String substring(int start) const {
        if (start < 0) start = 0;
        if (static_cast<size_t>(start) >= value_.size()) return String();
        return fromStd(value_.substr(start));
    }
    String substring(int start, int end) const {
        if (start > end) std::swap(start, end);
        if (start < 0) start = 0;
        if (static_cast<size_t>(start) >= value_.size()) return String();
        return fromStd(value_.substr(start, end - start));
    }
    const char *c_str() const { return value_.c_str(); }
    unsigned length() const { return static_cast<unsigned>(value_.size()); }
    bool isEmpty() const { return value_.empty(); }
    bool operator==(const char *other) const { return value_ == other; }
    bool operator==(const String &other) const { return value_ == other.value_; }
    bool operator!=(const char *other) const { return value_ != other; }

    String &operator+=(const String &other) { value_ += other.value_; return *this; }
    String &operator+=(const char *other) { value_ += other ? other : ""; return *this; }
    String &operator+=(char other) { value_ += other; return *this; }

    friend String operator+(const String &left, const String &right) {
        return fromStd(left.value_ + right.value_);
    }
    friend String operator+(const String &left, const char *right) {
        return fromStd(left.value_ + (right ? right : ""));
    }
    friend String operator+(const char *left, const String &right) {
        return fromStd(std::string(left ? left : "") + right.value_);
    }
    friend String operator+(const String &left, char right) {
        return fromStd(left.value_ + right);
    }
    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> &&
                                                      !std::is_same_v<T, char>>>
    friend String operator+(const String &left, T right) {
        return left + String(right);
    }

private:
    static String fromStd(std::string value) { String result; result.value_ = std::move(value); return result; }
    static std::string format(double value, unsigned decimals) {
        char buffer[48];
        snprintf(buffer, sizeof(buffer), "%.*f", static_cast<int>(decimals), value);
        return buffer;
    }
    std::string value_;
};
