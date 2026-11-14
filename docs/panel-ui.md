# Physical panel UI

The panel remains six front pots, four buttons, one encoder with push, one
small, primarily text-based Adafruit Product 5393 display (320x172) and one
separate rear Master. No extra performance knobs.

The desktop layout places three pots above the display/encoder, three below,
then the four buttons. Connection and audio controls remain above a scrollable
panel; both axes can scroll in small windows or at larger text scaling. Tab
focus automatically scrolls a control into view. The debug UI uses the same
connection/status area and scrolling behavior.

Connecting runs build/open in one background worker; Tk stays responsive and
reports `Building / opening Virtual ESP…`. Connect is disabled until it finishes
to avoid overlapping builds. On failure it becomes available for retry. Closing
the window cancels adoption of a late connection and releases its transport.

`Requested (unconfirmed)`, `Accepted` and `Rejected` describe command delivery.
The separate `Confirmed` row shows Tune, Rate, Delay, Depth and Master from
STATUS, including effective synchronized Rate/Delay. Sliders and pot positions
are requests; an accepted pot movement may still await pickup. A rejection
remains visible across STATUS updates and subsequent replies from the same
batch, until another command is requested. Firmware without result packets
never produces a false acceptance indication.

| Control | Factory mapping / command |
|---|---|
| POT1..POT6 | `POT <1..6> <0..1>`; C++ applies the stored mapping |
| BTN1 TRIGGER | `PAD 1 0|1` |
| BTN2 HOLD | `PAD 2 0|1`; toggle on rising edge |
| BTN3 MOD UP | `PAD 3 0|1` |
| BTN4 ECHO CUT | `PAD 4 0|1` |
| Encoder turn | `ENCODER_ROTATE PREV|NEXT`; steps the Trigger voice, or the selected pad's variant |
| Encoder push | `ENCODER_SWITCH 0|1`; opens Menu on completed click/release |
| Rear Master | `SET MASTER` |

There is one panel layout. The factory mapping is POT1 Pitch (zones), POT2 Rate
(zones plus MANUAL), POT3-POT6 Echo Level/Time/Feedback/Filter, BTN1 Trigger,
BTN2 Hold, BTN3 Mod Up and BTN4 Echo Cut; every POT target and BTN action,
including Play Sound, Echo Loop, Echo Spin and Tap Tempo, is assignable in
SETUP. Pitch and Rate can use zones or continuous ranges;
target-specific curves, validation and formatting live in C++. Pickup is the
factory behavior, with an UP/DOWN direction shown until the physical pot
crosses the current value. Rear Master is fixed and remains immediately live.

Home has two views. While the selected button is not a Play Sound pad it shows
the large siren view: Trigger voice name, PITCH, RATE and a status line. While
it is a pad, Home shows the pad view: pad number, family and variant, Tune,
Rate, Depth, Master, echo values and status. With zoned Pitch and Rate the
siren view shows symbolic and physical values together, for example
`PITCH MID / 680 Hz` and `RATE SLOW / 0.70 Hz`. Selector labels and values come
from C++ STATUS, including hysteresis; Python does not implement a second
selector or any DSP.

The root menu contains Sound, Mod, Echo, Setup, Tap Tempo and Back. Setup
contains Pots, Buttons, Encoder, Reset Defaults and Back. Submenus expose only applicable
fields and the controller supplies the
page title, ordered rows, labels, values and footer to both renderers. Moving a
pot while Menu is open updates the footer without replacing the menu. Reset
Defaults requires an explicit YES confirmation.

The pot footer yields to the navigation hint on the next encoder turn/click.
Target descriptors own range limits, mapping and numeric units/formatting.
Both renderers fit long text within the safe width; firmware overlays
use the controller's title (including Pickup and Live Pitch) instead of a
fixed Tune label. Text sizing is presentation only; the C++ content remains
authoritative.

At Home, free encoder rotation steps the Trigger voice through 18 voices:
SINE 1, SINE 2, TEST TONE, SQUARE, SONAR, SAW, TRIANGLE, TWO TONE TRIANGLE,
TWO TONE SQUARE, the five LICKSHOT variants, the three PING variants and
GUNSHOT. When the selected button is a Play Sound pad, rotation steps that
pad's variant instead. Depth is edited in the MOD menu or from a pot assigned
to LFO Depth.

Holding a physical Trigger or Play Sound button while turning the encoder
applies transient live pitch. The factory step is one semitone with a +/-12
range. It never changes base Tune and resets when the final relevant physical
button is released. Gunshot reports N/A. Encoder push plus a button is a
shortcut only when the pressed button is a Play Sound pad (BTN1 pad latch,
BTN2/BTN3 variant down/up, BTN4 Echo Cut); it then takes precedence over
playing the pad and over the menu click.

The default filter is FLAT. Its centered deadband bypasses wet filtering;
left/right movement applies the project LP/HP calibration in the core.
Echo Cut leaves the delay evolving. Master and audio routing are unchanged.

Mouse/keyboard sources are tracked independently. Keys 1-4 operate buttons,
[ / ] rotate the encoder and E presses it. Autorepeat cannot retoggle Hold.
Tab also focuses every active pot and rear Master, with a blue focus outline.
Arrow keys adjust by 2% of the normalized travel; Home/End select minimum/maximum.
Inactive controls do not accept keyboard adjustments or participate in Tab order.
Releasing outside widgets, losing focus or disconnecting releases momentaries
with `PANEL_RESET`; intentional Hold remains latched until toggled. Pot
commands keep
one 25 ms timer per control, flush periodically and send the final position
on release.

