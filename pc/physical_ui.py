import json
import math
import os
import tkinter as tk
from pathlib import Path
from tkinter import ttk

from app import DubSirenApp, OUTPUT_VALUES
from control_ranges import CONTROL_SPECS, ControlSpec
from panel_display import DISPLAY_HEIGHT, DISPLAY_WIDTH, PanelDisplay

PANEL_KNOBS = tuple((f"POT {number}", f"POT_{number}") for number in range(1, 7))
FRONT_POT_SPEC = ControlSpec(0.0, 1.0, 0.5, lambda value: f"{value:.0%}")
PANEL_SCHEMA_VERSION = 1


class PanelConfigError(ValueError):
    pass

DEFAULT_PANEL_STATE = {
    "revision": 0,
    "menuRevision": 0,
    "selectedPad": 1,
    "currentPad": 1,
    "activePad": 0,
    "display": {
        "width": DISPLAY_WIDTH,
        "height": DISPLAY_HEIGHT,
        "mode": "HOME",
        "menuIndex": 0,
        "editing": False,
        "menuItems": [
            "PROFILE", "POTS", "BUTTONS", "ENCODER", "RESET_DEFAULTS", "BACK"
        ],
        "title": "SETTINGS",
        "footer": "TURN SELECT / PUSH OPEN",
        "rows": [
            {"key": "PROFILE", "label": "PROFILE", "value": "CLASSIC"},
            {"key": "POTS", "label": "POTS", "value": ""},
            {"key": "BUTTONS", "label": "BUTTONS", "value": ""},
            {"key": "ENCODER", "label": "ENCODER", "value": ""},
            {"key": "RESET_DEFAULTS", "label": "RESET DEFAULTS", "value": ""},
            {"key": "BACK", "label": "BACK", "value": ""},
        ],
        "profile": "CLASSIC", "voice": "SINE 1",
        "pitch": "MID", "rate": "MID", "hold": False, "echoCut": False,
        "depth": 1.0,
        "echoLoop": False,
    },
    "pads": [
        {"sound": f"PAD {number}", "variant": "CONNECT", "variantIndex": 0,
         "hold": False, "holdSupported": False}
        for number in range(1, 5)
    ],
}


def panel_config_path() -> Path:
    base = Path(os.environ.get("APPDATA", Path.home())) / "ESP-DubSiren"
    return base / "panel.json"


def _validated_panel_config(data: object) -> dict:
    if (not isinstance(data, dict) or type(data.get("schema")) is not int
            or data["schema"] != PANEL_SCHEMA_VERSION):
        schema = data.get("schema") if isinstance(data, dict) else None
        raise PanelConfigError(
            f"panel.json has an unsupported or missing schema "
            f"(expected {PANEL_SCHEMA_VERSION}, found {schema!r})"
        )
    if data.get("potBehavior") not in ("PICKUP", "JUMP"):
        raise PanelConfigError("panel.json has an invalid potBehavior")
    encoder = data.get("encoder")
    if (
        not isinstance(encoder, dict)
        or encoder.get("holdTurn") not in ("OFF", "LIVE_PITCH")
        or not isinstance(encoder.get("step"), int)
        or not isinstance(encoder.get("range"), int)
        or not 1 <= encoder["step"] <= encoder["range"] <= 24
    ):
        raise PanelConfigError("panel.json has invalid encoder settings")
    profiles = data.get("profiles")
    if not isinstance(profiles, dict):
        raise PanelConfigError("panel.json is missing profiles")
    allowed_targets = {
        "CLASSIC": {"NONE", "PITCH", "RATE", "ECHO LEVEL", "ECHO TIME",
                    "ECHO FEEDBACK", "ECHO FILTER"},
        "EXTENDED": {"NONE", "PITCH", "RATE", "LFO DEPTH", "ECHO LEVEL",
                     "ECHO TIME", "ECHO FEEDBACK", "ECHO FILTER", "DECAY",
                     "SWEEP AMOUNT", "SWEEP TIME"},
    }
    allowed_actions = {"NONE", "TRIGGER", "PLAY SOUND", "HOLD", "MOD UP",
                       "MOD DOWN", "ECHO CUT", "ECHO LOOP", "ECHO SPIN"}
    allowed_sounds = {"CLASSIC", "TWO TONE", "LICKSHOT", "PING"}
    for profile_name in ("CLASSIC", "EXTENDED"):
        profile = profiles.get(profile_name)
        if not isinstance(profile, dict):
            raise PanelConfigError(f"panel.json is missing {profile_name}")
        pots = profile.get("pots")
        buttons = profile.get("buttons")
        if not isinstance(pots, list) or len(pots) != 6:
            raise PanelConfigError(f"panel.json {profile_name} must contain six pots")
        if not isinstance(buttons, list) or len(buttons) != 4:
            raise PanelConfigError(f"panel.json {profile_name} must contain four buttons")
        for pot in pots:
            if not isinstance(pot, dict) or pot.get("target") not in allowed_targets[profile_name]:
                raise PanelConfigError(f"panel.json has an invalid {profile_name} pot target")
            if pot.get("mode") not in ("CONTINUOUS", "ZONES"):
                raise PanelConfigError("panel.json has an invalid pot mode")
            values = (pot.get("min"), pot.get("max"))
            zones = pot.get("zones")
            if (not all(isinstance(value, (int, float)) and math.isfinite(value) for value in values)
                    or values[0] >= values[1]
                    or not isinstance(zones, list) or len(zones) != 3
                    or not all(isinstance(value, (int, float)) and math.isfinite(value) for value in zones)):
                raise PanelConfigError("panel.json has invalid pot ranges")
            if pot["mode"] == "ZONES":
                if (profile_name != "CLASSIC" or pot["target"] not in ("PITCH", "RATE")
                        or not zones[0] < zones[1] < zones[2]):
                    raise PanelConfigError("panel.json has invalid zone values")
        for button in buttons:
            if not isinstance(button, dict) or button.get("action") not in allowed_actions:
                raise PanelConfigError("panel.json has an invalid button action")
            if (button.get("sound") not in allowed_sounds
                    or not isinstance(button.get("variant"), int)
                    or not isinstance(button.get("latch"), bool)):
                raise PanelConfigError("panel.json has invalid Play Sound data")
            if (profile_name == "CLASSIC" and button["action"] == "PLAY SOUND" and
                    (button["sound"] != "CLASSIC" or button["variant"] >= 2)):
                raise PanelConfigError("panel.json has an unsupported Classic sound")
    return data


