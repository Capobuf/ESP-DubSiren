# V2 purchasing audit

Source: repository-root `2026-10-02T050640.csv`, all 26 rows. The original file
is preserved. Quantities below are purchased stock, not quantities fitted in a
complete unit. The interim schematic BOM is a separate report.

| Row | Purchased | Manufacturer | Exact purchasing MPN | Finding / intended use |
|---:|---:|---|---|---|
| 1 | 1 | GCT | USB4085-GF-A | USB 2.0 Type-C receptacle, 16 signal/power tails plus four shell stakes; manufacturer drawing overrides ambiguous CSV description |
| 2 | 1 | Bourns | PTV111-1420A-B103 | MASTER, 10 kohm linear; four physical pins including dummy |
| 3 | 1 | Espressif | ESP32-S3-WROOM-1U-N16R8 | External-antenna module, 16 MB flash / 8 MB octal PSRAM |
| 4 | 1 | Same Sky (formerly CUI Devices) | RCJ-2123 | One dual RCA block, red and white sockets; purchasing description does not mean one physical socket |
| 5 | 4 | CHERRY | MX2A-11NW | Black linear, PCB locating-pin variant; exact mechanical drawing still needed |
| 6 | 6 | Bourns | PTV111-3420A-B203 | Front pots, 20 kohm linear; four physical pins including dummy |
| 7 | 1 | Bourns | PEC11R-4120F-S0018 | 18 pulses, 20 mm flatted shaft, push switch |
| 8 | 1 | Cliff | CL170805B | K85M black/grey knob; 6 mm D bore, 5 mm across flat; MASTER fit unresolved |
| 9 | 1 | Same Sky | SAK-023 | D bore 6 x 4.5 mm; provisional encoder assignment |
| 10 | 6 | Davies Molding | 1100 | Front knobs, 1/4 inch bore and set screw; 6 mm shaft concentricity needs physical check |
| 11 | 1 | MPS | MPM3612GLQ-33-Z | Fixed 3.3 V, 1 A integrated-inductor power module |
| 12 | 1 | Bel | 0ZCG0110CF2B | 1812 resettable PTC, 1.1 A hold / 33 V; custom verified recommended lands |
| 13 | 2 | C&K / Littelfuse | PTS636SM43SMTR LFS | RESET and BOOT, exact gull-wing 4.3 mm actuator variant |
| 14 | 10 | Yageo | AC0603FR-075K1L | 5.1 kohm, 1%, 0603; two separate USB CC pull-downs |
| 15 | 10 | Samsung Electro-Mechanics | CL10B105MO8NNWC | 1 uF, 16 V, X7R, +/-20%, 0603; reset network |
| 16 | 10 | Yageo | RC0603JR-0722RL | 22 ohm, 5%, 0603; USB series resistors |
| 17 | 10 | NIC Components | NRC06J100TRF | 10 ohm, 5%, 0603; MPM output-sense connection |
| 18 | 5 | Toshiba | 74HC4051D | One fitted 8:1 analog mux; manufacturer identified from purchasing SKU |
| 19 | 5 | PANJIT | SMF5.0A_R1_00001 | Unidirectional 5 V TVS, SOD-123FL; cathode to protected supply |
| 20 | 5 | SLKOR | USBLC6-2SC6 | Exact purchased USB ESD array; use SLKOR documentation, not ST interchangeability assumptions |
| 21 | 5 | Samsung Electro-Mechanics | CL21A226MPCLRNC | 22 uF, 10 V, X5R, +/-20%, 0805; DC-bias curve archived |
| 22 | 30 | Samsung Electro-Mechanics | CL10B104KO8NNNC | 100 nF, 16 V, X7R, +/-10%, 0603; local bypass and ADC filter |
| 23 | 10 | Yageo | RC0603JR-0710KL | 10 kohm, 5%, 0603; reset/boot pull-ups |
| 24 | 10 | Yageo | RC0603FR-074K7L | 4.7 kohm, 1%, 0603; seven proposed pot high-end resistors |
| 25 | 1 | Adafruit | 6250 | PCM510x I2S DAC breakout; one analog channel supplies mono RCA |
| 26 | 1 | Adafruit | 5393 | Already the correct 320 x 172 display in this CSV; no purchasing-row replacement needed |

The current CSV contains no Product 5394. Earlier software hardware dimensions
were corrected to 320 x 172. No substitute MCU, regulator, mux, ESD manufacturer
or connector MPN has been introduced.

USB4085 Rev B explicitly shows 16 circular signal holes and four shell slots.
The CSV text `16+8POS` must not be translated into eight PCB shell stakes.
The footprint has 20 copper lands; four share the SH symbol pin, giving
17 logical schematic pins. The meaning of the supplier's `+8` was not inferred.

The existing MPM circuit uses purchased passive MPNs. A USB slew-control solution
may require an additional part after input-current and attach-charge analysis;
none is silently included. Header hardware, antenna/pigtail, keycaps and final
enclosure fasteners do not have purchasing MPNs in this input. They remain open
assembly items, not invented BOM entries.

Authoritative electrical and mechanical sources are indexed in
[`SOURCES-V2.md`](../SOURCES-V2.md). Mechanical acceptance limitations are in
[`MECHANICAL-V2.md`](../MECHANICAL-V2.md).
