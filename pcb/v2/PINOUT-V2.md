# V2 GPIO and connector pinout

**INTERIM:** Derived from the saved KiCad netlist. Performance controls remain absent until exact mechanical acceptance. GPIO names and physical module pad numbers are different identifiers.

| MCU physical pad | Function / GPIO | Saved net | Status |
|---:|---|---|---|
| 1 | GND_1 | GND | Connected |
| 2 | 3V3_2 | +3V3 | Connected |
| 3 | EN_3 | /ESP_EN | Connected |
| 4 | IO4_4 | /ADC_MUX | Connected |
| 5 | IO5_5 | /MUX_S0 | Connected |
| 6 | IO6_6 | /MUX_S1 | Connected |
| 7 | IO7_7 | /TFT_RST | Connected |
| 8 | IO15_8 | unconnected-(U1-IO15-Pad8) | NC |
| 9 | IO16_9 | /I2S_BCLK | Connected |
| 10 | IO17_10 | /I2S_LRCK | Connected |
| 11 | IO18_11 | /I2S_DIN | Connected |
| 12 | IO8_12 | /TFT_DC | Connected |
| 13 | USB_D-_13 | /USB_DM_MCU | Connected |
| 14 | USB_D+_14 | /USB_DP_MCU | Connected |
| 15 | IO3_15 | unconnected-(U1-IO3-Pad15) | Unused strapping GPIO; NC |
| 16 | IO46_16 | unconnected-(U1-IO46-Pad16) | Unused strapping GPIO; NC |
| 17 | IO9_17 | /TFT_CS | Connected |
| 18 | IO10_18 | /MUX_S2 | Connected |
| 19 | IO11_19 | /SPI_MOSI | Connected |
| 20 | IO12_20 | /SPI_SCK | Connected |
| 21 | IO13_21 | unconnected-(U1-IO13-Pad21) | NC |
| 22 | IO14_22 | /TFT_LITE | Connected |
| 23 | IO21_23 | unconnected-(U1-IO21-Pad23) | NC |
| 24 | IO47_24 | unconnected-(U1-IO47-Pad24) | NC |
| 25 | IO48_25 | unconnected-(U1-IO48-Pad25) | NC |
| 26 | IO45_26 | unconnected-(U1-IO45-Pad26) | Unused strapping GPIO; NC |
| 27 | IO0_27 | /BOOT | Connected |
| 28 | RESERVED_IO35_28 | unconnected-(U1-RESERVED_IO35-Pad28) | Reserved for octal PSRAM; must remain NC |
| 29 | RESERVED_IO36_29 | unconnected-(U1-RESERVED_IO36-Pad29) | Reserved for octal PSRAM; must remain NC |
| 30 | RESERVED_IO37_30 | unconnected-(U1-RESERVED_IO37-Pad30) | Reserved for octal PSRAM; must remain NC |
| 31 | IO38_31 | /BTN1 | MCU and pull-up fitted; control not fitted |
| 32 | IO39_32 | /BTN2 | MCU and pull-up fitted; control not fitted |
| 33 | IO40_33 | /BTN3 | MCU and pull-up fitted; control not fitted |
| 34 | IO41_34 | /BTN4 | MCU and pull-up fitted; control not fitted |
| 35 | IO42_35 | /ENC_SW | MCU and pull-up fitted; control not fitted |
| 36 | RXD0_36 | /UART_RX | Connected |
| 37 | TXD0_37 | /UART_TX | Connected |
| 38 | IO2_38 | /ENC_B | MCU and pull-up fitted; control not fitted |
| 39 | IO1_39 | /ENC_A | MCU and pull-up fitted; control not fitted |
| 40 | GND_40 | GND | Connected |
| 41 | GND_41 | GND | Connected |

## Interface allocation

| Interface | GPIO | Physical MCU pad |
|---|---|---|
| ADC1_CH3 | 4 | 4 |
| Mux S0 / S1 / S2 | 5 / 6 / 10 | 5 / 6 / 18 |
| Display reset / DC / CS | 7 / 8 / 9 | 7 / 12 / 17 |
| Display MOSI / SCK / LITE | 11 / 12 / 14 | 19 / 20 / 22 |
| I2S BCLK / LRCK / DIN | 16 / 17 / 18 | 9 / 10 / 11 |
| USB D- / D+ | 19 / 20 | 13 / 14 |
| Four performance switches | 38 / 39 / 40 / 41 | 31 / 32 / 33 / 34 |
| Encoder A / B / push | 1 / 2 / 42 | 39 / 38 / 35 |
| UART RX / TX | 44 / 43 | 36 / 37 |
| BOOT / RESET | 0 / EN | 27 / 3 |

