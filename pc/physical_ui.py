import json
import math
import os
import tkinter as tk
from pathlib import Path
from tkinter import ttk

from app import DubSirenApp, OUTPUT_VALUES
from control_ranges import CONTROL_SPECS, ControlSpec

ECHO_FILTER_DEADBAND = 0.05


def echo_filter_amount(value: float) -> float:
    distance = max(0.0, min(1.0, abs(value)))
    if distance <= ECHO_FILTER_DEADBAND:
        return 0.0
    return (distance - ECHO_FILTER_DEADBAND) / (1.0 - ECHO_FILTER_DEADBAND)


def format_echo_filter(value: float) -> str:
    amount = echo_filter_amount(value)
    if amount == 0.0:
        return "FLAT"
    return f"{'LP' if value < 0 else 'HP'} {round(amount * 100)}%"


def echo_filter_frequencies(value: float) -> tuple[float, float]:
    amount = echo_filter_amount(value)
    if amount == 0.0:
        return 60.0, 16000.0
    if value < 0.0:
        return 60.0, 16000.0 * ((400.0 / 16000.0) ** amount)
    return 60.0 * ((4000.0 / 60.0) ** amount), 16000.0


ECHO_FILTER_SPEC = ControlSpec(-1.0, 1.0, 0.0, format_echo_filter)

PANEL_KNOBS = (
    ("TUNE", "TUNE_HZ"),
    ("RATE", "LFO_RATE_HZ"),
    ("ECHO LEVEL", "ECHO_LEVEL"),
    ("ECHO TIME", "DELAY_MS"),
    ("ECHO FEEDBACK", "FEEDBACK"),
    ("ECHO TONE", "ECHO_FILTER"),
)

DEFAULT_PANEL_STATE = {
    "revision": 0,
    "menuRevision": 0,
    "selectedPad": 1,
    "currentPad": 1,
    "activePad": 0,
    "pads": [
        {"sound": f"PAD {number}", "variant": "CONNECT", "variantIndex": 0,
         "hold": False, "holdSupported": False}
        for number in range(1, 5)
    ],
}


def panel_config_path() -> Path:
    base = Path(os.environ.get("APPDATA", Path.home())) / "ESP-DubSiren"
    return base / "panel.json"


def load_panel_config(path: Path) -> list[dict]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError, TypeError):
        return []
    pads = data.get("pads") if isinstance(data, dict) else None
    if not isinstance(pads, list) or len(pads) != 4:
        return []
    result = []
    for pad in pads:
        if not isinstance(pad, dict):
            return []
        sound = pad.get("sound")
        variant = pad.get("variantIndex")
        if not isinstance(sound, str) or not isinstance(variant, int):
            return []
        result.append({"sound": sound, "variantIndex": variant})
    return result


