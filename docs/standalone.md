# Uso autonomo: firmware del pannello provvisorio

Branch issue #12. Il pinout deriva dalla netlist esportata con Konnect/kicad-cli
il 18 ottobre 2026 dallo schema salvato su origin/main aafeeba.
Nessun file KiCad modificato. Il PCB resta provvisorio; questa configurazione
non certifica montaggio, cablaggio o uscita analogica.

| Funzione | Collegamento verificato nella netlist |
|---|---|
| ADC comune | U1 IO4 / ADC_MUX -> U3 A (pin 3), C9 100 nF a massa |
| Mux S0/S1/S2 | GPIO5 / GPIO6 / GPIO10 -> U3 pin 11 / 10 / 9 |
| POT1..6 | RV1..6 cursore -> U3 A0..A5 |
| Master | RV7 cursore -> U3 A6 |
| Mux enable / A7 | a massa |
| BTN1..4 | GPIO38 / 39 / 40 / 41, switch a massa, pull-up esterni |
| Encoder A/B/push | GPIO1 / 2 / 42, comune e switch a massa |
| I2S BCLK/LRCK/DIN | GPIO16 / 17 / 18 -> Adafruit 6250 |

## Configurazioni

`esp32-s3-devkitc-1-n16r8` mantiene disabilitati gli ingressi fisici per non
leggere pot inesistenti sulla DevKit. `panel-prototype` abilita PanelInputs:
GPIO e ADC Arduino-ESP32, quadratura nel PCNT nativo ESP-IDF. Nessuna nuova
libreria. Compilare con:

```powershell
pc\.venv\Scripts\python.exe -m platformio run -d firmware -e panel-prototype
```

Il PCNT conta quattro transizioni per scatto; direzione e numero di scatti
vanno verificati con l'encoder montato. Il filtro PCNT e il debounce pulsanti
(2 ms) sono tarature iniziali, non misure. Un pulsante premuto al boot deve
prima essere rilasciato; nessun gate viene richiamato dalle memorie.

Il mux attende almeno 6 ms per canale, senza bloccare il loop: 20k + 4,7k e
100 nF danno una costante RC nominale massima di circa 0,62 ms. Master viene
letto ogni due canali, senza Pickup; i sei pot usano il Pickup del controller.
La normalizzazione ADC usa i massimi nominali dei divisori a 3,3 V (2672,1 mV
per i pot da 20k, 2244,9 mV per Master da 10k) e una soglia di variazione 5 mV.
Tolleranze, estremi ADC, diafonia e assestamento devono essere misurati.
Master parte da zero fino alla prima lettura valida. Il display puo ritardare
il polling: STATUS espone `maxInputPollGapUs`; il PCNT continua a contare.
Questi tempi non sono una promessa di latenza pulsante/uscita.

## Audio e USB

Dopo inizializzazione I2S riuscita, l'uscita locale e abilitata al boot, con
Trigger/Hold spenti. `STREAM 0|1` riguarda solo il flusso USB; `LOCAL_AUDIO 0|1`
controlla il mute locale. `SET OUTPUT PC|GPIO|BOTH` conserva la scelta delle
destinazioni: PC esclude I2S, GPIO esclude USB, BOTH le permette entrambe.
La rimozione improvvisa di USB arresta lo streaming USB e rilascia gli input
remoti momentanei, preservando input fisici e Hold intenzionali. Stop Audio e
Disconnect della GUI silenziano anche I2S tramite `LOCAL_AUDIO 0`; per tornare
all'uso autonomo abilitare l'uscita locale oppure riavviare.

Il task audio scrive prima I2S. Il sink USB prova a scrivere un pacchetto intero
solo quando c'e spazio nel ring del driver; mutex e timeout sono non bloccanti.
Se il PC non tiene il passo, il pacchetto USB viene scartato e il numero di
sequenza permette al ricevitore di rilevare il salto. Nessuna attesa da 500 ms
nel percorso audio o nel polling del pannello.

STATUS aggiunge `localAudio`, `usbStreaming`, `usbDroppedPackets`,
`i2sWriteFailures`, `panelInputsReady`, `maxInputPollGapUs`. Gli errori di
inizializzazione pannello/I2S/task audio e scrittura I2S compaiono sul display.
`panelInputsReady` conferma solo l'inizializzazione software: ADC/GPIO non
possono riconoscere un pot o un filo assente. Gli errori latched I2S richiedono
riavvio dopo la diagnosi.

## Verifica ancora necessaria sulla scheda

1. Avvio con sola alimentazione: nessun transitorio, Master a zero e poi corsa
   completa, sei pot, quattro pulsanti, encoder/menu e cambio profilo.
2. Misurare offset ADC, estremi e diafonia del mux. Verificare conteggio/senso
   encoder e rimbalzo pulsanti; osservare maxInputPollGapUs con display attivo.
3. Trigger/Hold/echo durante collegamento, scollegamento e backpressure USB.
4. Misurare acquisizione -> audio, render/cycle/I2S/display: percentili e massimo.
   Non chiudere #12 in base alla sola compilazione o al VirtualESP.

## Confronti offline

`pc\.venv\Scripts\python.exe pc\offline_capture.py` genera 37 WAV e metriche
in una directory temporanea: SHOT tap/hold/retrigger, ROLL lento/veloce, HORN,
Two Tone, Tone su quattro voci e tre registri, Contour ed Echo Color/Cut.
Usare `--output-dir artifacts/offline-issues-5-12` per conservarli nel progetto.
I file sono normalizzati allo stesso RMS dei campioni attivi (sopra l'1% del
picco originale), senza clipping. Le metriche originali restano nel JSON.
Questo prepara il confronto d'ascolto, non sostituisce il giudizio sul riddim.
