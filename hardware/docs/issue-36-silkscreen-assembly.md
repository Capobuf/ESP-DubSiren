# Issue #36 — serigrafia e assemblaggio DIY

**Integrazione approvata il 31 ottobre 2026:** il lavoro #36 e il checkpoint
locale successivo sono preservati nella consegna #38 autorizzata per push e
merge. I riferimenti seguenti al ramo non ancora integrato descrivono lo
stato storico del 30 ottobre, non un obbligo di conservarlo dopo la verifica
dell'integrazione.

**Aggiornamento, 30 ottobre 2026:** la successiva
[pulizia della serigrafia](silkscreen-cleanup/README.md) ha rimosso 19 delle
20 etichette frontali qui documentate, conservando MASTER. Le anteprime e le
verifiche seguenti descrivono la consegna precedente; lo stato attuale e i
campi dei footprint ancora da sistemare sono nella nuova revisione.

L'utente ha accettato la consegna parziale («per me ok»); la issue #36 è
chiusa con i limiti residui esplicitati. Il ramo dedicato resta conservato,
perché non è ancora integrato in `main`. La chiusura non equivale a una
validazione produttiva o al completamento dei campi non modificabili con Konnect.

Consegna **parziale**, 30 ottobre 2026, sul ramo
`codex/issue-36-silkscreen-assembly` e sull'unico PCB attivo. Il bordo resta
**86 × 126 mm R3**, senza routing. La guida è in [assembly.md](assembly.md).
Alla consegna iniziale la issue restava aperta: completamento serigrafie e
revisione dell'utente erano pendenti; vedere l'aggiornamento sopra.

## Anteprime dal PCB salvato

- [Confronto top/bottom](assembly-36/review.png) / [SVG](assembly-36/review.svg).
- [Top](assembly-36/F-preview.png) / [SVG](assembly-36/F-preview.svg): serigrafia
  effettivamente presente, comprese le 20 nuove etichette.
- [Top con inviluppi dei comandi](assembly-36/F-controls.png) /
  [SVG](assembly-36/F-controls.svg): etichette contro gonne Davies Ø19,84375,
  encoder Ø14,5, display e keycap provvisori 20×20. Le sagome sono sovrapposte
  al plot per la lettura; non sono nuovi footprint o altezze certificate.
- [Bottom specchiato](assembly-36/B-preview.png) / [SVG](assembly-36/B-preview.svg):
  serigrafia esistente, senza nuove scritte posteriori.
- Disegni di montaggio con layer Fab e pad: [top](assembly-36/F-assembly.png),
  [bottom specchiato](assembly-36/B-assembly.png). I riferimenti sotto i corpi
  nei layer Fab sono indicazioni di assemblaggio, non nuove stampe sulla PCB.
- Export Konnect originali: [F](assembly-36/F-native.svg),
  [B](assembly-36/B-native.svg), [F.Fab](assembly-36/F-assembly-native.svg),
  [B.Fab](assembly-36/B-assembly-native.svg).

Le preview ritagliano il plot nativo e specchiano l'intera vista inferiore;
il cartiglio dell'export è escluso. **Nero e bianco sono una simulazione grafica
della finitura richiesta**, non colori impostati nello stackup KiCad né un
ordine di fabbricazione. Il rame color oro è convenzionale e non seleziona una
finitura produttiva. Le immagini non mostrano un assieme 3D qualificato.

## Modifiche applicate

Solo testi di scheda su **F.SilkS**, carattere nativo KiCad, aggiunti tramite
Konnect e verificati dopo salvataggio:

| Gruppo | Testi | Posizione / criterio |
|---|---|---|
| Bordo posteriore | MASTER, USB-C, MONO | Leggibili dall'alto, fuori da pad e aree dei quattro fissaggi |
| Fila superiore | PITCH, RATE, ECHO LEVEL | Funzioni predefinite RV1–3; ECHO LEVEL più arretrato per evitare il foro del jack |
| Encoder | SELECT, PUSH: MENU | Sopra e sotto l'inviluppo della manopola SAK-023 |
| Fila inferiore | ECHO TIME, ECHO FEEDBACK, ECHO FILTER | Y=65 mm, nel corridoio fra display e gonne delle manopole; evitare anche i fissaggi passanti del DAC |
| Tasti | BTN1–4 e TRIGGER / HOLD / MANUAL MOD / ECHO CUT | Sotto la riserva keycap 20×20, fuori dalle riserve M3; nomi funzione riferiti al Classic predefinito |
| Nome | ESP-DubSiren | Centro del bordo anteriore, senza logo o decorazioni aggiunte |