def save_panel_config(path: Path, pads: list[dict]) -> None:
    payload = {
        "pads": [
            {
                "sound": pad["sound"],
                "variantIndex": pad["variantIndex"],
            }
            for pad in pads
        ]
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


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


class PanelDisplay(ttk.Frame):
    def __init__(self, parent: tk.Misc) -> None:
        super().__init__(parent)
        self.canvas = tk.Canvas(
            self,
            width=340,
            height=182,
            bg="#07110c",
            highlightbackground="#4c5550",
            highlightthickness=2,
        )
        self.canvas.pack(fill="both", expand=True)

    def render_home(
        self,
        selected_pad: int,
        current_pad: int,
        sound: str,
        variant: str,
        depth: str,
        hold: bool,
        echo_loop: bool,
        connected: bool,
    ) -> None:
        self.canvas.delete("all")
        self._text(18, 18, "ESP DUB SIREN", 12, "#6d9079", "nw")
        self._text(315, 18, f"PAD {selected_pad}", 11, "#6d9079", "ne")
        self._text(170, 62, sound, 26, "#d9f5df", "center")
        self._text(170, 98, variant, 12, "#9cc9a6", "center")
        self._text(170, 120, f"DEPTH {depth}", 10, "#799382", "center")
        flags = []
        if hold:
            flags.append("HOLD")
        if echo_loop:
            flags.append("LOOP")
        if connected:
            flags.append("LINK")
        if current_pad != selected_pad:
            flags.append(f"LIVE P{current_pad}")
        self._text(170, 146, "  ·  ".join(flags) or "READY", 10, "#6d9079", "center")
        self._text(170, 168, "ENC: depth · press menu", 9, "#53695a", "center")

    def render_overlay(self, title: str, value: str) -> None:
        self.canvas.delete("all")
        self._text(170, 48, title, 14, "#80a98a", "center")
        self._text(170, 98, value, 30, "#e1f7e5", "center")
        self._text(170, 151, "live control", 10, "#53695a", "center")

    def render_menu(self, items: list[tuple[str, str]], index: int, editing: bool) -> None:
        self.canvas.delete("all")
        title = "MENU / EDIT" if editing else "MENU"
        self._text(16, 14, title, 12, "#7ca987", "nw")
        start = max(0, min(index - 1, max(0, len(items) - 3)))
        y = 52
        for absolute, (label, value) in enumerate(items[start:start + 3], start=start):
            active = absolute == index
            if active:
                self.canvas.create_rectangle(
                    10, y - 10, 330, y + 30, fill="#14311f", outline=""
                )
            prefix = "> " if active else "  "
            self._text(
                18,
                y,
                prefix + label,
                13,
                "#d6f0db" if active else "#799382",
                "nw",
            )
            self._text(
                318,
                y,
                value,
                12,
                "#a9cdb0" if active else "#68796d",
                "ne",
            )
            y += 48
        footer = (
            "rotate change · press confirm"
            if editing
            else "rotate select · press open"
        )
        self._text(170, 170, footer, 9, "#53695a", "center")

    def _text(
        self,
        x: int,
        y: int,
        value: str,
        size: int,
        color: str,
        anchor: str,
    ) -> None:
        self.canvas.create_text(
            x,
            y,
            text=value,
            fill=color,
            font=("Consolas", size, "bold" if size >= 12 else "normal"),
            anchor=anchor,
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
        self.echo_filter_macro = 0.0
        self.echo_filter_active = False
        self.panel_state = json.loads(json.dumps(DEFAULT_PANEL_STATE))
        self._last_menu_revision = 0
        self._panel_status_seen = False
        self._input_sources: dict[str, set[str]] = {}
        self._encoder_sources: set[str] = set()
        self._local_modifier_used = False
        self._save_config_on_status = False
        self._saved_config_applied = False
        self._panel_config_path = panel_config_path()
        self._saved_pad_configs = load_panel_config(self._panel_config_path)
        self.display_mode = "home"
        self.menu_index = 0
        self.menu_editing = False
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
                (
                    ECHO_FILTER_SPEC
                    if command_name == "ECHO_FILTER"
                    else CONTROL_SPECS[command_name]
                ),
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
        self.dials["ECHO_FILTER"].value_label.configure(text="CUSTOM 60 Hz / 7 kHz")
        self._update_pad_labels()

    def _panel_parameter_changed(
        self,
        name: str,
        value: float,
        final: bool,
    ) -> None:
        if name == "ECHO_FILTER":
            self.echo_filter_macro = value
            self.echo_filter_active = True
            self._show_overlay("ECHO TONE", format_echo_filter(value))
        else:
            self.parameter_values[name] = value
            self._show_overlay(
                self._parameter_label(name),
                CONTROL_SPECS[name].formatter(value),
            )

        if final:
            pending = self.pending_sends.pop(name, None)
            if pending is not None:
                self.root.after_cancel(pending)
            if name == "ECHO_FILTER":
                self._apply_echo_filter()
            else:
                self._send_parameter(name, value)
        elif name not in self.pending_sends:
            callback = (
                self._flush_echo_filter
                if name == "ECHO_FILTER"
                else lambda parameter=name: self._flush_panel_parameter(parameter)
            )
            self.pending_sends[name] = self.root.after(25, callback)

    def _flush_echo_filter(self) -> None:
        self.pending_sends.pop("ECHO_FILTER", None)
        self._apply_echo_filter()

    def _flush_panel_parameter(self, name: str) -> None:
        self.pending_sends.pop(name, None)
        self._send_parameter(name, self.parameter_values[name])

    def _parameter_label(self, name: str) -> str:
        return {
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
        if self.display_mode == "home":
            self._step_parameter("LFO_DEPTH_OCT", direction)
            value = self.parameter_values["LFO_DEPTH_OCT"]
            current = int(self.panel_state.get("currentPad", self.panel_state["selectedPad"]))
            ping = self.panel_state["pads"][current - 1]["sound"] == "PING"
            title = "DEPTH · STORED (PING N/A)" if ping else "DEPTH"
            self._show_overlay(title, CONTROL_SPECS["LFO_DEPTH_OCT"].formatter(value))
            return

        items = self._menu_items()
        if self.menu_editing:
            self._edit_menu_item(
                items[self.menu_index][0],
                direction,
            )
        else:
            self.menu_index = (
                self.menu_index + direction
            ) % len(items)
        self._refresh_display()

    def _encoder_down(self, source: str) -> None:
        if self.display_mode != "home":
            self._menu_press()
            return
        was_up = not self._encoder_sources
        self._encoder_sources.add(source)
        if was_up:
            self._local_modifier_used = False
            self._send("ENCODER_SWITCH 1")

    def _encoder_up(self, source: str) -> None:
        if self.display_mode != "home":
            return
        if source not in self._encoder_sources:
            return
        self._encoder_sources.discard(source)
        if self._encoder_sources:
            return
        if self.transport.connected:
            self._send("ENCODER_SWITCH 0")
        elif not self._local_modifier_used:
            self._open_menu()

    def _open_menu(self) -> None:
        self.display_mode = "menu"
        self.menu_index = 0
        self.menu_editing = False
        self._refresh_display()

    def _menu_press(self) -> None:
        items = self._menu_items()
        key = items[self.menu_index][0]
        if key == "BACK":
            self.display_mode = "home"
            self.menu_editing = False
        elif key in ("ECHO_LOOP", "HOLD"):
            self._edit_menu_item(key, 1)
        else:
            self.menu_editing = not self.menu_editing
        self._refresh_display()

    def _menu_items(self) -> list[tuple[str, str, str]]:
        selected = int(self.panel_state["selectedPad"])
        pad = self.panel_state["pads"][selected - 1]
        items = [
            ("PAD", "CONFIGURE PAD", str(selected)),
            ("ASSIGNMENT", "ASSIGNMENT", pad["sound"]),
            ("VARIANT", "VARIANT", pad["variant"]),
        ]
        if pad["holdSupported"]:
            items.append(("HOLD", "HOLD", "ON" if pad["hold"] else "OFF"))
        items.append(("ECHO_LOOP", "ECHO LOOP", "ON" if self.echo_loop_var.get() else "OFF"))
        items.append(("BACK", "BACK", ""))
        return items

    def _edit_menu_item(
        self,
        key: str,
        direction: int,
    ) -> None:
        if key == "PAD":
            selected = int(self.panel_state["selectedPad"])
            selected = ((selected - 1 + direction) % 4) + 1
            self.panel_state["selectedPad"] = selected
            self._send(f"PAD_SELECT {selected}")
        elif key == "ASSIGNMENT":
            self._save_config_on_status = True
            self._send(f"PAD_ASSIGN {'NEXT' if direction > 0 else 'PREV'}")
        elif key == "VARIANT":
            self._save_config_on_status = True
            self._send(f"PAD_VARIANT {'NEXT' if direction > 0 else 'PREV'}")
        elif key == "ECHO_LOOP":
            self.echo_loop_var.set(not self.echo_loop_var.get())
            self._send_bool("ECHO_LOOP", self.echo_loop_var.get())
        elif key == "HOLD":
            self._send("PAD_HOLD_TOGGLE")

        self._refresh_display()

    def _step_parameter(
        self,
        name: str,
        direction: int,
    ) -> None:
        spec = CONTROL_SPECS[name]
        position = spec.to_normalized(
            self.parameter_values[name]
        )
        position = max(
            0.0,
            min(1.0, position + 0.025 * direction),
        )
        self.parameter_values[name] = spec.from_normalized(
            position
        )
        self._send_parameter(
            name,
            self.parameter_values[name],
        )

    def _apply_echo_filter(self) -> None:
        hpf, lpf = echo_filter_frequencies(self.echo_filter_macro)

        self.parameter_values["HPF_HZ"] = hpf
        self.parameter_values["LPF_HZ"] = lpf
        self._send_parameter("HPF_HZ", hpf)
        self._send_parameter("LPF_HZ", lpf)

    def _format_echo_filter(self) -> str:
        if abs(self.echo_filter_macro) < 0.001:
            return "FLAT"
        return (
            "LOW PASS"
            if self.echo_filter_macro < 0.0
            else "HIGH PASS"
        )

    def _pad_down(self, pad: int, source: str) -> None:
        action = f"PAD_{pad}"
        sources = self._input_sources.setdefault(action, set())
        if source in sources:
            return
        first = not sources
        sources.add(source)
        if self._encoder_sources:
            self._local_modifier_used = True
            if pad in (2, 3):
                self._save_config_on_status = True
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
        for number, (button, pad) in enumerate(
            zip(self.pad_buttons, self.panel_state["pads"]), start=1
        ):
            button.configure(text=f"BTN{number} · {pad['sound']}")
        self._update_control_activity()

    def _update_control_activity(self) -> None:
        if "LFO_RATE_HZ" not in self.dials:
            return
        current = int(self.panel_state.get("currentPad", self.panel_state["selectedPad"]))
        ping = self.panel_state["pads"][current - 1]["sound"] == "PING"
        self.dials["LFO_RATE_HZ"].set_active(not ping)
        if ping:
            self.dials["LFO_RATE_HZ"].value_label.configure(text="INACTIVE FOR PING")
        else:
            value = self.parameter_values["LFO_RATE_HZ"]
            self.dials["LFO_RATE_HZ"].value_label.configure(
                text=CONTROL_SPECS["LFO_RATE_HZ"].formatter(value)
            )

    def _show_overlay(
        self,
        title: str,
        value: str,
    ) -> None:
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

        if self.display_mode == "home":
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
                self.echo_loop_var.get(),
                self.transport.connected,
            )
        else:
            items = self._menu_items()
            self.menu_index = min(
                self.menu_index,
                len(items) - 1,
            )
            self.display.render_menu(
                [
                    (label, value)
                    for _key, label, value in items
                ],
                self.menu_index,
                self.menu_editing,
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

        self._send("SET PROFILE EXTENDED")
        live_parameters = (
            "TUNE_HZ", "LFO_RATE_HZ", "LFO_DEPTH_OCT", "DELAY_MS",
            "FEEDBACK", "ECHO_LEVEL", "MASTER",
        )
        for name in live_parameters:
            value = self.parameter_values[name]
            self._send_parameter(name, value)
        if self.echo_filter_active:
            self._apply_echo_filter()
        self._send_bool("ECHO_LOOP", self.echo_loop_var.get())
        self._send("PANEL_RESET")
        if not self._saved_config_applied:
            self._saved_config_applied = True
            for number, pad in enumerate(self._saved_pad_configs, start=1):
                sound = pad["sound"].replace(" ", "_")
                self._send(
                    f"PAD_CONFIG {number} {sound} {pad['variantIndex']} 0"
                )
        self._refresh_display()

    def _handle_status(self, status: object) -> None:
        if isinstance(status, dict):
            panel = status.get("panel")
            if (
                isinstance(panel, dict)
                and isinstance(panel.get("pads"), list)
                and len(panel["pads"]) == 4
            ):
                menu_revision = int(panel.get("menuRevision", 0))
                open_menu = (
                    self._panel_status_seen
                    and menu_revision != self._last_menu_revision
                )
                self._panel_status_seen = True
                self._last_menu_revision = menu_revision
                self.panel_state = panel
                self._update_pad_labels()
                if open_menu and self.display_mode == "home":
                    self._open_menu()
                if self._save_config_on_status:
                    self._save_config_on_status = False
                    try:
                        save_panel_config(self._panel_config_path, panel["pads"])
                    except OSError:
                        pass
        super()._handle_status(status)
        self._refresh_display()

    def connect(self) -> None:
        self._saved_config_applied = False
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
