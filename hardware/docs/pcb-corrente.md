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
