# V2 electrical design evidence

Status: **SCHEMATIC COMPLETE - prototype engineering, not a fabrication release.** The saved schematic contains 57 fitted purchased components and seven PCB test pads, plus virtual power symbols/flags. All twelve performance controls are present. RV1-RV7 and SW7 use accepted Bourns footprints; SW1-SW4 use the native CHERRY footprint supported by the official MX1A-to-MX2A form-fit-function equivalence notice and legacy PCB drawing. USB power qualification and PCB validation remain separate tasks.

The final saved schematic reports **0 ERC errors and 0 warnings**, with no suppressions. Wire endpoints, component connections, shorts and orphan checks also return zero findings. These checks certify connectivity of present components, not PCB routing or USB qualification. The final schematic render and netlist were inspected; V1 was not modified.

## Saved evidence and BOM

- `../BOM-V2.csv` combines the actual fitted schematic parts with purchased knobs. It is an engineering unit BOM, not a fabrication release. `BOM-V2-SCHEMATIC.csv` is the direct MCP export of 64 present physical rows (57 purchased parts and seven PCB test pads).
- `netlist-v2-complete.net`, `erc-v2-complete.json`, `cherry-schematic-readback.json` and `schematic-v2-complete.png/.pdf` describe the current saved schematic. Earlier progress/interim exports remain historical evidence.
- `CONTROLS-POWER-PHYSICAL-PIN-MAPS.md`, `controls-library-create.json`, `controls-scratch-readback.json` and `power-switch-acceptance.json` record the new lead-to-symbol-to-pad acceptance. `controls-power-scratch.svg/.png` and `controls-power-symbols.png` show the disposable acceptance placements.
- Earlier `ELECTRICAL-PHYSICAL-PIN-MAPS.csv`, `USB-RESET-RCA-PIN-MAPS.csv` and Adafruit maps remain applicable to the previously accepted parts.
- `USB-POWER-DECISION.md` supersedes the earlier provisional current/inrush paragraphs. `CHERRY-NATIVE-FOOTPRINT-AUDIT.md` records acceptance of the native footprint and its pin map for MX2A.
- PCB placement/routing and its DRC are tracked by the mechanical/PCB workstream. Schematic ERC does not establish board acceptance.

## Display, audio and GPIO

Use [`PINOUT-V2.md`](../PINOUT-V2.md) for the complete allocation. GPIO35, GPIO36 and GPIO37 are reserved for the N16R8 octal PSRAM; the custom symbol marks them no-connect. GPIO3, GPIO45 and GPIO46 strapping pins are unused. Native USB uses GPIO19/20. Display and I2S assignments preserve the current firmware. Physical control scanning and debouncing are not implemented by this hardware work.

Adafruit 5393 VIN is connected to **3.3 V**. Its official Eagle schematic has 10 kohm TFTCS and RESET pull-ups to VIN. A 5 V VIN could drive ESP32 pins above VDD + 0.3 V while the MCU is high impedance. The official guide also specifies matching VIN to logic voltage. The module's 3Vo output is unconnected; it is not paralleled with the main rail. MISO and SDCS are unused; the onboard SDCS pull-up holds the SD card inactive. LITE goes to GPIO14.

Adafruit 6250 uses protected 5 V and direct 3.3 V I2S. Its onboard configuration defaults remain intact. Only OUTL is used: U6 pin 6 to J2 pin 2, the X1 center. J2 pin 4, the X2 center, is unconnected. Both barrels, pins 1 and 3, are grounded. OUTR is unconnected; there is no L/R summing. The existing firmware duplicates mono into both slots, so the selected channel carries mono. External load must be at least 1 kohm. The breakout already supplies reconstruction filtering and ground-centered line output; no redundant coupling capacitor or analog filter is added.

## Seven pots through one Toshiba 74HC4051D

Toshiba Rev 5.0 permits VCC from 2-6 V. Here VCC is 3.3 V, VEE and GND are ground, and INH is held low. Channels 0-5 are POT1-POT6, channel 6 is MASTER, and unused channel 7 is grounded. COM goes to GPIO4 / ADC1_CH3.

Direct 3.3 V pot endpoints exceed Espressif's calibrated attenuation-3 measurement range of 0-2.9 V. A purchased 4.7 kohm, 1% RC0603FR-074K7L in series with each pot's high terminal limits nominal 20 kohm front-pot endpoints to 2.672 V, and the 10 kohm master to 2.245 V. At a conservative 3.4 V rail, +20% pot resistance and -1% excitation resistor, the maxima are 2.848 V and 2.450 V. Seven resistors fit within the ten purchased. Separate excitation resistors avoid sharing sampling transients through one common reference impedance. The high-impedance wiper reading remains linear with angle; firmware must calibrate endpoints.

