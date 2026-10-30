# Issue #34 — verifica elettrica prima del layout

Verifica del **30 ottobre 2026**, sul solo progetto
`hardware/kicad/ESP-DubSiren.kicad_pro`. Schema e campi elettrici della BOM non sono stati modificati:
non è emerso un errore di connessione o di distinta che giustifichi una correzione.
La sola nota J2 della BOM � stata aggiornata per il dado facoltativo senza pannello.
Il confronto riguarda i file salvati, non una scheda assemblata. **Non è una
validazione per la fabbricazione**: il layout è oggetto della #35, senza routing.

## Risultato e copertura

| Controllo eseguito | Risultato | Evidenza |
|---|---|---|
| ERC KiCad 10.0.6, incluse segnalazioni informative | 0 errori, 0 warning | [erc.json](review-34/erc.json) |
| Netlist KiCad contro reti dei pad esportate dal PCB | **237/237 pin**, nessuna differenza; 80 reti, comprese 28 reti NC | [confronto](review-34/connectivity-comparison.json), [netlist](review-34/schematic.net), [GenCAD](review-34/board.cad) |
| Analisi diretta dei corti / reti a pin singolo | 0 / 0 | [corti](review-34/find_shorted_nets.json), [pin singoli](review-34/find_single_pin_nets.json) |
| Distinta pubblicata contro BOM riesportata dallo schema | 64/64 riferimenti uguali per valore, footprint, MPN, produttore, datasheet, quantità e DNP | [confronto](review-34/bom-comparison.json), [BOM esportata](review-34/schematic-bom.csv) |
| Copertura schema | 1 foglio, 136 simboli risolti; 64 riferimenti fisici e simboli di alimentazione | [audit nativo](review-34/run_design_review.json), [inventario pin](review-34/net-summary.json) |
| Inventario PCB iniziale | 68 footprint: 64 riferimenti dello schema e quattro fori H1–H4 | [inventario esportato](review-34/board-summary.json) |

Il confronto pin/rete usa la netlist **kicad-cli** come riferimento. Anche il
riepilogo Konnect è stato confrontato con essa: nessuna differenza. GenCAD è
usato come esportazione di analisi, non come pacchetto CAM né come prova del
bordo PCB. Le letture di pad Konnect riportano `source: file`; le
[geometrie dei pad critici](review-34/critical-pads-before.json) fotografano il
placement precedente alla #35. Le posizioni iniziali non prescrivono il nuovo layout.

## Reti e pinout verificati

