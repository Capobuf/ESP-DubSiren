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

## Misure del nuovo studio posteriore

Il riferimento provvisorio lega CAD e PCB con Xcad=Xpcb−62,5, Ycad=61,39−Ypcb,
F.Cu a Z23,8 e B.Cu a Z22,2, spessore nominale 1,6 mm. L'origine Z è il fondo esterno.
Questo riferimento allinea le interfacce posteriori; **non certifica il placement della PCB completa rispetto al Gate A**.

Lo STEP Neutrik originale è orientato verso +Y. La nuova proposta CAD sposta
il jack di **−4,5 mm lungo X**, portando l'asse da X17 a **X12,5**, per liberare
i piedini dall'inviluppo del potenziometro frontale in alto a destra.
Centri dei pin nel CAD proposto: T=(19,73;41,05), TN=(8,30;41,05),
S=(19,73;53,75), SN=(8,30;53,75), tutti sul piano Z22,2.
La posizione equivalente sulla PCB sarebbe **(82,23;20,34)**: non è stata
trasferita a KiCad, che conserva il placement della PR #17 fino alla verifica meccanica.
Il confronto STEP con differenza volumetrica zero della PR #17 resta un'evidenza
storica della posa precedente, non una dichiarazione di allineamento della nuova proposta.

I sei oggetti `FrontPotEnvelope` riprendono la proiezione F.Fab 12 × 13,4 mm
del footprint PTV111-3 e la profondità del corpo di 7,5 mm dal disegno Bourns.
Sono **inviluppi conservativi**, privi dei dettagli di cavità, piedini e clip,
non modelli completi dei potenziometri. Nella posa precedente lo STEP del jack
intersecava l'inviluppo in alto a destra per 5,775 mm³, fra Z23,8 e Z26,2.
La traslazione proposta porta il volume comune a zero e la distanza minima
a 1,52 mm nominali; non certifica il montaggio del componente reale.
Il layout frontale Gate A rimane invariato.

| Verifica | Esito | Misura / evidenza |
|---|---|---|
| Collegamenti elettrici | PASS | T→OUTL/AUDIO_MONO; S→GND; TN/SN e OUTR NC; ERC 0 errori e 0 warning |
| Altezza del jack | PASS nominale; tolleranze NON VALIDATE | Corpo 12,7 mm; Z9,5…22,2; piedini fino a Z26,2; margine nominale 6,9 mm sul budget precedente 19,6; distanza reale dal fondo 3,7 mm |
| Allineamento posteriore | PASS nominale; foro NON VALIDATO per produzione | Asse proposto X12,5 / Z15,7; parete 2,0003 mm lungo asse; inclinazione 1 grado. Studio aggiornato a D12 su richiesta dell'utente; D11,2 resta il riferimento Neutrik |
| Interferenza USB-C | PASS statico; cavi NON VALIDATI | Distanza fra solidi jack e USB 19,1 mm; volume comune zero; inviluppo dado–USB 17,108 mm |
| Piedini jack e corpi pot frontali | PASS del solo screening nominale | Nessuna intersezione con i sei inviluppi; distanza minima 1,52 mm. Piedini/clip dei pot e tolleranze non inclusi |
| MASTER e vicini | NON VALIDATO | Gap proiettati: MASTER 39,57 mm lungo X; PCB DAC 10,65 mm lungo Y. Mancano verifica completa di albero/manopola, altezza DAC e componenti frontali completi |
| Inserimento PCB | FAIL delle pose provate; percorso completo NON VALIDATO | D12 libera il naso in varie pose, ma restano interferenze USB/boccole/corpo jack. Sagoma, supporti e assieme completo non definiti |
| Fissaggio tramite dado | Copertura D12 PASS nominale in proiezione; serraggio NON VALIDATO | Flangia D17,45: sovrapposizione radiale centrata 2,725 mm. Primo appoggio sul bordo; fessura assiale fino a 0,305 mm. Filettatura e accesso utensile da verificare |

