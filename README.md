# ESP32-S3 Dub Siren

Dub siren controllata da una GUI Windows. Il DSP gira interamente sull'ESP32-S3
e il PCM mono già elaborato può essere inviato al PC, a un DAC PCM5102A via
I2S, oppure a entrambe le destinazioni senza resampling.

La baseline predefinita **DS01E / Classic** offre SINE 1, SINE 2, TEST TONE e
SQUARE, Pitch a tre posizioni e Rate a tre velocità più Manual. Il dry usa solo
un gate anti-click di 2 ms in attacco e 3 ms in rilascio. Echo Level zero esclude
completamente il wet. La revisione segue il comportamento funzionale descritto
nei manuali ufficiali Benidub, senza dichiarare un clone circuitale o una
corrispondenza sonora misurata.

**Extended** conserva Lickshot, Sonar, Syndrum, Gunshot, Dynamic Sweep, forme
LFO aggiuntive e controlli avanzati, e aggiunge i carrier SAW e TRIANGLE.
Sine 1 e Sine 2 usano le stesse voci di
Classic; Sonar emette ping pulsanti con cadenza regolata da Rate.
Il DSP/control C++ è condiviso da firmware
e Virtual ESP. Tarature, fonti e limiti sono in [controls](docs/controls.md);
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

La GUI funziona sia con l'ESP32-S3 reale sia senza hardware tramite **Virtual ESP**.
Il Virtual ESP compila e usa direttamente gli stessi sorgenti C++ DSP/control
del firmware; la libreria host viene ricompilata automaticamente quando quei
sorgenti cambiano.

La superficie predefinita presenta PITCH, RATE, ECHO LEVEL, ECHO TIME,
ECHO FEEDBACK ed ECHO FILTER; BTN1 TRIGGER, BTN2 HOLD, BTN3 MANUAL MOD e
BTN4 ECHO CUT. L'encoder cicla i quattro voicing; premendolo si apre il menu
al rilascio del click. I sei pot e i quattro pulsanti sono configurabili per
profilo dal controller C++ condiviso; Pickup evita salti dopo cambio profilo o
parametro. In Extended la rotazione libera cambia la variante del pad;
BTN1 premuto + rotazione regola Depth. Con gli altri pulsanti sonori
fisicamente premuti, l'encoder applica un transpose live temporaneo; in
Classic vale anche per BTN1. Il Master resta separato e non assegnabile sul
retro. La Home 320x172 mostra insieme etichette e valori fisici, con overlay
temporanei dei pot.
Il prodotto finale userà un display piccolo, principalmente testuale, per
stato, parametri, menu e impostazioni. Classic ed Extended mostrano solo testo;
il driver ST7789 usa la configurazione del 5393, ancora da verificare su hardware.
Dettagli: [pannello UI](docs/panel-ui.md).

Per lavorare senza hardware:

1. scegliere **Virtual ESP** nel selettore Device;
2. premere **Connect**;
3. premere **Start Audio**;
4. usare normalmente tutti i controlli della sirena.

In modalità Virtual ESP l'uscita è sempre **PC**. La catena fisica
I2S -> PCM5102A -> uscita analogica non è emulata.

Con l'hardware reale:

1. premere **Refresh** e scegliere la COM Espressif;
2. scegliere **PC**, **PCM5102A** o **Both** nel selettore Output;
3. premere **Connect** e attendere `Connected · firmware 0.9.0`;
4. premere **Start Audio**;
5. usare BTN1 Trigger, BTN2 Hold, BTN3 Manual Mod e BTN4 Echo Cut;
   ruotare l'encoder per scegliere il voicing, premerlo per il menu.

**Stop Audio** arresta il flusso; **Disconnect** chiude ordinatamente audio,
thread e seriale. Per un controllo automatico dell'hardware collegato:

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
`--output-dir <percorso>` per scegliere una destinazione persistente. Il profilo
CLASSIC offre preset di progetto, non una riproduzione misurata di altri modelli.
Le capture includono Dynamic Sweep su SINE1/LICKSHOT/SONAR, Syndrum up/down e
sine/square, più Gunshot dry/echo; richiedono hardware reale e non costituiscono
uno smoke test I2S/DAC.

## Test

```powershell
pc\.venv\Scripts\python.exe -m unittest discover -s pc -p "test_*.py"
pc\.venv\Scripts\python.exe tests\native\run.py
pc\.venv\Scripts\python.exe -m platformio run -d firmware
```

Il test nativo usa Zig C++ (`pip install ziglang`) oppure il compilatore indicato
da `CXX`; esegue parser e DSP senza scheda. Con hardware collegato, leggere
`renderUs`, `maxRenderUs`, `cycleUs` e `maxCycleUs` da STATUS durante gli scenari
di sweep e verificare che restino sotto 10.000 µs per blocco.
`displayRenderUs` e `maxDisplayRenderUs` misurano separatamente il ridisegno
del testo ST7789 e devono restare sotto il budget display di 100.000 µs.

## Hardware

L'unico progetto hardware è in [hardware/README.md](hardware/README.md):
ESP32-S3-WROOM-1U, DAC Adafruit 6250 e display Adafruit 5393.
Il **KiCad è provvisorio, senza routing né bordo definitivo: nessun PCB
fabbricabile**. La [issue #14](https://github.com/Capobuf/ESP-DubSiren/issues/14)
è in fase B con [pannello provvisorio](hardware/cad/gate-a-panel.pdf):
la [verifica Z](hardware/cad/gate-b-height-check.pdf) rileva un deficit di 0,9 mm
nello stack con RCA e potenziometri su facce opposte. Assieme completo,
sagoma PCB, inserimento e dima 3D restano da validare; i PDF non autorizzano la foratura.

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
con la scheda definitiva restano da verificare. La pulizia hardware della #15
non modifica firmware, collegamenti elettrici o pinout.

## Firmware per il pannello autonomo (issue #5-#12)

La revisione aggiunge articolazioni Lickshot, Tone/Contour, Echo Color,
menu Sound/Mod/Echo/Setup e quattro memorie Extended con SAVE/RECALL.
Vedi [controlli](docs/controls.md), [menu e memorie](docs/panel-ui.md) e
[pinout, build autonoma e verifiche hardware pendenti](docs/standalone.md).
`STREAM 0` interrompe soltanto USB; `LOCAL_AUDIO 0` silenzia l'uscita locale.
Le nuove tarature sonore richiedono ascolto e misure sull'ESP.

Il [tap tempo](docs/tempo.md) aggiunge BPM e divisioni ritmiche indipendenti
per Echo e LFO, con azione pulsante assegnabile e accesso dal menu.