| Blocco | Connessioni osservate e confronto |
|---|---|
| ESP32-S3 U1 | Pin 1/40/41 GND; 2 +3V3; 3 EN; 13 IO19/D−; 14 IO20/D+. IO35/36/37, riservati alla PSRAM della variante N16R8, sono NC. Numerazione confrontata con [Espressif](esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf), §§3–4. Nessun GPIO del pannello usa quei tre pin. |
| USB-C J1 | A4/A9/B4/B9→VBUS_RAW; A1/A12/B1/B12 e quattro SH→GND. A6/B6→USB_DP_CONN; A7/B7→USB_DM_CONN. CC1 A5→R4 5,1k→GND; CC2 B5→R5 5,1k→GND. SBU1/SBU2 NC. Confronto con tabella pin [GCT USB4085 RevB](GCT-USB4085-RevB.pdf), foglio 1. |
| USB ESD/serie | U4 SLKOR: I/O1 pin 1/6 su D−, I/O2 3/4 su D+, 2 GND, 5 VBUS_RAW; coincide con [SLKOR](SLKOR-USBLC6-2SC6.pdf), p.2, senza attribuirgli specifiche ST. R2/R3 22Ω fra connettore ed ESP32, senza incrocio D+/D−. |
| Alimentazione USB | J1→VBUS_RAW→F1→VBUS_PROTECTED→U7 VIN(1); U7 ON(3) allo stesso ingresso; VOUT(6)→+5V. D1 catodo a VBUS_PROTECTED, anodo GND. C10 1µF ingresso, C11 10nF su CT(4); QOD(5) NC, opzione ammessa da [TI TPS22918](TI-TPS22918.pdf), p.3. U7 regola la salita, **non** costituisce una negoziazione USB né una protezione da ogni sovratensione. |
| Convertitore U2 | EN(1)/VIN(2) +5V, GND(3)/EP GND, VOUT(4/5) +3V3, sense(6) tramite R1 10Ω alla stessa uscita. PG(7)/VCC(8) NC. C1/C2 sul +5V, C3/C4 sul +3V3. Pinout e applicazione corrispondono a [MPS](MPM3612GLQ-33.pdf), pp.4 e 18. Il condensatore esterno VCC è indicato **optional** in figura 5: non è un componente mancante dimostrato. |
| RESET / BOOT | SW5 porta EN a GND; R6 10k e C7 1µF formano il reset RC. SW6 porta IO0/pin27 a GND, R7 10k a +3V3. Nessun collegamento ai pin PSRAM. |
| Mux U3 | 74HC4051D: comune pin3→ADC_MUX→U1 IO4/pin4, C9 100nF a massa. S0/S1/S2 pin11/10/9→GPIO5/6/10. VCC16 +3V3, GND8/VEE7/enable6/A7 pin4 GND. C8 disaccoppia il +3V3. Pin e selezione canali confrontati con [Toshiba](74HC4051D_datasheet_en_20201203.pdf), pp.2–4. |
| Sei pot / MASTER | RV1–6: pin1 GND, cursore2→POT1–6→U3 A0–A5 pin13/14/15/12/1/5; pin3 alimentato attraverso R8–13 4,7k. RV7 cursore→MASTER→A6/pin2, R14 4,7k. Valori 20k/10k e part number PTV111-3420A-B203/PTV111-1420A-B103 coerenti con BOM. Il quarto foro del footprint non ha rete. |
| CHERRY | SW1–4 pin1→BTN1–4→GPIO38/39/40/41, pin2 GND, pull-up R15–18 da 10k. MPN MX2A-11NW coerente con acquisti; switch non polarizzato. [MX2A](../cad/sources/Cherry-MX2A-Black.pdf) e [layout MX](../cad/sources/Cherry-MX-Legacy-Layout.pdf). |
| Encoder | SW7 A/B→GPIO1/2, C→GND; S1→GPIO42, S2→GND; R19–21 10k. Pinout A-C-B e coppia switch del [PEC11R-4xxxF-Sxxxx](../cad/sources/PEC11R.pdf), p.2, corrispondenti. Direzione fisica e conteggio per scatto richiedono prova, senza inversioni preventive nel firmware. |
| Display 5393 U5 | Header 1 VIN +3V3, 2 3Vo NC, 3 GND, 4 SCK GPIO12, 5 MISO NC, 6 MOSI GPIO11, 7 CS GPIO9, 8 RST GPIO7, 9 DC GPIO8, 10 SDCS NC, 11 LITE GPIO14. Corrisponde a JP1 dell'[Eagle originale](../cad/sources/adafruit-5393/) e a [Config.h](../../firmware/include/Config.h). |
| DAC 6250 U6 | Pin1 VIN +5V, 2/7 GND, 3 WSEL GPIO17, 4 DIN GPIO18, 5 BCLK GPIO16, 6 OUTL→AUDIO_MONO. Pin8 OUTR NC; pin9 3Vo e pin10–14 controlli NC sul carrier. La mappa 1–8=JP2 e 9–14=JP1 originale è coerente con l'[Eagle Adafruit](../cad/sources/adafruit-6250/). |
| Jack J2 / mono | **T→AUDIO_MONO→U6.6 OUTL**, S→GND; TN/SN NC; OUTR non si unisce a OUTL né al jack. [Disegno NRJ4HF](../cad/sources/Neutrik_NRJ4HF.pdf) e [circuiti Neutrik](../cad/sources/Neutrik_Jacks_Circuits.pdf). TP5 è sulla sola AUDIO_MONO. Nessuna doppia RCA. |
| Test pad | TP1 +5V, TP2 +3V3, TP3 GND, TP4 ADC_MUX, TP5 AUDIO_MONO, TP6 UART_TX/IO43, TP7 UART_RX/IO44. Presenti sia nello schema sia nel PCB. Accessibilità finale nella #35. |

