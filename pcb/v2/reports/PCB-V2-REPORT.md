# ESP-DubSiren V2 implementation status

Status: **INCOMPLETE; not ready for fabrication**, 2026-10-02. The schematic
contains verified circuitry, but several controls are unaccepted, the main PCB
is blank, and no complete mechanical assembly exists. This is not a final
ERC/DRC or zero-unconnected report.

## Repository and input

Starting branch `main`, HEAD `036959192e2c5176d95e0679953c89dac3a07640`.
Working branch `pcb-v2-redesign`; no commit created. V1 source and historical
outputs are unchanged. The now-available root `2026-10-02T050640.csv` has been
reviewed completely; all 26 rows are in `PURCHASING-AUDIT.md`. It already names
Adafruit 5393. The original CSV is preserved and purchased quantities are not
used as fitted quantities.

## Electrical implementation

The new project and schematic are `../ESP-DubSiren-V2.kicad_pro` and
`../ESP-DubSiren-V2.kicad_sch`. The main PCB file exists but has no footprints,
outline, tracks or zones. Two copper layers and 1.6 mm thickness are project
defaults, not a validated fabrication stackup.

The interim schematic contains ESP32-S3-WROOM-1U-N16R8, one Toshiba 74HC4051D,
MPM3612GLQ-33-Z, purchased protection parts, USB4085-GF-A, two PTS636 service
switches, Adafruit 5393, Adafruit 6250 and the complete RCJ-2123 block.
`BOM-V2-INTERIM.csv` is exported from actual schematic instances: 42 purchased
component instances plus seven bare PCB test pads. It is not the complete-unit
BOM. Seven pots, encoder, four MX switches, knobs and mechanical hardware are
not included yet.

The MPM network actually drawn consists of C1/C3 (22 uF each), C2/C4 (100 nF
each) and R1 (10 ohm) from VOUT to VOUT_S. EN is tied to VIN; PG is NC and the
optional VCC capacitor omitted. There is no external inductor, switching
MOSFET, diode, feedback divider or replacement regulator. C5/C6 are additional
MCU local decoupling. The MPM NC metal region still needs an enforced PCB
copper keepout; a footprint drawing alone is insufficient.

One mux serves six front pots and MASTER, with its eighth input grounded.
Seven 4.7 kohm high-end resistors limit the intended pot outputs to the ADC
calibrated range; a common 100 nF filter is fitted. Settling uses an estimated
3.3 V mux resistance and needs prototype verification. The pots themselves
remain absent pending physical pin-map acceptance.

USB has two separate 5.1 kohm CC pull-downs, SLKOR ESD protection, two 22 ohm
series resistors, Bel PTC and unidirectional PANJIT TVS. Input current and USB
attach/inrush remain unresolved. The PTC does not provide controlled soft-start.

Display VIN is 3.3 V: the onboard CS/reset pull-ups use VIN, so 5 V VIN would
expose high-impedance MCU pins to 5 V. DAC VIN is protected 5 V with 3.3 V I2S.
DAC Lout connects to the red RCA center; the white center is NC and both barrels
are grounded. RCJ-2123 remains one physical dual-socket block. The two DAC
outputs are never shorted or summed.

The preliminary 3.3 V allocation is 500 mA MCU + 100 mA display + 5 mA logic =
605 mA, leaving 395 mA regulator capacity. Display current and the 60 mA DAC
allowance are provisional, not measured manufacturer maxima. At 4.5 V and 85%
efficiency the estimated input is 582 mA. This does not establish compatibility
with a 500 mA USB host. See `ELECTRICAL-DESIGN-EVIDENCE.md` for calculations,
DC-bias data, GPIO allocation and electrical limitations.

## Footprints, models and tools

Konnect 0.12.1 and KiCad 10.0.6 were used. Tool exposure was resolved using the
installed server's stdio MCP interface. Live scratch-board IPC was isolated
from the user's existing KiCad manager. FreeCAD 1.1 loaded, rigidly transformed
and validated the manufacturer's RCA STEP.

