# Physical panel UI

The panel remains six front pots, four buttons, one encoder with push, one
Adafruit Product 5393 display (320x172) and one
separate rear Master. No extra performance knobs.

The desktop layout places three pots above the display/encoder, three below,
then the four buttons. Connection and audio controls remain above a scrollable
panel; both axes can scroll in small windows or at larger text scaling. Tab
focus automatically scrolls a control into view. The debug UI uses the same
connection/status area and scrolling behavior.

Connecting opens the serial port in one background worker; Tk stays responsive
and reports `Opening device…`. Connect is disabled until it finishes to avoid
overlapping connections. On failure it becomes available for retry. Closing
the window cancels adoption of a late connection and releases its transport.

`Requested (unconfirmed)`, `Accepted` and `Rejected` describe command delivery.
The separate `Confirmed` row shows Tune, Rate, Delay, Depth and Master from
STATUS, including effective synchronized Rate/Delay. Sliders and pot positions
are requests. A rejection
remains visible across STATUS updates and subsequent replies from the same
batch, until another command is requested. Firmware without result packets
never produces a false acceptance indication.

| Control | Factory mapping / command |
|---|---|
| POT1..POT6 | `POT <1..6> <0..1>`; C++ applies the stored mapping |
| BTN1 pad SINE 1 | `PAD 1 0|1` |
| BTN2 pad TWO TONE | `PAD 2 0|1` |
| BTN3 pad GUNSHOT | `PAD 3 0|1` |
| BTN4 TAP TEMPO | `PAD 4 0|1`; MOD UP in MANUAL |
| Encoder turn | `ENCODER_ROTATE PREV|NEXT`; steps the sound of the last pressed pad, or the Trigger voice; the up/down lever while a pad is held |
| Encoder push | `ENCODER_SWITCH 0|1`; opens Menu on completed click/release; with a held pad locks Hold, while locked a click unlocks |
| Rear Master | `SET MASTER` |

There is one panel layout. The factory mapping is POT1 Pitch (zones), POT2 Rate
(zones plus MANUAL), POT3-POT6 Echo Level/Time/Feedback/Filter, BTN1-BTN3
sound pads (SINE 1, TWO TONE, GUNSHOT) and BTN4 Mod Up, which taps the tempo
outside MANUAL; every POT target and BTN action, including Trigger, Hold, Echo
Cut, Echo Loop, Echo Spin and Tap Tempo, is assignable in SETUP. Pitch and Rate can use zones or continuous ranges;
target-specific curves, validation and formatting live in C++. Every pot
reading applies at once, also after a voice change, a patch recall or a new
assignment: the knob always owns its value. On PING and GUNSHOT a Rate pot
sets Decay instead and its cell reads DECAY ([controls](controls.md)). The first reading of each pot
after power-on is taken silently, so a Rate pot sitting at SLOW is SLOW from
the start. Rear Master is fixed and immediately live; turning it shows MASTER
and the level in percent as an overlay, except for its first reading after
power-on.

Home is a map of the front panel, which has no silkscreen. Each cell sits
where its control is: the top row stands for POT1-POT3 above the display, the
bottom row for POT4-POT6 below it, the middle row is the voice banner and the
last row stands for the four keys. A pot cell shows only
the assigned function and a horizontal bar with the current position: one
segment per zone with zoned Pitch and Rate, centred for the bipolar Echo
Filter. No number is shown in the cell. A key cell shows its action (for a
Play Sound pad its sound), gold while pressed, red while latched
(Hold, Echo Loop, a pad held on); Tap Tempo shows the BPM. The gold banner, across the whole width, shows the current sound, named as on its key, and USB
when linked; a moved pot lights its cell and the banner shows the
controller's title and value for 1100 ms. Persistence and device errors turn
the banner red. Labels and values come from the C++ controller; Python does
not implement a second selector or any DSP.

