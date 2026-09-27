import unittest

from control_ranges import CONTROL_SPECS
from physical_ui import (
    LFO_SHAPES,
    PANEL_KNOBS,
    TRANSIENT_MODES,
    VOICE_VALUES,
    PhysicalDubSirenApp,
    echo_filter_frequencies,
    format_echo_filter,
)


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

    def configure(self, **kwargs):
        self.config.update(kwargs)

    def instate(self, _statespec):
        return self.config.get("state") == "disabled"

    def set_pressed(self, pressed):
        self.config["pressed"] = pressed


class FakeRoot:
    def __init__(self):
        self.callbacks = {}
        self.cancelled = []
        self.next_token = 0

    def after(self, _delay, callback):
        self.next_token += 1
        token = f"after-{self.next_token}"
        self.callbacks[token] = callback
        return token

    def after_cancel(self, token):
        self.cancelled.append(token)
        self.callbacks.pop(token, None)


class FakeTransport:
    connected = True


def make_app() -> PhysicalDubSirenApp:
    app = object.__new__(PhysicalDubSirenApp)
    app.mode_var = FakeVariable("SINE1")
    app.lfo_shape_var = FakeVariable("CLASSIC")
    app.syndrum_wave_var = FakeVariable("SINE")
    app.modulation_var = FakeVariable(True)
    app.retrigger_var = FakeVariable(False)
    app.echo_loop_var = FakeVariable(False)
    app.hold_var = FakeVariable(False)
    app.output_var = FakeVariable("PC")
    app.parameter_values = {
        name: spec.default for name, spec in CONTROL_SPECS.items()
    }
    app.echo_filter_macro = 0.0
    app.mod_down_button = FakeWidget()
    app.mod_up_button = FakeWidget()
    app.trigger_button = FakeWidget()
    app.echo_cut_button = FakeWidget()
    app.transport = FakeTransport()
    app.active_output = None
    app.momentary_active = set()
    app.pending_sends = {}
    app.root = FakeRoot()
    app.commands = []
    app._send = app.commands.append
    app._refresh_display = lambda: None
    return app


