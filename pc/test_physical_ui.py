import tempfile
import time
import unittest
from pathlib import Path

from control_ranges import CONTROL_SPECS
from physical_ui import (
    DEFAULT_PANEL_STATE,
    DISPLAY_HEIGHT,
    DISPLAY_WIDTH,
    PANEL_KNOBS,
    PanelDisplay,
    PhysicalDubSirenApp,
    echo_filter_frequencies,
    format_echo_filter,
    load_panel_config,
    save_panel_config,
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

    def set_pressed(self, pressed):
        self.config["pressed"] = pressed


class FakeDial:
    def __init__(self):
        self.finishes = 0
        self.active = True
        self.value_label = FakeWidget()

    def finish_drag(self):
        self.finishes += 1

    def set_active(self, active):
        self.active = active


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


class FakeCanvas:
    def __init__(self):
        self.calls = []

    def delete(self, *args):
        self.calls.append(("delete", args, {}))

    def create_text(self, *args, **kwargs):
        self.calls.append(("text", args, kwargs))

    def create_rectangle(self, *args, **kwargs):
        self.calls.append(("rectangle", args, kwargs))

    def create_line(self, *args, **kwargs):
        self.calls.append(("line", args, kwargs))

    def create_oval(self, *args, **kwargs):
        self.calls.append(("oval", args, kwargs))

    def create_polygon(self, *args, **kwargs):
        self.calls.append(("polygon", args, kwargs))


def make_app() -> PhysicalDubSirenApp:
    app = object.__new__(PhysicalDubSirenApp)
    app.output_var = FakeVariable("PC")
    app.echo_loop_var = FakeVariable(False)
    app.parameter_values = {
        name: spec.default for name, spec in CONTROL_SPECS.items()
    }
    app.parameter_values["HPF_HZ"] = 60.0
    app.parameter_values["LPF_HZ"] = 7000.0
    app.echo_filter_macro = 0.0
    app.echo_filter_active = False
    app.panel_state = {
        **DEFAULT_PANEL_STATE,
        "display": {
            **DEFAULT_PANEL_STATE["display"],
            "menuItems": list(DEFAULT_PANEL_STATE["display"]["menuItems"]),
        },
        "pads": [
            {"sound": "CLASSIC", "variant": "SINE 1", "variantIndex": 0,
             "hold": False, "holdSupported": True},
            {"sound": "TWO TONE", "variant": "RC", "variantIndex": 0,
             "hold": False, "holdSupported": True},
            {"sound": "LICKSHOT", "variant": "SAW DOWN", "variantIndex": 0,
             "hold": False, "holdSupported": True},
            {"sound": "PING", "variant": "SINE", "variantIndex": 0,
             "hold": False, "holdSupported": False},
        ],
    }
    app.pad_buttons = [FakeWidget() for _ in range(4)]
    app.dials = {"RATE_POSITION": FakeDial(), "PITCH_POSITION": FakeDial()}
    app.transport = FakeTransport()
    app.active_output = None
    app.pending_sends = {}
    app.root = FakeRoot()
    app.commands = []
    app._send = app.commands.append
    app._refresh_display = lambda: None
    app._input_sources = {}
    app._encoder_sources = set()
    app._saved_config_applied = True
    app._saved_pad_configs = []
    return app


class PhysicalUiTests(unittest.TestCase):
    def test_panel_mapping_names_echo_controls_explicitly(self):
        self.assertEqual(
            PANEL_KNOBS,
            (
                ("PITCH", "PITCH_POSITION"),
                ("RATE", "RATE_POSITION"),
                ("ECHO LEVEL", "ECHO_LEVEL"),
                ("ECHO TIME", "DELAY_MS"),
                ("ECHO FEEDBACK", "FEEDBACK"),
                ("ECHO FILTER", "ECHO_FILTER"),
            ),
        )

    def test_menu_configures_only_selected_pad_and_secondary_echo_loop(self):
        app = make_app()
        keys = [key for key, _label, _value in app._menu_items()]
        self.assertEqual(
            keys,
            ["PROFILE", "BACK"],
        )
        app.panel_state["selectedPad"] = 4
        app.panel_state["display"]["menuItems"] = [
            "PAD", "ASSIGNMENT", "VARIANT", "ECHO_LOOP", "BACK"
        ]
        keys = [key for key, _label, _value in app._menu_items()]
        self.assertEqual(
            keys, ["PAD", "ASSIGNMENT", "VARIANT", "ECHO_LOOP", "BACK"]
        )

    def test_encoder_sends_raw_detent_to_shared_controller(self):
        app = make_app()

        app._encoder_rotate(1)

        self.assertEqual(app.commands[-1], "ENCODER_ROTATE NEXT")
        self.assertFalse(any(command.startswith("SET MODE") for command in app.commands))

    def test_display_layout_is_native_320_by_170_and_stays_in_bounds(self):
        self.assertEqual((DISPLAY_WIDTH, DISPLAY_HEIGHT), (320, 170))
        display = object.__new__(PanelDisplay)
        display.canvas = FakeCanvas()
        display._boot_started = time.monotonic() - 10.0
        display._peak_provider = None
        display._focus = None
        display._focus_value = ""
        display._focus_until = 0.0
        display._transition_until = 0.0
        display._screen = "home"
        display._menu = ([], 0, False)

        display.render_home(
            2, 1, "TWO TONE", "TRIANGLE", "1.00 oct",
            True, True, True,
        )
        home = display.render_image()
        display.render_overlay("RATE · PING N/A", "20.00 Hz")
        focused = display.render_image()
        display.render_menu(
            [("CONFIGURE PAD", "2"), ("ASSIGNMENT", "TWO TONE"),
             ("VARIANT", "TRIANGLE"), ("HOLD", "ON"),
             ("ECHO LOOP", "ON"), ("BACK", "")],
            4,
            True,
        )

        menu = display.render_image()

        for image in (home, focused, menu):
            self.assertEqual(image.size, (DISPLAY_WIDTH, DISPLAY_HEIGHT))
        self.assertNotEqual(home.tobytes(), focused.tobytes())
        self.assertNotEqual(focused.tobytes(), menu.tobytes())

    def test_confirmed_selector_labels_and_baseline_buttons(self):
        app = make_app()
        app.panel_state["display"]["pitch"] = "HIGH"
        app.panel_state["display"]["rate"] = "MANUAL"
        app._update_pad_labels()
        self.assertEqual(app.dials["RATE_POSITION"].value_label.config["text"], "MANUAL")
        self.assertEqual(app.dials["PITCH_POSITION"].value_label.config["text"], "HIGH")
        for button, label in zip(app.pad_buttons, ("TRIGGER", "HOLD", "MANUAL MOD", "ECHO CUT")):
            self.assertIn(label, button.config["text"])

    def test_pad_sources_overlap_and_keyboard_repeat_is_ignored(self):
        app = make_app()

        app._pad_down(1, "key-1")
        app._pad_down(1, "key-1")
        app._pad_down(1, "mouse")
        app._pad_up(1, "mouse")
        self.assertEqual(app.commands, ["PAD 1 1"])
        self.assertTrue(app.pad_buttons[0].config["pressed"])

        app._pad_up(1, "key-1")
        self.assertEqual(app.commands, ["PAD 1 1", "PAD 1 0"])
        self.assertFalse(app.pad_buttons[0].config["pressed"])

    def test_mouse_release_does_not_cancel_keyboard_pad(self):
        app = make_app()
        app._pad_down(2, "key-2")
        app._pad_down(2, "mouse")

        app._global_release(object())

        self.assertEqual(app.commands, ["PAD 2 1"])
        self.assertTrue(app.pad_buttons[1].config["pressed"])
        self.assertEqual(app.dials["PITCH_POSITION"].finishes, 1)

    def test_full_release_uses_panel_reset_without_erasing_configuration(self):
        app = make_app()
        app._pad_down(3, "key-3")
        app._encoder_sources.add("key-e")

        app._global_release()

        self.assertEqual(app.commands[-1], "PANEL_RESET")
        self.assertEqual(app._input_sources, {})
        self.assertEqual(app._encoder_sources, set())

    def test_echo_filter_macro_reuses_existing_hpf_lpf(self):
        app = make_app()
        for macro, expected in (
            (0.0, (60.0, 16000.0)),
            (-1.0, (60.0, 400.0)),
            (1.0, (4000.0, 16000.0)),
        ):
            app.echo_filter_macro = macro
            app._apply_echo_filter()
            self.assertAlmostEqual(app.parameter_values["HPF_HZ"], expected[0])
            self.assertAlmostEqual(app.parameter_values["LPF_HZ"], expected[1])

    def test_echo_filter_has_flat_deadband_and_logarithmic_sides(self):
        self.assertEqual(format_echo_filter(-0.05), "FLAT")
        self.assertEqual(format_echo_filter(0.05), "FLAT")
        self.assertEqual(echo_filter_frequencies(0.05), (60.0, 16000.0))
        low_hpf, low_lpf = echo_filter_frequencies(-0.5)
        high_hpf, high_lpf = echo_filter_frequencies(0.5)
        self.assertEqual(low_hpf, 60.0)
        self.assertGreater(low_lpf, 400.0)
        self.assertLess(low_lpf, 16000.0)
        self.assertGreater(high_hpf, 60.0)
        self.assertLess(high_hpf, 4000.0)
        self.assertEqual(high_lpf, 16000.0)

    def test_knob_throttle_keeps_one_timer_and_sends_latest_periodically(self):
        app = make_app()
        app._show_overlay = lambda *_args: None

        app._panel_parameter_changed("ECHO_FILTER", -0.4, False)
        token = app.pending_sends["ECHO_FILTER"]
        app._panel_parameter_changed("ECHO_FILTER", 0.6, False)
        self.assertEqual(app.pending_sends["ECHO_FILTER"], token)
        self.assertEqual(app.root.cancelled, [])

        app.root.callbacks[token]()
        self.assertEqual(len(app.commands), 1)
        expected_hpf, expected_lpf = echo_filter_frequencies(0.6)
        self.assertEqual(
            app.commands,
            ["SET ECHO_FILTER 0.6"],
        )
        app._panel_parameter_changed("ECHO_FILTER", 1.0, False)
        final_token = app.pending_sends["ECHO_FILTER"]
        app._panel_parameter_changed("ECHO_FILTER", 1.0, True)
        self.assertIn(final_token, app.root.cancelled)
        self.assertEqual(app.commands[-1], "SET ECHO_FILTER 1")

    def test_sync_preserves_unrepresented_filter_until_tone_is_moved(self):
        app = make_app()
        app._synchronize_controls()

        self.assertNotIn("SET HPF_HZ 60", app.commands)
        self.assertNotIn("SET LPF_HZ 7000", app.commands)
        self.assertFalse(any(command.startswith("SET MODE") for command in app.commands))
        self.assertIn("PANEL_RESET", app.commands)

        app.commands.clear()
        app.echo_filter_active = True
        app._synchronize_controls()
        self.assertIn("SET ECHO_FILTER 0", app.commands)
        self.assertIn("SET PROFILE CLASSIC", app.commands)

    def test_panel_configuration_round_trip(self):
        pads = [
            {"sound": "CLASSIC", "variantIndex": 0, "hold": False},
            {"sound": "TWO TONE", "variantIndex": 0, "hold": False},
            {"sound": "LICKSHOT", "variantIndex": 0, "hold": False},
            {"sound": "PING", "variantIndex": 0, "hold": False},
        ]
        pads[1]["variantIndex"] = 2
        pads[2]["hold"] = True
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "panel.json"
            save_panel_config(path, pads)
            loaded = load_panel_config(path)
        self.assertEqual(loaded[1]["variantIndex"], 2)
        self.assertNotIn("hold", loaded[2])
        self.assertNotIn("selectedPad", loaded[0])


if __name__ == "__main__":
    unittest.main()
