# ESP-DubSiren — hardware

**PR #17 integrata: J2 è Neutrik NRJ4HF. Nuova proposta di posizione nel solo CAD; Gate B NON VALIDATO. Nessun PCB fabbricabile.**

- [Progetto KiCad](kicad/ESP-DubSiren.kicad_pro), [schema](kicad/ESP-DubSiren.kicad_sch), [PCB](kicad/ESP-DubSiren.kicad_pcb).
- [Assieme FreeCAD modificabile](cad/assembly.FCStd): Hammond originale, proposta pannello Gate A e studio posteriore NRJ4HF.
- [Posteriore e sezione con quote](cad/nrj4hf-rear-section.svg), [anteprima](cad/nrj4hf-rear-section.png), [vista interna](cad/nrj4hf-inside.png), [misure e limiti](cad/nrj4hf-validation.json).
- [Vista dall'alto con gli inviluppi dei corpi pot](cad/nrj4hf-top-screening.png): controllo parziale di interferenza, non assieme completo.
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
| Allineamento posteriore | PASS nominale; foro NON VALIDATO | Asse proposto X12,5 / Z15,7; parete 2,0003 mm lungo asse, inferiore a 3 mm; inclinazione 1 grado; D11,2 da disegno Neutrik |
| Interferenza USB-C | PASS statico; cavi NON VALIDATI | Distanza fra solidi jack e USB 19,1 mm; volume comune zero; inviluppo dado–USB 17,108 mm |
| Piedini jack e corpi pot frontali | PASS del solo screening nominale | Nessuna intersezione con i sei inviluppi; distanza minima 1,52 mm. Piedini/clip dei pot e tolleranze non inclusi |
| MASTER e vicini | NON VALIDATO | Gap proiettati: MASTER 39,57 mm lungo X; PCB DAC 10,65 mm lungo Y. Mancano verifica completa di albero/manopola, altezza DAC e componenti frontali completi |
| Inserimento PCB | NON VALIDATO | Sagoma, supporti e assieme popolato completo non definiti; nessun percorso continuo dimostrato |
| Fissaggio tramite dado | NON VALIDATO | Inviluppo D17,45 x 4,75; dislivello della parete sul dado 0,305 mm; filettatura, accesso utensile e serraggio senza sforzi da verificare |

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
Il gioco nullo fra queste geometrie non dimostra il gioco reale del filetto nel foro:
non giustifica da solo un allargamento arbitrario. Accoppiamento e serraggio restano da verificare.
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
supporti e ingombri completi per provare un percorso continuo. Non sono definiti angoli o traslazioni
fittizi e non è stata generata una dima 3D/STL non validata.

È stata aggiunta una **verifica parziale indipendente dal display**: sei cilindri
M9 × 5 mm rappresentano le sole boccole dei potenziometri, con base Z31,3;
le aperture frontali D9,2 sono quelle proposte al Gate A. La sonda
`MountingAperturesProbe` è una copia diagnostica della scocca, non modifica l'originale.
Nella posa finale jack e boccole non intersecano questa sonda. Traslando il gruppo
di 0,2 mm verso il fondo, il jack interseca la scocca per 4,480 mm³; arretrandolo
di 0,2 mm lungo −Y, le boccole la intersecano per 7,347 mm³.
Queste due prove escludono soltanto i movimenti rettilinei testati nel modello
nominale ridotto: **non dimostrano che un percorso inclinato sia impossibile**.
Il filetto semplificato del jack e i giochi ancora da verificare limitano ulteriormente
la prova. Nessuna sequenza completa è stata approvata.

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
