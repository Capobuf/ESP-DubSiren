# MX2A-11NW manufacturer drawing search, 2026-10-02

**RESOLVED in the continuation:** the user identified the official
[CHERRY Americas 2025-10-24 EOL / Replacement Notification](https://www.mouser.com/PCN/CHERRY_MX1A_11NW_EOL.pdf).
It names MX2A-11NW as the replacement for MX1A-11NW, with unchanged form, fit and
function. That explicitly closes the mounting-equivalence gap described below.
The [legacy manufacturer MX layout](https://cdn.sparkfun.com/datasheets/Components/Switches/MX%20Series.pdf)
and native KiCad `SW_Cherry_MX_1.00u_PCB` are now the accepted mounting basis.
See `CHERRY-NATIVE-FOOTPRINT-AUDIT.md` for the current acceptance result.
The historical investigation below is retained as provenance, not an active blocker.

The exact switch remains selected. This record concerns missing manufacturer
fabrication dimensions, not keycap tolerances or a proposed substitution.

## Documents checked

1. [Current CHERRY download center](https://www.cherry.de/en-gb/product/mx2a-black/downloads)
   exposes localized versions of the same PIM datasheet, not a mechanical drawing.
2. [Current English manufacturer datasheet](https://pim.cherry-world.net/datasheet/23002?locale=en_GB),
   saved as `../sources/Cherry-MX2A-Black.pdf`, has 16 pages. Pages 14-15 identify
   MX2A-11NW and specify 18.5 mm total height and 15.63 mm width/depth. No page
   provides a dimensioned PCB hole pattern.
3. Older manufacturer MX2A paper saved as
   `../sources/Cherry-MX2A-2024-republished.pdf`, obtained from
   [JW Shop's manufacturer-document mirror](https://www.jw-shop.com/pdf/spec-MX2A.pdf),
   identifies the five-pin fixation variant but lacks its hole dimensions.
4. Mouser's exact-part datasheet link targets
   `https://www.cherrymx.de/_Resources/Persistent/0/1/7/1/0171b6d59495ce36ce7bba26779a4a4ee7b510bc/Data_sheet_MX2A_Black.pdf`.
   On this date it redirects to CHERRY's gaming landing page rather than a PDF.
5. DigiKey's exact MX2A-11NW datasheet link targets an old MX2A **Red** PDF,
   `https://www.cherrymx.de/_Resources/Persistent/b/3/5/4/b354b7275b9afb1f1a1a69b45b317057791cd38a/Data_sheet_MX2A_Red.pdf`.
   That also redirects to the gaming page. It cannot establish Black geometry.
6. CHERRY's current [online catalogues](https://www.cherry.de/en-gb/online-catalogues)
   and linked 2026 Industrial/Security catalogue describe complete peripherals;
   no exact switch drilling drawing was found there.
7. The historical CHERRY 2010 switch catalogue, mirrored at
   `https://telcontar.net/KBK/Cherry/docs/Cherry%20switch%20catalogue%20%282010%29.pdf`,
   contains an MX drilling grid on printed page 70. It predates MX2A. A current
   manufacturer statement that the exact MX2A-11NW mounting geometry is identical
   was not found in the reviewed documents or targeted compatibility/PCN searches.
8. SparkFun's current Cherry MX breakout page states compatibility with several
   MX2A variants. This is useful corroboration of family compatibility, but is
   not CHERRY's exact-part drawing and does not meet the requested acceptance test.
9. The user supplied [sszczep/kicad-libraries](https://github.com/sszczep/kicad-libraries).
   Inspected revision `7f4d13488a3a255c5a3ec4e45466822d0e9351a6`, dated 2022-01-13.
   Its `footprints/generateCherryMX.py` links to the historical manufacturer
   [MX Series paper hosted by SparkFun](https://cdn.sparkfun.com/datasheets/Components/Switches/MX%20Series.pdf).
   That paper lists MX1A-11NW, not MX2A-11NW. No MX2A-specific source or drawing
   is present in the supplied library. It is useful corroboration, but does not
   supply the exact manufacturer evidence requested in section 7.

## Supplied library versus native KiCad

Compare component-side coordinates about the center locating post. The native
footprint origin is at electrical pad 1; translating its coordinates to the center
post makes the positions directly comparable. The two electrical pin numbers
are reversed between libraries; for the SPST contact this does not change the
switch function, but must be accounted for when assigning nets.

| Feature | Native KiCad SW_Cherry_MX_1.00u_PCB | Supplied CherryMX_1.00u_PCB |
|---|---|---|
| Electrical terminal positions | (+2.54,-5.08), (-3.81,-2.54) mm | Same positions, reversed pad numbers |
| Electrical drills | 1.50 mm | 1.50 mm |
| Center post | (0,0), 4.00 mm drill | Same |
| Fixation posts | (-5.08,0), (+5.08,0), 1.70 mm drills | Same coordinates, 1.75 mm drills |
| Fabrication body outline | 12.70 mm square | 15.60 mm square |
| Current CHERRY published width/depth | 15.63 mm | Same manufacturer part under review |

Thus the library strengthens the conventional MX coordinate evidence and exposes
a drill-size difference; it does not establish the current MX2A post diameter or
finished-hole tolerance. The native 13.20 mm courtyard also does not bound the
published 15.63 mm body. No library was silently substituted into the project.

## Previously missing evidence (now supplied by official equivalence)

Relative to the key/center-post axis, establish the X/Y coordinates and finished
PCB hole tolerances for both electrical terminals, both fixation posts and the
center post, plus the component-side viewing direction and PCB seating datum.
An exact-part drawing or an explicit manufacturer mounting-equivalence statement
linked to a dimensioned predecessor drawing would resolve this evidence gap.

The installed KiCad footprint was queried and inspected; readback is saved in
`cherry-native-footprint-readback.json` and the comparison is recorded in
`CHERRY-NATIVE-FOOTPRINT-AUDIT.md`. Matching a generic
MX footprint to an old MX drawing does not prove the requested exact-part match.
The known 15.63 mm body must also be reconciled with its fabrication outline and
courtyard. No arbitrary replacement footprint has been accepted.

The user's original section 7 explicitly required documenting the exact missing dimension
before stopping, and section 37 permits an intermediate stop when a manufacturer
dimension necessary for PCB fabrication cannot be established after exhausting
official documentation. The new official equivalence closes that source question; the resolved
Bourns orientation and purchased-knob assignments are not reopened here.
