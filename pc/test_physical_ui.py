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
    app.dials = {"LFO_RATE_HZ": FakeDial(), "TUNE_HZ": FakeDial()}
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
                ("TUNE", "TUNE_HZ"),
                ("RATE", "LFO_RATE_HZ"),
                ("ECHO LEVEL", "ECHO_LEVEL"),
                ("ECHO TIME", "DELAY_MS"),
                ("ECHO FEEDBACK", "FEEDBACK"),
                ("ECHO TONE", "ECHO_FILTER"),
            ),
        )

    def test_menu_configures_only_selected_pad_and_secondary_echo_loop(self):
        app = make_app()
        keys = [key for key, _label, _value in app._menu_items()]
        self.assertEqual(
            keys,
            ["PAD", "ASSIGNMENT", "VARIANT", "HOLD", "ECHO_LOOP", "BACK"],
        )
        app.panel_state["selectedPad"] = 4
        app.panel_state["display"]["menuItems"] = [
            "PAD", "ASSIGNMENT", "VARIANT", "ECHO_LOOP", "BACK"
        ]
        keys = [key for key, _label, _value in app._menu_items()]
        self.assertEqual(
            keys, ["PAD", "ASSIGNMENT", "VARIANT", "ECHO_LOOP", "BACK"]
        )

    def test_home_encoder_changes_depth_not_voice(self):
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
        display._screen = "home"
        display._menu = ([], 0, False)

        display.render_home(
            2, 1, "TWO TONE", "TRIANGLE", "1.00 oct",
            True, True, True,
        )
        display.render_overlay("RATE · PING N/A", "20.00 Hz")
        display.render_menu(
            [("CONFIGURE PAD", "2"), ("ASSIGNMENT", "TWO TONE"),
             ("VARIANT", "TRIANGLE"), ("HOLD", "ON"),
             ("ECHO LOOP", "ON"), ("BACK", "")],
            4,
            True,
        )

        text = " ".join(
            call[2].get("text", "") for call in display.canvas.calls
            if call[0] == "text"
        )
        for expected in (
            "TUNE", "RATE", "DEPTH", "HOLD", "LOOP", "RACK LABEL / EDIT"
        ):
            self.assertIn(expected, text)
        for kind, args, _kwargs in display.canvas.calls:
            if kind == "text":
                self.assertGreaterEqual(args[0], 0)
                self.assertLessEqual(args[0], DISPLAY_WIDTH)
                self.assertGreaterEqual(args[1], 0)
                self.assertLess(args[1], DISPLAY_HEIGHT)
            elif kind == "rectangle":
                self.assertGreaterEqual(min(args[0], args[2]), 0)
                self.assertLessEqual(max(args[0], args[2]), DISPLAY_WIDTH)
                self.assertGreaterEqual(min(args[1], args[3]), 0)
                self.assertLessEqual(max(args[1], args[3]), DISPLAY_HEIGHT)

    def test_rate_is_visibly_inactive_for_ping(self):
        app = make_app()
        app.panel_state["currentPad"] = 4

        app._update_control_activity()

        self.assertFalse(app.dials["LFO_RATE_HZ"].active)
        self.assertEqual(
            app.dials["LFO_RATE_HZ"].value_label.config["text"],
            "INACTIVE FOR PING",
        )

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
        self.assertEqual(app.dials["TUNE_HZ"].finishes, 1)

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
        self.assertEqual(len(app.commands), 2)
        expected_hpf, expected_lpf = echo_filter_frequencies(0.6)
        self.assertEqual(
            app.commands,
            [f"SET HPF_HZ {expected_hpf:.6g}", f"SET LPF_HZ {expected_lpf:.6g}"],
        )
        app._panel_parameter_changed("ECHO_FILTER", 1.0, False)
        final_token = app.pending_sends["ECHO_FILTER"]
        app._panel_parameter_changed("ECHO_FILTER", 1.0, True)
        self.assertIn(final_token, app.root.cancelled)
        self.assertEqual(app.commands[-2:], ["SET HPF_HZ 4000", "SET LPF_HZ 16000"])

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
        self.assertIn("SET HPF_HZ 60", app.commands)
        self.assertIn("SET LPF_HZ 16000", app.commands)

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
