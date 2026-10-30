# Issue #35 — proposta PCB standalone

Proposta del **30 ottobre 2026**, nell'unico [PCB attivo](../kicad/ESP-DubSiren.kicad_pcb):
**86 × 126 mm, angoli R3, quattro fori M3, nessun routing/via/zona**.
È il punto di revisione del placement, non un assieme qualificato o un file fabbricabile.
Il colore previsto è **nero**; i render KiCad conservano il verde predefinito.

**Aggiornamento approvato dall'utente:** lunghezza 126 mm; pulsanti e fori
anteriori fermi, fila inferiore dei potenziometri spostata di 3 mm verso il retro,
gruppo superiore e connettori posteriori di 6 mm. R11–R13 seguono la fila
inferiore di 3 mm. I margini nominali diventano **4,08 mm verso i keycap** e
**4,34 mm fra display e manopola centrale inferiore**.
Il [rapporto meccanico](issue-35-mechanical-clearance-review.md) conserva la
verifica prima/dopo e gli accoppiamenti manopola/albero ancora da qualificare.

La lunghezza supera di 14 mm il riferimento indicativo 86 × 112: i primi 8 mm
separavano i terminali posteriori dai potenziometri superiori; i successivi 6 mm,
autorizzati dopo la verifica degli ingombri, aumentano i margini fra le file dei
comandi. Passi orizzontali 28 mm e 22 mm invariati. Hammond, traiettoria di
inserimento e dima non sono requisiti.

## Viste e coordinate

- **[Comandi frontali con manopole e copritasti](layout-35/assembled-top.png)** / [SVG](layout-35/assembled-top.svg): proiezione XY in scala, superfici illustrative; copritasti provvisori 20×20 mm. MASTER e retro omessi, altezze non validate.
- **[Top 2D](layout-35/F-layout.png)** / [SVG](layout-35/F-layout.svg).
- **[Bottom 2D, specchiato](layout-35/B-layout.png)** / [SVG](layout-35/B-layout.svg).
- [Render 3D top](layout-35/top-3d.png) e [bottom](layout-35/bottom-3d.png).
  I modelli mancanti di pot, encoder, CHERRY, ESP32 e breakout rendono questi
  render **parziali**: l'assenza visiva di un corpo non dimostra spazio libero.
- [Proiezione longitudinale degli appoggi](layout-35/support-sections.svg):
  quote nominali e condizioni, non simulazione del montaggio.
- [Tutte le posizioni dei footprint](layout-35/positions.csv),
  [pad finali](layout-35/pads-final.json).

L'origine KiCad precedente è conservata: bordo posteriore a **Y=−6**,
bordo anteriore a **Y=120**, X da 0 a 86 mm. X verso destra, Y verso i pulsanti. Tutte le coordinate sotto sono **viste da sopra**,
anche per i componenti B.Cu. Spessore PCB di riferimento 1,6 mm, da confermare
nel futuro ordine di produzione.

| Elemento | X; Y in mm | Faccia / orientamento |
|---|---|---|
| H1, H2 | 5;−1 — 81;−1 | Ø3,2 NPTH, M3 |
| H3, H4 | 5;115 — 81;115 | Ø3,2 NPTH, M3; interassi 76 × 116 |
| RV1–3, assi | 15;18 — 43;18 — 71;18 | F.Cu, 0° |
| RV4–6, assi | 15;76 — 43;76 — 71;76 | F.Cu, 0° |
| Display U5 | anchor 44,016;46,5; area attiva 43;46,5 | F.Cu, 0° |
| Encoder SW7, asse | 75;46,5 | F.Cu, 0° |
| SW1–4, **steli** | 10;100 — 32;100 — 54;100 — 76;100 | F.Cu; anchor pad1 = stelo+(2,54;−5,08) |
| MASTER RV7 | 16;0 | B.Cu, 0°; albero verso il posteriore |
| USB-C J1 | anchor 40;2,61 | B.Cu, 0°; apertura verso il posteriore |
| Mono J2 | pad T 71,23;9 | B.Cu, 90°; apertura verso il posteriore |
| DAC U6 | 13;49,5 | B.Cu, 90° |
| ESP32 U1 / mux U3 | 43;46 / 43;69 | B.Cu, 0° |
| RESET SW5 / BOOT SW6 | 81;64 / 81;80 | B.Cu, 90°; accesso dal bordo destro |

