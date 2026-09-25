#pragma once

#include <math.h>
#include <string>

constexpr float TWO_PI = 6.2831853071795864769f;
using SemaphoreHandle_t = int *;
constexpr int portMAX_DELAY = 0;
constexpr int pdTRUE = 1;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { static int mutex; return &mutex; }
inline void vSemaphoreDelete(SemaphoreHandle_t) {}
inline int xSemaphoreTake(SemaphoreHandle_t, int) { return pdTRUE; }
inline void xSemaphoreGive(SemaphoreHandle_t) {}

class String {
public:
    String() = default;
    String(const char *value) : value_(value) {}
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
    String substring(int start) const { return String(value_.substr(start)); }
    String substring(int start, int end) const {
        return String(value_.substr(start, end - start));
    }
    const char *c_str() const { return value_.c_str(); }
    bool operator==(const char *other) const { return value_ == other; }
private:
    explicit String(std::string value) : value_(value) {}
    std::string value_;
};
