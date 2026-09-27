import math
import tkinter as tk
from tkinter import ttk

from app import DubSirenApp, OUTPUT_VALUES
from control_ranges import CONTROL_SPECS, ControlSpec

VOICE_VALUES = (
    "SINE1",
    "SINE2",
    "TEST_TONE",
    "SQUARE",
    "LICKSHOT",
    "SONAR",
    "SYNDRUM",
    "GUNSHOT",
)
VOICE_LABELS = {
    "SINE1": "SINE 1",
    "SINE2": "SINE 2",
    "TEST_TONE": "TEST TONE",
    "SQUARE": "SQUARE",
    "LICKSHOT": "LICKSHOT",
    "SONAR": "SONAR",
    "SYNDRUM": "SYNDRUM",
    "GUNSHOT": "GUNSHOT",
}
TRANSIENT_MODES = ("SYNDRUM", "GUNSHOT")
LFO_SHAPES = (
    "CLASSIC",
    "TRIANGLE",
    "SQUARE",
    "SAW_DOWN",
    "SAW_UP",
    "ASYM_DOWN",
    "ASYM_UP",
    "PULSE_25",
    "PULSE_75",
    "MANUAL",
)
LFO_LABELS = {value: value.replace("_", " ") for value in LFO_SHAPES}
SYNDRUM_WAVES = ("SINE", "TRIANGLE", "SQUARE")

def format_echo_filter(value: float) -> str:
    if abs(value) < 0.05:
        return "FLAT"
    amount = min(100, round(abs(value) * 100))
    return f"{'LP' if value < 0 else 'HP'} {amount}%"


ECHO_FILTER_SPEC = ControlSpec(-1.0, 1.0, 0.0, format_echo_filter)

PANEL_KNOBS = (
    ("TUNE", "TUNE_HZ"),
    ("RATE", "LFO_RATE_HZ"),
    ("ECHO", "ECHO_LEVEL"),
    ("DELAY", "DELAY_MS"),
    ("FEEDBACK", "FEEDBACK"),
    ("FILTER", "ECHO_FILTER"),
)


class DialControl(ttk.Frame):
    def __init__(
        self,
        parent: tk.Misc,
        label: str,
        command_name: str,
        spec: ControlSpec,
        callback,
        diameter: int = 112,
    ) -> None:
        super().__init__(parent)
        self.command_name = command_name
        self.spec = spec
        self.callback = callback
        self.value = spec.default
        self._diameter = diameter
        self._drag_y: int | None = None
        self._drag_position = spec.to_normalized(spec.default)

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
        self._drag_y = event.y_root
        self._drag_position = self.spec.to_normalized(self.value)

    def _dragged(self, event) -> None:
        if self._drag_y is None:
            return
        # Vertical dragging is predictable for a virtual rotary control and
        # does not require the pointer to trace a circular path.
        delta = (self._drag_y - event.y_root) / 170.0
        position = max(0.0, min(1.0, self._drag_position + delta))
        self.set_value(self.spec.from_normalized(position), notify=True, final=False)

    def _released(self, _event) -> None:
        self._drag_y = None
        self.callback(self.command_name, self.value, True)

    def _wheel(self, event) -> None:
        self._step(0.02 if event.delta > 0 else -0.02, True)

    def _step(self, delta: float, final: bool) -> None:
        position = self.spec.to_normalized(self.value)
        position = max(0.0, min(1.0, position + delta))
        self.set_value(self.spec.from_normalized(position), notify=True, final=final)

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
        self.canvas.create_line(cx, cy, x2, y2, fill="#e7e9ec", width=4)
        self.canvas.create_oval(
            cx - 4,
            cy - 4,
            cx + 4,
            cy + 4,
            fill="#c7ccd2",
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
        voice: str,
        shape: str,
        hold: bool,
        modulation: bool,
        echo_loop: bool,
        connected: bool,
    ) -> None:
        self.canvas.delete("all")
        self._text(18, 18, "ESP DUB SIREN", 12, "#6d9079", "nw")
        self._text(170, 63, VOICE_LABELS[voice], 28, "#d9f5df", "center")
        self._text(170, 102, LFO_LABELS[shape], 13, "#9cc9a6", "center")
        flags = []
        if hold:
            flags.append("HOLD")
        if not modulation:
            flags.append("MOD OFF")
        if echo_loop:
            flags.append("LOOP")
        if connected:
            flags.append("LINK")
        self._text(170, 143, "  ·  ".join(flags) or "READY", 11, "#6d9079", "center")
        self._text(170, 166, "ENCODER: rotate voice · press menu", 9, "#53695a", "center")

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
    def __init__(self, parent: tk.Misc, rotate_callback, press_callback) -> None:
        super().__init__(parent)
        self.rotate_callback = rotate_callback
        self.press_callback = press_callback

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
        self.canvas.bind("<Button-1>", lambda _event: press_callback())
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
            command=press_callback,
        ).pack(side="left", expand=True, fill="x", padx=3)
        ttk.Button(
            emulator,
            text="+",
            width=3,
            command=lambda: rotate_callback(1),
        ).pack(side="left")

    def _wheel(self, event) -> None:
        self.rotate_callback(-1 if event.delta > 0 else 1)


