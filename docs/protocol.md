# USB protocol

The device uses Arduino Hardware CDC / USB Serial-JTAG. Control traffic from PC
to ESP is ASCII, newline-delimited. ESP-to-PC traffic is binary only; firmware
does not print diagnostic text into the PCM stream.

## PC to ESP

Connection commands:

```text
HELLO
STREAM 1
STREAM 0
LOCAL_AUDIO 0|1
SET OUTPUT PC|GPIO|BOTH
```

`STREAM` enables only USB AUDIO packets; `LOCAL_AUDIO` independently enables
the PCM5102A I2S sink. OUTPUT selects which enabled sinks receive the same
rendered PCM block: PC, GPIO or BOTH. USB commands and STATUS remain available
in every output mode, including when both audio sinks are disabled.

Control commands:

```text
SET MODE SINE1|SINE2|TEST_TONE|SQUARE|SAW|TRIANGLE|LICKSHOT|SONAR|SYNDRUM|GUNSHOT
SET PROFILE CLASSIC|EXTENDED
SET CLASSIC_PITCH LOW|MID|HIGH
SET CLASSIC_MOD SLOW|MID|MEDIUM|FAST|MANUAL
SET PITCH_POSITION <0..1>
SET RATE_POSITION <0..1>
SET ECHO_FILTER <-1..1>
SET TUNE_HZ <30..9000>
SET LFO_SHAPE CLASSIC|TRIANGLE|SQUARE|SAW_UP|SAW_DOWN|ASYM_UP|ASYM_DOWN|PULSE_25|PULSE_75|MANUAL
SET LFO_RATE_HZ <0.05..20>
SET LFO_DEPTH_OCT <0..2>
SET DECAY_MS <0..3000>
SET SWEEP <0..1>
SET SWEEP_MS <50..3000>
SET SYNDRUM_WAVE SINE|TRIANGLE|SQUARE
SET SYNDRUM_SWEEP <-3..3>
SET DELAY_MS <50..1000>
SET FEEDBACK <0..1.05>
SET ECHO_LEVEL <0..1>
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
PAD_CONFIG <1..4> CLASSIC|TWO_TONE|LICKSHOT|PING <variant-index> 0|1
PANEL_RESET
```

Configurable-panel commands (additive in firmware 0.9.0):

```text
POT <1..6> <0..1>
PANEL BEHAVIOR PICKUP|JUMP
PANEL POT CLASSIC|EXTENDED <1..6> TARGET NONE|PITCH|RATE|LFO_DEPTH|ECHO_LEVEL|ECHO_TIME|ECHO_FEEDBACK|ECHO_FILTER|DECAY|SWEEP_AMOUNT|SWEEP_TIME
PANEL POT CLASSIC|EXTENDED <1..6> MODE CONTINUOUS|ZONES
PANEL POT CLASSIC|EXTENDED <1..6> RANGE <minimum> <maximum>
PANEL POT CLASSIC|EXTENDED <1..6> ZONES <low> <middle> <high>
PANEL BUTTON CLASSIC|EXTENDED <1..4> ACTION NONE|TRIGGER|PLAY_SOUND|HOLD|MOD_UP|MOD_DOWN|ECHO_CUT|ECHO_LOOP|ECHO_SPIN
PANEL BUTTON CLASSIC|EXTENDED <1..4> SOUND CLASSIC|TWO_TONE|LICKSHOT|PING <variant-index> <latch:0|1>
PANEL ENCODER OFF|LIVE_PITCH <step-semitones> <range-semitones>
PANEL RESET_DEFAULTS
PANEL BEGIN|COMMIT|CANCEL
```

`POT` is the raw normalized front-pot input. Assignment, zone/continuous
mapping, musical ranges and Pickup are applied only by the shared C++
controller. Configuration commands validate a complete candidate before
mutation; invalid ranges, unsupported profile targets and invalid sound/latch
combinations are rejected atomically. Existing semantic `SET` commands remain
available for development and cause matching Pickup latches to re-arm.
VirtualESP wraps a restored `panel.json` in `PANEL BEGIN` / `PANEL COMMIT` and
sends `PANEL CANCEL` on any rejected field, so a damaged file cannot partially
replace the running configuration.

The encoder menu opens on release of an unconsumed click in both profiles.
While a physical Trigger or Play Sound button is held, Home rotation applies
the configured transient live transpose (default one semitone per detent,
clamped to +/-12); releasing the final relevant button resets it to zero.
Extended BTN1+rotation takes precedence and edits Depth by 0.05 octaves,
clamped to 0-2, without changing base Tune or live transpose. Free rotation
in Extended cycles the selected pad's variant, leaving Depth unchanged.

