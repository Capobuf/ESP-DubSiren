import tempfile
import unittest
import wave
from pathlib import Path

from performance_capture import SCENARIOS, capture_sequence, save_wav


class FakeCapture:
    def __init__(self):
        self.commands = []

    def send(self, command):
        self.commands.append(command)

    def grab(self, blocks):
        return bytes(blocks * 960)


class PerformanceCaptureTests(unittest.TestCase):
    def test_scenarios_and_sweep_commands(self):
        self.assertEqual(len(SCENARIOS), 7)
        capture = FakeCapture()
        pcm = capture_sequence(capture, "DELAY_MS", ("100", "800", "100"))
        self.assertEqual(capture.commands, ["SET DELAY_MS 100",
                                            "SET DELAY_MS 800",
                                            "SET DELAY_MS 100"])
        self.assertEqual(len(pcm), 3 * 10 * 960)

    def test_wav_format(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.wav"
            save_wav(path, bytes(960))
            with wave.open(str(path), "rb") as input_file:
                self.assertEqual(input_file.getparams()[:4], (1, 2, 48000, 480))


if __name__ == "__main__":
    unittest.main()