Le dimensioni carattere sono 0,85–1,25 mm per etichette, 2,1 mm per il nome;
spessore nativo 0,15×dimensione (minimo 0,1275 mm sui testi più piccoli).
La leggibilità alla stampa e i minimi accettati dal futuro produttore vanno
confermati prima dei Gerber. Non sono state aggiunte scale numeriche o versi
di rotazione non provati. Pot e tasti restano configurabili: il display mostra
l'assegnazione attiva. MASTER è indipendente e non riassegnabile.

## Operazioni non completabili con i tool disponibili

L'utente ha richiesto di restare sui tool Konnect. Nessuna modifica manuale
dei file KiCad e nessun uso dell'interfaccia nativa per aggirare questi limiti.
Versione Konnect 0.12.1, KiCad 10.0.6; [riscontri e recupero](assembly-36/tool-limitations.json).

| Operazione pendente | Evidenza / lavoro ancora richiesto |
|---|---|
| Maschera nera e serigrafia bianca nel progetto | Il catalogo Konnect disponibile non espone colori dello stackup. Non disegnare nero su F.Mask/B.Mask: sono layer di aperture della maschera |
| Riferimenti, contorni e marker esistenti dei footprint | Non c'è un editor di campi/testi/linee delle istanze; `delete_graphics` esclude le grafiche interne ai footprint. Restano testi coperti dai corpi e warning della #35; spostarli/ripulirli prima della produzione |
| Nuove etichette posteriori | `add_board_text` su B.SilkS non specchia il testo: il DRC ha rilevato 13 `nonmirrored_text_on_back_layer`. Quei testi sono stati rimossi |
| Alternativa SVG posteriore | `import_svg_logo` ha dichiarato 530 poligoni, ma la rilettura IPC e il file salvato ne mostravano uno solo e la scala non corrispondeva. Anche quel risultato incompleto è stato rimosso |

Da aggiungere sul retro quando supportato: RESET/SW5, BOOT/SW6; valori dei
TP (+5V, +3V3, GND, ADC, MONO, TX, RX); pin 1/VIN dei due breakout e
identificazione del lato montaggio. La guida ne riporta già funzioni e verso.
Rivedere anche pin 1 di U1–4/U7, catodo D1 e piccoli marker dei fori/piedini:
non si dichiara completata la leggibilità della faccia di montaggio in base
ai soli contorni Fab.

## Verifica e limiti

Evidenze in [verification.json](assembly-36/verification.json),
[DRC salvato](assembly-36/drc.json), [GenCAD prima](assembly-36/before.cad),
[dopo](assembly-36/after.cad) e [BOM riesportata](assembly-36/schematic-bom.csv).

| Controllo | Esito |
|---|---|
| Modifiche semantiche agli oggetti preesistenti | Nessuna, confrontando anche UUID e geometrie; normalizzati ordine, `fill solid/yes`, angolo zero e numeri a precisione 0,000001 mm |
| Nuovi oggetti rimasti nel PCB | **20 testi F.SilkS**, nessuna grafica di prova sul retro |
| Placement, bordo, pad e reti esportati | Invariati; **237 pin**, sezioni COMPONENTS, BOARD, PADS, PADSTACKS, DEVICES e SIGNALS identiche in GenCAD |
| Forme GenCAD | Unico riordino di linee/archi di U6; geometrie preesistenti invariate nel confronto semantico |
| BOM contro schema riesportato | **64/64** riferimenti uguali per valore, footprint, MPN, produttore e DNP |
| DRC sui nuovi testi | **0** segnalazioni riferite ai nuovi UUID |
| DRC geometrico totale | Invariato dalla #35: **115 warning** = 64 differenze libreria + 27 silk/rame + 13 silk sovrapposte + 11 silk/bordo |
| Reti non instradate / parità schema | **160 / 68**, invariati; non è un PASS di fabbricazione |
| Routing / via / zone | Nessuna aggiunta; TRACKS e ROUTES esportate vuote, oggetti preesistenti invariati |
| Firmware e prove fisiche | Nessuna modifica; nessun upload, alimentazione o smoke test sull'assieme |

Il salvataggio nativo KiCad ha normalizzato la serializzazione del file,
rendendo il diff testuale ampio. Il confronto per oggetti e gli export
distinguono questa riscrittura dalle 20 aggiunte volute.

La guida contiene distinta elettronica, ferramenta facoltativa, due appoggi
commerciali dimensionati, orientamenti e sequenza di assemblaggio, cautele
5393/6250, accesso BOOT/RESET e controlli prima dell'accensione. Restano da
qualificare stack, fissaggi dei breakout, Hmax, keycap, manopole, cavi e
stabilità. Routing, DRC completo e revisione dell'utente precedono qualsiasi
fabbricazione. Non sono stati generati Gerber né un secondo progetto PCB.
