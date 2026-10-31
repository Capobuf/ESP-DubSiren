# Issue #38 — ESP32 a sinistra, DAC sotto il display

Proposta del **31 ottobre 2026**, ramo `codex/issue-38-rear-placement`, nell'unico
[PCB attivo](../kicad/ESP-DubSiren.kicad_pcb). **Studio da revisionare, disposizione
non consolidata e PCB non fabbricabile.** Nessun routing, via, zona, Gerber,
cambiamento di schema, BOM o firmware.

**Integrazione header nella issue, ricontrollata il 31 ottobre:** il
[nuovo esame SLxG1](issue-38-header-stack-review.md) documenta il riferimento
commerciale 2,50/3,00/6,10 mm. Con il lato corto U6 nella portante rimangono
1,10 mm nominali fra punta pin e piano U5, prima dei volumi contrapposti.
La sezione è aggiornata a questa prova. Le altezze locali del retro U5,
l'accoppiamento dei fori e la ferramenta restano da qualificare.

Il DAC scompare dal lato sinistro del frontale, ma lo stack display/DAC resta
**INCOMPLETO**: 18 errori DRC documentano i terminali e i fissaggi sotto U5.
Il beneficio elettrico principale è l'uscita audio più corta; il ratsnest globale
dei segnali aumenta di circa il 20%. Non è un miglioramento generale del routing.

## Viste della proposta

- [Prima/dopo fronte](placement-38/F-comparison.png) / [SVG](placement-38/F-comparison.svg).
  Rosso: 18 fori U6 scoperti nella base; verde tratteggiato: gli stessi fori
  coperti dalla PCB di U5 nella proposta. Gli altri fori sono lasciati nel plot:
  non sono stati nascosti terminali dei controlli o fissaggi della scheda.
- [Prima/dopo retro specchiato](placement-38/B-comparison.png) /
  [SVG](placement-38/B-comparison.svg).
- [Ratsnest dei segnali selezionati](placement-38/ratsnest.png): MST ricostruito
  dalle reti/pad effettivi esportati, vista da sopra anche per B.Cu. Non è una
  schermata del ratsnest live KiCad né una previsione dei percorsi del router.
- [Proiezione microSD/flex e ferramenta](placement-38/U5-U6-overlap.png) e
  [sezione simbolica dello stack](placement-38/U5-U6-section.png).
- [Tutte le coordinate prima/dopo](placement-38/positions.csv), con flag `Moved`.

I plot sono export nativi KiCad F/B.Cu, Fab, SilkS e Edge.Cuts. Colori, corpi
riempiti, sigle esplicative e display opaco sono sovrapposizioni per leggere la
proiezione XY; non sono nuove grafiche del PCB o una certificazione dei corpi.
Manopole, keycap, giunti e altezze non sono simulati. Le viste #35 restano il
riferimento storico per gli inviluppi dei comandi, che qui non si muovono.

## Base e modifiche

`origin/main` verificata e aggiornata a `21d4791`; #36 **non integrata**.
Prima dello studio è stato conservato sul suo ramo il checkpoint `3ae553e`,
comprendente le modifiche locali già presenti, anche quelle successive alle
preview #36: campi SW1–4 spostati, riferimenti nascosti e impostazioni locali
di progetto. La nuova branch parte da `origin/main` e riporta tramite cherry-pick
`15a98c3` e quel checkpoint (nuovi hash `241b652`, `734c2cb`). Nessun merge su
`main`; archivio `codex/issue-14-jack-fit` intatto.

La base delle immagini «prima» è **il checkout effettivo preservato**, non la
vecchia preview #36 con le etichette funzionali. Nessuna di quelle etichette
viene ripristinata dal lavoro #38.

Restano invariati tutti i componenti F.Cu, il centro attivo U5 a **X=43**,
l'anchor U5 `(44,016;46,5)`, encoder a destra, tre pot sopra e sotto, quattro
CHERRY, J1/J2/RV7 posteriori, quattro M3 e bordo **86 × 126 mm R3**.

Tutte le coordinate seguenti sono in mm, viste da sopra, **B.Cu**. 270° è
equivalente ai −90° normalizzati da Konnect. Sono stati riposizionati 20 dei
68 footprint; gli altri 48 conservano posizione, rotazione e faccia.

