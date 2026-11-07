# PCB corrente: promozione e verifiche residue (7 novembre 2026)

Il candidato [recuperato](autorouting-recovery/report.md), con i
[piani GND](gnd-stitching/report.md) e lo [stack U5/U6](u5-u6-stack.md), è stato
promosso a [ESP-DubSiren.kicad_pcb](../kicad/ESP-DubSiren.kicad_pcb).
Prima della copia è stato verificato che il PCB recuperato differisse dal principale
solo per rame e campi footprint, e il `.kicad_pro` solo per le 18 esclusioni U5/U6
e il nome nei metadati. Lo schema è identico. Le esclusioni sono state riportate
nel [progetto principale](../kicad/ESP-DubSiren.kicad_pro).

L'aggiornamento PCB da schema (F8), fatto dall'utente, ha aggiunto ai footprint i
campi `DesignStatus`, `MPN` e `Description` senza toccare rame, posizioni, pad o reti.

| DRC `ESP-DubSiren.kicad_pcb` | Esito |
|---|---|
| Errori | **0** (18 U5/U6 esclusi con motivazione) |
| Connessioni aperte | **0** |
| Parità schema/PCB | **0** |
| Avvisi | 64 differenze di libreria (footprint voluti, #34/#35), 28 serigrafia (#36) |
| Piste / vie | 821 / 176 |

## Alimentazione, USB e portata

| Punto | Valutazione | Azione |
|---|---|---|
| Limite di corrente | U2 MPM3612 eroga al massimo 1 A ([datasheet](MPM3612GLQ-33.pdf), p.1); F1 è un PTC 0ZCG0110, 1,1 A di mantenimento | Riferimento per le piste |
| Restringimenti 0,4 mm +5V/+3V3 su U2 | IPC-2221, rame 1 oz esterno, ΔT 10 °C: per 1,1 A servono ≈0,34 mm. I tratti da 0,4 mm sono lunghi 0,10–0,66 mm (< 1 mΩ). Più larghi non passano: il passo 0,65 mm lascerebbe meno di 0,15 mm dal pin vicino | Lasciati |
| PGND U2 (pin 3, EP, verso C1) | MPS p.17 chiede percorsi PGND corti e larghi. Il collegamento era 0,127–0,15 mm | 4 tratti pin3/EP portati a **0,4 mm**, 2 tratti verso C1 a **0,2 mm**, larghezza massima a 0,15 mm dagli altri potenziali. I due tratti da 0,127 mm vicino a C1 non si allargano senza rifare il percorso: in parallelo ci sono la via accanto a C1, quella accanto al pin 3 e il piano GND su F.Cu |
| USB D− 0,45 mm / D+ 0,6 mm | ESP32-S3: USB 2.0 **full-speed** 12 Mbps ([datasheet](esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)). Con tratte di pochi centimetri l'impedenza controllata e la differenza di larghezza non sono determinanti; il ritorno ora è sul piano GND | Nessuna modifica |
| Ripple, rumore audio, I²S | Non verificabili dal PCB | Da misurare sul prototipo |

Le copie `-recovered-diagnostic` e `-mixed-diagnostic` restano in `hardware/kicad`
come storico; il PCB di riferimento è `ESP-DubSiren.kicad_pcb`.

## Modelli 3D (7 novembre 2026)

Tutti i footprint hanno tipo SMD/THT e modello 3D, tranne U5 (nessuno STEP
Adafruit, spessori non documentati), TP1–TP7 e H1–H4. Provenienza, quote e
comandi per gli STEP per gruppo: [cad/3dmodels](../cad/3dmodels/README.md).
DRC dopo l'assegnazione: 0 errori non esclusi, 0 connessioni aperte, 0 parità;
differenze di libreria 8 (J2, TP1–TP7).

## Asole di fissaggio Bourns

**Corrette il 7 novembre 2026.** Il confronto dei modelli Bourns con il corpo
della scheda e con i disegni "Recommended PCB layout" mostrava tre footprint con
asole non conformi, ora allineate nella libreria locale e sul PCB:

| Footprint (riferimenti) | Disegno Bourns | Prima | Ora |
|---|---|---|---|
| `Bourns_PTV111-3` (RV1–RV6) | 13,6 mm **fra i bordi esterni**, asola 2,0×2,1 ([PTV/PTT](../cad/sources/Bourns-PTV-PTT.pdf), p.2) | centri ±6,8 (X 5,8–7,8) | centri ±5,8 (X 4,8–6,8) |
| `Bourns_PTV111-1` (RV7) | 10,2 mm **fra i bordi interni**, asola 2,0×2,1 (p.2) | centri ±5,1 (X 4,1–6,1) | centri ±6,1 (X 5,1–7,1) |
| `Bourns_PEC11R-4120F-S0018` (SW7) | 13,2 mm fra i bordi esterni, asola **1,8 (X) × 2,6 (Y)** ([PEC11R](../cad/sources/PEC11R.pdf), p.2) | 2,6 (X) × 1,8 (Y), centri ±6,6 | 1,8×2,6, centri ±5,7 |

Con le asole precedenti le gambe dei modelli Bourns attraversavano il materiale per
tutto lo spessore (PTV111-3: 0,43 mm³ per gamba; PEC11R: gamba lunga 2,5 mm contro
un'asola di 1,8). Ora resta solo il dente dell'aggancio a scatto sulla faccia
opposta della scheda (0,14–0,18 mm³ per gamba), cioè la ritenzione prevista.

Lo spostamento ha richiesto deviazioni locali di cinque piste, con le stesse
larghezze: MASTER (F.Cu, a destra dell'asola sinistra di RV1, x=11,0), BTN1
(F.Cu, a sinistra dell'asola destra di RV4, x=19,0), USB_DP_CONN (B.Cu, 0,6 mm,
diagonale spostata sotto l'asola sinistra di RV2; si allontana da USB_DM_CONN),
POT7_TOP (B.Cu, sopra l'asola destra di RV7) e ENC_B (B.Cu, y=48,5 sotto l'asola
sinistra di SW7). Modifiche applicate tramite Konnect MCP sul PCB aperto in KiCad.

| DRC dopo la correzione | Esito |
|---|---|
| Errori | **0** (18 U5/U6 esclusi) |
| Connessioni aperte / parità | **0 / 0** |
| Avvisi | 28 serigrafia (#36), 8 differenze di libreria (J2, TP1–TP7) |
