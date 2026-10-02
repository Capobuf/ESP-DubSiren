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
The existing nonlinear feedback path can sustain at settings below 1.0 (the
0.80 example approaches a plateau); its musical threshold remains a calibration
point. Exact wet mute may click when a loud repeat is interrupted. No claim
of indistinguishability from the DS01E is made.
