import json
import tempfile
import unittest
from pathlib import Path

from control_ranges import CONTROL_SPECS
from physical_ui import (
    DEFAULT_PANEL_STATE,
    DISPLAY_HEIGHT,
    DISPLAY_WIDTH,
    PANEL_KNOBS,
    PanelDisplay,
    PanelConfigError,
    PhysicalDubSirenApp,
    load_panel_config,
    panel_settings_commands,
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

    def set_value(self, value, notify=False):
        self.value = value


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
    app.dials = {f"POT_{number}": FakeDial() for number in range(1, 7)}
    app.dials["MASTER"] = FakeDial()
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
    app._connected_virtual = False
    app._saved_panel_config = None
    app._panel_config_error = None
    app._virtual_restore_pending = False
    app._last_saved_settings_revision = None
    return app


def sample_settings() -> dict:
    targets = ["PITCH", "RATE", "ECHO LEVEL", "ECHO TIME", "ECHO FEEDBACK", "ECHO FILTER"]
    ranges = [(80.0, 1600.0), (0.15, 20.0), (0.0, 1.0),
              (50.0, 1000.0), (0.0, 1.05), (-1.0, 1.0)]
    pots = [
        {"target": target, "mode": "CONTINUOUS", "min": limits[0], "max": limits[1],
         "zones": [0.2, 0.5, 0.8]}
        for target, limits in zip(targets, ranges)
    ]
    pots[0].update(mode="ZONES", min=80.0, max=1600.0,
                   zones=[261.63, 392.0, 587.33])
    pots[1].update(mode="ZONES", min=0.05, max=20.0,
                   zones=[0.35, 0.7, 2.8])
    buttons = [
        {"action": action, "sound": "CLASSIC", "variant": 0, "latch": False}
        for action in ("TRIGGER", "HOLD", "MOD UP", "ECHO CUT")
    ]
    extended_pots = json.loads(json.dumps(pots))
    for pot in extended_pots:
        pot["mode"] = "CONTINUOUS"
    extended_buttons = [
        {"action": "PLAY SOUND", "sound": sound, "variant": 0, "latch": False}
        for sound in ("CLASSIC", "TWO TONE", "LICKSHOT", "PING")
    ]
    return {
        "schema": 1, "potBehavior": "PICKUP",
        "encoder": {"holdTurn": "LIVE_PITCH", "step": 1, "range": 12},
        "profiles": {
            "CLASSIC": {"pots": pots, "buttons": buttons},
            "EXTENDED": {"pots": extended_pots, "buttons": extended_buttons},
        },
    }


class PhysicalUiTests(unittest.TestCase):
    def test_panel_knobs_send_raw_numbered_controls(self):
        self.assertEqual(
            PANEL_KNOBS,
            tuple((f"POT {number}", f"POT_{number}") for number in range(1, 7)),
        )
        app = make_app()
        app._show_overlay = lambda *_args: None
        app._panel_parameter_changed("POT_3", 0.75, True)
        self.assertEqual(app.commands[-1], "POT 3 0.75")
        self.assertFalse(any("ECHO_LEVEL" in command for command in app.commands))

    def test_menu_configures_only_selected_pad_and_secondary_echo_loop(self):
        app = make_app()
        keys = [key for key, _label, _value in app._menu_items()]
        self.assertEqual(
            keys,
            ["PROFILE", "POTS", "BUTTONS", "ENCODER", "RESET_DEFAULTS", "BACK"],
        )
        app.panel_state["selectedPad"] = 4
        app.panel_state["display"]["rows"] = [
            {"key": "ACTION", "label": "ACTION", "value": "PLAY SOUND"},
            {"key": "SOUND", "label": "SOUND", "value": "PING"},
            {"key": "VARIANT", "label": "VARIANT", "value": "SINE"},
            {"key": "BACK", "label": "BACK", "value": ""},
        ]
        keys = [key for key, _label, _value in app._menu_items()]
        self.assertEqual(
            keys, ["ACTION", "SOUND", "VARIANT", "BACK"]
        )

    def test_encoder_sends_raw_detent_to_shared_controller(self):
        app = make_app()

        app._encoder_rotate(1)

        self.assertEqual(app.commands[-1], "ENCODER_ROTATE NEXT")
        self.assertFalse(any(command.startswith("SET MODE") for command in app.commands))

    def test_text_display_preserves_home_parameters_overlay_and_menu(self):
        self.assertEqual((DISPLAY_WIDTH, DISPLAY_HEIGHT), (320, 172))
        display = object.__new__(PanelDisplay)
        display.canvas = FakeCanvas()

        def text():
            return [kwargs["text"] for kind, _args, kwargs in display.canvas.calls
                    if kind == "text"]

        display.render_home(
            1, 1, "SINE 2", "DS01E / CLASSIC", "", True, False, True,
            {"PROFILE": "CLASSIC", "PITCH": "HIGH", "RATE": "MANUAL", "ECHO_CUT": True},
        )
        self.assertIn("SINE 2", text())
        self.assertIn("PITCH  HIGH \u00b7 392 Hz", text())
        self.assertIn("RATE   MANUAL", text())
        self.assertIn("HOLD / ECHO CUT", text())

        display.canvas.calls.clear()
        display.render_home(
            2, 1, "TWO TONE", "TRIANGLE", "1.00 oct", True, True, True,
            {"TUNE_HZ": 440.0, "LFO_RATE_HZ": 7.5, "DELAY_MS": 1000.0,
             "FEEDBACK": 0.8, "ECHO_LEVEL": 0.6, "MASTER": 0.5},
            "LP 80%",
        )
        self.assertIn("P2 TWO TONE", text())
        self.assertIn("TRIANGLE", text())
        self.assertIn("TUNE 440 Hz  RATE 7.5 Hz", text())
        self.assertIn("ECHO 1000ms  FB 80%  LV 60%", text())
        self.assertIn("FILTER LP 80%", text())
        self.assertIn("PLAY P1 / HOLD / LOOP / LINK", text())
        display.render_overlay("ECHO FEEDBACK", "80%")
        self.assertIn("ECHO FEEDBACK", text())
        self.assertIn("80%", text())

        display.canvas.calls.clear()
        display.render_overlay("PITCH", "UP PICKUP - 392 Hz")
        self.assertIn("↑ PICKUP - 392 Hz", text())

        display.canvas.calls.clear()
        display.render_overlay("LIVE PITCH", "+3 st - 466 Hz")
        self.assertIn("LIVE PITCH", text())
        self.assertIn("+3 st - 466 Hz", text())

        display.canvas.calls.clear()
        display.render_menu(
            [("CONFIGURE PAD", "2"), ("ASSIGNMENT", "TWO TONE"),
             ("VARIANT", "TRIANGLE"), ("HOLD", "ON"),
             ("ECHO LOOP", "ON"), ("BACK", "")], 4, True,
        )
        self.assertIn("> ECHO LOOP", text())
        self.assertIn("SETTINGS", text())
        self.assertIn("TURN CHANGE / PUSH SAVE", text())
        self.assertNotIn("  CONFIGURE PAD", text())
        for kind, args, _kwargs in display.canvas.calls:
            if kind == "text":
                self.assertTrue(0 <= args[0] < DISPLAY_WIDTH)
                self.assertTrue(0 <= args[1] < DISPLAY_HEIGHT)

        display.canvas.calls.clear()
        display.render_menu(
            [("PROFILE", "CLASSIC"), ("POTS", ""), ("BUTTONS", "")],
            1, False, "SETTINGS", "PITCH UP PICKUP - 392 Hz",
        )
        self.assertIn("SETTINGS", text())
        self.assertIn("PITCH UP PICKUP - 392 Hz", text())

    def test_confirmed_selector_labels_and_baseline_buttons(self):
        app = make_app()
        app.panel_state["settings"] = sample_settings()
        app.panel_state["display"]["buttons"] = ["TRIGGER", "HOLD", "MOD UP", "ECHO CUT"]
        app._update_pad_labels()
        self.assertEqual(app.dials["POT_1"].value_label.config["text"], "PITCH")
        self.assertEqual(app.dials["POT_2"].value_label.config["text"], "RATE")
        for button, label in zip(app.pad_buttons, ("TRIGGER", "HOLD", "MOD UP", "ECHO CUT")):
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
        self.assertEqual(app.dials["POT_1"].finishes, 1)

    def test_full_release_uses_panel_reset_without_erasing_configuration(self):
        app = make_app()
        app._pad_down(3, "key-3")
        app._encoder_sources.add("key-e")

        app._global_release()

        self.assertEqual(app.commands[-1], "PANEL_RESET")
        self.assertEqual(app._input_sources, {})
        self.assertEqual(app._encoder_sources, set())

    def test_knob_throttle_keeps_one_timer_and_sends_latest_periodically(self):
        app = make_app()
        app._show_overlay = lambda *_args: None

        app._panel_parameter_changed("POT_6", 0.4, False)
        token = app.pending_sends["POT_6"]
        app._panel_parameter_changed("POT_6", 0.6, False)
        self.assertEqual(app.pending_sends["POT_6"], token)
        self.assertEqual(app.root.cancelled, [])

        app.root.callbacks[token]()
        self.assertEqual(len(app.commands), 1)
        self.assertEqual(app.commands, ["POT 6 0.6"])
        app._panel_parameter_changed("POT_6", 1.0, False)
        final_token = app.pending_sends["POT_6"]
        app._panel_parameter_changed("POT_6", 1.0, True)
        self.assertIn(final_token, app.root.cancelled)
        self.assertEqual(app.commands[-1], "POT 6 1")

    def test_real_device_sync_does_not_push_profile_or_defaults(self):
        app = make_app()
        app._synchronize_controls()

        self.assertFalse(any(command.startswith("SET PROFILE") for command in app.commands))
        self.assertFalse(any(command.startswith("SET ") and "OUTPUT" not in command
                             for command in app.commands))
        self.assertIn("PANEL_RESET", app.commands)

    def test_virtual_sync_applies_only_validated_panel_settings(self):
        app = make_app()
        app._connected_virtual = True
        app._saved_config_applied = False
        app._saved_panel_config = sample_settings()
        app.transport.send = app.commands.append

        app._synchronize_controls()

        self.assertTrue(app._virtual_restore_pending)
        self.assertIn("PANEL BEHAVIOR PICKUP", app.commands)
        self.assertIn("PANEL POT CLASSIC 1 TARGET PITCH", app.commands)
        self.assertIn("PANEL BUTTON EXTENDED 4 SOUND PING 0 0", app.commands)
        self.assertFalse(any(command.startswith("SET PROFILE") for command in app.commands))

    def test_panel_configuration_round_trip(self):
        settings = sample_settings()
        settings["profiles"]["EXTENDED"]["buttons"][1]["variant"] = 2
        settings["profiles"]["EXTENDED"]["buttons"][2]["latch"] = True
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "panel.json"
            save_panel_config(path, settings)
            loaded = load_panel_config(path)
        self.assertEqual(loaded["schema"], 1)
        self.assertEqual(loaded["profiles"]["EXTENDED"]["buttons"][1]["variant"], 2)
        commands = panel_settings_commands(loaded)
        self.assertIn("PANEL POT CLASSIC 1 TARGET PITCH", commands)
        self.assertIn("PANEL BUTTON EXTENDED 2 SOUND TWO_TONE 2 0", commands)

    def test_invalid_panel_configuration_is_surfaced(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "panel.json"
            path.write_text('{"schema": 99}', encoding="utf-8")
            with self.assertRaises(PanelConfigError):
                load_panel_config(path)

    def test_panel_configuration_save_failure_is_surfaced(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(OSError):
                save_panel_config(Path(directory), sample_settings())


if __name__ == "__main__":
    unittest.main()
