# Issue 14 — conclusione meccanica della configurazione corrente

Analisi nominale del 18 ottobre 2026, prosecuzione di `3112cd6`, PR #18.
L'unico assieme attivo resta [assembly.FCStd](assembly.FCStd).

**La posa finale non soddisfa il requisito di appoggio del dado jack senza
fessure nel modello rigido, anche se si trovasse una traiettoria d'ingresso.**
Il piano di appoggio del dado e la parete Hammond differiscono di 1°:
al primo contatto rimangono fino a **0,304591 mm** di vuoto sotto la flangia.
Questo è un impedimento all'esito completo richiesto, limitato all'accoppiamento
rigido diretto e alla registrazione corrente; non è una prova d'impossibilità
di tutti i percorsi 3D. Non si assume che il dado in plastica recuperi il vuoto.

Inoltre, il gruppo encoder acquistato ha un dislivello finora non rappresentato:
la gonna SAK-023 può scendere nominalmente solo a **9,9 mm sopra il pannello**.
L'appoggio della spalla encoder è **1 mm sotto il tetto interno**. Copertura
in pianta, appoggio dei dadi, serraggio e quota delle manopole sono verifiche
distinte. Il display rimane aperto senza una nuova richiesta di misure.

| Verifica | Esito |
|---|---|
| Registrazione scocca originale, labbro e colonne | PASS nominale |
| Sonda con tutte le aperture dello sketch | PASS, corrette le omissioni |
| Battuta dei sei pot sulla scocca | PASS nominale, distanze nulle dopo riapertura |
| Appoggio diretto rigido del dado jack senza fessure | FAIL, vuoto fino a 0,304591 mm |
| Quota SAK e serraggio encoder | Distacco 9,9 mm dimostrato nominalmente; appoggio interno 1 mm da risolvere |
| Percorso completo del blocco popolato | NON DIMOSTRATO, nessuna conclusione globale di impossibilità |
| Accoppiamento verticale display | DA VERIFICARE, due quote fisiche null |

## Geometria verificata e correzioni

| Elemento | Evidenza e ambito |
|---|---|
| Scocca originale | 414 facce e 708 vertici corrispondono allo STEP Hammond dopo la trasformazione dichiarata. Residuo massimo sui vertici 5,86×10⁻¹⁶ mm; differenza di volume 7,28×10⁻¹² mm³. Labbro, colonne e nervature sono inclusi. Il fondo e le quattro viti sono esclusi dal moto d'inserimento. |
| Sonda forata | La precedente sonda mancava dei fori encoder, display e quattro pulsanti. Ora deriva da tutte le **12 regioni chiuse** di `Openings` e dalle tre aperture posteriori. Volume 42 415,298 mm³, un solido valido. I centri delle sei aperture mancanti sono liberi. Nessuna apertura del layout è stata ridimensionata. |
| J2 | STEP originale Neutrik, con corpo, naso e terminali, nella proposta CAD X12,5/Z16,2/Y58. Nessun trasferimento a KiCad. |
| USB | STEP della libreria KiCad già presente. Apertura corrente 8,94×3,45 mm, al solo ingombro nominale del corpo; non definisce il gioco necessario al moto né il passaggio del cavo. |
| Sei pot frontali | Battuta Z31,8 e PCB Z24,3/22,7 conservate. Corpi 12×13,4×7,5 e boccole M9×5 sono inviluppi: clip, terminali e profilo dettagliato delle spalle non sono completi. Gli alberi Ø6 omettono flat/smussi e posizione angolare. Collisioni degli inviluppi non sono automaticamente collisioni del pezzo reale. |
| MASTER | Inviluppi nella proposta X−31/Z12,7/Y58 conservati. Non sono un modello completo di terminali, appoggio e ferramenta. |
| Encoder | Aggiunto lo stack assiale PEC11R-4120F-S0018 dai disegni Bourns: corpo 6,5, boccola 7, albero L20. Il nuovo corpo è un inviluppo, non una certificazione dell'orientamento dei terminali. La SAK è rappresentata dal limite della gonna ed è montata dopo l'inserimento. |
| Resto del blocco | Mancano una sagoma PCB chiusa, ingombri montati di DAC/display/CHERRY, terminali e saldature significative, ESP32 e gruppi di componenti minori. Un piano di riferimento non sostituisce la PCB. |

