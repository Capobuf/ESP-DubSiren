# Controls

The firmware and the Android app share one panel layout with
options. The factory layout plays four basic voices (SINE 1, SINE 2, TEST TONE,
SQUARE) with zoned Pitch and Rate; every other voice, pot target, button action
and sound option is reached from the encoder and its menu.

## Functional behavior and calibration

The factory layout provides three pitch positions, three automatic rates plus
manual, momentary/continuous triggering, 50–1000 ms echo, feedback up to
oscillation, a combined HP/LP filter and temporary wet mute. SINE 1 and SINE 2
are filtered pulse-derived voices; their names do not imply mathematically pure
sines. SINE 2 uses a narrower pulse and a higher filter cutoff. TEST TONE is
gated by the LFO; SQUARE alternates high/low pitch. Manual stops the automatic
LFO.

The LFO shapes SIREN and MANUAL use a fixed sweep instead of Depth. Pitch
follows one level between 0 and 1, linear in Hz:
`frequency = pitch * (0.12 + 0.88 * level)`. The selected pitch is therefore
the top of the sweep. With SIREN the level charges for 82% of the cycle and
discharges quickly in the rest, giving a rising sweep with a sharp drop (about
0.53-0.94 of the selected pitch once settled). Every new Trigger or Hold
activation restarts the cycle from level 0.15, also with tempo sync, so each
press opens with a rising sweep. MANUAL is either the stored shape or the
MANUAL zone of a zoned Rate control, which overrides the stored shape. In
MANUAL a Trigger or Hold activation raises the level to at least 0.85; Mod Up
pulls it to 1 with a 120 ms time constant and holds it there; with nothing held
it sinks with a 3.5 s time constant; Mod Down pushes it to the floor with the
same 120 ms constant.

The other shapes (TRIANGLE, SQUARE, SAW UP, SAW DOWN, ASYM UP, ASYM DOWN,
PULSE 25, PULSE 75) modulate the pitch symmetrically by Depth (0-2 octaves),
honour Retrigger and turn Mod Down into a -1 octave bend. A MOD UP button taps
the tempo outside MANUAL (see BTN3 below); the +1 octave bend remains for
the `MOD_UP` command.

These are this project's implemented behaviors and tuning values. No matched
hardware A/B recording is included, so sonic equivalence is unverified.

| Selector | Current calibration |
|---|---|
| PITCH LOW / MID / HIGH | 330 / 680 / 1000 Hz (top of the sweep) |
| RATE SLOW / MID / FAST | 0.70 / 3.00 / 6.50 Hz |
| RATE MANUAL | automatic phase stopped; Mod Up pulls the pitch up, release lets it sink |
| SIREN / MANUAL sweep | fixed, down to 0.12 of the selected pitch at most |
| SQUARE interval (SIREN / MANUAL) | selected pitch and 0.62 of it |

Pitch, rate, depth and harmonic balance are project calibration values.
Their suitability for performance still requires level-matched listening.

Musical calibration, voice defaults and DSP safety constants live in
`firmware/src/audio/PerformanceConfig.h`.

## Factory panel layout

| Control | Function |
|---|---|
| POT1 | PITCH: three approximately equal zones, LOW / MID / HIGH |
| POT2 | RATE: four approximately equal zones, SLOW / MID / FAST / MANUAL |
| POT3 | ECHO LEVEL: wet gain, 0-100% |
| POT4 | ECHO TIME: 50-1000 ms, logarithmic travel (midpoint about 224 ms) |
| POT5 | ECHO FEEDBACK: 0-1.05, protected feedback |
| POT6 | ECHO FILTER: LP <- FLAT -> HP |
| BTN1 | momentary TRIGGER |
| BTN2 | HOLD toggle on press; release and keyboard repeat do not toggle |
| BTN3 | MOD UP in MANUAL: pitch up while held and slow fall after release. With any other shape it is TAP TEMPO: a free echo follows the tapped tempo ([tempo](tempo.md)) |
| BTN4 | momentary ECHO CUT; BACK while the menu is open |
| Encoder | previous/next Trigger voice, one step per detent, wraps |
| Encoder push | MENU |
| Rear MASTER | final output gain |

SETUP reassigns every pot and button. A pot takes NONE, PITCH, RATE, LFO DEPTH,
ECHO LEVEL, ECHO TIME, ECHO FEEDBACK, ECHO FILTER, DECAY, SWEEP AMOUNT or SWEEP
TIME; Pitch and Rate choose between ZONES and CONTINUOUS (factory continuous
ranges 80-1600 Hz and 0.15-20 Hz). A button takes NONE, TRIGGER, PLAY SOUND,
HOLD, MOD UP, MOD DOWN, ECHO CUT, ECHO LOOP, ECHO SPIN or TAP TEMPO.

Pitch/Rate quantization and hysteresis run in the shared C++ controller, not
Python. Boundaries are 1/3 and 2/3, or 1/4, 1/2 and 3/4. Switching upward
requires crossing the boundary by 0.015 normalized travel, and downward by
0.015 in the opposite direction (about 61 ADC counts on a 12-bit input).
An ADC frontend can send `adc / 4095.0`; this repository still receives panel
controls through commands and does not add GPIO acquisition in this revision.