La posa CAD attuale **non rappresenta un serraggio verificato**. La spalla del
jack è sul piano Y58 e la faccia interna del dado su Y60,345: separazione 2,345 mm
contro 2,000305 mm di parete lungo l'asse. Lo scarto fra i piani è 0,344695 mm.
La parete inclinata lascia distanze variabili: sul corpo, da circa 0,079 a 0,300 mm;
sull'inviluppo del dado, da circa 0,00042 a 0,305 mm. I valori sono geometria nominale,
non una previsione di deformazione o di tenuta. Non sono stati introdotti spessori,
rondelle o elasticità presunte per dichiarare il serraggio riuscito.

La cavità nominale resta 29,7 mm. Il nuovo corpo risolve il deficit di altezza del precedente RCA
nella posa diagnostica, ma **6,9 mm è un margine nominale**, non un bilancio completo di tolleranze
PCB/pot/connettore. Il corpo è a soli **0,0785 mm dalla parete originale** nella posa scelta:
non è una riserva di produzione adeguatamente dimostrata. Il disegno Neutrik prescrive
il foro D11,2; lo STEP semplifica la filettatura in un cilindro liscio D11,2.
Il gioco nullo fra queste geometrie non dimostra il gioco reale del filetto nel foro.
Su richiesta dell'utente la proposta diagnostica
usa ora D12, con le verifiche e i limiti descritti sotto. Accoppiamento e serraggio
restano da verificare.
Le formule attive di `GateBHeight` usano ora il jack da 12,7 mm: stack nominale
22,8 mm, intervallo teorico F.Cu Z16,9…23,8 e margine 6,9 mm.
Questi numeri descrivono il solo bilancio assiale, non includono tutti gli ostacoli del box.

La scocca e il fondo Hammond originali non sono stati tagliati o sostituiti. L'oggetto
`RearAperturesStudy`, nascosto nella vista 3D, è una sola sonda diagnostica con aperture ipotetiche,
usata dalla tavola posteriore. Con queste aperture il volume di collisione statico jack/scocca e
USB/scocca è zero; ciò non dimostra l'inserimento. Il dado è un inviluppo, non una filettatura verificata.
MASTER e DAC sono solo proiezioni dichiarate. Non sono stati inventati modelli completi mancanti.

Sequenza richiesta ancora da dimostrare: PCB popolata inserita inclinata dal fondo → ingresso
contemporaneo di jack/USB/MASTER → raddrizzamento → fissaggio → dado esterno. Mancano sagoma,
supporti e ingombri completi per provare un percorso continuo. Gli angoli e le traslazioni
provati sotto sono sonde diagnostiche, non una sequenza approvata. La dima 3D/STL non è pronta.

La prima **verifica parziale indipendente dal display**, conservata come riferimento D11,2, usa sei cilindri
M9 × 5 mm che rappresentano le sole boccole dei potenziometri, con base Z31,3;
le aperture frontali D9,2 sono quelle proposte al Gate A. La sonda
`MountingAperturesProbe` è una copia diagnostica della scocca, non modifica l'originale.
Nella posa finale jack e boccole non intersecano questa sonda. Traslando il gruppo
di 0,2 mm verso il fondo, il jack interseca la scocca per 4,480 mm³; arretrandolo
di 0,2 mm lungo −Y, le boccole la intersecano per 7,347 mm³.
Queste due prove escludono soltanto i movimenti rettilinei testati nel modello
nominale ridotto: **non dimostrano che un percorso inclinato sia impossibile**.
Il filetto semplificato del jack e i giochi ancora da verificare limitano ulteriormente
la prova. Nessuna sequenza completa è stata approvata.

### Inserimento inclinato e foro D12 — studio attuale

