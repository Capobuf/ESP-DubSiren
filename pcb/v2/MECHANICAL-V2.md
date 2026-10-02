# V2 mechanical evidence and unresolved dimensions

Status: incomplete, 2026-10-02. No complete enclosure/assembly or manufacturing
template exists. No board size, mounting centers, panel stack or interference
clearance is presented as final.

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

## Mechanical acceptance blockers

| Exact part | Evidence available | Unresolved fact / required evidence |
|---|---|---|
| PTV111-3420A-B203 | Current Bourns PTV/PTT p2 and p5: four physical pins, pin 4 dummy; 2.5 mm pitch; 13.6 mm tab span; 11 mm row separation | Recommended PCB drawing does not explicitly label its viewing side; lateral pin-row datum relative to shaft/tab midpoint is not dimensioned. Need an authoritative exact-variant CAD/drawing or manufacturer clarification |
| PTV111-1420A-B103 | Same document: four pins, 2.5 mm pitch; 10.2 mm tab span; 3.3 mm row separation | Same view/datum issue; do not reuse a three-pad V1 potentiometer footprint |
| PEC11R-4120F-S0018 | Bourns PEC11R p2: encoder A/C/B and two switch terminals, hole/tab pattern | Recommended PCB pattern has no explicit view label; correspondence to frontal terminal view remains unaccepted |
| MX2A-11NW | Current CHERRY datasheet and older CHERRY sheet identify exact PCB-mount variant | Neither retrieved sheet contains a dimensioned hole/plate pattern. A legacy MX1A library name is insufficient proof of exact compatibility |
| CL170805B on MASTER | Cliff K85M drawing: 6 mm D bore with 5 mm across-flat dimension | Bourns pot flat is 4.5 mm. Secure torque transfer/retention with this combination is not proven |

Old Bourns material found during research differs from current tab-span values;
it must not override the current exact-series drawing. Generic web descriptions
of "top view" are not an explicit annotation on the manufacturer drawing.

The library workflow requires: "Refuse to use the part in a real schematic"
while a view ambiguity remains unresolved (`C:/Users/dot/.agents/skills/kicad-library/SKILL.md`).
This implements the request's prohibition on assumed pinouts and dimensions.
These controls are therefore held out of the real schematic/board, rather than
given unverified production footprints.

## Knobs and control topology

Provisional assignment is SAK-023 on encoder and CL170805B on MASTER. SAK-023
has a 6 x 4.5 mm D bore and 14.5 mm maximum base diameter. Cliff K85M has a
14 mm diameter and 17.8 mm height; its retention issue above remains open.
Davies 1100 uses a 6.35 mm bore and set screw on a nominal 6 mm pot shaft.
The manufacturer's fractional catalog dimensions give about 19.84 mm maximum
diameter and 11.91 mm overall height. Bore clearance and knob concentricity
require a physical fit check. No keycap or antenna purchase MPN was invented.

The required arrangement remains three pots above a central display, encoder
immediately right, three pots below, and four MX controls on the bottom row.
MASTER, USB and the complete dual RCA block belong on the rear. RESET/BOOT are
service controls. No arrangement has yet passed ergonomic and vertical-stack
checks; an arbitrary outline or enclosure would not resolve the missing facts.

## Unperformed assembly work

The historical Hammond STEP remains a candidate. It has not been retained or
rejected because an actual verified control layout fit test has not been run.
PCB size/layers, origin, mounting holes, bosses, screws, Z stack, panel cut-outs,
plug insertion envelopes and antenna cable clearance remain unselected.
No FreeCAD enclosure, complete assembly STEP, 1:1 panel DXF/SVG/PDF or decorative
overlay/bezel has been generated. Only the RCA component STEP is complete.

After footprint acceptance, these tasks remain: compare ergonomic layouts;
choose dimensions; check Hammond fit numerically; create any needed parametric
enclosure; import the placed PCB STEP; model fastening and display retention;
measure all requested CAD clearances; derive panel drawings from that same CAD.
Physical prototype checks then cover shafts/knobs, keycaps, display pressure,
USB/RCA mating, solder clearance, enclosure closure and antenna routing.