Gli ingressi fisici del firmware sono stati letti in
[PanelInputs.cpp](../../firmware/src/control/PanelInputs.cpp): mux `{5,6,10}`,
ADC4, pulsanti `{38,39,40,41,42}`, encoder1/2 e ordine canali coincidono con
le reti sopra. I massimi nominali ADC corrispondono ai divisori 20k+4,7k e
10k+4,7k; questo non verifica tolleranze, linearità o assestamento reali.

Il [pinout Adafruit 5393](https://learn.adafruit.com/adafruit-1-47-round-rectangle-tft-display/pinouts-2)
conferma VIN allo stesso livello logico del microcontrollore e MISO usato
solo dalla SD. Il [pinout 6250](https://learn.adafruit.com/adafruit-pcm510x-i2s-dac/pinouts)
conferma logica 3,3V e uscita di linea per carichi ≥1kΩ. Nel circuito originale
6250 MCLK, FORMAT, FILTER e DEMPH hanno pull-down da 10k; MUTE ha pull-up
da 10k. Lasciarli NC sul carrier non equivale a lasciarli flottanti sul DAC.

## Footprint e montaggio

Le reti dei pad sono state confrontate per tutti i riferimenti; la geometria
dei componenti critici è stata confrontata con disegni e PCB originali. Le
facce qui indicate sono quelle dell'inventario prima della proposta #35.

| Riferimento / faccia | Verifica dimensionale e limite |
|---|---|
| U1 B.Cu | 40 terminali a passo 1,27mm; pad periferici 1,5×0,9mm, fila inferiore ruotata 90°; EP41 3,7×3,7mm GND. I nove quadrati senza numero sono **solo B.Paste**, non nove piazzole di rame flottanti. Confronto con disegno Espressif del WROOM-1U, non il WROOM-1 con antenna PCB. |
| U2 B.Cu | Otto pad elettrici laterali a passo 0,65mm, 0,625×0,4mm; EP 1,1×1,9mm. Il nome locale `LGA-8-1EP` non è il nome commerciale LGA-10: MPS pp.3/19 mostra anche NC1/NC2 nella fascia da lasciare senza rame/piste/via. L'assenza di due pad saldabili in quella fascia è coerente con la land pattern MPS. **Preservare il keepout NC nel routing futuro**. |
| U3 B.Cu; U4/U7 B.Cu | SOIC16 passo1,27mm per Toshiba; SOT23-6 passo0,95mm per SLKOR/TI. I diversi nomi funzionali dei pin 1–6 sono verificati sui rispettivi datasheet, non dedotti dall'uguaglianza del package. |
| J1 B.Cu | Due file passo0,85mm distanti1,35mm, sedici fori Ø0,4mm; quattro ancoraggi SH a asola. Coerenti con il layout GCT USB4085. Non sono previste staffe. Forza di inserzione e robustezza della PCB esposta restano prove meccaniche. |
| RV1–6 F.Cu / RV7 B.Cu | File terminali a passo2,5mm, quattro fori Ø1,1mm, tre pad numerati; scostamento fila11mm per PTV111-3 e3,3mm per PTV111-1. La variante verticale e quella orizzontale non sono intercambiabili. [Bourns PTV/PTT](../cad/sources/Bourns-PTV-PTT.pdf), p.2. Asolature dei ganci, saldatura e fit fisico da controllare nel layout/prototipo. |
| SW7 F.Cu | A/C/B a passo2,5mm, coppia switch a5mm; file a7,5mm e−7mm dall'asse, fori Ø1,05mm. Disegno Bourns della variante4xxxF-S, non variante1xxx. I ganci laterali richiedono le aperture meccaniche del footprint, non piste né via. |
| SW1–4 F.Cu | Due terminali Ø1,5mm, foro centrale Ø4mm e due guide Ø1,7mm a±5,08mm dall'asse. Il riferimento del footprint è il pad1; lo stem è spostato(−2,54;+5,08)mm, da tenere presente nel placement dei quattro tasti. Keycap non identificati: nessuna certificazione di ingombro del cappuccio. |
| SW5/6 B.Cu | C&K PTS636SM43SMTR LFS: due pad2×1,5mm con interasse8,2mm, coerenti con [PTS636](CK-PTS636.pdf), disegno SM. Accesso dito/utensile da verificare nella #35. |
| U5 F.Cu | Header11 pin passo2,54mm, Ø1mm, fori supporto Ø2,5mm a31,75×25,4mm: corrispondenza con Eagle5393. Il footprint è visto dal **display**, mentre Eagle mostra la faccia componenti: la riflessione è intenzionale. La numerazione VIN→LITE resta corretta. Gli spessori di vetro, microSD, flex, header e distanziali non sono ricavati dal solo footprint. |
| U6 B.Cu | Header8+6 passo2,54mm, file distanti15,24mm; fori supporto a25,4×15,24mm. La numerazione relativa è quella della faccia componenti Adafruit; la posa sul retro richiede la trasformazione B.Cu. Header e distanziali non hanno MPN di montaggio definitivo nella BOM. |
| J2 B.Cu | Rettangolo terminali12,70×11,43mm, Ø1,50mm; centraggio Ø2mm e foro opzionale Ø3mm. Reti T/S/TN/SN corrispondenti al disegno. Lo STEP Neutrik originale rimane la fonte del corpo; non è una prova di saldatura o resistenza all'inserzione. |
| Passivi / F1 / D1 | Valori e package BOM uguali allo schema e ai riferimenti PCB. F1 è il [Bel1812 specifico](ds-cp-0zcg-series.pdf); D1 è [PANJIT SOD-123FL](https://www.panjit.com.cn/upload/datasheet/SMF5.0A_SERIES.pdf), montato nel footprint KiCad SOD-123F, catodo pad1 e anodo pad2. Polarità dello schema e PCB coincidenti. Nessun condensatore polarizzato in distinta. |

Le dimensioni esportate non certificano tolleranze, metallizzazione dei fori,
stencil, reflow o disponibilità del componente. Non si aggiorna in blocco una
libreria per eliminare un warning: i footprint comprendono modifiche e modelli
voluti, da confrontare prima di qualunque sincronizzazione.

## BOM, acquisti e ferramenta

La BOM contiene **57 componenti acquistabili e sette test pad**, ciascuno con
quantità1. Sei pot20k, un MASTER10k, quattro CHERRY, un encoder, un5393 e
un6250 corrispondono al registro. Le quantità acquistate dei passivi possono
essere superiori a quelle di montaggio: non sono errori di BOM.

Nel solo [ordine conservato](acquisti.csv) non compaiono **C11
GRM1885C1H103JA01D, U7 TPS22918DBVR e J2 NRJ4HF**. Non è prova che non siano
stati acquistati altrove. RCJ-2123 risulta nell'ordine storico ma non nella
distinta attiva. Davies1100×6, SAK-023×1 e Cliff CL170805B×1 sono accessori
acquistati, non simboli elettrici. Dado e rondella dell'encoder sono indicati
come forniti da Bourns; il loro uso finale dipende dal montaggio.

Con il PCB esposto **NRJ-NUT-B diventa facoltativo**, non può essere indicato
come obbligatorio per un pannello che non esiste più. Distanziali/viti/piedini
M3, fissaggi display e connessioni dei breakout sono distinta meccanica della
#35; non vanno aggiunti come componenti elettrici fittizi. Non è stato inventato
un MPN di keycap o di header non presente nel registro.

## DRC: cosa significa la baseline

Il [DRC iniziale](layout-35/drc-before.json) contiene:

- **160 connessioni non instradate:** attese in questa fase, restano blocco di
  fabbricazione; non si eliminano aggiungendo routing fuori scopo.
- **9 interferenze tra fori e courtyard** (6 PTH, 3 NPTH): reali evidenze di
  placement da affrontare nella #35, non falsi positivi perché il PCB è vuoto.
- **44 segnalazioni serigrafia** (26 rame, 10 sovrapposizione, 8 bordo): da
  ripulire sul placement definitivo.
- **64 differenze dalla libreria:** da riesaminare per geometria/modello,
  senza assumere che ogni modifica sia errata.
- **68 segnalazioni parità:** 64 per il solo campo `DesignStatus` non riportato
  nel footprint e quattro `extra_footprint` per H1–H4. Il confronto indipendente
  dei237 pin non rileva reti discordanti. Non sono68 errori di cablaggio.

Il risultato finale dopo spostamenti, bordo e fori è quello della consegna
#35. Il conteggio qui sopra resta una baseline dichiarata, non una certificazione
del nuovo placement. Le violazioni di bordo proprie di un footprint vanno
valutate con la relativa `owner.reference`: spostare tutto il componente non
cambia la distanza interna tra suo pad e sua asola.

## Audit automatico e verifiche ancora aperte

L'audit aggregato è stato completato sui136 simboli, ma contiene euristiche
che non prevalgono sui datasheet: il condensatore VCC di U2 è opzionale;
U5/U6 contengono già condensatori sulle uscite3V3 dei loro regolatori;
`VBUS_SLEW_CT` è il nodo di controllo CT, non una rail cui aggiungere10µF.
La richiesta generica di bulk ≥10µF su VBUS_PROTECTED non sostituisce il
progetto di slew con C10/C11. Le segnalazioni di capacità assente su VBUS_RAW
e U4 non dimostrano, da sole, un guasto o una necessità di redesign.

Restano da qualificare sul prototipo: assorbimento USB prima/dopo enumerazione,
spunto e cadute sulla catena F1/U7/U2; ripple e capacità effettiva sotto DC-bias
([dati Samsung](Samsung-CL21A226MPCLRNC-DCBias.json)); temperatura del
convertitore; ESD; reset/boot; estremi ADC, diafonia e tempi del mux; direzione
encoder e debounce; display e audio analogico, rumore e transitori del jack.
Un PTC da1,1A e la sola presenza dei resistori CC non dimostrano una corrente
USB disponibile di1,1A. Nessuna misura è stata sostituita con una simulazione.

Non è emerso un blocco **elettrico dimostrato** alla preparazione del layout.
La chiusura del layout resta subordinata alle interferenze meccaniche, alla
distinta di montaggio dei breakout e alle geometrie effettive della #35.
La fabbricazione richiederà poi routing, verifica dei vincoli dei datasheet e
DRC completo. Firmware, scelta dei componenti e reti restano invariati.


## Verifica indipendente finale del placement #35

Dopo il layout da 86 × 120 mm, due nuove esportazioni native (netlist e
GenCAD) confermano **237/237 pin sulla stessa rete**, senza differenze dalla
baseline. L'export IPC2581 contiene soltanto pad sui layer rame, nessuna pista,
via o riempimento esportato; GenCAD ha sezioni TRACKS e ROUTES vuote.
L'assenza di zone non riempite non è dimostrabile dal solo export.
Le evidenze numeriche e le posizioni native dei pad sono in
[final-check.json](review-34/final-check.json).

Controllati U6 su B.Cu a 90° (pad 1 +5V a 5,38/64,39 mm e pad 6 AUDIO_MONO
a 5,38/51,69 mm), i quattro steli CHERRY a X 10/32/54/76 e Y 100 mm,
il centro attivo display a 43/52,5 mm e i quattro fori M3 da 3,2 mm a
X 5/81 e Y 5/115 mm. I fori U5 sono a X 28,141/59,891 e Y 39,8/65,2 mm;
la rotazione del DAC e il riferimento degli steli non introducono errori
rispetto ai footprint verificati.

Il [DRC finale](layout-35/drc-after.json) riporta 115 segnalazioni geometriche:
64 differenze di libreria e 51 di serigrafia; rimangono 160 collegamenti da
instradare e le stesse 68 segnalazioni di parità. Non compaiono violazioni
clearance, PTH/NPTH dentro courtyard o sovrapposizioni courtyard. Questo
conferma il miglioramento del placement, non un PCB pronto per fabbricazione.

La ferramenta del display richiede ancora una scelta verificata: dal foro
inferiore sinistro al bordo del pad quadrato 1 (+3V3) restano 2,2098 mm.
Una rondella metallica di diametro 5 mm sovrapporrebbe quel rame di circa
0,29 mm; un ingombro di diametro 4 mm lascia soltanto 0,21 mm nominali.
Materiale isolante, diametri effettivi, tolleranze e altezza dei distanziali
vanno quindi verificati nella distinta meccanica prima del montaggio.
Nessuna nuova modifica elettrica risulta necessaria da questa verifica.
