# ESP32-S3 Dub Siren

Dub siren autonoma. Il DSP gira interamente sull'ESP32-S3 e il PCM mono
elaborato esce soltanto via I2S verso un DAC PCM5102A. La USB serve per
alimentazione, upload del firmware e un protocollo seriale di controllo e
diagnostica; non trasporta audio.

Il pannello ha un solo layout con opzioni. Quello di fabbrica offre SINE 1,
SINE 2, TEST TONE e SQUARE, Pitch a tre posizioni e Rate a tre velocità più
Manual. Con Decay a zero (default) il dry usa solo un gate anti-click di 2 ms
in attacco e 3 ms in rilascio. Echo Level zero esclude
completamente il wet. Le voci e la risposta dei controlli hanno una taratura
di progetto; una corrispondenza sonora con strumenti esterni non è misurata.

L'encoder scorre in tutto 18 voci: alle quattro di base si aggiungono Sonar,
Saw, Triangle, due Two Tone, cinque Lickshot, tre Ping (Syndrum) e Gunshot.
Dynamic Sweep, forme LFO aggiuntive, Tone/Contour ed Echo Color/Loop/Spin sono
opzioni dello stesso layout, raggiungibili dal menu; Sonar emette ping
pulsanti con cadenza regolata da Rate.
Il DSP/control C++ è condiviso da firmware
e [app Android](Android/README.md). Tarature, fonti e limiti sono in [controls](docs/controls.md);
le verifiche della revisione sono in [baseline validation](docs/baseline-validation.md).

## Requisiti

- ESP32-S3 DevKitC-1 o il PCB del progetto, collegati con un cavo USB dati
- modulo DAC PCM5102A per l'uscita audio
- display Adafruit 5393 / ST7789
- VS Code con estensione PlatformIO IDE, oppure PlatformIO Core 6.x
  (Python 3.11 o successivo)

La configurazione usa il profilo PlatformIO `esp32-s3-devkitc-1-n16r8`,
flash da 16 MB, Hardware CDC e USB Serial/JTAG. Il display non richiede PSRAM.

## Build e upload firmware

Da un terminale nella cartella `firmware`:

```powershell
python -m platformio run
python -m platformio run --target upload --upload-port COM6
```

Sostituire `COM6` con la porta effettiva. Usare la connessione USB nativa che
Windows identifica come Espressif USB Serial/JTAG (`VID 303A`, `PID 1001`), non
una porta Bluetooth. Al primo upload può essere necessario entrare nel
bootloader: tenere premuto **BOOT**, premere e rilasciare **RESET/EN**, quindi
rilasciare **BOOT**. Dopo l'upload, se resta nel bootloader, premere una volta
solo **RESET/EN**.

In VS Code sono equivalenti i comandi **PlatformIO: Build** e
**PlatformIO: Upload**.

## Collegamento PCM5102A

Il DAC usa I2S Philips standard a 48 kHz, stereo 16 bit. Il firmware duplica
ogni campione mono sui canali sinistro e destro; il PCM5102A ricava internamente
il clock necessario e non richiede MCLK dall'ESP32.

```text
ESP32 GPIO16 -> PCM5102A BCK
ESP32 GPIO17 -> PCM5102A LCK
ESP32 GPIO18 -> PCM5102A DIN
PCM5102A SCL/SCK -> GND
PCM5102A FMT -> GND
```

## Uso

Per provare la sirena senza hardware si usa l'[app Android](Android/README.md),
che compila gli stessi sorgenti C++ del firmware.

La superficie predefinita presenta PITCH, RATE, ECHO LEVEL, ECHO TIME,
ECHO FEEDBACK ed ECHO FILTER; BTN1 TRIGGER, BTN2 HOLD, BTN3 MOD UP e
BTN4 ECHO CUT. L'encoder scorre le voci del Trigger; premendolo si apre il menu
al rilascio del click. I sei pot e i quattro pulsanti sono configurabili da
SETUP nel controller C++ condiviso; ogni pot agisce subito appena si muove,
anche all'accensione e dopo un cambio di voce. Un pulsante può diventare un pad Play Sound con
un suono proprio: quando è selezionato, la rotazione libera cambia la variante
del pad. Con un pulsante Trigger o Play Sound
fisicamente premuto, l'encoder applica un transpose live temporaneo; Depth si
regola dal menu MOD o da un pot. Il Master resta separato e non assegnabile sul
retro; girandolo il display mostra MASTER e il livello. Il PCB non ha serigrafia: la Home 320x172 è una mappa del pannello, con
ogni pot e tasto nella sua posizione, la funzione assegnata, la posizione dei
pot e lo stato (Hold, pad selezionato), e al centro la voce. Il driver ST7789 usa la
configurazione del 5393, verificata sul primo PCB (orientamento, centratura, colori).
Dettagli: [pannello UI](docs/panel-ui.md).

