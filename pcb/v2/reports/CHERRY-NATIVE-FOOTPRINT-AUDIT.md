# CHERRY MX2A native footprint acceptance audit

Status: **Accepted for MX2A-11NW; SW1-SW4 are present.** The user's resolved acceptance is supported by the official [CHERRY MX1A-11NW discontinuation notice](https://www.mouser.com/PCN/CHERRY_MX1A_11NW_EOL.pdf), dated 2025-10-24, naming MX2A-11NW as the replacement and stating: “Optimized internal components. No changes to form, fit, or function.” This equivalence connects the legacy manufacturer's dimensioned PCB layout to the purchased MX2A part. No further dedicated MX2A pattern or model is required.

The MCP query of installed `Button_Switch_Keyboard:SW_Cherry_MX_1.00u_PCB` is saved as `cherry-native-footprint-readback.json`. Its local origin is electrical pad 1; its actuator center is X = -2.54 mm, Y = 5.08 mm. Recentered on the actuator, with X right and Y down in component-side PCB view:

| Feature | X / Y (mm) | Diameter / envelope (mm) |
|---|---|---|
| Electrical pad 1 | +2.54 / -5.08 | 1.50 drill; 2.20 copper land |
| Electrical pad 2 | -3.81 / -2.54 | 1.50 drill; 2.20 copper land |
| Central locating hole | 0 / 0 | 4.00 NPTH |
| Left locating peg hole | -5.08 / 0 | 1.70 NPTH |
| Right locating peg hole | +5.08 / 0 | 1.70 NPTH |
| F.Fab body | Centered | 12.70 x 12.70 |
| F.CrtYd | Centered | 13.20 x 13.20 |
| F.SilkS outline | Centered | Approximately 13.97 x 13.97 |
| Dwgs.User keycap envelope | Centered | 19.05 x 19.05 |

The legacy CHERRY drawing is saved as `../sources/Cherry-MX-Legacy-Layout.pdf` and rendered in `cherry-legacy-layout.png`. The “Keyswitch with fixation pins / 1-Pole” pattern uses a 0.05 inch (1.27 mm) grid. It places contacts at (+2, -4) and (-3, -2) grid intervals from the center, and fixation posts at (+/-4, 0), matching the native component-side coordinates above. The specified hole sizes are 0.157 +/- 0.004 inch (3.9878 +/- 0.1016 mm), 0.067 +/- 0.002 inch (1.7018 +/- 0.0508 mm) and 0.059 +/- 0.002 inch (1.4986 +/- 0.0508 mm). Native 4.00, 1.70 and 1.50 mm holes lie within those bands.

Two physical electrical leads map to symbol `Switch:SW_SPST` passive pins 1 and 2 and footprint pads 1 and 2. There are no duplicated electrical pads. The three unnumbered NPTH holes are mechanical. Contact numbering is an explicit circuit assignment for an unpolarized contact pair: pin/pad 1 goes to the corresponding BTN1-BTN4 net, pin/pad 2 to GND. Existing 10 kohm pull-ups make the input active-low when pressed.

The native footprint is retained unchanged as requested. Its small F.Fab/courtyard rectangles must not be mistaken for the complete upper housing envelope: mechanical collision checks use the actual housing/model and 19.05 mm keycap envelope. The associated KiCad model is `${KICAD10_3DMODEL_DIR}/Button_Switch_Keyboard.3dshapes/SW_Cherry_MX_1.00u_PCB.step`, with unit scale and zero native offset/rotation. The user accepts this existing MX model for MX2A. Prototype verification is a dry-fit of the purchased switch in the PCB, including contact and fixation holes, seating and keycap travel.

The user also supplied [sszczep/kicad-libraries](https://github.com/sszczep/kicad-libraries). Inspection at commit `7f4d13488a3a255c5a3ec4e45466822d0e9351a6` found a generator referring to the old MX-series manufacturer drawing mirrored by SparkFun. Its coordinates corroborate the native pattern, with reversed equivalent contact numbers and 1.75 mm fixation holes. The official CHERRY notice supplies the previously missing MX2A equivalence; this third-party library is supplementary evidence, not the selected footprint.

Native library query, disposable live PCB placement/pad readback, symbol query and disposable schematic placement are saved in `cherry-acceptance-readback.json`; inspected renders are `cherry-scratch.svg/.png` and `cherry-scratch-symbol.png`. The final symbol render moves SW901 away from U7 to avoid the first trial's overlap. Main-sheet placements and all connections are saved in `cherry-schematic-readback.json`. `erc-v2-complete.json` reports zero errors and warnings; component, wire, short and orphan checks also return zero. The schematic now includes all twelve performance controls. These checks do not replace PCB DRC or power qualification.
