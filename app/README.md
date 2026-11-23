# DubSiren (Flutter)

Simulatore della sirena per Windows e Android, in un'unica app Flutter. Suono,
logica e display vengono dagli stessi sorgenti C++ del firmware, compilati
senza modifiche. In Dart non c'è logica della sirena. L'app offre solo i
comandi del pannello fisico: sei pot frontali, quattro tasti momentanei,
encoder a scatti e push. Tutto il resto (voci, mappature, impostazioni) passa
dall'encoder e dal suo menu, come sull'hardware. Il Master posteriore non
compare: resta al massimo, e il livello è il volume del sistema.

| Parte | Dove |
|---|---|
| Nucleo C (persistenza, pot come l'ADC, render, framebuffer) | `native/siren_core.{h,cpp}` |
| DSP, controller, display | `firmware/src/audio`, `control`, `display/PanelDisplay.cpp`, compilati da `native/CMakeLists.txt` |
| Stub Arduino/FreeRTOS, framebuffer al posto dell'ST7789 | `native/stubs/`, usati anche da `tests/native/run.py` |
| Uscita audio | `native/audio_miniaudio.cpp` (Windows, miniaudio 0.11.26), `native/audio_aaudio.cpp` (Android) |
| Binding `dart:ffi` | `lib/siren_native.dart` |
| Pannello, gesti, tastiera e rotella | `lib/panel.dart` |
| Faccia del PCB | `lib/pcb_art.dart`, `assets/pcb_front.json` |

## Build

Requisiti: Flutter stable 3.50.1 (la CI usa la stessa versione). Per Windows,
Visual Studio 2022 o Build Tools con il carico "Sviluppo di applicazioni
desktop con C++". Per Android, Android SDK e JDK 17: NDK e CMake li installa
Gradle al primo build.

```powershell
cd app
flutter build windows --release
flutter build apk --release
```

- Windows: `build\windows\x64\runner\Release\dubsiren.exe`, con accanto
  `siren_core.dll`.
- Android: `build\app\outputs\flutter-apk\app-release.apk`, da installare con
  `adb -s <seriale> install -r build\app\outputs\flutter-apk\app-release.apk`.

Ogni 5 s l'app scrive lo stato audio: frequenza, buffer, underrun, tempo di
render del blocco. Su Windows compare nella console di
`flutter run -d windows --release`. Su Android si legge con
`adb logcat -s DubSiren flutter`: il tag `DubSiren` riporta anche l'apertura
dello stream e ogni underrun. miniaudio non conta gli underrun: su Windows
compare `xrun -1`. Su Android, AAudio parte in low latency con due burst di
buffer e aggiunge un burst a ogni underrun. Se il dispositivo non apre lo
stream a 48 kHz, compare `(!48k)`: il DSP resta a 48 kHz e l'intonazione
cambierebbe.

Gli underrun dei primi 200 ms dopo l'avvio dello stream vengono contati ma non
allungano il buffer (log `while settling, buffer kept`): sullo Xiaomi Pad 5
l'app ne ha uno in parte degli avvii (da 3 su 10 a 6 su 8 nelle misure), a
circa 25 ms dall'avvio e con la sirena ancora muta, e senza questa regola il buffer restava a 576 frame invece
di 384 per tutta la sessione. Ritardare l'apertura dell'audio di 1 s li riduceva
(3 avvii su 18) senza eliminarli. Con la regola: buffer 384 in 10 avvii su 10,
underrun ignorato in 3. Per questo lo stato può riportare `xrun 1` con
`buf 384`.

## Firma e APK pronta

La build release Android è firmata con la chiave release del progetto se
esiste `%USERPROFILE%\.dubsiren-signing\signing.properties` (`storeFile`,
`storePassword`, `keyAlias`, `keyPassword`), altrimenti con la chiave debug
locale. Chiave e password stanno solo lì e nei secret GitHub
`ANDROID_KEYSTORE_BASE64` e `ANDROID_KEYSTORE_PASSWORD` (alias `dubsiren`):
vanno conservate, perché senza la stessa chiave un'APK non può aggiornare
quella installata. Android rifiuta anche un aggiornamento tra firme diverse:
passando dalla chiave debug a quella release bisogna disinstallare l'app una
volta, perdendo le impostazioni salvate.

L'`applicationId` è lo stesso della vecchia app Kotlin
(`com.capobuf.dubsiren`), con `versionCode` più alto: l'APK si installa sopra
di essa. Le impostazioni del pannello restano, perché il file
`panel_settings.bin` sta nella stessa cartella (`filesDir`). Le posizioni dei
pot e la scelta del display ingrandito ripartono dai valori iniziali, una
volta.

Il workflow [Android APK](../.github/workflows/android.yml) compila l'APK a
ogni push che tocca `app/` o i sorgenti firmware condivisi, su qualunque
branch, e negli avvii manuali (Actions → Android APK → Run workflow) la
pubblica come GitHub Release `apk-<numero>`, con il file
`DubSiren-<numero>-<commit>.apk` da scaricare e aprire sul dispositivo. Restano
solo le due release APK più recenti.

## Pannello

La geometria viene da `hardware/kicad/ESP-DubSiren.kicad_pcb` (8/11/2026). La
faccia frontale è esportata in `assets/pcb_front.json`: contorno con asole,
piano GND e piste F.Cu, vie coperte dalla maschera, pad ENIG e serigrafia.
Dopo ogni modifica del PCB va rigenerata:

```powershell
python app\tools\export_pcb_front.py
```

RV1-RV6 e SW7 sono sull'asse del footprint. I tasti SW1-SW4 sono al centro
dello switch (foro da 4 mm: x 10/32/54/76, y 100). Le manopole sono le Davies
1100 (Ø 19,84 mm). Il display U5 usa la geometria Eagle di Adafruit contenuta
nel footprint: vetro, area attiva 32,35x17,4 mm con angoli R3, fori del modulo.
La luce viene dall'alto a sinistra. Le parti alte (manopole, keycap, modulo
display) mostrano il lato rivolto verso chi guarda.