class PhysicalUiTests(unittest.TestCase):
    def test_panel_mapping_is_six_performance_knobs(self):
        self.assertEqual(
            PANEL_KNOBS,
            (
                ("TUNE", "TUNE_HZ"),
                ("RATE", "LFO_RATE_HZ"),
                ("ECHO", "ECHO_LEVEL"),
                ("DELAY", "DELAY_MS"),
                ("FEEDBACK", "FEEDBACK"),
                ("FILTER", "ECHO_FILTER"),
            ),
        )

    def test_encoder_menu_hides_secondary_dsp_controls(self):
        app = make_app()
        keys = [key for key, _label, _value in app._menu_items()]
        self.assertEqual(
            keys,
            [
                "VOICE",
                "LFO_SHAPE",
                "LFO_DEPTH_OCT",
                "DECAY_MS",
                "SWEEP",
                "SWEEP_MS",
                "MODULATION",
                "RETRIGGER",
                "ECHO_LOOP",
                "HOLD",
                "BACK",
            ],
        )

        app.mode_var.set("SYNDRUM")
        keys = [key for key, _label, _value in app._menu_items()]
        self.assertEqual(
            keys,
            [
                "VOICE",
                "SYNDRUM_WAVE",
                "SYNDRUM_SWEEP",
                "DECAY_MS",
                "ECHO_LOOP",
                "BACK",
            ],
        )

        app.mode_var.set("GUNSHOT")
        keys = [key for key, _label, _value in app._menu_items()]
        self.assertEqual(keys, ["VOICE", "DECAY_MS", "ECHO_LOOP", "BACK"])

    def test_transient_voice_clears_hold_and_disables_mod_buttons(self):
        app = make_app()
        app.hold_var.set(True)

        app._set_mode("GUNSHOT")

        self.assertIn("GUNSHOT", TRANSIENT_MODES)
        self.assertFalse(app.hold_var.get())
        self.assertEqual(
            app.commands[:2],
            ["HOLD 0", "SET MODE GUNSHOT"],
        )
        self.assertEqual(
            app.mod_down_button.config["state"],
            "disabled",
        )
        self.assertEqual(
            app.mod_up_button.config["state"],
            "disabled",
        )

    def test_manual_lfo_changes_performance_button_labels(self):
        app = make_app()
        app.lfo_shape_var.set("MANUAL")

        app._update_button_labels()

        self.assertEqual(
            app.mod_down_button.config["text"],
            "MOD DOWN",
        )
        self.assertEqual(
            app.mod_up_button.config["text"],
            "MOD UP",
        )

        app.lfo_shape_var.set(LFO_SHAPES[0])
        app._update_button_labels()
        self.assertEqual(
            app.mod_down_button.config["text"],
            "DIVE",
        )
        self.assertEqual(
            app.mod_up_button.config["text"],
            "RISE",
        )

    def test_echo_filter_macro_reuses_existing_hpf_lpf(self):
        app = make_app()

        app.echo_filter_macro = 0.0
        app._apply_echo_filter()
        self.assertEqual(
            app.parameter_values["HPF_HZ"],
            60.0,
        )
        self.assertEqual(
            app.parameter_values["LPF_HZ"],
            16000.0,
        )

        app.echo_filter_macro = -1.0
        app._apply_echo_filter()
        self.assertAlmostEqual(
            app.parameter_values["HPF_HZ"],
            60.0,
        )
        self.assertAlmostEqual(
            app.parameter_values["LPF_HZ"],
            400.0,
        )

        app.echo_filter_macro = 1.0
        app._apply_echo_filter()
        self.assertAlmostEqual(
            app.parameter_values["HPF_HZ"],
            4000.0,
        )
        self.assertAlmostEqual(
            app.parameter_values["LPF_HZ"],
            16000.0,
        )

    def test_echo_filter_has_a_flat_deadband_and_smooth_safe_sides(self):
        self.assertEqual(format_echo_filter(-0.05), "FLAT")
        self.assertEqual(format_echo_filter(0.05), "FLAT")
        self.assertEqual(echo_filter_frequencies(-0.05), (60.0, 16000.0))
        self.assertEqual(echo_filter_frequencies(0.05), (60.0, 16000.0))

        low_hpf, low_lpf = echo_filter_frequencies(-0.5)
        high_hpf, high_lpf = echo_filter_frequencies(0.5)
        self.assertEqual(low_hpf, 60.0)
        self.assertGreater(low_lpf, 400.0)
        self.assertLess(low_lpf, 16000.0)
        self.assertGreater(high_hpf, 60.0)
        self.assertLess(high_hpf, 4000.0)
        self.assertEqual(high_lpf, 16000.0)

        low_end = echo_filter_frequencies(-1.0)
        high_end = echo_filter_frequencies(1.0)
        self.assertAlmostEqual(low_end[0], 60.0)
        self.assertAlmostEqual(low_end[1], 400.0)
        self.assertAlmostEqual(high_end[0], 4000.0)
        self.assertAlmostEqual(high_end[1], 16000.0)
        self.assertEqual(format_echo_filter(-1.0), "LP 100%")
        self.assertEqual(format_echo_filter(1.0), "HP 100%")

    def test_filter_throttle_is_cancelled_and_final_value_is_always_sent(self):
        app = make_app()
        app._show_overlay = lambda *_args: None

        app._panel_parameter_changed("ECHO_FILTER", -0.4, False)
        first_token = app.pending_sends["ECHO_FILTER"]
        app._panel_parameter_changed("ECHO_FILTER", 0.6, False)
        second_token = app.pending_sends["ECHO_FILTER"]

        self.assertIn(first_token, app.root.cancelled)
        self.assertNotEqual(first_token, second_token)
        self.assertEqual(app.commands, [])

        app._panel_parameter_changed("ECHO_FILTER", 1.0, True)
        self.assertIn(second_token, app.root.cancelled)
        self.assertNotIn("ECHO_FILTER", app.pending_sends)
        self.assertEqual(
            app.commands[-2:],
            ["SET HPF_HZ 4000", "SET LPF_HZ 16000"],
        )

    def test_mode_change_releases_active_momentary_controls(self):
        app = make_app()
        app.momentary_active.update(("TRIGGER", "MOD_DOWN"))

        app._set_mode("GUNSHOT")

        self.assertEqual(app.momentary_active, set())
        self.assertIn("TRIGGER 0", app.commands)
        self.assertIn("MOD_DOWN 0", app.commands)
        self.assertEqual(app.commands[-1], "SET MODE GUNSHOT")

    def test_synchronization_keeps_extended_as_internal_engine_profile(self):
        app = make_app()
        app.mode_var.set(VOICE_VALUES[4])
        app.lfo_shape_var.set("SAW_DOWN")
        app.echo_loop_var.set(True)

        app._synchronize_controls()

        self.assertIn("SET PROFILE EXTENDED", app.commands)
        self.assertIn("SET MODE LICKSHOT", app.commands)
        self.assertIn("SET LFO_SHAPE SAW_DOWN", app.commands)
        self.assertIn("ECHO_LOOP 1", app.commands)
        self.assertIn("ECHO_SPIN 0", app.commands)
        self.assertIn("TRIGGER 0", app.commands)
        self.assertIn("MOD_UP 0", app.commands)
        self.assertIn("MOD_DOWN 0", app.commands)


if __name__ == "__main__":
    unittest.main()
