# V2 mechanical evidence and enclosure studies

Status: incomplete, 2026-10-02. The Hammond has now been checked in FreeCAD and
rejected for the direct single planar PCB architecture. A parametric replacement
enclosure study exists, but no complete assembly or machining template is accepted.
Study dimensions below are not final PCB or panel dimensions.

## Accepted module and connector geometry

The Adafruit carrier footprints use the exact official Eagle board data, not
product photographs or rounded summary dimensions. Their physical lead maps
are in `reports/ADAFRUIT-PHYSICAL-LEAD-MAPS.md`.

| Item | Source-derived geometry | Current evidence |
|---|---|---|
| Adafruit 5393 | PCB 36.83 x 30.48 mm with 2.54 mm corner radii; four mounting centers at (+/-15.875, +/-12.7) mm about PCB center, 2.5 mm holes | Custom carrier footprint, 11 header leads; rendered and read back in scratch board |
| Adafruit 6250 | PCB 30.48 x 20.32 mm with 2.54 mm corner radii; four mounting centers at (+/-12.7, +/-7.62) mm, 2.5 mm holes | Custom carrier footprint, 14 documented header leads; rendered and read back |
| RCJ-2123 | Two sockets in one block, 14 mm port pitch; four electrical leads and two 2.5 x 4.2 mm mounting slots | Custom footprint plus original manufacturer STEP and aligned STEP |

The display-facing footprint uses Eagle's bottom view, so its header order is
reversed relative to the Eagle top view. TFT, microSD housing and EYESPI outlines
are preserved. The source also shows an inserted SD-card reference outline;
that is not a fitted part. The visible pixel area is offset from PCB center, so
a centered generic rectangular panel opening would require further verification.
The exact cover contact surfaces, compressed height and retention pressure are
not established by the PCB outline alone.

The DAC jack protrudes beyond the PCB edge and is included in its courtyard.
Its secondary six-pin configuration header is unpopulated. Header stack heights
and board-to-carrier spacing have not been selected.

The aligned RCA STEP has three valid solids. Its 27.4 mm width agrees with the
drawing's 27.5 +/-0.3 mm. Socket axes are 8 mm above the PCB and the body reaches
20.5 mm above it. `reports/RCA-PHYSICAL-LEAD-MAP.md` records the rigid transform
and validation; this is not an assembled enclosure interference check.

## Accepted control geometry

| Exact part | Evidence available | Unresolved fact / required evidence |
|---|---|---|
| PTV111-3420A-B203 | Current Bourns PTV/PTT p2 and p5: four physical pins, pin 4 dummy; 2.5 mm pitch; 13.6 mm tab span; 11 mm row separation | Resolved as PTV111-3; new project-local footprint and populated RV1-RV6 |
| PTV111-1420A-B103 | Same document: four pins, 2.5 mm pitch; 10.2 mm tab span; 3.3 mm row separation | Resolved as PTV111-1; separate footprint and populated RV7 |
| PEC11R-4120F-S0018 | Bourns PEC11R revision 04/26 pp2-3: encoder A/C/B and two switch terminals | New exact rear-pin/push-switch footprint and populated SW7 |
| MX2A-11NW | Official CHERRY 2025-10-24 MX1A replacement notice: unchanged form, fit and function; legacy dimensioned PCB layout | Resolved; native SW_Cherry_MX_1.00u_PCB accepted unchanged for SW1-SW4 |

Old Bourns material found during research differs from current tab-span values;
it must not override the current exact-series drawing. Generic web descriptions
of "top view" are not an explicit annotation on the manufacturer drawing.

The user's resolved Bourns interpretation is adopted. The previous Bourns
orientation blocker is closed. Mechanical apertures are now routed Edge.Cuts
because the installed MCP pad writer exposes only circular drills. Refer to the
control acceptance evidence for dimensions, milling radius and physical lead maps.
CHERRY equivalence closes the mounting evidence chain. See SOURCES-V2.md and
reports/CHERRY-NATIVE-FOOTPRINT-AUDIT.md for coordinates and tolerance comparison.
Dry-fit one purchased MX2A-11NW in the fabricated PCB as a prototype check.

## Knobs and control topology

The fixed assignment is SAK-023 on MASTER and CL170805B on the encoder. SAK-023
has a 6 x 4.5 mm D bore and 14.5 mm maximum base diameter. Cliff K85M has a
14 mm diameter and 17.8 mm height.
Davies 1100 uses a 6.35 mm bore and set screw on a nominal 6 mm pot shaft.
The manufacturer's fractional catalog dimensions give about 19.84 mm maximum
diameter and 11.91 mm overall height. Bore clearance and knob concentricity
require a physical fit check. No keycap or antenna purchase MPN was invented.

Verify Davies 1100 clamping/concentricity on 6.0 mm Bourns shaft. This is a
prototype check, not a footprint blocker.

The required arrangement remains three pots above a central display, encoder
immediately right, three pots below, and four MX controls on the bottom row.
MASTER, USB and the complete dual RCA block belong on the rear. RESET/BOOT are
service controls. No arrangement has yet passed ergonomic and vertical-stack
checks; an arbitrary outline or enclosure would not resolve the missing facts.

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

## Replacement enclosure study

`mechanical/enclosure-study-v2.FCStd`, generated by
`sources/build_enclosure_study.py`, has an editable parameter object, hollow body,
base and four bored support bosses. Body/base shapes are valid and have zero
interference. The saved-model projection `reports/enclosure-study-preview.png`
has been visually inspected.

| Study parameter | Value |
|---|---|
| Exterior | 125 x 140 x 42 mm |
| Walls / top / base | 3 / 2 / 3 mm |
| Trial board / lower Z | 113 x 128 x 1.6 mm, origin (6,6), lower Z = 29 mm |
| Support centers | (12,12), (113,12), (12,128), (113,128) mm |
| Boss / pilot diameters | 8 / 2.5 mm |
| Pot centers | X = 27.5, 62.5, 97.5; Y = 26 and 94 mm |
| Display board / encoder centers | (62.5,60) / (97.5,60) mm |
| Keycap centers | (27.5,122), (50.8,122), (74.2,122), (97.5,122) mm |

These are **study dimensions**, not final PCB dimensions. No unverified component
machining has been made. Final control Z stacks, rear openings, fastening, screw
access, display retention and all-component collision checks remain. The support
holes have not been applied to KiCad. No complete assembly STEP, final 1:1
DXF/SVG/PDF templates or bezel is claimed. The preview SVG is an illustration.

Final CAD must reserve U.FL access, cable bend space and antenna routing. Physical
prototype checks include shafts, knobs, keycaps, display pressure, USB/RCA mating,
solder clearance, closure and machining tolerances.
