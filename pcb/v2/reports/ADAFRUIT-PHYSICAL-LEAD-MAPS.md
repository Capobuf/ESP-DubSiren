# Adafruit module physical lead maps

Footprint lead-map acceptance completed through Konnect library queries, live
scratch placement, instance readback and visual inspection. Evidence:
`adafruit-footprint-mcp-readback.json`, `adafruit-scratch-mcp-readback.json`,
`adafruit-scratch.svg` and `adafruit-scratch.png`. This establishes the carrier
hole/header mapping, not a complete assembly, board DRC pass or physical fit.

Coordinates in mm, X right / Y down. Footprint origins are PCB outline centers.
Mount holes on the carrier are NPTH at the official module hole centers; 
they are mechanical features, not electrical symbol pins. Module plated holes
do not imply electrically connected carrier mounting holes.

## 5393

Source: `adafruit-5393\Adafruit 1.47in 320x172 Round Rectangle TFT.brd`; revision recorded beside source.
View: display face / Eagle bottom.
Footprint: `ESP-DubSiren-V2-Mechanical:Adafruit_5393_DisplayFace`.

| Physical lead | Function | Symbol / pad | X | Y | Drill | Pad diameter |
|---|---|---|---:|---:|---:|---:|
| JP1.1 | VIN | 1 | -12.7 | 13.335 | 1.0 | 1.9304 |
| JP1.2 | +3V3 | 2 | -10.16 | 13.335 | 1.0 | 1.9304 |
| JP1.3 | GND | 3 | -7.62 | 13.335 | 1.0 | 1.9304 |
| JP1.4 | SCK | 4 | -5.08 | 13.335 | 1.0 | 1.9304 |
| JP1.5 | MISO | 5 | -2.54 | 13.335 | 1.0 | 1.9304 |
| JP1.6 | MOSI | 6 | 0.0 | 13.335 | 1.0 | 1.9304 |
| JP1.7 | TFTCS | 7 | 2.54 | 13.335 | 1.0 | 1.9304 |
| JP1.8 | TFTRST | 8 | 5.08 | 13.335 | 1.0 | 1.9304 |
| JP1.9 | TFTDC | 9 | 7.62 | 13.335 | 1.0 | 1.9304 |
| JP1.10 | CARDCS | 10 | 10.16 | 13.335 | 1.0 | 1.9304 |
| JP1.11 | LITE | 11 | 12.7 | 13.335 | 1.0 | 1.9304 |

Pin 1 pad is square, using the same maximum diameter as the source land.
- U$7: X=-15.875, Y=-12.7, NPTH 2.5 mm.
- U$8: X=15.875, Y=-12.7, NPTH 2.5 mm.
- U$9: X=-15.875, Y=12.7, NPTH 2.5 mm.
- U$10: X=15.875, Y=12.7, NPTH 2.5 mm.

Counts: 11 electrical leads, same symbol/pad count; zero duplicate pads; four NPTH mounts.

F.Fab includes the exact TFT, EYESPI and microSD source outlines.
The additional SD card insertion outlines are reference geometry only; no SD card is fitted.
The carrier uses the display-facing view; its header order is reversed relative to the Eagle top view.

## 6250

Source: `adafruit-6250\Adafruit PCM510x I2S DAC.brd`; revision recorded beside source.
View: component face / Eagle top.
Footprint: `ESP-DubSiren-V2-Mechanical:Adafruit_6250_ComponentFace`.

| Physical lead | Function | Symbol / pad | X | Y | Drill | Pad diameter |
|---|---|---|---:|---:|---:|---:|
| JP2.1 | VIN | 1 | -8.89 | 7.62 | 1.0 | 1.6764 |
| JP2.2 | AGND | 2 | -6.35 | 7.62 | 1.0 | 1.6764 |
| JP2.3 | WORDSEL_IN | 3 | -3.81 | 7.62 | 1.0 | 1.6764 |
| JP2.4 | DATA_IN | 4 | -1.27 | 7.62 | 1.0 | 1.6764 |
| JP2.5 | BITCLK_IN | 5 | 1.27 | 7.62 | 1.0 | 1.6764 |
| JP2.6 | OUTL | 6 | 3.81 | 7.62 | 1.0 | 1.6764 |
| JP2.7 | AGND | 7 | 6.35 | 7.62 | 1.0 | 1.6764 |
| JP2.8 | OUTR | 8 | 8.89 | 7.62 | 1.0 | 1.6764 |
| JP1.1 | 3.3V | 9 | 6.35 | -7.62 | 1.0 | 1.778 |
| JP1.2 | FORMAT | 10 | 3.81 | -7.62 | 1.0 | 1.778 |
| JP1.3 | MUTE | 11 | 1.27 | -7.62 | 1.0 | 1.778 |
| JP1.4 | SCK_IN | 12 | -1.27 | -7.62 | 1.0 | 1.778 |
| JP1.5 | FILTER | 13 | -3.81 | -7.62 | 1.0 | 1.778 |
| JP1.6 | DEMPH | 14 | -6.35 | -7.62 | 1.0 | 1.778 |

Pin 1 pad is square, using the same maximum diameter as the source land.
- U$11: X=-12.7, Y=7.62, NPTH 2.5 mm.
- U$12: X=12.7, Y=7.62, NPTH 2.5 mm.
- U$13: X=-12.7, Y=-7.62, NPTH 2.5 mm.
- U$14: X=12.7, Y=-7.62, NPTH 2.5 mm.

Counts: 14 electrical leads, same symbol/pad count; zero duplicate pads; four NPTH mounts.

Secondary header JP1 maps to carrier pads 9–14. It is unpopulated; all six schematic pins are NC.
The eight-pin primary header alone carries the project signals.

