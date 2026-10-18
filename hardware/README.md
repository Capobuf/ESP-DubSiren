# ESP-DubSiren — hardware

**PR #17 integrata: J2 è Neutrik NRJ4HF. Nuova proposta di posizione nel solo CAD; Gate B NON VALIDATO. Nessun PCB fabbricabile.**

- [Progetto KiCad](kicad/ESP-DubSiren.kicad_pro), [schema](kicad/ESP-DubSiren.kicad_sch), [PCB](kicad/ESP-DubSiren.kicad_pcb).
- [Assieme FreeCAD modificabile](cad/assembly.FCStd): Hammond originale, proposta pannello Gate A e studio posteriore NRJ4HF.
- [Posteriore e sezione con quote](cad/nrj4hf-rear-section.svg), [anteprima](cad/nrj4hf-rear-section.png), [vista interna](cad/nrj4hf-inside.png), [misure e limiti](cad/nrj4hf-validation.json).
- [Vista dall'alto con gli inviluppi dei corpi pot](cad/nrj4hf-top-screening.png): controllo parziale di interferenza, non assieme completo.
- [Sonda di inserimento a +5° con foro D12](cad/nrj4hf-insertion-probe.png): naso blu, boccole arancio, interferenze rosse; non è una posa di montaggio ammessa.
- [Pannello Gate A e dima cartacea 1:1](cad/gate-a-panel.pdf), [SVG](cad/gate-a-panel.svg), [anteprima](cad/gate-a-preview.png). Stampare al 100%, verificare la barra da 50 mm; **non forare**.
- [BOM elettronica](docs/BOM.csv), [fonti e accessorio NRJ-NUT-B](docs/sources.md), [originali CAD](cad/sources/).

## Connettore e schema

J2 è il **Neutrik NRJ4HF**, non NRJ4HF-1. Simbolo standard
`Connector_Audio:AudioJack2_Switch`, footprint standard
`Connector_Audio:Jack_6.35mm_Neutrik_NRJ4HF_Horizontal`.
T è collegato ad AUDIO_MONO / OUTL del DAC U6; S a GND. TN e SN sono NC.
OUTR resta inutilizzato: non è unito a OUTL e non raggiunge J2. Nessuna modifica al firmware.
Il dado **NRJ-NUT-B, quantità 1**, è ferramenta separata documentata nelle fonti;
non è un componente elettrico fittizio nella BOM. Il vecchio RCA è assente dalla BOM e dall'assieme attivo.

Il PCB conserva 68 footprint e non contiene piste, via, zone o bordo approvato.
J2 è ora a **(86,73; 20,34) mm, 90 gradi, B.Cu**, con imboccatura verso il posteriore.
Gli altri footprint conservano il placement provvisorio precedente. Il piano CAD non è una sagoma PCB.
Le librerie locali mantengono il suffisso V2 per preservare gli identificatori; non esiste un secondo progetto hardware.
La BOM mantiene 57 componenti acquistabili e 7 test pad; l'[ordine originale](docs/acquisti.csv)
documenta gli acquisti, non le quantità di montaggio.

## Battuta dei potenziometri e posa corrente

**I sei potenziometri frontali fissano la PCB alla scocca tramite i dadi esterni.**
Le loro spalle appoggiano all'interno del tetto: il precedente margine artificiale
di 0,5 mm è rimosso. Dal corpo nominale Bourns di 7,5 mm e dal tetto interno
Z31,8 deriva F.Cu **Z24,3**, B.Cu **Z22,7** con PCB nominale 1,6 mm.
Sono quote di progetto, non misure certificate sui pezzi.

La trasformazione XY rimane Xcad=Xpcb−62,5, Ycad=61,39−Ypcb.
Il piano rettangolare visibile è un riferimento, **non la sagoma definitiva della PCB**.
Jack e USB salgono di 0,5 mm con la PCB; l'asse jack è ora X12,5 / Z16,2.
La proposta J2 equivale a PCB (82,23;20,34), ancora non trasferita a KiCad.
Il corpo jack occupa Z10…22,7 e i pin arrivano a Z26,7.
Il budget assiale nominale di GateBHeight è 22,3 mm con margine 7,4 mm:
non include tutte le interferenze o le tolleranze.

Il MASTER ora ha un inviluppo di corpo, boccola e albero: asse proposto
X−31 / Z12,7, spalla Y58, verso +Y, sul lato B.Cu. La proposta equivale
a PCB (31,5;10,89), **non è un placement approvato né trasferito a KiCad**.
Corpo conservativo 12 × 7,5 × 16,7 mm, asse 10 mm dalla PCB secondo Bourns
PTV111-1. Distanza nominale corpo–USB 3,03 mm, corpo–scocca solo 0,070 mm.
Piedini, clip e fissaggio completo restano da verificare. La vecchia
RV7_PlanProjection è nascosta e marcata storica.

## Fori Ø10 e fissaggio

I sei cerchi nativi Openings, la sonda principale e la dima cartacea usano ora
**Ø10**, con centri invariati; jack Ø12 e MASTER Ø10 nelle sonde posteriori.
Dime e aperture restano di studio: **non forare**.

| Verifica nominale | Risultato |
|---|---|
| Boccola M9 nel foro Ø10 | Gioco radiale 0,50 mm |
| Copertura Davies 1100, gonna Ø19,84375 | 4,922 mm centrata; 4,422 mm con eccentricità 0,50 mm |
| Copertura MASTER Cliff K85M Ø14 | 2 mm centrata; 1,5 mm con eccentricità 0,50 mm |
| Flangia NRJ-NUT-B Ø17,45 sul foro Ø12 | 2,725 mm centrata; 2,325 mm con eccentricità 0,40 mm |
| Boccola frontale M9 × 5, spalla a Z31,8 | Sporge 3 mm sopra il tetto esterno Z33,8 |

Il riferimento Alps, **non selezionato come ferramenta**, richiede 2 mm di dado
e 0,5 mm di rondella: rimangono nominalmente 0,5 mm. Non è una verifica del
filetto utile, della coppia, delle tolleranze o degli accessori Bourns effettivi.
L'incavo Cliff Ø12 non contiene necessariamente un dado SW11, diagonale circa
12,70 mm: posizione assiale del cap e accesso utensile non sono risolti.

La copertura in proiezione non dimostra l'assenza di fessure. La parete posteriore
inclinata di 1° produce **0,3046 mm di dislivello** sotto la flangia rigida del
dado jack. Nella posa corrente spalla jack–scocca distano circa 0,0698 mm,
dado–scocca circa 0,00915 mm. Serrando non si può presumere una deformazione
innocua o spostare la PCB dalla battuta dei pot. **Serraggio posteriore NON VALIDATO.**

## Inserimento del blocco rigido

La posa finale nominale ha volume comune nullo per jack, USB e boccole rispetto
alla sonda forata. Questo non basta a dimostrare l'ingresso dal fondo.
I corpi frontali sono inviluppi F.Fab 12 × 13,4 × 7,5 mm; le boccole sono
cilindri M9 × 5. Sono stati aggiunti gli alberi mancanti: L20 dalla spalla,
15 mm sopra la boccola, inviluppo massimo Ø6. D-flat e smussi sono omessi:
una collisione di questi inviluppi non prova da sola un impedimento reale.

Le sonde seguenti ruotano attorno all'asse X passante per
P=(12,5;59,183404867;16,2), senza traslazioni, nella posa aggiornata:

| Angolo | Jack–scocca mm³ | Sei boccole–scocca mm³ | USB–scocca mm³ |
|---|---:|---:|---:|
| 0° | 0 | 0 | 0 |
| +1° | 0 | 0 | 0,060 |
| +2° | 0 | 5,143 | 0,116 |
| +3° | 0,229 | 21,966 | 0,167 |
| +5° | 4,749 | 46,307 | 0,252 |
| +10° | 19,696 | 87,116 | 0,379 |

Non è solo un limite del perno scelto. Nella fase in cui il naso attraversa
il foro posteriore e la boccola centrale superiore attraversa il tetto,
un vincolo analitico ammette anche traslazioni Y/Z e concede al naso tutto
il gioco nominale di ±0,4 mm, ignorando ostacoli aggiuntivi.
A +3° il centro della sezione della boccola deve essere almeno a Y41,506,
ma le spalle reali del jack limitano Y a **41,139**: mancano **0,367 mm**.
A +5° il difetto minimo sale a **1,173 mm**. Le intersezioni native OCC
confermano il conflitto nelle pose limite.

**Questa è una verifica negativa di quella fase, non una prova di impossibilità
di ogni percorso 3D.** Non dimostra che una diversa sequenza debba attraversare
la stessa fase a 3°. Mancano un percorso continuo completo, la sagoma reale,
tutti gli ingombri significativi e le tolleranze. La sequenza richiesta
jack/MASTER → rotazione → pot frontali → serraggio **NON È VALIDATA**.

I dati e le formule sono in `active_study` del
[nrj4hf-validation.json](cad/nrj4hf-validation.json) e nella proprietà nativa
`MountingProbeResults.SeatedStudyJSON`. I cilindri inclinati sono ricostruiti
da base e asse trasformati, evitando il falso zero OCC osservato sulle copie
ruotate. `historical_baseline` conserva esplicitamente la posa F.Cu23,8 e
le precedenti prove Ø9,2/Ø9,5/Ø10: non sono risultati della posa corrente.

Per superare il blocco, le possibilità da valutare, in ordine di impatto, sono:

1. Mantenere i componenti e il frontale, cercando una diversa traiettoria 3D
   e registrazione delle interfacce posteriori; fattibilità ancora da dimostrare.
2. Studiare maggiore gioco o un profilo diverso delle aperture, verificando
   copertura dei caps e appoggio dei dadi; Ø10 non basta nelle fasi provate.
3. Se i vincoli restano incompatibili, concordare componenti posteriori o
   sequenza differenti. Una deroga al blocco unico richiede conferma.

Nessuna di queste alternative è una soluzione già validata. Non è stata
introdotta una sostituzione di componenti, una tasca nella scocca o una dima STL.
Il display rimane indipendentemente aperto sulle due quote fisiche indicate sotto.

## Controlli e limiti del progetto

Verifica elettrica della PR #17, 18 ottobre 2026: ERC senza errori o warning. Il confronto DRC prima/dopo conserva
117 segnalazioni geometriche (stesse categorie e coppie di oggetti), 68 segnalazioni di parità
schema/PCB e passa da 161 a 160 connessioni non instradate. Totale 346→345 (errori 170→169,
warning 176 invariati). J2 mantiene la segnalazione di differenza dalla libreria; il modello originale
è associato intenzionalmente. Il riposizionamento evita una nuova interferenza del pad T con il courtyard RV3.
Non sono state corrette anomalie estranee alla sostituzione. Il DRC non nullo e l'assenza della sagoma
impediscono di considerare il PCB pronto per la produzione.

La #14 mantiene la proposta Gate A: sei Davies 1100 a passo 28 mm, display 5393,
encoder con Same Sky SAK-023 e quattro pulsanti a passo 22 mm; Cliff K85M per il MASTER.
I keycap hanno una riserva di progetto 20 x 20 mm, non una misura verificata.
L'utente ha autorizzato lo studio Gate B mantenendo il layout provvisorio. Display a filo,
componenti completi, tolleranze e montaggio restano da validare. Nessuna approvazione automatica del Gate B.
Per la sezione del display, **entrambe le quote restano da verificare sul componente
reale**: spessore PCB e distanza fra faccia superiore della PCB e superficie del vetro.
Il file Eagle originale conservato (`73022f6`) usa `layerSetup=(1*16)`,
`mtIsolate=1.5mm` e `mtCopper=0.035mm` per lato: **1,57 mm è soltanto lo stackup
CAD nominale**, non uno spessore fisico certificato. Non viene usato per fissare
l'altezza del modulo nell'assieme. I 5,4 mm complessivi pubblicati da Adafruit
non separano le due quote né gli ingombri posteriori. Il relativo accoppiamento
verticale display/PCB/contenitore rimane NON DEFINITIVO; nessuna quota del vetro
è stata inventata. In `nrj4hf-validation.json` le due quote fisiche sono `null`.
La proposta CAD è stata salvata e riaperta: forme OCC valide, volume comune nullo
fra J2 e i sei inviluppi, distanza minima 1,52 mm riprodotta. Tavola SVG/PNG
controllata visivamente. Nessuna nuova verifica ERC/DRC: i file KiCad non sono cambiati.

## Evidenze storiche

La [verifica Z del vecchio RCA](cad/gate-b-height-check.pdf), la [vista diagnostica](cad/gate-b-cad.png)
e le [misure precedenti](cad/gate-b-height-check.json) documentano il deficit di 0,9 mm
(30,6 necessari contro 29,7 disponibili) e la penetrazione di 0,4 mm nel fondo.
Sono evidenze storiche, non una variante attiva. La precedente richiesta di doppia uscita RCA L/R
è superata dalla scelta mono NRJ4HF; non resta alcuna correzione OUTL→L / OUTR→R da implementare.

La [PR #17](https://github.com/Capobuf/ESP-DubSiren/pull/17) è stata integrata su `main`
con il commit `aafeeba`. La [issue #14](https://github.com/Capobuf/ESP-DubSiren/issues/14)
resta aperta: il merge della sostituzione non approva automaticamente il Gate B.
La prosecuzione sul branch `codex/issue-14-jack-fit` modifica CAD e documentazione;
schema, PCB e firmware conservano il contenuto integrato dalla PR #17.
