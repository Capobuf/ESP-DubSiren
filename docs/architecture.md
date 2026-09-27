# Architecture

## Data flow

```text
tkinter panel actions
      |
newline commands over USB
      |
CommandParser -> PerformanceController -> synchronized ControlStore
                         |
                  block snapshot
                         |
ControlState -> gate + activation sequence -> tonal oscillator + gated envelope
                         |           \-> Syndrum carrier + one-shot envelope
                         |           \-> filtered xorshift noise + one-shot
                         \-> shared Dynamic Sweep (tonal modes only)
                                                |
                    delay -> dry + wet -> master -> soft clip -> int16
                                                                      |
                                                        mono int16 PCM block
                                                            /         \
                                                   UsbPcmSink    I2sPcm5102Sink
                                                        |               |
                                             binary PCM over USB   stereo I2S
                                                        |               |
                                                RawOutputStream      PCM5102A
```

`audioTask` produces exactly 480 mono samples per iteration and is paced with
`vTaskDelayUntil` every 10 ms. The DSP uses floats internally and converts to
signed little-endian int16 only at the final output. The task takes one locked
snapshot of `ControlState` at each block boundary; command parsing never mutates
the state concurrently.

`AudioEngine` includes no USB, Python or PCM5102A dependencies. Its one rendered
block is routed through the `AudioSink` interface to `UsbPcmSink`,
`I2sPcm5102Sink`, or both. `UsbPcmSink` adds the binary framing and serializes
AUDIO and STATUS writes with a mutex. STATUS remains available over USB even
when USB AUDIO packets are disabled. Hardware CDC's TX ring is set to 8192 bytes
before USB startup, so a 970-byte audio packet can be queued without waiting
for the default 256-byte ring to drain. Packet writes complete any partial
`Serial.write` before another packet starts.

`I2sPcm5102Sink` duplicates each of the 480 mono int16 samples into a stereo
left/right frame and writes the resulting 960 values to I2S DMA. ESP32-S3 is
the 48 kHz, 16-bit Philips-I2S master TX on GPIO16 BCLK, GPIO17 LRCK and GPIO18
DATA. MCLK and DIN are unused. Four DMA buffers each hold 480 stereo frames;
the audio task remains paced at one rendered block every 10 ms.

The one-second delay is a fixed 48,000-sample int16 ring buffer in internal RAM
(96 kB). It does not use or require PSRAM. Delay reads are fractional and use
linear interpolation. Delay time follows a 25 ms one-pole smoother limited to
2000 ms of delay movement per second. A live sweep therefore bends pitch without
jumping the read head. HPF and LPF are local RBJ biquads with Q 1.0;
their cutoffs follow 18 ms smoothing and coefficients refresh every 64 samples
without resetting filter state. The feedback path uses
asymmetric saturation before the delay write, while the existing cubic
`softClip` remains the final safety limiter.

ECHO LOOP reuses that same ring buffer and allocates no additional audio
memory. Normal operation writes dry input plus saturated feedback. In loop
mode the write path instead copies the raw interpolated delayed sample back to
the ring, while the wet output still passes through the HPF/LPF. This avoids
reapplying filters and saturation on every loop pass. ECHO CUT only mutes the
wet mix through a dedicated 3 ms smoother and therefore leaves loop contents
intact. It does not alter feedback, filtering, or the stored Echo Level.

`SmoothedParameter` handles sample-rate tune, depth, rate, feedback, echo level,
master, DIVE/RISE bend, delay and filter cutoff changes. The LFO retains phase
across shape
changes and fades the modulation offset over 10 ms. Mode changes render both
voicings at the same oscillator phase for a 6 ms crossfade. Neither transition
resets the delay or its feedback state. CLASSIC preset selection happens inside
the firmware; EXTENDED retains the full control state for switching back.
With RETRIGGER enabled, rising edges of TRIGGER and HOLD reset only the LFO to
a shape-aware deterministic state; oscillator phase, filters and delay tails
continue uninterrupted. Disabling modulation zeros the LFO pitch contribution
without stopping its internal phase. CLASSIC always keeps its modulation active.

