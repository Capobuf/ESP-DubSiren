# ESP-DubSiren PCB V2: saved implementation and evidence

Status: **INCOMPLETE; not ready for fabrication**, 2026-10-02. Seven pots and
the encoder are now populated, the original 14 ERC warnings are resolved, and
a controlled USB input ramp is implemented. Four CHERRY switches still lack
the exact manufacturer drill-pattern evidence required by the task. The main
PCB remains empty; no routed-board, complete assembly or fabrication acceptance
is claimed. ERC alone cannot detect the missing intended switches.

## Repository preservation

Branch: `pcb-v2-redesign`. Starting repository HEAD was
`036959192e2c5176d95e0679953c89dac3a07640`. The previously saved work was preserved
before continuation in `a4303b1` (`pcb-v2: checkpoint schematic and 5393 migration`).
The Hammond test and replacement study were committed as `4f13d12`
(`pcb-v2: quantify Hammond depth conflict and save enclosure study`). Controls,
their footprints and the input slew stage are saved in `a9136fa`
(`pcb-v2: add validated Bourns controls and USB input slew stage`). The independent
review/source report checkpoint is listed by `git log` on this branch.
V1 is unchanged. Purchasing quantities from `2026-10-02T050640.csv` are not
substituted for fitted quantities.

## Schematic and footprints

The saved schematic BOM export has **60 instances: 53 purchased components and
7 bare PCB test pads**. It includes one ESP32-S3-WROOM-1U-N16R8, one 74HC4051D,
one MPM3612GLQ-33-Z, Adafruit 5393 and 6250, the complete RCJ-2123 block, seven
pots, encoder, service switches, protection, passives and the input slew switch.
This is not the final complete-unit fitted count. The four MX switches and
mechanical assembly parts must not be inferred present from this export.

| Control | Exact footprint | Source and result |
|---|---|---|
| RV1-RV6, PTV111-3420A-B203 | `ESP-DubSiren-V2-Mechanical:Bourns_PTV111-3` | Bourns PTV/PTT Rev 09/23 pp2,5; current 13.6 mm stake pitch |
| RV7, PTV111-1420A-B103 | `ESP-DubSiren-V2-Mechanical:Bourns_PTV111-1` | Same source; separate 10.2 mm stake pitch / 3.3 mm row pattern |
| SW7, PEC11R-4120F-S0018 | `ESP-DubSiren-V2-Mechanical:Bourns_PEC11R-4120F-S0018` | Exact Bourns push-switch and A/C/B pattern |
| SW1-SW4, MX2A-11NW | Not accepted or instantiated | Native `Button_Switch_Keyboard:SW_Cherry_MX_1.00u_PCB` inspected, but current CHERRY drilling drawing remains unavailable |

`CONTROLS-POWER-PHYSICAL-PIN-MAPS.md` records every physical lead, symbol pin,
footprint pad and coordinate. The Bourns dummy fourth lead has an isolated
unnumbered land. Mechanical stake apertures are closed routed Edge.Cuts with
0.4 mm internal radius, not electrical pins. They must survive board outline
export. The router radius is an explicit manufacturing choice: stake-corner fit
and finished-route tolerance still need acceptance. Library readback, disposable live placement and rendered inspection
were performed. These checks establish component acceptance, not final DRC.

Pot pin 1 is GND, pin 2 the wiper/mux input and pin 3 the respective 4.7 kohm-fed
top terminal, giving increasing voltage clockwise. Mux channel 7 is grounded;
ADC1 GPIO4 has the common 100 nF filter. Settling and the firmware acquisition
sequence remain documented implementation/measurement work. No external ADC or
I/O expander was added. Encoder GPIO1/2/42 and USB GPIO19 D-/GPIO20 D+ are retained.

The MPM circuit retains its integrated inductor, C1/C3 = 22 uF, C2/C4 = 100 nF,
and 10 ohm VOUT-to-VOUT_S connection. EN is tied to VIN; unused PG is NC, optional
VCC bypass omitted. No external power inductor, MOSFET, feedback divider,
Schottky diode or replacement regulator was added. Its NC copper region still
needs an enforced board keepout when layout is undertaken.

