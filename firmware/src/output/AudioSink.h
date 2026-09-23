#pragma once

#include <stddef.h>
#include <stdint.h>

class AudioSink {
public:
    virtual ~AudioSink() = default;
    virtual bool begin() = 0;
    virtual bool write(const int16_t *samples, size_t count) = 0;
};
