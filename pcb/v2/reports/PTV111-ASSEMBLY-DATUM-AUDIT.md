# PTV111 assembly datum audit

Independent read-only assembly review of Bourns PTV/PTT page 2, stored at `../sources/Bourns-PTV-PTT.pdf`. Source: [official Bourns drawing](https://bourns.com/docs/product-datasheets/ptvptt.pdf). The accepted footprint hole maps and part selections remain unchanged. This review identifies assembly directions and nominal datums; it does not invent an exact housing from a footprint rectangle.

## Front PTV111-3420A-B203

In the PTV111-3 side elevation, the **7.5 mm** dimension runs axially from the rear mounting/seating plane to the bushing-base plane. The **3.5 mm** tail protrusion ends at that rear plane. The **2.0 mm** callout is transverse to the shaft axis and describes the mounting-stake width; it is **not an additional axial stand-off**. Adding it to the PCB-to-bushing stack is incorrect.

For PCB component-face height `Zp` and the shaft pointing upward:

| Datum | Nominal Z |
|---|---:|
| Rear mounting/seating plane | Zp |
| Bushing base / front body datum | Zp + 7.5 mm |
| End of M9 x 0.75 threaded bushing | Zp + 12.5 mm |
| Shaft end, L = 20 mm measured from bushing base | Zp + 27.5 mm |

At the supplied `Zp = 30.6 mm`, these are 30.6, **38.1**, **43.1** and **58.1 mm**. With the top panel at Z40–42, the nominal body-to-panel-inside gap is 1.9 mm and thread beyond the outer face is only **1.1 mm**. The prior extra-2-mm assumption incorrectly predicted 3.1 mm thread exposure. A nut/washer stack cannot be accepted without its actual required engagement. Nominal available external thread is `Zp + 12.5 - Zpanel_outer`; choose the final board/panel stack from that equation and verified hardware dimensions, including tolerances.

## Rear PTV111-1420A-B103

The PTV111-1 side elevation gives the shaft axis **10.0 mm from the PCB seating plane**. The mounting-slot row centerline is the axial datum at the rear body plane: its vertical extension is the left endpoint of the 7.5 mm axial dimension. The electrical lead row is 3.3 mm toward the bushing from that row. The front body/bushing-base datum is 7.5 mm toward the shaft, bushing end 12.5 mm, and L20 shaft end 27.5 mm.

For bottom-side assembly with motherboard lower face `Zb = 29 mm`, shaft axis is:

`Zaxis = Zb - 10.0 = 19.0 mm`.

Define unit direction `d` along the shaft toward the knob, in the board plane, and slot-row origin `O`. Then:

`electrical_row = O + 3.3 d`

`bushing_base = O + 7.5 d`

`bushing_end = O + 12.5 d`

`shaft_end = O + 27.5 d`.

The accepted footprint electrical row is local **+Y**. The literal side elevation therefore makes the shaft point **local +Y as well**. The older negative-local-Y sentence in the control map is inconsistent with this axial dimension chain; correcting the assembly direction does not change the footprint's accepted pad coordinates.

Under the current KiCad bottom-side transform, B.Cu / rotation 180 places that local +Y toward **global +Y**. Thus the supplied RV7 origin `(23, 12)` points its shaft into the case, away from rear panel Y0–3. Its bushing base, end and shaft end would be Y19.5, 24.5 and 39.5 mm, not through the rear panel.

For a rear-facing orientation (`d = global -Y`; B.Cu / rotation 0 with the current footprint), the same origin gives bushing base Y4.5, bushing end Y-0.5, shaft end Y-15.5 mm, with axis `(X, Z) = (23, 19) mm`. It leaves 1.5 mm between body datum and inner panel Y3 and only 0.5 mm of thread beyond outer panel Y0. For a generic origin `Yo`, rear-facing thread exposure beyond Y0 is `12.5 - Yo`, while the body-to-inner-panel gap is `Yo - 7.5 - 3`. Both the orientation and axial position therefore need review against the real nut/washer stack before making the rear hole. Do not infer housing collision clearance from the approximate Fab envelope.

## Render references and limits

The reviewer inspected the exact page-2 side elevations and recommended PCB layouts, rendered at 4x from the saved manufacturer PDF. Working crops are `C:/Users/dot/AppData/Local/Temp/dubsiren-v2-tools/datum-rear.png` and `datum-front.png`; the reproducible source is the PDF page, not those temporary images. No KiCad or CAD object was changed. These are nominal manufacturer dimension chains; detailed molding profile, mounting compression, shaft/knob engagement, tolerances and actual washer/nut dimensions still need assembly acceptance.