def _validated_legacy_panel_config(data: dict) -> dict:
    pads = data.get("pads")
    variants = {"CLASSIC": 5, "TWO TONE": 3, "LICKSHOT": 2, "PING": 3}
    if (set(data) != {"pads"} or not isinstance(pads, list) or len(pads) != 4):
        raise PanelConfigError("panel.json has invalid legacy pad settings")
    for pad in pads:
        if (not isinstance(pad, dict) or not isinstance(pad.get("sound"), str)
                or pad["sound"] not in variants
                or type(pad.get("variantIndex")) is not int
                or not 0 <= pad["variantIndex"] < variants[pad["sound"]]):
            raise PanelConfigError("panel.json has invalid legacy pad settings")
    return data


def migrate_panel_config(legacy: dict, defaults: dict) -> dict:
    """Use the shared controller's defaults for fields absent in the old file."""
    legacy = _validated_legacy_panel_config(legacy)
    settings = json.loads(json.dumps(_validated_panel_config(defaults)))
    for button, pad in zip(settings["profiles"]["EXTENDED"]["buttons"], legacy["pads"]):
        button.update(action="PLAY SOUND", sound=pad["sound"],
                      variant=pad["variantIndex"], latch=False)
    return _validated_panel_config(settings)


def save_migrated_panel_config(path: Path, settings: dict) -> None:
    # Preserve the original bytes before replacing a recognized legacy file.
    original = path.read_bytes()
    backup = path.with_suffix(".legacy.json")
    try:
        with backup.open("xb") as destination:
            destination.write(original)
    except FileExistsError:
        if backup.read_bytes() != original:
            raise PanelConfigError(f"legacy backup already exists with different content: {backup}")
    save_panel_config(path, settings)


def load_panel_config(path: Path) -> dict | None:
    if not path.exists():
        return None
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise PanelConfigError(f"cannot read panel.json: {exc}") from exc
    except (ValueError, TypeError) as exc:
        raise PanelConfigError(f"panel.json is not valid JSON: {exc}") from exc
    try:
        if isinstance(data, dict) and "schema" not in data and "pads" in data:
            return _validated_legacy_panel_config(data)
        return _validated_panel_config(data)
    except PanelConfigError as exc:
        raise PanelConfigError(f"{exc}: {path}") from exc


def save_panel_config(path: Path, settings: dict) -> None:
    payload = json.loads(json.dumps(_validated_panel_config(settings)))
    payload.pop("revision", None)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def panel_settings_commands(settings: dict) -> list[str]:
    settings = _validated_panel_config(settings)
    commands = ["PANEL BEGIN", f"PANEL BEHAVIOR {settings['potBehavior']}"]
    encoder = settings["encoder"]
    commands.append(
        f"PANEL ENCODER {encoder['holdTurn']} {encoder['step']} {encoder['range']}"
    )
    for profile_name in ("CLASSIC", "EXTENDED"):
        profile = settings["profiles"][profile_name]
        for number, pot in enumerate(profile["pots"], start=1):
            target = pot["target"].replace(" ", "_")
            commands.append(f"PANEL POT {profile_name} {number} TARGET {target}")
            commands.append(
                f"PANEL POT {profile_name} {number} RANGE {pot['min']:.9g} {pot['max']:.9g}"
            )
            if pot["mode"] == "ZONES":
                zones = " ".join(f"{value:.9g}" for value in pot["zones"])
                commands.append(f"PANEL POT {profile_name} {number} MODE ZONES")
                commands.append(f"PANEL POT {profile_name} {number} ZONES {zones}")
        for number, button in enumerate(profile["buttons"], start=1):
            action = button["action"].replace(" ", "_")
            commands.append(f"PANEL BUTTON {profile_name} {number} ACTION {action}")
            if button["action"] == "PLAY SOUND":
                sound = button["sound"].replace(" ", "_")
                commands.append(
                    f"PANEL BUTTON {profile_name} {number} SOUND {sound} "
                    f"{button['variant']} {int(button['latch'])}"
                )
    commands.append("PANEL COMMIT")
    return commands


