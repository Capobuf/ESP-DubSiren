# Piani GND e vie di cucitura sul candidato recuperato — 7 novembre 2026

PCB: [ESP-DubSiren-recovered-diagnostic.kicad_pcb](../../kicad/ESP-DubSiren-recovered-diagnostic.kicad_pcb).
Il PCB attivo `ESP-DubSiren.kicad_pcb` resta invariato e senza routing.
Modifiche eseguite dal vivo in KiCad 10.0.6 tramite Konnect MCP (IPC), poi salvate.

**Routing non rifatto.** Le 820 piste e le 67 vie dell'autorouting recuperato restano
invariate: completano tutte le connessioni e le vie rispettano le netclass
(Power 0,7/0,3 mm su +3V3, +5V, VBUS; 0,6/0,3 mm sulle altre reti).
Il punto debole era la massa: solo piste da 0,127–0,2 mm e 8 vie, senza piano.

| Modifica | Dettaglio |
|---|---|
| Zona `GND F.Cu` | Tutta la scheda, clearance 0,2 mm, larghezza minima 0,2 mm, raccordi termici, isole rimosse |
| Zona `GND B.Cu` | Uguale; rispetta i keepout M3 e l'esclusione metallica sotto U2 |
| 24 vie GND locali | Una accanto a ogni pad GND SMD raggiungibile entro 2,5 mm: U2 PGND/EP, C1–C11, U1, U3, U7, ecc. |
| 85 vie GND a griglia | Passo 8 mm, solo dove la via cade nel rame GND su entrambi i lati |
| 1 pista GND B.Cu | 0,5 × 2 mm da RV3.1 verso il piano, per il raccordo termico che aveva 1 raggio su 2 |

Tutte le nuove vie sono 0,6/0,3 mm (netclass Default). I centri sono stati scelti a
≥ 0,55 mm dal bordo del riempimento su entrambi i lati e ≥ 0,65 mm dal rame di pad,
piste e vie esistenti (bordo pista da bordo via ≥ 0,35 mm);
elenco in [stitching-vias.json](stitching-vias.json). Totale vie: **176**.

U1 è un WROOM-1**U** (antenna esterna U.FL): il piano sotto il modulo non interferisce
con un'antenna su PCB.

| DRC finale | Esito |
|---|---|
| Connessioni aperte | **0** |
| Errori | **18**, tutti U6 sotto U5 (courtyard), lasciati aperti per decisione dell'utente |
| Avvisi | 92 geometrici/libreria + 68 parità, gli stessi della baseline |
| Nuove violazioni da piani e vie | **0** |

Evidenze: [DRC](drc.json), [F.Cu](F.Cu.png), [B.Cu](B.Cu.png).

**Restano aperti:** lo stack meccanico U5/U6 (18 errori); i restringimenti da 0,4 mm
di +5V/+3V3 sui pin di U2, imposti dal passo 0,65 mm e lunghi circa 0,66 mm ciascuno;
D− USB a 0,45 mm, non controllata in impedenza (USB full-speed); gli avvisi di
libreria, serigrafia e parità (campi `DesignStatus`/`MPN` mancanti, H1–H4 senza simbolo).