The ESP32 stores schema-3 `PanelSettings` in Preferences/NVS and reports corrupt
loads and save failures. A layout stored by another schema is discarded at
load, without migration: factory settings apply. VirtualESP stores the same
logical settings in `%APPDATA%/ESP-DubSiren/panel.json` and likewise discards
a file with an older schema; invalid files are surfaced instead of silently
ignored. A real device is authoritative on
connection: the PC adopts STATUS and never pushes a mapping or stale local
default. No pressed
state, gate, live transpose or other transient state is restored from disk.

Malformed configuration commands invalidate an entire staged restore, including
syntax errors. Integer fields are checked before narrowing and corrupt enum or
non-finite values are rejected on load. Host saves use a temporary file and an
atomic replacement, preserving the previous file if replacement fails. Focus loss cancels an encoder
click rather than opening/committing a menu; physically held inputs retain ownership.

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
unverified on the [current PCB](../hardware/README.md).

Rendering runs from `loop()`, never from `audioTask`. Home and Menu redraw on
state changes, at most once every 100 ms; temporary parameter text expires
after 1100 ms. The siren view shows voice, Pitch/Rate and Hold/Echo status. The
pad view shows pad/sound/variant, Tune, Rate, Depth, Master, echo values and status.
The encoder menu retains selection, editing, values and navigation hints.
Text is drawn directly, with no display buffer or PSRAM requirement.
`displayRenderUs` and `maxDisplayRenderUs` remain available in STATUS.
The desktop panel draws text with Tk; its existing control callbacks manage
parameter overlays. Audio, controls and VirtualESP synthesis remain unchanged.

On the PCB, U5 is the Adafruit 5393 SPI display.

## Menu performance (#10)

Il click da Home apre SOUND, MOD, ECHO, SETUP, TAP TEMPO, BACK. Tre righe
visibili con scroll. SOUND mostra VOICE (la voce del Trigger) oppure, se il
pulsante selezionato è un pad Play Sound, PAD, FAMILY e VARIANT; seguono per
ogni voce i parametri applicabili (Pitch, Decay oppure Length/Punch/Pitch Tau,
Sweep, Tone e Contour) e, per un pad, SAVE PATCH e RECALL PATCH. MOD mostra la
zona RATE quando Rate è a zone, poi SHAPE (la prima forma è SIREN), MODULATION,
RATE in Hz solo quando Rate non è a zone, e DEPTH e RETRIGGER solo per le forme
diverse da SIREN e MANUAL; le voci che non usano il LFO non mostrano queste
righe. ECHO espone sempre Level/Time/Feedback/Filter/Color e Loop, con Spin e
Cut come stati momentanei di sola lettura.

Le famiglie Play Sound sono SIREN, TWO TONE, LICKSHOT, PING, GUNSHOT e TEST
TONE. I parametri del suono corrente
restano al nuovo trigger dello stesso pad; cambiare famiglia applica i default.
Decay TAU indica la costante esponenziale, non il tempo al silenzio; a 0
seleziona il gate finito di 2 ms in attacco e 3 ms in rilascio.
Le modifiche sonore sono live e il click termina l'editing (DONE); il salvataggio
della patch è una funzione separata. Home mantiene Hold/Cut/Spin/Loop fuori
dall'area degli overlay. Leggibilità ST7789 da verificare sull'hardware.

## Quattro memorie sonore (#11)

SOUND / SAVE PATCH salva la voce corrente nel pad selezionato; RECALL PATCH
scarta le modifiche live e ripristina la memoria. Disponibili anche
`PANEL SAVE 1..4` e `PANEL RECALL 1..4`. Le memorie appartengono ai pulsanti
con azione Play Sound: per gli altri pulsanti i comandi sono rifiutati. Un asterisco nel titolo SOUND indica
una patch assente o modificata. Il retrigger dello stesso pad conserva gli edit
live; passando a un altro pad si richiama la sua memoria salvata (se presente).
Tornando al primo si perdono gli edit non salvati. Nessun autosave sonoro:
i parametri restano live fino a SAVE PATCH. Famiglia e variante del pad sono
invece assegnazioni del pulsante e vengono salvate sia dalla Home (encoder,
scorciatoie BTN) sia dalla pagina SOUND, con la stessa attesa di 2 s prima
della scrittura NVS.

La patch contiene voce/variante, Tune, Rate, Depth, Decay, Sweep, parametri
Lickshot, Tone/Contour, Syndrum, modulazione, retrigger e voicing. Echo e Master
restano globali; Recall non cancella il delay e riarma Pickup. Gli eventi audio
catturano l'intero stato sonoro.

Schema 3: un layout salvato con uno schema precedente viene scartato al
caricamento e valgono le impostazioni di fabbrica, con quattro memorie vuote.
Trigger, Hold, latch attivo, Loop e transpose non fanno parte della patch.

STATUS include `settings.patches`, quattro vettori numerici versionati di 23
campi, definiti da `soundPatchValue` in PanelSettings.h. Il ripristino VirtualESP
usa `PANEL PATCH n <23 valori>` dentro PANEL BEGIN/COMMIT, validando il candidato
prima di applicarlo. La persistenza NVS fisica resta da verificare al riavvio.

## Tap tempo

PERFORMANCE include TAP TEMPO; MOD ed ECHO includono CLOCK, BPM e divisione.
L'azione pulsante TAP TEMPO è assegnabile in SETUP.
Semantica, limiti e persistenza: [Tap tempo e sincronizzazione](tempo.md).
