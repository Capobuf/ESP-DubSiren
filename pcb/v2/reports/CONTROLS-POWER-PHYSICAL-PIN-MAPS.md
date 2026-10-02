# Bourns controls and input switch acceptance

Evidence date: 2026-10-02. Sources: [Bourns PTV/PTT Rev 09/23](https://bourns.com/docs/product-datasheets/ptvptt.pdf), pages 2 and 5; [Bourns PEC11R](https://www.bourns.com/docs/product-datasheets/pec11r.pdf), pages 2–3; [TI TPS22918 SLVSD76C](https://www.ti.com/lit/ds/symlink/tps22918.pdf), pages 3 and package drawing. The user's resolved PTV111 ordering and recommended-layout orientation are authoritative for this continuation. Coordinates below are footprint-local, component-side PCB view, X right/Y down; no silent solder-side mirroring.

## PTV111-3420A-B203, six front controls

Library `ESP-DubSiren-V2-Mechanical:Bourns_PTV111-3`, symbol `Device:R_Potentiometer`. Origin is the mounting-aperture midpoint; mounting apertures align with the shaft centerline in the side elevation. Use the current sheet's 13.6 mm aperture pitch, not the obsolete two-page archive's 13 mm.

| Datasheet lead | Function | Symbol pin/type | Pad | X/Y mm | Evidence |
|---|---|---|---|---|---|
|1|CCW end|1/passive|1|-3.75/11|p 2 PCB recommended layout, p 5 electrical diagram; component view|
|2|Wiper|2/passive|2|-1.25/11|same|
|3|CW end|3/passive|3|1.25/11|same|
|4|Dummy|Absent intentionally|Unnumbered plated hole|3.75/11|p 5 explicitly labels DUMMY; component view|
|Left snap stake|Mechanical|Absent|Edge.Cuts aperture|-6.8/0|p 2 component view|
|Right snap stake|Mechanical|Absent|Edge.Cuts aperture|6.8/0|p 2 component view|

Four physical metal terminals, three electrical symbol/pad identities, one electrically isolated dummy land; no duplicate electrical pads; two mechanical apertures. Electrical/dummy drill 1.10 mm within 1.0–1.2 mm recommended range; copper 1.90 mm; pin 1 square. Aperture envelope 2.0 x 2.1 mm, internal corner radius 0.4 mm to permit 0.8 mm routing. These are real board cutouts on Edge.Cuts, not schematic mounting pins or cosmetic drawings. Fabrication must preserve these internal routes. Silkscreen/fab rectangles are body clearance envelopes; the dimensioned drilling geometry is the acceptance basis, not a full molding outline.

## PTV111-1420A-B103, rear MASTER

Library `ESP-DubSiren-V2-Mechanical:Bourns_PTV111-1`, same three-pin symbol. Origin mounting-aperture midpoint; shaft is parallel to board and points toward negative local Y.

| Datasheet lead | Function | Symbol pin/type | Pad | X/Y mm | Evidence |
|---|---|---|---|---|---|
|1|CCW end|1/passive|1|-3.75/3.3|p 2 recommended layout; component view|
|2|Wiper|2/passive|2|-1.25/3.3|same|
|3|CW end|3/passive|3|1.25/3.3|same|
|4|Dummy|Absent intentionally|Unnumbered plated hole|3.75/3.3|p 5 electrical diagram|
|Left snap stake|Mechanical|Absent|Edge.Cuts aperture|-5.1/0|p 2 component view|
|Right snap stake|Mechanical|Absent|Edge.Cuts aperture|5.1/0|p 2 component view|

Counts, land/drill sizes and aperture construction as above, with 10.2 mm aperture pitch and 3.3 mm row separation. In all seven controls, pin 1 = GND, pin 3 = respective 4.7 kohm-fed POTn_TOP, pin 2 = mux input. Clockwise rotation increases ADC voltage. No control scan code existed to invert this direction; firmware must normalize calibrated endpoints with increasing physical CW value.

## PEC11R-4120F-S0018

Library `ESP-DubSiren-V2-Mechanical:Bourns_PEC11R-4120F-S0018`; symbol `Device:RotaryEncoder_Switch`. Footprint origin is shaft center, component view with A/C/B along positive Y; this is the exact 4xxxF-Sxxxx drawing, not the 1xxx/2xxx right-angle versions.

| Datasheet lead | Function | Symbol pin/type | Pad | X/Y mm | Evidence |
|---|---|---|---|---|---|
|A|Channel A|A/passive|A|-2.5/7.5|p 2 front and PCB projections, component view|
|C|Common|C/passive|C|0/7.5|same|
|B|Channel B|B/passive|B|2.5/7.5|same|
|1|Push contact|S1/passive|S1|-2.5/-7|same; no polarity|
|2|Push contact|S2/passive|S2|2.5/-7|same; no polarity|
|Left stake|Mechanical|Absent|Edge.Cuts aperture|-6.6/0|p 2 PCB projection|
|Right stake|Mechanical|Absent|Edge.Cuts aperture|6.6/0|p 2 PCB projection|

Five physical electrical leads, five symbol pins, five electrical pads, no duplicates; two mechanical apertures. Signal drills 1.05 mm within 1.0–1.1 mm recommendation, lands 1.90 mm. Apertures 2.6 x 1.8 mm with 0.4 mm internal radius. Body 12.5 x 13.4 mm. A/B connect GPIO1/2, common GND; switch connects GPIO42/GND. External 10 kohm pull-ups already fitted. Manufacturer quadrature timing defines direction; firmware must reject invalid transitions and debounce (2 ms specified contact bounce under stated test conditions), rather than presume one GPIO edge equals one detent.

## TPS22918DBVR

Library symbol `ESP-DubSiren-V2:TPS22918DBVR`, native footprint `Package_TO_SOT_SMD:SOT-23-6`. TI p 3 DBV top view, pin 1 upper-left, numbering counterclockwise. Native land pattern is an IPC/JEDEC-compatible extended toe land pattern, not claimed to be the identical TI illustration: 1.325 x 0.600 mm pads at X ± 1.1375 mm; 0.95 mm pitch, body 1.6 x 2.9 mm. No exposed pad.

| Physical lead | Function | Symbol pin/type | Pad | X/Y mm | Evidence |
|---|---|---|---|---|---|
|1|VIN|1/power_in|1|-1.1375/-0.95|TI p 3 DBV top view|
|2|GND|2/power_in|2|-1.1375/0|same|
|3|ON|3/input|3|-1.1375/0.95|same|
|4|CT|4/passive|4|1.1375/0.95|same|
|5|QOD|5/passive|5|1.1375/0|same|
|6|VOUT|6/power_out|6|1.1375/-0.95|same|

Six physical leads/symbol pins/electrical pads; zero duplicates or mechanical pads. QOD is intentionally unused per TI p 3, not an unverified floating input.

## Query and disposable acceptance

Library readback is saved in `controls-library-create.json`; real live disposable footprint pad queries and symbol instances are in `controls-scratch-readback.json` and `power-switch-acceptance.json`. Final rendered inspection: `controls-power-scratch.svg`, `controls-power-scratch.png`, `controls-power-symbols.png`. Pin 1 markers, pad order, outline, mounting routes and mirrored/rotated symbol pin mapping were inspected. A first saved-file fallback scratch placement could not be queried by the installed MCP; it was not accepted. Acceptance uses the subsequent live isolated Electrical-Acceptance board readback. This is component acceptance, not full PCB DRC/manufacturability release. The main board has not been populated or routed.