Rispetto al Gate A, una traslazione di riferimento sarebbe Xpcb=Xcad+43,
Ypcb=56−Ycad. **Non è stata copiata**: pot superiori Y14→18, display/encoder
Y44→46,5, inferiori Y71→76, steli Y96,5→100. Il vecchio placement KiCad
aveva inoltre passo pot35 mm e steli a Y122: le coordinate attuali sono state
verificate dall'esportazione nativa, non ricavate solo dal CAD storico.

## Ingombri e spazio di manovra

Su `Dwgs.User` sono presenti inviluppi delle manopole, area attiva TFT,
riserva keycap e quattro aree **8 × 8 mm** centrate sui fissaggi. Sono
annotazioni di progetto da rispettare su **entrambe le facce**: niente rame,
piste, componenti, saldature o cavi in quelle aree, eccetto il foro stesso.
Il [controllo geometrico dei pad](layout-35/m3-check.json) non trova intrusioni.
Le aree non sono zone di rame né regole automatiche del router: prima del
routing occorre tradurle in vincoli nativi e verificarli con la ferramenta scelta.

Usare teste/rondelle con ingombro massimo Ø7 mm entro la riserva 8 mm;
la misura è un limite di selezione, non la certificazione di una vite non scelta.
Rimangono 1 mm fra il bordo della riserva e il bordo PCB. Distanziali M3
esagonali AF5,5 hanno diagonale 6,35 mm e rientrano nella riserva.

| Inviluppo | Fonte e distanza nominale |
|---|---|
| Davies 1100 | Gonna Ø19,84375; corpo Ø16,66875; altezza manopola 11,90625 mm dal [catalogo p.24](../cad/sources/Davies-Catalog-Instrument-Knobs.pdf). Passo28: spazio laterale 8,15625 mm. L'accoppiamento del foro 1/4″ con albero e grano resta da provare. |
| SAK-023 | Ø14,5 nominale dal [disegno](../cad/sources/SameSky-SAK-023.pdf). Fra il bordo PCB del display e l'inviluppo encoder restano circa 5,3 mm. Accesso dita e corsa click richiedono prova. |
| CHERRY/keycap | Corpo CHERRY documentato; keycap **non identificato**: riserva20×20, passo22, gap2 mm. I cappucci estremi arrivano nominalmente ai bordi laterali. Fra riserva keycap e riserva vite anteriore resta1 mm. Distanza minima nominale dalle gonne inferiori4,08 mm. Non garantisce ergonomia per ogni cappuccio. |
| Display 5393 | PCB Eagle36,83×30,48; ingombro prodotto web37,3×30,5. Area attiva Eagle32,35×17,4. Il contorno TFT Eagle e la quota web non coincidono: usare il pezzo per qualificare il montaggio. Gap nominale dalla PCB breakout alla gonna dei pot inferiori circa4,34 mm. |
| DAC 6250 | PCB Eagle30,48×20,32, prodotto con jack32,5×20,3. Ruotato sul retro a sinistra; il suo jack3,5 non è una seconda uscita di pannello. Non sono previsti cavi su quel jack nel normale montaggio mono. |
| Retro | USB, NRJ4HF e MASTER sono disposti lungo il bordo posteriore. Il corpo del jack sporge oltre il bordo: è intenzionale. I cavi reali, il raggio di curvatura e la forza di inserzione non sono qualificati dal courtyard. |

I pad TP1–7 restano sul retro e leggibili nella vista inferiore. La sonda e
RESET/BOOT richiedono accesso da sotto o dal lato: non si promette la pressione
con un dito dall'alto. Alimentazione e passivi sono raggruppati sul retro;
il loro placement resta da rifinire in funzione dei percorsi quando il routing
sarà autorizzato, soprattutto per USB, decoupling e ritorni analogici.

## Altezze documentate e incognite fisiche