The filter has a central +/-0.05 deadband in its -1..1 control range. FLAT bypasses
the wet HP/LP processing after a short transition. On the left, LP moves
logarithmically from 16 kHz to 400 Hz (HP 60 Hz); on the right, HP moves from
60 Hz to 4 kHz (LP 16 kHz). These are project values. Direct HPF/LPF commands
remain available for development and leave the FLAT bypass.

## Generator timing and voices

DECAY selects the amplitude envelope of the tonal voices. DECAY 0, the default,
is a finite linear anti-click gate: 2 ms attack and at most 3 ms release,
reaching exact zero (rounding tolerance: one sample). Any other value is the
time constant of an exponential tail after release. TRIGGER OR HOLD opens the
envelope. Choosing a voice sets DECAY to 0 for the sustained voices and to
120 ms for Lickshot, Ping and Gunshot. Queued sub-block taps are replayed: a
tap pressed and released between audio snapshots still opens the gate, for the
2 ms attack with DECAY 0 and for one 10 ms block otherwise. Commands are
applied at a 480-sample block boundary (up to 10 ms), plus output buffering:
the 3 ms bound describes DSP fade time, not total host-to-speaker latency.

Dynamic Sweep, dry Tone/Contour, Echo Color/Loop/Spin, the legacy voicing flag
and modulation on/off are honoured for every voice; their defaults leave them
off (modulation on).

- SINE 1: PolyBLEP pulse with modulation-dependent duty 0.27-0.45,
  one tracking low-pass, asymmetric saturation and DC blocking; output gain 0.56.
- SINE 2: the same stages with duty 0.19-0.29 and a
  different tracking cutoff; output gain 0.54. Both voices contain harmonics.
- SONAR: sine carrier, gain 0.82, with a decaying ping on every rising
  LFO gate. RATE controls repetition, with one ping per automatic LFO cycle.
  Each ping has an internal 80 ms exponential time constant, a 2 ms attack and
  a gate closing over 3 ms at the end of the active half-cycle, leaving silent
  gaps even while HOLD is active. Pitch modulation still applies (the fixed
  sweep with SIREN, DEPTH with the other shapes); disabling pitch modulation
  keeps the amplitude pulses. A new trigger, Hold activation or selection of
  Sonar starts the pulse cycle immediately, even with RETRIGGER off. MANUAL
  produces a ping on each MOD UP press. DECAY shapes the outer release after
  Trigger/Hold ends; echo can fill the gaps, so use ECHO LEVEL zero to hear
  the dry ping pattern.
- TEST TONE: fixed base-pitch sine beep; half-cycle automatic gate with the
  same 2/3 ms anti-click ramp. MANUAL gates the beep directly from MOD UP.
- SQUARE: PolyBLEP/one-pole voicing. With SIREN the LFO gate switches between
  the selected pitch and 0.62 of it, with a 2.5 ms pitch slew; MANUAL switches
  those two targets directly from MOD UP, never from a frozen automatic phase.
  With the other shapes the pitch follows the LFO value and Depth.
- SAW and TRIANGLE: continuous tonal carriers. SAW is a PolyBLEP ramp to limit
  the obvious high-frequency aliasing of a naive digital saw; TRIANGLE is a
  direct continuous triangle with naturally faster harmonic rolloff. Both use
  DC blocking and the normal pitch/modulation/gate/echo chain.
- LICKSHOT, PING (Syndrum) and GUNSHOT are described below; Ping and Gunshot
  are one-shots that respond only to a trigger, not to Hold.

MANUAL does not advance the automatic LFO.
Stored pot zones keep earlier pitch and rate values until Reset Defaults.
All gains, harmonic proportions and RC timings require listening calibration.

## Independent echo path

The generator branches to dry output and the existing fractional delay.
The final mix is `dry + wet * level * cut`, followed by Master and the existing
safety limiter. Echo never controls the dry gate.

- ECHO LEVEL = 0 excludes wet immediately, including a previously filled buffer.
- FEEDBACK = 0 writes no feedback and permits one repetition.
- Below 100%, feedback makes the repeats decay; higher values produce longer
  tails. Saturation compensates its gain and the delay write limiter's gain,
  and the wet HP/LP filters have no resonant boost. Above 100% (or with
  SPIN), protected self-oscillation remains available. ECHO LOOP deliberately
  sustains the buffer independently of feedback.
- For a short tail, start with ECHO TIME 150-250 ms and FEEDBACK 30-50%.
  ECHO LEVEL controls volume, not tail duration. A pot applies its value as
  soon as it moves.
- ECHO CUT ramps wet gain to zero over 2 ms after the next block snapshot;
  release also takes 2 ms. The ring buffer, filters and feedback continue
  evolving. No buffer is cleared. Perceived clicks still require listening.
- Delay time uses the existing fractional read, smoothing and 2000 ms/s slew.

## Voices, pads and encoder

