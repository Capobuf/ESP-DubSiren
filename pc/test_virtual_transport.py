import array
import queue
import time
import unittest

from protocol import PROTOCOL_VERSION
from virtual_transport import VIRTUAL_DEVICE, VirtualTransport


class VirtualTransportTests(unittest.TestCase):
    def setUp(self):
        self.audio_queue = queue.Queue(maxsize=128)
        self.statuses = []
        self.transport = VirtualTransport(
            self.audio_queue,
            self.statuses.append,
        )
        self.transport.connect(VIRTUAL_DEVICE)

    def tearDown(self):
        if self.transport.connected:
            self.transport.disconnect()

    def _drain(self):
        while True:
            try:
                self.audio_queue.get_nowait()
            except queue.Empty:
                return

    def _assert_non_silent_audio(self, timeout=1.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                pcm = self.audio_queue.get(timeout=0.1)
            except queue.Empty:
                continue
            samples = array.array("h")
            samples.frombytes(pcm)
            if any(samples):
                return
        self.fail("Virtual ESP did not produce non-silent PCM")

    def _trigger_voice(self, voice):
        self.transport.send("TRIGGER 0")
        time.sleep(0.03)
        self._drain()
        self.transport.send(f"SET MODE {voice}")
        self.transport.send("TRIGGER 1")
        self._assert_non_silent_audio()
        self.transport.send("TRIGGER 0")

    def test_shared_dsp_controls_audio_transients_and_reconnect(self):
        self.assertEqual(len(self.statuses), 1)
        self.assertTrue(self.statuses[0]["virtual"])
        self.assertEqual(self.statuses[0]["protocol"], PROTOCOL_VERSION)
        self.assertEqual(
            [pad["sound"] for pad in self.statuses[0]["panel"]["pads"]],
            ["CLASSIC", "TWO TONE", "LICKSHOT", "PING"],
        )

        commands = (
            "SET PROFILE EXTENDED",
            "SET TUNE_HZ 330",
            "SET LFO_RATE_HZ 2.5",
            "SET ECHO_LEVEL 0.7",
            "SET DELAY_MS 80",
            "SET FEEDBACK 0.65",
            "SET HPF_HZ 60",
            "SET LPF_HZ 16000",
            "SET MASTER 0.8",
            "SET LFO_SHAPE SAW_DOWN",
            "SET LFO_DEPTH_OCT 1.25",
            "SET DECAY_MS 300",
            "SET SWEEP 0.5",
            "SET SWEEP_MS 400",
            "SET SYNDRUM_WAVE TRIANGLE",
            "SET SYNDRUM_SWEEP -1.5",
            "MODULATION 1",
            "RETRIGGER 1",
            "ECHO_LOOP 0",
            "HOLD 0",
            "MOD_DOWN 0",
            "MOD_UP 0",
            "ECHO_CUT 0",
        )
        for command in commands:
            self.transport.send(command)

        self.transport.send("STREAM 1")
        self._trigger_voice("SYNDRUM")
        self._trigger_voice("GUNSHOT")

        self._drain()
        self.transport.send("SET MODE LICKSHOT")
        self.transport.send("HOLD 1")
        self._assert_non_silent_audio()
        self.transport.send("SET HPF_HZ 4000")
        self.transport.send("SET LPF_HZ 16000")
        self._assert_non_silent_audio()
        self.transport.send("ECHO_CUT 1")
        self.transport.send("ECHO_CUT 0")
        self.transport.send("HOLD 0")
        self.transport.send("STREAM 0")

        count = len(self.statuses)
        self.transport.disconnect()
        self.assertFalse(self.transport.connected)
        self.transport.connect(VIRTUAL_DEVICE)
        self.assertTrue(self.transport.connected)
        self.assertEqual(len(self.statuses), count + 1)

    def test_shared_panel_priority_combinations_and_one_shot(self):
        self.transport.send("SET PROFILE EXTENDED")
        self.transport.send("STREAM 1")
        self.transport.send("PAD 1 1")
        self.assertEqual(self.statuses[-1]["panel"]["activePad"], 1)
        self.transport.send("PAD 2 1")
        self.assertEqual(self.statuses[-1]["panel"]["activePad"], 2)
        self.transport.send("PAD 2 0")
        self.assertEqual(self.statuses[-1]["panel"]["activePad"], 1)
        self.transport.send("PAD 1 0")
        self.assertEqual(self.statuses[-1]["panel"]["activePad"], 0)

        menu_revision = self.statuses[-1]["panel"]["menuRevision"]
        self.transport.send("ENCODER_SWITCH 1")
        self.transport.send("PAD 3 1")
        self.transport.send("PAD 3 0")
        self.transport.send("ENCODER_SWITCH 0")
        panel = self.statuses[-1]["panel"]
        self.assertEqual(panel["menuRevision"], menu_revision)
        self.assertEqual(panel["pads"][0]["variant"], "SINE 2")
        self.assertEqual(panel["activePad"], 0)

        self._drain()
        self.transport.send("PAD 4 1")
        self.transport.send("PAD 4 0")
        self._assert_non_silent_audio()

    def test_display_state_and_shared_encoder_menu(self):
        self.transport.send("SET PROFILE EXTENDED")
        display = self.statuses[-1]["panel"]["display"]
        self.assertEqual((display["width"], display["height"]), (320, 170))
        self.assertEqual(display["mode"], "HOME")

        self.transport.send("ENCODER_ROTATE NEXT")
        display = self.statuses[-1]["panel"]["display"]
        self.assertAlmostEqual(display["depth"], 1.05, places=4)

        self.transport.send("ENCODER_SWITCH 1")
        self.transport.send("ENCODER_SWITCH 0")
        display = self.statuses[-1]["panel"]["display"]
        self.assertEqual(display["mode"], "MENU")
        self.assertEqual(display["menuIndex"], 0)
        self.assertFalse(display["editing"])

        self.transport.send("ENCODER_ROTATE NEXT")
        self.assertEqual(self.statuses[-1]["panel"]["display"]["menuIndex"], 1)
        self.transport.send("ENCODER_SWITCH 1")
        self.transport.send("ENCODER_SWITCH 0")
        self.assertTrue(self.statuses[-1]["panel"]["display"]["editing"])
        self.transport.send("ENCODER_ROTATE NEXT")
        self.assertEqual(self.statuses[-1]["panel"]["pads"][0]["sound"], "TWO TONE")
        self.transport.send("ENCODER_SWITCH 1")
        self.transport.send("ENCODER_SWITCH 0")
        self.assertFalse(self.statuses[-1]["panel"]["display"]["editing"])

        self.transport.send("ENCODER_ROTATE NEXT")
        self.transport.send("ENCODER_SWITCH 1")
        self.transport.send("ENCODER_SWITCH 0")
        self.transport.send("ENCODER_ROTATE NEXT")
        self.assertEqual(self.statuses[-1]["panel"]["pads"][0]["variant"], "TRIANGLE")
        self.transport.send("ENCODER_SWITCH 1")
        self.transport.send("ENCODER_SWITCH 0")

        self.transport.send("ENCODER_ROTATE NEXT")
        self.assertEqual(
            self.statuses[-1]["panel"]["display"]["menuItems"][
                self.statuses[-1]["panel"]["display"]["menuIndex"]
            ],
            "HOLD",
        )
        self.transport.send("ENCODER_SWITCH 1")
        self.transport.send("ENCODER_SWITCH 0")
        self.assertTrue(self.statuses[-1]["panel"]["pads"][0]["hold"])

        self.transport.send("ENCODER_ROTATE NEXT")
        self.transport.send("ENCODER_SWITCH 1")
        self.transport.send("ENCODER_SWITCH 0")
        self.assertTrue(self.statuses[-1]["panel"]["display"]["echoLoop"])

        self.transport.send("ENCODER_ROTATE NEXT")
        self.transport.send("ENCODER_ROTATE NEXT")
        self.assertEqual(
            self.statuses[-1]["panel"]["display"]["menuItems"][
                self.statuses[-1]["panel"]["display"]["menuIndex"]
            ],
            "BACK",
        )
        self.transport.send("ENCODER_SWITCH 1")
        self.transport.send("ENCODER_SWITCH 0")
        self.assertEqual(self.statuses[-1]["panel"]["display"]["mode"], "HOME")

    def test_classic_default_selectors_buttons_and_audio_release(self):
        def display():
            return self.statuses[-1]["panel"]["display"]
        self.assertEqual(display()["profile"], "CLASSIC")
        self.assertEqual(display()["voice"], "SINE 1")
        self.transport.send("SET PITCH_POSITION 0.9")
        self.transport.send("SET RATE_POSITION 1")
        self.assertEqual(display()["pitch"], "HIGH")
        self.assertEqual(display()["rate"], "MANUAL")
        for voice in ("SINE 2", "TEST TONE", "SQUARE", "SINE 1"):
            self.transport.send("ENCODER_ROTATE NEXT")
            self.assertEqual(display()["voice"], voice)
            self.assertEqual(display()["depth"], 1.0)
        self.transport.send("SET ECHO_LEVEL 0")
        self.transport.send("SET FEEDBACK 0")
        self.transport.send("STREAM 1")
        self.transport.send("PAD 1 1")
        self._assert_non_silent_audio()
        self.transport.send("PAD 2 1")
        self.transport.send("PAD 2 0")
        self.transport.send("PAD 1 0")
        self.assertTrue(display()["hold"])
        self._drain()
        self._assert_non_silent_audio()
        self.transport.send("PAD 2 1")
        self.transport.send("PAD 2 0")
        self.assertFalse(display()["hold"])
        time.sleep(0.04)
        self._drain()
        for _ in range(4):
            values = array.array("h", self.audio_queue.get(timeout=0.2))
            self.assertFalse(any(values))
        self.transport.send("PAD 4 1")
        self.assertTrue(display()["echoCut"])
        self.transport.send("PAD 4 0")
        self.assertFalse(display()["echoCut"])


if __name__ == "__main__":
    unittest.main()
