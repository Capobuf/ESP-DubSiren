#include "PanelLcd.h"

#include <Arduino.h>
#include <driver/spi_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#include "Config.h"

namespace {

constexpr spi_host_device_t kHost = SPI2_HOST;
// A frame takes about 22 ms on the bus, well inside the display refresh.
constexpr unsigned kPixelClockHz = 40 * 1000 * 1000;
constexpr size_t kFrameBytes = PanelLcd::kWidth * PanelLcd::kHeight * sizeof(uint16_t);
// Called by esp_lcd once the last DMA chunk of a frame is out.
bool frameSent(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *busy) {
    static_cast<std::atomic<bool> *>(busy)->store(false);
    return false;
}

}  // namespace

bool PanelLcd::begin() {
    spi_bus_config_t bus = {};
    bus.mosi_io_num = Config::kSpiMosiPin;
    bus.miso_io_num = -1;  // the panel is only written
    bus.sclk_io_num = Config::kSpiSckPin;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = kFrameBytes;  // esp_lcd splits it in 32 kB DMA chunks
    if (spi_bus_initialize(kHost, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return false;

    esp_lcd_panel_io_spi_config_t ioConfig = {};
    ioConfig.cs_gpio_num = Config::kDisplayCsPin;
    ioConfig.dc_gpio_num = Config::kDisplayDcPin;
    ioConfig.spi_mode = 0;
    ioConfig.pclk_hz = kPixelClockHz;
    ioConfig.trans_queue_depth = 10;  // more than the 4 chunks of a frame: send() never waits
    ioConfig.on_color_trans_done = frameSent;
    ioConfig.user_ctx = &busy_;
    ioConfig.lcd_cmd_bits = 8;
    ioConfig.lcd_param_bits = 8;
    esp_lcd_panel_io_handle_t io = nullptr;
    if (esp_lcd_new_panel_io_spi(reinterpret_cast<esp_lcd_spi_bus_handle_t>(kHost),
                                 &ioConfig, &io) != ESP_OK) return false;

    esp_lcd_panel_dev_config_t device = {};
    device.reset_gpio_num = Config::kDisplayResetPin;
    device.color_space = ESP_LCD_COLOR_SPACE_RGB;
    device.bits_per_pixel = 16;
    esp_lcd_panel_handle_t panel = nullptr;
    if (esp_lcd_new_panel_st7789(io, &device, &panel) != ESP_OK) return false;
    if (esp_lcd_panel_reset(panel) != ESP_OK) return false;
    delay(120);  // ST7789 reset cancel time when the panel was awake (soft reboot)
    // Adafruit Product 5393, as Adafruit_ST7789 init(172, 320) + setRotation(3),
    // how U5 is mounted (MY|MV shows it upside down): MADCTL MX|MV,
    // inversion on, the 172 px glass centred in the 240 px RAM.
    if (esp_lcd_panel_init(panel) != ESP_OK ||
        esp_lcd_panel_invert_color(panel, true) != ESP_OK ||
        esp_lcd_panel_swap_xy(panel, true) != ESP_OK ||
        esp_lcd_panel_mirror(panel, true, false) != ESP_OK ||
        esp_lcd_panel_set_gap(panel, 0, (240 - kHeight) / 2) != ESP_OK ||
        esp_lcd_panel_disp_on_off(panel, true) != ESP_OK) return false;
    panel_ = panel;
    return true;
}

bool PanelLcd::send(uint16_t *pixels) {
    if (panel_ == nullptr || busy_.load()) return false;
    // GFX keeps RGB565 in CPU order; the ST7789 reads the high byte first.
    for (size_t i = 0; i < kFrameBytes / sizeof(uint16_t); ++i)
        pixels[i] = __builtin_bswap16(pixels[i]);
    busy_.store(true);
    if (esp_lcd_panel_draw_bitmap(panel_, 0, 0, kWidth, kHeight, pixels) == ESP_OK) return true;
    busy_.store(false);
    return false;
}
