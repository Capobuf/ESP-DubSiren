import unittest

from app import CLASSIC_MODES, EXTENDED_MODES, DubSirenApp


class FakeVariable:
    def __init__(self, value):
        self.value = value

    def get(self):
        return self.value

    def set(self, value):
        self.value = value


class FakeWidget:
    def __init__(self):
        self.config = {}
        self.master = object()
        self.pack_calls = []
        self.forgotten = False

    def configure(self, **kwargs):
        self.config.update(kwargs)

    def pack(self, **kwargs):
        self.pack_calls.append(kwargs)

    def pack_forget(self):
        self.forgotten = True


class FakeSlider:
    def __init__(self, value):
        self.value = value


class FakeRoot:
    def __init__(self):
        self.callbacks = {}
        self.cancelled = []
        self.counter = 0

    def after(self, _delay, callback):
        self.counter += 1
        token = f"timer-{self.counter}"
        self.callbacks[token] = callback
        return token

    def after_cancel(self, token):
        self.cancelled.append(token)
        self.callbacks.pop(token, None)


def make_app():
    app = object.__new__(DubSirenApp)
    app.profile_var = FakeVariable("EXTENDED")
    app.mode_var = FakeVariable("LICKSHOT")
    app.classic_pitch_var = FakeVariable("MID")
    app.classic_mod_var = FakeVariable("MEDIUM")
    app.lfo_shape_var = FakeVariable("TRIANGLE")
    app.syndrum_wave_var = FakeVariable("SINE")
    app.modulation_var = FakeVariable(True)
    app.retrigger_var = FakeVariable(False)
    app.echo_loop_var = FakeVariable(False)
    app.hold_var = FakeVariable(False)
    app.output_var = FakeVariable("PC")
    app.mode_combo = FakeWidget()
    app.extended_controls = FakeWidget()
    app.classic_controls = FakeWidget()
    app.mod_down_button = FakeWidget()
    app.mod_up_button = FakeWidget()
    app.hold_check = FakeWidget()
    app.syndrum_controls = FakeWidget()
    app.sliders = {
        "TUNE_HZ": FakeSlider(220.0),
        "SWEEP": FakeSlider(0.0),
        "SWEEP_MS": FakeSlider(650.0),
        "SYNDRUM_SWEEP": FakeSlider(2.0),
    }
    app.momentary_active = set()
    app.pending_sends = {}
    app.root = FakeRoot()
    app.commands = []
    app._send = app.commands.append
    return app


class PerformanceControlTests(unittest.TestCase):
    def test_extended_and_classic_mode_lists(self):
        self.assertEqual(
            EXTENDED_MODES,
            ("SINE1", "SINE2", "TEST_TONE", "SQUARE", "SAW", "TRIANGLE",
             "LICKSHOT", "SONAR", "SYNDRUM", "GUNSHOT"),
        )
        app = make_app()
        app._update_profile_ui()
        self.assertEqual(app.mode_combo.config["values"], EXTENDED_MODES)

        app.profile_var.set("CLASSIC")
        app._update_profile_ui()
        self.assertEqual(app.mode_combo.config["values"], CLASSIC_MODES)
        self.assertEqual(app.mode_var.get(), "SINE1")
        self.assertIn("SET MODE SINE1", app.commands)

        app.commands.clear()
        for mode in ("SAW", "TRIANGLE", "LICKSHOT", "SONAR", "SYNDRUM", "GUNSHOT"):
            app.mode_var.set(mode)
            app._mode_changed()
        self.assertEqual(
            app.commands,
            ["SET MODE SAW", "SET MODE TRIANGLE", "SET MODE LICKSHOT",
             "SET MODE SONAR", "SET MODE SYNDRUM", "SET MODE GUNSHOT"],
        )

    def test_transient_mode_controls(self):
        app = make_app()
        app.hold_var.set(True)
        app.mode_var.set("SYNDRUM")
        app._mode_changed()
        self.assertFalse(app.hold_var.get())
        self.assertEqual(app.commands[:2], ["HOLD 0", "SET MODE SYNDRUM"])
        self.assertEqual(app.hold_check.config["state"], "disabled")
        self.assertEqual(app.mod_down_button.config["state"], "disabled")
        self.assertTrue(app.syndrum_controls.pack_calls)

        app.mode_var.set("SINE1")
        app._mode_changed()
        self.assertEqual(app.hold_check.config["state"], "normal")
        self.assertEqual(app.mod_up_button.config["state"], "normal")

        app.mode_var.set("GUNSHOT")
        app._update_mode_ui()
        self.assertTrue(app.syndrum_controls.forgotten)

    def test_buttons_switch_between_rise_dive_and_manual_labels(self):
        app = make_app()
        app._update_manual_buttons()
        self.assertEqual(app.mod_down_button.config,
                         {"state": "normal", "text": "DIVE"})
        self.assertEqual(app.mod_up_button.config,
                         {"state": "normal", "text": "RISE"})

        app.lfo_shape_var.set("MANUAL")
        app._update_manual_buttons()
        self.assertEqual(app.mod_down_button.config["text"], "MOD DOWN")
        self.assertEqual(app.mod_up_button.config["text"], "MOD UP")

    def test_synchronization_and_echo_spin_release(self):
        app = make_app()
        app._synchronize_controls()
        self.assertIn("MODULATION 1", app.commands)
        self.assertIn("RETRIGGER 0", app.commands)
        self.assertIn("ECHO_LOOP 0", app.commands)
        self.assertIn("ECHO_SPIN 0", app.commands)
        self.assertIn("SET SYNDRUM_WAVE SINE", app.commands)
        self.assertIn("SET SWEEP_MS 650", app.commands)

        app.commands.clear()
        app._press("ECHO_SPIN")
        app._global_release()
        self.assertEqual(app.commands, ["ECHO_SPIN 1", "ECHO_SPIN 0"])

    def test_slider_throttle_does_not_postpone_continuous_updates(self):
        app = make_app()
        slider = app.sliders["TUNE_HZ"]

        app._slider_changed("TUNE_HZ", 230.0, False)
        token = app.pending_sends["TUNE_HZ"]
        slider.value = 250.0
        app._slider_changed("TUNE_HZ", 250.0, False)
        self.assertEqual(app.pending_sends["TUNE_HZ"], token)
        self.assertEqual(app.root.cancelled, [])

        app.root.callbacks[token]()
        self.assertEqual(app.commands[-1], "SET TUNE_HZ 250")
        app._slider_changed("TUNE_HZ", 260.0, False)
        final_token = app.pending_sends["TUNE_HZ"]
        app._slider_changed("TUNE_HZ", 260.0, True)
        self.assertIn(final_token, app.root.cancelled)
        self.assertEqual(app.commands[-1], "SET TUNE_HZ 260")


if __name__ == "__main__":
    unittest.main()
