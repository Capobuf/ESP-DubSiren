# Repository review — 22 ottobre 2026

Base `84830999d7debec9d7db8c811c3fbbcbf71cae09`, branch
`codex/repository-review`. Review di firmware, DSP, controller, protocollo,
client desktop, Virtual ESP, build/test e documentazione hardware. I file CAD
e KiCad non sono stati modificati. I risultati e le attività residue sono in:

| Issue | Riscontro | Intervento in questo round |
|---|---|---|
| [#29](https://github.com/Capobuf/ESP-DubSiren/issues/29) | Stop non silenziava I2S; USB persa lasciava input remoti attivi; errori seriali e handshake non gestiti | Correzioni lifecycle, parsing e STATUS periodico |
| [#30](https://github.com/Capobuf/ESP-DubSiren/issues/30) | Nessuna CI; dipendenze firmware non fissate | GitHub Actions Windows/Ubuntu, due build, artefatti e versioni fissate |
| [#31](https://github.com/Capobuf/ESP-DubSiren/issues/31) | Build Virtual ESP bloccante, pot senza tastiera, layout/scaling da verificare, comandi senza NACK, overlay Depth errato | Corretti Depth e validazione iniziale STATUS; altre attività aperte |
| [#32](https://github.com/Capobuf/ESP-DubSiren/issues/32) | Nessuna prova sonora comparata/hardware in questo round; documentazione obsoleta | Rigenerate capture, corretti RCA/J3/stato PR17 e protocollo |

## Verifiche locali

- Suite Python/Virtual ESP e suite C++ DSP/controller passate. Le regressioni
  aggiunte coprono uscite, timeout, perdita seriale, pacchetti malformati,
  overlay e rilascio degli input remoti con pressione fisica simultanea.
- Build PlatformIO 6.2.0 riuscita per DevKit e `panel-prototype`, con
  Espressif32 6.13.0 e librerie Adafruit fissate. Nessun upload su scheda.
- Istanza Tk effettiva con configurazione temporanea: Connect, PCM non nullo,
  pad/rilascio, encoder/menu, Stop e Disconnect passati. Dimensione richiesta
  locale 1012×697; questo non verifica rendering a tutti i DPI o accessibilità.
- Rigenerate 20 clip Classic e 37 confronti Extended con gli script esistenti.
  Le metriche Extended corrispondono al file già versionato: nessuna nuova
  taratura DSP arbitraria è stata introdotta. WAV locali esclusi da Git.
- SINE1: dry nullo dopo 146 campioni dal rilascio; feedback zero dà una sola
  ripetizione. A feedback 0,8 i picchi delle prime sei ripetizioni sono
  12589, 7648, 5435, 4075, 3136, 2447. Sono misure del core, non del DAC.

## Limiti e prossime verifiche

L'implementazione produce una dub siren con modulazione ed echo; la somiglianza
timbrica con uno strumento esterno richiede registrazioni comparabili e ascolto.
Non è dimostrata da build, nomi delle voci o metriche aggregate. I valori
esistenti sono conservati e documentati come tarature di progetto.

Rimangono da misurare pad→audio, underrun/backpressure USB, I2S analogico,
mute/unmute, ADC/mux, encoder, NVS e display. La queue PC contiene fino a
120 ms di PCM, oltre alla latenza del backend audio. Le modifiche ai sink e
alla telemetria richiedono verifica sull'ESP.

Konnect ha identificato il progetto KiCad 10, ma i tool ERC/DRC/netlist non
sono diventati disponibili dopo il caricamento. Verdetto hardware:
**INCOMPLETE — review could not evaluate the full design**. Le verifiche
del 18 ottobre in `hardware/README.md` sono storiche. Bordo, routing,
meccanica e inserimento restano nella [#14](https://github.com/Capobuf/ESP-DubSiren/issues/14).

Per ripetere i controlli con arresto al primo errore, dalla root in PowerShell:

```powershell
pc\.venv\Scripts\python.exe -m unittest discover -s pc -p "test_*.py" -f
if ($LASTEXITCODE -ne 0) { throw "Python tests failed" }
pc\.venv\Scripts\python.exe tests\native\run.py
if ($LASTEXITCODE -ne 0) { throw "Native DSP tests failed" }
pc\.venv\Scripts\python.exe -m platformio run -d firmware -e esp32-s3-devkitc-1-n16r8 -e panel-prototype
if ($LASTEXITCODE -ne 0) { throw "Firmware build failed" }
```

Gli script di acquisizione esistenti sono `pc/baseline_capture.py` e
`pc/offline_capture.py`. Lo smoke test hardware è `pc/smoke_test.py PORTA`:
specificare una porta reale e un firmware aggiornato; si arresta alla prima
asserzione fallita. Lo stato CI remoto è nei run collegati alla PR.