class PhysicalDubSirenApp(DubSirenApp):
    def __init__(self, root: tk.Tk) -> None:
        self.parameter_values = {
            name: spec.default for name, spec in CONTROL_SPECS.items()
        }
        self.parameter_values["HPF_HZ"] = 60.0
        self.parameter_values["LPF_HZ"] = 16000.0
        self.echo_filter_macro = 0.0
        self.display_mode = "home"
        self.menu_index = 0
        self.menu_editing = False
        self.overlay_token: str | None = None
        self.dials: dict[str, DialControl] = {}
        super().__init__(root)
        self.root.title("ESP Dub Siren")
        self.root.minsize(980, 700)
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
            sticky="nsew",
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
            self._encoder_press,
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

        self.trigger_button = ttk.Button(buttons, text="TRIGGER")
        self.mod_down_button = ttk.Button(buttons, text="DIVE")
        self.mod_up_button = ttk.Button(buttons, text="RISE")
        self.echo_cut_button = ttk.Button(buttons, text="ECHO CUT")
        for index, button in enumerate(
            (
                self.trigger_button,
                self.mod_down_button,
                self.mod_up_button,
                self.echo_cut_button,
            )
        ):
            button.grid(
                row=0,
                column=index,
                sticky="ew",
                padx=6,
                ipady=12,
            )

        self._bind_momentary(self.trigger_button, "TRIGGER")
        self._bind_momentary(self.mod_down_button, "MOD_DOWN")
        self._bind_momentary(self.mod_up_button, "MOD_UP")
        self._bind_momentary(self.echo_cut_button, "ECHO_CUT")

        rear = ttk.LabelFrame(outer, text="REAR / MASTER", padding=8)
        rear.pack(fill="x", pady=(10, 0))
        self.master_dial = DialControl(
            rear,
            "MASTER",
            "MASTER",
            CONTROL_SPECS["MASTER"],
            self._panel_parameter_changed,
            diameter=82,
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
        self._update_button_labels()

    def _panel_parameter_changed(
        self,
        name: str,
        value: float,
        final: bool,
    ) -> None:
        if name == "ECHO_FILTER":
            self.echo_filter_macro = value
            self._show_overlay("FILTER", format_echo_filter(value))
        else:
            self.parameter_values[name] = value
            self._show_overlay(
                self._parameter_label(name),
                CONTROL_SPECS[name].formatter(value),
            )

        pending = self.pending_sends.pop(name, None)
        if pending is not None:
            self.root.after_cancel(pending)

        if final:
            if name == "ECHO_FILTER":
                self._apply_echo_filter()
            else:
                self._send_parameter(name, value)
        else:
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
            "DELAY_MS": "DELAY",
            "FEEDBACK": "FEEDBACK",
            "ECHO_LEVEL": "ECHO",
            "ECHO_FILTER": "FILTER",
            "MASTER": "MASTER",
            "DECAY_MS": "DECAY",
            "SWEEP": "SWEEP",
            "SWEEP_MS": "SWEEP TIME",
            "SYNDRUM_SWEEP": "DRUM SWEEP",
        }.get(name, name)

    def _encoder_rotate(self, direction: int) -> None:
        if self.display_mode == "home":
            current = VOICE_VALUES.index(self.mode_var.get())
            self._set_mode(
                VOICE_VALUES[
                    (current + direction) % len(VOICE_VALUES)
                ]
            )
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

    def _encoder_press(self) -> None:
        if self.display_mode == "home":
            self.display_mode = "menu"
            self.menu_index = 0
            self.menu_editing = False
            self._refresh_display()
            return

        items = self._menu_items()
        key = items[self.menu_index][0]
        if key == "BACK":
            self.display_mode = "home"
            self.menu_editing = False
        elif key in (
            "MODULATION",
            "RETRIGGER",
            "ECHO_LOOP",
            "HOLD",
        ):
            self._edit_menu_item(key, 1)
        else:
            self.menu_editing = not self.menu_editing
        self._refresh_display()

    def _menu_items(self) -> list[tuple[str, str, str]]:
        items = [
            (
                "VOICE",
                "VOICE",
                VOICE_LABELS[self.mode_var.get()],
            ),
            (
                "LFO_SHAPE",
                "LFO SHAPE",
                LFO_LABELS[self.lfo_shape_var.get()],
            ),
            (
                "LFO_DEPTH_OCT",
                "DEPTH",
                CONTROL_SPECS["LFO_DEPTH_OCT"].formatter(
                    self.parameter_values["LFO_DEPTH_OCT"]
                ),
            ),
            (
                "DECAY_MS",
                "DECAY",
                CONTROL_SPECS["DECAY_MS"].formatter(
                    self.parameter_values["DECAY_MS"]
                ),
            ),
            (
                "SWEEP",
                "SWEEP",
                CONTROL_SPECS["SWEEP"].formatter(
                    self.parameter_values["SWEEP"]
                ),
            ),
            (
                "SWEEP_MS",
                "SWEEP TIME",
                CONTROL_SPECS["SWEEP_MS"].formatter(
                    self.parameter_values["SWEEP_MS"]
                ),
            ),
            (
                "MODULATION",
                "MODULATION",
                "ON" if self.modulation_var.get() else "OFF",
            ),
            (
                "RETRIGGER",
                "RETRIGGER",
                "ON" if self.retrigger_var.get() else "OFF",
            ),
            (
                "ECHO_LOOP",
                "ECHO LOOP",
                "ON" if self.echo_loop_var.get() else "OFF",
            ),
            (
                "HOLD",
                "HOLD",
                "ON" if self.hold_var.get() else "OFF",
            ),
        ]

        if self.mode_var.get() == "SYNDRUM":
            items.insert(
                2,
                (
                    "SYNDRUM_WAVE",
                    "DRUM WAVE",
                    self.syndrum_wave_var.get(),
                ),
            )
            items.insert(
                3,
                (
                    "SYNDRUM_SWEEP",
                    "DRUM SWEEP",
                    CONTROL_SPECS["SYNDRUM_SWEEP"].formatter(
                        self.parameter_values["SYNDRUM_SWEEP"]
                    ),
                ),
            )

        items.append(("BACK", "BACK", ""))
        return items

    def _edit_menu_item(
        self,
        key: str,
        direction: int,
    ) -> None:
        if key == "VOICE":
            current = VOICE_VALUES.index(self.mode_var.get())
            self._set_mode(
                VOICE_VALUES[
                    (current + direction) % len(VOICE_VALUES)
                ]
            )
        elif key == "LFO_SHAPE":
            current = LFO_SHAPES.index(self.lfo_shape_var.get())
            self.lfo_shape_var.set(
                LFO_SHAPES[
                    (current + direction) % len(LFO_SHAPES)
                ]
            )
            self._send(
                f"SET LFO_SHAPE {self.lfo_shape_var.get()}"
            )
            self._update_button_labels()
        elif key == "SYNDRUM_WAVE":
            current = SYNDRUM_WAVES.index(
                self.syndrum_wave_var.get()
            )
            self.syndrum_wave_var.set(
                SYNDRUM_WAVES[
                    (current + direction) % len(SYNDRUM_WAVES)
                ]
            )
            self._send(
                f"SET SYNDRUM_WAVE {self.syndrum_wave_var.get()}"
            )
        elif key in (
            "LFO_DEPTH_OCT",
            "DECAY_MS",
            "SWEEP",
            "SWEEP_MS",
            "SYNDRUM_SWEEP",
        ):
            self._step_parameter(key, direction)
        elif key == "MODULATION":
            self.modulation_var.set(
                not self.modulation_var.get()
            )
            self._send_bool(
                "MODULATION",
                self.modulation_var.get(),
            )
        elif key == "RETRIGGER":
            self.retrigger_var.set(not self.retrigger_var.get())
            self._send_bool(
                "RETRIGGER",
                self.retrigger_var.get(),
            )
        elif key == "ECHO_LOOP":
            self.echo_loop_var.set(not self.echo_loop_var.get())
            self._send_bool(
                "ECHO_LOOP",
                self.echo_loop_var.get(),
            )
        elif (
            key == "HOLD"
            and self.mode_var.get() not in TRANSIENT_MODES
        ):
            self.hold_var.set(not self.hold_var.get())
            self._send_bool("HOLD", self.hold_var.get())

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
        macro = self.echo_filter_macro
        if abs(macro) < 0.001:
            hpf, lpf = 60.0, 16000.0
        elif macro < 0.0:
            hpf = 60.0
            lpf = 16000.0 * (
                (400.0 / 16000.0) ** -macro
            )
        else:
            hpf = 50.0 * (
                (4000.0 / 50.0) ** macro
            )
            lpf = 16000.0

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

    def _set_mode(self, mode: str) -> None:
        self.mode_var.set(mode)
        if (
            mode in TRANSIENT_MODES
            and self.hold_var.get()
        ):
            self.hold_var.set(False)
            self._send_bool("HOLD", False)
        self._send(f"SET MODE {mode}")
        self._update_button_labels()
        self._refresh_display()

    def _update_button_labels(self) -> None:
        manual = self.lfo_shape_var.get() == "MANUAL"
        transient = self.mode_var.get() in TRANSIENT_MODES
        self.mod_down_button.configure(
            text="MOD DOWN" if manual else "DIVE",
            state="disabled" if transient else "normal",
        )
        self.mod_up_button.configure(
            text="MOD UP" if manual else "RISE",
            state="disabled" if transient else "normal",
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
            self.display.render_home(
                self.mode_var.get(),
                self.lfo_shape_var.get(),
                self.hold_var.get(),
                self.modulation_var.get(),
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

        # EXTENDED is now an internal engine profile. The user-facing layer is
        # the physical panel, so the old profile switch is intentionally hidden.
        self._send("SET PROFILE EXTENDED")
        self._send(f"SET MODE {self.mode_var.get()}")
        self._send(
            f"SET LFO_SHAPE {self.lfo_shape_var.get()}"
        )
        self._send(
            f"SET SYNDRUM_WAVE {self.syndrum_wave_var.get()}"
        )
        for name, value in self.parameter_values.items():
            self._send_parameter(name, value)

        self._send_bool(
            "MODULATION",
            self.modulation_var.get(),
        )
        self._send_bool(
            "RETRIGGER",
            self.retrigger_var.get(),
        )
        self._send_bool(
            "ECHO_LOOP",
            self.echo_loop_var.get(),
        )
        self._send_bool("ECHO_SPIN", False)
        self._send_bool("TRIGGER", False)
        self._send_bool("HOLD", self.hold_var.get())
        self._send_bool("MOD_UP", False)
        self._send_bool("MOD_DOWN", False)
        self._send_bool("ECHO_CUT", False)
        self._refresh_display()

    def connect(self) -> None:
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
