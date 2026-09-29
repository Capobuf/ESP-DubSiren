import unittest

from control_ranges import CONTROL_SPECS, format_frequency


class ControlRangeTests(unittest.TestCase):
    def assert_mapping(self, name: str, normalized: float, expected: float) -> None:
        actual = CONTROL_SPECS[name].from_normalized(normalized)
        self.assertAlmostEqual(actual, expected, places=9)

    def test_required_endpoints_and_decay_midpoint(self) -> None:
        expected_ranges = {
            "TUNE_HZ": (80.0, 1600.0),
            "LFO_RATE_HZ": (0.15, 20.0),
            "LFO_DEPTH_OCT": (0.0, 2.0),
            "DECAY_MS": (0.0, 1500.0),
            "SWEEP": (0.0, 1.0),
            "SWEEP_MS": (50.0, 3000.0),
            "SYNDRUM_SWEEP": (-3.0, 3.0),
            "FEEDBACK": (0.0, 1.05),
            "HPF_HZ": (50.0, 4000.0),
            "LPF_HZ": (400.0, 16000.0),
        }
        for name, (minimum, maximum) in expected_ranges.items():
            with self.subTest(name=name, endpoint="minimum"):
                self.assert_mapping(name, 0.0, minimum)
            with self.subTest(name=name, endpoint="maximum"):
                self.assert_mapping(name, 1.0, maximum)
        self.assert_mapping("DECAY_MS", 0.5, 375.0)
        self.assert_mapping("SWEEP_MS", 0.5, 787.5)

    def test_feedback_curve(self) -> None:
        spec = CONTROL_SPECS["FEEDBACK"]
        for normalized in (0.0, 0.25, 0.5, 0.75, 0.9, 1.0):
            with self.subTest(normalized=normalized):
                self.assertAlmostEqual(
                    spec.from_normalized(normalized),
                    1.05 * (normalized**0.8),
                    places=9,
                )

    def test_required_round_trips(self) -> None:
        values = {
            "TUNE_HZ": 220.0,
            "LFO_RATE_HZ": 0.70,
            "DECAY_MS": 120.0,
            "SWEEP": 0.65,
            "SWEEP_MS": 650.0,
            "SYNDRUM_SWEEP": -2.0,
            "FEEDBACK": 0.62,
            "HPF_HZ": 60.0,
            "LPF_HZ": 7000.0,
        }
        for name, value in values.items():
            spec = CONTROL_SPECS[name]
            with self.subTest(name=name):
                normalized = spec.to_normalized(value)
                self.assertAlmostEqual(
                    spec.from_normalized(normalized), value, places=9
                )

    def test_defaults_and_labels(self) -> None:
        expected = {
            "TUNE_HZ": (220.0, "220 Hz"),
            "LFO_RATE_HZ": (0.70, "0.70 Hz"),
            "LFO_DEPTH_OCT": (1.0, "1.00 oct"),
            "DECAY_MS": (120.0, "120 ms"),
            "SWEEP": (0.0, "0 %"),
            "SWEEP_MS": (650.0, "650 ms"),
            "SYNDRUM_SWEEP": (2.0, "+2.00 oct"),
            "DELAY_MS": (360.0, "360 ms"),
            "FEEDBACK": (0.62, "62 %"),
            "ECHO_LEVEL": (0.45, "45 %"),
            "HPF_HZ": (60.0, "60 Hz"),
            "LPF_HZ": (7000.0, "7.0 kHz"),
            "MASTER": (0.50, "50 %"),
        }
        for name, (default, label) in expected.items():
            spec = CONTROL_SPECS[name]
            with self.subTest(name=name):
                self.assertEqual(spec.default, default)
                self.assertEqual(spec.formatter(spec.default), label)

    def test_echo_time_logarithmic_range(self):
        self.assert_mapping("DELAY_MS", 0, 50)
        self.assert_mapping("DELAY_MS", 1, 1000)
        self.assert_mapping("DELAY_MS", 0.5, (50 * 1000) ** 0.5)

    def test_frequency_formatting(self) -> None:
        self.assertEqual(format_frequency(880.0), "880 Hz")
        self.assertEqual(format_frequency(1200.0), "1.2 kHz")
        self.assertEqual(format_frequency(4000.0), "4.0 kHz")
        self.assertEqual(format_frequency(16000.0), "16.0 kHz")


if __name__ == "__main__":
    unittest.main()
