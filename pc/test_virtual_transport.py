import array
import queue
import time
import unittest
from unittest.mock import patch

import virtual_transport
from protocol import PROTOCOL_VERSION
from virtual_transport import VIRTUAL_DEVICE, VirtualTransport


class VirtualTransportTests(unittest.TestCase):
    def test_command_results_report_sync_rejections_and_confirmed_values(self):
        results = []
        self.transport._command_callback = results.append
        self.transport.send("TEMPO BPM 120")
        self.transport.send("TEMPO LFO SYNC")
        self.transport.send("TEMPO ECHO SYNC")
        before = self.statuses[-1]["panel"]["display"]
        for command in ("SET LFO_RATE_HZ 2", "SET DELAY_MS 400", "SET OUTPUT INVALID"):
            with self.assertRaisesRegex(RuntimeError, "rejected"):
                self.transport.send(command)
            self.assertEqual(results[-1], {"command": command, "accepted": False})
        after = self.statuses[-1]["panel"]["display"]
        self.assertEqual((after["rateHz"], after["delayMs"]), (before["rateHz"], before["delayMs"]))
        self.transport.send("TEMPO ECHO FREE")
        self.transport.send("SET DELAY_MS 400")
        self.assertEqual(results[-1], {"command": "SET DELAY_MS 400", "accepted": True})
        self.assertEqual(self.statuses[-1]["panel"]["display"]["delayMs"], 400)

    def test_loaded_library_is_not_relinked_and_stale_sources_ask_for_restart(self):
        self.transport.disconnect()
        with patch("virtual_transport.subprocess.run") as run:
            with patch.object(virtual_transport._build, "needs_build", return_value=True):
                with self.assertRaisesRegex(RuntimeError, "restart the GUI"):
                    self.transport.connect(VIRTUAL_DEVICE)
            with patch.object(virtual_transport._build, "needs_build", return_value=False):
                self.transport.connect(VIRTUAL_DEVICE)
        run.assert_not_called()
        self.assertTrue(self.transport.connected)

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

    def _sound_pads(self):
        for pad, sound in enumerate(("SIREN", "TWO_TONE", "LICKSHOT", "PING"), start=1):
            self.transport.send(f"PANEL BUTTON {pad} ACTION PLAY_SOUND")
            self.transport.send(f"PANEL BUTTON {pad} SOUND {sound} 0 0")

    def _trigger_voice(self, voice):
        self.transport.send("TRIGGER 0")
        time.sleep(0.03)
        self._drain()
        self.transport.send(f"SET MODE {voice}")
        self.transport.send("TRIGGER 1")
        self._assert_non_silent_audio()
        self.transport.send("TRIGGER 0")

    def test_saved_patches_survive_file_and_core_restart(self):
        import tempfile
        from pathlib import Path
        from physical_ui import save_panel_config, load_panel_config, panel_settings_commands
        self._sound_pads()
        for pad in range(1, 5):
            for command in (f"PAD {pad} 1", f"PAD {pad} 0", f"SET TUNE_HZ {220 * pad}",
                            f"SET DECAY_MS {150 * pad}", f"PANEL SAVE {pad}"):
                self.transport.send(command)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "panel.json"
            save_panel_config(path, self.statuses[-1]["panel"]["settings"])
            settings = load_panel_config(path)
        self.transport.disconnect()
        self.transport.connect(VIRTUAL_DEVICE)
        for command in panel_settings_commands(settings):
            self.transport.send(command)
        self.assertFalse(self.statuses[-1]["panel"]["display"]["hold"])
        for pad in range(1, 5):
            self.transport.send(f"PANEL RECALL {pad}")
            self.assertAlmostEqual(self.statuses[-1]["panel"]["display"]["tuneHz"], 220 * pad)

    def test_tempo_status_menu_and_button_persistence(self):
        import json
        import tempfile
        from pathlib import Path
        from physical_ui import save_panel_config, load_panel_config, panel_settings_commands

        def display():
            return self.statuses[-1]["panel"]["display"]

        def click():
            self.transport.send("ENCODER_SWITCH 1")
            self.transport.send("ENCODER_SWITCH 0")

        def select(key):
            rows = display()["rows"]
            target = next(i for i, row in enumerate(rows) if row["key"] == key)
            for _ in range((target - display()["menuIndex"]) % len(rows)):
                self.transport.send("ENCODER_ROTATE NEXT")
            click()

        for command in ("TEMPO BPM 120", "TEMPO ECHO SYNC", "TEMPO LFO SYNC",
                        "TEMPO ECHO_DIV 1/8D", "TEMPO LFO_DIV 1/8T",
                        "PANEL BUTTON 4 ACTION TAP_TEMPO"):
            self.transport.send(command)
        tempo = display()["tempo"]
        self.assertEqual(tempo["echoMs"], 375)
        self.assertEqual(tempo["lfoHz"], 6)
        self.assertEqual(display()["delayMs"], 375)
        self.assertEqual(display()["rateHz"], 6)
        self.assertIn("120.0 BPM", tempo["status"])
        self.assertIn("1/8D", tempo["echoText"])
        click()
        select("ECHO")
        self.assertIn("375.0 ms", next(r["value"] for r in display()["rows"] if r["key"] == "ECHO_DIV"))
        select("ECHO_DIV")
        self.transport.send("ENCODER_ROTATE NEXT")
        click()
        self.assertAlmostEqual(display()["tempo"]["echoMs"], 500 / 3, places=3)
        select("TAP_TEMPO")
        self.assertFalse(display()["editing"])
        self.assertIn("TAP AGAIN", display()["footer"])
        self.transport.send("TEMPO BPM 30")
        self.transport.send("TEMPO ECHO_DIV 1/4")
        self.assertFalse(display()["tempo"]["echoAvailable"])
        self.assertIn("N/A HOLD", display()["tempo"]["echoText"])
        self.assertLess(len(json.dumps(self.statuses[-1]).encode()), 8192)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "panel.json"
            save_panel_config(path, self.statuses[-1]["panel"]["settings"])
            commands = panel_settings_commands(load_panel_config(path))
        self.transport.disconnect()
        self.transport.connect(VIRTUAL_DEVICE)
        for command in commands:
            self.transport.send(command)
        self.assertEqual(display()["buttons"][3], "TAP TEMPO")
        self.assertFalse(display()["tempo"]["known"])
        self.assertFalse(display()["tempo"]["echoSync"])
        self.assertFalse(display()["tempo"]["lfoSync"])
        self.transport.send("TAP_TEMPO")
        self.assertEqual(display()["overlay"]["value"], "TAP AGAIN")
        self.transport.send("TAP_TEMPO")
        self.assertEqual(display()["overlay"]["value"], "TOO FAST - IGNORED")

    def test_all_panel_menu_pages_fit_real_tk_canvas(self):
        import tkinter as tk
        from panel_display import PanelDisplay
        try:
            root = tk.Tk()
        except tk.TclError as exc:
            self.skipTest(f"Tk display unavailable: {exc}")
        root.withdraw()
        self.addCleanup(root.destroy)
        renderer = PanelDisplay(root)

        def display():
            return self.statuses[-1]["panel"]["display"]

        def click():
            self.transport.send("ENCODER_SWITCH 1")
            self.transport.send("ENCODER_SWITCH 0")

        def select(key):
            rows = display()["rows"]
            target = next(i for i, row in enumerate(rows) if row["key"] == key)
            for _ in range((target - display()["menuIndex"]) % len(rows)):
                self.transport.send("ENCODER_ROTATE NEXT")
            click()

        def assert_bounds():
            for item in renderer.canvas.find_all():
                bounds = renderer.canvas.bbox(item)
                self.assertIsNotNone(bounds)
                left, top, right, bottom = bounds
                self.assertTrue(8 <= left < right <= 312, bounds)
                self.assertTrue(0 <= top < bottom <= 162, bounds)

        paths = [[], ["SOUND"], ["MOD"], ["ECHO"], ["SETUP"],
                 ["SETUP", "POTS"], ["SETUP", "BUTTONS"],
                 ["SETUP", "ENCODER"], ["SETUP", "RESET_DEFAULTS"]]
        paths += [["SETUP", "POTS", f"POT_{i}"] for i in range(1, 7)]
        paths += [["SETUP", "BUTTONS", f"BTN_{i}"] for i in range(1, 5)]
        for command in ("TEMPO BPM 30", "TEMPO ECHO SYNC", "TEMPO LFO SYNC"):
            self.transport.send(command)
        # The factory layout first, then four sound pads with their own menu rows.
        for sound_pads in (False, True):
            if sound_pads:
                self._sound_pads()
            for path in paths:
                with self.subTest(sound_pads=sound_pads, path=path):
                    while display()["mode"] == "MENU":
                        select("BACK")
                    click()
                    for key in path:
                        select(key)
                    view = display()
                    for index in range(len(view["rows"])):
                        renderer.render_menu([(r["label"], r["value"]) for r in view["rows"]],
                                             index, False, view["title"], view["footer"])
                        assert_bounds()
        renderer.canvas.delete("all")
        renderer.render_overlay("PITCH", "DOWN PICKUP - HIGH - 9000 Hz")
        assert_bounds()
        tempo = display()["tempo"]
        for pad_selected, sound in ((False, "TWO TONE TRIANGLE"), (True, "TWO TONE")):
            renderer.render_home(1, 1, sound, "TRIANGLE", "1 oct", False, False, True,
                                 {"PAD_SELECTED": pad_selected, "TEMPO": tempo})
            assert_bounds()

    def test_shared_dsp_controls_audio_transients_and_reconnect(self):
        self.assertEqual(len(self.statuses), 1)
        self.assertTrue(self.statuses[0]["virtual"])
        self.assertEqual(self.statuses[0]["protocol"], PROTOCOL_VERSION)
        self.assertEqual(
            [pad["sound"] for pad in self.statuses[0]["panel"]["pads"]],
            ["TRIGGER", "HOLD", "MOD UP", "ECHO CUT"],
        )

        commands = (
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
        self._sound_pads()
        self.assertEqual(
            [pad["sound"] for pad in self.statuses[-1]["panel"]["pads"]],
            ["SIREN", "TWO TONE", "LICKSHOT", "PING"],
        )
        self.assertEqual(self.statuses[-1]["panel"]["pads"][1]["variant"], "SIREN")
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
        display = self.statuses[-1]["panel"]["display"]
        self.assertEqual((display["width"], display["height"]), (320, 172))
        self.assertEqual(display["mode"], "HOME")
        self.assertNotIn("profile", display)
        self.assertFalse(display["padSelected"])

        self.transport.send("ENCODER_ROTATE NEXT")
        display = self.statuses[-1]["panel"]["display"]
        self.assertEqual(display["voice"], "SINE 2")
        self.assertAlmostEqual(display["depth"], 1.0, places=4)
        self.assertEqual(display["liveTranspose"], 0)

        self.transport.send("ENCODER_SWITCH 1")
        self.transport.send("ENCODER_SWITCH 0")
        display = self.statuses[-1]["panel"]["display"]
        self.assertEqual(display["mode"], "MENU")
        self.assertEqual(display["menuIndex"], 0)
        self.assertFalse(display["editing"])

        for _ in range(3):
            self.transport.send("ENCODER_ROTATE NEXT")
        self.transport.send("ENCODER_SWITCH 1")
        self.transport.send("ENCODER_SWITCH 0")
        display = self.statuses[-1]["panel"]["display"]
        self.assertEqual(display["title"], "SETUP")
        self.assertEqual([row["key"] for row in display["rows"]],
                         ["POTS", "BUTTONS", "ENCODER", "RESET_DEFAULTS", "BACK"])
        self.transport.send("ENCODER_SWITCH 1")
        self.transport.send("ENCODER_SWITCH 0")
        display = self.statuses[-1]["panel"]["display"]
        self.assertEqual(display["title"], "POT CONTROLS")
        self.assertEqual(display["rows"][0]["label"], "BEHAVIOR")

        self.transport.send("ENCODER_ROTATE NEXT")
        self.transport.send("ENCODER_SWITCH 1")
        self.transport.send("ENCODER_SWITCH 0")
        display = self.statuses[-1]["panel"]["display"]
        self.assertTrue(display["title"].startswith("POT 1"))
        self.assertEqual(display["rows"][0]["key"], "FUNCTION")

        self.transport.send("ENCODER_SWITCH 1")
        self.transport.send("ENCODER_SWITCH 0")
        self.assertTrue(self.statuses[-1]["panel"]["display"]["editing"])
        self.transport.send("ENCODER_ROTATE NEXT")
        self.assertEqual(
            self.statuses[-1]["panel"]["settings"]["pots"][0]["target"],
            "RATE",
        )
        self.transport.send("ENCODER_SWITCH 1")
        self.transport.send("ENCODER_SWITCH 0")
        self.assertFalse(self.statuses[-1]["panel"]["display"]["editing"])

    def test_profile_commands_are_gone_and_any_button_can_be_a_sound_pad(self):
        def panel():
            return self.statuses[-1]["panel"]

        self.assertEqual(panel()["settings"]["schema"], 3)
        self.assertNotIn("profiles", panel()["settings"])
        self.assertEqual([pot["target"] for pot in panel()["settings"]["pots"]],
                         ["PITCH", "RATE", "ECHO LEVEL", "ECHO TIME",
                          "ECHO FEEDBACK", "ECHO FILTER"])
        self.assertEqual([pot["mode"] for pot in panel()["settings"]["pots"][:2]],
                         ["ZONES", "ZONES"])
        self.assertEqual(panel()["settings"]["pots"][0]["zones"], [330.0, 680.0, 1000.0])
        self.assertEqual(panel()["display"]["buttons"],
                         ["TRIGGER", "HOLD", "MOD UP", "ECHO CUT"])
        for command in ("SET PROFILE CLASSIC", "SET PROFILE EXTENDED",
                        "PANEL POT CLASSIC 1 TARGET PITCH",
                        "PANEL BUTTON EXTENDED 1 ACTION PLAY_SOUND",
                        # BTN3 is MOD UP: only a PLAY_SOUND button takes a pad sound.
                        "PAD_CONFIG 3 PING 1 0"):
            with self.subTest(command=command), self.assertRaisesRegex(RuntimeError, "rejected"):
                self.transport.send(command)
        # Formerly profile-bound commands, targets and actions are always accepted.
        for command in ("SET MODE SAW", "SET MODE GUNSHOT", "SET LICKSHOT_MODE HORN",
                        "SET LICKSHOT_LENGTH_MS 200", "SET DRY_TONE_HZ 1200",
                        "SET FILTER_CONTOUR_OCT 2", "SET FILTER_CONTOUR_MS 300",
                        "SET ECHO_COLOR 0.5", "SET LFO_SHAPE CLASSIC", "SET LFO_SHAPE SIREN",
                        "ECHO_LOOP 1", "ECHO_LOOP 0", "ECHO_SPIN 1", "ECHO_SPIN 0",
                        "PANEL POT 3 TARGET LFO_DEPTH", "PANEL POT 4 TARGET SWEEP_TIME",
                        "PANEL BUTTON 4 ACTION ECHO_SPIN",
                        "PANEL BUTTON 3 ACTION PLAY_SOUND", "PAD_CONFIG 3 PING 1 0",
                        "PANEL BUTTON 2 ACTION PLAY_SOUND",
                        "PANEL BUTTON 2 SOUND CLASSIC 3 1"):
            self.transport.send(command)
        self.assertEqual(panel()["display"]["buttons"],
                         ["TRIGGER", "PLAY SOUND", "PLAY SOUND", "ECHO SPIN"])
        self.assertEqual(panel()["settings"]["buttons"][1],
                         {"action": "PLAY SOUND", "sound": "SIREN", "variant": 3, "latch": True})
        self.assertEqual(
            [(pad["sound"], pad["variant"]) for pad in panel()["pads"]],
            [("TRIGGER", "N/A"), ("SIREN", "SAW"), ("PING", "TRIANGLE"), ("ECHO SPIN", "N/A")],
        )

    def test_held_sound_button_turn_is_live_pitch_and_release_restores_variant_selection(self):
        self._sound_pads()
        self.transport.send("PAD 1 1")
        self.transport.send("ENCODER_ROTATE NEXT")
        panel = self.statuses[-1]["panel"]
        self.assertTrue(panel["display"]["padSelected"])
        self.assertEqual(panel["display"]["overlay"]["title"], "LIVE PITCH")
        self.assertEqual(panel["display"]["liveTranspose"], 1)
        self.assertAlmostEqual(panel["display"]["depth"], 1.0, places=4)
        self.assertEqual(panel["pads"][0]["variant"], "SINE 1")
        self.transport.send("PAD 1 0")
        self.transport.send("ENCODER_ROTATE NEXT")
        panel = self.statuses[-1]["panel"]
        self.assertEqual(panel["pads"][0]["variant"], "SINE 2")
        self.assertEqual(panel["display"]["liveTranspose"], 0)
        self.assertFalse(panel["display"]["hold"])

    def test_trigger_voice_encoder_cycle_and_audio(self):
        def display():
            return self.statuses[-1]["panel"]["display"]
        for command in ("SET ECHO_LEVEL 0", "STREAM 1"):
            self.transport.send(command)
        self.assertEqual(display()["voice"], "SINE 1")
        for voice in ("SINE 2", "TEST TONE", "SQUARE", "SONAR", "SAW", "TRIANGLE",
                      "TWO TONE TRIANGLE", "TWO TONE SQUARE", "LICKSHOT SAW DOWN",
                      "LICKSHOT ASYM DOWN", "LICKSHOT SHOT", "LICKSHOT ROLL",
                      "LICKSHOT HORN", "PING SINE", "PING TRIANGLE", "PING SQUARE",
                      "GUNSHOT", "SINE 1"):
            self.transport.send("ENCODER_ROTATE NEXT")
            self.assertEqual(display()["voice"], voice)
            self.assertFalse(display()["padSelected"])
            if voice in ("SAW", "TRIANGLE", "LICKSHOT HORN", "PING SINE"):
                self._drain()
                self.transport.send("PAD 1 1")
                self._assert_non_silent_audio()
                self.transport.send("PAD 1 0")
            if voice in ("SAW", "TRIANGLE"):
                # Sustained voices use the short gate: silent right after release.
                time.sleep(0.04)
                self._drain()
                for _ in range(4):
                    self.assertFalse(any(array.array("h", self.audio_queue.get(timeout=0.2))))
        self.transport.send("ENCODER_ROTATE PREV")
        self.assertEqual(display()["voice"], "GUNSHOT")

    def test_pickup_live_pitch_and_menu_footer_share_controller_state(self):
        self.transport.send("POT 1 0")
        display = self.statuses[-1]["panel"]["display"]
        self.assertEqual(display["overlay"]["title"], "PITCH")
        self.assertIn("UP PICKUP", display["overlay"]["value"])

        self.transport.send("PAD 1 1")
        self.transport.send("ENCODER_ROTATE NEXT")
        display = self.statuses[-1]["panel"]["display"]
        self.assertEqual(display["overlay"]["title"], "LIVE PITCH")
        self.assertEqual(display["liveTranspose"], 1)
        self.transport.send("PAD 1 0")
        self.assertEqual(self.statuses[-1]["panel"]["display"]["liveTranspose"], 0)

        self.transport.send("ENCODER_SWITCH 1")
        self.transport.send("ENCODER_SWITCH 0")
        self.transport.send("POT 3 0")
        display = self.statuses[-1]["panel"]["display"]
        self.assertEqual(display["mode"], "MENU")
        self.assertEqual(display["title"], "PERFORMANCE")
        self.assertIn("ECHO LEVEL", display["footer"])

    def test_default_selectors_buttons_and_audio_release(self):
        def display():
            return self.statuses[-1]["panel"]["display"]
        self.assertEqual(display()["voice"], "SINE 1")
        self.assertEqual((display()["pitchMode"], display()["rateMode"]), ("ZONES", "ZONES"))
        self.transport.send("SET PITCH_POSITION 0.9")
        self.transport.send("SET RATE_POSITION 1")
        self.assertEqual(display()["pitch"], "HIGH")
        self.assertEqual(display()["tuneHz"], 1000)
        self.assertEqual(display()["rate"], "MANUAL")
        for voice in ("SINE 2", "TEST TONE", "SQUARE"):
            self.transport.send("ENCODER_ROTATE NEXT")
            self.assertEqual(display()["voice"], voice)
            self.assertEqual(display()["depth"], 1.0)
        for _ in range(3):
            self.transport.send("ENCODER_ROTATE PREV")
        self.assertEqual(display()["voice"], "SINE 1")
        # A position follows the control's stored mode: continuous once the pot is.
        self.transport.send("PANEL POT 1 MODE CONTINUOUS")
        self.transport.send("SET PITCH_POSITION 1")
        self.assertEqual(display()["pitchMode"], "CONTINUOUS")
        self.assertAlmostEqual(display()["tuneHz"], 1600, places=2)
        self.transport.send("PANEL POT 1 MODE ZONES")
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
