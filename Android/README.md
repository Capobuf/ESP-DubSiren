# DubSiren Android

Simulatore Android della sirena in un'unica APK. Suono, logica e display
vengono dagli stessi sorgenti C++ del firmware, compilati con l'NDK senza
modifiche:

| Parte | Sorgente | Su Android |
|---|---|---|
| DSP | `firmware/src/audio/*.cpp` | blocchi da 480 campioni mono a 48 kHz, uscita AAudio |
| Controller, menu, mappature, Pickup | `firmware/src/control/*.cpp` | input fisici dal pannello touch |
| Display | `firmware/src/display/PanelDisplay.cpp` e Adafruit GFX 1.12.6 | framebuffer 320x172 RGB565 al posto dell'ST7789 |

Nessuna logica della sirena è riscritta in Kotlin. L'app offre solo i gesti
del pannello fisico: sei pot frontali, quattro tasti momentanei, encoder a
scatti e push. Tutto il resto (voci, mappature, impostazioni) passa
dall'encoder e dal suo menu, come sull'hardware. Il Master posteriore non compare: resta
al massimo e il livello si regola con i tasti volume del dispositivo.

## Build

Requisiti: Android SDK (platform 36) e JDK 17 o successivo; va bene la JBR di
Android Studio. Al primo avvio Gradle installa da solo NDK 27.0.12077973 e
CMake 3.22.1, se la licenza SDK è già accettata.

```powershell
cd Android
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
.\gradlew.bat assembleRelease
```

Gradle compila fuori dal repository, in `%USERPROFILE%\.dubsiren-android`:
OneDrive tiene aperti i file durante la sincronizzazione e fa fallire il
packaging. L'APK si installa direttamente:

```powershell
adb install -r "$env:USERPROFILE\.dubsiren-android\app\outputs\apk\release\app-release.apk"
```

Si può anche aprire la cartella `Android` in Android Studio. La build release
è ottimizzata (`-O2`); conviene usarla anche per provare l'audio.

## Firma e APK pronta

La build release è firmata con la chiave release del progetto se esiste
`%USERPROFILE%\.dubsiren-signing\signing.properties` (`storeFile`,
`storePassword`, `keyAlias`, `keyPassword`), altrimenti con la chiave debug
locale. Chiave e password stanno solo lì e nei secret GitHub
`ANDROID_KEYSTORE_BASE64` e `ANDROID_KEYSTORE_PASSWORD` (alias `dubsiren`):
vanno conservate, perché senza la stessa chiave un'APK non può aggiornare
quella installata. Android rifiuta anche un aggiornamento tra firme diverse:
passando dalla chiave debug a quella release bisogna disinstallare l'app una
volta, perdendo impostazioni e posizioni dei pot salvate.

Il workflow [Android APK](../.github/workflows/android.yml) compila l'APK a
ogni push che tocca `Android/` o i sorgenti firmware condivisi, su qualunque
branch, e negli avvii manuali (Actions → Android APK → Run workflow) la
pubblica come GitHub Release `apk-<numero>`, con il file
`DubSiren-<numero>-<commit>.apk` da scaricare e aprire sul dispositivo. Restano
solo le due release APK più recenti.

## Pannello

La geometria viene da `hardware/kicad/ESP-DubSiren.kicad_pcb` (8/11/2026). La
faccia frontale è esportata in `app/src/main/assets/pcb_front.json`: contorno
con asole, piano GND e piste F.Cu, vie coperte dalla maschera, pad ENIG e
serigrafia. Dopo ogni modifica del PCB va rigenerata:

```powershell
python Android\tools\export_pcb_front.py
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

| Gesto | Equivalente fisico |
|---|---|
| Trascinamento verticale su un pot | rotazione; 55 mm di trascinamento = corsa di 300° (PTV111) |
| Due dita sulla stessa manopola, ruotate | la manopola gira dello stesso angolo della linea tra le dita, come afferrandola; il secondo dito la prende anche fino a 8 mm fuori dal bordo |
| Rotazione attorno all'encoder | scatti da 20° (PEC11R-4120F-S0018: 18 scatti/giro), con vibrazione a ogni scatto |
| Tocco al centro dell'encoder | push |
| Tocco su un tasto | MX2A premuto finché il dito resta giù |
| Doppio tocco sul display | scala reale o ingrandito; ingrandito in automatico se il pannello è disegnato sotto 1,2 volte la grandezza reale |

Il multitouch permette le combinazioni dell'hardware: tasto tenuto più
rotazione dell'encoder (Live Pitch), push più tasto, più tasti insieme, più
pot insieme (un dito ciascuno).

Pot e Master passano dallo stesso percorso di `PanelInputs.cpp`: la posizione
diventa millivolt interi sui massimi nominali del partitore (2672,1 mV per i pot,
2244,9 mV per il Master), con la soglia di variazione di 5 mV. Il Master parte da
zero e all'avvio viene portato a fine corsa. Le posizioni delle manopole restano salvate, come
su un pot fisico spento. Le impostazioni del pannello (`PanelSettings`) si
salvano come blob binario identico a quello in NVS, 2 s dopo l'ultima modifica.
Un file salvato con un altro schema viene scartato all'avvio: valgono le
impostazioni di fabbrica.

L'app non aggiunge scritte: come sul pannello fisico, che non ha serigrafia,
la funzione di ogni comando si legge sul display.

La libreria Adafruit GFX è la stessa versione fissata in
`firmware/platformio.ini`: CMake la scarica (URL e SHA256 in
`app/src/main/cpp/CMakeLists.txt`) e lo stub `Adafruit_ST7789` scrive solo i
pixel nel framebuffer. Font, primitive e canvas sono quelli della libreria.

## Limiti

- ADC ideale: niente rumore, offset, tolleranze o diafonia del mux. Il
  debounce non serve, perché il touch non rimbalza.
- Verso dell'encoder: orario = NEXT. Sull'hardware resta da verificare
  (`docs/standalone.md`).
- La latenza dipende dal telefono. AAudio parte in low latency con due burst di
  buffer e aggiunge un burst a ogni underrun. Ogni 5 s il logcat (tag
  `DubSiren`) riporta sample rate, buffer, xrun e tempo di render del blocco. Se il dispositivo non
  apre lo stream a 48 kHz, compare `(!48k)`: il DSP resta a 48 kHz e
  l'intonazione cambierebbe.
- Diametro della manopola SAK-023, colore del PCB del modulo display, altezze
  delle parti e keycap (ancora senza MPN) sono solo rappresentazioni visive.
- USB/PC, I2S e STATUS non esistono nell'app: l'uscita è l'audio del telefono.
