# Controls

| Control | Range/default | Behavior |
|---|---|---|
| Mode | SINE1 | rounded SINE1, brighter SINE2, gated TEST_TONE, high/low SQUARE |
| Tune | 30–9000 Hz / 220 Hz | logarithmic GUI mapping |
| LFO Shape | CLASSIC | RC-like CLASSIC, eight geometric shapes, plus MANUAL |
| LFO Rate | 0.05–20 Hz / 0.70 Hz | logarithmic GUI mapping |
| LFO Depth | 0–2 oct / 1 oct | exponential musical pitch modulation |
| Decay | 0–3000 ms / 120 ms | release time; attack is fixed at about 4 ms |
| Delay Time | 50–1000 ms / 360 ms | smoothed fractional read position |
| Feedback | 0–1.05 / 0.62 | values above one allow protected self-oscillation |
| Echo Level | 0–1 / 0.45 | wet level |
| High Pass | 50–7000 Hz / 60 Hz | logarithmic GUI mapping, echo path only |
| Low Pass | 200–19000 Hz / 7000 Hz | logarithmic GUI mapping, echo path only |
| Master | 0–1 / 0.50 | final gain before soft limiter |

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
the tail returns on release. Tune, LFO depth, delay time, echo level and master
are smoothed in the firmware. Slider commands are throttled to approximately
25 ms during dragging and the final release value is sent immediately.
