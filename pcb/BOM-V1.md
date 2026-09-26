# BOM V1

La BOM sorgente è [reports/bom-v1.csv](reports/bom-v1.csv). L’audit finale copre 67 componenti logici: nessun valore o footprint mancante e nessun MPN mancante sugli IC controllati.

| Ref | Parte | MPN / nota |
|---|---|---|
| U1 | ESP32-S3-WROOM-1 N16R8 | ESP32-S3-WROOM-1-N16R8 |
| U2 | Protezione USB | USBLC6-2SC6 |
| U3 | LDO 3.3 V | LD1117S33TR |
| U4 | ADC 12 bit, 8 canali | MCP3208-CI/SL |
| U5 | Expander I/O I²C | MCP23017-E/SO |
| U6 | DAC I²S | Adafruit 6421 |
| J1 | USB-C receptacle | GCT USB4085-GF-A |
| J2 | RCA stereo | Switchcraft PJRAS2X1S02X |
| RV1–RV5 | Potenziometri 10 kΩ B | Alps RK09L, variante da confermare |
| RV6 | MASTER rear-facing 10 kΩ B | Bourns PTV111-3415A-B103 |
| SW1–SW4 | Pulsanti panel | E-Switch PB300STQ |
| SW5–SW6 | RESET / BOOT | Omron B3F-1000 o equivalente footprint |
| SW7 | Encoder con push | Alps EC11E09244AQ |

## DNP

- C16: 1 nF, collegamento opzionale CHASSIS–GND.
- R17: 0 Ω, alternativa opzionale CHASSIS–GND.

Non montare contemporaneamente C16 e R17 senza una decisione EMC esplicita.

## Parti provvisorie

- I modelli 3D generati per U6, RV6 e J2 sono dimensionali, non cosmetici.
- L’esatto RK09L, altezza boccola, dado e lunghezza albero vanno confermati.
- PB300STQ, keycap e apertura top vanno verificati sul campione.
- Header e altezza finale del modulo Adafruit 6421 non sono fissati.
- Questa BOM non è una release di produzione.
