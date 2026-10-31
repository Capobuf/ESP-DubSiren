# Assemblaggio DIY — ESP-DubSiren

**Studio #38, 31 ottobre 2026:** il PCB attivo propone U1 a sinistra e U6
sotto il display. Le immagini e le coordinate #35/#36 qui richiamate sono
la base precedente. Per lo stack e la sequenza **DAC prima, display dopo**
usare la [revisione #38](issue-38-rear-placement.md#montaggio-e-manutenzione-alternativa-minima-di-fissaggio),
ancora da qualificare su pezzi fisici; non applicare un montaggio ravvicinato
DAC/display sulla sola base di questa guida.

Guida per il prototipo standalone **86 × 126 mm**, angoli R3, unico progetto
[KiCad](../kicad/ESP-DubSiren.kicad_pro). **Non fabbricare né alimentare il PCB
attuale: manca il routing.** Questa procedura si applicherà dopo completamento
dei collegamenti, DRC e revisione dell'utente. Quote di montaggio non misurate
e accessori non identificati restano da qualificare, come nella [#35](issue-35-standalone-layout.md).

## Riconoscere le due facce

Il bordo posteriore ospita USB-C, MASTER e jack mono; quello anteriore i quattro
CHERRY. Tutte le coordinate qui sono **viste dal lato utente**, X verso destra,
Y verso i pulsanti; una vista inferiore si legge specchiata.

Per il riconoscimento usare le [preview top/bottom](assembly-36/review.png)
e i disegni di montaggio [top](assembly-36/F-assembly.png) /
[bottom](assembly-36/B-assembly.png). Le nuove scritte posteriori e la pulizia
dei riferimenti esistenti non sono ancora applicate: non affidarsi soltanto
alla serigrafia per orientare i componenti.

| Faccia | Parti montate |
|---|---|
| Superiore, lato utente, F.Cu | RV1–3 sopra il display, U5/display e SW7/encoder, RV4–6 sotto, SW1–4 in basso |
| Inferiore, lato montaggio, B.Cu | U1 ESP32, U2 convertitore, U3 mux, U4 ESD, U6 DAC, U7 load switch; passivi, F1/D1; J1 USB-C, J2 mono e RV7 MASTER; SW5 RESET, SW6 BOOT e TP1–7 |

Le funzioni predefinite dei pot RV1–6 sono PITCH, RATE, ECHO LEVEL, ECHO TIME,
ECHO FEEDBACK, ECHO FILTER. In Classic SW1–4 sono TRIGGER, HOLD, MANUAL MOD,
ECHO CUT; in Extended cambiano funzione. Pot e tasti sono riassegnabili:
leggere il display per la funzione attiva. SW7 ruota e si preme per il menu;
RV7 è sempre MASTER. Vedi [pannello e menu](../../docs/panel-ui.md).

## Componenti elettronici

Usare la [BOM completa](BOM.csv), con MPN, produttore e footprint, confrontata
con lo schema nella [#34](issue-34-electrical-review.md). Le quantità sotto
sono **da montare**, non quelle dei lotti acquistati: 57 componenti e 7 test pad
realizzati nel rame. I quattro fori H1–H4 non sono componenti elettronici.

| Riferimenti | Qtà | Valore / MPN |
|---|---:|---|
| C1, C3, C5 | 3 | 22 µF 0805 — Samsung CL21A226MPCLRNC |
| C2, C4, C6, C8, C9 | 5 | 100 nF 0603 — Samsung CL10B104KO8NNNC |
| C7, C10 | 2 | 1 µF 0603 — Samsung CL10B105MO8NNWC |
| C11 | 1 | 10 nF C0G 50 V 0603 — Murata GRM1885C1H103JA01D |
| R1 | 1 | 10 Ω — NIC NRC06J100TRF |
| R2, R3 | 2 | 22 Ω — Yageo RC0603JR-0722RL |
| R4, R5 | 2 | 5,1 kΩ — Yageo AC0603FR-075K1L |
| R6, R7, R15–21 | 9 | 10 kΩ — Yageo RC0603JR-0710KL |
| R8–14 | 7 | 4,7 kΩ — Yageo RC0603FR-074K7L |
| D1 | 1 | TVS PANJIT SMF5.0A_R1_00001, catodo pad 1 |
| F1 | 1 | PTC Bel 0ZCG0110CF2B, 1,1 A, 1812 |
| U1 | 1 | Espressif ESP32-S3-WROOM-1U-N16R8 |
| U2 | 1 | MPS MPM3612GLQ-33-Z, con pad termico nascosto |
| U3 | 1 | Toshiba 74HC4051D, SOIC-16 |
| U4 | 1 | SLKOR USBLC6-2SC6, SOT-23-6 |
| U7 | 1 | Texas Instruments TPS22918DBVR, SOT-23-6 |
| U5 | 1 | Adafruit 5393, display ST7789 |
| U6 | 1 | Adafruit 6250, DAC PCM5102 |
| J1 | 1 | GCT USB4085-GF-A, USB-C |
| J2 | 1 | Neutrik NRJ4HF, mono TS 6,35 mm |
| RV1–6 | 6 | Bourns PTV111-3420A-B203, 20 kΩ, verticale |
| RV7 | 1 | Bourns PTV111-1420A-B103, 10 kΩ, orizzontale |
| SW1–4 | 4 | CHERRY MX2A-11NW |
| SW5, SW6 | 2 | C&K PTS636SM43SMTR LFS, RESET / BOOT |
| SW7 | 1 | Bourns PEC11R-4120F-S0018 |
| TP1–7 | 7 | Pad di prova: nessun pezzo da acquistare |

C11, U7 e J2 non compaiono nel registro acquisti conservato: controllare le
scorte, senza dedurne che non siano stati acquistati altrove. Le resistenze
sono 0603. I condensatori della BOM sono ceramici non polarizzati; F1, resistenze
e interruttori non hanno polarità elettrica, ma devono rispettare il footprint.

## Ferramenta e accessori facoltativi

Nessuna custodia, staffa USB o stampa 3D necessaria. Per usare la scheda sul
tavolo occorrono comunque appoggi che impediscano a componenti e saldature di
toccarlo; le due configurazioni seguenti sono alternative, non parti elettriche.

| Uso | Quantità e selezione | Stato |
|---|---|---|
| Appoggio piano | 4× [Harwin R30-1612000](https://www.harwin.com/products/R30-1612000), nylon, M3 femmina/femmina, L20 mm, esagono AF5,5 | Compatibile nominalmente con fori e riserve; altezza assieme da misurare |
| Appoggio inclinato | Dietro H1/H2: 2× [Harwin R30-1612500](https://www.harwin.com/products/R30-1612500), L25; davanti H3/H4: 2× R30-1612000 L20 | Stessa sezione M3/AF5,5; inclinazione nominale 2,47°, stabilità da provare |
| Fissaggio appoggi | 4 viti M3 superiori; 4 piedini antiscivolo compatibili con le colonne; eventuali viti inferiori se richieste dai piedini | Lunghezze, piedini e MPN da scegliere dopo misura; testa/rondella Ø≤7 mm |
| Display U5 | Fissaggi isolanti compatibili con fori Ø2,5, oppure adesivo idoneo su superfici libere | Nessun kit M2 generico approvato: vedi sotto |
| Collegamento breakout | U5: fila 11 pin; U6: file 8 e 6 pin, passo 2,54 mm | Header/socket, lunghezza pin e altezza non ancora selezionati |
| Comandi | 6 Davies 1100; 1 Same Sky SAK-023 per encoder; 1 Cliff CL170805B per MASTER; 4 keycap CHERRY compatibili | Manopole presenti nel registro; accoppiamenti da provare. Keycap senza MPN, riserva 20×20 mm |
| Jack J2 | Neutrik NRJ-NUT-B | Facoltativo solo per un eventuale pannello futuro |

**Geometrie di fissaggio:** bordo X=0…86, Y=−6…120 mm; H1/H2 a (5;−1),
(81;−1), H3/H4 a (5;115), (81;115), Ø3,2 NPTH, interassi **76 × 116 mm**.
Spessore di riferimento 1,6 mm, da confermare nell'ordine futuro. Rispettare
le riserve 8×8 mm su entrambe le facce: non aggiungere rame, fili o saldature.
AF5,5 ha diagonale 6,35 mm, interna alla riserva; verificare anche le teste.

La configurazione piana L20 richiede un inviluppo reale sottoscheda ≤17 mm
per lasciare almeno 3 mm dal tavolo, senza contare il contributo dei piedini.
Per quella inclinata usare il limite conservativo ≤16,7 mm. Il jack scende
nominalmente 12,7 mm sotto B.Cu, ma **non è un Hmax misurato dell'assieme**.
Per il DAC sommare distanza carrier–modulo, componenti, pin e saldature:
non basta la sola altezza prodotto di 6,2 mm.

Serrare senza flettere il PCB e senza andare a fondo nei fori ciechi; Harwin
specifica almeno 6 mm di tratto filettato per estremità, non una lunghezza
universale della vite. Nell'opzione inclinata usare piedini cedevoli che
aderiscano al tavolo: non forzare la PCB contro facce rigide non complanari.
Controllare che non scivoli né si ribalti premendo i tasti o inserendo i cavi.

## Sequenza di montaggio

1. **Prima del saldatore:** confrontare PCB e distinta della revisione instradata
   approvata; fare un montaggio a secco di pot, encoder, CHERRY e breakout.
   Verificare ganci/asole, keycap, manopole e accesso ai giunti su entrambe le
   facce. Servono lente/microscopio, flussante, multimetro e attrezzatura adatta
   ai package: U2 LGA e il pad centrale di U1 non sono saldature soltanto a punta.
   Per quei pad usare un processo di reflow controllato; chi non dispone
   dell'attrezzatura può far assemblare gli SMD da un servizio di montaggio.
2. **Lato B.Cu, SMD bassi:** saldare passivi, U2/U3/U4/U7, D1/F1 e U1 con
   processo conforme ai componenti. Controllare subito pin 1, ponti e giunti,
   prima che i comandi o i moduli limitino l'accesso. Non correggere la
   numerazione di un footprint in base alla sola vista specchiata.
3. **Connettori e servizio:** montare J1, J2, RV7, SW5/6 rispettando i corpi
   verso il bordo posteriore e RESET/BOOT verso il bordo destro. USB-C usa i
   propri ancoraggi; sostenere la scheda vicino al connettore durante l'uso.
   Controllare i terminali di J2 che attraversano il lato utente.
4. **Lato utente:** montare RV1–6, SW7 e SW1–4, in piano e senza piegare i pin
   per compensare fori non coincidenti. RV7 orizzontale non sostituisce un pot
   verticale. Provare la corsa dei quattro tasti prima dei keycap.
5. **Breakout, per ultimi:** scegliere lo stack dopo la prova a secco descritta
   sotto. Puntare prima pochi pin, controllare faccia, pin 1 e parallelismo;
   completare solo se il modulo è sostenuto e non forza le saldature. Lasciare
   accesso a ispezione e manutenzione. Non usare i soli pin come sostegno
   meccanico contro le pressioni sul display o l'inserimento di spine nel DAC.
6. **Appoggi e comandi:** fissare i quattro supporti, montare manopole/keycap,
   senza spingere gli alberi verso la PCB. Misurare Hmax reale, margini sotto
   la scheda, corsa encoder e tasti. Nessuna parte elettronica sul banco,
   anche durante la saldatura: usare un supporto per PCB.

### Verso e orientamento

| Parte | Controllo prima di saldare |
|---|---|
| U1–4, U7 | Tacca/punto pin 1 del pezzo contro pad 1 del footprint e disegno del datasheet in [fonti](sources.md). Non scambiare U4 e U7 perché entrambi SOT-23-6 |
| D1 | Banda catodo verso pad 1 = VBUS_PROTECTED; pad 2 = GND |
| U5/5393 | Vetro verso l'utente; pad 1 VIN = +3V3, pad 3 GND, pad 11 LITE. L'ordine va verificato sulle scritte del modulo: il footprint è visto dalla faccia display |
| U6/6250 | Sul retro, ruotato 90° nel PCB; pad 1 VIN = +5V, 2/7 GND, 6 OUTL = AUDIO_MONO. Pad 8 OUTR non collegato; non unirlo a OUTL |
| J2 | T = AUDIO_MONO, S = GND; TN/SN non collegati. La presa 3,5 mm sul DAC non è una seconda uscita di pannello prevista |
| RV / SW | Far coincidere anche ganci, guide e fori non elettrici. Non dedurre il centro CHERRY dall'anchor del pad 1 |

### Display e DAC: due facce da controllare

Ispezionare **entrambi i lati** dei moduli 5393 e 6250: possono avere componenti,
connettori o giunti sulla faccia rivolta al carrier. Controllare spazio per
microSD/flex del display, pin passanti, saldature e corpi sottostanti. Eventuale
isolante non sostituisce una distanza sufficiente: nessun componente deve
ricevere pressione o poggiare sul carrier.

I quattro fori del display sono a X=28,141/59,891 e Y=33,8/59,2 mm,
passi 31,75×25,4 mm. Vicino al foro inferiore sinistro, il rame del pad VIN
dista dal centro **2,2098 mm**: una rondella metallica Ø5 invade il rame di
0,2902 mm. Anche Ø4 lascia solo 0,2098 mm nominali. Selezionare fissaggi
**isolanti**, diametro effettivo e tolleranze verificati, senza coprire VIN;
altezza determinata dal retro reale del modulo. Fori e fissaggi del DAC vanno
analogamente confrontati con il pezzo prima di scegliere la minuteria.

Se si preferisce adesivo per U5, prima identificare superfici piane libere e
verificare compatibilità del materiale, isolamento e tenuta. Non applicarlo
su piazzole, componenti, flex o connettori, né premere il vetro. Per il TFT
senza cover, [Adafruit](https://www.adafruit.com/product/5393) segnala il rischio
di separazione dalla retroilluminazione: può servire una leggera ritenzione
laterale con materiale idoneo, verificata sul pezzo. Nessuna cornice stampata
è richiesta da questa guida.

## Controlli prima dell'alimentazione

- Esaminare entrambe le facce con lente: niente ponti, residui conduttivi,
  terminali liberi, giunti freddi o corpi appoggiati ad altre parti; verificare
  D1 e tutti i pin 1. Confrontare i giunti nascosti con il processo usato:
  una foto dall'alto non dimostra la saldatura dei pad centrali.
- A scheda scollegata controllare assenza di corti persistenti fra GND e
  VBUS_RAW, VBUS_PROTECTED, +5V e +3V3; i condensatori possono dare un transitorio
  allo strumento. Non esiste qui una soglia ohmica misurata da dichiarare PASS.
  Controllare continuità T→U6 OUTL e S→GND, e indipendenza di OUTR.
- Verificare supporti, isolamento e assenza di contatto con il banco; accesso
  a BOOT/RESET e TP; connettori liberi e pressione tasti senza flessione.
  MASTER al minimo, ingresso audio esterno scollegato per il primo avvio.

## Primo avvio e smoke test (su futura scheda instradata)

1. Alimentare da USB con corrente monitorata e limitazione adatta al prototipo;
   verificare la sorgente USB e l'assorbimento effettivo. PTC da 1,1 A e
   resistori CC non provano la disponibilità di 1,1 A. Togliere alimentazione
   in caso di corto, riscaldamento anomalo o tensioni errate. Misurare TP1 +5V,
   TP2 +3V3 rispetto a TP3 GND; ripple e tolleranze restano da qualificare.
2. Caricare il firmware `panel-prototype` seguendo [uso autonomo](../../docs/standalone.md).
   SW5 RESET è sul retro a (81;64), SW6 BOOT a (81;80) mm, accessibili da
   sotto/bordo destro: tenere BOOT, premere/rilasciare RESET, rilasciare BOOT.
   Usare un utensile isolante se il dito non passa; non fare leva sui componenti.
3. Con cavo dati e porta Espressif effettiva, dalla radice del repository:

   ```powershell
   pc\.venv\Scripts\python.exe pc\smoke_test.py COM6
   ```

   Sostituire COM6. Lo script si arresta al primo errore: verifica protocollo,
   PCM USB e stato/routing I2S, **non misura l'uscita analogica né i controlli
   fisici**. Non è stato eseguito su questo PCB.
4. Per la prova fisica, collegare J2 a un ingresso **di linea** con volume
   inizialmente basso; il [6250](https://learn.adafruit.com/adafruit-pcm510x-i2s-dac/pinouts)
   richiede un carico ≥1 kΩ. Abilitare l'audio locale dalla GUI oppure riavviare
   dopo Disconnect; aumentare MASTER gradualmente. Provare Trigger, Hold,
   Manual Mod, Echo Cut, sei pot con Pickup, rotazione/click encoder, display
   e RESET. Verificare le assegnazioni correnti, senza cancellare impostazioni
   salvate. Controllare il silenzio con MASTER minimo e Stop Audio.
5. Verificare USB/mono/MASTER con i cavi inseriti, stabilità degli appoggi e
   spazio sotto la scheda. Annotare tensioni, corrente, rumore, transitori e
   problemi: display, DAC e ADC del prototipo restano da qualificare sul reale.

## Punti ancora aperti

Routing e DRC completo; revisione dell'utente; serigrafie residue dei footprint;
impostazione colore nero/bianco nel progetto e nell'ordine futuro; header e
stack dei due breakout; fissaggi display; Hmax con giunti e cavi; keycap e fit
delle tre famiglie di manopole; stabilità degli appoggi e prove elettriche.
La [consegna #36](issue-36-silkscreen-assembly.md) riporta serigrafie effettivamente
applicate, anteprime e limiti dei tool. Questa guida non autorizza la produzione.
