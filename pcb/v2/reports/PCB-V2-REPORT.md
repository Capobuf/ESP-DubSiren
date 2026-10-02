# ESP-DubSiren PCB V2: saved implementation and evidence

Status: **checkpoint saved; routing stopped at the user's request**, 2026-10-02.
The user asked to close quickly and leave tracks/routing as they are. No further
routing cleanup or fabrication release is attempted. No Gerbers were generated.

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

The saved schematic BOM export has **64 instances: 57 purchased components and
7 bare PCB test pads**. It includes one ESP32-S3-WROOM-1U-N16R8, one 74HC4051D,
one MPM3612GLQ-33-Z, Adafruit 5393 and 6250, the complete RCJ-2123 block, seven
pots, encoder, four MX switches, service switches, protection, passives and the
input slew switch. Knobs, fasteners and enclosure parts are additional assembly
items; purchasing quantities are not fitted quantities.

| Control | Exact footprint | Source and result |
|---|---|---|
| RV1-RV6, PTV111-3420A-B203 | `ESP-DubSiren-V2-Mechanical:Bourns_PTV111-3` | Bourns PTV/PTT Rev 09/23 pp2,5; current 13.6 mm stake pitch |
| RV7, PTV111-1420A-B103 | `ESP-DubSiren-V2-Mechanical:Bourns_PTV111-1` | Same source; separate 10.2 mm stake pitch / 3.3 mm row pattern |
| SW7, PEC11R-4120F-S0018 | `ESP-DubSiren-V2-Mechanical:Bourns_PEC11R-4120F-S0018` | Exact Bourns push-switch and A/C/B pattern |
| SW1-SW4 / BTN1-BTN4, MX2A-11NW | `Button_Switch_Keyboard:SW_Cherry_MX_1.00u_PCB` | Native footprint unchanged; official CHERRY replacement equivalence plus dimensioned legacy PCB layout |

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

## CHERRY mounting validation — resolved

