# Controls

The firmware's absolute DSP/protocol range is intentionally wider than the
performance range exposed by the desktop UI. The GUI maps its normalized
slider position to a physical value and sends that value, in the units shown,
over USB. It never sends the normalized position.

The Profile selector offers **CLASSIC** and **EXTENDED**. EXTENDED exposes the
complete set of continuous controls below. CLASSIC shows Mode, three Pitch
positions, four Modulation positions, the performance buttons, delay, feedback,
echo level, echo HPF/LPF, Echo Cut and Master. CLASSIC fixes LFO depth to 1 octave,
decay to 120 ms, and uses the RC-like LFO shape. MANUAL enables MOD DOWN and
MOD UP; automatic modulation disables them. The firmware applies these preset
values even when controlled without the desktop UI. Returning to EXTENDED
restores its retained tune, rate, shape, depth and decay settings.

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
| Mode | SINE1 | SINE1 | rounded SINE1, brighter SINE2, gated TEST_TONE, high/low SQUARE |
| Tune | 30–9000 Hz | 80–1600 Hz / 220 Hz | logarithmic |
| LFO Shape | CLASSIC | CLASSIC | RC-like CLASSIC, eight geometric shapes, plus MANUAL |
| LFO Rate | 0.05–20 Hz | 0.15–8 Hz / 0.70 Hz | logarithmic |
| LFO Depth | 0–2 oct | 0–1.25 oct / 1 oct | linear; exponential musical pitch modulation remains in the DSP |
| Decay | 0–3000 ms | 0–1500 ms / 120 ms | quadratic, with more travel for short decays |
| Delay Time | 50–1000 ms | 50–1000 ms / 360 ms | linear; smoothed fractional read position |
| Feedback | 0–1.05 | 0–1.05 / 0.62 | power curve, exponent 0.8; values above one retain protected self-oscillation |
| Echo Level | 0–1 | 0–1 / 0.45 | linear wet level |
| High Pass | 50–7000 Hz | 50–4000 Hz / 60 Hz | logarithmic, echo path only |
| Low Pass | 200–19000 Hz | 400–16000 Hz / 7000 Hz | logarithmic, echo path only |
| Master | 0–1 | 0–1 / 0.50 | linear final gain before soft limiter |

`TRIGGER`, `MOD DOWN`, `MOD UP` and `ECHO CUT` are momentary. A global mouse
release handler releases them even if the pointer leaves the widget. `HOLD` is
a toggle and keeps the envelope gate open.

The CLASSIC LFO alternates between +1 and -1 through unequal RC-like charge and
fall curves. The manual modulation buttons are enabled only for MANUAL: UP
targets +1, DOWN targets -1, neither or both target zero through the same
charge/fall model.

SQUARE uses the LFO phase as a high/low selector and applies only a 2.5 ms
anti-click pitch slew. TEST_TONE uses the LFO as a half-cycle gate with 2 ms
attack and 4 ms release.

ECHO CUT mutes only the wet output. It does not clear or stop the delay line, so
the tail returns on release. Tune (12 ms), depth (15 ms), LFO rate (20 ms),
feedback (18 ms), echo level and master (15 ms), delay time (25 ms), and echo
filter cutoffs (18 ms) are smoothed in firmware. Delay movement is additionally
limited to 2000 ms/s while fractional reads preserve pitch bending. LFO shape
and Siren Mode transitions last 10 ms and 6 ms respectively. Slider commands are throttled to approximately
25 ms during dragging and the final release value is sent immediately.
