# Physical panel UI

## Goal

The desktop application represents the playable control surface. It sends
actions and displays controller state; the firmware/VirtualESP C++ controller
owns pad assignments, priority, combinations, and the sound configurations.
The PC still contains no oscillator, envelope, filter, or delay implementation.

## Main panel

| Physical control | Function | DSP/protocol mapping |
|---|---|---|
| POT1 | TUNE | `SET TUNE_HZ` |
| POT2 | RATE | `SET LFO_RATE_HZ` |
| POT3 | ECHO LEVEL | `SET ECHO_LEVEL` |
| POT4 | ECHO TIME | `SET DELAY_MS` |
| POT5 | ECHO FEEDBACK | `SET FEEDBACK` |
| POT6 | ECHO TONE | macro over `SET HPF_HZ` / `SET LPF_HZ` |
| BTN1–BTN4 | assigned sound pads | `PAD <1..4> 0|1` |
| Encoder rotation on Home | DEPTH | `ENCODER_ROTATE PREV|NEXT` |
| Encoder press | modifier, or open menu on release | `ENCODER_SWITCH 0|1` |
| Rear MASTER | final gain | `SET MASTER` |

The initial pad bank is defined once in `PerformanceController`:

| Pad | Assignment | Initial configuration |
|---|---|---|
| BTN1 | CLASSIC | SINE1 with the existing CLASSIC modulation |
| BTN2 | TWO TONE | SQUARE mode with the existing two-pitch behavior |
| BTN3 | LICKSHOT | LICKSHOT with the existing SAW DOWN gesture |
| BTN4 | PING | SYNDRUM sine one-shot with its existing sweep/default decay |

These are configurations of existing modes, not new DSP algorithms. Pad recall
does not write Tune, Rate, Depth, Echo Level/Time/Feedback/Tone, Master, or the
delay buffer. Ping uses the documented Syndrum defaults for the options that
belong to that voice.

## Priority and release

Each pad has an independent pressed state. The last pad pressed controls the
monophonic engine. Releasing another pad has no effect. Releasing the active pad
returns control to the most recently pressed remaining pad without creating a
new activation event; if none remains, the sustained gate releases while a
one-shot continues its envelope.

An activation sequence is stored atomically with the configuration that caused
it. This lets the audio task distinguish a new attack from a gate that remained
high and preserves a quick press/release between block snapshots. Switching
configurations does not reset the delay or echo filters.

HOLD is stored per pad only for supported sustained assignments. Selecting or
playing PING clears hidden holds so a previous sustained sound cannot reappear
later unexpectedly.

## Encoder combinations

On Home, press the encoder first and then a pad:

| Combination | Action |
|---|---|
| Encoder + BTN1 | toggle HOLD on the selected pad, when supported |
| Encoder + BTN2 | previous variant of the selected pad |
| Encoder + BTN3 | next variant of the selected pad |
| Encoder + BTN4 | momentary ECHO CUT |

The pad press is consumed by the combination and never becomes a note on
modifier release. ECHO CUT ends when either BTN4 or the encoder is released. A
plain encoder click opens the menu on release; a click used as a modifier does
not.

## Display and menu

The only logical viewport is 320×170 in landscape orientation. The desktop
widget may occupy more monitor pixels in the future, but its internal layout
and validation coordinates remain 320×170 with uniform scaling only.

Home is a source-art-driven cyber-dub scene rather than a text dashboard. The
preprocessed `home_bg.png` supplies the speaker system, rack, environment and
empty diegetic screens. Small alpha sprites add Soundboy and three levels of
speaker reaction; only real values, focus outlines, status lamps and the VU are
drawn at runtime. The selected pad normally follows the last played pad. The
menu uses `menu_bg.png` as a close-up of the same rack and can select another
pad without playing it, then edit only that pad's assignment, supported
variant, and Hold. Echo Loop remains a secondary function. Back returns to the
same sound-system scene through a short glitch cut.

`PerformanceController` owns Home/Menu mode, the current menu row, edit mode,
the menu's conditional Hold row, and all encoder behavior. Python sends raw
`ENCODER_SWITCH` and `ENCODER_ROTATE` events and renders the state returned by
the ESP; it does not run a second menu state machine. The menu renders a
three-row scrolling window so labels remain readable within 170 vertical
pixels.

Knob and encoder changes illuminate the corresponding physical module for
1.1 seconds and show the value on that module; there is no fullscreen value
popup. RATE remains inactive on the physical panel for PING because Syndrum
does not use the normal LFO; its stored value is not reassigned or destroyed.
Other unsupported sound options are omitted rather than sent to an ignored DSP
path.

### Scene mapping

| Real control/state | Diegetic element |
|---|---|
| Selected sound + variant | green rack nameplate |
| Tune / Rate | two oscillator-module displays |
| Encoder Depth | modulation module display and physical knob |
| Echo Time / Feedback / Level / Tone | four controls on the echo unit |
| Hold | red Hold lamp on the modulation module |
| Echo Loop | red Loop legend on the power amp |
| Master | power-amp/VU module |
| Rendered PCM peak | VU bars and speaker-cone excursion |
| Link | tiny amber service LED on the rack |

