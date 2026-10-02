# ESP-DubSiren V2 mechanical assembly

The saved assembly is `mechanical/assembly-v2.FCStd` and `assembly-v2.step`.
The adopted enclosure is **125 x 140 x 42 mm**, with an independently removable
base, top and rear panel. These replace the earlier feasibility study.
Final board release still depends on PCB DRC cleanup; these are prototype parts.

## Datums and placement

All coordinates are mm. X increases right, Y toward the front/button row, and
Z upward. The enclosure rear outside plane is Y=0 and base underside is Z=0.
The PCB outline is (6,3.5)..(119,134): **113 x 130.5 x 1.6 mm**.
PCB underside Z=30.75; controls face Z=32.35.

| Feature | Coordinates / dimensions |
|---|---|
| Front pots RV1..RV6 | X=27.5 /62.5 /97.5; Y=26 /94 |
| TFT visible center / carrier center | (62.5,60) / (63.516,60) |
| Encoder | (97.5,60) |
| Key centers BTN1..BTN4 | X=27.5 /50.8 /74.2 /97.5; Y=122 |
| Main PCB support centers | (12,12), (113,12), (12,128), (113,128) |
| Rear MASTER shaft axis | X=23, Z=20.75; shaft points toward -Y |
| USB opening center | X=45, Z=29.05; opening12 x6 |
| RCA axes | X=85.5 /99.5, Z=22.75; openings diameter11.2 |
| Top/base closure screw centers | (5,5), (120,5), (5,135), (120,135) |
| Rear panel screw axes | X=5 /120, Z=22; axis parallel Y |

## Control geometry and acceptance

RV1..RV6 use `Bourns_PTV111-3`, RV7 uses `Bourns_PTV111-1`, and SW7 uses
`Bourns_PEC11R-4120F-S0018` in the project-local mechanical library.
Bourns PTV/PTT Rev09/23 pages2,5 and PEC11R Rev04/26 pages2,3 supply the geometry.
The PTV111-3 PCB seat-to-bushing base is7.5mm; the 2mm stake width is not an
axial stand-off. Front pot bushing length5mm and shaft length20mm are separate.
The rear PTV111-1 shaft axis is10mm below the B-side PCB and its body-side
bushing datum is Y=2. RV7 rotation0 places the shaft toward the rear.

SW1..SW4 (BTN1..BTN4) use native
`Button_Switch_Keyboard:SW_Cherry_MX_1.00u_PCB`, unchanged.
Validation is MX2A-11NW -> official CHERRY replacement for MX1A-11NW -> unchanged
form/fit/function -> legacy dimensioned MX PCB layout -> native KiCad footprint.
See `SOURCES-V2.md` and `reports/CHERRY-NATIVE-FOOTPRINT-AUDIT.md`.
Nominal3.3mm below-body lead projection gives1.7mm below a1.6mm PCB; five-hole
clearance envelopes include this space. They are not invented detailed post CAD.
Dry-fit one purchased MX2A-11NW in the fabricated PCB before hardware validation.

The Davies1100 envelope is19.84375mm diameter x11.90625mm high, with6.35mm bore.
Nominal insertion9mm onto each6mm shaft is adjustable. MASTER uses SAK-023,
maximum14.5mm diameter and15.1mm total height including its1.1mm boss, with8.2mm
nominal shaft insertion. The encoder uses CLIFF CL170805B,14mm diameter x17.8mm
high,12.5mm nominal insertion and a0.8mm push-travel allowance. Actual D-flat
retention and Davies set-screw clamping remain physical checks.
Keycaps have no invented MPN: four19.05mm-square x8mm-high1u envelopes with4mm
travel fit20.05mm-square panel openings. No14mm plate-latch assumption is used.

## Enclosure, mounting and shaft stacks

Body walls, base and top are nominal3mm. Top control underside pockets extend
to Z40.5, leaving1.5mm locally. Pot spotfaces end at Z41.5, leaving1mm local
panel thickness. The top splits from the frame at Z39. The rear panel is
**125 x36 x1.5mm aluminium**, Z3..39, outside faceY0, insideY1.5.
Two rear M2x8 screws engage6.5mm-deep tapped bosses. The frame has an open rear
and PCB entry reliefs X5.5..8 and117..119.5, Y1.5..3, Z30.5..33.

Four OD8 motherboard bosses terminate at Z29.75. OD8/ID3.2 x1mm insulating
shims support the PCB at Z30.75. Main screws are M3x8 with0.5mm washers and
2.5mm tapped pilots. Four bottom M3x8 low-profile screws secure the base with
diameter6 x2mm recesses; use heads no more than2mm high. Four top M3x8 screws
with0.5mm washers engage4.5mm in5mm upper bosses (Z34..39). Verify allowable
torque/pullout in the selected prototype frame material; screw envelopes do
not model thread helices. Metal inserts require a separately revised boss CAD.

