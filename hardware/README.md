# ESP-DubSiren — hardware

**Direzione attiva (30/10/2026): PCB desktop a vista, senza enclosure obbligatoria.** Solder mask nera, serigrafia frontale, quattro fori per supporti e due configurazioni di appoggio possibili (orizzontale o leggermente inclinata). Tutte le decisioni e i limiti sono in [PCB standalone](docs/pcb-standalone.md).

## Progetto attivo

- [KiCad — progetto](kicad/ESP-DubSiren.kicad_pro), [schema](kicad/ESP-DubSiren.kicad_sch), [PCB](kicad/ESP-DubSiren.kicad_pcb).
- [BOM](docs/BOM.csv), [fonti/datasheet](docs/sources.md), [acquisti precedenti](docs/acquisti.csv).
- Issue [#34: validazione elettrica](https://github.com/Capobuf/ESP-DubSiren/issues/34), [#35: layout, altezza componenti e fissaggi](https://github.com/Capobuf/ESP-DubSiren/issues/35), [#36: serigrafia e guida assemblaggio](https://github.com/Capobuf/ESP-DubSiren/issues/36).

C'è **un solo progetto KiCad attivo**. Attualmente conserva 68 footprint, non ha routing, via, zone né una sagoma finale approvata. Il placement KiCad è provvisorio e non coincide integralmente con l'ultimo layout frontale studiato in FreeCAD. **Il PCB non è fabbricabile.** Le decisioni di questo documento non equivalgono a verifica elettrica o meccanica.

## Componenti e interfacce

Il lato utente deve mantenere tre potenziometri in alto, display Adafruit 5393 al centro con encoder alla destra, tre potenziometri più sotto e quattro CHERRY in basso. USB-C, jack mono e MASTER sono sul retro/bordo posteriore, con libertà di riposizionamento per accessibilità e ingombri. Non sostituire componenti senza una decisione motivata.

**J2: Neutrik NRJ4HF** (non NRJ4HF-1), jack mono TS 6,35 mm, inserito nello schema con simbolo `Connector_Audio:AudioJack2_Switch` e footprint `Connector_Audio:Jack_6.35mm_Neutrik_NRJ4HF_Horizontal`. T → AUDIO_MONO/OUTL del DAC U6, S → GND, TN e SN NC, OUTR del DAC inutilizzato e non collegato a OUTL. L'uscita doppia RCA è superata. La validazione elettrica completa è prevista nella #34.

Il dado Neutrik **NRJ-NUT-B** è documentato come accessorio storico per un montaggio a pannello, **non è necessario per il PCB a vista**. Non aggiungere una staffa USB-C: verificare i normali fissaggi del componente e prevedere accesso manuale quando si inserisce il cavo.

Display 5393 e DAC 6250 sono breakout che richiedono la verifica degli ingombri fisici sulle due facce; il fissaggio del display può usare minuteria semplice o adesivo isolante solo se compatibile con le superfici libere. Il disegno del display non è una prova della distanza fra vetro e PCB.

## Fissaggio sul tavolo

Quattro fori predisposti per M3, opportunamente isolati dal rame, devono consentire distanziali commerciali in nylon o metallo e piedini antiscivolo. Posizioni e diametri finali da determinare insieme a ingombri dei componenti su B.Cu, saldature e stabilità durante la pressione dei pulsanti. Un'eventuale base aggiuntiva o un'eventuale custodia stampata in futuro sono opzioni, **non requisiti**.

## Studi superati

L'[assiemaggio FreeCAD Hammond](cad/assembly.FCStd), il [pannello Gate A](cad/gate-a-panel.pdf) e le [verifiche NRJ4HF/Hammond](cad/nrj4hf-validation.json) appartengono al percorso di studio precedente, non definiscono il nuovo layout né autorizzano forature. Il contenitore Hammond 29830PSLA non è più un vincolo del PCB. La issue [#14](https://github.com/Capobuf/ESP-DubSiren/issues/14) e la PR [#18](https://github.com/Capobuf/ESP-DubSiren/pull/18) documentano la soluzione scartata, conservata anche nello storico Git.

Nessuna modifica a firmware, routing o Gerber è implicata dal cambio di impostazione.
