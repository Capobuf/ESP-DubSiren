# Same Sky RCJ-2123 physical lead map

Source: Same Sky RCJ-21 datasheet rev. 1.06, 2026-09-01, page 2,
`pcb/components-documentation/rcj_21.pdf`.
Manufacturer source: https://www.sameskydevices.com/product/resource/rcj-21.pdf

This is one block containing two RCA sockets. The schematic uses the red X1
socket for the mono signal and leaves the second center terminal unconnected.
Both barrel terminals connect to ground. The DAC outputs are never joined.

Origin: midpoint between mounting slots. X right, Y down into the enclosure;
the manufacturer's explicitly labelled recommended PCB layout is a top view.

| Physical lead | Function | Symbol pin / pad | X mm | Y mm | Drill mm | Copper mm |
|---|---|---|---:|---:|---:|---:|
| 1 | X1 barrel | 1 | 7 | 0 | 2.5 | 3.5 |
| 2 | X1 center | 2 | 3 | 4.75 | 1.2 | 2.2 |
| 3 | X2 barrel | 3 | -7 | 0 | 2.5 | 3.5 |
| 4 | X2 center | 4 | -11 | 4.75 | 1.2 | 2.2 |
| Left mounting tab | Mechanical | no electrical pin | -12.15 | 0 | oval 2.5 x 4.2 | none |
| Right mounting tab | Mechanical | no electrical pin | 12.15 | 0 | oval 2.5 x 4.2 | none |

Four electrical leads/pads; zero duplicates; two mechanical-only slots.
The 4.75 mm row separation is 10.25 - 5.5 from the manufacturer's drawing.
The 1 mm increase from electrical drill diameter to copper diameter gives
0.5 mm nominal annular rings; pin 1 is square to identify orientation.

Konnect `create_footprint` and `edit_footprint_pad` only accept scalar drill
diameters. `sources/set_rca_slots.py` uses the native KiCad API solely to set
the two oval drills, then saves and reopens the library footprint. This is the
fallback explicitly authorized in the task for operations missing from MCP.

STEP source: https://www.sameskydevices.com/product/resource/3dmodel/rcj-21-series/rcj-2123
Downloaded from the manufacturer's CAD service as `Same_Sky_RCJ-2123.step`.
The original model is preserved in `3dmodels/SameSky_RCJ-2123.step`.
Terms: https://www.sameskydevices.com/cad-model-terms
FreeCAD successfully loaded three solids and reported a valid shape.
The aligned copy uses the carrier origin and KiCad model axes (model Y is
opposite footprint Y). The rigid transformation is X=-sourceX+0.290298297,
Y=sourceZ+2.802225586, Z=sourceY+9.255353502 mm. It preserves analytic surfaces.
The port pitch is 14 mm and their axis is 8 mm above the PCB. The maximum
housing height is 20.5 mm. Width is 27.4 mm versus the drawing's 27.5 +/-0.3 mm;
front extent is 13.3 mm versus nominal 13.5 mm from the footprint origin.
The original and aligned solids both pass FreeCAD's `isValid()` check.
Physical fit remains a prototype check.

Footprint lead-map acceptance: MCP library and live instance readback passed,
including both oval drills; scratch render inspected after native save/reopen
and after a final MCP library refresh. Evidence: `rca-scratch.png`,
`rca-scratch.svg`, `rca-scratch-mcp-readback.json`, and
`rca-scratch-final-mcp-readback.json`. The final refresh applied one selected
footprint with no conflicts. This acceptance is not a complete-board DRC pass.
