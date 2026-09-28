# ESP32-S3 Dub Siren

Dub siren controllata da una GUI Windows. Il DSP gira interamente sull'ESP32-S3
e il PCM mono già elaborato può essere inviato al PC, a un DAC PCM5102A via
I2S, oppure a entrambe le destinazioni senza resampling.

Il motore V2 usa oscillatori pulse PolyBLEP, shaping RC mode-specifico,
saturazione asimmetrica e un LFO CLASSIC a carica/scarica esponenziale. Il
comando di debug `SET VOICING LEGACY|V2` permette il confronto a parità di
controlli; V2 è il default.

La revisione firmware 0.8.0 mantiene il DSP 0.6.0, il controller condiviso dei
quattro pad performativi e aggiunge lo stato UI condiviso più il driver del
display Adafruit 5394 / ST7789. **Dynamic Sweep**, **SYNDRUM** e
**GUNSHOT** restano disponibili. Syndrum usa carrier sine/triangle/square e un pitch transient
regolabile da -3 a +3 ottave; Gunshot usa rumore deterministico filtrato, non un
sample. Entrambi condividono delay, Echo Loop/Cut/Spin, master e routing con gli
altri mode. Un airhorn continuo resta disponibile con **MODE LICKSHOT +
MODULATION OFF** (`SET MODE LICKSHOT`, `MODULATION 0`). Tutti i voicing e le
relative costanti sono punti di taratura interni al progetto: non sono cloni
misurati di dispositivi commerciali. Syndrum è ispirato al comportamento
generale dei percussion synth analogici.

## Requisiti Windows

- ESP32-S3 DevKitC-1 collegata con un cavo USB dati
- modulo DAC PCM5102A per l'uscita audio GPIO
- display Adafruit 5394 / ST7789 opzionale per lo sviluppo firmware; il
  collegamento PCB V1 non è ancora completo, come descritto sotto
- VS Code con estensione PlatformIO IDE, oppure PlatformIO Core 6.x
- Python 3.11 o successivo (verificato con Python 3.13.14)
- uscita audio Windows

La configurazione usa il profilo PlatformIO base `esp32-s3-devkitc-1`, Hardware
CDC e USB Serial/JTAG. Non abilita PSRAM né imposta modalità flash specifiche.

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

La superficie predefinita riproduce il pannello previsto: sei potenziometri
frontali (Tune, Rate, Echo Level, Echo Time, Echo Feedback ed Echo Tone), quattro
pad momentanei, encoder con pressione, preview display e Master posteriore
separato. I pad iniziali sono Classic, Two Tone, Lickshot e Ping. Preview PC,
Virtual ESP e firmware usano lo stesso stato Home/Menu del controller C++; il
layout logico è sempre 320×170. Il display è una scena pixel-art diegetica:
stack, rack, VU, manopole e LED sono anche la UI dei parametri reali. Il boot
monta la scena e **Replay boot** nel Virtual ESP permette di rivederlo. Dettagli
e mapping: [pannello UI](docs/panel-ui.md).

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
3. premere **Connect** e attendere `Connected · firmware 0.8.0`;
4. premere **Start Audio**;
5. suonare BTN1–BTN4; l'encoder regola Depth e il suo push abilita le
   combinazioni secondarie.

**Stop Audio** arresta il flusso; **Disconnect** chiude ordinatamente audio,
thread e seriale. Per un controllo automatico dell'hardware collegato:

```powershell
.venv\Scripts\python.exe smoke_test.py COM6
```

Dettagli: [architettura](docs/architecture.md), [protocollo](docs/protocol.md) e
[controlli](docs/controls.md).

Per creare ventisei brevi WAV di confronto direttamente dal PCM firmware
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
`displayRenderUs` e `maxDisplayRenderUs` misurano separatamente il frame ST7789
e devono restare sotto il budget display di 100.000 µs.

## Hardware PCB V1

La revisione hardware diretta con ESP32-S3-WROOM-1, USB-C, ADC, I/O expander e
DAC Adafruit 6421 è documentata in [pcb/README.md](pcb/README.md). Il layout
corrente è pre-produzione e non deve essere inviato in fabbricazione.

Il firmware configura il 5394 con `init(170, 320)` e rotazione landscape,
ottenendo un'area runtime di 320×170. Usa lo SPI hardware esistente:

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
ST7789 Library**. In base alla [guida ufficiale Adafruit 5394](https://learn.adafruit.com/adafruit-1-9-color-ips-tft-display/arduino-wiring-test),
gli offset del pannello sono gestiti dalla libreria.

**BLOCKER PCB:** J3 deve essere rivisto per portare almeno `SPI_MOSI` e
`SPI_SCK` al display, oppure deve essere definita esplicitamente un'altra
connessione fisica. Il firmware compila, ma il funzionamento sul display reale
non è stato verificato e il cablaggio J3 corrente non è sufficiente.