The same activation-event detection starts a cached-coefficient `OneShotEnvelope`
for Dynamic Sweep. Its decaying value offsets tonal pitch by as much as -2
octaves and slows LFO rate by as much as 3 octaves, then converges continuously
to the selected Tune and Rate. `SWEEP=0` bypasses both offsets. Triggering may
therefore restart sweep and, when RETRIGGER is enabled, reset the LFO in the
same block without resetting oscillator phase or delay.

The V2 oscillator starts from a variable-duty band-limited pulse. SINE1 uses
three tracking one-poles; SINE2 uses two with a higher tracking ratio and more
asymmetry. TEST_TONE adds a click-free rhythmic gate. SQUARE selects high/low
frequency targets with a 2.5 ms anti-click slew while preserving oscillator
phase. LICKSHOT uses a separate open, lightly filtered pulse voicing and SONAR
uses a true sine wave; both share the same oscillator phase, envelope, delay
and mode crossfade. Filter coefficients are refreshed every 16 samples;
measured render
telemetry is exposed in STATUS as `renderUs` and `maxRenderUs`. `cycleUs` and
`maxCycleUs` measure rendering plus the selected output writes.

SYNDRUM and GUNSHOT are transient modes. A trigger activation event starts their
independent amplitude envelope; HOLD, the normal LFO, DIVE/RISE and Dynamic
Sweep do not affect them. Syndrum reuses the oscillator phase for a true sine,
PolyBLEP-integrated triangle or PolyBLEP square carrier and applies its own
signed pitch sweep. Gunshot uses a fixed-seed xorshift32 stream blended with an
existing one-pole low-pass. Neither allocates memory or resets the delay.
Transitions involving a transient use the same 6 ms transition budget to avoid
an abrupt boundary.

## Performance controller

`PerformanceController` is compiled into both firmware and VirtualESP. It owns
the four pad assignments, variant lists, per-pad Hold state, pressed-state
priority, encoder combinations, selected pad, and the initial Classic / Two
Tone / Lickshot / Ping bank. Python receives that state in STATUS and does not
duplicate the sound configurations.

A pad activation updates the mode-specific configuration and an eight-entry
activation ring in one `ControlStore` write. The audio engine transfers unseen
events to its local queue and consumes one per block. The sequence is
independent of the gate, so pressing another pad while one remains held still
retriggers the appropriate envelope, and rapid press/release pairs survive a
block snapshot. The active gate uses independent pad states and returns to the
most recently pressed remaining pad without generating a second event.

The controller changes only voice behavior fields. Tune, Rate, Depth, the four
echo controls, Master, and the delay/filter state are outside pad recall. The
audio engine remains monophonic and no additional engine or delay buffer is
created.

The preset pitch, rate, RC curves, smoothing, transient voicing and saturation
constants are project tuning points. Timbre calibration against a physical
instrument remains future work requiring recordings or measurements. No
circuit or audio identity with a third-party device is claimed. Syndrum follows
the broad behavior of analog percussion synthesis; Gunshot is procedural
synthesis rather than sample playback.

## PC concurrency

- tkinter runs only on the main thread;
- `SerialTransport` owns a reader thread and a resynchronizing packet parser;
- a bounded 12-block queue drops the oldest PCM when full;
- `AudioPlayer` owns the `sounddevice.RawOutputStream` thread and writes silence
  if a block is temporarily unavailable.
- mouse and keyboard sources are latched independently; focus loss sends
  `PANEL_RESET` to release momentary actions;
- dial traffic uses one periodic 25 ms timer per control and sends the final
  value synchronously on release.

The PC has no oscillator, LFO, envelope, delay or filter implementation.

## Output routing

`STREAM 0` disables both audio destinations without stopping USB control.
`STREAM 1` sends the same rendered block according to the atomic output mode:

```text
                         +-> UsbPcmSink -> PC
AudioEngine -> PCM ------+
                         +-> I2sPcm5102Sink -> PCM5102A
```

`SET OUTPUT PC|GPIO|BOTH` changes the destinations at a block boundary without
restarting `AudioEngine` or resetting oscillator, envelope, delay, or filters.
