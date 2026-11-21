# Architecture

## Data flow

```text
panel inputs (pots, buttons, encoder)   newline commands over USB
                   \                      /
CommandParser -> PerformanceController -> synchronized ControlStore
                         |                          \
                  logical display state             \-> PanelDisplay -> ST7789/SPI
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
                                                                      |
                                                              I2sPcm5102Sink
                                                                      |
                                                                 stereo I2S
                                                                      |
                                                                  PCM5102A
```

`audioTask` produces exactly 480 mono samples per iteration and is paced with
`vTaskDelayUntil` every 10 ms. It owns core 0, while `loop()` runs on core 1;
the DSP functions are `IRAM_ATTR`, so audio does not depend on the instruction
cache both cores share (measurements in [standalone](standalone.md)). The DSP uses floats internally and converts to
signed little-endian int16 only at the final output. The task takes one locked
snapshot of `ControlState` at each block boundary; command parsing never mutates
the state concurrently.

`AudioEngine` includes no USB or PCM5102A dependencies. Its one rendered block
goes to `I2sPcm5102Sink` only; USB carries no audio. `UsbPacketWriter`, used by
the main loop, adds the binary framing to STATUS and COMMAND_RESULT packets and
serializes them with a mutex. Hardware CDC's TX ring is set to 8192 bytes
before USB startup, so a complete STATUS packet can be queued. The writer
reserves space for a complete packet and issues one non-blocking write; a short
write reports failure.

`PanelDisplay` is outside `audioTask`. The main loop reads controller and
`ControlStore` state and renders Home or Menu with Adafruit GFX into a
320x172 `GFXcanvas16` (110 kB of internal heap). `PanelLcd` sends the frame
to the ST7789 through ESP-IDF `esp_lcd` (SPI2 with DMA, 40 MHz, about 22 ms per
frame) and returns at once; the next frame is drawn only after the transfer
callback, so `loop()` never waits for the bus. It redraws on state changes or
parameter-overlay expiry, at most once every 100 ms. STATUS exposes
`displayRenderUs` and `maxDisplayRenderUs` (drawing time in `loop()`, not the
DMA transfer) separately from audio timing.
The audio task contains no SPI or display calls.

Home maps the panel controls to their position; see
[panel UI](panel-ui.md). ST7789 initialization targets Adafruit
Product 5393 at 320x172 in landscape, verified on the first PCB. The existing SPI GPIO assignments are retained.

`I2sPcm5102Sink` duplicates each of the 480 mono int16 samples into a stereo
left/right frame and writes the resulting 960 values to I2S DMA. ESP32-S3 is
the 48 kHz, 16-bit Philips-I2S master TX on GPIO16 BCLK, GPIO17 LRCK and GPIO18
DATA. MCLK and DIN are unused. Four DMA buffers each hold 480 stereo frames;
the audio task remains paced at one rendered block every 10 ms.

The one-second delay is a fixed 48,000-sample int16 ring buffer in internal RAM
(96 kB). It does not use or require PSRAM. Delay reads are fractional and use
linear interpolation. Delay time follows a 25 ms one-pole smoother limited to
2000 ms of delay movement per second. A live sweep therefore bends pitch without
jumping the read head. HPF and LPF are local RBJ biquads with Q 0.7071;
their cutoffs follow 18 ms smoothing and coefficients refresh every 64 samples
without resetting filter state. The feedback path uses asymmetric saturation
normalized to compensate both the drive and the delay write `softClip`'s 1.5
small-signal gain. Below 100%, repeats decay instead of acquiring hidden gain
on each pass. Normal writes truncate int16 quantization toward zero to avoid
residual limit cycles; LOOP retains rounded copies. The existing cubic
`softClip` remains the final output safety limiter.

ECHO LOOP reuses that same ring buffer and allocates no additional audio
memory. Normal operation writes dry input plus saturated feedback. In loop
mode the write path instead copies the raw interpolated delayed sample back to
the ring, while the wet output still passes through the HPF/LPF. This avoids
reapplying filters and saturation on every loop pass. ECHO CUT only mutes the
wet mix with a 2 ms ramp and therefore leaves loop contents intact; release
also uses a 2 ms ramp. It does not alter feedback, filtering, or the stored Echo Level.

`SmoothedParameter` handles sample-rate tune, depth, rate, feedback, echo level,
master, DIVE/RISE bend, delay and filter cutoff changes. The LFO retains phase
across shape
changes and fades the modulation offset over 10 ms. Mode changes render both
voicings at the same oscillator phase for a 6 ms crossfade. Neither transition
resets the delay or its feedback state. Voice selection happens inside the
shared controller. The SIREN shape restarts its sweep level on every rising
edge of TRIGGER and HOLD and MANUAL raises it; for the other shapes, with
RETRIGGER enabled, those edges reset only the LFO to
a shape-aware deterministic state; oscillator phase, filters and delay tails
continue uninterrupted. Disabling modulation zeros the LFO pitch contribution
without stopping its internal phase.