def panel_settings_signature(settings: dict | None) -> object:
    if settings is None:
        return None
    value = json.loads(json.dumps(settings))
    value.pop("revision", None)

    def normalized(item):
        if isinstance(item, float):
            return round(item, 5)
        if isinstance(item, list):
            return [normalized(child) for child in item]
        if isinstance(item, dict):
            return {key: normalized(child) for key, child in item.items()}
        return item

    return normalized(value)


class DialControl(ttk.Frame):
    def __init__(
        self,
        parent: tk.Misc,
        label: str,
        command_name: str,
        spec: ControlSpec,
        callback,
        diameter: int = 104,
    ) -> None:
        super().__init__(parent)
        self.command_name = command_name
        self.spec = spec
        self.callback = callback
        self.value = spec.default
        self._diameter = diameter
        self._drag_y: int | None = None
        self._drag_position = spec.to_normalized(spec.default)
        self._active = True

        ttk.Label(self, text=label, anchor="center").pack(fill="x")
        self.canvas = tk.Canvas(
            self,
            width=diameter,
            height=diameter,
            bg="#1b1f24",
            highlightthickness=0,
            cursor="sb_v_double_arrow",
        )
        self.canvas.pack(pady=(3, 2))
        self.value_label = ttk.Label(self, anchor="center")
        self.value_label.pack(fill="x")

        self.canvas.bind("<ButtonPress-1>", self._pressed)
        self.canvas.bind("<B1-Motion>", self._dragged)
        self.canvas.bind("<ButtonRelease-1>", self._released)
        self.canvas.bind("<MouseWheel>", self._wheel)
        self.canvas.bind("<Button-4>", lambda _event: self._step(0.02, True))
        self.canvas.bind("<Button-5>", lambda _event: self._step(-0.02, True))
        self.set_value(spec.default)

    def set_value(self, value: float, notify: bool = False, final: bool = True) -> None:
        self.value = max(self.spec.minimum, min(self.spec.maximum, value))
        self._draw()
        self.value_label.configure(text=self.spec.formatter(self.value))
        if notify:
            self.callback(self.command_name, self.value, final)

    def _pressed(self, event) -> None:
        if not self._active:
            return
        self._drag_y = event.y_root
        self._drag_position = self.spec.to_normalized(self.value)

    def _dragged(self, event) -> None:
        if self._drag_y is None or not self._active:
            return
        # Vertical dragging is predictable for a virtual rotary control and
        # does not require the pointer to trace a circular path.
        delta = (self._drag_y - event.y_root) / 170.0
        position = max(0.0, min(1.0, self._drag_position + delta))
        self.set_value(self.spec.from_normalized(position), notify=True, final=False)

    def _released(self, _event) -> None:
        self.finish_drag()

    def _wheel(self, event) -> None:
        self._step(0.02 if event.delta > 0 else -0.02, True)

    def _step(self, delta: float, final: bool) -> None:
        if not self._active:
            return
        position = self.spec.to_normalized(self.value)
        position = max(0.0, min(1.0, position + delta))
        self.set_value(self.spec.from_normalized(position), notify=True, final=final)

    def finish_drag(self) -> None:
        if self._drag_y is None:
            return
        self._drag_y = None
        self.callback(self.command_name, self.value, True)

    def set_active(self, active: bool) -> None:
        self._active = active
        self.canvas.configure(cursor="sb_v_double_arrow" if active else "arrow")
        self.label_state = "normal" if active else "disabled"
        self._draw()

    def _draw(self) -> None:
        self.canvas.delete("all")
        size = self._diameter
        cx = cy = size / 2
        radius = size * 0.36
        position = self.spec.to_normalized(self.value)
        angle = math.radians(225.0 - 270.0 * position)

        self.canvas.create_arc(
            8,
            8,
            size - 8,
            size - 8,
            start=-45,
            extent=-270,
            style="arc",
            width=3,
            outline="#59636f",
        )
        self.canvas.create_oval(
            cx - radius,
            cy - radius,
            cx + radius,
            cy + radius,
            fill="#343b44",
            outline="#7f8a96",
            width=2,
        )
        x2 = cx + math.cos(angle) * radius * 0.72
        y2 = cy - math.sin(angle) * radius * 0.72
        pointer = "#e7e9ec" if self._active else "#687078"
        self.canvas.create_line(cx, cy, x2, y2, fill=pointer, width=4)
        self.canvas.create_oval(
            cx - 4,
            cy - 4,
            cx + 4,
            cy + 4,
            fill="#c7ccd2",
            outline="",
        )


