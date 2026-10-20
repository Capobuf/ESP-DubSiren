# Tap tempo e sincronizzazione

Il controller C++ condiviso da ESP e VirtualESP gestisce la issue #13.
Nel menu PERFORMANCE, **TAP TEMPO** acquisisce un tap a ogni click completo.
La stessa azione si può assegnare a qualsiasi pulsante in SETUP → BUTTONS:
acquisisce solo la pressione, non il rilascio né la ripetizione di uno stato
già premuto. Le scorciatoie Extended con encoder premuto mantengono precedenza.

Due tap validi definiscono il tempo; i successivi usano la media degli ultimi
quattro intervalli. Sono ammessi intervalli da 200 a 2000 ms (300–30 BPM).
Un tap prima di 200 ms mostra `TOO FAST - IGNORED` e non sposta il riferimento.
Dopo più di 2000 ms parte una nuova sequenza con `TAP AGAIN`: il tempo audio
precedente resta attivo fino al secondo tap valido. Nessuna quantizzazione
automatica a BPM interi. MOD ed ECHO permettono anche di impostare BPM
manualmente, da 30 a 300; questa operazione azzera la sequenza di tap.
Il valore iniziale proposto per l'editing è 120 BPM, ma non è acquisito finché
non viene regolato, confermato nel menu o ricevuto tramite due tap.

## FREE e SYNC

MOD ed ECHO hanno ciascuno CLOCK = FREE/SYNC e condividono il BPM.
In SYNC compare DIVISION; i pot assegnati a RATE/ECHO TIME selezionano le
stesse quattro zone, usando Pickup e l'isteresi già presenti nel controller.
La sequenza delle zone è 1/4, 1/8, 1/8D, 1/8T.

| Divisione | Durata in quarti | Echo a 120 BPM | LFO a 120 BPM |
|---|---:|---:|---:|
| 1/4 | 1 | 500 ms | 2 Hz |
| 1/8 | 1/2 | 250 ms | 4 Hz |
| 1/8D, ottavo puntato | 3/4 | 375 ms | 2,6667 Hz |
| 1/8T, terzina di ottavi | 1/3 | 166,6667 ms | 6 Hz |

Una divisione LFO indica un ciclo completo, non una mezza oscillazione.
Il limite Echo resta 50–1000 ms. Una divisione non rappresentabile, oppure
SYNC senza tempo acquisito, mostra **N/A HOLD** con l'ultimo valore utilizzabile:
il valore audio viene mantenuto, senza clamp e senza dichiararlo sincronizzato.
Per esempio a 30 BPM il quarto richiede 2000 ms ed è indisponibile, l'ottavo
richiede 1000 ms ed è disponibile. Se non esiste ancora un valore Sync valido,
viene mantenuto quello Free presente all'attivazione.
La selezione diventa disponibile automaticamente quando il BPM lo consente.

Il ritorno FREE conserva il valore temporale corrente e riattiva Pickup
anche se il comportamento generale è Jump. Dopo l'aggancio torna la mappatura
configurata del pot: continua, oppure le zone Classic già configurate.
Le selezioni Manual di Classic tornano operative in Free.
Echo Free conserva il pitch bend manuale del delay.

## Fase e voci

Il LFO ha fase libera: tap, cambio BPM e cambio divisione non lo riallineano,
non riavviano il carrier e non cancellano l'echo. In SYNC il retrigger LFO al
trigger è sospeso; il Dynamic Sweep continua a cambiare pitch ma non altera
la velocità LFO sincronizzata. Gli smoothing di frequenza/delay esistenti
restano attivi: ms/Hz mostrati sono i valori obiettivo, non una misura istantanea.

Classic SYNC usa la modulazione periodica anche se la selezione Free è Manual.
In Extended SYNC non abilita una modulazione spenta e non sostituisce la forma
Manual: `N/A VOICE` indica che la voce/configurazione non usa quel clock.
Lo stesso vale per Gunshot, Syndrum, Lickshot Shot/Horn. Lickshot Roll usa
la divisione per la cadenza dei colpi; il primo colpo conserva la propria
articolazione al trigger.

Home, overlay, menu e STATUS mostrano BPM/divisioni e valori effettivi tramite
il controller condiviso. Gli errori del dispositivo/persistenza mantengono
priorità sul footer firmware. Il renderer PC non calcola tempo o divisioni.

## Protocollo e persistenza

Comandi additivi, framing versione 1:

```text
TAP_TEMPO
TEMPO BPM 120
TEMPO ECHO SYNC
TEMPO LFO FREE
TEMPO ECHO_DIV 1/8D
TEMPO LFO_DIV 1/8T
PANEL BUTTON CLASSIC 4 ACTION TAP_TEMPO
```

`TEMPO ECHO|LFO` accetta solo FREE/SYNC. Divisioni ammesse: 1/4, 1/8, 1/8D,
1/8T. Numeri non finiti, BPM fuori intervallo e token aggiuntivi sono rifiutati
senza mutazione. In Sync i comandi diretti DELAY_MS oppure LFO_RATE_HZ,
RATE_POSITION e CLASSIC_MOD relativi al controllo sincronizzato sono rifiutati:
passare esplicitamente a FREE per modificarli.

`panel.display.tempo` espone `bpm`, `known`, `echoSync`, `lfoSync`,
`echoDivision`, `lfoDivision`, `echoAvailable`, `lfoAvailable`, `lfoActive`,
`echoMs`, `lfoHz` e i testi `status`, `echoText`, `lfoText`. Available descrive
la validità temporale, Active l'uso del clock da parte della voce/configurazione.
I campi display `delayMs` e `rateHz` riportano gli stessi obiettivi effettivi.

L'assegnazione TAP TEMPO usa Preferences/NVS e panel.json schema 2 esistenti,
senza cambiare il layout binario. BPM, Sync, divisioni e sequenza dei tap sono
stato runtime globale: non vengono scritti a ogni tap né salvati nelle patch.
Al boot partono FREE con tempo non acquisito. Cambio profilo e richiamo patch
non cambiano il tempo. RESET DEFAULTS lo azzera insieme alle assegnazioni.

## Verifiche

I test nativi coprono intervalli deterministici, media, timeout, tap ignorati,
rollover del timer, limiti BPM, conversioni, N/A, isteresi, Pickup e PCM
confrontato con un riferimento Free equivalente durante cambi tempo/divisione
e coda echo. VirtualESP copre STATUS, menu, round-trip dell'assegnazione TAP e
reset al riavvio; il canvas Tk verifica gli ingombri del testo.

La prova musicale resta da eseguire: tap irregolari sul riddim, cambi rapidi
di tempo, ascolto del delay durante i cambi, leggibilità sul TFT e risposta dei
pulsanti reali. I test software non sostituiscono queste verifiche.
