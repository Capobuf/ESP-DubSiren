# Physical panel UI

The panel remains six front pots, four buttons, one encoder with push, one
320x170 display and one separate rear Master. No extra performance knobs.

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
pad bank, Depth encoder, pad configuration menu and illustrated rack Home.
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
gate or Hold is restored from disk. Existing source artwork, boot sequence,
asset pipeline and the Extended rack renderer are retained.

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
bitmap frame. See the baseline validation report for current build sizes. The
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
