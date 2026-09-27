# Controls

The firmware's absolute DSP/protocol range is intentionally wider than the
performance range exposed by the desktop UI. The GUI maps each knob or encoder
position to a physical value and sends that value, in the units shown, over
USB. It never sends the normalized position.

The default `physical_ui.py` surface presents one instrument and keeps the
firmware on the EXTENDED profile internally. It exposes Tune, Rate, Echo Level,
Echo Time, Echo Feedback and the single Echo Tone macro as the six front knobs.
The encoder controls Depth directly on Home; its menu configures one selected
pad and Echo Loop. The separate rear MASTER maps directly to `SET MASTER`.

The older `app.py` interface remains available as a technical/debug tool. Its
Profile selector can still exercise the firmware's CLASSIC preset behavior:
three fixed pitches, four modulation positions, one-octave depth, 120 ms decay
and the RC-like LFO. CLASSIC supports the four original modes and always keeps
modulation active. These protocol behaviors remain for compatibility but are
not a primary concept in the default UI.

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
| Mode | eight named modes | SINE1 | six tonal modes plus one-shot SYNDRUM and GUNSHOT |
| Tune | 30–9000 Hz | 80–1600 Hz / 220 Hz | logarithmic |
| LFO Shape | CLASSIC | CLASSIC | RC-like CLASSIC, eight geometric shapes, plus MANUAL |
| LFO Rate | 0.05–20 Hz | 0.15–20 Hz / 0.70 Hz | logarithmic |
| LFO Depth | 0–2 oct | 0–2 oct / 1 oct | linear; exponential musical pitch modulation remains in the DSP |
| Decay | 0–3000 ms | 0–1500 ms / 120 ms | quadratic, with more travel for short decays |
| Sweep | 0–1 | 0–1 / 0 | tonal Dynamic Sweep amount; percentage in UI |
| Sweep Time | 50–3000 ms | 50–3000 ms / 650 ms | quadratic, with more travel for short times |
| Syndrum Wave | SINE, TRIANGLE, SQUARE | SINE | visible only in SYNDRUM |
| Syndrum Sweep | -3–3 oct | -3–3 oct / +2 oct | signed offset converging to Tune; visible only in SYNDRUM |
| Delay Time | 50–1000 ms | 50–1000 ms / 360 ms | linear; smoothed fractional read position |
| Feedback | 0–1.05 | 0–1.05 / 0.62 | power curve, exponent 0.8; values above one retain protected self-oscillation |
| Echo Level | 0–1 | 0–1 / 0.45 | linear wet level |
| High Pass | 50–7000 Hz | 50–4000 Hz / 60 Hz | logarithmic, echo path only |
| Low Pass | 200–19000 Hz | 400–16000 Hz / 7000 Hz | logarithmic, echo path only |
| Master | 0–1 | 0–1 / 0.50 | linear final gain before soft limiter |

The performance surface maps BTN1–BTN4 to independent pad events. The initial
assignments are CLASSIC, TWO TONE, LICKSHOT, and the Syndrum-based PING. Last
pressed wins; releasing it returns to a still-held previous pad without a new
attack. HOLD is per pad and is unavailable for PING. Encoder+BTN1 toggles Hold,
Encoder+BTN2/BTN3 select the previous/next supported variant, and Encoder+BTN4
holds ECHO CUT. DIVE/RISE, MOD UP/DOWN, direct TRIGGER, and SPIN remain protocol
and debug-UI functions, but are not primary panel button assignments.

Dynamic Sweep starts on every rising edge of TRIGGER or HOLD in SINE1, SINE2,
TEST_TONE, SQUARE, LICKSHOT and SONAR. It begins below/slower than the selected
Tune and Rate, then converges exponentially. At the default Sweep value 0 the
0.5.0 tonal behavior is retained. RETRIGGER and Dynamic Sweep can restart on
the same edge.

SYNDRUM is a one-shot percussion voice. DECAY controls amplitude duration and
its signed Sweep starts above Tune for positive values (falling pitch), below
Tune for negative values (rising pitch), or at Tune for zero. GUNSHOT is a
fixed-seed procedural noise burst with simple low-pass shaping and uses DECAY,
internally constrained to 20–1500 ms. Neither mode uses the normal LFO,
DIVE/RISE or global Dynamic Sweep. HOLD cannot sustain them. The performance
controller clears hidden Hold state when entering PING; the debug UI disables
Hold and DIVE/RISE for transient modes. Both transient voices continue through
the normal echo and master path.

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

ECHO CUT mutes only the wet output through a dedicated 3 ms anti-click ramp. It
does not clear or stop the delay line, so the tail returns on release. SPIN temporarily overrides feedback to 1.05 without
changing the stored Feedback control. LOOP has precedence: it recirculates the
raw delayed sample in the existing ring buffer, ignores dry input and feedback
for the write, and continues underneath ECHO CUT. No second delay buffer is
allocated. Tune (12 ms), depth (15 ms), LFO rate (20 ms), bend (30 ms),
feedback (18 ms), echo level and master (15 ms), delay time (25 ms), and echo
filter cutoffs (18 ms) are smoothed in firmware. Delay movement is additionally
limited to 2000 ms/s while fractional reads preserve pitch bending. LFO shape
and Siren Mode transitions last 10 ms and 6 ms respectively. Slider and dial
commands keep at most one 25 ms timer per control, periodically send the latest
value during a continuous drag, and send the final value immediately on release.
