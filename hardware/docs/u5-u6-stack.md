# Stack U6 (DAC) dietro U5 (display) — 7 novembre 2026

PCB: [ESP-DubSiren-recovered-diagnostic.kicad_pcb](../kicad/ESP-DubSiren-recovered-diagnostic.kicad_pcb).

**Decisione dell'utente:** DAC U6 sul retro della portante, display U5 sul fronte,
uno sopra l'altro, così le saldature del DAC restano nascoste dietro il display.
Il jack 3,5 mm del DAC viene rimosso; il DAC non ha viti ed è tenuto solo dai
14 pin degli header. Si monta prima il DAC, poi il display.

## Verifica

Proiettando i file Eagle ufficiali Adafruit 5393
([sorgente](../cad/sources/adafruit-5393/), trasformazione `Xc=62,431−Xe`, `Yc=61,74−Ye`,
la stessa della [issue 38](issue-38-rear-placement.md)), i componenti sul retro del
display sono rivolti verso la portante e cadono sopra i pin U6:

| Componente del display (verso la portante) | Pin U6 sottostanti |
|---|---|
| Q2 zoccolo microSD | 1, 2, 3, 4 (+ fori a sinistra) |
| Q3 SOT23 | 5, 6 |
| R5 0603, C2 0805 | 7, 8 |
| IC4 SOT23, R4/R1 0603 | 9, 10 |
| C4 0805 | 14 |

L'header U5 (riferimento SL11G1) lascia **2,50 mm** fra F.Cu della portante e la
PCB del display. Un pin U6 non tagliato sporge **1,40 mm** (3,00 − 1,60):
con un SOT23 di circa 1,1 mm il margine è nullo, con la microSD è negativo.
**Con i pin interi lo stack non entra.**

## Sequenza di montaggio richiesta

1. Saldare le strisce al DAC dalla sua faccia componenti.
2. Presentare il DAC sul retro della portante e saldare i pin corti dal fronte,
   ancora libero.
3. **Tagliare a filo i 14 pin U6** e controllare i giunti; pulire il flussante.
4. Prova a secco: presentare U5 con il suo header e verificare che nulla tocchi.
   Condizione: `altezza del giunto tagliato + altezza zoccolo microSD < 2,50 mm`
   (caso peggiore con la tolleranza dell'isolante: 2,30 mm). L'altezza dello
   zoccolo non è documentata da Adafruit: va misurata sul pezzo.
5. Solo dopo, saldare U5.

Se la prova a secco fallisce, l'alternativa è alzare U5 con un supporto più alto;
il fronte del display avanza della stessa quota.

## DRC

I 18 errori `pth_inside_courtyard`/`npth_inside_courtyard` U6↔U5 sono segnati come
**esclusioni** nel [progetto](../kicad/ESP-DubSiren-recovered-diagnostic.kicad_pro)
(`drc_exclusions`, con commento che rimanda a questo file). Non sono nascosti in
modo globale: le due regole restano errori per ogni altro componente.

DRC finale: **0 errori, 0 connessioni aperte**. Avvisi: 64 differenze di libreria
(footprint voluti, vedi #34/#35), 28 di serigrafia (accettata nella #36),
64 di parità per i campi `DesignStatus`/`MPN` assenti nei footprint.
H1–H4 sono segnati `board_only`, esclusi da BOM e piazzamento.