The menu is in Italian: titles, row labels and the footer. The names of
sounds, pot functions, button actions and LFO shapes stay as Home shows them.
The root menu, titled MENU, contains COMANDI, SUONO, MODULAZIONE, ECO, TEMPO,
ALTRO and INDIETRO. COMANDI is drawn as the Home map instead of a list: the six pot
cells and the four key cells. The middle row splits in two: about 80% is a
gold box with the selected control and what it does (`POT 3 - ECHO LEVEL`
above, `VOLUME DELL'ECO` below), the rest is ESCI, where the encoder sits at
the right of the display.
Moving a pot or pressing a key (other than BTN4, which is BACK) selects its
cell; the key does not play and a pot's power-on scan selects nothing. The
encoder steps through POT 1-6, TASTO 1-4 and ESCI, which returns straight to
Home (settings are saved anyway); BTN4 steps back to MENU instead. Pushing on a pot or key
cell chooses its function or action in the cell itself, between turn arrows,
and the banner explains each choice as it comes. A second push keeps it and
opens its options, when it has any: a pot that is not NONE, a key that plays
its own sound; BACK keeps it and stays on the map. ALTRO holds what has no
control on the panel: VALORI POT, RIPRISTINA and INDIETRO. VALORI POT is the general
switch between fixed positions (LOW/MID/HIGH) and a free sweep (LIBERI) for
every pot that has positions, Pitch and Rate; it reads MISTI when they differ,
and each pot keeps its own VALORI row on its page. A pot page has, for Pitch
and Rate, VALORI (LOW/MID/HIGH or SLOW/MID/FAST for fixed positions, LIBERI
for a continuous sweep), then the value of each position (LOW, MID, HIGH or
SLOW, MID, FAST) or MINIMO and MASSIMO. A key page has SUONO and, where the
sound allows it, LATCH. BACK from a page returns to its cell on the map. TEMPO holds BPM, LFO A TEMPO and ECO A TEMPO (ON/OFF);
the tempo is tapped on its key, not in the menu. The footer describes the
selected row in one line (for example `IL RATE SEGUE IL BPM` or `RATE LIBERO,
IN HZ` on LFO A TEMPO) and falls back to the navigation hint where the label
says it all. While a choice is changed it describes that choice: each LFO
shape (`SALE PIANO, CADE DI COLPO` for SIREN), each sound of SUONO, each pot
function and key action on the map. Submenus expose only applicable fields and
the controller supplies the page title, ordered rows, labels, values and
footer. Moving a pot on any other page updates the footer without replacing
the menu. RIPRISTINA requires an explicit SI confirmation.

While Menu is open, BTN4 (the key under the encoder) is BACK, whatever its
assigned action: it ends editing like the push (a Reset confirmation is
cancelled instead), otherwise it does what the page's BACK row does, up to
Home. It neither plays nor runs its own action. The menu footer shows it as
`◄ INDIETRO`.

The pot footer yields to the navigation hint on the next encoder turn/click.
Target descriptors own range limits, mapping and numeric units/formatting.
Both renderers fit long text within the safe width; firmware overlays
use the controller's title (including LEVA and HOLD) instead of a
fixed Tune label. Text sizing is presentation only; the C++ content remains
authoritative.

At Home, free encoder rotation steps the sound of the last pressed Play Sound
pad (or the Trigger voice) through the six sounds of the panel: SINE 1, SINE 2,
TWO TONE, SONAR, PING and GUNSHOT. No outline marks that pad: its key and the
banner show the new name. MENU > SUONO does the same. Depth is edited in the MOD menu or from a pot assigned
to LFO Depth.

Holding a Trigger or Play Sound button while turning the encoder moves a
three-position lever, DOWN / CENTRE / UP, one detent per position; it springs
back to the centre when the last playing key is released. Right raises, left
lowers, with every shape: in MANUAL it is MOD UP / MOD DOWN, with SIREN and the
other shapes a one-octave bend. The banner shows LEVA and MOD UP, MOD DOWN,
+1 OTTAVA, -1 OTTAVA or CENTRO. PING and GUNSHOT have no bend: N/A. It never
changes base Tune.

Holding a Trigger or Play Sound button and pushing the encoder, in either
order, locks the sound on (HOLD): it stays on after the button is released,
and that key turns red with a padlock before its name; the banner shows HOLD
BLOCCATO. One press of the locked key, or one encoder click, unlocks it
(SBLOCCATO) and does nothing else: the key does not play again and the click
does not open the menu. Pressing another playing key while locked plays its
sound and moves the lock to it. The push of the gesture never opens the menu.
A button assigned to HOLD toggles the same state; it turns red itself, with no
padlock on a sound key, and an encoder click releases it too.

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

The ESP32 stores schema-6 `PanelSettings` in Preferences/NVS and reports corrupt
loads and save failures. A layout stored by another schema is discarded at
load, without migration: factory settings apply. The device is authoritative on
connection: the PC adopts STATUS and never pushes a mapping or stale local
default. No pressed
state, gate, lever, lock or other transient state is restored from disk.

Malformed configuration commands invalidate an entire staged restore, including
syntax errors. Integer fields are checked before narrowing and corrupt enum or
non-finite values are rejected on load. Focus loss cancels an encoder
click rather than opening/committing a menu; physically held inputs retain ownership.

Feedback at 0% still produces one delayed repeat. Set Echo Level to 0% for
dry output, or hold ECHO CUT to mute the echo temporarily.

## Hardware display