| Rif. | X; Y | Rotazione | Motivo |
|---|---|---|---|
| U1 | 13;46 | 270° | Zona sinistra; USB/I²S escono dal bordo superiore del modulo |
| U6 | 44;46,5 | 0° | Sotto U5, fissaggi più distanti di quelli del candidato Y44 |
| R2 | 19,83;33,5 | 270° | Serie USB DM vicino al pin 13 U1 |
| R3 | 23;33,5 | 270° | Serie USB DP vicino al pin 14 U1 |
| R6 | 8,8;32 | 0° | Pull-up EN con C7 vicino a U1 |
| R7 | 25,3;55,5 | 180° | Pull-up BOOT vicino a U1 |
| C5 | 2,5;34,5 | 90° | Bulk U1 |
| C6 | 5,86;34,7 | 90° | 100 nF vicino al pin 2 U1 |
| C7 | 8,8;34,5 | 90° | Filtro EN |
| C9 | 11,5;34,5 | 90° | Filtro ADC vicino all'ingresso MCU |
| R15 | 19;58 | 90° | Pull-up BTN1 |
| R16 | 16;58 | 90° | Pull-up BTN2 |
| R17 | 13;58 | 90° | Pull-up BTN3 |
| R18 | 10;58 | 90° | Pull-up BTN4 |
| R19 | 4,5;58 | 90° | Pull-up encoder A |
| R20 | 7,2;58 | 90° | Pull-up encoder B |
| R21 | 20;61,5 | 90° | Pull-up pressione encoder |
| U3 | 13;67 | 0° | MUX verso U1 per ridurre il tratto ADC rispetto al candidato |
| C8 | 19;71,445 | 0° | Segue l'alimentazione del MUX |
| TP4 | 7,5;69 | 0° | Segue l'uscita MUX: evita il ramo ADC di prova lungo 25 mm |

Il primo spostamento U1 `(13;46;0°)` / U6 `(44;46,5;0°)` ha mostrato
15 conflitti di courtyard U6: R2/R3/R6/R7/R15–21, C5/C6/C7/C9.
Sono questi i passivi trasferiti; la catena di alimentazione, protezioni USB,
resistenze dei potenziometri, pulsanti e altri test point restano fermi.
U3/C8/TP4 si spostano come gruppo per il percorso analogico, non per simmetria.

Con U1 a 0° e U3 fermo, ADC dista 41,57 mm e USB U4–MCU circa 48–49 mm.
La rotazione 270° riduce quest'ultimo tratto a circa 36 mm. U3 spostato riduce
ADC a 31,73 mm, contro 46,53 mm lasciandolo al centro con U1 a 270°.
Queste sono distanze fra pad, non piste realizzate.

## U5/U6: cosa è verificato e cosa manca

U6 occupa nominalmente X28,76–59,24 / Y36,34–56,66, oltre alla sporgenza
del jack sul lato destro; U5 occupa X25,601–62,431 / Y31,26–61,74.
Tutti i 14 PTH e i quattro NPTH U6 rientrano sotto la PCB opaca di U5.
Fra i pad circolari U6 rimangono 0,864 mm di rame libero nella fila superiore
e 0,762 mm in quella inferiore: esiste spazio geometrico per una pista fine
con le regole attuali, ma non è stato verificato un fanout instradato.

| Controllo XY | Riscontro nominale |
|---|---|
| Minimo interasse fissaggi U5–U6, candidato `(44;44)` | 4,079 mm |
| Minimo interasse con U6 `(44;46,5)` | **5,982 mm** |
| Distanza fra ingombri circolari Ø4 dei due fissaggi | 1,982 mm, prima delle tolleranze |
| Courtyard U6 contro inviluppo dei fori U5 Ø2,5 | 0,790 mm minimo |
| Courtyard U6 contro inviluppo dei pad header U5 | 1,710 mm minimo |

Non basta la distanza fra le viti. Il contorno della scocca MICROSD nel file
Eagle 5393 si proietta circa a X27,225–42,325 / Y38,723–52,723; il corpo
EYE SPI a X56,88–61,28 / Y40–53. VIN/GND/LRCK U6 e il fissaggio superiore
sinistro sono sotto la zona microSD; gli ingombri Ø4 dei fissaggi destri
intersecano in pianta il connettore EYE SPI. Anche il fissaggio inferiore
sinistro si avvicina alla scocca SD. **Non è provata una collisione Z, ma non
è provato neppure il gioco.** Il flex del TFT, altri componenti e rame esposto
richiedono verifica ulteriore: la figura non è una mappa completa delle superfici.