Display VIN remains 3.3 V to avoid exposing MCU pins to the breakout's VIN
pull-ups. DAC VIN is switched 5 V. DAC Lout feeds one RCA center; the second
center is NC and both barrels are grounded. The outputs are not summed.

## CHERRY source gap

`MX2A-SOURCE-SEARCH.md` records the current exact 16-page manufacturer datasheet,
older MX2A paper, distributor redirects, catalogs, compatibility searches and
the user-supplied `sszczep/kicad-libraries` revision. The latter traces to an older
MX1A drawing. Its pin coordinates match native KiCad after shifting origins,
but fixation drills are 1.75 mm versus native 1.70 mm. The current CHERRY sheet
gives a 15.63 mm body; native Fab/courtyard geometry is smaller.

Missing exact evidence: terminal and fixation-post X/Y coordinates relative to
the center axis, finished-hole sizes/tolerances, center-post hole, viewing
direction and seating datum for MX2A-11NW. A manufacturer mounting-equivalence
statement tied to the older dimensioned drawing would also close the gap.
No arbitrary custom footprint or silent generic substitution was made.

The user's sections 7 and 37 permit an intermediate stop only for this genuine
manufacturer-dimension gap after official-document searches. Bourns orientation,
knob bore fit and unselected antenna/keycap MPNs are not used as stopping reasons.

## USB input and current decision

See `USB-POWER-DECISION.md` for source equations, MLCC bias, alternatives,
startup charge, tolerance limits and qualification requirements.

The passive small-CIN option was evaluated first: 4.7 uF nominal with a
hypothetical 1.36 uF effective corner gives about 73 mV ideal ripple at 0.5 A
output, 5 V input and 1.125 MHz. Thus 22 uF is not an absolute MPS minimum.
However, the purchased DAC itself has 10 uF directly on VIN. With a 4.7 uF buck
capacitor and 100 nF bypass the nominal direct total would be 14.8 uF, or
17.76 uF at +20%, before parasitics. Reducing only buck CIN cannot close attach
capacitance. DAC operation from 3.3 V was evaluated but not accepted at the
conservative AP2127 dropout / PCM5102 minimum-supply corner.

Implemented path: USB VBUS -> PTC -> **TPS22918DBVR** -> buck and DAC.
The PTC is overcurrent protection, not inrush control. The slew switch is not
a current limiter or reverse-blocking device.

| Quantity | Saved design / calculated value |
|---|---|
| Buck CIN | Purchased 22 uF + 100 nF, behind slew switch |
| 22 uF typical effective capacitance at 5 V | 6.957 uF from exact Samsung bias curve |
| C10 directly visible at input | 1 uF CL10B105MO8NNWC |
| Estimated attach-capacitance ceiling | 1.40 uF including positive tolerance/temperature and 0.02 uF parasitic allowance; allowance is not measured |
| C11 ramp capacitor | 10 nF C0G 50 V, GRM1885C1H103JA01D |
| Ramp at 5 V | 26.55 ms typical table value; no guaranteed minimum rise time supplied |
| Direct downstream 5 V bulk | 32.1 uF nominal; about 4.84 mA nominal charging contribution |
| Steady 3.3 V / DAC allocations | 205 mA / 30 mA |
| Upper operating 3.3 V / DAC allocations | 485 mA / 50 mA |
| Estimated steady VBUS at 5 V, 85% efficiency | 189 mA |
| Upper operating input corner | 487 mA at 4.5 V receptacle, 0.15 V path loss, 3.333 V rail and 85% efficiency |
| Margin to configured USB2 500 mA | 13 mA, about 2.6%; limited |

The upper operating allocation is **not a guaranteed unrestricted worst-case
current**. It includes 400 mA for MCU/memory, 80 mA for display/backlight and
5 mA for other 3.3 V loads. Display maximum and simultaneous activity are not
fully bounded by the retrieved specifications. A 500 mA MCU transient would
raise the modeled USB input to about 577 mA. Firmware currently enables no RF;
future RF operation needs a tested current policy. No CC controller or PD was
added based on an assumed high-current source.

The 500 mA budget applies after USB configuration; pre-configuration and suspend
limits require firmware policy. The current firmware has not implemented or
verified that full policy. Regulator startup and downstream rail capacitances
must be included in attach-waveform testing. A nominal slew calculation does
not establish USB compliance. New purchase MPNs are TPS22918DBVR and the 10 nF
Murata CT capacitor; the 1 uF input capacitor reuses the purchasing BOM.

