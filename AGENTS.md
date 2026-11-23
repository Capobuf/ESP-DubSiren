# Istruzioni per gli agenti

- Non inventare, non assumere e non dare niente per scontato. Non sovra-ingegnerizzare; preferire plugin e soluzioni native al codice personalizzato.
- Lasciare i test onerosi alla fine; se sono molto lunghi, fornire all'utente il comando per eseguirli con arresto al primo errore.
- Quando l'utente conferma che il lavoro è OK, chiudere le issue corrispondenti e pulire i branch già uniti, verificando prima che siano effettivamente integrati e preservando i branch esplicitamente destinati ad archivio o usati da altri worktree.

## App simulatore (`app/`, Flutter, Windows e Android)

- **Cosa fa:** è il simulatore della sirena e dell'ESP32 (il Virtual ESP desktop e l'app Kotlin non esistono più): simula la sirena in un'unica app Flutter per Windows e Android, sia nel suono sia nell'aspetto, ed è il banco di prova del firmware prima dell'hardware. Non deve mai offrire funzioni che il pannello fisico non ha: sei pot, quattro tasti MX, encoder con push, display. Il Master posteriore è fisso al massimo; il volume si regola dal sistema. Su Windows tastiera e rotella azionano gli stessi comandi (tasti 1-4, spazio, rotella su pot ed encoder), non funzioni nuove.
- **Come funziona:** CMake compila senza modifiche `firmware/src/audio/*.cpp`, `firmware/src/control/*.cpp` e `firmware/src/display/PanelDisplay.cpp` in `siren_core.dll` / `libsiren_core.so`, caricata da `lib/siren_native.dart` con `dart:ffi`. In Dart non c'è logica della sirena.
- **Strato nativo** (`app/native/`):
  - `stubs/` sostituisce Arduino, FreeRTOS e il driver del pannello (`PanelLcd.cpp`, esp_lcd): il display usa la vera Adafruit GFX 1.12.6, scaricata da CMake, e finisce in un framebuffer 320x172. Gli stessi stub servono ai test nativi (`tests/native/run.py`).
  - `siren_core.cpp` (API C in `siren_core.h`) fa da `main.cpp`: un timer Dart fa da loop() e passa gli input fisici al controller; il callback audio fa da `audioTask` e produce blocchi da 480 campioni mono a 48 kHz. Uscita audio: `audio_aaudio.cpp` (Android), `audio_miniaudio.cpp` (Windows).
  - Persistenza di `PanelSettings` e quantizzazione ADC dei pot in `siren_core.cpp` sono ricopiate a mano da `main.cpp` e `PanelInputs.cpp`: se cambiano lì, vanno aggiornate qui.
- **Grafica** (`app/lib/`):
  - `pcb_art.dart` disegna la faccia frontale del PCB da `app/assets/pcb_front.json`, generato da `python app/tools/export_pcb_front.py`. Va rilanciato dopo ogni modifica di `hardware/kicad/ESP-DubSiren.kicad_pcb`.
  - `panel.dart` disegna manopole, encoder, tasti e display e gestisce multitouch, tastiera e rotella. Nessuna scritta oltre alla serigrafia del PCB: le funzioni dei comandi le mostra il display. Le posizioni dei comandi sono costanti in mm prese dal PCB: i tasti sono al centro dello switch, non sull'origine del footprint MX.
- **Cosa segue il firmware:** le modifiche a DSP, controller e `PanelDisplay.cpp` arrivano nell'app ricompilando. I font del display (Teko SemiBold, OFL) sono in `firmware/src/display/fonts/` e si rigenerano con `python firmware/tools/make_display_fonts.py`.
- **Cosa la rompe:** se cambia la versione di Adafruit GFX in `firmware/platformio.ini`, vanno aggiornati URL e SHA256 in `app/native/CMakeLists.txt`; un nuovo simbolo Arduino usato dal firmware va aggiunto negli stub.
- **Build:** Flutter stable 3.50.1 in `C:\src\flutter` (la stessa versione del workflow `Android APK`). Dalla cartella `app`: `flutter build windows --release` (serve VS 2022 Build Tools con il carico C++) e `flutter build apk --release` (Android SDK e JDK 17; NDK e CMake li installa Gradle). Se un build fallisce non installare: l'APK in uscita sarebbe quella precedente.
- **Installazione sui dispositivi Android:**
  - Elencare i dispositivi con `adb devices -l`.
  - Installare con `adb -s <seriale> install -r app\build\app\outputs\flutter-apk\app-release.apk`.
  - Avviare con `adb -s <seriale> shell am start -S -n com.capobuf.dubsiren/.MainActivity`.
  - L'APK è firmata con la chiave release in `%USERPROFILE%\.dubsiren-signing` (la stessa dei secret GitHub usati dal workflow `Android APK`); senza quel file, con la chiave debug locale. Non stampare né committare chiave e password. Il `versionCode` viene da `version` in `app/pubspec.yaml`: non abbassarlo, Android rifiuta il downgrade.
  - Il workflow `Android APK` pubblica l'APK installabile come GitHub Release a ogni push che tocca `app/` o il firmware condiviso e negli avvii manuali, tenendo le ultime due; le pull request compilano soltanto.
- **Verifica:**
  - Testare sul dispositivo reale indicato dall'utente, non sull'emulatore.
  - Se lo schermo è bloccato, chiedere all'utente di sbloccarlo; non tentare di sbloccarlo.
  - Gli screenshot Android si catturano con `adb -s <seriale> exec-out screencap -p`.
  - Lo stato dell'audio si legge con `adb logcat -s DubSiren flutter` su Android e nella console di `flutter run -d windows --release` su Windows (una riga ogni 5 s).
- **Riferimento:** dettagli e limiti in [app/README.md](app/README.md).
