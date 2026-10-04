"""Capture comparable firmware performance scenarios as WAV files.

Requires a connected ESP32-S3 running this firmware. Pass --output-dir to keep
the captures; otherwise a new directory is made under the OS temp directory.
"""

import argparse
import tempfile
import time
import wave
from pathlib import Path

from voicing_test import Capture


SCENARIOS = (
    ("classic_sine1_low_slow", ("SET PROFILE CLASSIC", "SET MODE SINE1",
                                "SET CLASSIC_PITCH LOW", "SET CLASSIC_MOD SLOW")),
    ("classic_sine1_mid_medium", ("SET MODE SINE1", "SET CLASSIC_PITCH MID",
                                     "SET CLASSIC_MOD MEDIUM")),
    ("classic_sine2_mid_slow", ("SET MODE SINE2", "SET CLASSIC_PITCH MID",
                                   "SET CLASSIC_MOD SLOW")),
    ("classic_square_high_fast", ("SET MODE SQUARE", "SET CLASSIC_PITCH HIGH",
                                      "SET CLASSIC_MOD FAST")),
    ("test_tone", ("SET MODE TEST_TONE", "SET CLASSIC_PITCH MID",
                   "SET CLASSIC_MOD MEDIUM")),
    ("feedback_060", ("SET MODE SINE1", "SET FEEDBACK 0.6",
                      "SET ECHO_LEVEL 0.45")),
    ("feedback_095", ("SET FEEDBACK 0.95",)),
    ("lickshot_saw_down", ("SET PROFILE EXTENDED", "SET MODE LICKSHOT",
                           "SET LFO_SHAPE SAW_DOWN", "SET LFO_RATE_HZ 3.5",
                           "SET LFO_DEPTH_OCT 1.5", "MODULATION 1",
                           "RETRIGGER 1")),
    ("lickshot_triangle", ("SET MODE LICKSHOT", "SET LFO_SHAPE TRIANGLE",
                           "SET LFO_RATE_HZ 2.2", "SET LFO_DEPTH_OCT 1.2",
                           "MODULATION 1")),
    ("lickshot_airhorn", ("SET MODE LICKSHOT", "SET TUNE_HZ 220",
                          "MODULATION 0")),
    ("sonar", ("SET MODE SONAR", "SET TUNE_HZ 220",
               "SET LFO_RATE_HZ 1", "SET LFO_DEPTH_OCT 0", "MODULATION 1")),
)

TRANSIENT_CAPTURE_NAMES = (
    "dynamic_sweep_sine1",
    "dynamic_sweep_lickshot",
    "dynamic_sweep_sonar",
    "syndrum_down",
    "syndrum_up",
    "syndrum_sine",
    "syndrum_square",
    "gunshot_dry",
    "gunshot_echo",
)

CAPTURE_COUNT = len(SCENARIOS) + 6 + len(TRANSIENT_CAPTURE_NAMES)


def save_wav(path: Path, pcm: bytes) -> None:
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(48_000)
        output.writeframes(pcm)


def capture_sequence(device: Capture, command: str, values: tuple[str, ...]) -> bytes:
    pcm = bytearray()
    for value in values:
        device.send(f"SET {command} {value}")
        pcm.extend(device.grab(10))
    return bytes(pcm)


def capture_segments(
    device: Capture, segments: tuple[tuple[tuple[str, ...], int], ...]
) -> bytes:
    pcm = bytearray()
    for commands, blocks in segments:
        device.send(*commands)
        pcm.extend(device.grab(blocks))
    return bytes(pcm)