`PAD`, `ENCODER_SWITCH`, and `ENCODER_ROTATE` are raw front-panel actions
interpreted by the shared C++ `PerformanceController`. Classic is the default:
PAD 1 is Trigger, PAD 2 toggles Hold on press, PAD 3 is Manual Mod, PAD 4 is
Echo Cut. Home rotation cycles SINE1/SINE2/TEST_TONE/SQUARE; push opens Menu.
The Classic menu offers Extended or Back. Extended retains the old pad bank,
priority/combination gestures, with a return to Classic. Only a physically
held, unconsumed BTN1 enables its Depth rotation gesture; latched Hold does not.
`PANEL_RESET` releases momentaries and preserves intentional Hold and delay.
`PAD_CONFIG` stores Extended pad assignments without activating Extended.
Profile switching releases momentaries and Hold, preserving echo contents.

In Extended, the controller stores a monotonically increasing activation sequence and a
small circular queue of the configurations that caused recent attacks. The
audio task therefore sees distinct rapid attacks even when another pad kept the
gate high, or when press/release pairs arrived between two block snapshots.
Returning to a previously held pad does not enqueue an attack.

Development A/B command (Extended only; Classic always uses the baseline):

```text
SET VOICING LEGACY|V2
```

These limits are absolute DSP/protocol ranges. PITCH_POSITION and RATE_POSITION
are additive normalized inputs, quantized with 0.015 hysteresis in Classic.
In Extended they map to continuous 80-1600 Hz and 0.15-20 Hz. Direct enum
commands remain supported; MEDIUM is retained as an alias for MID. SAW and
TRIANGLE are Extended-only tonal carriers; they use the normal Tune, LFO,
Dynamic Sweep, live transpose, gate and echo paths.
ECHO_FILTER is an atomic bipolar macro with a central FLAT bypass; explicit
HPF_HZ/LPF_HZ commands disable that bypass. Other controls retain physical units.
`SET MODE` rejects Extended-only modes while in Classic; select Extended first. Existing finite numeric controls are clamped to their absolute
ranges. The additive 0.6.0 controls SWEEP, SWEEP_MS and SYNDRUM_SWEEP reject
out-of-range values without changing `ControlState`. Malformed and unknown
commands are ignored. Parsing occurs outside the audio task.

SWEEP defaults to 0 and SWEEP_MS to 650 ms. On Extended tonal modes, a TRIGGER or HOLD
rising edge begins below the selected pitch and LFO rate and converges to them.
SYNDRUM and GUNSHOT instead respond only to a TRIGGER rising edge and use
DECAY_MS as their autonomous amplitude envelope; keeping TRIGGER high or using
HOLD does not sustain them. SYNDRUM_SWEEP is signed: positive starts above Tune
and descends, negative starts below and rises. The normal LFO, DIVE/RISE and
global Dynamic Sweep do not affect either transient mode.

The new booleans default to `MODULATION 1`, `RETRIGGER 0`, `ECHO_SPIN 0` and
`ECHO_LOOP 0`. MODULATION disables only the audible LFO contribution; the LFO
phase keeps advancing. In CLASSIC it is intentionally ignored so the original
profile sound stays active. `SET MODE LICKSHOT` plus `MODULATION 0` is the
airhorn configuration. RETRIGGER resets LFO state on a rising TRIGGER or HOLD
edge without resetting oscillator phase, filters or delay. During automatic
LFO shapes, MOD_UP and MOD_DOWN are RISE (+1 octave) and DIVE (-1 octave);
MANUAL retains the original direct LFO behavior.

In Extended, ECHO_SPIN temporarily targets feedback 1.05 without changing the stored
FEEDBACK value. ECHO_LOOP takes precedence and recirculates existing ring-buffer
audio without acquiring dry input or applying feedback to the write. ECHO_CUT
continues to mute only wet output.

The boot profile is CLASSIC. Pitch 261.63/392.00/587.33 Hz and automatic rates 0.35/0.70/2.80 Hz
are project calibration values.
Classic uses fixed depth, a finite 2 ms attack / 3 ms release anti-click gate,
and ignores Extended decay, sweep, legacy voicing, automatic bend and retrigger.
Manual stops the automatic LFO and connects its gate to the manual button.
ECHO_LEVEL zero excludes wet at the next audio snapshot; ECHO_CUT ramps the
wet gain down/up over 2 ms. The buffer continues evolving. Spin and Loop are ignored
while Classic is active; their stored Extended settings are retained. Detailed behavior and
source references: [controls](controls.md).