| Stack | Nominal result |
|---|---|
| Front pot bushing base / thread tip / shaft tip | Z39.85 /44.85 /59.85 |
| Front panel spotface / washer / nut | Z41.5;0.5mm washer;2mm nut |
| Front pot thread remaining beyond nut | 0.85mm |
| Encoder bushing base / thread tip / shaft tip | Z38.85 /45.85 /58.85 |
| Encoder washer / nut above Z42 | 0.5 /2mm;1.35mm thread surplus |
| MASTER thread tip | Y=-3;0.5mm washer and2mm nut leave0.5mm surplus |
| Reaction collars | Front pots0.65mm; encoder1.65mm; MASTER0.5mm |

Fit reaction collars before tightening nuts so hardware does not bend the PCB.
The defined M9 fallback hardware is Alps RKZ00023UA (M9x0.75,11AF x2mm nut;
9.1ID/14OD x0.5mm washer). Bourns supplied hardware may substitute only after
measurement. Target at least0.10mm thread surplus after measured tolerances.

## Display and DAC stacks

Adafruit5393 is fixed. Its exact Eagle outline is36.83 x30.48mm with2.54mm
corner radii, four2.5mm mounting holes at(+/-15.875,+/-12.7), and11 header pins.
Visible TFT area32.35 x17.4mm has3mm corner radii and center offset-1.016mm inX.
The physical TFT/FPC outline is bounded by36.3 x19.4mm; module whole thickness
is5.4mm. The CAD conservatively allocates up to3.8mm on either PCB face where
the exact height split is unavailable; this is not a claimed real double height.

Four4.5mm OD3.5/ID2.2 spacers place display PCB underside at Z36.85 and top at
38.45. Four M2x12 screws, nuts and washers retain it. Lower washers are custom
OD4 (not OD5), clearing the first/last header solder envelopes. The soldered
header is Samtec TSW-111-07-T-S: post5.84, tail2.54, plastic2.54mm.
The48 x41 x3mm bezel supports a41 x34 x1mm clear cover at Z44..45, above a
38.3 x31.5mm top opening. Four bezel M2x10 screws are offset(+/-22,+/-18.5)
from the carrier center. Two30 x0.5mm soft foam strips bear only outside the
active area. Nominal2mm foam compressed to1.75mm is an adjustable allocation:
measure the actual glass-to-cover gap and select soft foam/shims without hard
preload. Minimum modeled active-area margin is0.20mm; inspect placement.

Adafruit6250 is30.48 x20.32mm,2.54mm corner radii, with four2.5mm holes at
(+/-12.7,+/-7.62). It faces downward,8mm below the motherboard, with PCB faces
Z22.75 /21.15 and conservative component bound endingZ16.55. Only the upper two
mounts use8mm spacers and M2x16 bolts. Two lower OD4 base pillars with diameter2
locating pegs support the lower holes, avoiding screw heads under the display
and encoder. Fit Samtec TSW-108-14-T-S (post8.13, tail2.79, plastic2.54mm) on
pins1..8. Pins9..14 are unpopulated/unsoldered configuration holes.

## Assembly sequence and clearances

1. Solder components and module headers. Assemble the display/DAC spacers,
   bolts and nuts; leave knobs, control nuts, the top and rear panel off.
2. With the frame open at the top/rear and base removed, slide the PCB/module
   assembly horizontally through the rear at its final Z30.75..32.35 elevation.
   Use the114mm-wide entry reliefs; do not lower the frame over the PCB.
3. Raise the base from below, engage the DAC locating pegs and support shims,
   and fasten the main PCB from the open top. Install the bottom closure screws.
4. Slide the rear panel axially over the MASTER shaft, USB and RCA. Install its
   two screws, then the MASTER collar/washer/nut and SAK knob.
5. Lower the top over the front shafts, TFT and MX stems. Fit the reaction
   collars, control washers/nuts, top screws, cover/bezel, front knobs and keys.

`reports/MECHANICAL-CLEARANCE-AUDIT.md` records the independent numerical audit,
including installation motion. `assembly-collision-raw.json` retains every
cross-group solid intersection: screw/pilot overlap represents intended thread
engagement. The original RCA STEP intersects the exact1.6mm PCB by0.517991mm3,
mainly its formed ground leads. Official hole dimensions are retained: shifting
the part cannot remove this without misaligning signal pins. This free-state
lead deformation requires a real insertion check; see the dedicated RCA audit.
It is not a footprint-coordinate ambiguity or a claim of zero rigid collision.

