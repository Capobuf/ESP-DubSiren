# ESP-DubSiren — hardware

**KiCad provvisorio. #14: fase B autorizzata con layout provvisorio, arresto sullo stack Z. Nessun PCB fabbricabile.**

- [Progetto KiCad](kicad/ESP-DubSiren.kicad_pro)
- [Schema elettrico](kicad/ESP-DubSiren.kicad_sch)
- [PCB](kicad/ESP-DubSiren.kicad_pcb)
- [FreeCAD attivo](cad/assembly.FCStd): Hammond originale, schizzi modificabili del pannello e audit Z del dual RCA; assieme completo non validato
- [Verifica Z del Gate B e alternative](cad/gate-b-height-check.pdf), [vista CAD diagnostica](cad/gate-b-cad.png) e [misure](cad/gate-b-height-check.json)
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
Il 18 ottobre l'utente ha autorizzato la fase B mantenendo il layout provvisorio.
La verifica conferma il deficit di 0,9 mm della configurazione con RCA e pot
su facce opposte: 20,5 + 1,6 + 7,5 + due margini da 0,5 = 30,6 mm.
La faccia superiore della PCB dovrebbe stare contemporaneamente a Z≥24,7
e Z≤23,8 mm, con origine sul fondo esterno. A Z=23,8 lo STEP RCA interseca
il fondo originale di 0,4 mm (volume 6,314 mm³ nella posa diagnostica).
Senza margini rimarrebbero solo 0,1 mm nominali, insufficienti già per la
tolleranza positiva di 0,3 mm del RCA. Non è una prova che ogni disposizione
possibile fallisca: la soluzione con componenti sulla stessa faccia resta da studiare.

Il gruppo `GateBHeight` contiene il dual RCA originale e gli elementi diagnostici:
il piano PCB non è una sagoma e il blocco pot è solo una rappresentazione della
quota del corpo. La posizione XY del RCA non è un placement approvato.
In base al punto 9 della #14, prima di cambiare componenti o disposizione
serve scegliere una delle alternative riportate nel PDF: RCA più basso,
potenziometri meno profondi oppure studio della configurazione sulla stessa faccia.
Il montaggio completo, il display a filo, il percorso d'inserimento e la dima 3D
non sono validati. La correzione OUTL→L / OUTR→R del dual RCA è rinviata
alla fase C, dopo il Gate B. Nessuno STL o assieme completo è dichiarato valido.
Il PCB di partenza non incorporava modelli 3D: la libreria RCA ora riferisce
direttamente lo STEP originale con la trasformazione precedente, senza una copia
STEP derivata. La verifica fisica dei componenti e del montaggio resta aperta.