Display and I2S allocations match the current firmware. Pot scanning, button handling and encoder debouncing still require firmware. GPIO13 remains unconnected despite being configured as the unused SPI MISO parameter; the display SD interface is not used.

## Fitted connector and breakout nets

### J1

| Physical pin | Saved net |
|---|---|
| A5 | /USB_CC1 |
| B5 | /USB_CC2 |
| A7 | /USB_DM_CONN |
| B7 | /USB_DM_CONN |
| A6 | /USB_DP_CONN |
| B6 | /USB_DP_CONN |
| A4 | /VBUS_RAW |
| A9 | /VBUS_RAW |
| B4 | /VBUS_RAW |
| B9 | /VBUS_RAW |
| A1 | GND |
| A12 | GND |
| B1 | GND |
| B12 | GND |
| SH | GND |
| A8 | unconnected-(J1-SBU1-PadA8) |
| B8 | unconnected-(J1-SBU2-PadB8) |

### J2

| Physical pin | Saved net |
|---|---|
| 2 | /AUDIO_MONO |
| 1 | GND |
| 3 | GND |
| 4 | unconnected-(J2-X2_CENTER-Pad4) |

### U5

| Physical pin | Saved net |
|---|---|
| 1 | +3V3 |
| 6 | /SPI_MOSI |
| 4 | /SPI_SCK |
| 7 | /TFT_CS |
| 9 | /TFT_DC |
| 11 | /TFT_LITE |
| 8 | /TFT_RST |
| 3 | GND |
| 2 | unconnected-(U5-3V3_OUT-Pad2) |
| 5 | unconnected-(U5-MISO_SD-Pad5) |
| 10 | unconnected-(U5-SD_CS-Pad10) |

### U6

| Physical pin | Saved net |
|---|---|
| 1 | +5V |
| 6 | /AUDIO_MONO |
| 5 | /I2S_BCLK |
| 4 | /I2S_DIN |
| 3 | /I2S_LRCK |
| 2 | GND |
| 7 | GND |
| 9 | unconnected-(U6-3V3_OUT-Pad9) |
| 14 | unconnected-(U6-DEMPH-Pad14) |
| 13 | unconnected-(U6-FILTER-Pad13) |
| 10 | unconnected-(U6-FORMAT-Pad10) |
| 12 | unconnected-(U6-MCLK-Pad12) |
| 11 | unconnected-(U6-MUTE-Pad11) |
| 8 | unconnected-(U6-OUTR-Pad8) |

### SW5

| Physical pin | Saved net |
|---|---|
| 1 | /ESP_EN |
| 2 | GND |

### SW6

| Physical pin | Saved net |
|---|---|
| 1 | /BOOT |
| 2 | GND |

U5 is Adafruit 5393, primary header 1-11; its VIN is 3.3 V. U6 is Adafruit 6250, primary JP2 pins 1-8; secondary JP1 pins 1-6 are represented as footprint/symbol pins 9-14 and left unconnected. Header 3Vo outputs are never tied to the carrier supply.

J1 has four physical shield stakes sharing SH. J2 is a dual RCA block: pin 2 is the connected X1 center, pin 4 is the unused X2 center, pins 1 and 3 are barrels.

## Planned controls, not fitted

| Reference | Exact MPN | Intended connection | Acceptance gap |
|---|---|---|---|
| RV1-RV6 | PTV111-3420A-B203 | 1 GND, 2 POT1-POT6, 3 respective POT_TOP, 4 dummy NC | View and row-to-axis geometry unresolved |
| RV7 | PTV111-1420A-B103 | 1 GND, 2 MASTER, 3 POT7_TOP, 4 dummy NC | View and row-to-axis geometry unresolved |
| SW1-SW4 | MX2A-11NW | BTN1-BTN4 to GND, 10 kohm pull-ups fitted | Exact MX2A drawing not verified |
| SW7 | PEC11R-4120F-S0018 | A ENC_A, B ENC_B, C GND; push to ENC_SW/GND | PCB drawing view unresolved |

The absent parts explain the empty lower schematic regions. ERC cannot identify missing switches when a pull-up and MCU already give their net two connected pins. No generic substitute has been placed to hide this incompleteness.
