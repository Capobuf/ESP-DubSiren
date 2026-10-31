# Pulizia serigrafia frontale — 30 ottobre 2026

**Esito parziale**, con uso esclusivo di Konnect confermato dall'utente.
PCB salvato: `hardware/kicad/ESP-DubSiren.kicad_pcb`.

Consegna parziale accettata dall'utente il 30 ottobre 2026 («per me ok»).
Issue #36 chiusa riportando i limiti residui. Il ramo dedicato, non ancora
integrato in `main`, è conservato insieme all'archivio `codex/issue-14-jack-fit`.

Rimosse tramite IPC le 19 scritte autonome su F.SilkS: PITCH, RATE,
ECHO LEVEL, ECHO TIME, ECHO FEEDBACK, ECHO FILTER, SELECT, PUSH: MENU,
BTN1–BTN4, TRIGGER, HOLD, MANUAL MOD, ECHO CUT, USB-C, MONO e ESP-DubSiren.
MASTER è conservato senza variazioni. Non sono state aggiunte scritte o grafiche.
Nessuna etichetta funzionale programmabile rimane nell'export frontale.

## Anteprima e lavoro residuo

[Anteprima attuale](F-preview.png) / [SVG](F-preview.svg), ricavati
dall'[export nativo](F-native.svg) del PCB salvato con F.SilkS e Edge.Cuts.
Il ritaglio esclude il cartiglio; nero e bianco sono solo colori di anteprima.
Le aperture dei fori sono quelle emesse dal plot KiCad. Non è una vista del rame
né una nuova grafica da stampare. [Anteprima precedente](F-before-preview.png).

| Richiesta | Esito verificato |
|---|---|
| Rimuovere le funzioni programmabili e USB-C/MONO/nome | Completato, 19 testi eliminati |
| Conservare MASTER e RV1–RV6/SW7 | Completato, contenuto e posizione invariati |
| SW1–SW4 dentro i rispettivi quadrati | **Da fare**: rimangono sopra il bordo superiore dei quadrati |
| Nascondere H1–H4 | **Da fare**: restano visibili |
| Conservare soltanto le scritte consentite | **Da fare**: oltre a H1–H4 resta anche U5 |
| Eliminare le informazioni descrittive inferiori dalla serigrafia | Dopo la rimozione del nome, nessuna è presente nel plot F.SilkS; le annotazioni su altri layer restano invariate |
| Superficie libera e leggibilità finale | Migliorata, ma verifica finale incompleta per i campi e le sovrapposizioni preesistenti |

Konnect 0.12.1 non espone modifica di visibilità/posizione dei campi dei
footprint. `delete_graphics` esclude espressamente i loro elementi interni;
`edit_component` espone soltanto il valore. I riferimenti non sono stati
eliminati dal progetto, duplicati con nuovi testi o spostati su altri layer.
I contorni esistenti dei footprint e il retro sono invariati.

## Riscontri

[Verifica dettagliata](verification.json), [DRC](drc.json),
GenCAD [prima](before.cad) / [dopo](after.cad).

- Diff del PCB: soltanto 19 blocchi `gr_text` F.SilkS eliminati,
  209 righe rimosse e nessuna aggiunta. Nessun cambiamento a footprint,
  riferimenti, pad, fori, bordo, schema o layer funzionali.
- GenCAD: BOARD, PADS, PADSTACKS, ARTWORKS, SHAPES, COMPONENTS, SIGNALS,
  TRACKS e ROUTES identici. DEVICES differisce per il PART attribuito
  all'impronta condivisa dei resistori (10 / 5.1k); il diff del PCB
  conferma che nessun valore o footprint è stato modificato.
- DRC sul file salvato: stessi 115 warning geometrici della consegna #36,
  verificati anche per regola e UUID degli elementi: 64 differenze libreria,
  27 silk/rame, 13 sovrapposizioni silk, 11 silk/bordo.
  Restano 160 connessioni non instradate e 68 segnalazioni di parità schema.
  Non è una dichiarazione di prontezza alla fabbricazione.

La base richiesta resta incompleta finché non sarà disponibile una modifica
supportata dei campi dei footprint. Nessun intervento elettronico o meccanico
è stato effettuato.