Fonte XY: Eagle ufficiale locale, revisioni
[5393](../cad/sources/adafruit-5393/revision.txt) e
[6250](../cad/sources/adafruit-6250/revision.txt). Per 5393 la proiezione usata è
`Xcarrier=62,431−Xeagle`, `Ycarrier=61,74−Yeagle`, verificabile dai quattro fori
e dalla fila header. L'inviluppo SD usa le prime otto linee del package
MICROSD, escludendo i disegni della scheda inserita/estratta; EYE SPI usa
il rettangolo layer 51. Questi contorni non documentano altezze o isolamento.

Le altezze prodotto **5,4 mm U5** e **6,2 mm U6** sono riportate da
[Adafruit 5393](https://www.adafruit.com/product/5393) e
[Adafruit 6250](https://www.adafruit.com/product/6250). Non definiscono la
separazione dal carrier o la quota massima dell'assieme.

La sezione distingue:

- `g5`: carrier F.Cu–faccia interna della PCB U5;
- `hU5`: massima sporgenza verso il carrier del retro U5, compresi SD/flex;
- `hF`: massimo sopra il carrier di saldature/pin U6 e teste delle sue viti;
- `g6`: distanza carrier B.Cu–PCB DAC, includendo header e ferramenta.

**Precisazione dell'utente, 31 ottobre 2026:** il display è sempre saldato
tramite header. L'integrazione successiva identifica **SL11G1** per U5 e
**SL8G1/SL6G1** per U6 come riferimento commerciale dello studio, senza
attribuirne le quote ai pin Adafruit inclusi. Non comprende uno zoccolo
sfilabile. Il controllo DRC qui riportato è in
pianta e non valuta la separazione verticale creata dall'header; i 18 errori
non dimostrano da soli una collisione nell'assieme montato.

Occorre misurare e soddisfare **`g5 > hU5 + hF + margine`**, usando altezze
reali e tolleranze. Il margine non è stato scelto arbitrariamente. Verificare
anche `g6` e la profondità finale sotto il carrier rispetto agli appoggi #35.
La maschera non rende accettabile il contatto o la pressione sulla scocca SD.

## Montaggio e manutenzione: alternativa minima di fissaggio

La proposta non assume un display appoggiato sul DAC. L'alternativa minima
al montaggio ravvicinato del candidato è **U6 a Y46,5 e U5 sostenuto tramite
i suoi fori**, con separazione ricavata dalle misure sopra. Non aggiunge
staffe, box o fori. Non è ancora un kit di ferramenta selezionato e approvato.

Sequenza da qualificare sul campione prima di consolidare il placement:

1. Montare/verificare SMD e U1 prima dei breakout, poi gli altri componenti
   bassi. I PTH frontali non attraversano gli ingombri posteriori spostati.
2. Presentare DAC/header e fissaggi isolanti a secco. Le viti U6 entrano dal
   fronte del carrier prima di U5, con serraggio/manutenzione dal retro.
   Verificare spazio per dadi, utensile e corpi del DAC; nessuna rondella
   metallica aggiunta d'ufficio.
3. Montare **DAC prima**: il fronte privo del display lascia accesso ai suoi
   14 giunti. Saldare, pulire, ispezionare e misurare la massima sporgenza
   effettiva dei pin/giunti e delle teste. Non tagliare pin a una quota inventata.
4. Presentare U5 con lo **SL11G1 di riferimento**, senza assumere uno zoccolo
   sfilabile. Provare il distacco nominale 2,50 mm e il verso dei terminali
   come nel [ricontrollo header](issue-38-header-stack-review.md). Non si
   confermano quattro colonne Ø4: l'inviluppo plastico passa troppo vicino
   ai due fori inferiori per presumerne la compatibilità. Il margine al rame
   vicino a VIN resta inoltre circa 0,21 mm con ingombro Ø4. Nessun articolo
   aggiunto alla BOM elettrica.
5. Montare **display dopo**, serrando la PCB del breakout senza premere
   vetro, backlight o flex e senza piegarlo per raggiungere i distanziali.
   Verificare anche accesso laterale alla microSD e all'eventuale flex.
6. Per manutenzione con SL11G1 saldato a entrambe le schede: togliere
   alimentazione, dissaldare i suoi 11 contatti dal lato accessibile e liberare
   i fissaggi, sostenendo la PCB del display. Accesso con utensile e facilità
   di rimozione restano da qualificare. Non è una connessione sfilabile;
   rimuovere U5 rende di nuovo accessibili giunti e teste U6.

Se non si trova lo spazio per header/fissaggi o il rialzo richiesto non è
accettabile, lo stack rimane non valido: non sostituirlo con un generico
biadesivo. Nessuna superficie adesiva è stata qualificata. Prima di un redesign
serve il riscontro sulle quote `g5/hU5/hF/g6` e sulla minuteria effettiva.

## U1 a sinistra e antenna esterna

La proiezione del corpo Fab dopo la rotazione è circa X3,87–23,07 / Y37–55;
il courtyard è X3,89–23,05 / Y36,25–55,75. Il corpo Fab supera il courtyard
di 0,02 mm su un bordo: qui il courtyard non è assunto come inviluppo
meccanico completo. Non è stato modificato il footprint per nascondere il limite.

Il controllo dei pad frontali contro i courtyard posteriori spostati non trova
intersezioni. L'accesso a U1 avviene dal retro; rimozione/rilavorazione richiede
il processo SMD adatto anche al pad centrale, come nella base #34.

U1 resta **ESP32-S3-WROOM-1U-N16R8**. Le figure 3-1 e 10-2 del
[datasheet Espressif locale](esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)
indicano la variante con connettore esterno: non si applica l'antenna PCB della
1 non-U. Trasformando il disegno, il centro nominale del connettore è circa
`(6,33;52)`, rivolto verso l'esterno su B.Cu. La linea tratteggiata nel retro
propone l'uscita del coassiale verso sinistra, nella fascia libera a Y52,
prima della fila di pull-up Y58. Non è un'antenna aggiunta al progetto.

Restano da misurare innesto, corpo della spina, utensile di estrazione,
raggio del cavo e altezza sotto la scheda. Antenna e cavo non hanno un MPN
identificato nella distinta acquisti; non è dichiarata una clearance RF/cavo
qualificata sulla base del solo punto centrale.

## Giudizio elettrico e di instradabilità

Distanze **rettilinee pad–pad**, non lunghezze di piste o risultati SI:

| Collegamento | Base #36 | Proposta | Valutazione |
|---|---:|---:|---|
| USB DM, U4.6–U1.13 | 31,03 | 35,83 | Più lungo; R2–MCU scende da 5,85 a 2,93 |
| USB DP, U4.4–U1.14 | 30,35 | 36,23 | Più lungo; R3–MCU scende da 5,40 a 3,49 |
| BCLK | 29,14 | 30,56 | Poco più lungo |
| LRCK | 30,66 | 24,23 | Più corto |
| DIN | 30,26 | 25,49 | Più corto |
| OUTL U6.6–J2.T | 75,38 | 37,96 | Vantaggio concreto sul tratto principale analogico |
| ADC U3.3–U1.4 | 21,25 | 31,73 | Peggiora; C9–MCU resta 3,68 |
| U3.3–TP4 | 3,16 | 3,03 | Il test point segue il MUX |
| C6 VDD–U1.2 | 5,47 | 1,78 | Disaccoppiamento più vicino |
| C8 VDD–U3.16 | 7,94 | 2,75 | Disaccoppiamento più vicino |

USB conserva J1, protezione U4 e resistenze da 22 Ω. Il corridoio verso U1
deve aggirare alimentazione/test point e terminali dei pot superiori, con
ritorno GND continuo e coppia controllata: la differenza delle rette non è
skew accettato. Nessuna impedenza è stata validata da questo studio.

I²S esce dal bordo superiore U1 verso la fila Y38,88 U6. Il fanout deve evitare
gli altri pad MCU, le serie USB e i fissaggi U5/U6; può richiedere cambi faccia
nel futuro routing. Le rette del ratsnest non dimostrano che possa rimanere
tutto su B.Cu. L'uscita mono ora sale verso J2 nella metà destra, lontano dal
convertitore a sinistra; evitare di far correre I²S parallelo ad AUDIO_MONO e
garantire ritorni vicini, senza inventare una separazione/taglio delle masse.

U6 prende sempre +5V e GND; U2/U7, i loro condensatori e le protezioni non
si spostano. C5/C6 seguono U1 e C8 U3. Le distanze VDD non certificano la
dimensione del loop o il ritorno di massa, ancora da realizzare.

MUX e ADC restano il costo analogico della proposta: collegamento MCU più
lungo, C9 vicino all'ingresso e diversi segnali dei pot più estesi. Encoder,
BOOT/RESET, UART e pulsanti raggiungono ora U1 a sinistra; pull-up vicini
al MCU non eliminano quelle lunghezze o gli attraversamenti.

Per evitare una valutazione limitata ai collegamenti favorevoli, il
[MST per tutte le reti di segnale](placement-38/signal-mst-comparison.json)
passa da **1412,57 a 1692,02 mm, +19,8%**. Sono esclusi GND, +5V, +3V3 e
le reti `unconnected`; è un limite inferiore geometrico, non routing. I maggiori
incrementi sono encoder A/B (circa +47 mm ciascuno), UART (circa +38–39 mm),
encoder push (+38 mm) e POT6 (+28 mm). Non si afferma che questo renda la
scheda inutilizzabile, ma il DAC nascosto **non basta per preferire il nuovo
placement**. Il compromesso da revisionare è audio più corto e fronte libero
contro più collegamenti lunghi e uno stack meccanico ancora incerto.

## Verifica e punto di stop

Modifiche eseguite esclusivamente con `set_component_placements` Konnect MCP.
Il collegamento app non aggiornava il catalogo dopo `load_toolset`: è stato
usato lo stesso server MCP locale `127.0.0.1:3000/mcp`, senza un secondo server
né scritture custom ai sorgenti KiCad. KiCad era chiuso; risposte `source:file`
con fallback protetto e letture `saved_file`, nessun risultato live rivendicato.

| Controllo finale | Esito |
|---|---|
| Footprint / coppie uniche riferimento-pad numerato | 68 / **237**, invariati |
| UUID pad, reti, drill, layer, forma e dimensioni | Invariati; posizioni rilette e coerenti con trasformazioni |
| GenCAD BOARD / SIGNALS | Identici; PADS/PADSTACKS ricodificati dalle rotazioni, geometrie verificate separatamente |
| GenCAD TRACKS / ROUTES | Vuoti, invariati; nessuna operazione su piste/via/zone |
| Intersezioni courtyard posteriori, 52 footprint | **0** fra tutti gli inviluppi letti |
| Pad/fori frontali contro courtyard posteriori spostati | **0**; non copre volumi dei giunti o utensili |
| Intrusioni pad nelle quattro riserve M3 8×8 | **0** |
| DRC geometrico base | 0 errori, 115 warning |
| DRC geometrico proposta | **18 errori**, **92 warning** |
| Errori nuovi | 14 `pth_inside_courtyard` + 4 `npth_inside_courtyard`, tutti U6 sotto U5 |
| Warning proposta | 64 libreria, 13 silk/rame, 4 silk sovrapposte, 11 silk/bordo |
| Connessioni non instradate | **160**, invariate |
| Segnalazioni parità schema/PCB | **68**, invariate; non risolte dallo studio |

I 18 errori non sono stati esclusi dal DRC o dichiarati falsi positivi: sono
il vincolo da risolvere con la verifica dello stack reale. Nessun altro errore
di clearance, bordo, PTH/NPTH o courtyard risulta dal DRC finale.
I warning di libreria/serigrafia residui non vengono corretti fuori scope.

Evidenze: [verifica numerica](placement-38/verification.json), DRC
[prima](placement-38/before-drc.json) / [dopo](placement-38/after-drc.json),
GenCAD [prima](placement-38/before.cad) / [dopo](placement-38/after.cad),
[pad finali](placement-38/after-pads.json) e
[courtyard finali](placement-38/after-courtyards.json).

**Stop per revisione utente:** una sola proposta nel PCB attivo. Prima di
consolidarla servono feedback sul compromesso e misure di stack, ferramenta,
header e cavo antenna. Nessun merge autonomo, chiusura della #38 o routing.