Boot is a 3.35 second camera-cut sequence: INIT, ROLL-IN, CONNECT and POWER use
four separate 320x170 backgrounds. Only scanlines, connector/power pulses and
brief deterministic glitch cuts are procedural. Home is capped at 10 fps;
Soundboy uses a deliberately sparse six-step idle cadence while the interaction
strip is used briefly for control/pad activity. The desktop preview has a
`Replay boot` control for visual verification.

Confirmed pad assignment and variant changes are saved to
`%APPDATA%\ESP-DubSiren\panel.json`. Only configuration is persisted: pressed
states, Hold/gates, ECHO CUT, active combinations, and the selected pad are not.

## Echo Tone and initial state

Echo Tone preserves the existing musical macro:

- center deadband: 60 Hz HPF / 16 kHz LPF;
- turn left: logarithmically lower the LPF to 400 Hz;
- turn right: logarithmically raise the HPF to 4 kHz.

The engine boot default is the documented technical state 60 Hz / 7 kHz, which
the macro cannot represent exactly. The UI therefore displays `CUSTOM 60 Hz /
7 kHz` and does not silently send a different cutoff during connection. The
macro becomes authoritative only when Echo Tone is actually moved.

ECHO CUT mutes only the wet return with a 3 ms anti-click ramp. It does not
change Echo Level, feedback, filtering, loop state, or delay contents.

## Desktop input

Mouse and keyboard sources are tracked independently. `1`–`4` play the pads,
`[` / `]` rotate the encoder, and `E` presses it. Keyboard autorepeat cannot
retrigger a held pad. Releasing outside a widget, losing focus, disconnecting,
or closing releases momentary actions safely while preserving assignments,
Hold configuration, and the delay state.

Knob movement uses one 25 ms timer per control. The latest value is sent on
each periodic flush, and release cancels any outstanding timer before sending
the final value immediately.

## Hardware display

Firmware 0.8.0 includes an Adafruit GFX/ST7789 renderer for Product 5394. It
uses the existing hardware SPI nets on GPIO11 MOSI and GPIO12 SCK, with GPIO9
CS, GPIO8 DC, GPIO7 reset, and GPIO14 backlight held high. Initialization is
`init(170, 320)` followed by rotation 1; runtime width and height must be
320×170. No touch, microSD, image decoder, filesystem, LVGL or brightness menu
is included.

Rendering runs from `loop()`, never from `audioTask`. Boot and Home are capped
at 10 fps; Menu redraws only on state changes and the 120 ms transition. Six
backgrounds are raw RGB565 in flash. Soundboy, glitch and speaker frames are
RGB565 plus a 1-bit mask. The generated payload is 935,119 bytes. On the
ESP32-S3 N16R8 build, flash is configured for 16 MB and OPI PSRAM is enabled
with `qio_opi`; `psramFound()` is checked at runtime. When available, a
108,800-byte framebuffer is allocated with `MALLOC_CAP_SPIRAM` for bitmap
composition. Allocation failure falls back to direct ST7789 background blits
and masked row runs, so the firmware remains usable without PSRAM. Dynamic rack
text and indicators are drawn with compact Adafruit GFX bitmap glyphs after the
bitmap frame. The final build uses 119,408 bytes of static RAM (36.4%) and
1,278,313 bytes of flash (19.5% of the 6.25 MB application partition). The
ST7789 library selects a 32 MHz SPI clock; a full 320×170 RGB565 frame has a
27.2 ms theoretical wire time before command and UI overhead. Runtime STATUS
exposes `displayRenderUs` and `maxDisplayRenderUs`; board measurements remain
the authority for checking the 100 ms budget. The audio task remains higher
priority and owns no display calls.

### Asset pipeline

`assets/display/source/` contains the ten high-resolution source artworks.
`assets/display/manifest.json` is the source of truth for crop rectangles,
runtime sizes, frame counts, anchors and UI zones. Background sources use the
explicit centered crop `[12, 6, 1708, 907]` (1696×901, exactly 32:17) before a
proportional Lanczos resize to 320×170. Three-frame strips are split into 724px
square cells, alpha-bounded and proportionally contained in their runtime
rectangles. Run `python tools/display_assets/build_assets.py` to regenerate
`assets/display/runtime/` and the deterministic `DisplayAssets.h/.cpp` arrays.
Pillow is the only new desktop dependency; it is used for the same PNG
compositing path in the Virtual ESP and reference capture generator.

Reference captures generated from the same desktop renderer are in
`docs/screenshots/display-boot-01.png` through `display-boot-04.png`, plus
`display-idle.png`, `display-tune.png`, `display-echo.png` and
`display-menu.png`. Every capture is exactly 320×170.

The current PCB is not display-ready: J3 exposes power, I²C, CS, DC, reset and
backlight, but not `SPI_MOSI` or `SPI_SCK`. J3 must be revised or another
explicit physical connection must be defined. No KiCad source was changed in
this software/UI revision, and operation on real display hardware remains
unverified.