class PanelButton(ttk.Frame):
    def __init__(self, parent: tk.Misc, text: str) -> None:
        super().__init__(parent)
        self._text = text
        self._state = "normal"
        self._pressed = False
        self.label = ttk.Label(self, text=text, anchor="center")
        self.label.pack(fill="x")
        self.canvas = tk.Canvas(
            self,
            width=88,
            height=70,
            bg="#1b1f24",
            highlightthickness=0,
            cursor="hand2",
        )
        self.canvas.pack()
        self.canvas.bind("<ButtonPress-1>", self._show_pressed)
        self.canvas.bind("<ButtonRelease-1>", self._show_released)
        self._draw()

    def bind(self, sequence=None, func=None, add=None):
        return self.canvas.bind(sequence, func, add)

    def configure(self, cnf=None, **kwargs):
        if cnf:
            kwargs.update(cnf)
        if "text" in kwargs:
            self._text = kwargs.pop("text")
            self.label.configure(text=self._text)
        if "state" in kwargs:
            self._state = kwargs.pop("state")
            self.canvas.configure(
                cursor="arrow" if self._state == "disabled" else "hand2"
            )
        if kwargs:
            super().configure(**kwargs)
        self._draw()

    config = configure

    def cget(self, key):
        if key == "text":
            return self._text
        if key == "state":
            return self._state
        return super().cget(key)

    def instate(self, statespec=None, callback=None, *args):
        disabled = self._state == "disabled"
        matches = statespec == ["disabled"] and disabled
        if callback is not None and matches:
            return callback(*args)
        return matches

    def set_pressed(self, pressed: bool) -> None:
        self._pressed = pressed and self._state != "disabled"
        self._draw()

    def _show_pressed(self, _event) -> None:
        self.set_pressed(True)

    def _show_released(self, _event) -> None:
        self.set_pressed(False)

    def _draw(self) -> None:
        self.canvas.delete("all")
        disabled = self._state == "disabled"
        rim = "#3d4349" if disabled else "#8c959f"
        cap = (
            "#3b4147"
            if disabled
            else ("#a8322c" if self._pressed else "#c83c34")
        )
        inset = 8 if self._pressed else 5
        self.canvas.create_oval(11, 4, 77, 68, fill="#111418", outline=rim, width=3)
        self.canvas.create_oval(
            16,
            inset + 4,
            72,
            inset + 58,
            fill=cap,
            outline="#68201d" if not disabled else "#30353a",
            width=2,
        )
        self.canvas.create_oval(
            28,
            inset + 13,
            60,
            inset + 25,
            fill="#e76a60" if not disabled else "#4b5157",
            outline="",
        )


class EncoderControl(ttk.Frame):
    def __init__(
        self, parent: tk.Misc, rotate_callback, press_callback, release_callback
    ) -> None:
        super().__init__(parent)
        self.rotate_callback = rotate_callback
        self.press_callback = press_callback
        self.release_callback = release_callback

        ttk.Label(self, text="ENCODER", anchor="center").pack(fill="x")
        self.canvas = tk.Canvas(
            self,
            width=112,
            height=112,
            bg="#1b1f24",
            highlightthickness=0,
            cursor="hand2",
        )
        self.canvas.pack(pady=(3, 4))
        self.canvas.create_oval(
            18, 18, 94, 94, fill="#343b44", outline="#7f8a96", width=2
        )
        self.canvas.create_line(56, 56, 56, 28, fill="#e7e9ec", width=4)
        self.canvas.create_text(
            56,
            74,
            text="PRESS",
            fill="#c9d0d7",
            font=("Segoe UI", 8, "bold"),
        )
        self.canvas.bind("<ButtonPress-1>", lambda _event: press_callback("mouse"))
        self.canvas.bind("<ButtonRelease-1>", lambda _event: release_callback("mouse"))
        self.canvas.bind("<MouseWheel>", self._wheel)
        self.canvas.bind("<Button-4>", lambda _event: rotate_callback(-1))
        self.canvas.bind("<Button-5>", lambda _event: rotate_callback(1))

        emulator = ttk.Frame(self)
        emulator.pack(fill="x")
        ttk.Button(
            emulator,
            text="−",
            width=3,
            command=lambda: rotate_callback(-1),
        ).pack(side="left")
        ttk.Button(
            emulator,
            text="PRESS",
            command=self._click,
        ).pack(side="left", expand=True, fill="x", padx=3)
        ttk.Button(
            emulator,
            text="+",
            width=3,
            command=lambda: rotate_callback(1),
        ).pack(side="left")

    def _wheel(self, event) -> None:
        self.rotate_callback(-1 if event.delta > 0 else 1)

    def _click(self) -> None:
        self.press_callback("button")
        self.release_callback("button")