## Validation results and limits

| Check | Result |
|---|---|
| Saved schematic ERC | **0 errors, 0 warnings**; original 14 warnings resolved by wiring the pots |
| Direct short / orphan checks | 0 candidates in the saved partial schematic |
| Independent ERC | 0 errors, 0 warnings; see independent review evidence |
| Main-board DRC | Empty-board check: 1 invalid-outline error and 60 missing-footprint parity warnings; no populated-board acceptance |
| Whole-design unconnected count | Not established; not zero |
| Final USB 90 ohm geometry | Not selected; requires actual fabricator stackup |
| Layer decision | Two copper layers / 1.6 mm remain defaults; no claim of routing feasibility |
| Complete mechanical collision check | Not performed; feasibility studies only |

No global ERC or DRC suppression was introduced. `INDEPENDENT-V2-REVIEW.md`
records the fresh audit and any outstanding findings. It is not a fabrication
approval. Existing display software validation from the checkpoint remains:
PlatformIO build passed, native parser/DSP passed, desktop/VirtualESP 36 tests
passed in an isolated source copy. These were not rerun for hardware-only
changes and do not validate physical controls, USB power or audio hardware.

## Mechanics and deliverables

`../MECHANICAL-V2.md` contains exact mechanical sources and reproduction scripts.
The original Hammond STEP was imported into FreeCAD. Its **29.70 mm** cavity
cannot accommodate the necessary **30.60 mm** opposite-face RCA / PCB / pot-body
stack with 0.5 mm clearance at each enclosure face. This already excludes pot
stand-off, solder and display hardware. Hammond 29830PSLA is rejected for the
single planar board; the shortfall is at least **0.90 mm**.

The portrait control-envelope trial retains the required topology, with gaps of
5.91 mm between adjacent knobs, 4.64 mm from lower knob to display-board envelope,
6.34 mm from encoder knob to display board and 4.65 mm from lower knobs to keycaps.
These are XY feasibility figures, not all-component assembly clearances.

| Requested result | Actual saved status |
|---|---|
| Final PCB size / placement / routing | Not established; main PCB empty |
| Replacement enclosure | Editable **125 x 140 x 42 mm study** with body, base and bored bosses |
| Study PCB | 113 x 128 x 1.6 mm at (6,6), lower Z 29 mm; not applied to KiCad |
| Study support centers | (12,12), (113,12), (12,128), (113,128) mm; 8 mm bosses, 2.5 mm pilots |
| Study front pots | X 27.5 / 62.5 / 97.5; Y 26 / 94 mm |
| Study display / encoder centers | (62.5,60) / (97.5,60) mm |
| Study keycap centers | X 27.5 / 50.8 / 74.2 / 97.5; Y 122 mm |
| Full PCB STEP and 3D assembly | Not generated; component STEP plus feasibility FCStd documents exist |
| Body/base geometry check | Valid solids; zero body/base intersection in saved study |
| Top/rear 1:1 DXF/SVG/PDF templates | Not generated |
| Overlay / bezel | Not generated; final panel and mounting geometry pending |
| Schematic PDF | Intermediate controls/power PDF generated; not final fabrication schematic |
| BOM-V2 | One-unit selection/status BOM; actual schematic export retained separately |
| Gerbers / Excellon / positions / fabrication ZIP | Not generated; validation gates not satisfied |

Generated evidence includes the physical pin-map report, library and scratch
readbacks/renders, current schematic PNG/PDF/netlist/BOM, power analysis and
source PDFs, CHERRY comparison, Hammond FCStd/JSON and replacement FCStd/JSON
with SVG/PNG illustration. The preview SVG is not a machining template.

Physical prototype checks remain Davies 1100 clamping/concentricity on 6.0 mm
Bourns shafts, shaft/nut/washer stacks, keycaps and travel, display retention
pressure, antenna/pigtail access and routing, real USB/RCA plugs, enclosure
tolerances, USB signal quality, power startup/RF peaks, mux settling and audio
noise. They do not replace the unfinished schematic, PCB and assembly work.