| Critical nominal clearance | mm |
|---|---:|
| Adjacent Davies knobs | 15.156 |
| Keycap travel to each panel opening, per side | 0.50 |
| Encoder swept knob to top / bezel | 3.55 /3.034 |
| Display conservative underside to PCB top | 0.70 |
| TFT conservative glass bound to clear cover | 1.75 |
| RCA plug allowances to each other | 1.00 |
| Upper frame boss to PCB top | 1.65 |
| Antenna access to nearest non-target display nut | 1.221 |
| Pigtail routing corridor to nearest screw | 3.019 |
| Service probe to access hole, radial | 0.50 |

USB plug allowance is14 x8mm with28mm rear access; RCA plugs are diameter13
with30mm rear access. No actual plug MPN is implied. U.FL access centers on
(42,56.14) with a12 x12mm access volume and a routed cable corridor toward the
front; minimum bend allowance5mm. The antenna/pigtail purchase remains open.
Trim metal leads/solder to at most1.5mm beyond the opposite PCB face where
specified; CHERRY locating-feature reserve remains1.7mm and plastic is not trimmed.

## Files and reproduction

`sources/build_full_assembly.py` imports the KiCad PCB STEP, builds verified
component envelopes and native FreeCAD CSG enclosure features. The enclosure
parameter object and cutting primitives are editable. Component datums remain
fixed; rerun all validation and templates after changes. `check_full_assembly.py`
checks the saved solids. Open the saved FCStd or STEP assembly for 3D inspection.
The historical Hammond/enclosure-study files are retained only as evidence.

`export_panel_templates.py` projects the SAME native CAD cutting primitives.
`panel-top-v2`, `panel-rear-v2`, `panel-base-v2`, and `panel-overlay-v2` each have
DXF, SVG and PDF outputs. DXF units aremm; top/base use negative planY and rear
uses Z-42. PDFs/SVGs use A4 at1:1. Print100%/Actual Size and verify the100mm bar.
Black lines are through cuts; blue dashed features are pockets/spotfaces;
grey circles are internal hardware references, not extra top drill holes.
Rear purple outline is the125 x36mm removable plate inside the125 x42mm overall
rear reference. Overlay includes visible TFT, knobs, keycaps, screws and5mm
safe margins. `panel-template-validation.json` records unit/scale checks.
Separate STEP parts exist for frame, base, top, rear panel and bezel.

Prototype checks remain purchased switch dry-fit, RCA formed-lead insertion,
shaft/hardware tolerances, knob/keycap retention/travel, display pressure,
antenna/pigtail and real plug fit, thread strength, solder trimming, machining
accuracy and practical assembly handling. These do not replace the saved CAD
or justify an unrouted PCB.

## Hammond feasibility measured in FreeCAD

`sources/check_hammond_stack.py` imports the original Hammond STEP and aligned
manufacturer RCA STEP. It saves `mechanical/hammond-stack-feasibility.FCStd` and
`reports/hammond-stack-feasibility.json`. The six Hammond solids are valid.
Use `optimalBoundingBox()` for this STEP: ordinary bounds include untrimmed
surface extents and give incorrect dimensions.

Exterior: 120.80 x 95.50 x 33.80 mm. Two large internal horizontal CAD faces
establish **29.70 mm** available height, consistent with the Hammond drawing.

| Necessary opposite-face stack | mm |
|---|---:|
| RCA body, measured from STEP | 20.50 |
| PCB | 1.60 |
| Pot body only, excluding stand-off | 7.50 |
| Chosen clearance at each enclosure face | 0.50 + 0.50 |
| Required lower bound | **30.60** |
| Available | **29.70** |
| Deficit | **0.90** |

At 0.50 mm RCA-to-lid clearance, the body-only pot depth gauge penetrates the
inner top plane by 0.40 mm. Adding the upper clearance gives the full deficit.
This lower-bound check excludes stand-off, solder, tolerances and display hardware.
An RCA on the controls face cannot clear its 20.50 mm body while engaging the
pots' 5 mm bushings. XY rotation cannot solve this vertical conflict.

The saved portrait trial has the six Davies envelopes, exact 5393 PCB envelope,
right encoder envelope and four 19.05 mm keycap allowances. Edge gaps are
5.91 mm knob-to-knob, 4.64 mm lower-knob-to-display-board, 6.34 mm
encoder-to-display-board and 4.65 mm lower-knob-to-keycap row. Landscape needs
109.22 mm row height with explicit 4 mm gaps/end margins, exceeding 95.50 mm.
These are feasibility trials, not a full collision audit. The 100 x 74 mm depth
gauge is not an adopted V2 PCB outline.
