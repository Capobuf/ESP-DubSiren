# RCJ-2123 model / motherboard interference review

Independent, read-only assembly review, 2026-10-02. No PCB, footprint, STEP,
or assembly geometry was changed. This is a focused fit assessment, not a
fabrication approval or a complete assembly certification.

## Finding

The interference is reproducible in the aligned manufacturer STEP before any
enclosure placement. The current seating height and hole-row alignment are
supported by the model and drawing. There is no supported placement correction
or hole enlargement to apply. Keep the manufacturer's recommended holes.

The drawing explicitly depicts bent ground terminals. A free-state terminal
shape that deforms during insertion is the most consistent explanation for
the ground-lead intersection; this is an engineering inference, not a vendor
statement confirming allowable deflection or insertion force. Actual insertion
fit remains unverified. Do not relabel this as a zero-interference assembly.

## Sources and method

- Same Sky RCJ-21 datasheet, revision 1.06, 2026-09-01, page 2, local
  `pcb/components-documentation/rcj_21.pdf`; [manufacturer source](https://www.sameskydevices.com/product/resource/rcj-21.pdf).
  The page was rendered and visually inspected. It gives two 2.5 mm ground
  holes, two 1.2 mm signal holes, and two 2.5 x 4.2 mm mounting slots in the
  explicitly labelled recommended PCB layout, top view. Ground pitch is
  14 mm, slot pitch 24.3 mm, and signal-row separation is 10.25 - 5.5 =
  4.75 mm. General drawing tolerance is 0.30 mm and PCB tolerance 0.10 mm,
  both plus/minus unless otherwise specified.
- Preserved vendor model `pcb/v2/3dmodels/SameSky_RCJ-2123.step` and aligned
  copy `SameSky_RCJ-2123_aligned.step`; [manufacturer model source](https://www.sameskydevices.com/product/resource/3dmodel/rcj-21-series/rcj-2123).
  The alignment script is `pcb/v2/sources/align_rca_model.py`.
- Existing evidence: `assembly-collision-raw.json`,
  `pcb/v2/mechanical/rca-board-intersection-review.step`, and
  `RCA-PHYSICAL-LEAD-MAP.md`.
- Independent FreeCAD 1.1 / OpenCascade solid intersections and 0.002 mm
  sections of the aligned STEP. A new temporary solid represented the board
  from local Z = -1.6 to 0, with the exact nominal manufacturer hole pattern
  subtracted. This did not modify or save the board or model.

Model-local Y is opposite footprint-local Y. The board therefore crosses
negative model Z on this through-hole part. The reported assembly uses
PCB bottom Z = 30.75 mm, top Z = 32.35 mm, B180 connector origin
(92.5, 12) mm.

## Datum checks

Four model seating-foot faces are at Z = 0, with X ranges
11.35 to 12.95 mm and -12.95 to -11.35 mm, and Y ranges -4.5 to -1.5 mm
and 1.5 to 4.5 mm. Each face has area 4.8 mm². A central housing underside
face is at Z = 0.3 mm, so it does not define a lower seating plane.
This supports the existing seat at PCB bottom Z = 30.75 mm.

At board depth, the two rigid signal leads have 1.0 x 0.5 mm rectangular
sections centered at model (3, -4.75) and (-11, -4.75) mm, exactly matching
the footprint hole centers after the stated Y conversion. The mounting
stems center at (12.15, 0) and (-12.15, 0) mm. Moving the complete connector
to make its ground bends fit would displace these correctly aligned features.

## Measured interference

| Feature | Count | Volume each, mm³ | Model-local intersection envelope, mm |
|---|---:|---:|---|
| Ground terminal | 2 | 0.25726855 | X = 6 to 8 or -8 to -6; Y = 0.75 to 1.419769; Z = -1.6 to -0.194165 |
| Mounting-tab corner | 4 | 0.00086403 | Outer X near ±13.35; absolute Y = 1.2 to 1.5; Z = -1.6 to -1.461773 |

Total independent intersection is 0.51799325 mm³, agreeing with the full
assembly's 0.517991 mm³ within about 0.000003 mm³. The assembly ground-lead
envelope consequently includes Z = 30.944165 to 32.35 mm and Y = 10.580231
to 11.25 mm, reproducing the reported locations. No signal-pin intersection
was found in this nominal rigid geometry check.

The 2 mm wide ground terminals have these approximate local Y envelopes:

| Depth below model seating plane, mm | Minimum Y, mm | Maximum Y, mm |
|---:|---:|---:|
| 0 | 0.103 | 0.658 |
| 0.8 | 0.484 | 1.039 |
| 1.6 | 0.865 | 1.420 |

The ground-hole radius is 1.25 mm. At the terminal edges X = center ±1 mm,
the circular hole permits only |Y| <= sqrt(1.25² - 1²) = 0.75 mm.
Thus the model's furthest corner at board depth exceeds that boundary by
about 0.670 mm. By comparison, the correctly centered rigid signal lead
allows a global Y translation of at most sqrt(0.6² - 0.5²) - 0.25 =
0.08166 mm in its nominal 1.2 mm circular hole. A global seating translation
does not resolve the conflict. Lifting the part enough to avoid the bend
would also abandon its seating feet and alter the panel datum.

The tiny tab-corner intersections occur at the ends of the wider lower
tab geometry, not at the central stem. They are consistent with a retention
feature or a drawing/model tolerance difference, but no allowable insertion
deformation or force has been established. Their small volume alone is not
proof of acceptable fit.

## Disposition

Retain the 2.5 mm ground holes, 1.2 mm signal holes, and 2.5 x 4.2 mm slots
from the manufacturer's recommended PCB layout. Do not enlarge holes or trim
the official model merely to make the collision count zero. The rigid CAD
collision remains recorded and is distinguished from an enclosure or PCB
placement error.

Qualification requires a representative RCJ-2123 sample inserted into a
1.6 mm coupon with the intended finished plated-hole sizes and NPTH slots.
Check full seating on all four feet, both ground leads and retention tabs,
reasonable assembly force, and no lead, plating, laminate, or housing damage.
Use actual finished-hole and board-thickness tolerances when ordering that
coupon. A manufacturer confirmation of intended bent-lead insertion behavior
and permitted board thickness would also resolve the missing design evidence.
Without either evidence, physical insertion fit is **unverified**, even though
the footprint follows the published nominal pattern.