Il display imita il vetro IPS del 5393 (circa 10 pixel per mm): il
framebuffer è scalato con filtro, non a quadretti, con il nero appena
sollevato dalla retroilluminazione e il riflesso del vetro. La griglia tra i
pixel compare solo quando un pixel copre almeno 4 punti dello schermo, cioè
nella vista ingrandita. Sono approssimazioni visive, non misure del pannello;
i pixel restano quelli del firmware.

L'app non aggiunge scritte: come sul pannello fisico, che non ha serigrafia,
la funzione di ogni comando si legge sul display.

## Comandi

| Gesto | Equivalente fisico |
|---|---|
| Trascinamento verticale su un pot | rotazione; 55 mm di trascinamento = corsa di 300° (PTV111) |
| Due dita sulla stessa manopola, ruotate | la manopola gira dello stesso angolo della linea tra le dita, come afferrandola; il secondo dito la prende anche fino a 8 mm fuori dal bordo |
| Rotazione attorno all'encoder | scatti da 20° (PEC11R-4120F-S0018: 18 scatti/giro), con vibrazione a ogni scatto su Android |
| Tocco al centro dell'encoder | push |
| Tocco su un tasto | MX2A premuto finché il dito resta giù |
| Doppio tocco sul display | scala reale o ingrandito; ingrandito in automatico se il pannello è disegnato sotto 1,2 volte la grandezza reale |

Il multitouch permette le combinazioni dell'hardware: tasto tenuto più
rotazione dell'encoder (leva su/giù), push più tasto, più tasti insieme, più
pot insieme (un dito ciascuno). Con il mouse, che è un solo puntatore, gli
stessi comandi rispondono anche a:

| Input | Equivalente fisico |
|---|---|
| Tasti 1-4 tenuti premuti | tasti MX SW1-SW4 |
| Spazio tenuto premuto | push dell'encoder |
| Rotella su un pot | rotazione, 10° per scatto, su = orario |
| Rotella sull'encoder | uno scatto per scatto, su = orario |

Pot e Master passano dallo stesso percorso di `PanelInputs.cpp`: la posizione
diventa millivolt interi sui massimi nominali del partitore (2672,1 mV per i
pot, 2244,9 mV per il Master), con la soglia di variazione di 8 mV. Il Master
parte da zero e all'avvio viene portato a fine corsa. Le posizioni delle
manopole restano salvate, come su un pot fisico spento. Le impostazioni del
pannello (`PanelSettings`) si salvano come blob binario identico a quello in
NVS, 2 s dopo l'ultima modifica e alla chiusura. Un file salvato con un altro
schema viene scartato all'avvio: valgono le impostazioni di fabbrica.

## Limiti

- ADC ideale: niente rumore, offset, tolleranze o diafonia del mux. Il
  debounce non serve, perché il touch non rimbalza.
- Verso dell'encoder: orario = NEXT. Sull'hardware resta da verificare
  (`docs/standalone.md`).
- La scelta automatica del display ingrandito usa i pixel logici nominali
  (160 dpi Android, 96 dpi Windows), non i dpi misurati dello schermo.
- Su Windows il percorso del file delle impostazioni passa a `fopen` nella
  code page ANSI: va bene finché il profilo utente ha un percorso ASCII.
- Diametro della manopola SAK-023, colore del PCB del modulo display, altezze
  delle parti e keycap (ancora senza MPN) sono solo rappresentazioni visive.
- USB/PC, I2S e STATUS non esistono nell'app: l'uscita è l'audio del sistema.
