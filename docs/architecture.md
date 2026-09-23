# Architecture

## Data flow

```text
tkinter controls
      |
newline commands over USB
      |
CommandParser -> synchronized ControlStore
                         |
                  block snapshot
                         |
ControlState -> AudioEngine -> Oscillator -> Envelope
                                      |          |
                                      +-> Delay -> HPF -> LPF
                                                   |
                              dry + wet -> master -> soft clip -> int16
                                                                      |
                                                                 AudioSink
                                                                      |
                                                                 UsbPcmSink
                                                                      |
                                         binary PCM -> RawOutputStream
```

`audioTask` produces exactly 480 mono samples per iteration and is paced with
`vTaskDelayUntil` every 10 ms. The DSP uses floats internally and converts to
signed little-endian int16 only at the final output. The task takes one locked
snapshot of `ControlState` at each block boundary; command parsing never mutates
the state concurrently.

`AudioEngine` includes no USB, Python or PCM5102A dependencies. Its output is
written through the `AudioSink` interface. `UsbPcmSink` adds the binary framing
and serializes AUDIO and STATUS writes with a mutex.

The one-second delay is a fixed 48,000-sample int16 ring buffer in internal RAM
(96 kB). It does not use or require PSRAM. Delay reads are fractional and use
linear interpolation. HPF and LPF are local RBJ biquads; coefficients change
only when their cutoff changes. A single cubic `softClip` implementation
protects both feedback and final output.

## PC concurrency

- tkinter runs only on the main thread;
- `SerialTransport` owns a reader thread and a resynchronizing packet parser;
- a bounded 12-block queue drops the oldest PCM when full;
- `AudioPlayer` owns the `sounddevice.RawOutputStream` thread and writes silence
  if a block is temporarily unavailable.

The PC has no oscillator, LFO, envelope, delay or filter implementation.

## Future PCM5102A backend

A future backend will implement:

```cpp
class I2sPcm5102Sink : public AudioSink;
```

It will use standard ESP32-S3 I2S TX and receive the same mono int16 blocks,
optionally duplicating them into the left and right slots. `AudioEngine` will
not change. GPIO assignments are intentionally undecided until the physical
module and board connections are inspected; this repository contains no
speculative PCM5102A wiring.