Validation chain: **MX2A-11NW -> official replacement for MX1A-11NW -> no
changes to form, fit or function -> legacy CHERRY MX1A PCB layout -> native
KiCad SW_Cherry_MX_1.00u_PCB**. The official [CHERRY notice dated 2025-10-24](https://www.mouser.com/PCN/CHERRY_MX1A_11NW_EOL.pdf)
states: “Optimized internal components. No changes to form, fit, or function.”
The legacy manufacturer “Keyswitch with fixation pins” drawing is retained in
`sources/Cherry-MX-Legacy-Layout.pdf` relative to the V2 directory.

The installed native footprint matches the manufacturer grid and hole bands:
1.50 mm electrical drills, 4.00 mm center NPTH and two 1.70 mm fixation NPTHs.
`CHERRY-NATIVE-FOOTPRINT-AUDIT.md` records all coordinates, origin conversion,
hole tolerances, pin mapping and rendered/readback acceptance. SW1-SW4 use this
footprint directly. No custom CHERRY footprint was created. Dedicated MX2A CAD
research is closed. The associated native MX mechanical model is acceptable by
the same form/fit equivalence; its installed path currently has no STEP file,
so the assembly must obtain that legacy model or use verified envelope solids.

Dry-fit one purchased MX2A-11NW into the fabricated PCB before considering the
hardware production-validated. This prototype check does not block routing.

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

## Final checkpoint results

| Check | Result |
|---|---|
| Fresh ERC | **0 errors / 0 warnings** |
| PCB | **113 x 130.5 x 1.6 mm, 2 layers** |
| Footprints / trace segments | 68 / 670 |
| Unconnected items | **0** |
| DRC including parity | **11 errors / 184 warnings** |
| Shorts / copper-clearance violations | **0 / 0** |
| Drill-spacing warnings | 5 findings caused by 3 redundant vias |
| Dangling via | 1 redundant I2S_LRCK via |
| Complete assembly | **184 valid fitted solids**, 33 clearance reserves |
| Enclosure | **125 x 140 x 42 mm**, removable top, rear and base |
| Templates | Top/rear/base/overlay DXF, SVG and PDF, 1:1 |
| Gerbers / Excellon / positions / fabrication ZIP | **Not generated** |

Authoritative checks are `pcb-checkpoint-drc.json`, `pcb-checkpoint-validation.json`
and `erc-checkpoint.json`. Routing remains as saved after the user's stop.
The redundant vias still requiring cleanup are VBUS_RAW at (42.9,9.6), VBUS_RAW
at (45,15.1), and I2S_LRCK at (71.8,41.3). The last one reports dangling after
the clock trace was connected directly to the DAC plated pad. Native cleanup
was not performed. Zero unconnected items is not fabrication acceptance.

The 11 errors comprise 9 courtyard/through-hole findings and 2 thermal-relief
findings. Opposite-face module stacks and mounting hardware were independently
checked in 3D. U6 lower holes have no front screw heads, and pads9..14 stay
unpopulated/unsoldered. U5 pin3 and J1 SH have explicit ground traces in addition
to the remaining thermal connections. No global DRC classes were suppressed.
Warnings comprise 64 library-footprint mismatches (physical pads independently
compared; importer graphics/metadata differ), 64 schematic-field mismatches,
4 expected extra mounting footprints, 46 silkscreen findings, 5 drill-spacing
findings and 1 dangling via. DRC release remains open.

The USB pair is 0.60 mm wide with 0.15 mm gap. The tested quasi-TEM grids estimate
86-90 ohms, without a demonstrated convergence or fabrication tolerance contract.
The main pair has continuous opposite-layer ground; local launch/escape crossings
are documented in `INDEPENDENT-USB-ROUTING-REVIEW.md`. Skew conventions give
0.919 or 2.519 mm, below TI's 3.81 mm USB2 guideline. The added second ground
return via is 0.825 mm from the DP transition. Actual signal quality remains a
prototype check. The 487 mA budget above is an allocation, not a guaranteed
worst case; the original 0.15 V path-loss assumption is slightly optimistic
against the independent resistance estimate. See the review for qualifications.

## Mechanical deliverables

Hammond29830PSLA is rejected: 29.70 mm cavity versus 30.60 mm minimum required
stack, a 0.90 mm deficit. The final PCB underside is Z30.75, top Z32.35, with
outline (6,3.5)..(119,134). Front pots are X27.5/62.5/97.5 at Y26/94; TFT active
center (62.5,60), carrier (63.516,60), encoder (97.5,60); keys are
X27.5/50.8/74.2/97.5 at Y122. Supports are (12,12),(113,12),(12,128),(113,128).

The saved complete assembly is `../mechanical/assembly-v2.FCStd` and `.step`.
Separate STEP files exist for the PCB, frame, base, top, rear panel and bezel.
`panel-top-v2`, `panel-rear-v2`, `panel-base-v2` and `panel-overlay-v2` each have
DXF/SVG/PDF outputs under `mechanical`. They derive from the same CAD cutting
tools, use millimetres at 1:1 and include a 100 mm print check.
The schematic PDF is `schematic-v2-complete.pdf`.

The numerical assembly check has 15 cross-group positive intersections:
14 intended screw/pilot engagements and 0.517991 mm3 at the original RCA model's
formed leads. Retain manufacturer holes and check real insertion. All 130 rear
insertion envelopes clear the actual frame reliefs; base insertion, panel motion
samples and driver access pass. Display nut1 needs a thin/open-ended 4 mm-AF tool.
Full dimensions, shaft/hardware stacks and clearances are in `../MECHANICAL-V2.md`
and `MECHANICAL-CLEARANCE-AUDIT.md`.

The one-unit BOM contains 57 electronic components, 7 PCB test pads and 8 knobs,
plus 30 assembly rows with 99 item/kit quantities. Added MPNs are Samtec
TSW-111-07-T-S and TSW-108-14-T-S for the module heights, and Alps RKZ00023UA as
dimensioned M9 hardware. Generic fasteners and custom parts have explicit
specifications. Keycaps and antenna/pigtail remain explicitly unselected.

Physical checks remain CHERRY dry-fit, RCA insertion, Davies clamping, shaft and
hardware tolerances, keycap travel, display pressure, antenna/pigtail and actual
plugs, thread strength, machining, solder trimming, USB/power measurements,
RF peaks, ADC settling and audio noise. The routing stop does not constitute
fabrication approval or production validation.
