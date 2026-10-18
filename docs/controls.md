# Controls: DS01E / Classic baseline

Classic is the boot profile in the firmware, Virtual ESP and both desktop UIs.
The primary generator has four voicings: SINE 1, SINE 2, TEST TONE, SQUARE.
Extended remains available from the encoder menu and the debug UI (`pc/app.py`).

## Functional reference and calibration

Official sources consulted on 2026-09-29:

- [Benidub DS01E](https://benidub.com/products/ds01e-dub-siren/) and
  [DS01E manual](https://benidub.com/wp-content/uploads/2023/07/Benidub-DS01E-Dubsiren-User-manual.pdf):
  three pitch positions, three automatic rates plus manual, momentary/continuous
  triggering, 50-1000 ms echo, feedback from one repeat to oscillation, combined
  HP/LP filter, and a button that temporarily mutes the echo output.
- [Benidub DS01](https://benidub.com/products/ds01-dub-siren/) and
  [DS01 manual](https://benidub.com/wp-content/uploads/2022/03/Benidub-DS01-User-manual.pdf):
  Sine 1 is the rounder voice, Sine 2 has more bite, Test Tone is gated by the
  LFO, Square alternates high/low pitch; manual stops the automatic LFO.

The consulted official material does **not** specify numeric pitch/rate presets,
wave spectra, modulation depth, gate timing or filter calibration. No hardware
A/B measurement was performed. These are functional references, not evidence
of circuit or sonic identity. The single-button manual gesture and bipolar
filter calibration below are project implementation choices.

Additional waveform research checked on 2026-10-04:

- [ZEN Instruments' NJD redraw](https://zeninstruments.blogspot.com/2022/04/njd-dub-siren-schematic-redraw-and-pcb.html)
  describes the classic NJD audio oscillator as a variable-pulse-width square
  oscillator followed by cascaded low-pass stages. Keeping the DS01-inspired
  Classic voicings pulse-derived is a project choice informed by this NJD
  reference, not evidence of the Benidub circuit or literal sine oscillators.
- [Benidub DS71](https://benidub.com/products/ds71-dub-siren/) documents eight
  preset **LFO** waveforms but does not identify them as alternative carrier
  waveforms, so they are not copied into the audio oscillator.
- [Zongo Dub Siren](https://siren.zongosound.com/) exposes the conventional
  Sine/Square/Triangle/Sawtooth carrier set. Triangle and Saw are therefore
  added only to Extended as useful, distinct project waveforms; this is not a
  claim that either waveform exists in the DS01/DS01E circuit.

| Selector | Current Classic calibration |
|---|---|
| PITCH LOW / MID / HIGH | 261.63 / 392.00 / 587.33 Hz |
| RATE SLOW / MID / FAST | 0.35 / 0.70 / 2.80 Hz |
| RATE MANUAL | automatic phase stopped; BTN3 controls manual modulation |
| Depth | 1 octave either side of base pitch |

The pitch presets reflect the completed DS01E audio/video analysis. Rate and
depth remain project values because that analysis did not support replacing
them numerically.

Musical calibration, Extended defaults and DSP safety constants live in
`firmware/src/audio/PerformanceConfig.h`.

## Primary panel

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
| BTN3 | momentary MANUAL MOD; no additional bend in automatic Classic |
| BTN4 | momentary ECHO CUT |
| Encoder | previous/next Classic voicing, one step per detent, wraps |
| Encoder push | MENU; Extended and Back |
| Rear MASTER | final output gain |

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
remain available in Extended/debug and leave the FLAT bypass.

## Generator timing and voices

Classic uses a finite linear anti-click gate: 2 ms attack and at most 3 ms
release, reaching exact zero (rounding tolerance: one sample). TRIGGER OR HOLD
opens it. DECAY, Dynamic Sweep, legacy voicing selection, automatic RISE/DIVE
retrigger, Spin and Loop cannot alter this baseline. Classic does not replay queued
sub-block taps after release; taps wholly between audio snapshots may be
missed. Commands are applied at a 480-sample block boundary (up to 10 ms),
plus output buffering: the 3 ms bound describes DSP fade time, not total
host-to-speaker latency.

- SINE 1 (Classic and Extended): PolyBLEP pulse with modulation-dependent duty 0.27-0.45,
  one tracking low-pass, asymmetric saturation and DC blocking; output gain 0.56.
- SINE 2 (Classic and Extended): the same stages with duty 0.19-0.29 and a
  different tracking cutoff; output gain 0.54. Both voices contain harmonics.
  Both profiles call the same oscillator code, preserving the Classic timbres;
  profile differences remain in controls and the outer release envelope.
- Extended SONAR: sine carrier, gain 0.82, with a decaying ping on every rising
  LFO gate. RATE controls repetition, with one ping per automatic LFO cycle.
  Each ping has an internal 80 ms exponential time constant, a 2 ms attack and
  a gate closing over 3 ms at the end of the active half-cycle, leaving silent
  gaps even while HOLD is active. Pitch modulation and DEPTH still apply;
  disabling pitch modulation keeps the amplitude pulses. A new trigger, Hold
  activation or selection of Sonar starts the pulse cycle immediately, even
  with RETRIGGER off. Manual produces a ping on each MOD UP press.
  DECAY retains its meaning as the outer release after Trigger/Hold ends;
  echo can fill the gaps, so use ECHO LEVEL zero to hear the dry ping pattern.
- TEST TONE: fixed base-pitch sine beep; half-cycle automatic gate with the
  same 2/3 ms anti-click ramp. Manual gates the beep directly from BTN3.
- SQUARE: PolyBLEP/one-pole voicing; automatic high/low pitch uses the LFO gate
  and a 2.5 ms pitch slew. Manual switches those two targets directly from
  BTN3, never from a frozen automatic phase.

Sine voices retain the project's RC-like automatic pitch modulation. Manual
uses an 8 ms attack / 12 ms release RC response for their pitch, without
advancing the automatic LFO. Button up returns to the base pitch.
All gains, harmonic proportions and RC timings require listening calibration.

## Independent echo path

The generator branches to dry output and the existing fractional delay.
The final mix is `dry + wet * level * cut`, followed by Master and the existing
safety limiter. Echo never controls the dry gate.

- ECHO LEVEL = 0 excludes wet immediately, including a previously filled buffer.
- FEEDBACK = 0 writes no feedback and permits one repetition.
- Below 100%, feedback makes the repeats decay; higher values produce longer
  tails. Saturation compensates its gain and the delay write limiter's gain,
  and the wet HP/LP filters have no resonant boost. Above 100% (or Extended
  SPIN), protected self-oscillation remains available. ECHO LOOP deliberately
  sustains the buffer independently of feedback.
- For a short tail, start with ECHO TIME 150-250 ms and FEEDBACK 30-50%.
  ECHO LEVEL controls volume, not tail duration. With Pickup, follow the UP/DOWN
  indication until the pot reaches the stored value, then lower it; while
  PICKUP is shown the stored feedback is unchanged.
- ECHO CUT immediately mutes wet at the next block snapshot; release is smoothed.
  The ring buffer, filters and feedback continue evolving. No buffer is cleared.
  Abruptly cutting a loud wet waveform may click: exact mute takes precedence.
- Delay time uses the existing fractional read, smoothing and 2000 ms/s slew.

## Extended

Extended preserves Lickshot, Sonar, Syndrum, Gunshot, Dynamic Sweep, extra LFO
shapes, continuous Tune/Rate, Depth, Decay, RISE/DIVE, Retrigger, Spin and Loop.
It also adds SAW and TRIANGLE as continuous tonal carriers. SAW is a PolyBLEP
ramp to limit the obvious high-frequency aliasing of a naive digital saw;
TRIANGLE is a direct continuous triangle with naturally faster harmonic rolloff.
Both use DC blocking and the normal Extended pitch/modulation/gate/echo chain.
Lickshot, Gunshot, Saw and Triangle are not DS01E voices. `pc/app.py` exposes
all advanced parameters. The physical UI retains its four-pad bank inside
Extended: Classic (Sine 1/Sine 2/Sonar/Saw/Triangle), Two Tone, Lickshot, Ping
(Syndrum). Last pad wins;
free encoder rotation cycles the selected pad's waveform variant. BTN1 held
while turning adjusts Depth in 0.05-octave steps (0-2 octaves). A latched Hold
alone leaves rotation on variant selection. Holding other Trigger/Play Sound
buttons retains configured live pitch. Encoder push+BTN1 toggles pad Hold,
push+BTN2/3 changes variant, and push+BTN4 cuts echo. Extended menu includes
a return to DS01E.

Profile changes release gates and momentary controls, clear pad Holds and keep
delay contents; advanced numeric settings remain stored. Saved pad assignments
cannot silently switch the default profile. Syndrum/Gunshot remain one-shots;
DECAY retains its exponential meaning in Extended (120 ms default time constant).

### Two Tone Extended (#7)

RC, Triangle e Square modulano il pitch con il valore continuo del LFO scelto.
Il carrier resta quadrato; Classic conserva la commutazione discreta dei due livelli.
Rate e Depth mantengono cadenza ed escursione nominale; Manual usa la curva
manuale e MODULATION 0 esclude la modulazione.

### Lickshot articolato (#5)

Le varianti 0 SAW DOWN e 1 ASYM DOWN conservano la sintesi precedente.
Le nuove varianti 2 SHOT, 3 ROLL e 4 HORN sono selezionabili con PAD_CONFIG
o dall'encoder. SHOT termina anche tenendo premuto; ROLL ripete a Rate Hz
finché gate/Hold sono attivi e rilascia in massimo 3 ms; HORN sostiene senza LFO.
Il pitch di SHOT/ROLL decade indipendentemente da Rate e dalla durata sonora.

Comandi Extended: `SET LICKSHOT_MODE LEGACY|SHOT|ROLL|HORN`,
`SET LICKSHOT_LENGTH_MS 20..1000`, `SET LICKSHOT_PUNCH_OCT 0..3`,
`SET LICKSHOT_PITCH_MS 10..300`. Length è il tempo al silenzio, con attacco
di 2 ms; Punch è la caduta iniziale in ottave; Pitch Ms è la costante di tempo
della caduta. Default 150 ms, 2 ottave, 70 ms: tarature iniziali da ascoltare.
Le nuove articolazioni non usano il LFO per ampiezza/pitch e non azzerano l'echo.
HORN resta selezionato al nuovo colpo se richiamato come variante del pad.

Restano da verificare ascolto sul riddim e costo DSP massimo sull'ESP.

### Tone e Contour dry Extended (#8)

`SET DRY_TONE_HZ 200..19000` imposta un passa-basso dry assoluto a due poli
(Q 0,7071). `SET FILTER_CONTOUR_OCT -4..4` sposta il cutoff al trigger e
`SET FILTER_CONTOUR_MS 10..3000` determina il ritorno esponenziale a Tone.
Il rilascio non riavvia o azzera Contour: prosegue durante la coda. Ogni colpo
ROLL lo riattiva. Quantità positiva apre il filtro all'attacco, negativa lo
chiude; il cutoff è limitato a 50..19000 Hz. Nessuna variazione intenzionale del pitch.

Tone 19000 e Contour zero escludono lo stadio (default). Classic lo ignora.
Il filtro usa Biquad già presente, aggiornamento coefficienti ogni millisecondo,
parametri smussati e crossfade del bypass; Sweep conserva il significato sul pitch.
Non è stato aggiunto gain per compensare automaticamente le armoniche rimosse:
la taratura comparata delle voci e l'ascolto restano da validare, insieme al
budget hardware e all'aliasing nelle combinazioni estreme.

### Echo Color e Cut (#9)

`SET ECHO_COLOR 0..1` (Extended) riutilizza low-pass e saturazione del feedback.
Zero conserva la risposta neutra; aumentando Color si miscela il filtro nel
ritorno anche in FLAT, il limite delle alte scende fino a 1500 Hz e il drive
sale fino a tre volte il valore base, con compensazione della pendenza.
Non c'è guadagno aggiuntivo del loop sotto il 100%; Loop conserva la copia
del materiale, Spin mantiene il feedback temporaneamente elevato.

Echo Cut usa ora una rampa lineare di massimo 2 ms in entrambe le direzioni,
con zero esatto a fine taglio. Agisce solo sul ritorno: dry e memoria dell'echo
continuano. Nessuna risonanza o wow/flutter aggiunti; richiederebbero una
validazione separata. Ascolto dei tagli ritmici ancora da eseguire.
