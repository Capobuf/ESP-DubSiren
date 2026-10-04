import tempfile
import unittest
import wave
from pathlib import Path

from performance_capture import (
    CAPTURE_COUNT,
    SCENARIOS,
    TRANSIENT_CAPTURE_NAMES,
    capture_segments,
    capture_sequence,
    capture_triggered,
    save_wav,
)


class FakeCapture:
    def __init__(self):
        self.commands = []

    def send(self, *commands):
        self.commands.extend(commands)

    def grab(self, blocks):
        return bytes(blocks * 960)

    def drain(self):
        pass


class PerformanceCaptureTests(unittest.TestCase):
    def test_scenarios_and_sweep_commands(self):
        self.assertEqual(len(SCENARIOS), 13)
        self.assertEqual(CAPTURE_COUNT, 28)
        self.assertEqual(len(TRANSIENT_CAPTURE_NAMES), 9)
        self.assertIn("dynamic_sweep_sine1", TRANSIENT_CAPTURE_NAMES)
        self.assertIn("gunshot_echo", TRANSIENT_CAPTURE_NAMES)
        names = {name for name, _commands in SCENARIOS}
        self.assertTrue({"extended_saw", "extended_triangle",
                         "lickshot_saw_down", "lickshot_triangle",
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

        capture.commands.clear()
        triggered = capture_triggered(
            capture, ("SET MODE SYNDRUM", "SET SYNDRUM_SWEEP -2"), 4
        )
        self.assertEqual(
            capture.commands,
            ["HOLD 0", "TRIGGER 0", "SET MODE SYNDRUM",
             "SET SYNDRUM_SWEEP -2", "TRIGGER 1", "TRIGGER 0"],
        )
        self.assertEqual(len(triggered), 4 * 960)

    def test_wav_format(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.wav"
            save_wav(path, bytes(960))
            with wave.open(str(path), "rb") as input_file:
                self.assertEqual(input_file.getparams()[:4], (1, 2, 48000, 480))


if __name__ == "__main__":
    unittest.main()
