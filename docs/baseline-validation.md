# Classic baseline validation - 2026-09-29

This revision establishes a DS01E-inspired functional baseline, not a measured
replica. Official source links and calibration distinctions are in
[controls](controls.md). No PCB, GPIO, I2S, USB routing, PCM5102A configuration,
sample rate or dependency changes were made.

## Cause and correction

The old `kClassicDecayMs = 120` selected an exponential envelope time constant,
not a 120 ms deadline. At 120 ms its amplitude was still about 37%; reaching
the 0.00001 cutoff took about 1.38 seconds. The physical controller also forced
Extended on startup, whose default release was the same 120 ms time constant.
The tail was therefore generated before the delay, even with no wet return.

Classic now applies a finite 2 ms attack / 3 ms release gate after the carrier
and its filters. Extended keeps its exponential decay and one-shot behavior.
Audio command snapshots occur once per 10 ms block; output buffering remains
unchanged. The gate bound does not include command or device buffering latency.

## Automated checks

Required commands, all successful:

```powershell
pc\.venv\Scripts\python.exe -m unittest discover -s pc -p "test_*.py"
pc\.venv\Scripts\python.exe tests\native\run.py
python -m platformio run -d firmware
```

- Python: 38 tests, no failures or errors.
- Native: 26 regression functions, including seven new Classic test groups.
  Assertions are explicitly enabled even when Zig's optimized build defines
  NDEBUG. Previously that build could report success without evaluating the
  assertions or the parser calls inside them. Existing Extended fixtures now
  select Extended explicitly; the frequency estimator interpolates crossing
  intervals instead of counting whole crossings in a short window. Two old
  expectations were corrected (selected Sine 2 variant and Manual depth).
- ESP32-S3 build: successful; 119,304 bytes static RAM (36.4%), 1,282,161 bytes
  flash (19.6%). PlatformIO reports a pre-existing multiple-core-version warning
  (active 6.1.19 versus previously used 6.2.0); compilation succeeds.
- `git diff --check`: no whitespace errors.

New native coverage checks all four dry release gates, exact dry equality with
different populated delay buffers, immediate wet exclusion by Level zero and
Cut, surviving echo under Cut, one repeat at zero feedback, increasing repeat
energy with feedback, selector ADC sweeps and boundary jitter in both
directions, Hold, four-voice encoder wrap, Manual independent of automatic rate,
automatic beep/two-tone behavior, Sine 1/Sine 2 harmonic distinction, and
isolation from advanced Classic-inapplicable settings. Existing Extended,
one-shot, loop, saturation and transition regressions remain active.

## Virtual ESP application and PCM checks

The actual Tk application was connected to Virtual ESP and PC audio started.
Its callbacks exercised all four voices, Trigger release, Hold, Manual,
Pitch/Rate selectors, filter overlay, menu and profile changes. The audio player
reported no errors. Home, filter and menu text were inspected at 320x170.
This was an application/PCM check, not a human listening verdict or a test of
physical ADCs, the hardware display, I2S or a Benidub unit.

Reproduce listening files with:

```powershell
pc\.venv\Scripts\python.exe pc\baseline_capture.py
```

`artifacts/baseline/` contains 16 short trigger/echo clips, four longer dry
performance clips and `metrics.json`. Each uses MID pitch, MID rate and the
shared firmware DSP. Short clips use a 120 ms trigger and 300 ms echo time.
All four dry clips have an exact zero peak after sample 146 following release
(3.04 ms). With zero feedback, every clip has exactly one repeat; later repeat
windows are zero. Example SINE1 repeat peaks (int16):

| Feedback | Repeat 1 | Repeat 2 | Repeat 3 | Repeat 6 |
|---|---:|---:|---:|---:|
| 0 | 12856 | 0 | 0 | 0 |
| 0.50 | 12856 | 9206 | 7077 | 3702 |
| 0.80 | 12856 | 11908 | 11509 | 11181 |

Generated audio files are local artifacts excluded from Git; the capture
script is tracked for reproduction.

## Remaining listening calibration