One 100 nF capacitor sits at the MCU ADC input, following Espressif's recommendation. The worst front-pot Thevenin resistance is bounded by (24 kohm + 4.747 kohm) / 4 = 7.187 kohm. Toshiba does not guarantee ON resistance at 3.3 V. An engineering estimate of 1 kohm for mux resistance and +10% capacitance gives a 0.901 ms time constant. A 7 ms settling interval gives about 0.043% residual under that assumption, or roughly 20 complete seven-channel scans/s. Discard an initial conversion, use calibrated attenuation 3 and average subsequent readings. This is a prototype starting point, not a guaranteed Toshiba timing limit. Verify response latency and settling; faster performance may require a justified smaller filter capacitor or another charge-storage arrangement.

## MPM3612GLQ-33-Z minimum circuit

MPS Rev 1.0, pages 3-4 and 15-19 establishes the pin map: 1 EN tied to VIN; 2 VIN from protected 5 V; 3 GND; 4 and 5 VOUT; 6 VOUT_S sensed through 10 ohm; 7 PG unused; 8 VCC optional capacitor omitted; exposed pad to GND. Fit one 22 uF plus 100 nF at input and one 22 uF plus 100 nF at output. R1 is the purchased NRC06J100TRF. There is no external inductor, MOSFET, Schottky, feedback divider or second regulator.

The package has eight functional edge lands plus an exposed ground pad. NC1/NC2 metal regions are forbidden PCB copper/via contact areas, not extra solder lands. The footprint omits them and marks the local forbidden region X = [-0.575, +0.575] mm, Y = [-1.50, -1.10] mm on Dwgs.User. Before release, layout must enforce this as a board copper keepout or prove equivalent copper exclusion. A drawing alone is not an enforced keepout.

Samsung's exact CL21A226MPCLRNC DC-bias curve reports typical loss of 68.3788% at 5 V and 52.3712% at 3.3 V: effective capacitance is 6.957 uF and 10.478 uF at typical 25 C. At 1 A, 5 V input, 3.3 V output and 1.25 MHz, MPS equation 5 gives about 25.8 mV input ripple. With the integrated 2.2 uH inductor, equation 8 gives about 3.9 mV ideal capacitive output ripple, excluding ESR and transients. Applying initial -20% tolerance and a separate -15% temperature allowance raises these estimates to 38.0 mV and 5.7 mV. These calculations support a prototype starting network, not guaranteed combined bias/temperature corners. Aging, ESR heating and load-step response remain measurements. The MCU has its own separate 22 uF and 100 nF pair, consistent with Espressif's reference circuit.

## USB, protection and input power

J1 is the exact GCT USB4085-GF-A. Manufacturer drawing Rev B, pages 1-2 shows 16 signal tails and **four** shell stakes, matching 20 physical solder lands. The schematic has 16 contact pins plus one shared SH pin. The purchasing description '16+8POS' is not used to infer eight shell legs. All four shell lands share ground. The GCT PCN explicitly states the PCB footprint was unchanged in Rev B. KiCad's matching footprint has the correct component-side map, 0.4 mm signal drills and four plated shell slots; its 0.70 mm signal lands are 0.05 mm wider than GCT's recommended 0.65 mm, retaining 0.15 mm copper spacing at 0.85 mm pitch and increasing radial annulus by 0.025 mm. This is an explicit land-size choice, not a numbering discrepancy.

Each CC pin has its own 5.1 kohm, 1% Rd. D+ and D- use 22 ohm series resistors near the MCU. The purchased ESD array is **SLKOR USBLC6-2SC6**, not ST: pin pairs 1/6 and 3/4 are the two protected lines, pin 2 is GND, and pin 5 is VBUS_RAW. Place it next to the receptacle. The PANJIT SMF5.0A is unidirectional: cathode pad 1 to VBUS_PROTECTED, anode pad 2 to GND. It uses the unidirectional `Device:D_Zener` symbol.