def capture_triggered(
    device: Capture, commands: tuple[str, ...], blocks: int = 100
) -> bytes:
    device.send("HOLD 0", "TRIGGER 0", *commands)
    time.sleep(0.12)
    device.drain()
    device.send("TRIGGER 1")
    pcm = device.grab(blocks)
    device.send("TRIGGER 0")
    return pcm


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="ESP32 USB serial port, e.g. COM6")
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    directory = args.output_dir or Path(tempfile.mkdtemp(prefix="dubsiren-ab-"))
    directory.mkdir(parents=True, exist_ok=True)
    device = Capture(args.port)
    try:
        device.configure()
        device.send("SET DECAY_MS 120", "SET DELAY_MS 360")
        for name, commands in SCENARIOS:
            device.send(*commands)
            time.sleep(0.20)
            device.drain()
            save_wav(directory / f"{name}.wav", device.grab(100))
        device.send("SET PROFILE EXTENDED", "SET MODE SINE1",
                    "SET LFO_SHAPE CLASSIC", "SET LFO_DEPTH_OCT 1")
        save_wav(directory / "delay_sweep.wav",
                 capture_sequence(device, "DELAY_MS",
                                  ("100", "180", "300", "500", "800",
                                   "650", "450", "300", "180", "100")))
        save_wav(directory / "feedback_sweep.wav",
                 capture_sequence(device, "FEEDBACK",
                                  ("0", "0.2", "0.4", "0.6", "0.8",
                                   "0.95", "1", "1.05", "0.7", "0")))
        device.send("SET FEEDBACK 0.95")
        pcm = bytearray()
        for hpf, lpf in (("50", "19000"), ("100", "12000"),
                         ("300", "7000"), ("1000", "3000"),
                         ("3000", "1000"), ("7000", "200"),
                         ("3000", "1000"), ("1000", "3000"),
                         ("300", "7000"), ("50", "19000")):
            device.send(f"SET HPF_HZ {hpf}", f"SET LPF_HZ {lpf}")
            pcm.extend(device.grab(10))
        save_wav(directory / "filter_sweep.wav", bytes(pcm))
        device.send("SET MODE SONAR", "SET TUNE_HZ 220",
                    "SET LFO_DEPTH_OCT 0", "MODULATION 1", "HOLD 1")
        save_wav(directory / "rise_dive.wav", capture_segments(device, (
            (("MOD_UP 0", "MOD_DOWN 0"), 25),
            (("MOD_UP 1",), 50),
            (("MOD_UP 0",), 25),
            (("MOD_DOWN 1",), 50),
            (("MOD_DOWN 0",), 25),
            (("MOD_UP 1", "MOD_DOWN 1"), 35),
            (("MOD_UP 0", "MOD_DOWN 0"), 25),
        )))
        device.send("SET MODE SINE1", "SET DELAY_MS 120", "SET FEEDBACK 0.65",
                    "SET ECHO_LEVEL 1", "HOLD 1")
        save_wav(directory / "echo_spin.wav", capture_segments(device, (
            (("ECHO_SPIN 0",), 40),
            (("ECHO_SPIN 1",), 100),
            (("ECHO_SPIN 0",), 60),
        )))
        device.send("SET FEEDBACK 0.72", "ECHO_LOOP 0", "HOLD 1")
        save_wav(directory / "echo_loop.wav", capture_segments(device, (
            ((), 50),
            (("SET DECAY_MS 0", "HOLD 0", "ECHO_LOOP 1"), 140),
            (("ECHO_CUT 1",), 25),
            (("ECHO_CUT 0",), 50),
            (("ECHO_LOOP 0",), 25),
        )))
        dynamic_base = (
            "SET PROFILE EXTENDED", "SET TUNE_HZ 220", "SET DECAY_MS 500",
            "SET LFO_DEPTH_OCT 0", "SET SWEEP 1", "SET SWEEP_MS 650",
            "SET ECHO_LEVEL 0", "MODULATION 0",
        )
        for name, mode in (
            ("dynamic_sweep_sine1", "SINE1"),
            ("dynamic_sweep_lickshot", "LICKSHOT"),
            ("dynamic_sweep_sonar", "SONAR"),
        ):
            save_wav(
                directory / f"{name}.wav",
                capture_triggered(device, dynamic_base + (f"SET MODE {mode}",)),
            )
        syndrum_base = (
            "SET MODE SYNDRUM", "SET TUNE_HZ 220", "SET DECAY_MS 500",
            "SET ECHO_LEVEL 0",
        )
        for name, wave, sweep in (
            ("syndrum_down", "SINE", "2"),
            ("syndrum_up", "SINE", "-2"),
            ("syndrum_sine", "SINE", "0"),
            ("syndrum_square", "SQUARE", "0"),
        ):
            save_wav(
                directory / f"{name}.wav",
                capture_triggered(
                    device,
                    syndrum_base + (
                        f"SET SYNDRUM_WAVE {wave}",
                        f"SET SYNDRUM_SWEEP {sweep}",
                    ),
                ),
            )
        gunshot_base = (
            "SET MODE GUNSHOT", "SET DECAY_MS 450", "SET DELAY_MS 180",
        )
        save_wav(
            directory / "gunshot_dry.wav",
            capture_triggered(
                device, gunshot_base + ("SET FEEDBACK 0", "SET ECHO_LEVEL 0")
            ),
        )
        save_wav(
            directory / "gunshot_echo.wav",
            capture_triggered(
                device,
                gunshot_base + ("SET FEEDBACK 0.60", "SET ECHO_LEVEL 0.65"),
            ),
        )
        device.send("ECHO_SPIN 0", "ECHO_LOOP 0", "ECHO_CUT 0",
                    "MOD_UP 0", "MOD_DOWN 0", "TRIGGER 0", "HOLD 0")
        device.status_event.clear()
        device.send("HELLO")
        if not device.status_event.wait(1.0):
            raise RuntimeError("STATUS timeout after capture")
        status = device.last_status
        print("Capture budget (us):", {
            "maxRenderUs": status["maxRenderUs"],
            "maxCycleUs": status["maxCycleUs"],
        })
        if status["maxRenderUs"] >= 10_000 or status["maxCycleUs"] >= 10_000:
            raise RuntimeError("Audio cycle exceeded 10 ms during capture")
        print(f"{CAPTURE_COUNT} WAV captures saved in {directory}")
    finally:
        device.close()


if __name__ == "__main__":
    main()
