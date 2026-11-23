# DubSiren (Flutter)

Simulatore della sirena per Windows (Fase 1, issue #67), che in Fase 2
sostituirà l'app Android Kotlin (issue #68). Suono, logica e display vengono
dagli stessi sorgenti C++ del firmware, compilati senza modifiche. In Dart non
c'è logica della sirena. Il piano è in `docs/flutter-windows-mvp.md`.

| Parte | Dove |
|---|---|
| Nucleo C (persistenza, pot come l'ADC, render, framebuffer) | `native/siren_core.{h,cpp}` |
| Stub Arduino/FreeRTOS, framebuffer al posto dell'ST7789 | `native/stubs/` |
| Uscita audio | `native/audio_miniaudio.cpp` (Windows, miniaudio 0.11.26), `native/audio_aaudio.cpp` (Android) |
| Binding `dart:ffi` | `lib/siren_native.dart` |
| Pannello, gesti, tastiera e rotella | `lib/panel.dart` (port di `PanelView.kt`) |
| Faccia del PCB | `lib/pcb_art.dart` (port di `PcbArt.kt`), `assets/pcb_front.json` |

`native/` è usata anche dall'app Kotlin (`Android/app/src/main/cpp/CMakeLists.txt`)
e da `tests/native/run.py`.

## Build Windows

Requisiti: Flutter stable e Visual Studio 2022 o Build Tools con il carico
"Sviluppo di applicazioni desktop con C++".

```powershell
cd app
flutter build windows --release
```

L'eseguibile è `build\windows\x64\runner\Release\dubsiren.exe`, con accanto
`siren_core.dll`. Con `flutter run -d windows --release` la console riporta
ogni 5 s lo stato audio: frequenza, buffer WASAPI, tempo di render del blocco.
miniaudio non conta gli underrun: compare `xrun -1`.

## Comandi

Gli stessi gesti dell'app Android (trascinamento verticale sui pot, rotazione
attorno all'encoder, tocco su tasti e push, multitouch su touch screen). Con il
mouse, che è un solo puntatore, gli stessi comandi rispondono anche a:

| Input | Equivalente fisico |
|---|---|
| Tasti 1-4 tenuti premuti | tasti MX SW1-SW4 |
| Spazio tenuto premuto | push dell'encoder |
| Rotella su un pot | rotazione, 10° per scatto, su = orario |
| Rotella sull'encoder | uno scatto per scatto, su = orario |

Così le combinazioni dell'hardware (tasto tenuto + encoder, push + tasto) si
fanno anche senza touch screen. Il Master posteriore resta al massimo: il
livello è il volume di sistema.

## Limiti

- La scelta automatica del display ingrandito usa i pixel logici nominali
  (96 dpi su Windows), non i dpi misurati dello schermo.
- Il percorso del file delle impostazioni passa a `fopen` nella code page ANSI:
  va bene finché il profilo utente ha un percorso ASCII.