Le prove FreeCAD MCP ruotano i componenti attorno all'asse X passante per
**(12,5; 59,192132; 15,7) mm**, a metà parete nel foro jack. Il dado è escluso
dal movimento perché montabile dall'esterno dopo l'inserimento.
Con D11,2 il naso cilindrico semplificato interseca la parete a tutti gli angoli
campionati ±0,1°, ±0,5°, ±1°, ±2°, ±5°, ±10°, ±15°; anche 32 combinazioni
di inclinazione e traslazione assiale falliscono. Questi dati restano nel JSON
come riferimento precedente; non sono i risultati del foro attuale.

**Su richiesta dell'utente il foro diagnostico è ora D12.** Il naso D11,2 dispone
di 0,4 mm nominali per lato. È stata aggiornata l'unica proposta CAD e la tavola;
gli originali Hammond restano intatti. Il foro D12 libera il naso fino a +10°
nei campioni provati, ma non risolve le altre interferenze:

| Posa rispetto al riferimento | Jack/scocca mm³ | Boccole/scocca mm³ | USB/scocca mm³ |
|---|---:|---:|---:|
| Finale | 0 | 0 | 0 |
| Abbassamento Z−0,2 mm | 0 | 0 | 2,707 |
| Inclinazione +0,5° | 0 | 3,081 | 0,031 |
| Inclinazione +5° | 4,478 | 75,542 | 0,252 |
| Inclinazione +15° | 36,387 | 0 | 0,399 |
| +0,5°, Y+0,1 mm, Z−0,4 mm | 0,006 | 0 | 4,558 |

L'apertura USB è ancora il solo inviluppo minimo, senza gioco validato: i suoi
esiti non dimostrano un'impossibilità del componente reale. Anche le boccole
sono cilindri nominali e il corpo jack proviene da uno STEP semplificato.
I campioni escludono soltanto le pose e le famiglie di movimento provate;
non costituiscono una ricerca esaustiva a sei gradi di libertà né la validazione
di un percorso continuo dell'assieme completo.

Il gruppo nativo `InclinedInsertionProbe`, nascosto nella posa finale, conserva
la sonda +5° con D12: naso blu, boccole arancio, volumi di interferenza rossi.
Un controllo indipendente ha individuato un falso volume comune nullo usando
una boccola semplicemente ruotata. Le boccole sono state quindi ricostruite
come cilindri analitici dalla base e dall'asse trasformati: il punto
(−28; 36,5; 32,8) è interno sia alla prima boccola sia alla scocca e conferma
l'interferenza a +5°. La sola validità OCC non è stata usata come prova di assenza
di collisioni. Il report conserva metodo e controverifica.

### Copertura del foro e serraggio senza fessure

Il disegno originale NRJ-NUT-B mostra una **flangia circolare D17,45** e una
chiave SW15. Su D12, la copertura radiale proiettata è **2,725 mm a foro centrato**,
oppure 2,325 mm con eccentricità nominale di 0,4 mm. Quindi il bordo del foro
è coperto nella proiezione lungo l'asse del jack; non si è usato il solo esagono
SW15 come se fosse la flangia. Sono valori nominali, senza tolleranze di lavorazione.

Questo non garantisce l'assenza di fessure sotto il dado. La parete è inclinata
di 1° rispetto alle facce di appoggio: il jack raggiunge il primo contatto dopo
**+Y0,078522 mm**, il dado dopo **−Y0,000420 mm**. Il primo appoggio avviene sul
bordo, non su tutta la faccia. Nel modello rigido resta una variazione del gioco
di **0…0,304591 mm** sul diametro del dado. Avanzare ancora 0,01 mm produce
interferenza: circa 0,045 mm³ al corpo jack e 0,010 mm³ all'inviluppo del dado.
Le distanze della spalla sono calcolate sulla sua faccia, non sull'intero jack
che nel riferimento D11,2 tocca già il bordo del foro.

