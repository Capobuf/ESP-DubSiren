import unittest

from control_ranges import CONTROL_SPECS
from physical_ui import (
    LFO_SHAPES,
    PANEL_KNOBS,
    TRANSIENT_MODES,
    VOICE_VALUES,
    PhysicalDubSirenApp,
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
    app.transport = FakeTransport()
    app.active_output = None
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
        self.assertIn("SYNDRUM_WAVE", keys)
        self.assertIn("SYNDRUM_SWEEP", keys)

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
