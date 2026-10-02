# Independent V2 milestone review

2026-10-02. **INCOMPLETE — review could not evaluate the full design.** This is a read-only audit of the saved controls/power milestone, not fabrication approval. No KiCad design files were modified by the reviewer.

## Direct evidence collected in this review

| Check | Observed result | Meaning |
|---|---|---|
| MCP `run_erc`, severity info | 0 errors, 0 warnings | Present saved schematic only |
| MCP short / orphan / single-pin checks | 0 / 0 / 0 | No candidates in those checks |
| Saved netlist summary | 133 symbol instances, 52 named nets | Includes virtual power symbols; not 133 fitted parts |
| Saved-board DRC | 1 invalid-outline error, 60 missing-footprint parity warnings | Main PCB is unfinished |
| DRC unconnected items | 0 | Vacuous on a board containing zero footprints, pads and named nets; **not routed acceptance** |
| Aggregate review | Partial; `zero_footprints` diagnostic | Verdict remains INCOMPLETE |
| Default live board inventory | IPC connection unavailable | Not represented as a successful live inspection; aggregate saved-file inventory establishes the empty board |

Evidence files are `independent-erc.json`, `independent-empty-board-drc.json`, `independent-connectivity.json` and `independent-aggregate-review.json`. The aggregate explicitly reports saved schematic hierarchy plus saved board as its source. It covered one schematic file / sheet instance with no unresolved symbols. No suppression was introduced in this review.

## Confirmed controls and input-switch result

Fresh exported connectivity confirms RV1–RV7 pin 1 to ground, pin 2 to POT1–POT6 / MASTER, and pin 3 to the individual POTn_TOP excitation net. The mux maps those signals to channels 0–6 and grounds channel 7. SW7 A/B/common map to ENC_A / ENC_B / GND; its switch maps ENC_SW / GND. GPIO1/2/42 are the corresponding MCU connections. Four performance switches SW1–SW4 are absent; their pull-ups and named nets do not make the schematic complete.

