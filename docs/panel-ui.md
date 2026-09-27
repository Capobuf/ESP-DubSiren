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
| Encoder rotation on Home | DEPTH | `SET LFO_DEPTH_OCT` |
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

Home shows the explicitly selected pad, its assignment and variant, Depth, Hold,
Echo Loop, and link state. The selected pad normally follows the last played
pad. The menu can select another pad without playing it, then edit only that
pad's assignment, supported variant, and Hold. Echo Loop remains available as
a secondary function. If a latched sound differs from the selected pad, Home
adds a `LIVE Pn` indicator. Back returns to Home.

Knob and encoder changes show a 900 ms value overlay without moving keyboard
focus. RATE is visibly inactive for PING because Syndrum does not use the normal
LFO; its stored value is not reassigned or destroyed. Other unsupported sound
options are omitted rather than sent to an ignored DSP path.

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

## Hardware scope

The display and controls in this revision are software emulation. No display
driver, GPIO assignment, PCB change, or physical-control reader is included.
The shared C++ controller is deliberately independent of those drivers so a
future hardware input layer can send the same pad and encoder actions.
