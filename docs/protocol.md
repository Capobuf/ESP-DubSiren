# USB protocol

The device uses Arduino Hardware CDC / USB Serial-JTAG. The link carries
control and diagnostics only: audio leaves the device solely through I2S to the
PCM5102A, which is always enabled once initialized. Host-to-ESP traffic is
ASCII, newline-delimited. ESP-to-host traffic is binary framed packets; firmware
does not print diagnostic text into them.

## Host to ESP

Connection commands:

```text
HELLO
```

Protocol 2 removed USB PCM streaming together with `STREAM`, `LOCAL_AUDIO`
and `SET OUTPUT`; those commands are now rejected like any unknown command.

Control commands:

```text
SET MODE SINE1|SINE2|TEST_TONE|SQUARE|SAW|TRIANGLE|LICKSHOT|SONAR|SYNDRUM|GUNSHOT
SET CLASSIC_PITCH LOW|MID|HIGH
SET CLASSIC_MOD SLOW|MID|MEDIUM|FAST|MANUAL
SET PITCH_POSITION <0..1>
SET RATE_POSITION <0..1>
SET ECHO_FILTER <-1..1>
SET TUNE_HZ <30..9000>
SET LFO_SHAPE SIREN|TRIANGLE|SQUARE|SAW_UP|SAW_DOWN|ASYM_UP|ASYM_DOWN|PULSE_25|PULSE_75|MANUAL
SET LFO_RATE_HZ <0.05..20>
SET LFO_DEPTH_OCT <0..2>
SET DECAY_MS <0..3000>
SET SWEEP <0..1>
SET SWEEP_MS <50..3000>
SET SYNDRUM_WAVE SINE|TRIANGLE|SQUARE
SET SYNDRUM_SWEEP <-4..4>
SET LICKSHOT_MODE LEGACY|SHOT|ROLL|HORN
SET LICKSHOT_LENGTH_MS <20..1000>
SET LICKSHOT_PUNCH_OCT <0..3>
SET LICKSHOT_PITCH_MS <10..300>
SET DRY_TONE_HZ <200..19000>
SET FILTER_CONTOUR_OCT <-4..4>
SET FILTER_CONTOUR_MS <10..3000>
SET DELAY_MS <50..1000>
SET FEEDBACK <0..1.05>
SET ECHO_LEVEL <0..1>
SET ECHO_COLOR <0..1>
SET HPF_HZ <50..7000>
SET LPF_HZ <200..19000>
SET MASTER <0..1>
TRIGGER 0|1
HOLD 0|1
MOD_UP 0|1
MOD_DOWN 0|1
MODULATION 0|1
RETRIGGER 0|1
ECHO_CUT 0|1
ECHO_SPIN 0|1
ECHO_LOOP 0|1
```

Playable-panel commands (additive in firmware 0.8.0):

```text
PAD <1..4> 0|1
ENCODER_SWITCH 0|1
ENCODER_ROTATE PREV|NEXT
PAD_SELECT <1..4>
PAD_ASSIGN PREV|NEXT
PAD_VARIANT PREV|NEXT
PAD_HOLD_TOGGLE
PAD_CONFIG <1..4> SIREN|TWO_TONE|LICKSHOT|PING|GUNSHOT|TEST_TONE <variant-index> 0|1
PANEL_RESET
```

Configurable-panel commands (additive in firmware 0.9.0):

```text
POT <1..6> <0..1>
PANEL BEHAVIOR PICKUP|JUMP
PANEL POT <1..6> TARGET NONE|PITCH|RATE|LFO_DEPTH|ECHO_LEVEL|ECHO_TIME|ECHO_FEEDBACK|ECHO_FILTER|DECAY|SWEEP_AMOUNT|SWEEP_TIME
PANEL POT <1..6> MODE CONTINUOUS|ZONES
PANEL POT <1..6> RANGE <minimum> <maximum>
PANEL POT <1..6> ZONES <low> <middle> <high>
PANEL BUTTON <1..4> ACTION NONE|TRIGGER|PLAY_SOUND|HOLD|MOD_UP|MOD_DOWN|ECHO_CUT|ECHO_LOOP|ECHO_SPIN|TAP_TEMPO
PANEL BUTTON <1..4> SOUND SIREN|TWO_TONE|LICKSHOT|PING|GUNSHOT|TEST_TONE <variant-index> <latch:0|1>
PANEL ENCODER OFF|LIVE_PITCH <step-semitones> <range-semitones>
PANEL SAVE|RECALL <1..4>
PANEL PATCH <1..4> <23 values>
PANEL RESET_DEFAULTS
PANEL BEGIN|COMMIT|CANCEL
```

