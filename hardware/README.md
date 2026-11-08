# ESP-DubSiren — hardware

PCB desktop a vista, unico progetto KiCad attivo. Instradato, con DRC a 0 errori
(18 esclusioni volute per lo stack U5/U6), 0 connessioni aperte e schema allineato.
Non ancora ordinato: Gerber e forature non sono stati generati.

## File

| Cosa | Dove |
|---|---|
| Progetto, schema, PCB | [kicad/ESP-DubSiren.kicad_pro](kicad/ESP-DubSiren.kicad_pro), [schema](kicad/ESP-DubSiren.kicad_sch), [PCB](kicad/ESP-DubSiren.kicad_pcb) |
| Librerie locali | [footprint](kicad/footprints/), [simboli](kicad/symbols/) |
| Riferimento tecnico del PCB | [docs/pcb.md](docs/pcb.md) |
| Montaggio DIY | [docs/assembly.md](docs/assembly.md) |
| Distinta e acquisti | [docs/BOM.csv](docs/BOM.csv), [docs/acquisti.csv](docs/acquisti.csv) |
| Datasheet e disegni | [docs/sources.md](docs/sources.md), [cad/sources/](cad/sources/) |
| Modelli 3D | [cad/3dmodels/](cad/3dmodels/README.md), generati e assegnati con [tools/](tools/) |

## Decisioni in vigore

- **Scheda:** 86 × 126 mm, angoli R3, 2 strati, 1,6 mm. Nessuna custodia, staffa
  USB o stampa 3D obbligatoria; si usa orizzontale o leggermente inclinata.
- **Produzione:** JLCPCB, solder mask nero, finitura ENIG 1µ", serigrafia bianca.
  Sono già impostati nello stackup del PCB; vanno selezionati anche nell'ordine.
- **Fissaggi:** quattro fori M3 Ø3,2 NPTH in (5;−1), (81;−1), (5;115), (81;115),
  interassi 76 × 116 mm, con riserve 8 × 8 mm senza rame su entrambe le facce.
  Origine KiCad: bordo posteriore Y=−6, anteriore Y=120.
- **Fronte (F.Cu):** RV1–3 in alto, display U5 (Adafruit 5393) al centro con
  encoder SW7 a destra, RV4–6 sotto, quattro CHERRY SW1–4 in basso.
- **Bordo posteriore:** USB-C J1 (GCT USB4085), jack mono J2 (Neutrik NRJ4HF),
  MASTER RV7. RESET SW5 e BOOT SW6 sul bordo destro.
- **Retro (B.Cu):** tutti gli SMD; ESP32-S3-WROOM-1U-N16R8 (U1) a sinistra.
  Il DAC U6 (Adafruit 6250, senza jack 3,5 mm) sta dietro il display U5:
  header U5 da 2,50 mm, pin U6 tagliati a filo prima di montare U5. Eventuali
  adattamenti (pin più alti o simili) si fanno a mano in montaggio.
- **Uscita audio J2:** T → AUDIO_MONO (OUTL di U6), S → GND, TN/SN non collegati;
  OUTR di U6 non collegato. Il dado NRJ-NUT-B serve solo con un pannello.
- **Archivio Hammond:** lo studio della issue
  [#14](https://github.com/Capobuf/ESP-DubSiren/issues/14) è conservato nel branch
  `codex/issue-14-jack-fit`. Non va eliminato e non vincola il layout.

## Da verificare sul prototipo

- Stack U5/U6: altezza reale fra display e DAC.
- Fit di manopole, keycap e distanziali; stabilità degli appoggi.
- Ripple delle alimentazioni, rumore audio, I²S e display sull'hardware reale.
- Prima dell'ordine: export di Gerber e forature e controllo del PCB in KiCad.
