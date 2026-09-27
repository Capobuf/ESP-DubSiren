import tempfile
import unittest
import wave
from pathlib import Path

from performance_capture import (
    CAPTURE_COUNT,
    SCENARIOS,
    capture_segments,
    capture_sequence,
    save_wav,
)


class FakeCapture:
    def __init__(self):
        self.commands = []

    def send(self, *commands):
        self.commands.extend(commands)

    def grab(self, blocks):
        return bytes(blocks * 960)


class PerformanceCaptureTests(unittest.TestCase):
    def test_scenarios_and_sweep_commands(self):
        self.assertEqual(len(SCENARIOS), 11)
        self.assertEqual(CAPTURE_COUNT, 17)
        names = {name for name, _commands in SCENARIOS}
        self.assertTrue({"lickshot_saw_down", "lickshot_triangle",
                         "lickshot_airhorn", "sonar"} <= names)
        capture = FakeCapture()
        pcm = capture_sequence(capture, "DELAY_MS", ("100", "800", "100"))
        self.assertEqual(capture.commands, ["SET DELAY_MS 100",
                                            "SET DELAY_MS 800",
                                            "SET DELAY_MS 100"])
        self.assertEqual(len(pcm), 3 * 10 * 960)

        segmented = capture_segments(capture, (
            (("MOD_UP 1",), 2),
            (("MOD_UP 0", "MOD_DOWN 1"), 3),
        ))
        self.assertEqual(capture.commands[-3:],
                         ["MOD_UP 1", "MOD_UP 0", "MOD_DOWN 1"])
        self.assertEqual(len(segmented), 5 * 960)

    def test_wav_format(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.wav"
            save_wav(path, bytes(960))
            with wave.open(str(path), "rb") as input_file:
                self.assertEqual(input_file.getparams()[:4], (1, 2, 48000, 480))


if __name__ == "__main__":
    unittest.main()
