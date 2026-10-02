# V2 authoritative sources

Reviewed 2026-10-02. Manufacturer documents take precedence over distributor
descriptions and generic library names. A saved manufacturer document obtained
from a distributor is identified as a mirror, not independent engineering proof.
Acceptance results and unresolved geometry are recorded separately in reports.

## Electrical

| Part / subject | Authority and document | Local evidence |
|---|---|---|
| ESP32-S3-WROOM-1U-N16R8 | [Espressif module datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf), v1.8; pinout, memory reservations, power and land pattern | `../components-documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf` |
| ESP32 implementation | [Espressif hardware guidelines](https://documentation.espressif.com/esp-hardware-design-guidelines/en/latest/esp32s3/index.html), power/reset, ADC and USB | `../components-documentation/esp-hardware-design-guidelines-en-master-esp32s3.pdf` |
| MPM3612GLQ-33-Z | [MPS MPM3612-33 datasheet](https://www.monolithicpower.com/en/documentview/productdocument/index/version/2/document_type/Datasheet/lang/en/sku/MPM3612GLQ-33/document_id/11833/), Rev 1.0, particularly pp3-4 and15-19 | `../components-documentation/MPM3612GLQ-33.pdf` |
| Toshiba 74HC4051D | [Toshiba manufacturer product page](https://toshiba.semicon-storage.com/us/semiconductor/product/general-purpose-logic-ics/detail.74HC4051D.html), datasheet Rev5.0 | `../components-documentation/74HC4051D_datasheet_en_20201203.pdf` |
| SLKOR USBLC6-2SC6 | [SLKOR datasheet](https://www.slkoric.com/upload/file/1782371251_7008.PDF), Rev1,2018-11-21 | `sources/SLKOR-USBLC6-2SC6.pdf` |
| PANJIT SMF5.0A_R1_00001 | [PANJIT SMF5.0A series](https://www.panjit.com.cn/upload/datasheet/SMF5.0A_SERIES.pdf), unidirectional polarity and SOD-123FL | Physical pin-map/electrical acceptance reports |
| Bel 0ZCG0110CF2B | Bel 0ZCG series manufacturer drawing and hold/trip curves | `../components-documentation/ds-cp-0zcg-series.pdf` |
| Samsung CL21A226MPCLRNC | [Samsung exact product family](https://product.samsungsem.com/mlcc/CL21A226MPCLRN.do), 22uF DC-bias data | `sources/Samsung-CL21A226MPCLRNC-DCBias.json`; `reports/samsung-22uF-dc-bias.csv` |
| Samsung CL10B105MO8NNWC | [Samsung exact product family](https://product.samsungsem.com/mlcc/CL10B105MO8NNW.do), includes packaging suffix C | Manufacturer page, 1uF/16V/X7R/0603 |
| Samsung CL10B104KO8NNNC | [Samsung exact product family](https://product.samsungsem.com/mlcc/CL10B104KO8NNN.do), includes packaging suffix C | Manufacturer page, 100nF/16V/X7R/0603 |
| Yageo AC0603FR-075K1L | [Yageo exact specsheet](https://www.yageogroup.com/component-documentation/download/specsheet/AC0603FR-075K1L) | 5.1kohm/1%/0603 |
| Yageo RC0603JR-0722RL | [Yageo exact specsheet](https://www.yageogroup.com/component-documentation/download/specsheet/RC0603JR-0722RL) | 22ohm/5%/0603 |
| Yageo RC0603JR-0710KL | [Yageo exact specsheet](https://www.yageogroup.com/component-documentation/download/specsheet/RC0603JR-0710KL) | 10kohm/5%/0603 |
| Yageo RC0603FR-074K7L | [Yageo exact specsheet](https://www.yageogroup.com/component-documentation/download/specsheet/RC0603FR-074K7L) | `sources/Yageo-RC0603FR-074K7L.pdf`; 4.7kohm/1%/0603 |
| NIC NRC06J100TRF | [NIC exact specification page](https://www.niccomp.com/quickBUILDER/qbpart.php?pNumber=NRC06J100TRF) | 10ohm/5%/0603 |

The ST USBLC6 documents already in the historical repository are not authority
for the purchased SLKOR part. The MPS evaluation-board EMI circuit is reference
material, not a reason to add its optional filter network.

## Breakouts, controls and connectors

| Part | Authority | Stored source / limitation |
|---|---|---|
| Adafruit 5393 | [Official PCB repository](https://github.com/adafruit/Adafruit-1.47in-320x172-Round-Rectangle-TFT-PCB), [guide](https://learn.adafruit.com/adafruit-1-47-round-rectangle-tft-display?view=all) | `sources/adafruit-5393`, commit `73022f63a4b82117093ff5c250c119b6720005df`, original license retained |
| Adafruit 6250 | [Official PCB repository](https://github.com/adafruit/Adafruit-PCM510x-I2S-DAC-PCB), [guide](https://learn.adafruit.com/adafruit-pcm510x-i2s-dac?view=all) | `sources/adafruit-6250`, commit `2edfe9e71b59e6acc73d0ab2b2f796149146ce74`, original license retained |
| Bourns PTV111 variants | [Current PTV/PTT datasheet](https://bourns.com/docs/product-datasheets/ptvptt.pdf), revision 09/23, pp2,5 | `sources/Bourns-PTV-PTT.pdf`; separate PTV111-3 / PTV111-1 footprints accepted with the user's resolved ordering/view interpretation; `reports/CONTROLS-POWER-PHYSICAL-PIN-MAPS.md` |
| Bourns PEC11R-4120F-S0018 | [Bourns PEC11R mechanical and ordering drawings](https://www.bourns.com/docs/product-datasheets/pec11r.pdf) | `../components-documentation/PEC11R.pdf`; exact five-lead push-switch footprint accepted; same physical pin-map report |
| CHERRY MX2A-11NW | [CHERRY product downloads](https://www.cherry.de/en-gb/product/mx2a-black/downloads), [current English datasheet](https://pim.cherry-world.net/datasheet/23002?locale=en_GB) | `sources/Cherry-MX2A-Black.pdf`; no dimensioned PCB pattern in retrieved sheet |
| CHERRY older MX2A sheet | [Manufacturer paper republished by JW Shop](https://www.jw-shop.com/pdf/spec-MX2A.pdf), exact Black variant pp5-6 | `sources/Cherry-MX2A-2024-republished.pdf`; also lacks mechanical pattern |
| Same Sky RCJ-2123 | [RCJ-21 datasheet](https://www.sameskydevices.com/product/resource/rcj-21.pdf), Rev1.06,2026-09-01,p2 | `../components-documentation/rcj_21.pdf`; explicit TOP VIEW |
| RCJ-2123 STEP | [Manufacturer CAD service](https://www.sameskydevices.com/product/resource/3dmodel/rcj-21-series/rcj-2123), [terms](https://www.sameskydevices.com/cad-model-terms) | Original and aligned STEP under `3dmodels`; alignment script under `sources` |
| Same Sky SAK-023 | [Manufacturer drawing](https://www.sameskydevices.com/product/resource/sak-023.pdf),2025-07-14 | `sources/SameSky-SAK-023.pdf` |
| Cliff CL170805B | [Cliff K85M drawing via DigiKey](https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/6166/Knob%20K85M.pdf),2022-03 | `sources/Cliff-K85M.pdf`; encoder knob, 14 mm diameter / 17.8 mm height; SAK-023 is MASTER |
| Davies Molding 1100 | [Manufacturer product series](https://daviesmolding.com/catalog/series/knobs/control/1100/); [manufacturer catalog p24 mirror](https://app.mectronic.com/dsheet/1145_Davies-Molding-LLC.pdf) | `sources/Davies-Catalog-Instrument-Knobs.pdf`; exact drawing server unavailable |
| GCT USB4085-GF-A | [GCT drawing RevB via Y-IC](https://az.y-ic.com/datasheet/e9/USB4085-GF-A.pdf),2023-02-28; [GCT PCN20230426 via DigiKey](https://media.digikey.com/pdf/PCNs/GCT/USB4085%20B%20PCN%2020230426.pdf) states footprint unchanged | `sources/GCT-USB4085-RevB.pdf`, `sources/GCT-USB4085-PCN20230426.pdf`; manufacturer papers retrieved via mirrors |
| PTS636SM43SMTR LFS | [C&K/Littelfuse PTS636 datasheet](https://www.ckswitches.com/media/2779/pts636.pdf), MI2026-09-02 | `sources/CK-PTS636.pdf`; exact SM43 gull-wing variant |

The Adafruit repositories contain board/schematic data but no complete STEP
assembly in the inspected revisions. Source geometry extraction and footprint
request scripts are retained under `sources`; they do not establish enclosure
fit or breakout stack heights.

## Additional power and mechanical evidence

| Subject | Authority | Saved evidence / interpretation |
|---|---|---|
| TPS22918DBVR | [TI datasheet SLVSD76C](https://www.ti.com/lit/ds/symlink/tps22918.pdf) | `sources/TI-TPS22918.pdf`; input slew switch justified by DAC breakout's direct 10 uF input in addition to buck CIN |
| CT capacitor | [Murata GRM1885C1H103JA01 reference sheet](https://search.murata.co.jp/Ceramy/image/img/A01X/G101/ENG/GRM1885C1H103JA01-01A.pdf) | `sources/Murata-GRM1885C1H103JA01.pdf`; 10 nF C0G, 50 V, 5%, 0603 |
| DAC dropout | [Diodes AP2127 datasheet](https://www.diodes.com/datasheet/download/AP2127.pdf) | Actual regulator in official Adafruit Eagle schematic; conservative dropout corner prevents accepting 3.3 V VIN solely from typical curves |
| USB inrush | [USB-IF electrical test material](https://compliance.usb.org/index.asp?UpdateFile=Electrical) | `reports/USB-POWER-DECISION.md`; controlled ramp calculation does not replace complete startup-waveform measurement or firmware power policy |
| Hammond 29830PSLA | [Hammond drawing](https://www.hammfg.com/files/parts/pdf/29830PSLA.pdf) and original `../3dmodels/Hammond_29830PSLA.stp` | `reports/hammond-stack-feasibility.json` and `mechanical/hammond-stack-feasibility.FCStd`; 29.70 mm internal depth, 30.60 mm minimum required stack |

The Hammond is rejected for a direct single planar PCB. The 125 x 140 x 42 mm
parametric replacement is a study, not an accepted complete assembly. Its sources
and reproducible generators are recorded in `MECHANICAL-V2.md`.

CHERRY documentation was searched beyond the current datasheet: older exact
MX2A paper, exact-part distributor links, the current manufacturer catalog and
explicit mounting-equivalence references. See `reports/MX2A-SOURCE-SEARCH.md`
for URLs, failed redirects, the historical MX drawing limitation and the exact
missing fabrication dimensions. The native KiCad footprint is not accepted merely
because its name says Cherry MX.

The user's additional [sszczep library](https://github.com/sszczep/kicad-libraries)
was inspected at `7f4d13488a3a255c5a3ec4e45466822d0e9351a6`. Its generator cites
the historical MX1A paper hosted by SparkFun. The current source-search and
native-footprint audit record its matching pin positions and 1.75 mm fixation
drills (native KiCad: 1.70 mm). It contains no exact MX2A manufacturer drawing.