Le sezioni native `BottomAccessContour` e `PCBCavityContour` mostrano i contorni
liberi, comprensivi degli ostacoli della scocca, a Z4 e Z22,7. Gli estremi XY
sono rispettivamente ±45,746/±58,396 e ±45,420/±58,070 mm. **Questi estremi non
definiscono rettangoli utilizzabili**: occorre rispettare il contorno attorno
alle colonne e verificare tutto il moto. Le sezioni non sono una nuova sagoma
PCB approvata.

Le BoundBox OCC di alcune superfici rifilate risultano molto più grandi della
materia reale. Non sono state usate per dedurre ostacoli. Anche una booleana
fra due copie quasi coincidenti della scocca ha restituito un volume negativo:
quel risultato è stato scartato. La corrispondenza con l'originale è verificata
sui vertici, sulla topologia e sul volume, non su quel falso risultato.

## Ricerca di movimento e limite della conclusione

È stata cercata una via di estrazione dalla posa finale, reversibile in
inserimento. La trasformazione è:

`p' = P + Rz(yaw) Ry(roll) Rx(pitch) (p−P) + (dx,dy,dz)`

con `P=(12,5;59,183404867;16,2) mm`. Il centro serve solo a parametrizzare il
moto: **tutte e tre le traslazioni sono variabili**, così come roll e yaw.
Il controllo locale usa pitch da −4° a +4°, traslazioni ±5 mm, roll/yaw ±5°,
cinque inizializzazioni per angolo, 32 generatrici per sezione di boccola e
vertici dell'imboccatura USB. Include encoder e MASTER oltre ai sei pot e al
jack. I corpi semplificati restano conservativi.

Il candidato positivo passa per queste trasformazioni nominali:

| Pitch ° | dx mm | dy mm | dz mm | Roll Y ° | Yaw Z ° |
|---:|---:|---:|---:|---:|---:|
| 0 | 0 | 0 | 0 | 0 | 0 |
| 0,25 | 0,001638 | 0,026827 | −0,002001 | −0,000521 | 0,119484 |
| 0,50 | 0,001637 | 0,104159 | 0,080444 | −0,001042 | 0,119450 |
| 0,75 | 0,001293 | 0,139926 | −0,005122 | −0,001224 | 0,093492 |

Interpolando il primo tratto, al campione **pitch 0,025°**, con
`t=(0,000163770;0,002682742;−0,000200052) mm`,
`roll=−0,000052135°`, `yaw=0,011948440°`, la verifica nativa trova
**0,00423248 mm³ di interferenza USB–scocca**. Il candidato è respinto;
non viene dichiarato un tratto continuo libero sulla base dei soli campioni.
Per pitch negativo non emerge un candidato non nullo nello screening locale.
La controverifica inverte gli operandi OCC e riproduce lo stesso volume;
la forma comune è valida e contiene un punto verificato interno a entrambi
i solidi, registrato nel JSON. Non si usa un semplice zero booleano come prova.

Le intersezioni native usano la scocca con tutte le aperture. I cilindri sono
ricostruiti da base e asse trasformati. La ricerca numerica non certifica un
ottimo globale; quando una generatrice esce dalla lunghezza della boccola,
il modello di quella fase non è applicabile. Questi casi sono marcati nel JSON.
**Nessuno di questi esiti dimostra l'assenza di un diverso percorso 6DOF.**
Una ricerca esaustiva del blocco incompleto non risolverebbe comunque il
difetto indipendente del serraggio finale.

## Appoggio posteriore — impedimento dimostrato nel modello rigido

Dalle facce piane dello STEP, in mm:

`parete interna: y + tan(1°) z = 58,466024591`

`parete esterna: y + tan(1°) z = 60,466329247`

Il dado coassiale al jack ha piano di appoggio `y = costante`. La flangia è
Ø17,45, secondo il disegno Neutrik. Il primo contatto senza penetrazione è
a **Y60,335852636**, sul bordo inferiore Z7,475. Il vuoto è zero in quel punto,
0,152295 mm alla quota dell'asse e **0,304591 mm al bordo opposto Z24,925**.
Il testimone anulare nativo esterno al foro Ø12 ha volume comune zero e
distanza dalla scocca inferiore a 10⁻¹³ mm: il primo contatto non è un appoggio
anulare completo.

La differenza è `17,45 tan(1°)`. Spostare il dado o il jack in Y cambia solo
il valore iniziale del vuoto, non la sua pendenza. Stringere oltre il primo
contatto richiede deformazione o interferenza. Inclinare l'intera PCB di 1°
abbandonerebbe la battuta simultanea dei pot sul tetto piano. Il requisito
rigido senza fessure richiede quindi una modifica dell'interfaccia, indipendente
dal percorso d'ingresso. La copertura radiale del foro resta 2,725 mm, ma non
risolve questo impedimento.