Fresh library queries confirm PTV111-3 electrical terminals at X = -3.75, -1.25, +1.25 mm and Y = 11 mm; the dummy plated terminal at X = +3.75 mm is unnumbered. PTV111-1 uses the same X values and Y = 3.3 mm. Both have 1.10 mm drills / 1.90 mm lands. The independently viewed [Bourns PTV/PTT drawing](https://bourns.com/docs/product-datasheets/ptvptt.pdf), page 2, supports the distinct 13.6 mm and 10.2 mm mounting-aperture pitches. The user's resolved view/orientation remains authoritative. There is no new basis to reopen that decision.

The PEC11R query confirms A/C/B at (-2.5, 7.5), (0, 7.5), (+2.5, 7.5) mm and switch contacts at (+/-2.5, -7) mm, with 1.05 mm drills. These agree with the exact 4xxxF-Sxxxx projection in the independently viewed [Bourns PEC11R drawing](https://www.bourns.com/docs/product-datasheets/pec11r.pdf), page 2. Electrical pin counts and pad identities agree; no duplicate electrical identities were found.

U7 pin map and native SOT-23-6 pad order match [TI TPS22918 Rev C](https://www.ti.com/lit/ds/symlink/tps22918.pdf), page 3: VIN 1, GND 2, ON 3, CT 4, QOD 5, VOUT 6. Fresh netlist confirms VIN and ON on VBUS_PROTECTED, VOUT on +5V, C10 from input to ground, C11 from CT to ground, and QOD intentionally open. TI explicitly permits floating QOD. No new wiring correction was identified in these reviewed additions.

## Findings requiring completion before release

1. **Incomplete / fabrication gate: exact MX2A drilling acceptance and four switches.** `MX2A-SOURCE-SEARCH.md` records the source gap; a legacy MX drawing cannot establish exact MX2A-11NW equivalence. Obtain the manufacturer hole coordinates, diameters/tolerances and view; compare the native footprint, then have the schematic owner add SW1–SW4 through MCP and rerun ERC/netlist/render acceptance. This is a source-dependent unfinished requirement, not an ERC defect.
2. **Incomplete / fabrication gate: board and full assembly do not exist yet.** The direct DRC's invalid outline and 60 missing footprints are expected but real. After schematic acceptance, sync through `update_pcb_from_schematic`, complete geometry/placement/routing, enforce the MPM NC-metal copper exclusion, and run saved/refilled DRC plus connectivity and manufacturing checks. USB impedance, clearances, routing, enclosure cutouts, mounting, key travel, silkscreen and manufacturing files are presently unevaluated. No fabrication package is justified.
3. **Warning / operating qualification: USB startup, configuration and suspend policy remains unimplemented/unverified.** Hardwired U7 ON controls charging slope but does not authorize a 500 mA load before USB configuration or meet suspend current by itself. The power report correctly discloses this. Define and implement the intended startup/USB state policy and validate complete-unit current before calling default-source operation qualified; account for the always-powered DAC/display. This cannot be closed by an ERC pass.
4. **Warning / operating qualification: 487 mA is a conditional allocation, not a guaranteed maximum.** Independent arithmetic reproduces approximately 487 mA from 485 mA at 3.333 V, 4.35 V at the buck, 85% efficiency, and 50 mA DAC load. The 13 mA margin is small; the MCU memory/activity allowance, TFT backlight allocation, efficiency and interconnect drop require measurements. A 500 mA MCU-only rail allocation would exceed the USB budget. Preserve the bounded RF policy and do not infer extra Type-C current from the connector alone.
5. **Warning / mechanical qualification: routed mounting apertures require fabrication and fit acceptance.** MCP readback confirms real closed Edge.Cuts constructions, not decorative slots. The adopted 0.4 mm corner radius is a manufacturing choice absent from the square-corner manufacturer sketches; nominal outside dimensions alone do not establish corner fit or finished-route tolerances. Check the actual stake envelope and selected fabricator's router/finished dimensions, then verify fit. PTV/PEC custom footprints currently report no attached 3D model, so pad acceptance is not complete assembly-model acceptance.

## Power calculation review

`USB-POWER-DECISION.md` correctly distinguishes direct attach capacitance from DC-biased operating capacitance. Its passive-small-CIN feasibility calculation does not ignore the DAC's separate 10 uF VIN capacitor. The selected input switch isolates that downstream bulk during the ramp; the PTC is not treated as inrush control. The 1.40 uF direct-input figure includes a stated, unmeasured parasitic allowance and must retain that qualification.

The nominal 4.84 mA bulk-charging calculation is consistent with 32.1 uF, a 4 V change and 26.55 ms. TI's equation on page 14 already accounts for its 10–90% convention and instructs multiplying by VIN; the report's 27.65 ms equation estimate and 26.55 ms typical table value are consistent with that document. Neither is a guaranteed minimum rise time. The report appropriately includes downstream regulator capacitors and startup load as additional concerns, rather than relabeling 4.84 mA as total inrush. The 50 V C0G CT choice meets TI's recommended minimum rating. Bench attach/reconnect/startup traces remain necessary.

## Aggregate heuristic reconciliation

The aggregate emits six errors / 62 warnings / three informational findings, combining one real outline error, 60 missing footprints, and schematic heuristics. These totals must not be confused with ERC's 0/0 result.

| Aggregate candidate | Review disposition |
|---|---|
| U2 VCC lacks bypass | Existing MPM evidence identifies this external capacitor as optional; no automatic addition justified |
| U5/U6 unused 3V3_OUT lacks bypass | Breakout-local regulator capacitors are inside the modules; unused exported pins need not get redundant carrier capacitors |
| U4 / VBUS_RAW lacks bypass | ESD-array and USB attachment/layout decision, not a generic logic-IC decoupling rule; no confirmed wiring defect from this heuristic |
| VBUS_PROTECTED lacks >=10 uF bulk | Deliberately small input capacitance; blindly adding bulk would undermine the attach-capacitance decision |
| VBUS_SLEW_CT lacks >=10 uF bulk | CT is a timing node, not a supply rail; C11 = 10 nF is intentional |
| RAW / PROTECTED / CT lacks test points | Optional debug-access review during layout; existing connector/pad probe access may suffice |

## Mechanical claim consistency and coverage

The Hammond report's lower-bound arithmetic is consistent: 20.5 mm RCA + 1.6 mm PCB + 7.5 mm pot body + 1.0 mm total clearance = 30.6 mm, exceeding the stated 29.7 mm cavity by 0.9 mm. This supports rejection of that stated opposite-face, single-planar-board stack. It is not exhaustive certification of all conceivable assemblies. The replacement 125 x 140 x 42 mm model and 113 x 128 mm trial board are explicitly studies; their own JSON leaves final Z stack, cutouts, collisions, key travel and fasteners unperformed. The reviewer did not rerun a full CAD collision analysis.

No new confirmed miswire was found in the controls/U7 milestone. Release remains **INCOMPLETE**, with source acceptance, physical implementation and operating qualification outstanding. Re-review after the owner resolves those items; do not export final Gerbers from this milestone.
