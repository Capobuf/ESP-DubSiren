# ESP-DubSiren — hardware

**KiCad provvisorio. CAD e dima da #14. Nessun PCB fabbricabile.**

- [Progetto KiCad](kicad/ESP-DubSiren.kicad_pro)
- [Schema elettrico](kicad/ESP-DubSiren.kicad_sch)
- [PCB](kicad/ESP-DubSiren.kicad_pcb)
- [Sorgenti CAD e disegni originali](cad/sources/), incluso lo [STEP Hammond 29830PSLA](cad/sources/Hammond_29830PSLA.stp)
- [Distinta elettronica](docs/BOM.csv)
- [Fonti e datasheet](docs/sources.md)

La #15 conserva lo schema della precedente copia con display, con 64 riferimenti
e 79 net. La BOM contiene 57 componenti acquistabili e 7 test pad PCB, distinti
nella colonna `Purchasable`; non contiene il vecchio contenitore custom.
L'[elenco acquisti originale](docs/acquisti.csv) documenta quantità acquistate e
manopole da verificare nella #14; non sostituisce le quantità di montaggio della BOM.

Il PCB conserva i 68 footprint, incluse le quattro impronte dei fori esistenti,
con posizione, orientamento e assegnazione delle net invariati. Piste, via e zone
sono azzerati; il vecchio bordo 113 × 130,5 mm è rimosso. Restano le geometrie
interne ai footprint, comprese le asole: non definiscono una sagoma approvata.
I nomi delle librerie locali mantengono il suffisso `V2` per preservarne gli
identificatori; non esiste un secondo progetto hardware.

Verifica del 18 ottobre 2026: ERC senza errori o warning, netlist invariata dopo
la riorganizzazione. Il DRC resta non nullo: segnala 161 collegamenti non
instradati, differenze preesistenti tra footprint e librerie, campi `DesignStatus`
mancanti nel PCB e problemi preesistenti di serigrafia e courtyard. La rimozione
del bordo cambia inoltre alcune segnalazioni di distanza dalla serigrafia.
L'assenza del bordo e dei collegamenti instradati è intenzionale; il progetto
non è pronto per la produzione.

La [#14](https://github.com/Capobuf/ESP-DubSiren/issues/14) dovrà validare il fit
nell'Hammond originale, definire sagoma e placement, correggere il dual RCA L/R
e produrre assieme e dima. Non sono presenti assiemi, STL o dime fittizi.
Il PCB di partenza non incorporava modelli 3D: la libreria RCA ora riferisce
direttamente lo STEP originale con la trasformazione precedente, senza una copia
STEP derivata. La verifica fisica dei componenti e del montaggio resta aperta.
