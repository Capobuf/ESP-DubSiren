# Independent USB routing and power-copper review

Read-only review, 2026-10-02. No PCB geometry was changed. Trace evidence began
with `pcb-routed-traces.json` (666 segments); USB lengths were independently
checked through native KiCad read-only loading at 677 segments. Added segments
were unrelated to the USB lengths. `independent-usb-route-metrics.json` records
that saved board's SHA-256. Subsequent parent edits require refreshed checks.

## Length and skew

Lengths below are copper centerline sums, not measured propagation delays.
The resistor contribution is its 1.650 mm pad-center span, not a claim about
the resistor's internal electrical delay. The 1.595306 mm B6-to-A6 and
B7-to-A7 ties are equal connector branches and are excluded from paths starting
at A6/A7. Summing every segment on the net without this exclusion would
overstate each A-terminal path by that amount.

| Contribution | D+, mm | D-, mm |
|---|---:|---:|
| A6/A7 routed pad to connector-side resistor pad | 57.190221 | 57.894384 |
| Resistor pad-center span | 1.650000 | 1.650000 |
| Resistor MCU-side pad to U1 pad | 2.482775 | 2.459474 |
| Explicit F-to-B via at (46.6, 13.025) | 1.600000 | 0 |
| Total under that convention | **62.922996** | **62.003859** |

D+ is **0.919137 mm longer** when counting the explicit signal via only.
For a bounded correction to that metric, lengthen D- by approximately 0.92 mm
in an uncoupled escape; do not lengthen D+ or disturb the long coupled trunk.
Recheck clearance and the final complete path after editing.

There is an additional launch-plane issue: J1 A6/A7 are plated through-hole
pads, and D+ leaves A6 on F.Cu whereas D- leaves A7 on B.Cu. The connector is
mounted on B.Cu. Taking both physical terminal launches at the B-side board
surface adds another nominal board-thickness traversal through A6 for D+.
Under that common launch-plane convention the totals become 64.522996 and
62.003859 mm, giving **2.519137 mm** mismatch. Accordingly, 0.92 mm compensation
alone must not be advertised as matching the physical connector-to-MCU path.
If a common B-side launch is the selected length criterion, the bounded
geometric correction is about **2.52 mm added to D-**, with the connector lead
and via discontinuities still outside an exact delay model. Do not count the
equal B-pin tie branches as extra series length to conceal this distinction.

The saved uniform-cross-section solution implies approximately 5.59 ps/mm,
so the explicit-via geometric mismatch corresponds to about 5.1 ps under a
uniform-medium approximation. The actual launch, narrow escape and barrel
delays differ; neither that conversion nor a centerline match establishes
measured signal quality.