Non sono state assunte deformazioni della plastica, coppie di serraggio,
impegno del filetto o rondelle aggiuntive. Il requisito dell'utente — nessun
vuoto visibile a montaggio concluso — **rimane da soddisfare**.

Le due strade da valutare, in ordine di impatto, sono:

1. Conservare componenti e D12, definire i giochi effettivi delle altre aperture
   e studiare appoggi che compensino la parete di 1°, verificando poi nuovamente
   il percorso completo. Non è ancora dimostrato che basti; nessun appoggio
   aggiuntivo è stato progettato o approvato.
2. Se il vincolo della PCB già popolata impedisce il percorso, concordare una
   modifica del fissaggio/connettore o della sequenza di assemblaggio. È una
   modifica ai vincoli della #14, non applicata automaticamente.

Le dimensioni fisiche del display restano aperte e non partecipano a queste prove.

### Confronto fori potenziometri: D9,5 e D10

Su richiesta dell'utente sono stati confrontati D9,5 e D10 con D9,2, per dare
gioco alle boccole M9 × 0,75 dei PTV111-3 frontali. Il PTV111-1 del MASTER usa
la stessa filettatura, ma il suo montaggio completo sul retro resta da modellare.
**D10 è il candidato preferito per lo studio**, con 0,5 mm nominali per lato.

| Foro | Gioco radiale su M9 | Copertura Davies centrata | Copertura Davies al massimo decentramento nominale |
|---|---:|---:|---:|
| D9,2, riferimento Gate A | 0,10 mm | 5,322 mm | 5,222 mm |
| D9,5 | 0,25 mm | 5,172 mm | 4,922 mm |
| D10 | 0,50 mm | 4,922 mm | 4,422 mm |

La gonna della Davies 1100 è D19,84375 (25/32 pollici nel catalogo originale).
Questi sono margini di copertura in proiezione; l'altezza della manopola e il
serraggio devono ancora impedire aperture visibili lateralmente. Per la Cliff
K85M del MASTER, D14 esterno, i margini corrispondenti su D10 sono 2 mm centrato
e 1,5 mm al massimo decentramento nominale. Non sono incluse tolleranze reali.

Con jack D12, D9,5 elimina l'interferenza delle sole boccole a +0,5° ma non a
+1° (2,554 mm³). D10 le lascia libere anche a +1°; a +2° resta un'interferenza
di 5,698 mm³, a +5° di 48,607 mm³. I due risultati nulli sono stati verificati
anche tramite contenimento delle sezioni ellittiche nei fori, indipendentemente
dal booleano OCC: riserva radiale conservativa 0,089 mm per D9,5/+0,5° e
0,172 mm per D10/+1°. Queste prove non eliminano le interferenze del jack e
dell'USB né dimostrano il percorso completo.

La manopola non sostituisce dado e rondella. Il riferimento già conservato
Alps RKZ00023UA, **non selezionato come ferramenta del progetto**, ha rondella
D14 × D9,1 × 0,5 e dado M9 × 0,75, SW11, spessore 2. La rondella coprirebbe
D10 per 2 mm radiali a centraggio nominale. Tuttavia la boccola frontale sporge
solo 2,5 mm dal pannello nella posa attuale: dado e rondella occuperebbero tutti
quei 2,5 mm, senza riserva assiale dimostrata. Per il MASTER, gli spigoli del dado
SW11 arrivano a circa D12,70, oltre l'incavo D12 della Cliff: non si può dichiarare
che questa ferramenta rientri nella manopola. Fissaggio, accesso e distanza dal
pannello rimangono quindi da risolvere, senza presumere un montaggio senza vuoti.

Il confronto D10 è salvato nell'oggetto nativo nascosto `PotHole10Comparison`
e nel report. **La dima Gate A e la sonda principale mantengono D9,2 come riferimento**;
D10 è un candidato verificato parzialmente, non una foratura già rilasciata.
Non è stato duplicato il layout né cambiata la posizione dei controlli.

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
