"""Capture ten comparable firmware scenarios as temporary WAV files.

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
)


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
        print(f"10 WAV captures saved in {directory}")
    finally:
        device.close()


if __name__ == "__main__":
    main()