### Due alternative posteriori, ordinate per invasività sulla scocca

**A — Compensatori inclinati interni ed esterni, mantenendo la scocca.**
Sono componenti da progettare e approvare, non accessori già selezionati.
Come esempio revisionabile, scegliendo un minimo progettuale di 0,5 mm:

- Jack: compensatore interno 0,500…0,721679 mm e arretramento del jack verso
  l'interno di **0,430205 mm**. Compensatore esterno 0,500…0,804591 mm.
  La punta del naso supera ancora l'inviluppo esterno del dado di **0,833942 mm**;
  ciò non certifica il filetto utile o la coppia.
- MASTER: interno 0,500…0,791500 mm; esterno Ø14 di riferimento
  0,500…0,744371 mm. Con lo stesso arretramento rimangono **1,702959 mm** di
  boccola oltre l'appoggio esterno. Il riferimento Alps dado+rondella 2,5 mm
  **non entra**, con deficit 0,797041 mm. Occorrono una ferramenta verificata
  entro quel budget, un'altra combinazione di spessori dimostrata oppure una
  diversa boccola. L'opzione non è una soluzione pronta per entrambi i componenti.

**B — Appoggi locali paralleli ricavati nella scocca originale.**
Richiede autorizzare espressamente le lavorazioni finora escluse. Non sono
state applicate al CAD:

- Jack: spianatura esterna sull'impronta Ø17,45, profondità massima
  **0,304591 mm**; interna sul campo di appoggio alto 12,7 mm, massimo
  **0,221679 mm**. I piani risultanti lasciano **1,739788 mm** assiali fra loro.
  La spalla deve avanzare da Y58 a **Y58,291474**. Sono quote geometriche;
  resistenza, utensile, bordo residuo e raggi d'appoggio richiedono verifica.
- MASTER, usando ancora il riferimento esterno Ø14 e l'inviluppo interno:
  massimi **0,244371 / 0,291500 mm**, parete fra i piani **1,761170 mm**;
  spalla Y58,361294 e boccola disponibile all'esterno **3,238830 mm**.
  Le quote vanno rifatte sull'effettiva superficie d'appoggio e ferramenta.

Entrambe richiedono riesaminare traiettoria, terminali e fissaggi dopo la
scelta. L'elasticità del dado non viene presentata come una terza soluzione
in assenza di dati o di una prova controllata.

## Encoder e manopola

Fonti: Bourns PEC11R pagine 2–3, variante `4xxxF-Sxxxx`;
Same Sky SAK-023 pagina 1. Nessuna quota è ricavata misurando pixel.

| Quota nominale | mm |
|---|---:|
| PCB superiore | Z24,3 |
| Corpo encoder | 6,5 |
| Spalla encoder | Z30,8 |
| Tetto interno / esterno | Z31,8 / Z33,8 |
| Boccola, tabella L20 | 7; termina a Z37,8 |
| Albero L20 | Punta Z50,8 |
| Profondità SAK fra gonna e fondo vano albero | 7,1 |
| Gonna alla massima inserzione ideale | Z43,7; **9,9 sopra il pannello** |
| Sporgenza inferiore dell'inserto SAK | 1,1; scende solo a Z42,6 |
| Corsa pulsante encoder | 0,5; gap gonna ancora 9,4 a fondo corsa |

Questo è un limite inferiore nominale del distacco: un arresto anticipato
dell'accoppiamento può solo aumentarlo. Non impone un valore estetico di gioco
scelto dall'utente, ma rende revisionabile l'esito della combinazione acquistata.
La rondella esterna può coprire il foro senza far scendere la gonna.

Il dado encoder non va usato per tirare la PCB contro il suo corpo:
la battuta è già definita dai pot. Serve un **appoggio interno nominale da 1 mm**
attorno alla boccola, da dimensionare e approvare. I 4 mm esterni di boccola
lasciano nominalmente 1,5 mm dopo dado 2 e rondella 0,5; non è una verifica di
filetto utile, tolleranze o coppia.

Due alternative concrete per una gonna vicina al pannello:

1. **Conservare SAK e approvare la rilavorazione dell'albero.** A titolo di quota
   di studio, per 0,5 mm fra gonna e pannello servirebbe L≤10,6 mm, quindi
   rimuovere almeno **9,4 mm**. Il flat originale è lungo 10 mm: il solo taglio
   ne lascerebbe **0,6 mm**, perciò richiede anche rifacimento del flat/smusso.
   I 0,5 mm non sono un gioco finale approvato: vanno aggiunti i margini per
   corsa, tolleranze e inserto inferiore da 1,1 mm, che può interferire con la
   ferramenta. Nessuna istruzione di lavorazione definitiva è emessa.
2. **Conservare PEC11R L20 e scegliere una manopola diversa.** Per lo stesso
   esempio di gioco 0,5 mm occorre profondità del vano albero **almeno 16,5 mm**,
   con incavo compatibile con boccola, dado e rondella e con il profilo reale
   dell'albero. Nessun modello alternativo è selezionato. Passare al solo L15
   della serie lascia ancora **4,9 mm** con la SAK attuale.

L'accoppiamento a pressione non è garantito dall'etichetta «Ø6 D»: SAK specifica
Ø5,96…5,98 e piatto 4,46…4,48; Bourns indica Ø6±0,1 e piatto 4,5±0,05.
Gli intervalli si intersecano ma non coincidono. Per il MASTER, Cliff quota
5 A/F mentre Bourns PTV quota 4,5 con tolleranza negativa; tenuta e gioco
richiedono una verifica dedicata. La quota Cliff 12,5 arriva a uno scalino
interno e **non viene arbitrariamente trattata come profondità totale del foro**.

## Cosa serve per la verifica successiva

**Decisioni e dati effettivamente assenti:** strategia d'appoggio posteriore;
ferramenta M9 reale e relativi appoggi; quota desiderata/accettabile delle
manopole e relativo rimedio encoder; forma/corsa dei keycap reali; gioco
funzionale dell'apertura USB e ingombro del cavo; fissaggio e distanziamento
del DAC; una sagoma PCB candidata e le posizioni posteriori da approvare.

I tagli pulsanti 20,4 mm superano già di 0,2 mm per lato la **riserva** keycap
20 mm: con tale riserva non si può dichiarare il foro coperto. Questo non è
una misura dei keycap reali e non autorizza a modificare i tagli.

**Lavoro CAD ancora da eseguire con fonti disponibili:** completare clip,
terminali, CHERRY e componenti significativi; registrare le altezze sul lato
corretto; includere saldature e tolleranze; costruire una PCB chiusa e quindi
verificare il percorso completo con margini. Non si presenta questo lavoro
incompleto come richiesta di nuove misure all'utente.

**Display differito:** spessore fisico PCB e distanza PCB superiore–vetro
rimangono `null`. Eagle 1,57 mm resta solo nominale; 5,4 mm complessivi non
separano le due quote. Il display impedisce l'approvazione verticale finale,
ma non influenza la prova del contatto jack né il calcolo encoder sopra.

## Evidenze riproducibili

- [Report numerico](nrj4hf-validation.json): `active_study.mechanical_conclusion`;
  stessa copia in `MountingProbeResults.SeatedStudyJSON`.
- [Controllo FreeCAD](issue14-mechanical-check.py): eseguibile via MCP, senza
  modificare documenti, funzioni `audit`, `search_engaged_phase`,
  `native_candidate_screen`. Il solo screening locale impiega pochi secondi;
  non è un certificatore del percorso completo.
- [Tavola posteriore](nrj4hf-rear-section.svg), [sezione encoder](issue14-encoder-section.svg).
- Le precedenti prove +3°/+5° restano diagnostiche storiche di una fase e non
  vengono elevate a prova globale. `historical_baseline` è conservato.

Verifica finale: assieme salvato e riaperto via MCP; otto forme critiche valide;
sei raggi frontali da 5 mm e sei contatti pot nulli; corrispondenza del JSON
nativo con quello su disco; confermate registrazione jack, aperture complete,
punta encoder Z50,8 e prova del contatto posteriore. Tavole SVG/PNG e viste
3D controllate visivamente. Script verificato sintatticamente e `git diff
--check` superato. L'altro documento FreeCAD aperto è stato lasciato intatto.
Nessun ERC/DRC o test firmware aggiuntivo: i relativi file non sono cambiati.

Non sono stati cambiati contenitore, componenti, layout approvato, file KiCad
o firmware. Le alternative non sono applicate. PR #18 resta in bozza;
nessun merge, routing, Gerber, acquisto o dima STL di produzione.