Custom footprint acceptance includes ESP32-S3-WROOM-1U, MPM3612, Bel PTC, exact
PTS636, both Adafruit carrier patterns and RCJ-2123. Library queries, disposable
placement, rendering and instance readback provide the evidence. Standard
footprints are documented in the physical pin maps. USB4085 has 16 signal lands
and four shared-SH slots. Its 0.70 mm lands add 0.025 mm annular width compared
with the manufacturer's recommended 0.65 mm lands.

RCA oval NPTH drills required native KiCad API fallback because MCP exposes
only scalar drill-diameter editing. The footprint was reopened and revalidated
through MCP/readback/rendering afterward. The original and aligned manufacturer
STEP are retained. Component acceptance is not full-board DRC or assembly fit.

## Electrical checks

Latest intermediate ERC: **0 errors, 14 warnings**. The warnings are isolated
labels at POT1_TOP through POT7_TOP, POT1 through POT6, and MASTER. Every warning
is retained in `erc-v2-progress.json`; none was globally suppressed. These are
unfinished connections, not final justified exceptions. Missing encoder/MX
parts cannot be inferred complete merely because ERC does not flag them.
Connectivity checks found no wire/component orphans or shorts in the current
partial schematic.

No populated V2 PCB DRC has been accepted. The main PCB is empty; a zero count
from it would be meaningless. Whole-design unconnected count is **not
established**, not zero.

## Display software and validation

Firmware now uses `tft_.init(172, 320)` and rotation 1 for 320 x 172 landscape,
as shown by the [official Adafruit initialization guide](https://learn.adafruit.com/adafruit-1-47-round-rectangle-tft-display/arduino-wiring-test).
The installed Adafruit library 1.11.0 computes the native 34 px column offset.
Shared dimensions, desktop preview, existing tests and current docs were
updated minimally. No new assets, animation or microSD functionality was added.
Physical GPIO/mux acquisition is not implemented by this display change.

| Check | Result |
|---|---|
| PlatformIO firmware esp32-s3-devkitc-1-n16r8 | Passed; RAM 119092 bytes, flash 340009 bytes |
| Native parser/DSP suite | Passed |
| Desktop and VirtualESP | 36 tests passed in isolated current-source copy |
| Tracked-change git diff --check | Passed |
| Physical hardware | Not tested |

The first desktop run had 32 passes and four DLL replacement setup errors while
a user app held `virtual_esp.dll`. Rebuilding in a fresh source copy passed all
36 tests; no user app was terminated. Software tests are not hardware checks.

## Mechanical and fabrication status

`../MECHANICAL-V2.md` records accepted geometry and exact blockers. The Bourns
pot PCB viewing direction and lateral datum remain unresolved; the PEC11R
pattern view is not explicit; retrieved CHERRY MX2A sheets lack a dimensioned
hole pattern. Cliff CL170805B's 5 mm flat does not establish secure fit on the
MASTER pot's 4.5 mm flat. Uncertain footprints have not been promoted to the
main PCB.

| Requested result | Actual status |
|---|---|
| PCB dimensions / origin | Not selected |
| Layer count | Two-layer default; routing feasibility not evaluated |
| Final fitted component count | Unknown; interim 42 purchased parts + 7 PCB test pads |
| Actual 74HC4051 count in schematic | 1 |
| Control arrangement | Required 3 pots / display + right encoder / 3 pots / 4 MX retained as requirement; no validated placement |
| Enclosure dimensions / mounting | Not designed |
| Hammond 29830PSLA | Neither retained nor rejected; fit test pending |
| Full PCB STEP / FreeCAD assembly | Not generated; RCA component STEP only |
| Complete interference / Z-stack check | Not performed |
| Top/rear 1:1 DXF/SVG/PDF | Not generated |
| Overlay / bezel | Pending verified panel and assembly geometry |
| Final unit BOM | Not generated; interim actual schematic BOM available |
| Gerbers / drills / fabrication ZIP | Not generated |

Remaining physical checks include shaft/knob retention, keycaps, nuts/washers,
display cover pressure, USB/RCA mating, enclosure closure, antenna/pigtail
routing, power transients, EMI, USB signal quality, mux latency/noise and audio
quality. These follow the outstanding schematic, layout and CAD work; they do
not substitute for completing it. Antenna and keycap MPNs were not invented.
