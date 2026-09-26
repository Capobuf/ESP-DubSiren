# ESP-DubSiren PCB V0

Questa revisione e una piattaforma di verifica meccanica ed elettrica, non un progetto autorizzato alla fabbricazione. Serve a verificare nel materiale reale l'ingombro nel box Hammond 29830PSLA, l'ergonomia dei controlli, le forature, lo stack verticale e i due moduli montati sul retro.

## Stato della revisione

- PCB: 98.00 x 72.00 mm, angoli R6.00 mm, 2 layer, FR4 1.6 mm, rame standard.
- Origine PCB: angolo superiore sinistro dell'Edge.Cuts; X verso destra, Y verso il basso.
- Regole V0: clearance 0.20 mm, pista minima 0.20 mm, via 0.60/0.30 mm, foro-foro 0.25 mm.
- Controlli sul lato frontale; ESP32-S3 DevKitC-1 e breakout PCM5102A sul lato posteriore.
- I2S e alimentazioni sono instradati; GND e su entrambi i piani. I dieci segnali di controllo restano intenzionalmente non assegnati a GPIO.
- Il progetto e valido come V0 di verifica, ma **non deve essere mandato in produzione prima delle misure elencate sotto**.

## Box e derivazione delle dimensioni PCB

Box di riferimento: Hammond / Eddystone 29830PSLA.

Dal drawing ufficiale:

- esterno: 120.80 x 95.50 x 33.80 mm;
- vano interno del corpo: 104.71 x 79.41 mm;
- zona interna del lid: 102.83 x 77.13 mm;
- profondita interna nominale del corpo: circa 29.70-30.00 mm;
- spessore coperchio: 2.10 mm;
- interassi dei quattro bossoli: 110.75 x 85.75 mm.

La PCB 98 x 72 mm lascia rispetto alla zona interna del lid 4.83 mm totali in X e 5.13 mm totali in Y, cioe circa 2.42 e 2.57 mm per lato se centrata. Rispetto al vano del corpo lascia circa 3.36 mm per lato in X e 3.71 mm per lato in Y. Gli angoli R6 evitano i bossoli; il margine geometrico minimo tra arco PCB e bossolo nominale e circa 6.3 mm.

`MECH1`, centrato in (49, 36), e un riferimento board-only: su `Dwgs.User` contiene perimetro esterno, vano interno, zona interna del lid e quattro bossoli. Il modello STEP Hammond ufficiale e associato allo stesso riferimento, ruotato per allineare il piano del box alla PCB. Lo STEP rappresenta il box chiuso e quindi oscura la scheda nelle viste dall'alto; le immagini separate mostrano sia la scheda sia l'accoppiamento del box.

Fonti ufficiali:

