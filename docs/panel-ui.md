# Physical panel UI

## Goal

The desktop application must behave as a software representation of the physical instrument, not as a generic DSP editor. The primary surface therefore mirrors the controls that are intended to be available while performing. Secondary synthesis parameters remain available through the encoder/display.

The DSP and serial protocol remain the source of audio behavior. This UI layer changes how the existing parameters are presented and controlled; it does not create a second audio engine on the PC.

## Current software control surface

### Front panel

| Physical control | Primary function | DSP/protocol mapping |
|---|---|---|
| POT1 | TUNE | `SET TUNE_HZ` |
| POT2 | RATE | `SET LFO_RATE_HZ` |
| POT3 | ECHO | `SET ECHO_LEVEL` |
| POT4 | DELAY | `SET DELAY_MS` |
| POT5 | FEEDBACK | `SET FEEDBACK` |
| POT6 | FILTER | macro over `SET HPF_HZ` / `SET LPF_HZ` |
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

The menu contains the following entries for tonal voices:

- Voice
- LFO Shape
- Depth
- Decay
- Sweep
- Sweep Time
- Modulation
- Retrigger
- Echo Loop
- Hold
- Back

The encoder rotates through entries. Press selects an editable entry; rotation changes its value; press exits edit. Boolean values toggle on press.

Transient voices only show controls that affect their DSP path. `SYNDRUM` shows Voice, Drum Wave, Drum Sweep, Decay, Echo Loop and Back. `GUNSHOT` shows Voice, Decay, Echo Loop and Back. LFO, modulation, retrigger, global sweep and hold entries are hidden in these modes.

## Echo filter macro

The existing independent HPF and LPF parameters are intentionally hidden. The physical FILTER potentiometer exposes one performance macro instead:

- center: wide/flat echo path;
- rotate toward low-pass: lower LPF while keeping HPF near its minimum;
- rotate toward high-pass: raise HPF while keeping LPF wide.

The center 5% on either side is a flat deadband at 60 Hz / 16 kHz. Outside that area each side follows a logarithmic curve to 400 Hz LPF or 4 kHz HPF at the corresponding endpoint. This preserves the existing DSP without exposing two technical frequency sliders on the primary UI.

## Future physical implementation (not included)

The intended hardware model is six front potentiometers, four momentary front buttons, one push encoder and a separate rear MASTER. The repository PCB V1 still documents five front potentiometers and one rear master, so a later hardware revision must update the analog mapping and placement before fabrication.

There is no hardware display implementation in the current firmware: no display controller, driver, library or pin mapping is selected. The display in the desktop UI is only a functional simulation of the planned interaction model.

The firmware also does not yet read MCP3208, MCP23017 or the physical encoder. Those hardware-input and PCB changes are outside this software UI revision. The current data path is:

`desktop controls -> protocol -> ControlState -> AudioEngine`
