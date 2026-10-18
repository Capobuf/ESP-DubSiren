# Fonti hardware

Fonti conservate per lo schema attivo e la futura verifica meccanica #14.
I file originali spostati mantengono il contenuto; studi, rendering e report delle
revisioni eliminate sono recuperabili dallo storico Git. I datasheet specifici
per ciascun riferimento sono indicati anche in [BOM.csv](BOM.csv).

| Uso | Fonte conservata / autorità |
|---|---|
| Contenitore confermato dall'utente nella #14 | [STEP originale Hammond 29830PSLA](../cad/sources/Hammond_29830PSLA.stp); [disegno originale, rev. 03.01.2024](../cad/sources/Hammond_29830PSLA.pdf), [fonte Hammond](https://www.hammfg.com/files/parts/pdf/29830PSLA.pdf) |
| STORICO: dual RCA RCJ-2123 sostituito, assente da BOM e assieme attivi | [STEP originale](../cad/sources/SameSky_RCJ-2123.step), [disegno RCJ-21](../cad/sources/rcj_21.pdf); [servizio CAD Same Sky](https://www.sameskydevices.com/product/resource/3dmodel/rcj-21-series/rcj-2123), [termini](https://www.sameskydevices.com/cad-model-terms) |
| Display 5393 | [Eagle, licenza e revisione originali](../cad/sources/adafruit-5393/); [repository Adafruit](https://github.com/adafruit/Adafruit-1.47in-320x172-Round-Rectangle-TFT-PCB), commit `73022f63a4b82117093ff5c250c119b6720005df` |
| DAC 6250 | [Eagle, licenza e revisione originali](../cad/sources/adafruit-6250/); [repository Adafruit](https://github.com/adafruit/Adafruit-PCM510x-I2S-DAC-PCB), commit `2edfe9e71b59e6acc73d0ab2b2f796149146ce74`; [PCM5102](pcm5102.pdf) |
| Potenziometri ed encoder | [Bourns PTV/PTT](../cad/sources/Bourns-PTV-PTT.pdf), [PEC11R](../cad/sources/PEC11R.pdf); disegni dei componenti effettivi PTV111-3420A-B203, PTV111-1420A-B103 e PEC11R-4120F-S0018 |
| Pulsanti CHERRY | [MX2A Black](../cad/sources/Cherry-MX2A-Black.pdf), [layout MX originale](../cad/sources/Cherry-MX-Legacy-Layout.pdf); [notifica ufficiale di sostituzione MX1A → MX2A](https://www.mouser.com/PCN/CHERRY_MX1A_11NW_EOL.pdf). Footprint nativo KiCad; prova fisica pendente |
| Manopole acquistate | [Cliff K85M / CL170805B](../cad/sources/Cliff-K85M.pdf), [Same Sky SAK-023](../cad/sources/SameSky-SAK-023.pdf), [Davies 1100](../cad/sources/Davies-Catalog-Instrument-Knobs.pdf); identificazione nell'[ordine sorgente](acquisti.csv) |
| Ferramenta M9 da valutare | [Alps RKZ00023UA](../cad/sources/Alps-RKZ00023UA-hardware.pdf): riferimento dimensionale alternativo, non una scelta di montaggio approvata |
| ESP32 e multiplexer | [ESP32-S3-WROOM-1/1U](esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf), [Toshiba 74HC4051D](74HC4051D_datasheet_en_20201203.pdf) |
| Alimentazione | [MPS MPM3612GLQ-33](MPM3612GLQ-33.pdf), [Bel 0ZCG](ds-cp-0zcg-series.pdf), [TI TPS22918](TI-TPS22918.pdf); [dati originali DC-bias Samsung CL21A226MPCLRNC](Samsung-CL21A226MPCLRNC-DCBias.json) |
| USB | [GCT USB4085 RevB](GCT-USB4085-RevB.pdf), [PCN GCT](GCT-USB4085-PCN20230426.pdf), [SLKOR USBLC6-2SC6](SLKOR-USBLC6-2SC6.pdf). I documenti ST eliminati non descrivono il componente SLKOR montato |
| Altri componenti dello schema | [C&K PTS636](CK-PTS636.pdf), [Murata GRM1885C1H103JA01](Murata-GRM1885C1H103JA01.pdf), [Yageo RC0603FR-074K7L](Yageo-RC0603FR-074K7L.pdf) |

La libreria RCA storica conserva la trasformazione rigida preesistente:
`X = -x + 0.290298296874877`, `Y = z + 2.80222558586811`,
`Z = y + 9.25535350182772` mm. È espressa nei parametri 3D del footprint;
lo STEP originale resta integro. Nessuno degli STEP o dei disegni conservati
dimostra il fit dell'assieme nell'Hammond. Non sono disponibili STEP originali
completi dei breakout nelle revisioni Adafruit conservate.

## J2 attivo: Neutrik NRJ4HF (non NRJ4HF-1)

- [Pagina Neutrik](https://www.neutrik.com/en/product/nrj4hf), [DigiKey 6473-NRJ4HF-ND](https://www.digikey.it/it/products/detail/neutrik-americas-inc/NRJ4HF/29427414).
- [Disegno originale](../cad/sources/Neutrik_NRJ4HF.pdf), [origine](https://www.neutrik.com/media/8561/download/st-nrj4hf.PDF?v=2).
- [STEP originale](../cad/sources/Neutrik_NRJ4HF.stp), [origine](https://www.neutrik.com/media/12335/download/nrj4hf-2.stp?v=1).
- [Circuiti originali Neutrik](../cad/sources/Neutrik_Jacks_Circuits.pdf), [origine](https://www.neutrik.com/media/8557/download/jacks---circuits.pdf).
- Accessorio meccanico obbligatorio: **NRJ-NUT-B, quantità 1**, [disegno originale](../cad/sources/Neutrik_NRJ-NUT-B.pdf), [origine](https://www.neutrik.com/media/8611/download/nrj-nut-b_r-1.pdf?v=1), [DigiKey](https://www.digikey.it/it/products/detail/neutrik-americas-inc/NRJ-NUT-B/29427303). Registrato qui come ferramenta, senza simbolo elettrico fittizio.

Il footprint standard `Connector_Audio:Jack_6.35mm_Neutrik_NRJ4HF_Horizontal`
coincide con la foratura del disegno Neutrik: interassi elettrici 12,70 x 11,43 mm,
quattro fori D1,50; centraggio D2 a x=9,50; foro opzionale D3 a (3,15; 2,13).
Coordinate locali KiCad: T=(0;0), TN=(0;11,43), S=(12,70;0), SN=(12,70;11,43).
Il simbolo standard `Connector_Audio:AudioJack2_Switch` conserva gli stessi nomi dei pin.

Lo STEP fornito da Neutrik è collegato direttamente a J2, senza copie trasformate.
Percorso `${KIPRJMOD}/../cad/sources/Neutrik_NRJ4HF.stp`; scala 1;
rotazioni **nella finestra KiCad** X=90, Y=0, Z=90 gradi; offset (16,95; -7,23; 0) mm.
Nel file KiCad i segni delle rotazioni sono opposti a quelli dell'interfaccia.
Il file standard della libreria 3D KiCad non era disponibile nell'installazione.

Il dado nel CAD è solo un inviluppo conservativo D17,45 x 4,75 mm (chiave 15),
non uno STEP originale né una simulazione della filettatura. Il modello USB proviene
invece dalla libreria KiCad installata `Connector_USB.3dshapes/USB_C_Receptacle_GCT_USB4085.step`.
MASTER e DAC sono rappresentati solo dalle proiezioni dichiarate in assieme.
Gli hash dei quattro originali Neutrik e le misure sono in
[nrj4hf-validation.json](../cad/nrj4hf-validation.json).

La prosecuzione dopo il merge della PR #17 introduce una proposta **nel solo CAD**:
traslazione J2 di −4,5 mm lungo X, asse X12,5. Il modello nel PCB mantiene la
posizione della PR #17; il precedente confronto STEP è riferito a quella posa.
Gli inviluppi frontali PTV111-3 sono proiezioni F.Fab 12 × 13,4 mm del footprint
del progetto, lette con Konnect, estruse per i 7,5 mm del corpo nel disegno Bourns.
Non sono STEP originali e non includono piedini, clip, cavità o tolleranze.
Il loro uso è limitato allo screening conservativo delle interferenze.

Per il display, la [pagina Adafruit 5393](https://www.adafruit.com/product/5393)
riporta 5,4 mm complessivi, senza separare PCB, vetro e componenti posteriori:
questa quota non è stata trasformata in un'altezza del vetro presunta.
Nel file Eagle conservato alla revisione `73022f63a4b82117093ff5c250c119b6720005df`,
`layerSetup=(1*16)`, `mtIsolate=1.5mm` e `mtCopper=0.035mm` per lato danno
1,57 mm di **stackup CAD nominale**. Conferma della fonte segnalata dall'utente,
non certificazione dello spessore prodotto. Spessore PCB fisico e quota PCB-vetro
restano entrambi da misurare; non guidano l'accoppiamento verticale definitivo.

Le boccole frontali sono cilindri nominali M9 × 5 dal disegno Bourns PTV/PTT,
pagina 2, estrusi sopra il piano frontale nominale del corpo a Z31,3. La filettatura
e i componenti completi non sono modellati. Sono usate soltanto per due sonde
rettilinee parziali di montaggio, senza dimostrazione del percorso inclinato completo.
