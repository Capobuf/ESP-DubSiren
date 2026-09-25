"""Hardware-level PCM metrics for legacy/V2 voicing comparisons."""

import argparse
import math
import queue
import struct
import threading
import time

from transport import SerialTransport


SAMPLE_RATE = 48_000
BASE_FREQUENCY = 220.0


def unpack(pcm: bytes) -> tuple[int, ...]:
    return struct.unpack(f"<{len(pcm) // 2}h", pcm)


def harmonic_levels(values: tuple[int, ...], harmonics: int = 8) -> list[float]:
    levels = []
    count = len(values)
    for harmonic in range(1, harmonics + 1):
        angular = 2.0 * math.pi * BASE_FREQUENCY * harmonic / SAMPLE_RATE
        real = 0.0
        imaginary = 0.0
        for index, value in enumerate(values):
            real += value * math.cos(angular * index)
            imaginary -= value * math.sin(angular * index)
        levels.append(2.0 * math.hypot(real, imaginary) / count)
    fundamental = max(levels[0], 1.0)
    return [round(20.0 * math.log10(max(level, 1.0) / fundamental), 1)
            for level in levels]


def metrics(pcm: bytes, include_harmonics: bool) -> dict:
    values = unpack(pcm)
    rms = math.sqrt(sum(value * value for value in values) / len(values))
    result = {
        "rms": round(rms, 1),
        "peak": max(abs(value) for value in values),
        "dc": round(sum(values) / len(values), 1),
        "crest": round(max(abs(value) for value in values) / max(rms, 1.0), 2),
    }
    if include_harmonics:
        result["harmonics_db"] = harmonic_levels(values)
    return result


class Capture:
    def __init__(self, port: str) -> None:
        self.audio: queue.Queue[bytes] = queue.Queue(maxsize=300)
        self.status_event = threading.Event()
        self.last_status: dict = {}
        self.transport = SerialTransport(self.audio, self._status)
        self.transport.connect(port)
        if not self.status_event.wait(2.0):
            raise RuntimeError("STATUS timeout")

    def _status(self, status: dict) -> None:
        self.last_status = status
        self.status_event.set()

    def send(self, *commands: str) -> None:
        for command in commands:
            self.transport.send(command)

    def drain(self) -> None:
        while True:
            try:
                self.audio.get_nowait()
            except queue.Empty:
                return

    def grab(self, blocks: int) -> bytes:
        return b"".join(self.audio.get(timeout=0.3) for _ in range(blocks))

    def configure(self) -> None:
        self.send(
            "SET OUTPUT PC",
            "SET TUNE_HZ 220",
            "SET LFO_SHAPE TRIANGLE",
            "SET LFO_RATE_HZ 4",
            "SET LFO_DEPTH_OCT 0",
            "SET DECAY_MS 0",
            "SET ECHO_LEVEL 0",
            "SET MASTER 0.5",
            "HOLD 1",
            "STREAM 1",
        )
        time.sleep(0.25)
        self.drain()

    def capture(self, mode: str, voicing: str, blocks: int = 50) -> bytes:
        self.send(f"SET MODE {mode}", f"SET VOICING {voicing}")
        time.sleep(0.20)
        self.drain()
        return self.grab(blocks)

    def close(self) -> None:
        try:
            self.send("STREAM 0", "HOLD 0")
        finally:
            self.transport.disconnect()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("port")
    parser.add_argument(
        "--modes",
        nargs="+",
        default=["SINE1", "SINE2", "TEST_TONE", "SQUARE"],
    )
    args = parser.parse_args()

    capture = Capture(args.port)
    try:
        capture.configure()
        for mode in args.modes:
            for voicing in ("LEGACY", "V2"):
                pcm = capture.capture(mode, voicing)
                print(mode, voicing, metrics(pcm, mode in ("SINE1", "SINE2")))
    finally:
        capture.close()


if __name__ == "__main__":
    main()