At Home the encoder steps the Trigger voice through 18 voices, in this order:
SINE 1, SINE 2, TEST TONE, SQUARE, SONAR, SAW, TRIANGLE, TWO TONE TRIANGLE,
TWO TONE SQUARE, LICKSHOT (SAW DOWN, ASYM DOWN, SHOT, ROLL, HORN), PING (SINE,
TRIANGLE, SQUARE) and GUNSHOT. The same list is the VOICE row of the SOUND
menu. Selecting a voice applies its defaults: the sustained voices get the
SIREN shape and DECAY 0, the Two Tone voices their own shape with Retrigger on,
the one-shots a 120 ms DECAY.

A button assigned to PLAY SOUND is a sound pad with its own family, variant and
optional latch: SIREN (Sine 1/Sine 2/Sonar/Saw/Triangle), TWO TONE (Siren/
Triangle/Square), LICKSHOT, PING (Syndrum), GUNSHOT, TEST TONE. Last pad wins.
While the selected button is a pad, free encoder rotation cycles that pad's
variant instead of the Trigger voice, and encoder push plus a pad button is a
shortcut: BTN1 toggles the selected pad's latch, BTN2/BTN3 step its variant
down/up, BTN4 cuts the echo. The shortcuts apply only when the pressed button
is a pad.

Holding a Trigger or Play Sound button while turning the encoder is always the
live-pitch gesture. Depth is edited in the MOD menu or assigned to a pot.
Tone/Contour, articulation, Color and tempo controls are available through the
shared encoder menu and protocol.

### Two Tone (#7)

Le varianti Triangle e Square modulano il pitch con il valore continuo del LFO
scelto: Rate e Depth mantengono cadenza ed escursione nominale. Il carrier
resta quadrato. La variante Siren e la forma Manual commutano invece i due
livelli discreti (pitch selezionato e 0,62 del pitch); MODULATION 0 esclude la
modulazione.

### Lickshot articolato (#5)

Le varianti 0 SAW DOWN e 1 ASYM DOWN usano la sintesi con LFO.
Le varianti 2 SHOT, 3 ROLL e 4 HORN sono selezionabili con PAD_CONFIG
o dall'encoder. SHOT termina anche tenendo premuto; ROLL ripete a Rate Hz
finché gate/Hold sono attivi e rilascia in massimo 3 ms; HORN sostiene senza LFO.
Il pitch di SHOT/ROLL decade indipendentemente da Rate e dalla durata sonora.

Comandi: `SET LICKSHOT_MODE LEGACY|SHOT|ROLL|HORN`,
`SET LICKSHOT_LENGTH_MS 20..1000`, `SET LICKSHOT_PUNCH_OCT 0..3`,
`SET LICKSHOT_PITCH_MS 10..300`. Length è il tempo al silenzio, con attacco
di 2 ms; Punch è la caduta iniziale in ottave; Pitch Ms è la costante di tempo
della caduta. Default 150 ms, 2 ottave, 70 ms: tarature iniziali da ascoltare.
Le articolazioni SHOT/ROLL/HORN non usano il LFO per ampiezza/pitch e non azzerano l'echo.
HORN resta selezionato al nuovo colpo se richiamato come variante del pad.

Restano da verificare ascolto sul riddim e costo DSP massimo sull'ESP.

### Tone e Contour dry (#8)

`SET DRY_TONE_HZ 200..19000` imposta un passa-basso dry assoluto a due poli
(Q 0,7071). `SET FILTER_CONTOUR_OCT -4..4` sposta il cutoff al trigger e
`SET FILTER_CONTOUR_MS 10..3000` determina il ritorno esponenziale a Tone.
Il rilascio non riavvia o azzera Contour: prosegue durante la coda. Ogni colpo
ROLL lo riattiva. Quantità positiva apre il filtro all'attacco, negativa lo
chiude; il cutoff è limitato a 50..19000 Hz. Nessuna variazione intenzionale del pitch.

Tone 19000 e Contour zero escludono lo stadio (default); vale per ogni voce.
Il filtro usa Biquad già presente, aggiornamento coefficienti ogni millisecondo,
parametri smussati e crossfade del bypass; Sweep conserva il significato sul pitch.
Non è stato aggiunto gain per compensare automaticamente le armoniche rimosse:
la taratura comparata delle voci e l'ascolto restano da validare, insieme al
budget hardware e all'aliasing nelle combinazioni estreme.

### Echo Color e Cut (#9)

`SET ECHO_COLOR 0..1` riutilizza low-pass e saturazione del feedback.
Zero conserva la risposta neutra; aumentando Color si miscela il filtro nel
ritorno anche in FLAT, il limite delle alte scende fino a 1500 Hz e il drive
sale fino a tre volte il valore base, con compensazione della pendenza.
Non c'è guadagno aggiuntivo del loop sotto il 100%; Loop conserva la copia
del materiale, Spin mantiene il feedback temporaneamente elevato.

Echo Cut usa una rampa lineare di massimo 2 ms in entrambe le direzioni,
con zero esatto a fine taglio. Agisce solo sul ritorno: dry e memoria dell'echo
continuano. Nessuna risonanza o wow/flutter aggiunti; richiederebbero una
validazione separata. Ascolto dei tagli ritmici ancora da eseguire.
