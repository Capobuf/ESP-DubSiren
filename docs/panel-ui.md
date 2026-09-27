# Physical panel UI

## Goal

The desktop application must behave as a software representation of the physical instrument, not as a generic DSP editor. The primary surface therefore mirrors the controls that are intended to be available while performing. Secondary synthesis parameters remain available through the encoder/display.

The DSP and serial protocol remain the source of audio behavior. This UI layer changes how the existing parameters are presented and controlled; it does not create a second audio engine on the PC.

## Proposed control surface

### Front panel

| Physical control | Primary function | DSP/protocol mapping |
|---|---|---|
| POT1 | TUNE | `SET TUNE_HZ` |
| POT2 | RATE | `SET LFO_RATE_HZ` |
| POT3 | DEPTH | `SET LFO_DEPTH_OCT` |
| POT4 | DELAY | `SET DELAY_MS` |
| POT5 | FEEDBACK | `SET FEEDBACK` |
| POT6 | ECHO | `SET ECHO_LEVEL` |
| BTN1 | TRIGGER | `TRIGGER 0/1` |
| BTN2 | DIVE / MOD DOWN | `MOD_DOWN 0/1` |
| BTN3 | RISE / MOD UP | `MOD_UP 0/1` |
| BTN4 | ECHO CUT | `ECHO_CUT 0/1` |
| Encoder rotation | Voice on home; navigation/edit in menu | multiple existing commands |
| Encoder press | Open/select/edit/confirm | UI state only or existing commands |
| Display | State, navigation and temporary parameter feedback | UI state |

`DIVE` / `RISE` become `MOD DOWN` / `MOD UP` when the LFO shape is `MANUAL`.
Transient modes disable the two modulation buttons because the current DSP does not use them.

### Rear panel

`MASTER` remains independent and maps to `SET MASTER`.

## Display UX

### Home

The home page shows:

- current voice in the largest type;
- current LFO shape;
- compact state flags such as `HOLD`, `MOD OFF`, `LOOP` and connection state;
- a short hint for encoder use in the desktop emulator.

Rotating the encoder on the home page changes the voice. Pressing it opens the menu.

### Parameter overlay

Moving a physical/front-panel knob temporarily replaces the home page with the parameter name and formatted value. The desktop implementation currently returns to the previous page after 900 ms.

This gives visual confirmation without requiring permanent labels or duplicate sliders on the display.

### Encoder menu

The first menu revision contains:

- Voice
- LFO Shape
- Decay
- Sweep
- Sweep Time
- Echo Filter
- Modulation
- Retrigger
- Echo Loop
- Hold
- Syndrum Wave and Drum Sweep only while `SYNDRUM` is selected
- Back

The encoder rotates through entries. Press selects an editable entry; rotation changes its value; press exits edit. Boolean values toggle on press.

## Echo filter macro

The existing independent HPF and LPF parameters are intentionally hidden from the performance surface. The menu exposes one `ECHO FILTER` macro instead:

- center: wide/flat echo path;
- rotate toward low-pass: lower LPF while keeping HPF near its minimum;
- rotate toward high-pass: raise HPF while keeping LPF wide.

This preserves the existing DSP without exposing two technical frequency sliders on the primary UI.

## Current hardware mismatch

The repository PCB V1 currently documents five front potentiometers and one rear master. A six-front-control panel plus rear master requires the analog mapping to be revised before fabrication. The MCP3208 has enough channels for six front controls plus master, but the schematic/pinout must become the source of truth before physical-control firmware is implemented.

The display connector also remains controller-agnostic. Do not add a concrete OLED/TFT driver until the actual display module and interface are selected. The desktop display model should be used to settle navigation and information hierarchy first.

## Firmware work still required

The current firmware has no physical-panel input or display loop. The next hardware-facing revision needs a small control task that reads MCP3208, MCP23017 and the quadrature encoder, updates `ControlStore`, and renders the display from the same UI state model used by the desktop prototype.

Keep that task independent from `AudioEngine`. Potentiometer sampling needs modest smoothing/deadband; button/encoder events need debouncing. Display rendering should be dirty/event driven rather than tied to the 48 kHz audio path.
