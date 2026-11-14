# ESP32-S3 Dub Siren

Dub siren controllata da una GUI Windows. Il DSP gira interamente sull'ESP32-S3
e il PCM mono già elaborato può essere inviato al PC, a un DAC PCM5102A via
I2S, oppure a entrambe le destinazioni senza resampling.

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

## Requisiti Windows

- ESP32-S3 DevKitC-1 collegata con un cavo USB dati
- modulo DAC PCM5102A per l'uscita audio GPIO
- display Adafruit 5393 / ST7789 opzionale per lo sviluppo firmware; il
  montaggio sul PCB resta da validare, come descritto sotto
- VS Code con estensione PlatformIO IDE, oppure PlatformIO Core 6.x
- Python 3.11 o successivo (verificato con Python 3.13.14)
- uscita audio Windows

La configurazione usa il profilo PlatformIO `esp32-s3-devkitc-1-n16r8`,
flash da 16 MB, Hardware CDC e USB Serial/JTAG. Il display non richiede PSRAM.

## Preparazione

Aprire un prompt nella cartella `pc` ed eseguire una volta:

```bat
setup.bat
```

Lo script crea `.venv`, aggiorna pip e installa `pyserial` e `sounddevice`.

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

## Avvio

Dalla cartella `pc`:

```bat
run.bat
```

La GUI controlla l'ESP32-S3 reale via USB seriale. Per provare la sirena senza
hardware si usa l'[app Android](Android/README.md), che compila gli stessi
sorgenti C++ del firmware.

La superficie predefinita presenta PITCH, RATE, ECHO LEVEL, ECHO TIME,
ECHO FEEDBACK ed ECHO FILTER; BTN1 TRIGGER, BTN2 HOLD, BTN3 MOD UP e
BTN4 ECHO CUT. L'encoder scorre le voci del Trigger; premendolo si apre il menu
al rilascio del click. I sei pot e i quattro pulsanti sono configurabili da
SETUP nel controller C++ condiviso; Pickup evita salti dopo un cambio di
assegnazione o di parametro, mentre all'accensione ogni pot vale subito dove si
trova. Un pulsante può diventare un pad Play Sound con
un suono proprio: quando è selezionato, la rotazione libera cambia la variante
del pad. Con un pulsante Trigger o Play Sound
fisicamente premuto, l'encoder applica un transpose live temporaneo; Depth si
regola dal menu MOD o da un pot. Il Master resta separato e non assegnabile sul
retro. Il PCB non ha serigrafia: la Home 320x172 è una mappa del pannello, con
ogni pot e tasto nella sua posizione, la funzione assegnata, la posizione dei
pot e lo stato (Pickup, Hold, pad selezionato), e al centro la voce. Il driver ST7789 usa la
configurazione del 5393, ancora da verificare su hardware.
Dettagli: [pannello UI](docs/panel-ui.md).

Con l'hardware reale:

1. premere **Refresh** e scegliere la COM Espressif;
2. scegliere **PC**, **PCM5102A** o **Both** nel selettore Output;
3. premere **Connect** e attendere `Connected · firmware 0.9.0`;
4. premere **Start Audio**;
5. usare BTN1 Trigger, BTN2 Hold, BTN3 Mod Up e BTN4 Echo Cut;
   ruotare l'encoder per scegliere la voce, premerlo per il menu.

**Stop Audio** silenzia sia il PC sia il DAC. **Connect** prende il controllo
delle uscite e le mantiene silenziose fino a **Start Audio**; **Disconnect**
le silenzia e chiude audio, thread e seriale. Il DSP continua a elaborare le
code echo. Dopo Disconnect il DAC resta disabilitato fino a un nuovo Start,
`LOCAL_AUDIO 1` o riavvio; all'accensione l'uso autonomo resta abilitato.
Per un controllo automatico dell'hardware collegato:

```powershell
.venv\Scripts\python.exe smoke_test.py COM6
```

Dettagli: [architettura](docs/architecture.md), [protocollo](docs/protocol.md) e
[controlli](docs/controls.md).

Per creare ventotto brevi WAV di confronto direttamente dal PCM firmware
(con la scheda collegata), eseguire dalla cartella `pc`:

```powershell
.venv\Scripts\python.exe performance_capture.py COM6
```

Lo script stampa la directory temporanea che contiene i WAV. Usare
`--output-dir <percorso>` per scegliere una destinazione persistente. Le
capture usano preset di progetto, non una riproduzione misurata di altri modelli.
Le capture includono Dynamic Sweep su SINE1/LICKSHOT/SONAR, Syndrum up/down e
sine/square, più Gunshot dry/echo; richiedono hardware reale e non costituiscono
uno smoke test I2S/DAC.

## Test

La [CI](.github/workflows/ci.yml) esegue i test Python della GUI e DSP nativi
su Windows e compila entrambi i firmware su Ubuntu. I binari e i file ELF
sono pubblicati come artefatti del run, separati per ambiente. Non viene
eseguito alcun upload su scheda. Le versioni firmware sono fissate in
`firmware/platformio.ini`; la CI usa PlatformIO 6.2.0.

```powershell
pc\.venv\Scripts\python.exe -m unittest discover -s pc -p "test_*.py" -f
pc\.venv\Scripts\python.exe -m pip install -r tests\native\requirements.txt
pc\.venv\Scripts\python.exe tests\native\run.py
pc\.venv\Scripts\python.exe -m platformio run -d firmware -e esp32-s3-devkitc-1-n16r8 -e panel-prototype
```

Il test nativo usa Zig C++ (`tests/native/requirements.txt`) oppure il compilatore indicato
da `CXX`; esegue parser e DSP senza scheda. Con hardware collegato, leggere
`renderUs`, `maxRenderUs`, `cycleUs` e `maxCycleUs` da STATUS durante gli scenari
di sweep e verificare che restino sotto 10.000 µs per blocco.
`displayRenderUs` e `maxDisplayRenderUs` misurano separatamente il ridisegno
del display ST7789 e devono restare sotto il budget display di 100.000 µs.

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
implementata. Le librerie sono **Adafruit GFX Library** e **Adafruit ST7735 and
ST7789 Library**. In base alla [guida ufficiale Adafruit 5393](https://learn.adafruit.com/adafruit-1-47-round-rectangle-tft-display/arduino-wiring-test),
gli offset del pannello sono gestiti dalla libreria.

Il funzionamento del display sull'hardware reale e l'integrazione del firmware
con la scheda definitiva restano da verificare.

## Firmware per il pannello autonomo (issue #5-#12)

La revisione aggiunge articolazioni Lickshot, Tone/Contour, Echo Color,
menu Sound/Mod/Echo/Setup e quattro memorie sonore con SAVE/RECALL.
Vedi [controlli](docs/controls.md), [menu e memorie](docs/panel-ui.md) e
[pinout, build autonoma e verifiche hardware pendenti](docs/standalone.md).
`STREAM 0` interrompe soltanto USB; `LOCAL_AUDIO 0` silenzia l'uscita locale.
Le nuove tarature sonore richiedono ascolto e misure sull'ESP.

Il [tap tempo](docs/tempo.md) aggiunge BPM e divisioni ritmiche indipendenti
per Echo e LFO, con azione pulsante assegnabile e accesso dal menu.
