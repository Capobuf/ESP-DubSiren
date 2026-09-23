# USB protocol

The device uses Arduino Hardware CDC / USB Serial-JTAG. Control traffic from PC
to ESP is ASCII, newline-delimited. ESP-to-PC traffic is binary only; firmware
does not print diagnostic text into the PCM stream.

## PC to ESP

Connection commands:

```text
HELLO
STREAM 1
STREAM 0
```

Control commands:

```text
SET MODE SINE1|SINE2|TEST_TONE|SQUARE
SET TUNE_HZ <30..9000>
SET LFO_SHAPE TRIANGLE|SQUARE|SAW_UP|SAW_DOWN|ASYM_UP|ASYM_DOWN|PULSE_25|PULSE_75|MANUAL
SET LFO_RATE_HZ <0.05..20>
SET LFO_DEPTH_OCT <0..2>
SET DECAY_MS <0..3000>
SET DELAY_MS <50..1000>
SET FEEDBACK <0..1.05>
SET ECHO_LEVEL <0..1>
SET HPF_HZ <50..7000>
SET LPF_HZ <200..19000>
SET MASTER <0..1>
TRIGGER 0|1
HOLD 0|1
MOD_UP 0|1
MOD_DOWN 0|1
ECHO_CUT 0|1
```

Finite numeric values are clamped to their valid range. Malformed and unknown
commands are ignored. Parsing occurs outside the audio task.

## ESP to PC

Every packet begins with this packed, 10-byte little-endian header:

| Offset | Field | Type | Value |
|---:|---|---|---|
| 0 | magic | 2 bytes | `DS` |
| 2 | version | uint8 | `1` |
| 3 | type | uint8 | AUDIO `0x01`, STATUS `0x02` |
| 4 | payload length | uint16 | bytes |
| 6 | sequence | uint32 | increments per packet type |

An AUDIO payload is always 960 bytes: 480 signed int16 mono PCM samples at
48,000 Hz. A STATUS payload is UTF-8 JSON. `HELLO` currently returns:

```json
{"name":"DubSiren","protocol":1,"sampleRate":48000,"blockSamples":480,"firmware":"0.1.0"}
```

There is no CRC because USB is the reliable transport. The Python parser scans
again for `DS` after invalid data or lost framing and rejects AUDIO payloads of
the wrong length.
