#include "I2sPcm5102Sink.h"

I2sPcm5102Sink::I2sPcm5102Sink() : ready_(false), stereoBuffer_{} {}

I2sPcm5102Sink::~I2sPcm5102Sink() {
    if (ready_) {
        i2s_driver_uninstall(kPort);
    }
}

bool I2sPcm5102Sink::begin() {
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
    config.sample_rate = Config::kSampleRate;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = 0;
    config.dma_buf_count = 4;
    config.dma_buf_len = Config::kBlockSamples;
    config.use_apll = false;
    config.tx_desc_auto_clear = true;
    config.fixed_mclk = 0;
    config.mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT;
    config.bits_per_chan = I2S_BITS_PER_CHAN_16BIT;

    if (i2s_driver_install(kPort, &config, 0, nullptr) != ESP_OK) {
        return false;
    }

    const i2s_pin_config_t pins = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = Config::kI2sBclkPin,
        .ws_io_num = Config::kI2sLrckPin,
        .data_out_num = Config::kI2sDataPin,
        .data_in_num = I2S_PIN_NO_CHANGE,
    };
    if (i2s_set_pin(kPort, &pins) != ESP_OK) {
        i2s_driver_uninstall(kPort);
        return false;
    }

    ready_ = true;
    silence();
    return true;
}

bool I2sPcm5102Sink::write(const int16_t *samples, size_t count) {
    if (!ready_ || samples == nullptr || count > Config::kBlockSamples) {
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        stereoBuffer_[2 * i] = samples[i];
        stereoBuffer_[2 * i + 1] = samples[i];
    }

    const size_t byteCount = count * 2 * sizeof(int16_t);
    size_t bytesWritten = 0;
    const esp_err_t result = i2s_write(
        kPort, stereoBuffer_, byteCount, &bytesWritten,
        pdMS_TO_TICKS(Config::kBlockDurationMs));
    return result == ESP_OK && bytesWritten == byteCount;
}

void I2sPcm5102Sink::silence() {
    if (ready_) {
        i2s_zero_dma_buffer(kPort);
    }
}