## ESP to PC

Every packet begins with this packed, 10-byte little-endian header:

| Offset | Field | Type | Value |
|---:|---|---|---|
| 0 | magic | 2 bytes | `DS` |
| 2 | version | uint8 | `1` |
| 3 | type | uint8 | AUDIO `0x01`, STATUS `0x02` |
| 4 | payload length | uint16 | bytes |
| 6 | sequence | uint32 | increments per packet type |

An AUDIO payload is always 960 bytes: 480 signed int16 mono PCM samples at
48,000 Hz. A STATUS payload is UTF-8 JSON. `HELLO` currently returns:

```json
{"name":"DubSiren","protocol":1,"sampleRate":48000,"blockSamples":480,"firmware":"0.9.0","output":"BOTH","i2sReady":true,"panel":{"revision":1,"settings":{"schema":2,"potBehavior":"PICKUP"},"persistenceStatus":"","display":{"width":320,"height":172,"mode":"HOME","menuIndex":0,"editing":false,"title":"PERFORMANCE","rows":[{"key":"PROFILE","label":"PROFILE","value":"CLASSIC"}],"profile":"CLASSIC","voice":"SINE 1","pitch":"MID","pitchMode":"ZONES","rate":"MID","rateMode":"ZONES","tuneHz":392.0,"rateHz":0.7,"liveTranspose":0}}}
```

Timing zeros above are placeholders, not measured performance. `renderUs` is the
most recent 480-sample render time and `maxRenderUs` is the
maximum observed since boot. `cycleUs` and `maxCycleUs` include selected output
writes. `maxUsbWriteUs` and `maxI2sWriteUs` isolate the maximum time spent in
each output write. `displayRenderUs` and `maxDisplayRenderUs` measure complete
ST7789 text redraws independently from the audio counters. `output` reports the
active routing selection and `i2sReady` reports
whether I2S initialization succeeded. `panel` reports controller-owned runtime
and settings state; the example intentionally abbreviates timing fields,
profiles, pots, buttons and menu rows. `panel.display` is logical state, not pixels: it reports the fixed
dimensions, Home/Menu mode, menu cursor/edit state, ordered row keys, current
Depth, Echo Loop, profile, voice, confirmed pitch/rate labels, Hold, Echo Cut
and Extended tuneHz/rateHz. Firmware sends an updated STATUS packet when controller
or menu state changes and at least once per second after HELLO while USB has
space available. Failed periodic sends are retried no faster than every 200 ms.
These are additive status/command changes: existing
commands, packet framing, and protocol version 1 remain compatible.

In firmware 0.9.0, `panel.settings` contains the complete schema-2 persistent
panel configuration and its revision. `panel.display.rows` contains ordered
objects with controller-owned `key`, `label` and already-formatted `value`,
plus `title`, `footer` and `overlay`. Renderers consume these strings rather
than reconstructing menu presentation. Effective pitch/rate/echo/master values
and `liveTranspose` are included for connection reconstruction.
`panel.persistenceStatus` is non-empty when NVS loading or saving failed.

There is no CRC because USB is the reliable transport. The Python parser scans
again for `DS` after invalid data or lost framing and rejects AUDIO payloads of
the wrong length before waiting for their payload. STATUS payloads must fit
the firmware's 8192-byte packet buffer (8182 payload bytes). Invalid UTF-8,
JSON and non-object status payloads are discarded without stopping the reader.

## Revisione uso autonomo (#12)

`STREAM 0|1` controlla ora solo il PCM USB. `LOCAL_AUDIO 0|1` controlla
l'abilitazione locale (default ON); la destinazione OUTPUT resta applicabile.
Stop Audio/Disconnect del client inviano entrambi i comandi di disabilitazione,
silenziano USB e I2S, e lasciano il DSP attivo. Connect mantiene le uscite mute
fino a Start Audio. La rimozione improvvisa di USB rilascia i comandi remoti
momentanei, conserva quelli fisici e non cambia l'abilitazione locale.
Le nuove metriche e la configurazione del pannello sono in [standalone](standalone.md).

## Tap tempo

I comandi `TAP_TEMPO` e `TEMPO` e il campo `panel.display.tempo` sono descritti
in [Tap tempo e sincronizzazione](tempo.md). Framing invariato, versione 1.
