# ESP32-S3 Dub Siren

Dub siren controllata da una GUI Windows. Il DSP gira interamente sull'ESP32-S3
e il PCM mono già elaborato può essere inviato al PC, a un DAC PCM5102A via
I2S, oppure a entrambe le destinazioni senza resampling.

Il motore V2 usa oscillatori pulse PolyBLEP, shaping RC mode-specifico,
saturazione asimmetrica e un LFO CLASSIC a carica/scarica esponenziale. Il
comando di debug `SET VOICING LEGACY|V2` permette il confronto a parità di
controlli; V2 è il default.

## Requisiti Windows

- ESP32-S3 DevKitC-1 collegata con un cavo USB dati
- modulo DAC PCM5102A per l'uscita audio GPIO
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

La GUI si apre anche senza ESP collegata. Poi:

1. premere **Refresh** e scegliere la COM Espressif;
2. scegliere **PC**, **PCM5102A** o **Both** nel selettore Output;
3. premere **Connect** e attendere `Connected · firmware 0.4.0`;
4. premere **Start Audio**;
5. usare **TRIGGER** oppure attivare **HOLD**.

**Stop Audio** arresta il flusso; **Disconnect** chiude ordinatamente audio,
thread e seriale. Per un controllo automatico dell'hardware collegato:

```powershell
.venv\Scripts\python.exe smoke_test.py COM6
```

Dettagli: [architettura](docs/architecture.md), [protocollo](docs/protocol.md) e
[controlli](docs/controls.md).

Per creare dieci brevi WAV di confronto direttamente dal PCM firmware (con la
scheda collegata), eseguire dalla cartella `pc`:

```powershell
.venv\Scripts\python.exe performance_capture.py COM6
```

Lo script stampa la directory temporanea che contiene i WAV. Usare
`--output-dir <percorso>` per scegliere una destinazione persistente. Il profilo
CLASSIC offre preset di progetto, non una riproduzione misurata di altri modelli.

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