Final recommendation: **leave the existing lengths unchanged**. Espressif
does not supply a numerical USB length-skew limit in the cited layout guide;
its adjacent 50 mil rule concerns SDIO. TI's primary USB layout training
recommends a USB 2.0 mismatch within 150 mil (3.81 mm). Both measured
conventions above are below that guidance. This does not establish an
Espressif-specific guarantee, but gives no evidence that the present
full-speed skew is materially unacceptable.
[TI USB layout training](https://www.ti.com/video/6087491555001).

An optional +1.72 mm midpoint compensation was checked offline on the D-
escape from (44.05, 12.075) to (44.05, 13.8625). A 0.20 mm route via
(43.272843, 12.075), (43.072843, 12.275), (43.072843, 13.6625), and
(43.272843, 13.8625) passes a 0.15 mm effective-copper clearance check on
B.Cu. However, its long X = 43.072843 segment would run above the F.Cu
VBUS_RAW reference void, worsening the return path. The equivalent rightward
detour intersects GND copper features near the ESD device. Neither is
recommended merely to improve the reported length metric. No candidate
was added to the board.

## Ground reference and layer transition

The filled polygons were examined with native `GetFilledPolysList` and
`Contains`, without refill or mutation by this reviewer. Centerlines were
sampled at at most 0.025 mm intervals. Every 0.60 mm main-trunk segment had
opposite-layer GND under its centerline. This sampling is not a complete
field-width or return-impedance proof.

The B.Cu zone outline deliberately excludes the USB corridor by about 3 mm.
Actual filled-copper checks at the horizontal trunk around X = 30 mm and
vertical trunk at Y = 25 mm confirm that the zone's general 0.15 mm clearance
does **not** put coplanar ground 0.15 mm beside this main pair. F.Cu ground is
present beneath those trunk samples. The outline is recorded in
`ground-zone-plan-revised.json`.

Local reference gaps were found at these escapes:

| USB location | Opposite-layer interruption |
|---|---|
| D- at (44.05, 15.2) | F.Cu VBUS_RAW crossing, roughly 0.8 mm reference gap along D- |
| Both lanes near Y = 16.6 | F.Cu +5V crossing, roughly 0.8 mm reference gap |
| Both resistor approaches around X = 23.2 | F.Cu +3V3 crossing |
| D+ MCU escape at (25.5, 40.6) | F.Cu POT1 crossing, roughly 0.5 mm reference gap |
| Connector ties, pad exits and DP transition | Opposite signal copper / pad and via antipads |

Thus an assertion of a continuous reference plane along the **entire** USB
path is not supported. A bounded improvement is moving the POT1 crossing away
from the D+ MCU escape where practical. Review short power crossings and
provide nearby return stitching if rerouting them is impractical. Refill and
repeat the polygon check after any change; stitching is not equivalent to an
unbroken plane.

At the explicit DP via, the nearest GND via is (45, 12.8625), 1.608 mm away;
the next is (49.325, 11.02), 3.384 mm away. Consider a second close return via
if clearance allows. Espressif recommends equal parallel USB routes,
90 ohm differential impedance within 10%, a continuous reference, and a pair
of return vias at a necessary layer transition.
[Espressif USB layout guidance](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/pcb-layout-design.html#usb).

A concrete additional GND via candidate is **(47.4, 13.225) mm**, 0.60 mm
diameter / 0.30 mm drill, 0.8246 mm from the signal via. A native effective-shape
check found at least 0.15 mm clearance to non-GND pads/tracks on both copper
layers, and its center lies in both filled GND zones. The J1 holes and VBUS
routing are clear in this check. The PCB owner must still add, refill and run
direct DRC; this is an offline candidate, not a saved board change.

## Two-layer impedance estimate

`sources/solve_usb_cross_section.py` solves a two-dimensional electrostatic
problem with +/-1 V conductors, a ground reference, dielectric and air runs.
Its differential capacitance convention, Q / 2 V, and
Zdiff = 1 / (c sqrt(Cair Cdielectric)) are consistent. It estimates the uniform
trunk cross-section; it does not solve pads, barrels, bends, narrow escapes,
plane slots or frequency-dependent losses.

| 0.60 mm width / 0.15 mm gap case | Differential impedance |
|---|---:|
| Saved 0.0125 mm grid | 90.2429 ohm |
| Saved 0.00625 mm grid | 87.7445 ohm |
| Owner's subsequent saved 0.005 mm grid | 86.3340 ohm |
| Independent 0.0125 mm reproduction | 90.2429 ohm |
| Independent domain doubled from 8 x 4 to 16 x 8 mm, same grid | 90.4105 ohm |

The 2.4984 ohm / 2.77% change on halving the first grid is too large to call the
answer converged to a fraction of an ohm. Thin copper and mask are represented
by only a few grid cells, with staircase placement that changes under grid
refinement. Doubling the far-field domain changes the result by only 0.168 ohm
at the tested grid, suggesting mesh representation is the larger numerical
uncertainty. Further refinement or an independent validated field solver is
needed before claiming numerical convergence.

The subsequent 0.005 mm result (`usb-cross-section-0.005.json`) changes by
another 1.4105 ohm from the 0.00625 mm case. The nominal sequence spans
approximately 86 to 90 ohm, within Espressif's 90 ohm +/-10% recommendation,
but is still not a converged or manufacturing-tolerance-bounded result. No
additional heavy grid run was required to identify that limitation.

For sensitivity only, introducing ideal coplanar ground 0.15 mm from the
traces in the coarse model changes the result to 78.49 ohm; 0.30 and 0.50 mm
clearances give 83.62 and 86.55 ohm. Those cases are **not the actual trunk**.
They explain why preserving the deliberate ground corridor matters. The
modified trial omitted detailed mask coverage over the added distant ground
conductors, and is not a substitute for a validated production field solver.
Results are in `independent-usb-cross-section.json`.

Stack assumptions need clear qualification:

- JLCPCB lists nominal 4.5 dielectric constant for its two-layer FR4 and
  3.8 for solder mask. Its current capabilities offer controlled impedance
  on four and more layers, and specify 10% finished-board thickness tolerance.
  These do not guarantee a two-layer 90 ohm result.
  [JLCPCB capabilities](https://jlcpcb.com/capabilities/pcb-capabilities/).
- The assumed 0.04064 mm outer copper and 0.01524 mm over-copper mask are
  1.6 and 0.6 mil, consistent with the Chinese manufacturer's impedance
  guidance. That page gives 1.0 mil mask on substrate/between traces, whereas
  the script uses 1.2 mil in its general mask region. At the coarse mesh,
  changing to 1.0 mil happened to leave the occupied grid rows unchanged;
  this demonstrates resolution limits rather than proving equivalence.
  [Manufacturer stack parameters](https://www.jlc.com/portal/q6i36372.html).
- The exact 1.48824 mm substrate spacing is derived as
  1.6 - 2(0.04064) - 2(0.01524). It is a modeling assumption about the
  finished-thickness convention, not a published guaranteed two-layer core
  thickness. No dielectric or etched-width tolerance sweep was supplied.

Keep 0.60 / 0.15 mm as the current nominal routing candidate; this review
does not justify a width change from the estimates alone. Report it as a
calculated nominal geometry requiring actual stack/fabrication and prototype
signal-quality qualification, not as manufacturer-controlled 90 ohm copper.

## Power copper

Read-only trace-graph resistance estimates used copper resistivity
0.01724 ohm mm²/m, 0.035 mm foil and illustrative 0.020 mm via-wall copper.
Pads were treated as equipotential, track centerlines were split at graph
junctions, and the lowest-resistance connected trace path was selected.
Parallel paths, pad spreading, contacts, ground-return resistance, temperature
and actual plating tolerances are not solved. Values are estimates rather
than guaranteed bounds or full simultaneous-load voltage solutions.

| Path | Estimated resistance | Illustrative drop |
|---|---:|---:|
| J1 to F1 | 13.87 milliohm | 6.76 mV at 487 mA |
| F1 to U7 VIN | 11.23 milliohm | 5.47 mV at 487 mA |
| U7 output to buck input | 19.29 milliohm | 9.40 mV at 487 mA |
| U7 output to DAC | 63.35 milliohm | 3.17 mV at 50 mA, excluding shared-load effects |
| Buck output to MCU | 37.53 milliohm | 15.01 mV at 400 mA, excluding other shared loads |
| Buck output to display | 84.59 milliohm | 6.77 mV at 80 mA, excluding shared-load effects |
| Buck output to mux | 61.68 milliohm | Low-current branch |

VBUS_RAW uses at least 0.50 mm traces. The protected input has short 0.30 mm
pin escapes. +5V has at least 0.40 mm traces; +3V3 has short 0.30/0.40 mm
escapes and wider distribution. No gross trace-width deficiency was found
for the allocated sub-0.5 A loads. For scale, 1 mm of 0.30 mm x 35 um copper
has about 1.64 milliohm resistance and dissipates about 0.41 mW at 0.5 A.
That is not a temperature-rise qualification or fault-current rating.

The 150 mV total path-loss allocation in `USB-POWER-DECISION.md` remains
tight: using its 79 milliohm switch bound, the documented 0.20 ohm post-trip
PTC bound and about 44.40 milliohm positive copper gives approximately
157.5 mV at 487 mA, even before connector and ground-return losses. This
does not establish a failing nominal device, but it does prevent treating the
existing 487 mA budget calculation as a guaranteed worst-case closure.
Evaluate the reset/hot PTC, actual shared currents and measured path voltage.

## Disposition

The long USB trunk is a plausible two-layer prototype route, with an
intentional same-layer ground exclusion and a present opposite reference at
the tested centerlines. Final acceptance remains **INCOMPLETE** for USB
impedance and complete return continuity. Use a stated launch-plane convention
for any matching correction; preserve the measured exceptions and qualification
limits in the final report. Refresh this review's geometric checks after the
owner's final routing and ground-fill changes.