The sound family token `SIREN` and the LFO shape token `SIREN` are also
accepted as `CLASSIC`. Variant counts are SIREN 5, TWO_TONE 3, LICKSHOT 5,
PING 3, GUNSHOT 1 and TEST_TONE 1.

`POT` is the raw normalized front-pot input. Assignment, zone/continuous
mapping, musical ranges and Pickup are applied only by the shared C++
controller. Configuration commands validate a complete candidate before
mutation; invalid ranges, ZONES on a target other than Pitch or Rate and
invalid sound/latch combinations are rejected atomically. `PANEL POT ... TARGET`
resets that pot to the target's continuous default range. `PANEL BUTTON ...
SOUND`, `PAD_CONFIG`, `PANEL SAVE` and `PANEL RECALL` are rejected unless the
button's action is PLAY_SOUND. Existing semantic `SET` commands remain
available for development and cause matching Pickup latches to re-arm.
`PANEL BEGIN` stages the following configuration commands; `PANEL COMMIT`
validates and applies them together and `PANEL CANCEL` discards them, so a
rejected field cannot partially replace the running configuration.

The encoder menu opens on release of an unconsumed click.
While a physical Trigger or Play Sound button is held, Home rotation applies
the configured transient live transpose (default one semitone per detent,
clamped to +/-12); releasing the final relevant button resets it to zero.
Free rotation steps the selected pad's variant when the selected button is a
Play Sound pad, and the Trigger voice otherwise.

`PAD`, `ENCODER_SWITCH`, and `ENCODER_ROTATE` are raw front-panel actions
interpreted by the shared C++ `PerformanceController`. With the factory layout
PAD 1 is Trigger, PAD 2 toggles Hold on press, PAD 3 is Mod Up, PAD 4 is
Echo Cut. Home rotation steps the Trigger voice through 18 voices: SINE 1,
SINE 2, TEST TONE, SQUARE, SONAR, SAW, TRIANGLE, TWO TONE TRIANGLE, TWO TONE
SQUARE, LICKSHOT (5 variants), PING (3 variants), GUNSHOT; push opens Menu.
Encoder push plus a button is a shortcut only when the pressed button is a
Play Sound pad: PAD 1 toggles the selected pad's latch, PAD 2/3 step its
variant, PAD 4 cuts the echo.
`PANEL_RESET` releases momentaries and preserves intentional Hold and delay.
`TRIGGER`, `HOLD`, `MOD_UP`, `MOD_DOWN`, `ECHO_CUT` and `ECHO_SPIN` are kept as
remote states OR-ed with the panel buttons, so a later panel event does not
cancel them; `PANEL_RESET`, USB loss and host silence clear them.
`PAD_CONFIG` stores the family, variant and latch of a Play Sound button.

The controller stores a monotonically increasing activation sequence and a
small circular queue of the configurations that caused recent attacks. The
audio task therefore sees distinct rapid attacks even when another pad kept the
gate high, or when press/release pairs arrived between two block snapshots.
Returning to a previously held pad does not enqueue an attack.

Development A/B command (it selects the legacy TEST_TONE and SQUARE oscillators;
V2 is the default):

```text
SET VOICING LEGACY|V2
```

These limits are absolute DSP/protocol ranges. PITCH_POSITION and RATE_POSITION
are additive normalized inputs that follow the stored mode of the Pitch or
Rate control: with ZONES they are quantized with 0.015 hysteresis and select
the configured zone values; with CONTINUOUS they map to 80-1600 Hz and
0.15-20 Hz. Direct enum commands remain supported; MEDIUM is retained as an
alias for MID. `SET CLASSIC_MOD MANUAL` selects the MANUAL zone, which takes
effect while a pot is assigned to Rate in ZONES mode. SAW and TRIANGLE are
tonal carriers; they use the normal Tune, LFO, Dynamic Sweep, live transpose,
gate and echo paths.
ECHO_FILTER is an atomic bipolar macro with a central FLAT bypass; explicit
HPF_HZ/LPF_HZ commands disable that bypass. Other controls retain physical units.
`SET MODE` accepts every mode and changes only the oscillator: unlike choosing
a voice on the panel it leaves DECAY and LFO shape unchanged. The displayed
voice name follows the live mode and shape. Existing finite numeric controls are clamped to their absolute
ranges. The additive 0.6.0 controls SWEEP, SWEEP_MS and SYNDRUM_SWEEP reject
out-of-range values without changing `ControlState`. Malformed and unknown
commands are ignored. Parsing occurs outside the audio task.

SWEEP defaults to 0 and SWEEP_MS to 650 ms. On tonal modes, a TRIGGER or HOLD
rising edge begins below the selected pitch and LFO rate and converges to them.
DECAY_MS defaults to 0, which selects the finite 2 ms attack / 3 ms release
gate for tonal modes; any other value is the exponential release time constant.
SYNDRUM and GUNSHOT instead respond only to a TRIGGER rising edge and use
DECAY_MS (at least 20 ms) as their autonomous amplitude envelope; keeping TRIGGER high or using
HOLD does not sustain them. SYNDRUM_SWEEP is signed: positive starts above Tune
and descends, negative starts below and rises. The normal LFO, DIVE/RISE and
global Dynamic Sweep do not affect either transient mode.

The new booleans default to `MODULATION 1`, `RETRIGGER 0`, `ECHO_SPIN 0` and
`ECHO_LOOP 0`. MODULATION disables only the audible LFO contribution; the LFO
phase keeps advancing. `SET MODE LICKSHOT` plus `MODULATION 0` is the
airhorn configuration. RETRIGGER resets LFO state on a rising TRIGGER or HOLD
edge without resetting oscillator phase, filters or delay; it applies to the
shapes other than SIREN and MANUAL and is suspended while the LFO is tempo
synced. With those shapes, MOD_UP and MOD_DOWN are RISE (+1 octave) and DIVE
(-1 octave) and LFO_DEPTH_OCT sets the excursion.

ECHO_SPIN temporarily targets feedback 1.05 without changing the stored
FEEDBACK value. ECHO_LOOP takes precedence and recirculates existing ring-buffer
audio without acquiring dry input or applying feedback to the write. ECHO_CUT
continues to mute only wet output.

At boot the Trigger voice is SINE 1 with the SIREN shape and zoned Pitch and
Rate. Pitch 330/680/1000 Hz and automatic rates 0.70/3.00/6.50 Hz
are project calibration values.
The SIREN and MANUAL shapes use their own fixed pitch sweep (see controls.md)
and ignore depth and bend. SIREN restarts from level 0.15 on every TRIGGER or
HOLD rising edge, also with tempo sync. MANUAL, selected as the shape or by the
MANUAL zone of a zoned Rate control, stops the automatic LFO: a TRIGGER or HOLD
rising edge raises the level to at least 0.85, MOD_UP pulls it to the top and
connects the LFO gate, MOD_DOWN pushes it to the floor.
ECHO_LEVEL zero excludes wet at the next audio snapshot; ECHO_CUT ramps the
wet gain down/up over 2 ms. The buffer continues evolving. Detailed behavior and
source references: [controls](controls.md).

## ESP to host

Every packet begins with this packed, 10-byte little-endian header:

| Offset | Field | Type | Value |
|---:|---|---|---|
| 0 | magic | 2 bytes | `DS` |
| 2 | version | uint8 | `2` |
| 3 | type | uint8 | STATUS `0x02`, COMMAND_RESULT `0x03` (`0x01` was AUDIO up to protocol 1) |
| 4 | payload length | uint16 | bytes |
| 6 | sequence | uint32 | shared by STATUS and COMMAND_RESULT |

A STATUS payload is UTF-8 JSON. `HELLO` currently returns:

```json
{"name":"DubSiren","protocol":2,"sampleRate":48000,"blockSamples":480,"firmware":"0.9.0","i2sReady":true,"panel":{"revision":1,"settings":{"schema":3,"revision":1,"potBehavior":"PICKUP","encoder":{"holdTurn":"LIVE_PITCH","step":1,"range":12},"pots":[{"target":"PITCH","mode":"ZONES","min":80.00000,"max":1600.00000,"zones":[330.00000,680.00000,1000.00000]}],"buttons":[{"action":"TRIGGER","sound":"SIREN","variant":0,"latch":false}],"patches":[]},"persistenceStatus":"","display":{"width":320,"height":172,"mode":"HOME","menuIndex":0,"editing":false,"title":"PERFORMANCE","rows":[{"key":"SOUND","label":"SOUND","value":""}],"buttons":["TRIGGER","HOLD","MOD UP","ECHO CUT"],"padSelected":false,"voice":"SINE 1","pitch":"MID","pitchMode":"ZONES","rate":"MID","rateMode":"ZONES","tuneHz":680.0000,"rateHz":3.0000,"liveTranspose":0}}}
```

`COMMAND_RESULT` has a 2–512 byte payload: byte 0 is `1` (accepted) or `0`
(rejected), followed by the original trimmed ASCII command, without newline.
Every non-empty command of at most 511 bytes receives a result except `HELLO`,
which returns STATUS. Oversize lines are discarded. Acceptance means the
controller accepted the operation; clamping, pickup and tempo sync can still
determine the effective value, reported separately in `panel.display`.
Firmware retains one pending result and retries the non-blocking USB write
before consuming the next command. Disconnect clears that pending reply.
Audio rendering continues independently. Rejection also requests a fresh STATUS;
periodic STATUS retries cover a full USB buffer.

Timing zeros above are placeholders, not measured performance. `renderUs` is the
most recent 480-sample render time and `maxRenderUs` is the
maximum observed since boot. `cycleUs` and `maxCycleUs` include the I2S
write, whose maximum alone is `maxI2sWriteUs`. `displayRenderUs` and
`maxDisplayRenderUs` measure complete ST7789 redraws independently from the
audio counters. `i2sReady` reports whether I2S initialization succeeded. `panel` reports controller-owned runtime
and settings state; the example intentionally omits timing and other fields
and shortens `pots` (six entries), `buttons` (four), `patches` (four) and the
menu rows. `panel.display` is logical state, not pixels: it reports the fixed
dimensions, Home/Menu mode, menu cursor/edit state, ordered row keys, current
Depth, Echo Loop, the four button actions, `padSelected` (the selected button
is a Play Sound pad), `voice` (the Trigger voice name), confirmed pitch/rate
labels and modes, Hold, Echo Cut and tuneHz/rateHz. Firmware sends an updated STATUS packet when controller
or menu state changes, at most every 200 ms, and at least once per second after
HELLO while USB has space available; accepted commands no longer trigger an
immediate STATUS. Failed periodic sends are retried no faster than every 200 ms.

In firmware 0.9.0, `panel.settings` contains the complete schema-3 persistent
panel configuration and its revision: `potBehavior`, `encoder`, and `pots`,
`buttons` and `patches` at top level. A configuration stored by another schema
is discarded at load and the factory settings apply. `panel.display.rows` contains ordered
objects with controller-owned `key`, `label` and already-formatted `value`,
plus `title`, `footer` and `overlay`. Renderers consume these strings rather
than reconstructing menu presentation. Effective pitch/rate/echo/master values
and `liveTranspose` are included for connection reconstruction.
`panel.persistenceStatus` is non-empty when NVS loading or saving failed.
`panel.deviceStatus` separately reports I2S, panel-input or audio-task errors.
NVS is written once the settings revision has been stable for 2 s; a failed
write is retried after another 2 s.

There is no CRC because USB is the reliable transport. A reader should scan
again for `DS` after invalid data or lost framing. STATUS payloads must fit
the firmware's 8192-byte packet buffer (8182 payload bytes).

## Revisione uso autonomo (#12)

La rimozione improvvisa di USB rilascia i comandi remoti momentanei e conserva
quelli fisici.
Lo stesso rilascio avviene se un host collegato non invia byte per 3 s (app
terminata con il cavo inserito): un host che vuole restare collegato invia una
riga vuota ogni secondo come keepalive, ignorata dal firmware e senza
COMMAND_RESULT.
Le nuove metriche e la configurazione del pannello sono in [standalone](standalone.md).

## Tap tempo

I comandi `TAP_TEMPO` e `TEMPO` e il campo `panel.display.tempo` sono descritti
in [Tap tempo e sincronizzazione](tempo.md).
