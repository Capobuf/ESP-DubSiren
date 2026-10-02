# CHERRY MX2A native footprint acceptance audit

Status: **Not accepted for MX2A-11NW; SW1-SW4 remain absent.** This is a saved library comparison, not a claim that MX and MX2A mounting patterns differ. Exact manufacturer evidence has not established that they are identical.

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

Two electrical symbol pins and two electrical pads would be sufficient for an unpolarized SPST switch; three NPTH locating holes are mechanical. Swapping equivalent electrical contact numbers would not itself change function. The exact MX2A component-side contact positions, central-hole diameter, locating-peg positions/diameters and body seating projection still require an authoritative drawing or explicit manufacturer mounting-equivalence statement. The current MX2A sheet's 15.63 mm body envelope also requires a corrected body/courtyard representation; the native 12.70 mm F.Fab box is not adequate evidence of MX2A body clearance.

The user supplied [sszczep/kicad-libraries](https://github.com/sszczep/kicad-libraries). Parent inspection at commit `7f4d13488a3a255c5a3ec4e45466822d0e9351a6` found a generator referring to the old MX-series manufacturer drawing mirrored by SparkFun. Its actuator-centered electrical positions match the native geometry with reversed contact numbering; locating-hole diameter is 1.75 mm rather than native 1.70 mm, and its 15.6 mm F.Fab body is more representative. This is useful comparison evidence but does not independently establish MX2A-11NW mounting equivalence. See [MX2A source search](MX2A-SOURCE-SEARCH.md) for exact provenance, inspected documents and remaining source request.

No generic switch or unverified custom footprint has been substituted. This is the sole remaining control footprint acceptance gap; the seven Bourns pots and Bourns encoder are present in the saved schematic. A clean ERC cannot detect these four deliberately absent switches.