- [Hammond 29830PSLA drawing](https://www.hammfg.com/files/parts/pdf/29830PSLA.pdf?v=1704738244)
- [Hammond 29830PSLA STEP](https://www.hammfg.com/files/parts/stp/29830PSLA.zip?v=1697661995)
- [Espressif ESP32-S3-DevKitC-1 user guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html)
- [Espressif ESP32-S3-DevKitC-1 dimensional drawing](https://dl.espressif.com/dl/DXF_ESP32-S3-DevKitC-1_V1_20210312CB.pdf)
- [ALPS Alpine RK09L official catalog](https://tech.alpsalpine.com/cms.media/product_catalog_rv_01_rk09l_en_b8437bd47d.pdf)
- [E-Switch PB300STQ product page](https://www.e-switch.com/product/pb300-series-long-travel-pushbutton-switch/?part-number=PB300STQ)
- [E-Switch PB300STQ drawing T003000 Rev. B](https://configured-product-images.s3.amazonaws.com/2D/specs/PB300STQ.pdf)
- [E-Switch PB300STQ STEP](https://configured-product-images.s3.amazonaws.com/stp3dmodels/PB300STQ.stp)

## Layout e coordinate

Le coordinate dei potenziometri sono i centri degli alberi, non l'origine del footprint KiCad. Le coordinate dei pulsanti sono i centri degli attuatori.

| Ref | Funzione | X mm | Y mm | Lato |
|---|---|---:|---:|---|
| RV1 | TUNE | 20.00 | 14.00 | Front |
| RV2 | LFO RATE | 49.00 | 14.00 | Front |
| RV3 | DELAY TIME | 78.00 | 14.00 | Front |
| RV4 | FEEDBACK | 20.00 | 36.00 | Front |
| RV5 | ECHO LEVEL | 49.00 | 36.00 | Front |
| RV6 | MASTER | 78.00 | 36.00 | Front |
| SW2 | MOD DOWN | 14.00 | 58.50 | Front |
| SW1 | TRIGGER | 37.33 | 58.50 | Front |
| SW3 | MOD UP | 60.67 | 58.50 | Front |
| SW4 | ECHO CUT | 84.00 | 58.50 | Front |

La distanza tra gli assi dei potenziometri e 29 mm in X e 22 mm in Y. I pulsanti sono separati di circa 23.33 mm; TRIGGER e in seconda posizione ed e il comando piu immediato per la mano destra o sinistra.

Posizioni dei moduli:

| Ref | Modulo | X mm | Y mm | Rotazione | Lato | Nota |
|---|---|---:|---:|---:|---|---|
| U1 | ESP32-S3-DevKitC-1 N16R8 | 62.00 | 36.00 | 90 deg | Back | USB-C verso la parete destra; proiezione su `Dwgs.User`/`Cmts.User` |
| U2 | PCM5102A breakout | 18.00 | 36.00 | 0 deg | Back | footprint provvisorio, da sostituire dopo misura |
| MECH1 | Hammond 29830PSLA reference | 49.00 | 36.00 | 0 deg | board-only | sagoma e STEP ufficiale |

La DevKit e ruotata sul retro. La sua proiezione X/Y si sovrappone ai controlli frontali, ma i fori non coincidono. Il progetto assume montaggio su socket rialzato e terminali dei controlli rifilati: e una condizione di stack, non una collisione planare autorizzata automaticamente.

## Schema V0

Collegamenti gia formalizzati dal firmware:

- GPIO16 -> `I2S_BCLK` -> PCM5102A BCK;
- GPIO17 -> `I2S_LRCK` -> PCM5102A LCK;
- GPIO18 -> `I2S_DATA` -> PCM5102A DIN;
- MCLK non usato.

Il breakout PCM usa +5 V su VCC, +3V3 su XMT e GND su GND/FLT/DMP/SCL/FMT, coerentemente con le indicazioni gia presenti nel repository. Il suo pin 3V3 e marcato NC. Non e stato aggiunto alcun ADC.

I net `POT_*` e `BTN_*` sono presenti ma terminano intenzionalmente sul solo controllo; l'assegnazione ai GPIO sara definita nella revisione elettrica successiva.

## Footprint e simboli

Creati nel progetto:

- `ESP-DubSiren:ESP32-S3-DevKitC-1_N16R8`: 44 pin THT, corpo ufficiale 25.40 x 62.74 mm, passo 2.54 mm e file di proiezione USB. Il courtyard 2D e omesso intenzionalmente sulla V0 perche l'assemblaggio e su due livelli Z; la sagoma reale resta su Fab/Silk/Dwgs e la verifica va fatta con lo stack fisico.
- `ESP-DubSiren:PCM5102A_BREAKOUT_PROVISIONAL`: singola fila 11 pin, 31.8 x 23.8 mm e passo 2.54 mm **solo come placeholder da verificare**.
- `ESP-DubSiren:PB300STQ`: foratura e corpo dal drawing ufficiale; pad elettrici KiCad 1/2 corrispondenti ai due terminali fisici del disegno, piu ancoraggi e foro di posizionamento. STEP ufficiale associato.
- `ESP-DubSiren:Hammond_29830PSLA_REFERENCE`: riferimento senza pad, escluso da BOM e position file, con sagome e STEP ufficiale.
- simboli di progetto per ESP32-S3-DevKitC-1 e breakout PCM5102A.

Per `RK09L1140A5L` e usato il footprint standard KiCad `Potentiometer_THT:Potentiometer_Alps_RK09L_Single_Vertical`, la cui descrizione include esplicitamente il modello 1140A5L. Include pin, ancoraggi meccanici, corpo, albero e courtyard.

## Componenti provisional

- **PCM5102A breakout:** footprint, ingombro, passo e orientamento non sono affidabili senza il modulo acquistato.
- **PB300STQ:** geometria del footprint ricavata dal documento ufficiale, ma la scelta del componente resta provvisoria e deve essere confermata sul campione.
- **Stack ESP32:** il footprint planare e ufficiale; socket, pin strip e altezza massima della variante N16R8 montata non sono definiti.
- **Posizione del foro USB nel box:** e indicata solo come parete candidata; nessun foro box e stato progettato.

## Verifica 3D e altezza

La verifica planare con la sagoma ufficiale non mostra interferenze con pareti o bossoli. DRC e placement score confermano Edge.Cuts e posizioni; lo STEP Hammond allineato mostra che la PCB resta dentro il perimetro utile.

La verifica verticale non e conclusiva: non esistono nel progetto modelli 3D affidabili del breakout PCM acquistato e dell'esatto sistema DevKit + socket. Una stima conservativa per il ramo peggiore e:

- PB300STQ sopra PCB: 10.6 mm nominali dal drawing;
- PCB: 1.6 mm;
- DevKit su socket sul retro: circa 12-14 mm, dipendente dal socket e dai componenti;
- stack stimato: circa 24.2-26.2 mm contro circa 29.7 mm di profondita interna nominale;
- margine teorico: circa 3.5-5.5 mm, prima di tolleranze, saldature, pin sporgenti e fissaggio PCB.

Questa stima suggerisce fattibilita, ma non prova l'assenza di collisioni Z. In particolare, la DevKit deve essere rialzata sopra le code dei pin THT frontali e tali code devono essere rifilate.

## ERC e DRC

Risultati finali salvati in `erc-v0.json` e `drc-v0.json`:

- ERC: 0 errori, 10 warning. Sono esclusivamente le 10 label isolate `POT_*` e `BTN_*`, intenzionali per non inventare il pinout GPIO.
- DRC: 0 errori, 0 unconnected items, 29 warning.
- Placement score Konnect: 100/100, nessuna hard failure, Edge.Cuts presente.

I 29 warning DRC sono documentati:

- 8 `lib_footprint_mismatch`: limite del refresh typed Konnect/KiCad per footprint standard con zone e per istanze custom aggiornate/flippate;
- 8 `silk_over_copper`: reference designator automatici dei quattro PB300STQ sopra i propri pad; le etichette funzione restano leggibili sotto i pulsanti;
- 12 `footprint_symbol_field_mismatch`: campi Description/Datasheet presenti nello schema ma non replicati nei footprint;
- 1 `extra_footprint`: `MECH1`, intenzionalmente board-only e assente dallo schema.

Non restano short, errori di clearance, collisioni di foratura, piste incrociate, courtyard overlap critici o connessioni non instradate.

## Measurements to verify before fabrication

1. Breakout PCM5102A acquistato: lunghezza, larghezza, altezza massima, passo reale, offset del primo pin, ordine/orientamento degli 11 pin e lato di montaggio.
2. DevKit N16R8 reale: dimensioni massime inclusi USB-C, BOOT/RESET e componenti; altezza dei pin header; tipo e altezza dei socket che saranno usati.
3. Distanza reale tra PCB principale e DevKit/PCM sul retro; lunghezza residua delle code THT di potenziometri e pulsanti dopo saldatura.
4. RK09L1140A5L acquistati: lunghezza boccola/filetto, rondella e dado, altezza corpo sotto pannello, diametro e tolleranza del foro pannello.
5. PB300STQ acquistati: variante esatta, altezza attuatore a riposo/premuto, corsa, eventuale keycap e foro/apertura nel coperchio.
6. Hammond reale: vano interno, raggi di fusione, posizione/diametro bossoli, planarita del lid e quota realmente disponibile tra lid, PCB e fondo.
7. Metodo di fissaggio PCB: distanziali, adesivi o staffe; la V0 non aggiunge fori di montaggio perche i bossoli Hammond sono esterni al perimetro PCB.
8. Posizione del taglio USB-C sulla parete destra dopo aver montato socket e DevKit; verificare anche accesso a BOOT e RESET.
9. Prova completa dello stack chiuso con plastilina/calibro: lid -> controlli -> PCB -> socket/moduli -> fondo, includendo saldature e tolleranze.

## Immagini

- `screenshots/pcb-top.png`: plot 2D top della PCB.
- `screenshots/pcb-3d-top.png`: vista 3D ortogonale della scheda senza il box opaco.
- `screenshots/pcb-3d-iso.png`: vista 3D isometrica della scheda senza il box opaco.
- `screenshots/enclosure-fit-top.png`: accoppiamento XY con lo STEP Hammond e i quattro bossoli.
- `screenshots/enclosure-fit-3d.png`: vista prospettica del riferimento Hammond chiuso.
- `screenshots/schematic-v0.png`: schema elettrico V0.
