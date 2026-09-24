#pragma once

#include <Arduino.h>
#include <driver/i2s.h>

#include "AudioSink.h"
#include "Config.h"

class I2sPcm5102Sink final : public AudioSink {
public:
    I2sPcm5102Sink();
    ~I2sPcm5102Sink() override;

    bool begin() override;
    bool write(const int16_t *samples, size_t count) override;
    void silence();

private:
    static constexpr i2s_port_t kPort = I2S_NUM_0;

    bool ready_;
    int16_t stereoBuffer_[Config::kBlockSamples * 2];
};
