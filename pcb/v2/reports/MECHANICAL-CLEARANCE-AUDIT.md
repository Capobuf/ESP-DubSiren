# Independent mechanical clearance audit

Read-only review, 2026-10-02. The evidence is the saved
`mechanical/assembly-v2.FCStd`, its generated `assembly-cad-geometry.json`,
and native FreeCAD/OpenCascade Boolean intersections and minimum distances.
The machine-readable snapshot and SHA-256 are in
`independent-mechanical-reserve-check.json`. Construction history and cutting
tools are excluded from fitted parts. Clearance reserves are checked
separately; they are not presented as physical component models.

Final audited snapshot SHA-256:
`c7381048e5a0e4950cae89033da7456310f6331b37484a8c7ac7f78981a46e38`.
It contains **184 fitted solids and 33 reserves**. The independent check found
15 cross-group positive-volume intersections: 14 intended fastener/thread
engagements and the previously reviewed 0.517991 mm³ RCA/PCB intersection.
No additional unintended fitted collision was found. Own component/hardware
groups are not counted as unrelated intersections.

## Nominal clearances

All values are in mm. These are nominal modeled dimensions, not combined
manufacturing-tolerance guarantees.

| Interface | Clearance / dimension |
|---|---:|
| Adjacent Davies 1100 knobs in each row, XY edge gap | 15.156 |
| Front knobs to display bezel, smallest XY edge gap | 3.578 |
| Front knobs to bezel, minimum 3D distance | 6.858 |
| Encoder knob to bezel, XY edge gap | 2.984 |
| Encoder knob to bezel, including full push travel | 3.034 minimum 3D |
| Encoder knob to top panel, full 0.8 travel | 3.550 axial |
| Lower knobs to corresponding keycap envelopes | 8.553 |
| Adjacent keycap envelopes | 4.250 / 4.350 / 4.250 |
| Each complete key-travel envelope to opening | 0.500 per side |
| PCB top to ordinary top-panel underside | 6.650 |
| Pot body to pocket ceiling | 0.650, filled by reaction collar |
| Encoder bushing base to pocket ceiling | 1.650, filled by reaction collar |
| MASTER body to rear-panel inner face | 0.500, filled by reaction collar |
| Front pot spotface washer radial margin | 0.500 |
| Front / encoder / MASTER thread beyond modeled nut | 0.850 / 1.350 / 0.500 |
| Local front-pot panel thickness between pocket and spotface | 1.000 |
| Display PCB bottom to motherboard top | 4.500 |
| Conservative display underside bound to motherboard | 0.700 |
| Display glass bound to cover underside | 1.750 |
| Foam strip to active-screen reserve | 0.200 |
| DAC PCB upper face to motherboard lower face | 8.000 |
| DAC outward bound to flat base plane | 13.550 |
| DAC outward bound to its nearest base support relief | 0.700 |
| MASTER body to USB body | 11.530 |
| MASTER knob to USB external-plug reserve | 8.354 minimum 3D |
| Adjacent RCA external-plug reserves | 1.000 |
| Antenna mating-access reserve to nearest non-target hardware | 1.221, display nut 1 |
| Antenna cable reserve to nearest fastener | 3.019, display screw 2 |
| Service probe to each base access-hole wall | 0.500 radial |
| CHERRY lower-feature reserves to nearest outer base supports | 7.174 |

The display glass and underside solids each use a conservative side allocation
derived from the complete module height. They are deliberately conservative
clearance bounds, not a claim that the actual module has that height on both
sides simultaneously. Actual glass-to-cover gap determines foam selection and
compression; the nominal 1.75 mm compressed strips cannot guarantee pressure
on an unmeasured module. A 0.20 mm active-area margin requires controlled
placement. No active-screen overlap was found in the modeled reserve check.

## Solder and lower features

The first solder-reserve run found two actionable issues, both corrected by
the design owner and independently rechecked:

- U6 pads 9–14 are the unused lower auxiliary header row. Creating solder
  envelopes on that unpopulated row falsely occupied the display underside
  space. The saved assembly now reserves solder only for the eight installed
  header pins. The lower row must remain unpopulated and unsoldered.
- The first and last U5 header solder envelopes overlapped the original
  5 mm bottom washers at mounting positions 2 and 4. The four bottom washers
  are now custom insulating 4.0 mm OD, 2.2 mm ID, 0.5 mm thickness. The worst
  nominal edge clearance to the full radial solder envelope is about
  0.273 mm. No washer or nut intersection remains in the reserve check.

Other installed through-hole joints use a 1.5 mm exposed-height allocation,
with a radial boundary based on the copper pad. Trim applicable metal leads
and control solder height to that assembly instruction. Intersections between
these reserves and their own header pins or RCA leads are expected; they are
not interactions with another component.

The accepted legacy CHERRY drawing, `sources/Cherry-MX-Legacy-Layout.pdf`
page 1, gives 11.6 mm body, 3.6 mm stem and 3.30 mm below-body projection.
That leaves 1.70 mm below a 1.60 mm board. The central plastic locator has no
separate axial dimension in that drawing; it must not be assigned a trimming
instruction intended for metal leads. All five verified hole locations now
have a conservative 1.70 mm lower-feature reserve. These clear the other
modeled parts and are explicitly envelopes rather than invented plastic-post
geometry. The existing manufacturer form/fit acceptance remains unchanged.

## Plug, antenna and service reserves

The USB reserve is an unselected 14 x 8 mm overmould with 28 mm outside access.
It reaches the panel outer face without a positive-volume collision. Its face
is 1.39 mm from the recessed connector body datum. The 12 x 6 mm opening clears
the modeled 8.94 x 3.4 mm receptacle body by 1.53 mm per horizontal side and
1.30 mm per vertical side. These facts do not establish mating insertion for
every USB plug: the chosen plug's exposed metal nose, shoulder and overmould
must be checked against the 1.39 mm setback.

RCA reserves are 13 mm diameter x 30 mm long. They are separated by 1 mm and
remain 1.3 mm outside the rear panel at their forward limit. The 11.2 mm
openings provide nominal 0.5 mm radial clearance to the 10.2 mm connector
barrels. Actual plug grip diameter, finger access and insertion depth remain
purchase-specific checks.

The antenna mating reserve is 12 x 12 mm, extending 9.55 mm below the module;
the adjoining 8 x 64 x 6 mm corridor goes toward the front wall. They have no
unintended fitted-solid intersections. This is space for a future pigtail,
not a selected antenna or a certified cable bend radius. The intended bend
allowance is at least 5 mm; confirm the purchased cable and mating tool, and
secure its final route. No antenna MPN was invented.

The two service paths reserve a straight 3 mm insulated probe through 4 mm
base holes to the RESET/BOOT switches. They contact their target switch faces
and have no intervening fitted-solid interference.

## Fasteners, enclosure and assembly motion

The modeled motherboard supports include 1 mm adjustable insulating shims.
The ordinary base and enclosure walls are 3 mm. Main-board screws enter their
base bosses by a nominal 4.90 mm. Lower closure screws engage their body bosses
by 7 mm. The removable top's M3 x 8 screws, with 0.5 mm washers, reach from
Z = 42.5 to 34.5, providing 4.5 mm engagement in bosses spanning Z = 34 to 39.
The upper bosses clear the motherboard by 1.65 mm. Thread intersections in
these simplified solids are intended engagement, not evidence that the
plastic material or thread torque has been strength-qualified.

The two rear M2 x 8 screws pass through the 1.5 mm panel and engage 6.5 mm
of the side bosses. Their pilots open at the inner boss face; retain that
through-hole construction rather than treating the exact screw-tip plane as
clearance at the bottom of a blind hole.

