# Controls

The firmware's absolute DSP/protocol range is intentionally wider than the
performance range exposed by the desktop UI. The GUI maps its normalized
slider position to a physical value and sends that value, in the units shown,
over USB. It never sends the normalized position.

The Profile selector offers **CLASSIC** and **EXTENDED**. EXTENDED exposes the
complete set of continuous controls below. CLASSIC shows Mode, three Pitch
positions, four Modulation positions, the performance buttons, delay, feedback,
echo level, echo HPF/LPF, Echo Cut and Master. CLASSIC fixes LFO depth to 1 octave,
decay to 120 ms, and uses the RC-like LFO shape. The firmware applies these
preset values even when controlled without the desktop UI. In MANUAL the two
buttons control the LFO; with automatic modulation they become DIVE and RISE.
Returning to EXTENDED restores its retained tune, rate, shape, depth and decay
settings.
CLASSIC exposes only the four original modes and always keeps its modulation
active. If LICKSHOT or SONAR is selected when switching to CLASSIC, the GUI
selects and sends SINE1.

| CLASSIC selector | Project preset |
|---|---:|
| Pitch LOW / MID / HIGH | 110 / 220 / 440 Hz |
| Modulation SLOW / MEDIUM / FAST | 0.35 / 0.70 / 2.80 Hz |
| Modulation MANUAL | RC response to MOD DOWN / MOD UP |

These are internal starting values derived from the project's range, not
measurements of another siren. They and the RC charge/fall ratios are centralized
for later listening-based calibration.

| Control | Absolute DSP/protocol range | Performance UI range / default | UI mapping |
|---|---|---|---|
| Mode | six named modes | SINE1 | rounded SINE1, brighter SINE2, gated TEST_TONE, high/low SQUARE, pulse LICKSHOT, sine SONAR |
| Tune | 30–9000 Hz | 80–1600 Hz / 220 Hz | logarithmic |
| LFO Shape | CLASSIC | CLASSIC | RC-like CLASSIC, eight geometric shapes, plus MANUAL |
| LFO Rate | 0.05–20 Hz | 0.15–20 Hz / 0.70 Hz | logarithmic |
| LFO Depth | 0–2 oct | 0–2 oct / 1 oct | linear; exponential musical pitch modulation remains in the DSP |
| Decay | 0–3000 ms | 0–1500 ms / 120 ms | quadratic, with more travel for short decays |
| Delay Time | 50–1000 ms | 50–1000 ms / 360 ms | linear; smoothed fractional read position |
| Feedback | 0–1.05 | 0–1.05 / 0.62 | power curve, exponent 0.8; values above one retain protected self-oscillation |
| Echo Level | 0–1 | 0–1 / 0.45 | linear wet level |
| High Pass | 50–7000 Hz | 50–4000 Hz / 60 Hz | logarithmic, echo path only |
| Low Pass | 200–19000 Hz | 400–16000 Hz / 7000 Hz | logarithmic, echo path only |
| Master | 0–1 | 0–1 / 0.50 | linear final gain before soft limiter |

`TRIGGER`, the two modulation buttons, `ECHO CUT` and `SPIN` are momentary. A
global mouse release handler releases them even if the pointer leaves the widget. `HOLD` is
a toggle and keeps the envelope gate open. `LOOP`, `MOD ON` and `RETRIGGER` are
checkbuttons.

The CLASSIC LFO alternates between +1 and -1 through unequal RC-like charge and
fall curves. In MANUAL the buttons are labelled MOD UP and MOD DOWN: UP targets
+1, DOWN targets -1, and neither or both target zero through the existing
charge/fall model. With an automatic LFO they are labelled RISE and DIVE and
add a smoothed +1 or -1 octave offset after normal modulation; pressing both
cancels the offset.

LICKSHOT remains active while TRIGGER or HOLD opens the amplitude envelope and
uses the selected LFO shape for continuous musical pitch modulation. SAW_DOWN
provides the descending laser gesture. Turning MOD ON off removes pitch
modulation while the LFO continues advancing. **MODE LICKSHOT + MODULATION OFF**
is the project's airhorn configuration. SONAR is a true sine carrier with the same
tune, modulation, bend, envelope, delay and mode-crossfade path. RETRIGGER makes
each rising edge of TRIGGER or HOLD reset the LFO without resetting the carrier,
filters or echo tail.

SQUARE uses the LFO phase as a high/low selector and applies only a 2.5 ms
anti-click pitch slew. TEST_TONE uses the LFO as a half-cycle gate with 2 ms
attack and 4 ms release. With MOD ON off, SQUARE stays at its central pitch and
TEST_TONE stays continuously open.

ECHO CUT mutes only the wet output. It does not clear or stop the delay line, so
the tail returns on release. SPIN temporarily overrides feedback to 1.05 without
changing the stored Feedback control. LOOP has precedence: it recirculates the
raw delayed sample in the existing ring buffer, ignores dry input and feedback
for the write, and continues underneath ECHO CUT. No second delay buffer is
allocated. Tune (12 ms), depth (15 ms), LFO rate (20 ms), bend (30 ms),
feedback (18 ms), echo level and master (15 ms), delay time (25 ms), and echo
filter cutoffs (18 ms) are smoothed in firmware. Delay movement is additionally
limited to 2000 ms/s while fractional reads preserve pitch bending. LFO shape
and Siren Mode transitions last 10 ms and 6 ms respectively. Slider commands are throttled to approximately
25 ms during dragging and the final release value is sent immediately.