Pitch/rate presets, modulation depth and curves, Sine 2 harmonic balance,
relative voice loudness, Square edge character, manual response and filter
resonance need subjective evaluation and eventually measured A/B recordings.
The table above records the earlier feedback calibration, which sustained below
1.0 because saturation and the write limiter added small-signal gain. This was
corrected on 2026-10-04 by normalizing both gains, using non-resonant echo filters
and truncating normal delay writes to avoid int16 limit cycles. Regenerated
captures in `artifacts/echo-decay/` give these SINE1 repeat peaks:

| Feedback | Repeat 1 | Repeat 2 | Repeat 3 | Repeat 6 |
|---|---:|---:|---:|---:|
| 0 | 12589 | 0 | 0 | 0 |
| 0.50 | 12589 | 5620 | 2734 | 337 |
| 0.80 | 12589 | 7648 | 5435 | 2447 |

Native tests check tail decay at feedback 0-0.99 with flat and filtered echo,
and confirm that lowering an already self-oscillating 1.05 loop to 0.50 drains
the buffer. Above-unity feedback and explicit ECHO LOOP remain available.
Exact wet mute may click when a loud repeat is interrupted. No claim
of indistinguishability from the DS01E is made.

## Shared sine voicing and pulsed Sonar - 2026-10-04

SINE1 and SINE2 now call their existing Classic oscillator implementations in
both profiles. The separate Extended sine carriers, calibration constants and
oscillator profile argument were removed. Existing Classic harmonic, gain,
DC, stability and gate checks pass unchanged. A new full-engine comparison
checks both sine voices in Classic and Extended at equal effective controls,
in automatic and Manual modulation, with either legacy/V2 selection. After
the different gate attacks settle, every output sample agrees within one
int16 unit. Extended release and control ranges remain profile-specific.

Sonar now emits a decaying sine ping per rising LFO gate, with exact silent
gaps after the active half-cycle. Native PCM checks over three seconds record
3 pings at 1 Hz and 12 at 4 Hz, with respectively 147 and 131 entirely silent
10 ms blocks. The same results hold with pitch modulation enabled or disabled.
Coverage also checks decay inside a pulse, immediate restart on queued taps,
trigger edges and voice selection, and dry silence after release with DECAY zero.
Pitch and Dynamic Sweep fixtures use a continuous sine voice so silent gaps
do not invalidate their frequency estimates.

The Virtual ESP C ABI was exercised through the panel path: Extended, two free
encoder steps to SONAR, then PAD 1 press/release. Dry listening clips at 1 Hz
and 4 Hz are available locally as `artifacts/baseline/sonar_1hz_dry.wav` and
`sonar_4hz_dry.wav`; boundary rounding can include the start of the next ping
in the final capture block. Echo is excluded in these clips.

Validation: native DSP suite and all 45 Python tests pass; ESP32-S3 build passes
with 120,772 bytes static RAM and 388,221 bytes flash. `git diff --check` passes.
The hardware smoke test was updated for pulse cadence and silence instead of
a continuous Sonar frequency window; it was not run on hardware in this check.

## Extended Saw/Triangle PR integration - 2026-10-04

PR #4 adds Extended-only SAW and TRIANGLE carriers, protocol commands,
desktop controls and two more variants in the Extended Classic pad family.
Integration preserves the shared Sine voicings, pulsed Sonar and current
encoder gestures from main. Classic still exposes only its original four modes.

Review corrected a missing import in the new legacy-panel test and adjusted
the waveform crossing assertion to the current 4,800-sample (100 ms) window:
220 Hz produces about 22 positive crossings, not 220. Native spectral checks
verify the Saw harmonics and Triangle's suppressed even harmonics. A new
Virtual ESP regression cycles all five pad variants, plays both new carriers,
checks dry silence after release and rejects them after switching to Classic.

Validation: all 47 Python tests and the native DSP suite pass; ESP32-S3 build
passes with 120,844 bytes static RAM and 388,477 bytes flash. Python tests ran
from a temporary copy of the tracked sources because the working checkout's
Virtual ESP DLL was already loaded by another process. No hardware smoke test,
upload or physical listening check was performed during this review.