Static clearance of the original one-piece enclosure did not prove
installability. Removing the top alone still did not permit the proposed
straight PCB/base slide: MASTER's shaft reaches Y = -18, so clearing the inner
rear wall at Y = 3 would require at least 21 mm forward movement, whereas the
PCB front has only 3 mm available. The design owner added a separate 1.5 mm
aluminium rear panel, with M2 screw axes at X = 5 / 120, Z = 22.

Lowering the frame over an already base-mounted PCB also failed a motion
check: the rear screw bosses intersected the PCB by 28.8 mm³ at a 10 mm frame
offset, and the lower closure bosses by 42.291 mm³ at a 20 mm offset. The
implemented solution is inserting the PCB horizontally before raising the base,
with small entry reliefs in the rear columns. The saved reliefs are
X = 5.5 to 8 and 117 to 119.5, Y = 1.5 to 3, Z = 30.5 to 33. A temporary
read-only trial was followed by a fresh check of the actual saved geometry:
continuous swept bounding boxes for **130** PCB/module/control/hardware and
solder/lower-feature objects pass 160 mm from the rear to their final positions
without frame intersection. The entry opening provides 0.50 mm horizontal
margin per PCB side, 0.25 mm below the board and 0.65 mm above it.

The base check used a conservative continuous swept solid, formed from the
base and its horizontal faces extruded 40 mm downward. It had no intersection
with the fixed frame or PCB/module assembly. Rear-panel motion at offsets
0, 0.2, 0.5, 1, 2, 3, 5, 8, 12, 18, 22 and 26 mm outward had no collision;
the top and its bezel/foam/hardware subassembly likewise cleared at upward
offsets 0, 0.2, 0.5, 1, 2, 3, 5, 8, 12, 20 and 30 mm. Panel motion checks are
discrete exact-shape samples, not an arbitrary-orientation assembly simulation.

The intended assembly sequence with separate top and rear panels is:

1. Fit and inspect all PCB components, headers and daughterboards outside the
   frame. Fasten display and DAC hardware while both sides remain accessible.
   Fit the antenna pigtail and retain it within the reserved corridor.
2. With the base, top and rear panel absent, slide the PCB/module assembly
   forward through the open rear at its final board height, using the rear
   entry reliefs. Rear shafts and ports remain free through the open rear.
3. Raise the base from below, align the motherboard shims and DAC locating
   supports, then fasten the motherboard from the open top and the lower
   closure screws from below. Do not lower the frame over a base-mounted PCB.
4. With rear knob, nut and washer removed, slide the rear panel axially over
   MASTER's shaft and the rear connectors, seat the reaction collar, then
   fasten the panel. Keep the rear extraction path free for service.
5. Lower the top over the front bushings and switch bodies with front knobs,
   washers, nuts and keycaps removed. Fasten the top and then the panel-control
   hardware using the measured collars; fit knobs and keycaps last.

Tool-path checks found no collision for 5 mm diameter axial driver reserves
above the four top screws, below the four lower closure screws, and above the
four motherboard screws with the top removed. Two 3 mm diameter rear-driver
reserves also clear the fitted assembly. These tested 25 mm exterior tool
lengths and 40 mm motherboard-driver access; match the selected tools to those
envelopes. Display nut 1 has only 0.641 mm to the ESP module: use a suitable
thin/open-ended 4 mm-AF counter-hold tool from the accessible side, and do not
assume an arbitrary socket outside diameter fits there.

Use top access for motherboard/display fasteners before closure. The modeled
1 mm local pot-panel sections and tapped plastic supports require material,
torque, creep and handling checks in the physical prototype.

## Disposition

Nominal control, module, cable, service and solder clearances have been checked
numerically. The known RCA/PCB intersection remains 0.517991 mm³ and is
adjudicated in `RCA-MODEL-COLLISION-AUDIT.md`; no hole enlargement is justified
solely by that free-state model. Do not call the assembly a zero-intersection
design. Remaining explicit physical checks are part tolerances, knob grip,
keycap selection and travel, display retention force, actual plugs and cable,
RCA lead insertion, enclosure material strength and the demonstrated assembly
sequence. These are distinct from unresolved CAD collisions.