The Bel 0ZCG0110CF2B is a 1.1 A hold / 2.2 A trip PTC at 23 C, rated 33 V. Its exact recommended land pattern is used: two 1.78 x 3.5 mm pads, centers +/-2.615 mm. Initial resistance is 0.06 ohm minimum; post-trip maximum is 0.2 ohm. Manufacturer derating reduces hold current to about 0.671 A at 70 C and 0.528 A at 85 C. Enclosure temperature and fault coordination need validation.

U7 TPS22918DBVR now separates direct attach capacitance from the existing MPM and DAC bulk capacitors. The path is `VBUS_RAW -> F1 -> VBUS_PROTECTED -> U7 -> +5V`; D1 and the new 1 uF C10 sit on VBUS_PROTECTED. U7 ON follows VIN, CT uses new 10 nF / 50 V C0G C11, and QOD is intentionally unused. This is the minimum selected controlled-slew arrangement after assessing a passive 4.7 uF buck-input alternative and 3.3 V DAC powering. The DAC's own direct 10 uF VIN capacitor makes reducing buck CIN alone insufficient. The accepted 22 uF MPM input capacitor is retained behind U7.

**Power qualification remains open.** The [power decision](USB-POWER-DECISION.md) gives a 1.40 uF direct-input design allowance, typical 26.55 ms output ramp and complete startup-capacitor analysis. TI supplies no guaranteed minimum ramp time, so the estimates are not a guaranteed worst-case inrush limit. Intended upper operating allocations yield about 487 mA at the selected low-voltage/85% efficiency corner, leaving only 13 mA against a 500 mA configured-device allowance. Display/backlight and complete MCU activity allocations require measurement; a higher MCU transient can exceed that allowance. The existing firmware has not implemented or verified USB pre-configuration/suspend power policy. Source-current detection is required if tested sustained operating modes exceed the default allowance; no USB compliance claim is made.

## RESET and BOOT

SW5 and SW6 are exact PTS636SM43SMTR LFS gull-wing switches, 4.3 mm actuator height. The installed generic PTS636 footprint was for the 2.5 mm version and was rejected. The custom footprint follows current MI 09/02/26 page 2: two 2.0 x 1.5 mm lands centered at +/-4.1 mm, a 6.0 x 3.5 mm body and sufficient courtyard. It is a symmetric two-terminal SPST switch; reversing the two equivalent contacts changes neither function nor geometry. SW5 pulls EN low; SW6 pulls GPIO0 low. Both have 10 kohm pull-ups; EN has the purchased 1 uF reset capacitor. The switch 3D model remains pending.

## Sources

- [Espressif ESP32-S3-WROOM-1/1U v1.8](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf), pages 11-12, 27-29, 41 and 46.
- [Espressif hardware guidelines](https://documentation.espressif.com/esp-hardware-design-guidelines/en/latest/esp32s3/index.html), ADC, USB, supply and reset.
- [MPS MPM3612-33 Rev 1.0](https://www.monolithicpower.com/en/documentview/productdocument/index/version/2/document_type/Datasheet/lang/en/sku/MPM3612GLQ-33/document_id/11833/), pages 3-4 and 15-19.
- [Samsung CL21A226MPCLRNC](https://product.samsungsem.com/mlcc/CL21A226MPCLRN.do); exact curve saved in `samsung-22uF-dc-bias.csv`.
- [Toshiba 74HC4051D](https://toshiba.semicon-storage.com/us/semiconductor/product/general-purpose-logic-ics/detail.74HC4051D.html), Rev 5.0, pages 2-6 and 14.
- [SLKOR USBLC6-2SC6](https://www.slkoric.com/upload/file/1782371251_7008.PDF), pages 1-3.
- [PANJIT SMF5.0A](https://www.panjit.com.cn/upload/datasheet/SMF5.0A_SERIES.pdf), polarity and package.
- [Adafruit 5393](https://learn.adafruit.com/adafruit-1-47-round-rectangle-tft-display?view=all) and [6250](https://learn.adafruit.com/adafruit-pcm510x-i2s-dac?view=all); official Eagle source copies are stored under `sources/adafruit-5393` and `sources/adafruit-6250`.
- [GCT USB4085 Rev B manufacturer drawing, republished](https://az.y-ic.com/datasheet/e9/USB4085-GF-A.pdf) and [GCT PCN 20230426](https://media.digikey.com/pdf/PCNs/GCT/USB4085%20B%20PCN%2020230426.pdf).
- [C&K PTS636 current datasheet](https://www.ckswitches.com/media/2779/pts636.pdf), MI 09/02/26, pages 2 and 8.
