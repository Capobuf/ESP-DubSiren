# PCB — riferimento tecnico

Stato di [ESP-DubSiren.kicad_pcb](../kicad/ESP-DubSiren.kicad_pcb) al
**8 novembre 2026**: 825 piste, 177 vie, piani GND su entrambe le facce.

## Regole e stackup

| Netclass | Reti | Pista | Via | Clearance |
|---|---|---|---|---|
| Default | tutte le altre | 0,2 mm | 0,6/0,3 mm | 0,15 mm |
| Power | +3V3, +5V, VBUS_RAW, VBUS_PROTECTED | 0,8 mm | 0,7/0,3 mm | 0,15 mm |
| USB | USB_DM/DP, *_MCU, *_CONN | 0,6 mm | 0,6/0,3 mm | 0,15 mm |

Minimi della scheda: clearance 0,127 mm, foro–rame 0,25 mm, foro–foro 0,25 mm,
rame–bordo 0,5 mm, foro passante 0,3 mm. Piste e spazi restano sopra il minimo
JLCPCB per 2 strati con rame 1 oz, 0,10/0,10 mm
([capacità JLCPCB](https://jlcpcb.com/capabilities/pcb-capabilities/), consultate il 31/10/2026).

Stackup (Board Setup → Physical Stackup): FR4 1,51 mm, rame 35 µm, maschera
**Black** su entrambe le facce, serigrafia **White**, finitura **ENIG**.
Il viewer 3D usa questi colori con "Use board stackup colors"
(`kicad-cli pcb render --use-board-stackup-colors`).

## DRC

| Controllo | Esito |
|---|---|
| Errori | **0**, più 18 esclusi |
| Connessioni aperte / parità schema | 0 / 0 |
| Avvisi | 11 serigrafie dei footprint oltre il bordo (il fabbricante le taglia), 1 serigrafia sul rame (pallino del pin 1 di U2 sul pad di C1, interno al footprint), 8 differenze di libreria volute (J2, TP1–TP7) |

Le 18 esclusioni, motivate nel [progetto](../kicad/ESP-DubSiren.kicad_pro), sono
i fori di U6 dentro il courtyard di U5: lo stack DAC dietro display è voluto.

## Produzione

Export dell'8/11/2026 con KiCad 10.0.6, da caricare così com'è:
[ESP-DubSiren-JLCPCB-2026-11-08.zip](../fab/ESP-DubSiren-JLCPCB-2026-11-08.zip).

- Gerber RS-274X X2 con estensioni Protel: F/B.Cu, F/B.Mask, F/B.Silkscreen,
  Edge.Cuts (`.gm1`). Pasta non inclusa: serve solo per stencil o assemblaggio.
- Forature Excellon in mm, origine assoluta, formato decimale, asole in modo
  alternativo; PTH e NPTH separati, con mappe PDF nella cartella dell'export.
- Fori: 177 vie da 0,3 mm; PTH 16 × 0,4, 4 asole 0,6 (guscio J1), 25 × 1,0,
  5 × 1,05, 28 × 1,1, 12 × 1,5; NPTH 8 × 1,7, 2 × 2,0, 8 × 2,5, 1 × 3,0,
  4 × 3,2, 4 × 4,0. Contorno unico 86 × 126 mm con 16 asole fresate (pot, encoder).
- Riempimento delle zone verificato prima dell'export: identico a quello salvato.

Controllo aggiuntivo con i minimi JLCPCB per 2 strati
([capacità](https://jlcpcb.com/capabilities/pcb-capabilities/), consultate l'8/11/2026):
foro PTH–rame 0,28 mm, NPTH–rame 0,2 mm, foro–foro 0,45 mm, testo serigrafia
≥1 mm, spaziatura pad 0,13 mm per maschera nera. Tutti rispettati, tranne:

| Scostamento | Motivo |
|---|---|
| Anello dei fori di J1 0,15 mm (JLCPCB: minimo 0,18) | Fori Ø0,40 e pad 0,70 come raccomandato da [GCT](GCT-USB4085-RevB.pdf); con passo 0,85 mm non si può allargare |
| Linee di serigrafia 0,10–0,12 mm e sigle con tratto 0,125 mm (JLCPCB: ≥0,15) | Solo estetico: le scritte possono risultare sottili |

Nell'ordine: 2 strati, 1,6 mm, rame 1 oz, maschera nera, serigrafia bianca, ENIG.

## Alimentazione e USB

| Punto | Valutazione |
|---|---|
| Corrente | U2 MPM3612 eroga al massimo 1 A ([datasheet](MPM3612GLQ-33.pdf), p.1); F1 PTC 0ZCG0110, 1,1 A di mantenimento |
| Restringimenti 0,4 mm +5V/+3V3 sui pin di U2 | Per 1,1 A con rame 1 oz esterno e ΔT 10 °C (IPC-2221) bastano circa 0,34 mm; i tratti sono lunghi 0,10–0,66 mm. Più larghi non passano con il passo 0,65 mm |
| PGND di U2 | Pin 3 ed EP a 0,4 mm, verso C1 a 0,2 mm (MPS p.17: percorsi corti e larghi); in parallelo vie e piano GND |
| USB D− 0,45 mm / D+ 0,6 mm | USB 2.0 full-speed 12 Mbps ([ESP32-S3](esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)): con tratte di pochi centimetri impedenza e simmetria non sono critiche; ritorno sul piano GND |
| U2 | Area di divieto per il metallo NC sotto il modulo |
| Ripple, rumore audio, I²S | Da misurare sul prototipo |

## Massa

Zone GND su F.Cu e B.Cu, su tutta la scheda (clearance 0,2 mm, raccordi termici,
isole rimosse), fuori dalle riserve M3 e dall'area di divieto di U2. Le vie di
cucitura sono una accanto a ogni pad GND SMD raggiungibile e una griglia di passo
8 mm dove la via cade nel rame GND di entrambi i lati.

## Criteri per la saldatura a mano

Applicati a tutta la scheda:

- **Vie di segnale:** almeno 0,5 mm dai pad di altre reti, SMD e THT, e
  0,2 mm dai pad SMD della propria rete (niente via nel pad). Tutte le vie sono
  coperte dal solder mask (tented). Due non hanno spazio per arrivare a 0,5 mm:
  ENC_SW in (17,983; 57,996) a 0,468 mm da R15.2 (BTN1) e USB_CC1 in
  (42,987; 3,551) a 0,377 mm da J1.A7, fra D+ e D− sotto il connettore.
- **Pin 4 dei potenziometri:** è un pin dummy metallizzato, saldato dal lato B
  dopo i passivi. R8–R13 stanno ad almeno 1,58 mm dai pad dei pot.
- **Condensatori di U1:** C5, C6, C7 e C9 stanno ad almeno 1,075 mm dai pin di U1,
  la stessa distanza di R15–R21 sull'altro lato. Dei pad di U1 si salda la parte
  che sporge di 0,5 mm dal modulo.
- **Zona U2:** le coppie di pad sotto 1 mm sono sulla stessa rete (C1–U2 +5V,
  R1–U2 e C3/C4 +3V3, C4–C10 GND, C11–U7): un ponte non crea cortocircuiti.
  Restano R6–C7 a 0,86 mm e R14–F1 a 0,88 mm, su reti diverse.
- **U2 (LGA) e pad centrale di U1** non si saldano a punta: vedi
  [montaggio](assembly.md).
- **Serigrafia:** nessuna sigla sopra i pad o sovrapposta ad altre.

## Footprint locali

| Footprint | Nota |
|---|---|
| `Bourns_PTV111-3` (RV1–6) | Asole 2,0 × 2,1 mm, centri ±5,8 mm: 13,6 mm fra i bordi esterni ([PTV/PTT](../cad/sources/Bourns-PTV-PTT.pdf), p.2) |
| `Bourns_PTV111-1` (RV7) | Asole 2,0 × 2,1 mm, centri ±6,1 mm: 10,2 mm fra i bordi interni (p.2) |
| `Bourns_PEC11R-4120F-S0018` (SW7) | Asole 1,8 (X) × 2,6 (Y) mm, centri ±5,7 mm ([PEC11R](../cad/sources/PEC11R.pdf), p.2). Con le asole corrette resta solo il dente dell'aggancio a scatto sulla faccia opposta |
| `Espressif_ESP32-S3-WROOM-1U` (U1) | Land pattern della fig. 11-2 del datasheet v1.8 (p.46): pad 1,5 × 0,9 mm che sporgono 0,5 mm su tre lati. Contorno Fab sul corpo 18 × 19,2 mm, courtyard su corpo e pad + 0,25 mm |
| `Adafruit_5393_DisplayFace`, `Adafruit_6250_ComponentFace` | Breakout come footprint meccanici; U5 senza modello 3D, perché Adafruit non pubblica uno STEP |

Provenienza dei modelli 3D e comandi per gli STEP per gruppo:
[cad/3dmodels](../cad/3dmodels/README.md).
