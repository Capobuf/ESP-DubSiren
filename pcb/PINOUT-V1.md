# Pinout V1

## ESP32-S3-WROOM-1

| GPIO / pin | Net | Funzione |
|---|---|---|
| EN | ESP_EN | reset |
| IO0 | BOOT_GPIO0 | boot |
| IO1 / IO2 | ENC_A / ENC_B | encoder |
| IO4 | IOEXP_INTA | interrupt MCP23017 |
| IO5 / IO6 | I2C_SDA / I2C_SCL | I²C |
| IO7 / IO8 / IO9 | DISPLAY_RST / DC / CS | display |
| IO10 | ADC_CS | chip-select MCP3208 |
| IO11 / IO12 / IO13 | SPI_MOSI / SCK / MISO | SPI |
| IO14 | DISPLAY_BL_PWM | backlight |
| IO15 | GPIO15 | espansione |
| IO16 / IO17 / IO18 | I2S_BCLK / LRCK / DATA | Adafruit 6421 |
| IO38–IO42, IO47, IO48 | GPIOxx | espansione/display |
| RXD0 / TXD0 | UART_RX / UART_TX | test point |
| USB_D- / USB_D+ | USB_DM / USB_DP | USB-C via ESD e 22 Ω |

IO3, IO21, IO35–IO37, IO45 e IO46 sono NC.

## MCP3208

| Canale | Segnale |
|---:|---|
| CH0 | POT1 |
| CH1 | POT2 |
| CH2 | POT3 |
| CH3 | POT4 |
| CH4 | POT5 |
| CH5 | MASTER |
| CH6 | ADC_CH6 / TP5 |
| CH7 | ADC_CH7 / TP6 |

I potenziometri sono tra +3V3 e GND; il cursore attraversa 1 kΩ e 100 nF verso l’ADC.

## MCP23017

| Pin | Net / funzione |
|---|---|
| GPA0–GPA3 | TRIGGER, MOD DOWN, MOD UP, ECHO CUT |
| GPA4 | ENC_SW |
| GPA5 | DAC_MUTE |
| GPA6–GPA7 | espansione |
| GPB0–GPB3 | espansione |
| A0–A2 | GND, indirizzo 0x20 |
| INTA | IOEXP_INTA |
| RESET | IOEXP_RESET, pull-up 10 kΩ |

GPB4–GPB7 e INTB sono NC.

## Adafruit 6421

| Pin | Net |
|---:|---|
| 1 VIN | +5V |
| 2, 9 AGND | GND |
| 3 WSEL/LRCK | I2S_LRCK |
| 4 DIN | I2S_DATA |
| 5 BCLK | I2S_BCLK |
| 6 MCLK | NC |
| 7 MUTE | DAC_MUTE |
| 8 LOUT | AUDIO_L |
| 10 ROUT | AUDIO_R |
| 16 SDA/MOSI | I2C_SDA |
| 17 SCL/SCK | I2C_SCL |

I pin strap 11–15 e 18–20 restano alla configurazione del modulo e sono NC nello schema.

## J3 — display 2×6

| Pin | Net | Pin | Net |
|---:|---|---:|---|
| 1 | +5V | 2 | GND |
| 3 | +3V3 | 4 | I2C_SDA |
| 5 | I2C_SCL | 6 | DISPLAY_CS |
| 7 | DISPLAY_DC | 8 | DISPLAY_RST |
| 9 | DISPLAY_BL_PWM | 10 | GPIO47 |
| 11 | GPIO48 | 12 | GND |

## J4 — espansione 2×10

| Pin | Net | Pin | Net |
|---:|---|---:|---|
| 1 | +5V | 2 | +3V3 |
| 3 | GND | 4 | I2C_SDA |
| 5 | I2C_SCL | 6 | GPIO15 |
| 7 | GPIO38 | 8 | GPIO39 |
| 9 | GPIO40 | 10 | GPIO41 |
| 11 | GPIO42 | 12 | GPIO47 |
| 13 | GPIO48 | 14 | IOEXP_GPA6 |
| 15 | IOEXP_GPA7 | 16 | IOEXP_GPB0 |
| 17 | IOEXP_GPB1 | 18 | IOEXP_GPB2 |
| 19 | IOEXP_GPB3 | 20 | GND |

## Test point

TP1 +5V; TP2 +3V3; TP3–TP4 GND; TP5 ADC_CH6; TP6 ADC_CH7; TP7 AUDIO_L; TP8 AUDIO_R; TP9 AUDIO_GND; TP10 UART_RX; TP11 UART_TX.