The Adafruit GFX renderer targets Adafruit Product 5393; `PanelLcd` sends
each frame with the ESP-IDF `esp_lcd` ST7789 driver over SPI DMA at 40 MHz.
It retains SPI GPIO11 MOSI and GPIO12 SCK, GPIO9 CS, GPIO8 DC, GPIO7 reset,
and GPIO14 backlight held high. Initialization matches the Adafruit library's
`init(172, 320)` with rotation 3 (320x172, how U5 is mounted): MADCTL MX|MV,
inversion on and the 172 px glass centred in the 240 px controller RAM, as in the
[Adafruit guide](https://learn.adafruit.com/adafruit-1-47-round-rectangle-tft-display/arduino-wiring-test). Orientation, centring and
colours are verified on the first [PCB](../hardware/README.md).

Rendering runs from `loop()`, never from `audioTask`. Home and Menu redraw on
state changes, at most once every 100 ms; temporary parameter text expires
after 1100 ms. The menu shows a gold title, three rows of large text with the
value on the right (boxed between arrows while editing), a scroll bar, the
controller's footer and the BACK key. Text uses Teko SemiBold (SIL OFL 1.1)
as Adafruit GFX fonts with cap heights of 10, 14, 21 and 26 px, in
`firmware/src/display/fonts/`; each text takes the largest that fits, and a
pot name too long for one line puts its first word small above the rest
(`ECHO` / `FEEDBACK`). `python firmware/tools/make_display_fonts.py`
regenerates them from a pinned, hash-checked copy of the font. The glass has
3 mm rounded corners, so the layout keeps 8 px side margins. Each frame is drawn in a 110 kB `GFXcanvas16` and sent with one
`drawRGBBitmap`, so the panel never shows a cleared screen; if the buffer
cannot be allocated, drawing falls back to the panel directly. No PSRAM is
required. `displayRenderUs` and `maxDisplayRenderUs` remain available in STATUS
and include the transfer. The desktop panel draws text with Tk; its existing
control callbacks manage parameter overlays.

On the PCB, U5 is the Adafruit 5393 SPI display.

## Menu performance (#10)

Il click da Home apre COMANDI, SUONO, MODULAZIONE, ECO, TEMPO, INDIETRO. Tre
righe visibili con scroll. SUONO mostra il suono del Trigger oppure, se il
pulsante selezionato è un pad Play Sound, TASTO e SUONO; seguono per
ogni voce i parametri applicabili (PITCH, DECAY, SWEEP e TEMPO SWEEP, TONO,
CONTOUR e TEMPO CONTOUR) e, per un pad, SALVA e RICHIAMA. MODULAZIONE mostra la
zona RATE quando Rate è a zone, poi FORMA (la prima forma è SIREN), MODULAZIONE,
RATE in Hz solo quando Rate non è a zone, e AMPIEZZA e RETRIGGER solo per le
forme diverse da SIREN e MANUAL; le voci che non usano il LFO non mostrano
queste righe. ECO espone LIVELLO, RITARDO, FEEDBACK, FILTRO, COLORE e LOOP.
Con un clock in SYNC, RATE e RITARDO diventano DIVISIONE.

I suoni di un pad sono SINE 1, SINE 2, TWO TONE, SONAR, PING e GUNSHOT. I
parametri del suono corrente
restano al nuovo trigger dello stesso pad; cambiare suono applica i default.
Decay TAU indica la costante esponenziale, non il tempo al silenzio; a 0
seleziona il gate finito di 2 ms in attacco e 3 ms in rilascio.
Le modifiche sonore sono live e il click termina l'editing (DONE); il salvataggio
della patch è una funzione separata. Home mantiene Hold/Cut/Spin/Loop fuori
dall'area degli overlay. Leggibilità, memoria libera e tempo di invio del
frame ST7789 da verificare sull'hardware.

## Quattro memorie sonore (#11)

SOUND / SAVE PATCH salva la voce corrente nel pad selezionato; RECALL PATCH
scarta le modifiche live e ripristina la memoria. Disponibili anche
`PANEL SAVE 1..4` e `PANEL RECALL 1..4`. Le memorie appartengono ai pulsanti
con azione Play Sound: per gli altri pulsanti i comandi sono rifiutati. Un asterisco nel titolo SOUND indica
una patch assente o modificata. Il retrigger dello stesso pad conserva gli edit
live; passando a un altro pad si richiama la sua memoria salvata (se presente).
Tornando al primo si perdono gli edit non salvati. Nessun autosave sonoro:
i parametri restano live fino a SAVE PATCH. Famiglia e variante del pad sono
invece assegnazioni del pulsante e vengono salvate sia dalla Home (encoder)
sia dalla pagina SUONO, con la stessa attesa di 2 s prima
della scrittura NVS.

La patch contiene voce/variante, Tune, Rate, Depth, Decay, Sweep, parametri
Lickshot, Tone/Contour, Syndrum, modulazione e retrigger. Echo e Master
restano globali; Recall non cancella il delay. Gli eventi audio
catturano l'intero stato sonoro.

Schema 6 (senza le impostazioni del live pitch): un layout salvato con uno schema precedente viene scartato al
caricamento e valgono le impostazioni di fabbrica, con quattro memorie vuote.
Trigger, Hold, latch attivo, Loop e leva non fanno parte della patch.

STATUS include `settings.patches`, quattro vettori numerici versionati di 23
campi, definiti da `soundPatchValue` in PanelSettings.h. Un ripristino
usa `PANEL PATCH n <23 valori>` dentro PANEL BEGIN/COMMIT, validando il candidato
prima di applicarlo. La persistenza NVS fisica resta da verificare al riavvio.

## Tap tempo

La pagina TEMPO del menu contiene BPM, LFO A TEMPO ed ECO A TEMPO; le divisioni
stanno in MODULAZIONE ed ECO.
L'azione pulsante TAP TEMPO è assegnabile in SETUP.
Semantica, limiti e persistenza: [Tap tempo e sincronizzazione](tempo.md).
