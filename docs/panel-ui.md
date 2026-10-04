# Physical panel UI

The panel remains six front pots, four buttons, one encoder with push, one
small, primarily text-based Adafruit Product 5393 display (320x172) and one
separate rear Master. No extra performance knobs.

| Control | Classic mapping / command |
|---|---|
| POT1..POT6 | `POT <1..6> <0..1>`; C++ applies the active profile mapping |
| BTN1 TRIGGER | `PAD 1 0|1` |
| BTN2 HOLD | `PAD 2 0|1`; toggle on rising edge |
| BTN3 MANUAL MOD | `PAD 3 0|1` |
| BTN4 ECHO CUT | `PAD 4 0|1` |
| Encoder turn | `ENCODER_ROTATE PREV|NEXT`; cycles four voicings |
| Encoder push | `ENCODER_SWITCH 0|1`; opens Menu on completed click/release |
| Rear Master | `SET MASTER` |

Factory mappings reproduce the original controls. Each profile has independent
POT and BTN assignments. Pitch and Rate can use zones or continuous ranges;
target-specific curves, validation and formatting live in C++. Pickup is the
factory behavior, with an UP/DOWN direction shown until the physical pot
crosses the current value. Rear Master is fixed and remains immediately live.

Classic Home shows symbolic and physical values together, for example
`PITCH MID / 392 Hz` and `RATE SLOW / 0.35 Hz`. Selector labels and values come
from C++ STATUS, including hysteresis; Python does not implement a second
selector or any DSP.

The shallow menu contains Profile, Pots, Buttons, Encoder, Reset Defaults and
Back. Submenus expose only applicable fields and the controller supplies the
page title, ordered rows, labels, values and footer to both renderers. Moving a
pot while Menu is open updates the footer without replacing the menu. Reset
Defaults requires an explicit YES confirmation.

In Extended Home, free encoder rotation cycles the selected pad's waveform
variant. Holding physical BTN1 while turning adjusts Depth in 0.05-octave steps,
clamped to 0-2 octaves. Releasing BTN1 keeps Depth and returns to variant
selection; a latched Hold alone does not activate this modifier. Ping and
Gunshot store Depth but report N/A because they do not use LFO depth.

Holding a physical Trigger or Play Sound button while turning the encoder
applies transient live pitch, except Extended BTN1 whose Depth gesture takes
precedence. The factory step is one semitone with a +/-12 range. It never
changes base Tune and resets when the final relevant physical button is
released. Gunshot reports N/A. Existing Extended encoder-push+button shortcuts
retain precedence over both rotation gestures.

The default filter is FLAT. Its centered deadband bypasses wet filtering;
left/right movement applies the project LP/HP calibration in the core.
Echo Cut leaves the delay evolving. Master and audio routing are unchanged.

Mouse/keyboard sources are tracked independently. Keys 1-4 operate buttons,
[ / ] rotate the encoder and E presses it. Autorepeat cannot retoggle Hold.
Releasing outside widgets, losing focus or disconnecting releases momentaries
with `PANEL_RESET`; intentional Hold remains latched until toggled or the
profile changes. Switching profile clears gates and Hold. Pot commands keep
one 25 ms timer per control, flush periodically and send the final position
on release.

The ESP32 stores schema-1 `PanelSettings` in Preferences/NVS and reports corrupt
loads and save failures. VirtualESP stores the same logical schema in
`%APPDATA%/ESP-DubSiren/panel.json`; invalid files are surfaced instead of
silently ignored. A real device is authoritative on connection: the PC adopts
STATUS and never pushes a profile, mapping or stale local default. No pressed
state, gate, live transpose or other transient state is restored from disk.

Older VirtualESP files containing only four `pads` are migrated on connection.
The controller supplies the default pots, Classic buttons and encoder settings;
saved sounds and variants become Extended Play Sound button assignments.
The original file is preserved as `panel.legacy.json` before saving schema 1.
Microsoft Store Python may redirect the AppData file into its package's
`LocalCache/Roaming/ESP-DubSiren` directory.

Feedback at 0% still produces one delayed repeat. Set Echo Level to 0% for
dry output, or hold ECHO CUT to mute the echo temporarily.

## Hardware display

The Adafruit GFX/ST7789 text renderer targets Adafruit Product 5393.
It retains SPI GPIO11 MOSI and GPIO12 SCK, GPIO9 CS, GPIO8 DC, GPIO7 reset,
and GPIO14 backlight held high. Initialization is `init(172, 320)` with
rotation 1 (320x172), as specified in the
[Adafruit guide](https://learn.adafruit.com/adafruit-1-47-round-rectangle-tft-display/arduino-wiring-test).
The library supplies the centered panel offsets. Physical operation remains
unverified; the V2 PCB connection and mounting are not yet implemented.

Rendering runs from `loop()`, never from `audioTask`. Home and Menu redraw on
state changes, at most once every 100 ms; temporary parameter text expires
after 1100 ms. Classic shows voice, Pitch/Rate and Hold/Echo status. Extended
shows pad/sound/variant, Tune, Rate, Depth, Master, echo values and status.
The encoder menu retains selection, editing, values and navigation hints.
Text is drawn directly, with no display buffer or PSRAM requirement.
`displayRenderUs` and `maxDisplayRenderUs` remain available in STATUS.
The desktop panel draws text with Tk; its existing control callbacks manage
parameter overlays. Audio, controls and VirtualESP synthesis remain unchanged.

The current PCB is not display-ready: J3 exposes power, I²C, CS, DC, reset and
backlight, but not `SPI_MOSI` or `SPI_SCK`. J3 must be revised or another
explicit physical connection must be defined. No KiCad source was changed in
this software/UI revision, and operation on real display hardware remains
unverified.
