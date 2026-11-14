# Istruzioni per gli agenti

- Non inventare, non assumere e non dare niente per scontato. Non sovra-ingegnerizzare; preferire plugin e soluzioni native al codice personalizzato.
- Lasciare i test onerosi alla fine; se sono molto lunghi, fornire all'utente il comando per eseguirli con arresto al primo errore.
- Quando l'utente conferma che il lavoro è OK, chiudere le issue corrispondenti e pulire i branch già uniti, verificando prima che siano effettivamente integrati e preservando i branch esplicitamente destinati ad archivio o usati da altri worktree.

## App Android (`Android/`)

- **Cosa fa:** simula la sirena in una sola APK, sia nel suono sia nell'aspetto, ed è il banco di prova del firmware prima dell'hardware. Non deve mai offrire funzioni che il pannello fisico non ha: sei pot, quattro tasti MX, encoder con push, display. Il Master posteriore è fisso al massimo; il volume si regola con i tasti del dispositivo.
- **Come funziona:** l'NDK compila senza modifiche `firmware/src/audio/*.cpp`, `firmware/src/control/*.cpp` e `firmware/src/display/PanelDisplay.cpp`. In Kotlin non c'è logica della sirena.
- **Strato nativo** (`app/src/main/cpp/`):
  - `stubs/` sostituisce Arduino, FreeRTOS e l'ST7789: il display finisce in un framebuffer 320x172, con il font 5x7 di Adafruit GFX.
  - `siren_bridge.cpp` fa da `main.cpp`: il thread UI fa da loop() e passa gli input fisici al controller; il callback AAudio fa da `audioTask` e produce blocchi da 480 campioni mono a 48 kHz.
  - Persistenza di `PanelSettings` e quantizzazione ADC dei pot sono ricopiate a mano da `main.cpp` e `PanelInputs.cpp`: se cambiano lì, vanno aggiornate qui.
- **Grafica** (`app/src/main/java/com/capobuf/dubsiren/`):
  - `PcbArt.kt` disegna la faccia frontale del PCB da `app/src/main/assets/pcb_front.json`, generato da `python Android/tools/export_pcb_front.py`. Va rilanciato dopo ogni modifica di `hardware/kicad/ESP-DubSiren.kicad_pcb`.
  - `PanelView.kt` disegna manopole, encoder, tasti e display e gestisce il multitouch. Le posizioni dei comandi sono costanti in mm prese dal PCB: i tasti sono al centro dello switch, non sull'origine del footprint MX.
- **Cosa segue il firmware:** le modifiche a DSP, controller e `PanelDisplay.cpp` arrivano nell'app ricompilando.
- **Cosa la rompe:** una nuova funzione GFX usata dal display va aggiunta in `stubs/Adafruit_ST7789.*`; un nuovo file in `firmware/src/display/` va aggiunto in `app/src/main/cpp/CMakeLists.txt`.
- **Build:** dalla cartella `Android`, con `JAVA_HOME` sulla JBR di Android Studio o su un JDK 17+, eseguire `.\gradlew.bat assembleRelease`. Gradle scrive in `%USERPROFILE%\.dubsiren-android`, fuori da OneDrive, che bloccava il packaging. Se un build fallisce non installare: l'APK in uscita sarebbe quella precedente.
- **Installazione sui dispositivi:**
  - Elencare i dispositivi con `adb devices -l`.
  - Installare con `adb -s <seriale> install -r "$env:USERPROFILE\.dubsiren-android\app\outputs\apk\release\app-release.apk"`.
  - Avviare con `adb -s <seriale> shell am start -S -n com.capobuf.dubsiren/.MainActivity`.
  - L'APK è firmata con la chiave debug locale.
- **Verifica:**
  - Testare sul dispositivo reale indicato dall'utente, non sull'emulatore.
  - Se lo schermo è bloccato, chiedere all'utente di sbloccarlo; non tentare di sbloccarlo.
  - Gli screenshot si catturano con `adb -s <seriale> exec-out screencap -p`.
  - Lo stato dell'audio si legge nella riga in basso dell'app e nei log con `adb logcat -s DubSiren`.
- **Riferimento:** dettagli e limiti in [Android/README.md](Android/README.md).
