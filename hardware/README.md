# ESP-DubSiren — hardware

**J2 sostituito con Neutrik NRJ4HF. Gate B NON VALIDATO: PR #17 in bozza per verifica dell'utente. Nessun PCB fabbricabile.**

- [Progetto KiCad](kicad/ESP-DubSiren.kicad_pro), [schema](kicad/ESP-DubSiren.kicad_sch), [PCB](kicad/ESP-DubSiren.kicad_pcb).
- [Assieme FreeCAD modificabile](cad/assembly.FCStd): Hammond originale, proposta pannello Gate A e studio posteriore NRJ4HF.
- [Posteriore e sezione con quote](cad/nrj4hf-rear-section.svg), [anteprima](cad/nrj4hf-rear-section.png), [vista interna](cad/nrj4hf-inside.png), [misure e limiti](cad/nrj4hf-validation.json).
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

Lo STEP Neutrik originale è allineato ai quattro pad e orientato verso +Y nel CAD.
Centri dei pad nel riferimento CAD: T=(24,23;41,05), TN=(12,80;41,05),
S=(24,23;53,75), SN=(12,80;53,75), tutti sul piano Z22,2.
L'esportazione STEP nativa di J2 coincide con la forma FreeCAD (differenza volumetrica zero),
normalizzando il piano di appoggio dell'esportatore da Z−0,085 a Z22,2.
Questo scarto di riferimento comprende l'offset del modello nell'esportatore e il rame;
non è stato aggiunto un offset al modello di J2.

| Verifica | Esito | Misura / evidenza |
|---|---|---|
| Collegamenti elettrici | PASS | T→OUTL/AUDIO_MONO; S→GND; TN/SN e OUTR NC; ERC 0 errori e 0 warning |
| Altezza del jack | PASS nominale; tolleranze NON VALIDATE | Corpo 12,7 mm; Z9,5…22,2; piedini fino a Z26,2; margine nominale 6,9 mm sul budget precedente 19,6; distanza reale dal fondo 3,7 mm |
| Allineamento posteriore | PASS nominale; foro NON VALIDATO | Asse X17 / Z15,7; parete 2,0003 mm lungo asse, inferiore a 3 mm; inclinazione 1 grado; D11,2 nominale senza gioco assegnato |
| Interferenza USB-C | PASS statico; cavi NON VALIDATI | Distanza fra solidi jack e USB 23,6 mm; volume comune zero; inviluppo dado–USB 21,608 mm |
| MASTER e vicini | NON VALIDATO | Gap proiettati: MASTER 44,07 mm lungo X; PCB DAC 10,65 mm lungo Y. Mancano verifica completa di albero/manopola, altezza DAC e correlazione del placement frontale |
| Inserimento PCB | NON VALIDATO | Sagoma, supporti e assieme popolato completo non definiti; nessun percorso continuo dimostrato |
| Fissaggio tramite dado | NON VALIDATO | Inviluppo D17,45 x 4,75; dislivello della parete sul dado 0,305 mm; filettatura, accesso utensile e serraggio senza sforzi da verificare |

La cavità nominale resta 29,7 mm. Il nuovo corpo risolve il deficit di altezza del precedente RCA
nella posa diagnostica, ma **6,9 mm è un margine nominale**, non un bilancio completo di tolleranze
PCB/pot/connettore. Il corpo è a soli **0,0785 mm dalla parete originale** nella posa scelta:
non è una riserva di produzione adeguatamente dimostrata. Il foro D11,2 nominale coincide con
il diametro nominale della boccola; il gioco di montaggio deve ancora essere assegnato.

La scocca e il fondo Hammond originali non sono stati tagliati o sostituiti. L'oggetto
`RearAperturesStudy`, nascosto nella vista 3D, è una sola sonda diagnostica con aperture ipotetiche,
usata dalla tavola posteriore. Con queste aperture il volume di collisione statico jack/scocca e
USB/scocca è zero; ciò non dimostra l'inserimento. Il dado è un inviluppo, non una filettatura verificata.
MASTER e DAC sono solo proiezioni dichiarate. Non sono stati inventati modelli completi mancanti.

Sequenza richiesta ancora da dimostrare: PCB popolata inserita inclinata dal fondo → ingresso
contemporaneo di jack/USB/MASTER → raddrizzamento → fissaggio → dado esterno. Mancano sagoma,
supporti e ingombri completi per provare un percorso continuo. Non sono definiti angoli o traslazioni
fittizi e non è stata generata una dima 3D/STL non validata.

## Controlli e limiti del progetto

Verifica del 18 ottobre 2026: ERC senza errori o warning. Il confronto DRC prima/dopo conserva
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

## Evidenze storiche

La [verifica Z del vecchio RCA](cad/gate-b-height-check.pdf), la [vista diagnostica](cad/gate-b-cad.png)
e le [misure precedenti](cad/gate-b-height-check.json) documentano il deficit di 0,9 mm
(30,6 necessari contro 29,7 disponibili) e la penetrazione di 0,4 mm nel fondo.
Sono evidenze storiche, non una variante attiva. La precedente richiesta di doppia uscita RCA L/R
è superata dalla scelta mono NRJ4HF; non resta alcuna correzione OUTL→L / OUTR→R da implementare.

La [PR #17](https://github.com/Capobuf/ESP-DubSiren/pull/17) resta in bozza e la
[issue #14](https://github.com/Capobuf/ESP-DubSiren/issues/14) aperta. Richiesto feedback sulle viste CAD
prima dell'approvazione del Gate B. Nessun routing, Gerber o merge eseguito.
