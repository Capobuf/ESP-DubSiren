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

Home is a pixel-art dub stage rather than a text dashboard. A large handmade
speaker stack occupies the left side, the active rack occupies the center, and
the soundboy remains secondary on the right. The rack's own green display shows
the selected pad assignment and variant. The selected pad normally follows the
last played pad. The menu can select another pad without playing it, then edit
only that pad's assignment, supported variant, and Hold. Echo Loop remains
available as a secondary function. Back returns to the same assembled scene.

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

The soundboy enters carrying the first cabinet, completes the stack, places the
rack, connects the cable and powers the system over a 3.3 second boot sequence.
The last boot composition is already the Home composition. Idle runs at 10 fps
with only head/body bob, hand movement, power LEDs, VU and cone excursion. The
desktop preview has a `Replay boot` control for visual verification.

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
320×170. No touch, microSD, LVGL, framebuffer, or brightness menu is included.

Rendering runs from `loop()`, never from `audioTask`. Boot and Home are capped
at 10 fps; Menu redraws only on state changes. A full RGB565 framebuffer would
cost 108,800 bytes and is deliberately not allocated. The scene has no bitmap
assets, so asset RAM/flash cost is 0 bytes; it is drawn from Adafruit GFX
primitives. The `PanelDisplay` global is 428 bytes in the linked image and its
release code/constant sections are about 10,099 bytes. The full firmware build
uses 119,204 bytes of static RAM (36.4%) and 341,085 bytes of flash (10.2%).
The ST7789 library selects a 32 MHz SPI clock; a full 320×170 RGB565 clear has a
27.2 ms theoretical wire time before primitive overhead. Runtime STATUS exposes
`displayRenderUs` and `maxDisplayRenderUs` so the actual board can be checked
against the 100 ms frame budget. The audio task remains higher priority and
owns no display calls.

Reference captures generated from the same desktop renderer are in
`docs/screenshots/display-idle.png`, `display-tune.png`, `display-echo.png`,
`display-menu.png`, and `display-boot.png`.

The current PCB is not display-ready: J3 exposes power, I²C, CS, DC, reset and
backlight, but not `SPI_MOSI` or `SPI_SCK`. J3 must be revised or another
explicit physical connection must be defined. No KiCad source was changed in
this software/UI revision, and operation on real display hardware remains
unverified.
