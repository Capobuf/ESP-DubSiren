# ESP-DubSiren — hardware

**Proposta standalone a PCB esposto, 86 × 126 mm, da revisionare prima del routing. Nessun PCB fabbricabile.**

La direzione approvata il 30/10/2026 supera il vincolo Hammond: una sola PCB,
colore finale nero, quattro fissaggi M3, nessuna scocca o stampa 3D obbligatoria.
Il solo progetto attivo resta quello nella cartella `kicad`.

- [Verifica elettrica #34](docs/issue-34-electrical-review.md): ERC 0 errori/0 warning,
  237 pin coerenti fra schema e PCB, distinta verificata.
- [Proposta e verifica ingombri #35](docs/issue-35-standalone-layout.md):
  coordinate, altezze documentate, fissaggi e prove fisiche pendenti.
- [Serigrafia e assemblaggio #36](docs/issue-36-silkscreen-assembly.md):
  [pulizia successiva](docs/silkscreen-cleanup/README.md) preservata,
  senza ripristinare le etichette funzionali; [guida DIY](docs/assembly.md).
- **[Studio di placement #38](docs/issue-38-rear-placement.md)**: stato attivo,
  ESP32 a sinistra e DAC sotto U5, [fronte prima/dopo](docs/placement-38/F-comparison.png)
  e [retro prima/dopo](docs/placement-38/B-comparison.png). Stack non qualificato:
  18 errori DRC U6 sotto U5; nessun routing. Attende revisione dell'utente.
- [Vista superiore](docs/layout-35/F-layout.png),
  [vista inferiore](docs/layout-35/B-layout.png),
  [render superiore](docs/layout-35/top-3d.png),
  [render inferiore](docs/layout-35/bottom-3d.png).
  I render sono diagnostici: modelli incompleti e colore predefinito verde;
  non rappresentano un assieme completo o la finitura finale.

Il PCB conserva 68 footprint, ora riposizionati; bordo 86 × 126 mm R3,
fori Ø3,2 in (5;−1), (81;−1), (5;115), (81;115) mm.
Bordo posteriore Y=−6 e anteriore Y=120; interassi dei fissaggi 76×116 mm. Nessuna pista, via o zona.
Le riserve 8 × 8 mm ai fissaggi sono annotate su `Dwgs.User` per entrambe le
facce; non sono ancora regole automatiche del router.
Il dado NRJ-NUT-B è facoltativo nell'uso senza pannello.

[Decisioni e vincoli standalone](docs/pcb-standalone.md). Le opzioni di appoggio
sono orizzontale o leggermente inclinata con distanziali commerciali.

Sono disponibili anche le [viste dei comandi montati](docs/layout-35/assembled-top.png)
e il [render 3D inclinato](docs/layout-35/assembled-oblique.png):
quote verticali, copritasti e minuteria sono illustrativi, non un assieme validato.

## Progetto attivo

- [KiCad — progetto](kicad/ESP-DubSiren.kicad_pro), [schema](kicad/ESP-DubSiren.kicad_sch), [PCB](kicad/ESP-DubSiren.kicad_pcb).
- [BOM](docs/BOM.csv), [fonti/datasheet](docs/sources.md), [acquisti precedenti](docs/acquisti.csv).
- Issue [#34: validazione elettrica](https://github.com/Capobuf/ESP-DubSiren/issues/34), [#35: layout, altezza componenti e fissaggi](https://github.com/Capobuf/ESP-DubSiren/issues/35), [#36: serigrafia e guida assemblaggio](https://github.com/Capobuf/ESP-DubSiren/issues/36).

C'è **un solo progetto KiCad attivo**, con 68 footprint e dimensione 86×126 mm autorizzata dall'utente. Non ha routing, via o zone. La base standalone è documentata nella #35; la proposta corrente è nella [#38](docs/issue-38-rear-placement.md) e non deriva dalla copia del pannello FreeCAD storico. **Il PCB non è fabbricabile**: le verifiche geometriche non sostituiscono la qualifica dell'assieme reale.

## Componenti e interfacce

Il lato utente deve mantenere tre potenziometri in alto, display Adafruit 5393 al centro con encoder alla destra, tre potenziometri più sotto e quattro CHERRY in basso. USB-C, jack mono e MASTER sono sul retro/bordo posteriore, con libertà di riposizionamento per accessibilità e ingombri. Non sostituire componenti senza una decisione motivata.

**J2: Neutrik NRJ4HF** (non NRJ4HF-1), jack mono TS 6,35 mm, inserito nello schema con simbolo `Connector_Audio:AudioJack2_Switch` e footprint `Connector_Audio:Jack_6.35mm_Neutrik_NRJ4HF_Horizontal`. T → AUDIO_MONO/OUTL del DAC U6, S → GND, TN e SN NC, OUTR del DAC inutilizzato e non collegato a OUTL. L'uscita doppia RCA è superata. La verifica documentale elettrica è riportata nella [#34](docs/issue-34-electrical-review.md); la validazione fisica resta pendente.

Il dado Neutrik **NRJ-NUT-B** è documentato come accessorio storico per un montaggio a pannello, **non è necessario per il PCB a vista**. Non aggiungere una staffa USB-C: verificare i normali fissaggi del componente e prevedere accesso manuale quando si inserisce il cavo.

Display 5393 e DAC 6250 sono breakout che richiedono la verifica degli ingombri fisici sulle due facce; il fissaggio del display può usare minuteria semplice o adesivo isolante solo se compatibile con le superfici libere. Il disegno del display non è una prova della distanza fra vetro e PCB.

## Fissaggio sul tavolo

Quattro fori predisposti per M3, opportunamente isolati dal rame, devono consentire distanziali commerciali in nylon o metallo e piedini antiscivolo. Le posizioni e i diametri sono riportati sopra; restano da qualificare le altezze assemblate su B.Cu, le saldature e la stabilità durante la pressione dei pulsanti. Un'eventuale base aggiuntiva o un'eventuale custodia stampata in futuro sono opzioni, **non requisiti**.

## Studi superati

L'[assiemaggio FreeCAD Hammond](cad/assembly.FCStd), il [pannello Gate A](cad/gate-a-panel.pdf) e le [verifiche NRJ4HF/Hammond](cad/nrj4hf-validation.json) appartengono al percorso di studio precedente, non definiscono il nuovo layout né autorizzano forature. Il contenitore Hammond 29830PSLA non è più un vincolo del PCB. La issue [#14](https://github.com/Capobuf/ESP-DubSiren/issues/14) e la PR [#18](https://github.com/Capobuf/ESP-DubSiren/pull/18) documentano la soluzione scartata, conservata anche nello storico Git.

Nessuna modifica a firmware, routing o Gerber è implicata dal cambio di impostazione.
