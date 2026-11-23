# Piano tecnico: MVP Flutter Windows

Risponde al commento del 23/11/2026 su Jira DEV-1. Fase 1 = app Flutter
Windows con le stesse funzioni del simulatore Android; Fase 2 = la stessa app
sostituisce quella Kotlin. Nessuna funzione oltre al pannello fisico (sei pot,
quattro tasti MX, encoder con push, display).

## Cosa c'è oggi

| Parte | File | Righe | In Flutter |
|---|---|---|---|
| DSP, controller, menu, display | `firmware/src/audio`, `control`, `display/PanelDisplay.cpp` | — | invariati, compilati da CMake |
| Stub Arduino/FreeRTOS, framebuffer LCD | `Android/app/src/main/cpp/stubs/` | ~190 | invariati (già usati anche da `tests/native/run.py`) |
| Adafruit GFX 1.12.6 | scaricata da CMake | — | stesso `FetchContent` |
| Ponte nativo | `Android/app/src/main/cpp/siren_bridge.cpp` | 359 | diviso in due (sotto) |
| Superficie JNI | `NativeSiren.kt` | 27 | diventa FFI Dart |
| Ciclo UI (loop) | `MainActivity.kt` | 89 | `Ticker` Flutter |
| Pannello, gesti multitouch | `PanelView.kt` | 643 | da riscrivere in Dart (`CustomPainter` + `Listener`) |
| Faccia del PCB | `PcbArt.kt` + `assets/pcb_front.json` | 228 | da riscrivere in Dart, stesso JSON |

Il codice della sirena non si tocca. Il lavoro vero è la grafica e i gesti
(~870 righe Kotlin da portare in Dart) e il ponte nativo.

## Ponte nativo: cosa si riusa e cosa cambia

`siren_bridge.cpp` oggi mescola tre cose:

1. **Nucleo** (riusabile così com'è): `Siren` (ControlStore, CommandParser,
   PanelDisplay, AudioEngine), persistenza di `PanelSettings` su file,
   quantizzazione ADC dei pot, `renderBlock()`, framebuffer RGB565.
2. **Uscita audio AAudio**: callback, apertura stream, `tuneLatency()`.
3. **JNI**: le funzioni `Java_com_capobuf_dubsiren_NativeSiren_*`.

Adattamento minimo:

- `siren_core.cpp` + `siren_core.h`: il punto 1 con un'API C semplice
  (`siren_init(path)`, `siren_set_pot`, `siren_set_button`,
  `siren_set_encoder_pressed`, `siren_rotate_encoder`,
  `siren_tick(uint32_t *rgba, int since)`, `siren_render(int16_t *out, frames)`,
  `siren_audio_status`). Spostato, non riscritto: la persistenza e la
  quantizzazione restano in un solo posto.
- `audio_aaudio.cpp`: il punto 2, spostato. L'app Kotlin passa da qui finché
  esiste; in Fase 2 lo usa l'app Flutter su Android.
- `audio_miniaudio.cpp` (nuovo, solo Windows): `ma_device` in playback,
  48 kHz, mono, `s16`, profilo low latency, il callback chiama
  `siren_render`. miniaudio è un singolo header, dominio pubblico o MIT-0,
  backend WASAPI.
- JNI ridotto a chiamate verso `siren_core`: l'app Kotlin continua a
  funzionare per tutta la Fase 1.

Thread invariati: il callback audio (WASAPI o AAudio) fa da `audioTask`, il
thread UI Flutter fa da `loop()`. Dart non entra mai nel callback audio.

## App Flutter

- Progetto creato con `flutter create --template=plugin_ffi` (o app +
  pacchetto FFI): CMake compila lo stesso nucleo per Windows e Android.
- Binding generati con `ffigen` da `siren_core.h`, non scritti a mano.
- Display: il nucleo converte RGB565 in RGBA8888 (oggi lo fa già verso ARGB),
  Dart crea l'immagine con `ui.decodeImageFromPixels` solo quando il contatore
  del frame cambia, e la disegna scalata con filtro.
- Ciclo: un `Ticker` a ogni vsync chiama `siren_tick`, come `Choreographer`.
- Gesti: `Listener` sui `PointerEvent` grezzi, un `Grab` per puntatore come in
  `PanelView.kt`; stesse costanti in mm dal PCB.
- Salvataggi: impostazioni pannello nel file binario di oggi (cartella da
  `path_provider`), posizioni dei pot con `shared_preferences`. Pacchetti
  ufficiali Flutter.
- Master fisso a fine corsa; il livello è il volume di sistema.

## Framework UI: shadcn e alternative

Per il pannello della Fase 1 nessuna libreria di componenti serve: la UI è il
disegno del PCB con misure in mm e i gesti dell'hardware, senza pulsanti,
menu o testi dell'app (il menu è sul display del firmware).