class PhysicalDubSirenApp(DubSirenApp):
    def __init__(self, root: tk.Tk) -> None:
        self.parameter_values = {
            name: spec.default for name, spec in CONTROL_SPECS.items()
        }
        self.parameter_values["HPF_HZ"] = 60.0
        self.parameter_values["LPF_HZ"] = 7000.0
        self.panel_state = json.loads(json.dumps(DEFAULT_PANEL_STATE))
        self._last_menu_revision = 0
        self._panel_status_seen = False
        self._input_sources: dict[str, set[str]] = {}
        self._encoder_sources: set[str] = set()
        self._saved_config_applied = False
        self._virtual_restore_pending = False
        self._connected_virtual = False
        self._last_saved_settings_revision: int | None = None
        self._panel_config_path = panel_config_path()
        self._panel_config_error: str | None = None
        self._panel_io_error: str | None = None
        self._load_saved_panel_config()
        self.overlay_token: str | None = None
        self.dials: dict[str, DialControl] = {}
        super().__init__(root)
        self.root.title("ESP Dub Siren")
        self.root.minsize(980, 700)
        self._bind_keyboard()
        self.root.bind("<FocusOut>", self._focus_out, add="+")
        self._refresh_display()

    def _build_ui(self) -> None:
        outer = ttk.Frame(self.root, padding=10)
        outer.pack(fill="both", expand=True)

        connection = ttk.Frame(outer)
        connection.pack(fill="x", pady=(0, 10))
        ttk.Label(connection, text="DEVICE").pack(side="left")
        self.port_combo = ttk.Combobox(
            connection,
            textvariable=self.port_var,
            state="readonly",
            width=20,
        )
        self.port_combo.pack(side="left", padx=(6, 3))
        ttk.Button(
            connection,
            text="Refresh",
            command=self.refresh_ports,
        ).pack(side="left", padx=3)
        self.connect_button = ttk.Button(
            connection,
            text="Connect",
            command=self.toggle_connection,
        )
        self.connect_button.pack(side="left", padx=(3, 12))
        ttk.Label(connection, text="OUTPUT").pack(side="left")
        self.output_combo = ttk.Combobox(
            connection,
            textvariable=self.output_var,
            values=tuple(OUTPUT_VALUES),
            state="readonly",
            width=10,
        )
        self.output_combo.pack(side="left", padx=6)
        self.output_combo.bind("<<ComboboxSelected>>", self._output_changed)
        self.audio_button = ttk.Button(
            connection,
            text="Start Audio",
            command=self.toggle_audio,
            state="disabled",
        )
        self.audio_button.pack(side="left", padx=3)
        ttk.Label(
            connection,
            textvariable=self.connection_var,
        ).pack(side="right", padx=6)

        panel = ttk.LabelFrame(outer, text="FRONT PANEL", padding=14)
        panel.pack(fill="both", expand=True)
        for column in range(6):
            panel.columnconfigure(column, weight=1, uniform="panel")

        for index, (label, command_name) in enumerate(PANEL_KNOBS):
            row = 0 if index < 3 else 1
            column = index if index < 3 else index - 3
            dial = DialControl(
                panel,
                label,
                command_name,
                FRONT_POT_SPEC,
                self._panel_parameter_changed,
            )
            dial.grid(
                row=row,
                column=column,
                padx=8,
                pady=8,
                sticky="nsew",
            )
            self.dials[command_name] = dial

        display_frame = ttk.Frame(panel)
        display_frame.grid(
            row=0,
            column=3,
            columnspan=2,
            rowspan=2,
            padx=12,
            pady=8,
            sticky="",
        )
        ttk.Label(
            display_frame,
            text="DISPLAY",
            anchor="center",
        ).pack(fill="x", pady=(0, 4))
        self.display = PanelDisplay(display_frame)
        self.display.pack(fill="both", expand=True)

        self.encoder = EncoderControl(
            panel,
            self._encoder_rotate,
            self._encoder_down,
            self._encoder_up,
        )
        self.encoder.grid(
            row=0,
            column=5,
            rowspan=2,
            padx=8,
            pady=8,
            sticky="n",
        )

        buttons = ttk.Frame(panel)
        buttons.grid(
            row=2,
            column=0,
            columnspan=6,
            sticky="ew",
            pady=(16, 2),
        )
        for column in range(4):
            buttons.columnconfigure(column, weight=1, uniform="buttons")

        self.pad_buttons = [
            PanelButton(buttons, text=f"BTN{number} · PAD {number}")
            for number in range(1, 5)
        ]
        for index, button in enumerate(
            self.pad_buttons
        ):
            button.grid(
                row=0,
                column=index,
                sticky="ew",
                padx=6,
            )

            button.bind(
                "<ButtonPress-1>",
                lambda _event, pad=index + 1: self._pad_down(pad, "mouse"),
                add="+",
            )

        rear = ttk.LabelFrame(outer, text="REAR / MASTER", padding=8)
        rear.pack(fill="x", pady=(10, 0))
        self.master_dial = DialControl(
            rear,
            "MASTER",
            "MASTER",
            CONTROL_SPECS["MASTER"],
            self._panel_parameter_changed,
            diameter=74,
        )
        self.master_dial.pack(side="left", padx=8)
        self.dials["MASTER"] = self.master_dial
        ttk.Label(
            rear,
            text=(
                "Secondary sound options are controlled "
                "from the encoder/display."
            ),
            justify="left",
        ).pack(side="left", fill="x", expand=True, padx=12)
        ttk.Label(
            rear,
            text="KEYS  1–4 pads   [ / ] encoder   E press",
            justify="right",
        ).pack(side="right", padx=12)
        self._update_pad_labels()

    def _panel_parameter_changed(
        self,
        name: str,
        value: float,
        final: bool,
    ) -> None:
        self.parameter_values[name] = value
        if name.startswith("POT_"):
            number = int(name.removeprefix("POT_"))
            send = lambda: self._send(f"POT {number} {self.parameter_values[name]:.6g}")
        else:
            self._show_overlay(self._parameter_label(name), CONTROL_SPECS[name].formatter(value))
            send = lambda: self._send_parameter(name, self.parameter_values[name])

        if final:
            pending = self.pending_sends.pop(name, None)
            if pending is not None:
                self.root.after_cancel(pending)
            send()
        elif name not in self.pending_sends:
            self.pending_sends[name] = self.root.after(
                25, lambda parameter=name: self._flush_panel_parameter(parameter)
            )

    def _flush_panel_parameter(self, name: str) -> None:
        self.pending_sends.pop(name, None)
        if name.startswith("POT_"):
            number = int(name.removeprefix("POT_"))
            self._send(f"POT {number} {self.parameter_values[name]:.6g}")
        else:
            self._send_parameter(name, self.parameter_values[name])

    def _parameter_label(self, name: str) -> str:
        return {
            "PITCH_POSITION": "PITCH",
            "RATE_POSITION": "RATE",
            "TUNE_HZ": "TUNE",
            "LFO_RATE_HZ": "RATE",
            "LFO_DEPTH_OCT": "DEPTH",
            "DELAY_MS": "ECHO TIME",
            "FEEDBACK": "ECHO FEEDBACK",
            "ECHO_LEVEL": "ECHO LEVEL",
            "ECHO_FILTER": "ECHO TONE",
            "MASTER": "MASTER",
            "DECAY_MS": "DECAY",
            "SWEEP": "SWEEP",
            "SWEEP_MS": "SWEEP TIME",
            "SYNDRUM_SWEEP": "DRUM SWEEP",
        }.get(name, name)

    def _encoder_rotate(self, direction: int) -> None:
        self._send(f"ENCODER_ROTATE {'NEXT' if direction > 0 else 'PREV'}")

    def _encoder_down(self, source: str) -> None:
        was_up = not self._encoder_sources
        self._encoder_sources.add(source)
        if was_up:
            self._send("ENCODER_SWITCH 1")

    def _encoder_up(self, source: str) -> None:
        if source not in self._encoder_sources:
            return
        self._encoder_sources.discard(source)
        if self._encoder_sources:
            return
        if self.transport.connected:
            self._send("ENCODER_SWITCH 0")

    def _menu_items(self) -> list[tuple[str, str, str]]:
        display = self.panel_state.get("display", {})
        return [
            (str(row.get("key", "")), str(row.get("label", "")),
             str(row.get("value", "")))
            for row in display.get("rows", ())
            if isinstance(row, dict)
        ]

    def _pad_down(self, pad: int, source: str) -> None:
        action = f"PAD_{pad}"
        sources = self._input_sources.setdefault(action, set())
        if source in sources:
            return
        first = not sources
        sources.add(source)
        if first:
            self._send(f"PAD {pad} 1")
        self.pad_buttons[pad - 1].set_pressed(True)

    def _pad_up(self, pad: int, source: str) -> None:
        action = f"PAD_{pad}"
        sources = self._input_sources.setdefault(action, set())
        if source not in sources:
            return
        sources.remove(source)
        if not sources:
            self._send(f"PAD {pad} 0")
        self.pad_buttons[pad - 1].set_pressed(bool(sources))

    def _bind_keyboard(self) -> None:
        for pad, key in enumerate(("1", "2", "3", "4"), start=1):
            self.root.bind_all(
                f"<KeyPress-{key}>",
                lambda _event, number=pad, source=key: self._pad_down(number, f"key-{source}"),
                add="+",
            )
            self.root.bind_all(
                f"<KeyRelease-{key}>",
                lambda _event, number=pad, source=key: self._pad_up(number, f"key-{source}"),
                add="+",
            )
        self.root.bind_all("<KeyPress-bracketleft>", lambda _event: self._encoder_rotate(-1), add="+")
        self.root.bind_all("<KeyPress-bracketright>", lambda _event: self._encoder_rotate(1), add="+")
        self.root.bind_all("<KeyPress-e>", lambda _event: self._encoder_down("key-e"), add="+")
        self.root.bind_all("<KeyRelease-e>", lambda _event: self._encoder_up("key-e"), add="+")

    def _focus_out(self, _event=None) -> None:
        self.root.after_idle(self._release_if_unfocused)

    def _release_if_unfocused(self) -> None:
        if self.root.focus_get() is None:
            self._global_release()

    def _global_release(self, event=None) -> None:
        for dial in self.dials.values():
            dial.finish_drag()
        if event is None:
            self._input_sources.clear()
            self._encoder_sources.clear()
            for button in self.pad_buttons:
                button.set_pressed(False)
            self._send("PANEL_RESET")
            return
        for pad in range(1, 5):
            self._pad_up(pad, "mouse")
        self._encoder_up("mouse")

    def _update_pad_labels(self) -> None:
        if not hasattr(self, "pad_buttons"):
            return
        actions = self.panel_state.get("display", {}).get("buttons", [])
        profile_name = self.panel_state.get("display", {}).get("profile", "CLASSIC")
        configured_buttons = self.panel_state.get("settings", {}).get(
            "profiles", {}
        ).get(profile_name, {}).get("buttons", [])
        for number, (button, pad) in enumerate(
            zip(self.pad_buttons, self.panel_state["pads"]), start=1
        ):
            action = actions[number - 1] if len(actions) == 4 else "PLAY SOUND"
            if action == "PLAY SOUND" and len(configured_buttons) == 4:
                label = configured_buttons[number - 1].get("sound", pad["sound"])
            else:
                label = pad["sound"] if action == "PLAY SOUND" else action
            button.configure(text=f"BTN{number} - {label}")
        self._update_control_activity()

    def _update_control_activity(self) -> None:
        display = self.panel_state.get("display", {})
        profile_name = display.get("profile", "CLASSIC")
        settings = self.panel_state.get("settings", {})
        profile = settings.get("profiles", {}).get(profile_name, {})
        pots = profile.get("pots", [])
        for number in range(1, 7):
            name = f"POT_{number}"
            if name not in self.dials:
                continue
            target = pots[number - 1].get("target", "CONNECT") if len(pots) == 6 else "CONNECT"
            self.dials[name].set_active(True)
            self.dials[name].value_label.configure(text=target)

    def _show_overlay(
        self,
        title: str,
        value: str,
    ) -> None:
        if self.panel_state.get("display", {}).get("mode") != "HOME":
            self._refresh_display()
            return
        if self.overlay_token is not None:
            self.root.after_cancel(self.overlay_token)
        self.display.render_overlay(title, value)
        self.overlay_token = self.root.after(
            900,
            self._overlay_finished,
        )

    def _overlay_finished(self) -> None:
        self.overlay_token = None
        self._refresh_display()

    def _refresh_display(self) -> None:
        if not hasattr(self, "display"):
            return
        if self.overlay_token is not None:
            return

        display = self.panel_state.get("display", DEFAULT_PANEL_STATE["display"])
        if display.get("mode") == "HOME" and display.get("profile", "CLASSIC") == "CLASSIC":
            params = dict(self.parameter_values, PROFILE="CLASSIC",
                          PITCH=(display.get("pitch", "MID")
                                 if display.get("pitchMode", "ZONES") == "ZONES" else ""),
                          RATE=(display.get("rate", "MID")
                                if display.get("rateMode", "ZONES") == "ZONES" else ""),
                          ECHO_CUT=display.get("echoCut", False))
            self.display.render_home(1, 1, display.get("voice", "SINE 1"), "DS01E / CLASSIC", "",
                                     bool(display.get("hold")), bool(display.get("echoLoop")),
                                     self.transport.connected, params,
                                     str(display.get("filter", "FLAT")))
        elif display.get("mode") == "HOME":
            selected = int(self.panel_state["selectedPad"])
            current = int(self.panel_state.get("currentPad", selected))
            pad = self.panel_state["pads"][selected - 1]
            current_pad = self.panel_state["pads"][current - 1]
            depth = CONTROL_SPECS["LFO_DEPTH_OCT"].formatter(
                self.parameter_values["LFO_DEPTH_OCT"]
            )
            if current_pad["sound"] == "PING":
                depth = "STORED · PING N/A"
            self.display.render_home(
                selected,
                current,
                pad["sound"],
                pad["variant"],
                depth,
                current_pad["hold"],
                bool(display.get("echoLoop")),
                self.transport.connected,
                self.parameter_values,
                str(display.get("filter", "CUSTOM")),
            )
        else:
            items = self._menu_items()
            if not items:
                return
            menu_index = min(int(display.get("menuIndex", 0)), len(items) - 1)
            self.display.render_menu(
                [
                    (label, value)
                    for _key, label, value in items
                ],
                menu_index,
                bool(display.get("editing")),
                str(display.get("title", "SETTINGS")),
                str(display.get("footer", "")),
            )

    def _send_parameter(
        self,
        name: str,
        value: float,
    ) -> None:
        self._send(f"SET {name} {value:.6g}")

    def _synchronize_controls(self) -> None:
        selected = self._selected_output()
        self._send(f"SET OUTPUT {selected}")
        self.active_output = selected
        self._send("PANEL_RESET")
        if self._connected_virtual and not self._saved_config_applied:
            self._saved_config_applied = True
            if self._saved_panel_config is not None:
                self._virtual_restore_pending = True
                applied = False
                try:
                    legacy = "schema" not in self._saved_panel_config
                    settings = (migrate_panel_config(
                        self._saved_panel_config, self.panel_state.get("settings", {})
                    ) if legacy else self._saved_panel_config)
                    for command in panel_settings_commands(settings):
                        self.transport.send(command)
                    applied = True
                    self._saved_panel_config = settings
                    if legacy:
                        save_migrated_panel_config(self._panel_config_path, settings)
                        self._panel_config_error = self._panel_io_error = None
                except Exception as exc:
                    if not applied:
                        try:
                            self.transport.send("PANEL CANCEL")
                        except Exception:
                            pass
                        self._virtual_restore_pending = False
                    # After COMMIT, await its STATUS even if saving fails, so
                    # the queued acknowledgement cannot overwrite the old file.
                    action = "save" if applied else "apply"
                    self._panel_io_error = f"panel.json {action} failed: {exc}"
                    self.connection_var.set(self._panel_io_error)
            elif self._panel_config_error:
                self.connection_var.set(self._panel_config_error)
        self._refresh_display()

    def _handle_status(self, status: object) -> None:
        if isinstance(status, dict):
            self._connected_virtual = status.get("virtual") is True
            panel = status.get("panel")
            if (
                isinstance(panel, dict)
                and isinstance(panel.get("pads"), list)
                and len(panel["pads"]) == 4
            ):
                old_display = self.panel_state.get("display", {})
                menu_revision = int(panel.get("menuRevision", 0))
                self._panel_status_seen = True
                self._last_menu_revision = menu_revision
                self.panel_state = panel
                display = panel.get("display", {})
                for key, parameter in (
                    ("tuneHz", "TUNE_HZ"), ("rateHz", "LFO_RATE_HZ"),
                    ("depth", "LFO_DEPTH_OCT"), ("echoLevel", "ECHO_LEVEL"),
                    ("delayMs", "DELAY_MS"), ("feedback", "FEEDBACK"),
                    ("master", "MASTER"),
                ):
                    if key in display:
                        self.parameter_values[parameter] = float(display[key])
                for key, title in (("pitch", "PITCH"), ("rate", "RATE")):
                    if (display.get("mode") == "HOME"
                            and display.get(key) != old_display.get(key)
                            and display.get("profile") == "CLASSIC"):
                        self._show_overlay(title, display.get(key, "MID"))
                if "depth" in display:
                    old_depth = self.parameter_values["LFO_DEPTH_OCT"]
                    depth = float(display["depth"])
                    self.parameter_values["LFO_DEPTH_OCT"] = depth
                    if (
                        display.get("mode") == "HOME"
                        and abs(old_depth - depth) > 0.0001
                    ):
                        current = int(panel.get("currentPad", panel["selectedPad"]))
                        ping = panel["pads"][current - 1]["sound"] == "PING"
                        self._show_overlay(
                            "DEPTH · STORED (PING N/A)" if ping else "DEPTH",
                            CONTROL_SPECS["LFO_DEPTH_OCT"].formatter(depth),
                        )
                if "echoLoop" in display:
                    self.echo_loop_var.set(bool(display["echoLoop"]))
                if "MASTER" in self.dials and hasattr(self.dials["MASTER"], "set_value"):
                    self.dials["MASTER"].set_value(self.parameter_values["MASTER"])
                self._update_pad_labels()
                if old_display.get("mode") != display.get("mode"):
                    if self.overlay_token is not None:
                        self.root.after_cancel(self.overlay_token)
                        self.overlay_token = None
                overlay = display.get("overlay", {})
                if (
                    display.get("mode") == "HOME"
                    and isinstance(overlay, dict)
                    and overlay.get("title")
                    and overlay != old_display.get("overlay", {})
                ):
                    self._show_overlay(str(overlay["title"]), str(overlay.get("value", "")))

                settings = panel.get("settings")
                if self._connected_virtual and isinstance(settings, dict):
                    settings_revision = int(settings.get("revision", 0))
                    comparable = panel_settings_signature(settings)
                    saved = panel_settings_signature(self._saved_panel_config)
                    if self._virtual_restore_pending:
                        if saved == comparable:
                            self._virtual_restore_pending = False
                            self._last_saved_settings_revision = settings_revision
                    elif self._last_saved_settings_revision is None:
                        self._last_saved_settings_revision = settings_revision
                    elif settings_revision != self._last_saved_settings_revision:
                        self._last_saved_settings_revision = settings_revision
                        try:
                            save_panel_config(self._panel_config_path, settings)
                            self._saved_panel_config = comparable
                            self._panel_config_error = self._panel_io_error = None
                        except (OSError, PanelConfigError) as exc:
                            self._panel_io_error = f"panel.json save failed: {exc}"
                            self.connection_var.set(self._panel_io_error)
                if panel.get("persistenceStatus"):
                    self.connection_var.set(str(panel["persistenceStatus"]))
        super()._handle_status(status)
        if isinstance(status, dict) and isinstance(status.get("panel"), dict):
            persistence_status = status["panel"].get("persistenceStatus")
            if persistence_status:
                self.connection_var.set(str(persistence_status))
        if self._panel_config_error and self._connected_virtual:
            self.connection_var.set(self._panel_config_error)
        elif self._panel_io_error and self._connected_virtual:
            self.connection_var.set(self._panel_io_error)
        self._refresh_display()

    def _load_saved_panel_config(self) -> None:
        self._panel_config_error = None
        try:
            self._saved_panel_config = load_panel_config(self._panel_config_path)
        except PanelConfigError as exc:
            self._saved_panel_config = None
            self._panel_config_error = str(exc)

    def connect(self) -> None:
        self._saved_config_applied = False
        self._virtual_restore_pending = False
        self._connected_virtual = False
        self._last_saved_settings_revision = None
        self._panel_io_error = None
        self._load_saved_panel_config()
        self._panel_status_seen = False
        super().connect()
        self._refresh_display()

    def disconnect(self) -> None:
        super().disconnect()
        self._refresh_display()


def main() -> None:
    root = tk.Tk()
    PhysicalDubSirenApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
