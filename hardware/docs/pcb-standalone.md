# ESP-DubSiren — PCB desktop standalone

Decisione di progetto: **30 ottobre 2026**. Sostituisce i vincoli meccanici della issue [#14](https://github.com/Capobuf/ESP-DubSiren/issues/14). Le verifiche dello studio Hammond non vengono presentate come validazione della nuova scheda.

## Configurazione scelta

- **Un solo PCB strutturale a vista**, per uso DIY su un tavolo. Niente Hammond o altra enclosure obbligatoria, né stampa 3D necessaria.
- **Solder mask nera**, serigrafia frontale curata e leggibile; bianco come proposta di partenza. Le etichette saranno progettate direttamente in KiCad.
- Restare **compatti**, usando circa **86 × 112 mm** del precedente studio CAD come riferimento iniziale, non come misura definitiva o promessa di compatibilità con l'Hammond.
- **Frontale sostanzialmente invariato**: tre potenziometri in alto, display Adafruit 5393 al centro, encoder alla sua destra, tre potenziometri sotto, quattro pulsanti CHERRY in basso.
- **Posteriore da riordinare**: USB-C GCT USB4085, jack mono TS Neutrik NRJ4HF e potenziometro MASTER Bourns; garantire spazio reale per spine, dita, manopola e accesso ai comandi.
- Il PCB deve poter essere usato **orizzontale o leggermente inclinato**, semplicemente cambiando la ferramenta di supporto.
- Non aggiungere staffe metalliche di rinforzo per USB-C, scocche, piastre o fissaggi custom senza un problema concreto verificato.

La possibilità futura di progettare una custodia su misura resta aperta, ma **non condiziona il layout attuale**. Nessuna compatibilità dimensionale Hammond è dichiarata.

## Fissaggi e opzioni di appoggio

Predisporre **quattro fori non metallizzati** per viteria **M3**, preferibilmente vicino agli angoli del PCB, con area libera adeguata per teste delle viti, rondelle, distanziali, rame e saldature. Diametro, coordinate, distanza dai bordi e giochi da fissare nella [#35](https://github.com/Capobuf/ESP-DubSiren/issues/35) sulla geometria vera.

Tre montaggi non esclusivi, sfruttando se possibile gli stessi quattro fori:

1. **Piano**: quattro distanziali di altezza uguale, viti e piedini antiscivolo.
2. **Inclinato**: due distanziali anteriori più bassi e due posteriori più alti, controllando la stabilità dei contatti con il tavolo e il gioco dei componenti sottostanti.
3. **Alternativo**: fissaggio ai medesimi fori su una base generica; clip/supporti a scatto soltanto se il produttore ne documenta la compatibilità con foro e spessore PCB. Non presumere che una clip standard funzioni in un foro M3.

**Indicazione non definitiva** per i distanziali: iniziare a valutare 15 mm davanti / 20 mm dietro, oppure quattro distanziali da 15–20 mm. Nello studio precedente il corpo del jack sul lato inferiore arriva nominalmente circa **12,7 mm sotto B.Cu**. Le altezze utilizzabili e l'angolo dipendono però dalla posizione reale di jack, DAC, componenti, saldature e punti di appoggio. Non ordinare la minuteria prima di questa verifica.

Materiali standard: nylon nero, viti M3, rondelle isolanti dove servono, piedini in gomma/silicone. Nessuna soluzione di supporto deve diventare un componente elettronico obbligatorio nella BOM.

## Display e moduli

- **Adafruit 5393**: evitare una custodia o una cornice dedicata. Il footprint corrente rappresenta **quattro fori Ø2,5 mm**; prima di usarli per il fissaggio verificare la corrispondenza con il modulo reale e gli ingombri dei suoi componenti sul retro. Possibile fissaggio con piccole viti/distanziali (ad es. M2 se compatibile), oppure biadesivo **isolante**, sottile, su aree effettivamente piane e libere; non applicarlo su piste nude, pad, componenti, connettori, flex o vetro. Non fare affidamento solo sulle saldature dei pin per resistere a sollecitazioni.
- **Avvertenza Adafruit**: senza una copertura che trattenga il TFT, il gruppo della retroilluminazione può separarsi dal display; il produttore suggerisce, per uso a vista, un leggero fissaggio laterale con adesivo idoneo o nastro sottile. Fonte: [guida Adafruit 5393](https://learn.adafruit.com/adafruit-1-47-round-rectangle-tft-display/overview). Verificare visivamente il modulo prima di applicare un adesivo.
- Lo spessore reale della PCB del 5393 e la quota dalla PCB alla superficie del vetro non sono stati certificati dal vecchio studio. Non serve più ottenere un allineamento a filo della scocca, ma serve comunque verificare distanziatori, accessibilità, saldature e assenza di schiacciamenti.
- **DAC Adafruit 6250** e altri componenti su B.Cu: verificare la faccia effettiva di montaggio e il volume occupato, compresi eventuali pin passanti, giunti di saldatura e spazio sotto la scheda.
- I quattro CHERRY devono poter essere premuti senza che keycap, piedini o PCB interferiscano; definire i keycap effettivi, non usare un volume generico come dato certificato.
- Encoder e manopola SAK-023: conservare lo studio dimensionale precedente come evidenza, ma rivalutare la posizione della manopola **senza** l'obbligo della battuta sulla parete Hammond.

## Stato reale del progetto

- [Schema KiCad](../kicad/ESP-DubSiren.kicad_sch), [PCB](../kicad/ESP-DubSiren.kicad_pcb), [BOM](BOM.csv), [fonti](sources.md).
- Il PCB attuale **non è instradato**, non ha un bordo finale approvato e conserva posizioni di studio. In particolare, le coordinate dei potenziometri sul PCB non coincidono con quelle usate per il precedente pannello FreeCAD; il layout va riconciliato, non semplicemente copiato.
- J2 resta **Neutrik NRJ4HF mono TS**, non doppio RCA. Collegamento previsto: T → AUDIO_MONO/OUTL, S → GND, TN/SN NC, OUTR indipendente e non unito a OUTL. Il dado NRJ-NUT-B serviva al fissaggio attraverso un pannello: **non è obbligatorio nel montaggio a PCB nudo**; valutarne la presenza solo se si aggiungerà una parete opzionale.
- Conservare l'accessibilità fisica a USB, jack, MASTER, RESET/BOOT e ai test pad; non introdurre staffe o supporti che coprano le aree da saldare.
- L'assemblaggio completamente popolato **non è stato validato**. Il colore del PCB e gli schemi di montaggio sono decisioni approvate, non prova di fattibilità dei footprint correnti.

## Lavori successivi

1. [#34 — Schema, pinout, BOM](https://github.com/Capobuf/ESP-DubSiren/issues/34): verifica elettrica pertinente prima del layout definitivo.
2. [#35 — Layout e supporti](https://github.com/Capobuf/ESP-DubSiren/issues/35): posizione finale dei footprint, bordo compatto, quattro fori, distanze inferiori, fissaggio display, confronto statico top/bottom e opzioni di appoggio. **Revisione dell'utente prima del routing**.
3. [#36 — Serigrafia e guida DIY](https://github.com/Capobuf/ESP-DubSiren/issues/36): labeling KiCad e procedura pratica di saldatura/montaggio, con minuteria facoltativa.

Non creare PCB paralleli, né reintrodurre le verifiche di inserimento Hammond. Niente routing, via, zone o Gerber fino alla revisione di questa nuova disposizione. Gli studi Hammond precedenti sono **storici** e non rappresentano la meccanica attiva.
