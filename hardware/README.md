# ESP-DubSiren — hardware

**KiCad provvisorio. #14 al Gate A, in attesa di riscontro. Nessun PCB fabbricabile.**

- [Progetto KiCad](kicad/ESP-DubSiren.kicad_pro)
- [Schema elettrico](kicad/ESP-DubSiren.kicad_sch)
- [PCB](kicad/ESP-DubSiren.kicad_pcb)
- [FreeCAD attivo](cad/assembly.FCStd): Hammond originale orientato con fondo in basso e schizzi modificabili del pannello; solo studio XY, assieme completo non validato
- [Pannello quotato e dima cartacea 1:1](cad/gate-a-panel.pdf), [SVG 1:1](cad/gate-a-panel.svg) e [anteprima](cad/gate-a-preview.png): stampare al 100%, verificare la barra da 50 mm; **non forare**
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

La [#14](https://github.com/Capobuf/ESP-DubSiren/issues/14) ha preparato una sola
proposta di pannello al Gate A: sei Davies 1100 a passo 28 mm, display 5393,
encoder con Same Sky SAK-023 e quattro pulsanti a passo 22 mm. La Cliff K85M
resta destinata al MASTER posteriore. I keycap generici hanno una riserva di
progetto di 20 × 20 mm, **non una misura verificata**. Quote, fonti e limiti
sono nella seconda pagina del PDF; le [misure OCC](cad/gate-a-geometry.json)
documentano le verifiche. I corridoi delle aperture dei pulsanti non intersecano
le colonne del box; ergonomia, tolleranze e corsa restano da provare.

Il modello conferma 29,7 mm di profondità nominale utile. Il rettangolo
85,62 × 111,12 mm collide con le colonne se portato vicino al frontale;
le sonde rettangolari più piccole documentate nel PDF riguardano il solo
contenitore vuoto, non approvano una sagoma PCB o l'inserimento dell'assieme.
Il deficit nominale di 0,9 mm dello stack precedente resta aperto.
Prima di proseguire con stack Z, assieme completo e dima 3D serve il riscontro
dell'utente al Gate A. La correzione OUTL→L / OUTR→R del dual RCA è rinviata
alla fase C, dopo il Gate B. Nessuno STL o assieme completo è dichiarato valido.
Il PCB di partenza non incorporava modelli 3D: la libreria RCA ora riferisce
direttamente lo STEP originale con la trasformazione precedente, senza una copia
STEP derivata. La verifica fisica dei componenti e del montaggio resta aperta.
