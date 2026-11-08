#pragma once

#include <Arduino.h>
#include <driver/i2s.h>

#include <stddef.h>
#include <stdint.h>
#include "Config.h"

class I2sPcm5102Sink {
public:
    I2sPcm5102Sink();
    ~I2sPcm5102Sink();

    bool begin();
    bool write(const int16_t *samples, size_t count);
    void silence();

private:
    static constexpr i2s_port_t kPort = I2S_NUM_0;

    bool ready_;
    int16_t stereoBuffer_[Config::kBlockSamples * 2];
};
