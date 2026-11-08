# Modelli 3D del PCB

Modelli STEP usati da [ESP-DubSiren.kicad_pcb](../../kicad/ESP-DubSiren.kicad_pcb)
e dai footprint della libreria locale. Sono già nel sistema di riferimento del
footprint: X uguale, Y opposto (asse 3D di KiCad), Z=0 sulla superficie del PCB.
In KiCad hanno offset 0, rotazione 0 e scala 1, salvo dove indicato.

- Generazione: [build_3d_models.py](../../tools/build_3d_models.py) (FreeCAD 1.1, senza GUI).
- Assegnazione: [assign_3d_models.py](../../tools/assign_3d_models.py) (Python di KiCad, con il PCB chiuso).

## Provenienza

| File | Riferimenti | Fonte | Note |
|---|---|---|---|
| `Bourns_PTV111-3420A.step` | RV1–RV6 | Bourns, [`ptv111-3220a.stp`](https://bourns.com/resources/design-tools/engineering-files-list?folder=PTVPTT) | Stesso esterno del 3420A, che differisce solo per il detent interno. Appoggio sui 4 piedini da 1,3 mm |
| `Bourns_PTV111-1420A.step` | RV7 | Bourns, `ptv111-1x15a.stp` (PTV111-1415A) | Albero allungato di 5 mm (da 15 a 20 mm) nel tratto liscio Ø6, a 13 mm. Appoggio sui piedini a z=10 |
| `Bourns_PEC11R-4120F-S0018.step` | SW7 | Bourns, [`pec11r-4x20f-sxxxx.stp`](https://bourns.com/resources/design-tools/engineering-files-list?folder=PEC11R) | L'intestazione interna dice PEC11-4220F-S0024: cambia solo il numero di detent, non l'esterno |
| `vendor/ESP32-S3-WROOM-1U.STEP` | U1 | Espressif, [kicad-libraries](https://github.com/espressif/kicad-libraries) | Offset (−9; −9,60; 0): quello del footprint Espressif (−9,75) corretto di 0,15 mm, perché il pad 1 qui è a y=−8,41 invece di −8,26 |
| `Adafruit_6250_PCM510x_no_jack.step` | U6 | Adafruit, [Adafruit_CAD_Parts/6250](https://github.com/adafruit/Adafruit_CAD_Parts/tree/main/6250%20PCM510x%20I2S%20DAC) | Senza jack PJ-332A, che viene rimosso ([README hardware](../../README.md)). Offset z=2,5 mm, l'isolante SLxG1. Header non modellati |
| `CHERRY_MX2A-11NW_simplified.step` | SW1–SW4 | **Creato** dai datasheet CHERRY in `../sources` | Cherry non pubblica uno STEP. Inviluppo: corpo 15,6×15,6 alto 11,6 mm, parte inferiore 14×14×5, stelo a croce 4×1,2 alto 3,6, pin 3,3 (totale 18,5). Croce e pin di contatto sono approssimati |
| `CK_PTS636SM43SMTR_simplified.step` | SW5, SW6 | **Creato** da [CK-PTS636.pdf](../../docs/CK-PTS636.pdf), p.2 | Corpo 6,0×3,5×3,3, attuatore 3,0×1,5 fino a 4,3, terminali larghi 1,0 con ingombro totale 8,7. Spessore dei terminali (0,3) approssimato |
| `Bel_0ZCG0110CF2B_envelope.step` | F1 | **Creato**: corpo massimo 4,73×3,41×1,30 | Solo inviluppo |
| `MPS_MPM3612GLQ-33_envelope.step` | U2 | **Creato**: LGA 3×3×2 mm (datasheet p.1) | Solo inviluppo |
| `Same_Sky_SAK-023_on_PEC11R.step` | SW7 (manopola) | Same Sky, [`Same_Sky_SAK-023.step`](https://www.sameskydevices.com/product/resource/3dmodel/sak-023) (STEP AP214, CAD Model Terms Same Sky) | Piatto D allineato a quello dell'albero (lato −X). Fondo del foro (9,2 mm sopra il fondo del nucleo) sulla punta dell'albero, z=26,6 |
| `Davies_1100_envelope_on_PTV111-3.step` | RV1–RV6 (manopole) | **Creato** da [Davies-Catalog-Instrument-Knobs.pdf](../sources/Davies-Catalog-Instrument-Knobs.pdf) | Davies fornisce il disegno solo su richiesta. Gonna Ø25/32", corpo Ø21/32", altezza 15/32", foro Ø1/4" profondo 13/32" sulla punta dell'albero (z=27,4). L'altezza della gonna (2,1 mm) non è quotata ed è letta dalle proporzioni del disegno. Scanalature, disco e grano omessi |
| `Cliff_K85M_CL170805B_envelope_on_PTV111-1.step` | RV7 (manopola) | **Creato** da [Cliff-K85M.pdf](../sources/Cliff-K85M.pdf) | Cliff fornisce lo STEP solo su richiesta. Gonna Ø14×3,5 con incavo Ø12×2,5, corpo conico 11,2→10 (le misure A/F sono usate come diametri) fino a 17,0, foro D Ø6 / 5 A/F profondo 12,5 sulla punta dell'albero (y=−27,5). Linea di riferimento (fino a 17,8) omessa |

Le manopole sono un secondo modello negli stessi footprint, spinte fino in fondo
(nessun pannello modellato). Rispetto al modello dell'albero resta una
sovrapposizione di circa 9 mm³ per SAK-023 e K85M: il foro a D delle manopole
copre anche il tratto tondo dell'albero sotto il piatto e, per il SAK-023, le
alette di serraggio del nucleo. Il Davies 1100 ha un foro da 1/4" (6,35 mm) sugli
alberi da 6 mm dei PTV111, fissato col grano. I keycap di SW1–SW4 non hanno MPN e non sono modellati.

`vendor/` contiene i file originali scaricati, necessari per rigenerare i modelli.
Ancora senza modello: **U5** (Adafruit 5393), perché Adafruit non pubblica uno STEP
e gli spessori di vetro e modulo TFT non sono documentati. TP1–TP7 e H1–H4 non lo richiedono.

## Verifica

In uno STEP esportato da KiCad (origine 0,0) ogni foro di RV1–RV7, SW1–SW4 e SW7
contiene un pin del modello. Il confronto col corpo della scheda ha portato a
correggere le asole dei footprint `Bourns_PTV111-3`, `Bourns_PTV111-1` e
`Bourns_PEC11R-4120F-S0018`: ora resta solo il dente dell'aggancio a scatto.
Vedi [pcb.md](../../docs/pcb.md#footprint-locali).

## STEP per gruppo

Dalla cartella `hardware/kicad`, con l'eseguibile `kicad-cli` di KiCad 10.
L'uscita va in `hardware/cad/step-gruppi/`, che non è versionata:

```bash
kicad-cli pcb export step -f --no-board-body --component-filter "SW1,SW2,SW3,SW4" -o ../cad/step-gruppi/pulsanti.step ESP-DubSiren.kicad_pcb
kicad-cli pcb export step -f --no-board-body --component-filter "RV1,RV2,RV3,RV4,RV5,RV6,RV7" -o ../cad/step-gruppi/potenziometri.step ESP-DubSiren.kicad_pcb
kicad-cli pcb export step -f --no-board-body --component-filter "SW7" -o ../cad/step-gruppi/encoder.step ESP-DubSiren.kicad_pcb
kicad-cli pcb export step -f --no-board-body --component-filter "SW5,SW6" -o ../cad/step-gruppi/tasti-servizio.step ESP-DubSiren.kicad_pcb
kicad-cli pcb export step -f --no-board-body --component-filter "U5,U6" -o ../cad/step-gruppi/display-dac.step ESP-DubSiren.kicad_pcb
kicad-cli pcb export step -f --no-board-body --component-filter "J1,J2" -o ../cad/step-gruppi/connettori.step ESP-DubSiren.kicad_pcb
kicad-cli pcb export step -f --board-only --include-silkscreen --include-soldermask --include-pads -o ../cad/step-gruppi/scheda.step ESP-DubSiren.kicad_pcb
kicad-cli pcb export step -f -o ../cad/step-gruppi/assieme-completo.step ESP-DubSiren.kicad_pcb
```

Il gruppo `elettronica` usa l'elenco esplicito, perché un carattere jolly come `R*`
includerebbe anche RV1–RV7:

```bash
kicad-cli pcb export step -f --no-board-body --component-filter "C1,C2,C3,C4,C5,C6,C7,C8,C9,C10,C11,R1,R2,R3,R4,R5,R6,R7,R8,R9,R10,R11,R12,R13,R14,R15,R16,R17,R18,R19,R20,R21,D1,F1,U1,U2,U3,U4,U7" -o ../cad/step-gruppi/elettronica.step ESP-DubSiren.kicad_pcb
```
Tutti i file hanno la stessa origine, quindi importandoli nello stesso CAD si
sovrappongono già allineati, ognuno come corpo separato.