| Opzione | Cosa dà | Per il pannello |
|---|---|---|
| [shadcn_ui](https://pub.dev/packages/shadcn_ui) (MIT, 0.61.2, ~107k download) | port Flutter di shadcn/ui: pulsanti, form, dialog, tabelle, temi | no: nessuna manopola, encoder o display |
| [shad](https://pub.dev/packages/shad) (MIT, 1.0.2, Serverpod) | nato dallo stesso progetto di shadcn_ui, 40+ componenti | no, stesso motivo |
| [shadcn_flutter](https://pub.dev/packages/shadcn_flutter) (BSD-3, 0.0.55) | progetto separato, solo stile New York | no, stesso motivo |
| [flutter_dial_knob](https://pub.dev/packages/flutter_dial_knob) (MIT, 0.1.1) | manopola con trascinamento verticale o orizzontale e colori | no: niente multitouch indicato, niente corsa di 300° su 55 mm, niente presa a due dita |
| [flutter_audio_widgets](https://pub.dev/packages/flutter_audio_widgets) (MIT, 0.1.0) | manopole, VU, EQ in stile plugin audio | no: uploader non verificato, analisi pub.dev incompleta, gesti non documentati |
| Canvas di Flutter (`CustomPainter`, `Path`, `Paint`, shader, `Listener`) | le stesse primitive di `android.graphics` usate da `PanelView.kt` e `PcbArt.kt` | sì: il port è una traduzione riga per riga, non un disegno da zero |

shadcn/ui vero e proprio è React: per usarlo servirebbe uno stack web
(Tauri o Electron) al posto di Flutter, contro la decisione presa.

Una libreria in stile shadcn diventa utile con le schermate fuori scope
(connessione ESP32, aggiornamento firmware, editor dei preset). Si sceglie
allora; oggi `shadcn_ui` è la più usata. Aggiungerla adesso sarebbe una
dipendenza senza uso.

## Decisioni (23/11/2026)

1. **Input simultanei senza touch screen:** la rotella gira il pot o l'encoder
   sotto il cursore; i tasti 1-4 tenuti premuti sono i tasti MX e lo spazio è
   il push dell'encoder. Sono solo un altro modo di azionare i comandi del
   pannello, non funzioni nuove. Con un touch screen vale il multitouch come su
   Android.
2. **Progetto Flutter in `app/`.**
3. **Codice nativo spostato subito** in `app/native/`: stub, nucleo
   `siren_core` e uscita AAudio. L'app Kotlin, `tests/native/run.py` e la CI
   puntano lì già in Fase 1.

## Prerequisiti e rischi

- Sul PC mancano Flutter SDK e Visual Studio 2022 (o Build Tools) con il
  carico "Sviluppo di applicazioni desktop con C++", obbligatorio per
  `flutter build windows`. Le build con plugin chiedono la Modalità
  sviluppatore di Windows (link simbolici): va attivata dall'utente.
- **MSVC non è mai stato provato sul firmware**: oggi lo compilano GCC (ESP32),
  Clang (NDK) e Zig. Primo passo dell'MVP: compilare nucleo + Adafruit GFX con
  MSVC e far passare `tests/native/run.py` con `CXX=cl`.
- OneDrive: il repository è sotto `Documents`; la cartella `build/` di Flutter
  potrebbe avere lo stesso blocco visto con Gradle. Da verificare alla prima
  build.
- Latenza Windows: WASAPI condiviso aggiunge in genere più ritardo di AAudio.
  Va misurata e riportata come oggi fa il logcat (rate, buffer, tempo di
  render); la modalità esclusiva è un'opzione di miniaudio se serve.
- Fase 2: l'audio Android resta AAudio con la logica di oggi (underrun →
  +1 burst), così le prestazioni non peggiorano. miniaudio anche su Android si
  valuta solo con misure a confronto.

## Ordine dei lavori (MVP Windows)

1. Toolchain: Flutter + VS Build Tools, `flutter doctor` pulito.
2. MSVC: nucleo e test nativi compilati con `cl`.
3. Stub e nucleo spostati in `app/native/`, `siren_bridge.cpp` diviso in nucleo + AAudio + JNI; APK ricompilata
   e provata sul dispositivo (nessun cambiamento atteso).
4. Progetto Flutter, CMake, `ffigen`, audio miniaudio: suono dalla prima
   finestra con i pot a valori fissi.
5. Display dal framebuffer.
6. Pannello: PCB, manopole, encoder, tasti, gesti.
7. Confronto con l'APK: stessi comandi, stesso suono, latenza misurata.

## Issue proposte

- **MVP Flutter Windows**: punti 1-7 sopra.
- **Migrazione Android a Flutter**: stessa app su Android con AAudio, touch e
  multitouch, vibrazione negli scatti dell'encoder, tasti volume sullo stream
  media, firma release e workflow `Android APK` portati sulla build Flutter;
  rimozione dell'app Kotlin.

Fuori scope (ESP32 via USB, aggiornamento firmware, editor preset, synth,
MIDI): nessuna predisposizione ora.
