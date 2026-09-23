# ESP32-S3 Dub Siren

Dub siren controllata da una GUI Windows. Il DSP gira interamente sull'ESP32-S3;
il PC riceve PCM mono già elaborato e lo inoltra senza resampling alla scheda audio.
Il PCM5102A non è usato in questa milestone.

## Requisiti Windows

- ESP32-S3 DevKitC-1 collegata con un cavo USB dati
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

## Avvio

Dalla cartella `pc`:

```bat
run.bat
```

La GUI si apre anche senza ESP collegata. Poi:

1. premere **Refresh** e scegliere la COM Espressif;
2. premere **Connect** e attendere `Connected · firmware 0.1.0`;
3. premere **Start Audio**;
4. usare **TRIGGER** oppure attivare **HOLD**.

**Stop Audio** arresta il flusso; **Disconnect** chiude ordinatamente audio,
thread e seriale. Per un controllo automatico dell'hardware collegato:

```powershell
.venv\Scripts\python.exe smoke_test.py COM6
```

Dettagli: [architettura](docs/architecture.md), [protocollo](docs/protocol.md) e
[controlli](docs/controls.md).