Dettagli: [architettura](docs/architecture.md), [protocollo](docs/protocol.md) e
[controlli](docs/controls.md).

## Test

La [CI](.github/workflows/ci.yml) esegue i test DSP nativi su Windows e
compila entrambi i firmware su Ubuntu. I binari e i file ELF
sono pubblicati come artefatti del run, separati per ambiente. Non viene
eseguito alcun upload su scheda. Le versioni firmware sono fissate in
`firmware/platformio.ini`; la CI usa PlatformIO 6.2.0.

```powershell
python -m pip install -r tests\native\requirements.txt
python tests\native\run.py
python -m platformio run -d firmware -e esp32-s3-devkitc-1-n16r8 -e panel-prototype
```

Il test nativo usa Zig C++ (`tests/native/requirements.txt`) oppure il compilatore indicato
da `CXX`; esegue parser e DSP senza scheda. Con hardware collegato, leggere
`renderUs`, `maxRenderUs`, `cycleUs` e `maxCycleUs` da STATUS durante gli scenari
di sweep e verificare che restino sotto 10.000 µs per blocco.
`displayRenderUs` e `maxDisplayRenderUs` misurano separatamente il tempo che
`loop()` passa a disegnare un frame; l'invio all'ST7789 avviene poi via DMA.

## App Android

[Android](Android/README.md) è il simulatore della sirena e dell'ESP32, in
un'unica APK. Compila con l'NDK gli stessi sorgenti `firmware/src` audio, control e
display, e riproduce il pannello fisico con le misure del PCB.

## Hardware

**PCB desktop a vista** 86 × 126 mm, senza custodia obbligatoria:
ESP32-S3-WROOM-1U, DAC Adafruit 6250 e display Adafruit 5393, solder mask nero
con finitura ENIG, quattro fissaggi M3. Il PCB è instradato e passa il DRC; il
montaggio fisico resta da provare sul prototipo.

- [Hardware: stato, decisioni e file](hardware/README.md)
- [Riferimento tecnico del PCB](hardware/docs/pcb.md), [guida di montaggio DIY](hardware/docs/assembly.md)

Il firmware configura il 5393 con `init(172, 320)` e rotazione landscape,
ottenendo un'area runtime di 320×172. Usa lo SPI hardware esistente:

```text
GPIO7  -> TFT RST
GPIO8  -> TFT DC
GPIO9  -> TFT CS
GPIO11 -> TFT MOSI
GPIO12 -> TFT SCK
GPIO14 -> backlight (livello fisso HIGH)
```

GPIO13/MISO non serve al TFT write-only e la microSD del breakout non è
implementata. Il disegno usa **Adafruit GFX Library**; l'invio del frame usa il
driver nativo ESP-IDF `esp_lcd` (ST7789, SPI con DMA) in
`firmware/src/display/PanelLcd.cpp`, con l'inizializzazione e gli offset che la
libreria Adafruit ST7789 usa per il 5393.

Il funzionamento del display sull'hardware reale e l'integrazione del firmware
con la scheda definitiva restano da verificare.

## Firmware per il pannello autonomo (issue #5-#12)

La revisione aggiunge articolazioni Lickshot, Tone/Contour, Echo Color,
menu Sound/Mod/Echo/Setup e quattro memorie sonore con SAVE/RECALL.
Vedi [controlli](docs/controls.md), [menu e memorie](docs/panel-ui.md) e
[pinout, build autonoma e verifiche hardware pendenti](docs/standalone.md).
Le nuove tarature sonore richiedono ascolto e misure sull'ESP.

Il [tap tempo](docs/tempo.md) aggiunge BPM e divisioni ritmiche indipendenti
per Echo e LFO, con azione pulsante assegnabile e accesso dal menu.
