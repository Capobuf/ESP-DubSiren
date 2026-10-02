# Physical panel UI

The panel remains six front pots, four buttons, one encoder with push, one
small, primarily text-based Adafruit Product 5393 display (320x172) and one
separate rear Master. No extra performance knobs.

| Control | Classic mapping / command |
|---|---|
| POT1 PITCH | `SET PITCH_POSITION <0..1>` |
| POT2 RATE | `SET RATE_POSITION <0..1>` |
| POT3 ECHO LEVEL | `SET ECHO_LEVEL` |
| POT4 ECHO TIME | `SET DELAY_MS` (50-1000 ms, logarithmic travel) |
| POT5 ECHO FEEDBACK | `SET FEEDBACK` |
| POT6 ECHO FILTER | `SET ECHO_FILTER <-1..1>`; atomic shared HP/LP macro |
| BTN1 TRIGGER | `PAD 1 0|1` |
| BTN2 HOLD | `PAD 2 0|1`; toggle on rising edge |
| BTN3 MANUAL MOD | `PAD 3 0|1` |
| BTN4 ECHO CUT | `PAD 4 0|1` |
| Encoder turn | `ENCODER_ROTATE PREV|NEXT`; cycles four voicings |
| Encoder push | `ENCODER_SWITCH 0|1`; opens Menu on press |
| Rear Master | `SET MASTER` |

Classic Home shows the selected voicing, PITCH LOW/MID/HIGH, RATE
SLOW/MID/FAST/MANUAL and HOLD/ECHO/ECHO CUT/DRY state. A brief pot overlay shows
its name and confirmed selector position, time in ms, percentage or FILTER
LP/FLAT/HP amount. Selector labels come from C++ STATUS, including hysteresis;
Python does not implement a second selector or any DSP.

Classic Menu contains SWITCH TO EXTENDED and BACK. Extended retains the old
pad bank, Depth encoder, pad configuration menu and textual Home.
Its menu also offers SWITCH TO DS01E. All advanced sounds and settings remain
available in the debug UI (`pc/app.py`, choose EXTENDED). See
[controls](controls.md) for the preserved Extended gestures.

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

Saved Extended pad assignments remain in `%APPDATA%/ESP-DubSiren/panel.json`;
restoring them never leaves Classic or triggers audio. No pressed state,
gate or Hold is restored from disk.

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
