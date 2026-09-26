# ESP-DubSiren PCB V1

Questa cartella contiene la revisione V1 della mainboard per Hammond 29830PSLA. Lo schema è completo e verificato; il layout è una **revisione pre-produzione non autorizzata alla fabbricazione**.

## Stato verificato

- KiCad 10.0.6, schema e PCB a 2 layer.
- Schema: 72 simboli, 69 net nominate, ERC finale 0 errori / 0 warning.
- BOM: 67 componenti logici; audit BOM 0 issues.
- PCB: 100.00 × 74.00 mm, angoli R6.00 mm, 4 fori M3.
- Pre-flight JLCPCB: **NOT READY**.
- DRC: 212 errori, 291 warning/parity, 175 collegamenti non instradati, 503 segnalazioni complessive.
- Non sono stati generati Gerber né package di fabbricazione.

Il routing automatico non è stato accettabile. Il profilo con archi non è esportabile dal backend Specctra disponibile e l’API Konnect installata non espone la cancellazione dei via. Le piste del tentativo sono state rimosse, ma restano 106 via orfani. La scheda non deve essere ordinata.

## Architettura

- U1: ESP32-S3-WROOM-1-N16R8 diretto, non DevKit.
- J1: USB-C USB 2.0 device con Rd 5.1 kΩ, USBLC6-2SC6 e 22 Ω serie.
- U3: LD1117S33TR, +5 V → +3V3.
- U4: MCP3208, SPI, sei canali analogici usati e CH6/CH7 su test point.
- U5: MCP23017, I²C, pulsanti/encoder/mute e GPIO d’espansione.
- U6: Adafruit 6421 PCM5122, I²S + I²C, alimentato a +5 V.
- J2: RCA stereo Switchcraft PJRAS2X1S02X.
- J3: display 2×6; J4: espansione 2×10.
- Top: 5 potenziometri, encoder con push, 4 PB300STQ.
- Rear: MASTER, USB-C e RCA L/R.

Il part number MASTER richiesto inizialmente, PTV111-1415A-B103, non è coerente con il montaggio orizzontale sul rear panel. La V1 usa PTV111-3415A-B103; footprint, pannello e acquisto devono essere confermati sul campione.

## Orientamento meccanico

Il corpo profondo Hammond è la parte superiore. I controlli sono sul lato B e attraversano il top; l’elettronica è sul lato F verso il coperchio inferiore.

La PCB centrata lascia 1.415 mm per lato in X e 1.565 mm in Y rispetto al lid 102.83 × 77.13 mm; rispetto al vano corpo 104.71 × 79.41 mm lascia 2.355 e 2.705 mm. H2 è arretrato a (95,31) per evitare antenna/courtyard WROOM.

Stima Z non validata: parete-top → PCB 10–11 mm; con PCB 1.6 mm restano 17–18 mm nominali verso il fondo. Il DAC su header è stimato 8–10 mm sotto PCB, quindi margine teorico 7–10 mm prima di tolleranze, saldature e cavi.

## DRC finale

| Quantità | Regola |
|---:|---|
| 106 | via_dangling |
| 39 | silk_over_copper |
| 26 | lib_footprint_mismatch |
| 20 | silk_overlap |
| 13 | nonmirrored_text_on_back_layer |
| 12 | hole_to_hole |
| 12 | drill_out_of_range |
| 10 | pth_inside_courtyard |
| 5 | courtyards_overlap |
| 3 | clearance |
| 3 | hole_clearance |
| 3 | starved_thermal |
| 3 | silk_edge_clearance |
| 1 | npth_inside_courtyard |

Alle 256 violazioni sopra si aggiungono 175 unconnected e 72 segnalazioni di parità schema/footprint. I 12 fori da 0.20 mm sono i thermal-via del footprint ufficiale WROOM. Gli overlap maggiori coinvolgono WROOM/antenna, U6/RV5, U5/SW4 e l’area J2.

## File

- [BOM-V1.md](BOM-V1.md) e [reports/bom-v1.csv](reports/bom-v1.csv)
- [PINOUT-V1.md](PINOUT-V1.md)
- [MECHANICAL-V1.md](MECHANICAL-V1.md)
- [SOURCES-V1.md](SOURCES-V1.md)
- [erc-v1.json](erc-v1.json) e [drc-v1.json](drc-v1.json)
- disegni 1:1 in [mechanical](mechanical), immagini in [screenshots](screenshots), export in [outputs](outputs)

## Prima della prossima revisione

1. Rimuovere i 106 via orfani con un backend Konnect che supporti la cancellazione via o ricostruire il PCB.
2. Rivedere placement e dimensione scheda, poi instradare integralmente.
3. Portare DRC e unconnected a zero.
4. Misurare componenti meccanici, aggiornare footprint/pannelli e provare lo stack nel box.
5. Solo dopo generare Gerber, drill e package fab.
