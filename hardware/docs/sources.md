# Fonti hardware

Fonti conservate per lo schema attivo e la futura verifica meccanica #14.
I file originali spostati mantengono il contenuto; studi, rendering e report delle
revisioni eliminate sono recuperabili dallo storico Git. I datasheet specifici
per ciascun riferimento sono indicati anche in [BOM.csv](BOM.csv).

| Uso | Fonte conservata / autorità |
|---|---|
| Contenitore da verificare | [STEP originale Hammond 29830PSLA](../cad/sources/Hammond_29830PSLA.stp); [disegno Hammond](https://www.hammfg.com/files/parts/pdf/29830PSLA.pdf) |
| Dual RCA RCJ-2123 | [STEP originale](../cad/sources/SameSky_RCJ-2123.step), [disegno RCJ-21](../cad/sources/rcj_21.pdf); [servizio CAD Same Sky](https://www.sameskydevices.com/product/resource/3dmodel/rcj-21-series/rcj-2123), [termini](https://www.sameskydevices.com/cad-model-terms) |
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

Il modello RCA usa la trasformazione rigida preesistente:
`X = -x + 0.290298296874877`, `Y = z + 2.80222558586811`,
`Z = y + 9.25535350182772` mm. È espressa nei parametri 3D del footprint;
lo STEP originale resta integro. Nessuno degli STEP o dei disegni conservati
dimostra il fit dell'assieme nell'Hammond. Non sono disponibili STEP originali
completi dei breakout nelle revisioni Adafruit conservate.