| Elemento | Quota disponibile | Cosa manca per lo stack reale |
|---|---|---|
| NRJ4HF B.Cu | Corpo12,7 mm sotto B.Cu, terminali4 mm dal piano B.Cu: [STEP/disegno verificati](../cad/nrj4hf-validation.json) | Tolleranze, saldature, gioco del connettore e del cavo. I terminali attraversano la PCB e sporgono sul fronte. |
| DAC6250 B.Cu | Altezza **prodotto**6,2 mm, [Adafruit](https://www.adafruit.com/product/6250) | Aggiungere distanza carrier–modulo, header, terminali e saldature: 6,2 non è l'altezza assemblata sotto la sirena. |
| Display5393 F.Cu | Altezza **prodotto**5,4 mm, [Adafruit](https://www.adafruit.com/product/5393) | Ripartizione sopra/sotto PCB breakout, microSD, connettore flex, header e sostegni. |
| ESP32 B.Cu | 3,2±0,15 mm da [Espressif, disegno dimensionale 1U](esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf) | Saldatura, connettore antenna e cavo. Nessun cavo antenna inventato. |
| PTV111-3 F.Cu / MASTER PTV111-1 B.Cu | Variante L20, geometrie diverse, terminali nominali3,5 mm nel [disegno Bourns p.2](../cad/sources/Bourns-PTV-PTT.pdf) | Montaggio reale, ganci, troncatura terminali, altezza albero/manopola e MASTER rispetto a B.Cu. Non si usa L20 come altezza sotto PCB. |
| Encoder / SAK | Disegni [PEC11R](../cad/sources/PEC11R.pdf) e [SAK](../cad/sources/SameSky-SAK-023.pdf) | Inserimento manopola, corsa click, ganci e saldature; stack non certificato. |
| CHERRY / keycap | [Disegno switch](../cad/sources/Cherry-MX2A-Black.pdf) e [layout](../cad/sources/Cherry-MX-Legacy-Layout.pdf) | Altezza keycap, corsa completa, terminali e saldature misurati. |
| Passivi / IC / pulsanti | Package e pad verificati nella [#34](issue-34-electrical-review.md) | Altezza finale di giunti, F1, SW5/6 e componenti saldati da rilevare sul prototipo. |

**Non esiste ancora un valore misurato di Hmax dell'assieme.** La quota12,7
del jack è il maggior ingombro sottoscheda finora quantificato, non la prova
che ogni elemento assemblato sia più basso. Perciò gli appoggi seguenti sono
opzioni commerciali dimensionate, da qualificare con misure, non altezze finali.

## Due configurazioni di appoggio

| Configurazione | Ferramenta commerciale candidata | Gioco nominale / condizione |
|---|---|---|
| Piana | 4× [Harwin R30-1612000](https://www.harwin.com/products/R30-1612000), M3 femmina/femmina, nylon66, L20, AF5,5; viti M3 compatibili e piedini antiscivolo | Ignorando il contributo favorevole dei piedini, B.Cu–tavolo20 mm e gioco jack7,3 mm. Accettare solo se ogni corpo/terminale/cavo resta entro17 mm da B.Cu, riservando almeno3 mm al tavolo. |
| Leggermente inclinata | H1/H2: 2× [R30-1612500](https://www.harwin.com/products/R30-1612500), L25; H3/H4: 2× R30-1612000, L20; stessi M3 e piedini cedevoli | Interasse116: inclinazione nominale atan(5/116)=2,47°. Con colonne normali alla PCB, quota minima nominale al bordo anteriore≈19,77 mm; gioco jack nella sua zona posteriore≈11,5 mm. Per ≥3 mm globali, inviluppo sottoscheda conservativo≤16,7 mm. |

Nella soluzione inclinata i piedini devono assorbire il dislivello della propria
faccia inferiore e aderire al tavolo senza imporre flessione al PCB. L'inclinazione
non si ottiene serrando la PCB contro quattro colonne verticali a facce rigide
non complanari. Verificare stabilità, grip, adesione dei piedini e forza sui
connettori; non sono state acquistate né dichiarate approvate viti o piedini.
Lunghezza viti da scegliere con spessore PCB/rondella e filetto utile reale,
senza andare a fondo foro. Le fonti Harwin specificano6 mm minimi filettati a
ciascuna estremità per i due distanziali nylon.

Un DAC con distanza carrier–faccia interna del modulo **g** occupa almeno lo
stack da verificare `g + 6,2 mm`, più le eventuali sporgenze non comprese nel
prodotto. La configurazione piana lascia un budget totale17 mm: non si può
scegliere un header alto e continuare ad attribuire il massimo al solo jack.
Lo stesso controllo vale per MASTER, pin passanti e cavo antenna.

Alternative sugli stessi fori: [Harwin R30-1002002](https://www.harwin.com/products/R30-1002002)
in ottone M3/L20/AF5 con isolamento e riserve rispettate; fissaggio a una base
generica tramite gli stessi76×116 mm; appoggi antiscivolo sotto le colonne.
Supporti a scatto **non qualificati**: occorre MPN con foro ammesso Ø3,2 e
spessore1,6, diametro testa dentro8 mm e altezza adeguata. Nessun foro aggiunto.

## Fissaggio display

I quattro fori esistenti Ø2,5 sono a X28,141/59,891 e Y33,8/59,2.
La distanza centro foro inferiore sinistro–bordo del pad VIN quadrato è
**2,2098 mm**: una rondella metallica Ø5 invade nominalmente il rame di0,2902 mm;
un ingombro Ø4 lascia solo0,2098 mm prima delle tolleranze.
Non è quindi approvato un generico kit M2 con rondelle.

Prima opzione da qualificare: viti/teste e colonne **isolanti**, diametro effettivo
non superiore a4 mm nelle zone vicine ai pad, con altezza determinata dal retro
reale del breakout e dai pin. Esistono [colonne commerciali M2 Ø4](https://www.fastenright.com/general-fixings/plastic-round-threaded-nylon-spacers),
ma materiale, altezza, tolleranze e testa della vite devono essere selezionati:
non sono componenti già approvati o acquistati. Nessun metallo deve coprire VIN.

Adesivo isolante sottile resta un'alternativa **non qualificata**: dalle sole
quote2D non è dimostrata una superficie piana libera sufficiente, per la presenza
di microSD, flex e componenti posteriori. Non applicare adesivo sui componenti
né premere vetro/flex. Per il TFT nudo, [Adafruit avverte](https://www.adafruit.com/product/5393)
che il backlight può separarsi dal TFT e suggerisce ritenzione leggera ai bordi
o copertura; questa proposta non aggiunge un enclosure obbligatorio.

## Verifiche e punto di revisione

| Controllo sul salvato | Esito |
|---|---|
| Schema/BOM/pinout | [Rapporto #34](issue-34-electrical-review.md); 237/237 pin invariati dopo i movimenti; sola nota J2 della BOM aggiornata per dado facoltativo |
| DRC geometrico finale | [115 warning](layout-35/drc-after.json): 64 differenze libreria, 27 silk sul rame, 13 silk sovrapposte, 11 silk/bordo; vedere conteggi effettivi nel JSON |
| Errori di clearance / bordo / PTH-NPTH nel courtyard / courtyard overlap | **0** nel DRC finale |
| Connessioni non instradate | **160**, attese: PCB non instradato |
| Parità schema/PCB | **68**: 64 campi DesignStatus e quattro fori solo PCB; non reti errate |
| Piste / via / zone | Assenti; verifica indipendente nell'[appendice #34](issue-34-electrical-review.md#verifica-indipendente-finale-del-placement-35) |
| Fissaggi M3 | Nessun pad invade le quattro riserve8×8; nessuna prova di carico fisica |

Le differenze di libreria non sono state eliminate con aggiornamenti indiscriminati:
si preservano i footprint specifici già verificati. La serigrafia curata appartiene
alla fase successiva; i warning sono esplicitati, non ignorati come un PASS globale.
Le scritture KiCad sono avvenute solo tramite Konnect MCP, con letture/export
successivi. I rendering/plot nativi KiCad sono esportazioni in sola lettura.

**Dimensione autorizzata:** 86×126 mm; spostamento applicato e verificato.
**Da validare:** accessibilità e comodità reali dei comandi assemblati.
**Da misurare prima di consolidarla:** stack display/DAC, keycap, manopole,
ganci/pin/saldature, presa dei cavi e stabilità degli appoggi. Le riserve dei
fissaggi vanno rese vincoli automatici prima del routing. Non sono stati prodotti
Gerber, dima Hammond o una scheda alternativa.