The same activation-event detection starts a cached-coefficient `OneShotEnvelope`
for Dynamic Sweep. Its decaying value offsets tonal pitch by as much as -2
octaves and slows LFO rate by as much as 3 octaves, then converges continuously
to the selected Tune and Rate. `SWEEP=0` bypasses both offsets. Triggering may
therefore restart sweep and, when RETRIGGER is enabled, reset the LFO in the
same block without resetting oscillator phase or delay.

SINE1 and SINE2 use PolyBLEP pulse
carriers with distinct duty ranges,
one tracking low-pass filter, asymmetric saturation and DC blocking. Their
duty responds to modulation, so harmonic balance changes during the sweep.
Each sine voice has one implementation and shared filter state. Legacy
voicing selection does not select a separate sine implementation. The outer
envelopes remain in AudioEngine.
TEST_TONE gates a fixed-pitch sine with the LFO; SQUARE retains PolyBLEP and
one tracking low-pass, selecting high/low pitch with a 2.5 ms slew when the
shape is SIREN or MANUAL and following the LFO value and Depth otherwise. Their
manual gates come from MOD UP rather than frozen automatic phase.
With DECAY 0 the tonal amplitude uses a finite linear 2 ms attack / 3 ms
release; its gate is applied after oscillator filtering so no dry filter tail
escapes. Any other DECAY selects the exponential release; the one-shot voices
have their own envelopes. LICKSHOT keeps
its pulse/filter/saturation voice. SONAR applies an 80 ms exponential ping
envelope to its sine carrier at each rising LFO gate, plus a finite 2/3 ms
gate to leave exact silent gaps during the inactive half-cycle. Pulse cadence
follows effective LFO rate, including Dynamic Sweep; amplitude pulses remain
active with pitch modulation disabled. Entry into Sonar or a trigger/hold
activation resets its ping envelope and the LFO even with RETRIGGER off, while
oscillator phase and delay contents continue. Manual uses the MOD UP gate.
SAW uses a PolyBLEP ramp and TRIANGLE uses a direct continuous triangle;
both are continuous carriers with DC blocking. Calibration is
centralized in PerformanceConfig.h and does not claim measured hardware fidelity.

SYNDRUM and GUNSHOT are transient modes. A trigger activation event starts their
independent amplitude envelope; HOLD, the normal LFO, DIVE/RISE and Dynamic
Sweep do not affect them. Syndrum reuses the oscillator phase for a true sine,
PolyBLEP-integrated triangle or PolyBLEP square carrier and applies its own
signed pitch sweep. Gunshot uses a fixed-seed xorshift32 stream that darkens
through a one-pole low-pass as it decays, over a falling sine thump, saturated. Neither allocates memory or resets the delay.
Transitions involving a transient use the same 6 ms transition budget to avoid
an abrupt boundary.

## Performance controller

`PerformanceController` is compiled into both firmware and the Android app.
`PanelSettings` (schema 6) owns the single versioned POT and BTN mapping,
zones/ranges and the four sound patches. Raw numbered POT inputs, target curves, hysteresis and the
filter macro are interpreted only in shared C++. The controller also owns the
one sound list stepped by the encoder; Play Sound pads keep last-pressed
priority, the Hold lock (a held pad plus the encoder push) and the up/down
lever (a held pad plus encoder rotation).

The firmware reports the same compact logical display state in STATUS
`panel.display`: 320x172 dimensions, Home/Menu mode, controller-formatted title,
ordered rows, values, footer and temporary overlay, plus the full versioned
panel settings in `panel.settings`. The Adafruit renderer and any host reading
STATUS therefore show the same state, parameter values and encoder menus
without rebuilding UI semantics.

A Trigger or pad activation updates the mode-specific configuration and an eight-entry
activation ring in one `ControlStore` write. The audio engine transfers unseen
events into the current block, preserving relative microsecond press intervals.
There is no multi-block backlog: the ring retains the newest eight events;
overwritten, stale (outside the newest block-sized timestamp window), or
simultaneous/superseded events increment `droppedActivations`.
The first captured event starts at the block boundary. With DECAY 0 a
zero-duration tap receives a 2 ms minimum gate and the usual 3 ms release;
with any other DECAY it keeps a 10 ms minimum gate and the exponential/one-shot
tails. Hardware input latency,
block wait and I2S buffering must be measured separately. The sequence is
independent of the gate, so pressing another pad while one remains held still
retriggers the appropriate envelope, and rapid press/release pairs survive a
block snapshot. The active gate uses independent pad states and returns to the
most recently pressed remaining pad without generating a second event.

Choosing a voice or playing a pad without a saved patch changes only voice
behavior fields; a saved patch also restores Tune, Rate and Depth. The four
echo controls, Master, and the delay/filter state are outside pad recall. The
audio engine remains monophonic and no additional engine or delay buffer is
created.

The preset pitch, rate, RC curves, smoothing, transient voicing and saturation
constants are project tuning points. Timbre calibration against a physical
instrument remains future work requiring recordings or measurements. No
circuit or audio identity with a third-party device is claimed. Syndrum follows
the broad behavior of analog percussion synthesis; Gunshot is procedural
synthesis rather than sample playback.
