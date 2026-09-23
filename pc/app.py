import math
import queue
import tkinter as tk
from tkinter import messagebox, ttk

from serial.tools import list_ports

from audio_player import AudioPlayer
from protocol import PROTOCOL_VERSION
from transport import SerialTransport


DEFAULTS = {
    "TUNE_HZ": 220.0,
    "LFO_RATE_HZ": 0.70,
    "LFO_DEPTH_OCT": 1.0,
    "DECAY_MS": 120.0,
    "DELAY_MS": 360.0,
    "FEEDBACK": 0.62,
    "ECHO_LEVEL": 0.45,
    "HPF_HZ": 60.0,
    "LPF_HZ": 7000.0,
    "MASTER": 0.50,
}


class SliderControl(ttk.Frame):
    def __init__(
        self,
        parent: tk.Misc,
        label: str,
        command_name: str,
        minimum: float,
        maximum: float,
        initial: float,
        formatter,
        callback,
        logarithmic: bool = False,
    ) -> None:
        super().__init__(parent)
        self.command_name = command_name
        self.minimum = minimum
        self.maximum = maximum
        self.formatter = formatter
        self.callback = callback
        self.logarithmic = logarithmic
        self.value = initial

        ttk.Label(self, text=label, width=12).grid(row=0, column=0, sticky="w")
        self.scale = tk.Scale(
            self,
            from_=0,
            to=1000,
            orient=tk.HORIZONTAL,
            showvalue=False,
            highlightthickness=0,
            command=self._changed,
        )
        self.scale.grid(row=0, column=1, sticky="ew")
        self.value_label = ttk.Label(self, width=12, anchor="e")
        self.value_label.grid(row=0, column=2, sticky="e")
        self.columnconfigure(1, weight=1)
        self.scale.set(self._to_position(initial))
        self._show_value(initial)
        self.scale.bind("<ButtonRelease-1>", self._released)

    def _to_position(self, value: float) -> float:
        if self.logarithmic:
            ratio = math.log(value / self.minimum) / math.log(
                self.maximum / self.minimum
            )
        else:
            ratio = (value - self.minimum) / (self.maximum - self.minimum)
        return ratio * 1000.0

    def _from_position(self, position: float) -> float:
        ratio = position / 1000.0
        if self.logarithmic:
            return self.minimum * (self.maximum / self.minimum) ** ratio
        return self.minimum + (self.maximum - self.minimum) * ratio

    def _changed(self, position: str) -> None:
        self.value = self._from_position(float(position))
        self._show_value(self.value)
        self.callback(self.command_name, self.value, False)

    def _released(self, _event) -> None:
        self.callback(self.command_name, self.value, True)

    def _show_value(self, value: float) -> None:
        self.value_label.configure(text=self.formatter(value))


class DubSirenApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("ESP32-S3 Dub Siren")
        self.root.minsize(760, 610)
        self.audio_queue: queue.Queue[bytes] = queue.Queue(maxsize=12)
        self.ui_events: queue.Queue[tuple[str, object]] = queue.Queue()
        self.transport = SerialTransport(
            self.audio_queue,
            lambda status: self.ui_events.put(("status", status)),
        )
        self.player = AudioPlayer(self.audio_queue)
        self.protocol_ok = False
        self.streaming = False
        self.pending_sends: dict[str, str] = {}
        self.sliders: dict[str, SliderControl] = {}
        self.momentary_active: set[str] = set()

        self.port_var = tk.StringVar()
        self.connection_var = tk.StringVar(value="Disconnected")
        self.mode_var = tk.StringVar(value="SINE1")
        self.lfo_shape_var = tk.StringVar(value="CLASSIC")
        self.hold_var = tk.BooleanVar(value=False)

        self._build_ui()
        self.refresh_ports()
        self.root.after(50, self._poll_ui_events)
        self.root.bind_all("<ButtonRelease-1>", self._global_release, add="+")
        self.root.protocol("WM_DELETE_WINDOW", self.close)

    def _build_ui(self) -> None:
        outer = ttk.Frame(self.root, padding=10)
        outer.pack(fill="both", expand=True)

        connection = ttk.LabelFrame(outer, text="CONNECTION", padding=8)
        connection.pack(fill="x", pady=(0, 10))
        ttk.Label(connection, text="COM").pack(side="left")
        self.port_combo = ttk.Combobox(
            connection, textvariable=self.port_var, state="readonly", width=18
        )
        self.port_combo.pack(side="left", padx=6)
        ttk.Button(connection, text="Refresh", command=self.refresh_ports).pack(
            side="left", padx=3
        )
        self.connect_button = ttk.Button(
            connection, text="Connect", command=self.toggle_connection
        )
        self.connect_button.pack(side="left", padx=3)
        self.audio_button = ttk.Button(
            connection,
            text="Start Audio",
            command=self.toggle_audio,
            state="disabled",
        )
        self.audio_button.pack(side="left", padx=3)
        ttk.Label(connection, textvariable=self.connection_var).pack(
            side="right", padx=6
        )

        columns = ttk.Frame(outer)
        columns.pack(fill="both", expand=True)
        siren = ttk.LabelFrame(columns, text="SIREN", padding=8)
        echo = ttk.LabelFrame(columns, text="ECHO", padding=8)
        siren.grid(row=0, column=0, sticky="nsew", padx=(0, 5))
        echo.grid(row=0, column=1, sticky="nsew", padx=(5, 0))
        columns.columnconfigure(0, weight=1)
        columns.columnconfigure(1, weight=1)

        self._combo_row(
            siren,
            "Mode",
            self.mode_var,
            ("SINE1", "SINE2", "TEST_TONE", "SQUARE"),
            self._mode_changed,
        )
        self._add_slider(
            siren, "Tune", "TUNE_HZ", 30, 9000, lambda v: f"{v:.0f} Hz", True
        )
        self._combo_row(
            siren,
            "LFO Shape",
            self.lfo_shape_var,
            (
                "CLASSIC",
                "TRIANGLE",
                "SQUARE",
                "SAW_UP",
                "SAW_DOWN",
                "ASYM_UP",
                "ASYM_DOWN",
                "PULSE_25",
                "PULSE_75",
                "MANUAL",
            ),
            self._lfo_shape_changed,
        )
        self._add_slider(
            siren,
            "Rate",
            "LFO_RATE_HZ",
            0.05,
            20,
            lambda v: f"{v:.2f} Hz",
            True,
        )
        self._add_slider(
            siren,
            "Depth",
            "LFO_DEPTH_OCT",
            0,
            2,
            lambda v: f"{v:.2f} oct",
        )
        self._add_slider(
            siren,
            "Decay",
            "DECAY_MS",
            0,
            3000,
            lambda v: f"{v:.0f} ms",
        )

        mod_row = ttk.Frame(siren)
        mod_row.pack(fill="x", pady=7)
        self.mod_down_button = ttk.Button(mod_row, text="MOD DOWN")
        self.mod_up_button = ttk.Button(mod_row, text="MOD UP")
        self.mod_down_button.pack(side="left", fill="x", expand=True, padx=(0, 3))
        self.mod_up_button.pack(side="left", fill="x", expand=True, padx=(3, 0))
        self._bind_momentary(self.mod_down_button, "MOD_DOWN")
        self._bind_momentary(self.mod_up_button, "MOD_UP")

        trigger_row = ttk.Frame(siren)
        trigger_row.pack(fill="x", pady=7)
        trigger = ttk.Button(trigger_row, text="TRIGGER")
        trigger.pack(side="left", fill="x", expand=True, padx=(0, 3))
        self._bind_momentary(trigger, "TRIGGER")
        ttk.Checkbutton(
            trigger_row,
            text="HOLD",
            variable=self.hold_var,
            command=lambda: self._send_bool("HOLD", self.hold_var.get()),
        ).pack(side="left", fill="x", expand=True, padx=(3, 0))

        self._add_slider(
            echo,
            "Time",
            "DELAY_MS",
            50,
            1000,
            lambda v: f"{v:.0f} ms",
        )
        self._add_slider(
            echo,
            "Feedback",
            "FEEDBACK",
            0,
            1.05,
            lambda v: f"{v * 100:.0f} %",
        )
        self._add_slider(
            echo,
            "Echo Level",
            "ECHO_LEVEL",
            0,
            1,
            lambda v: f"{v * 100:.0f} %",
        )
        self._add_slider(
            echo,
            "High Pass",
            "HPF_HZ",
            50,
            7000,
            lambda v: f"{v:.0f} Hz",
            True,
        )
        self._add_slider(
            echo,
            "Low Pass",
            "LPF_HZ",
            200,
            19000,
            lambda v: f"{v:.0f} Hz",
            True,
        )
        echo_cut = ttk.Button(echo, text="ECHO CUT")
        echo_cut.pack(fill="x", pady=12)
        self._bind_momentary(echo_cut, "ECHO_CUT")

        master = ttk.LabelFrame(outer, text="MASTER", padding=8)
        master.pack(fill="x", pady=(10, 0))
        self._add_slider(
            master,
            "Volume",
            "MASTER",
            0,
            1,
            lambda v: f"{v * 100:.0f} %",
        )
        self._update_manual_buttons()

    def _combo_row(self, parent, label, variable, values, callback) -> None:
        row = ttk.Frame(parent)
        row.pack(fill="x", pady=3)
        ttk.Label(row, text=label, width=12).pack(side="left")
        combo = ttk.Combobox(
            row, textvariable=variable, values=values, state="readonly"
        )
        combo.pack(side="left", fill="x", expand=True)
        combo.bind("<<ComboboxSelected>>", callback)

    def _add_slider(
        self,
        parent,
        label,
        command_name,
        minimum,
        maximum,
        formatter,
        logarithmic=False,
    ) -> None:
        slider = SliderControl(
            parent,
            label,
            command_name,
            minimum,
            maximum,
            DEFAULTS[command_name],
            formatter,
            self._slider_changed,
            logarithmic,
        )
        slider.pack(fill="x", pady=2)
        self.sliders[command_name] = slider

    def _bind_momentary(self, button: ttk.Button, command: str) -> None:
        button.bind("<ButtonPress-1>", lambda _event: self._press(command))

    def _press(self, command: str) -> None:
        self.momentary_active.add(command)
        self._send_bool(command, True)

    def _global_release(self, _event=None) -> None:
        for command in tuple(self.momentary_active):
            self._send_bool(command, False)
        self.momentary_active.clear()

    def _mode_changed(self, _event=None) -> None:
        self._send(f"SET MODE {self.mode_var.get()}")

    def _lfo_shape_changed(self, _event=None) -> None:
        if self.lfo_shape_var.get() != "MANUAL":
            self._global_release()
        self._update_manual_buttons()
        self._send(f"SET LFO_SHAPE {self.lfo_shape_var.get()}")

    def _update_manual_buttons(self) -> None:
        state = "normal" if self.lfo_shape_var.get() == "MANUAL" else "disabled"
        self.mod_down_button.configure(state=state)
        self.mod_up_button.configure(state=state)

    def _slider_changed(self, name: str, value: float, final: bool) -> None:
        pending = self.pending_sends.pop(name, None)
        if pending is not None:
            self.root.after_cancel(pending)
        if final:
            self._send_parameter(name, value)
        else:
            token = self.root.after(
                25, lambda n=name: self._flush_slider(n)
            )
            self.pending_sends[name] = token

    def _flush_slider(self, name: str) -> None:
        self.pending_sends.pop(name, None)
        self._send_parameter(name, self.sliders[name].value)

    def _send_parameter(self, name: str, value: float) -> None:
        self._send(f"SET {name} {value:.6g}")

    def _send_bool(self, name: str, value: bool) -> None:
        self._send(f"{name} {1 if value else 0}")

    def _send(self, command: str) -> None:
        if not self.transport.connected:
            return
        try:
            self.transport.send(command)
        except Exception as exc:
            self.connection_var.set(f"Serial error: {exc}")

    def refresh_ports(self) -> None:
        ports = [port.device for port in list_ports.comports()]
        self.port_combo["values"] = ports
        if self.port_var.get() not in ports:
            self.port_var.set(ports[0] if ports else "")

    def toggle_connection(self) -> None:
        if self.transport.connected:
            self.disconnect()
        else:
            self.connect()

    def connect(self) -> None:
        port = self.port_var.get()
        if not port:
            messagebox.showerror("Connection", "No serial port is available.")
            return
        try:
            self.transport.connect(port)
        except Exception as exc:
            messagebox.showerror("Connection", str(exc))
            return
        self.protocol_ok = False
        self.connection_var.set("Waiting for STATUS...")
        self.connect_button.configure(text="Disconnect")
        self.port_combo.configure(state="disabled")

    def disconnect(self) -> None:
        self.stop_audio()
        self._global_release()
        self.transport.disconnect()
        self.protocol_ok = False
        self.connection_var.set("Disconnected")
        self.connect_button.configure(text="Connect")
        self.audio_button.configure(text="Start Audio", state="disabled")
        self.port_combo.configure(state="readonly")
        self.refresh_ports()

    def toggle_audio(self) -> None:
        if self.streaming:
            self.stop_audio()
        else:
            self.start_audio()

    def start_audio(self) -> None:
        if not self.protocol_ok or not self.transport.connected:
            return
        self._drain_audio_queue()
        self.player.start()
        self._send("STREAM 1")
        self.streaming = True
        self.audio_button.configure(text="Stop Audio")
        self.root.after(100, self._check_player)

    def stop_audio(self) -> None:
        if self.transport.connected:
            self._send("STREAM 0")
        self.player.stop()
        self.streaming = False
        self.audio_button.configure(text="Start Audio")
        self._drain_audio_queue()

    def _check_player(self) -> None:
        if self.streaming and self.player.error is not None:
            error = self.player.error
            self.stop_audio()
            messagebox.showerror("Audio output", str(error))
        elif self.streaming:
            self.root.after(100, self._check_player)

    def _drain_audio_queue(self) -> None:
        while True:
            try:
                self.audio_queue.get_nowait()
            except queue.Empty:
                break

    def _poll_ui_events(self) -> None:
        try:
            while True:
                event, payload = self.ui_events.get_nowait()
                if event == "status":
                    self._handle_status(payload)
        except queue.Empty:
            pass
        self.root.after(50, self._poll_ui_events)

    def _handle_status(self, status: object) -> None:
        if not isinstance(status, dict) or status.get("protocol") != PROTOCOL_VERSION:
            self.connection_var.set("Protocol mismatch")
            self.root.after(0, self.disconnect)
            return
        self.protocol_ok = True
        self.connection_var.set(
            f"Connected · firmware {status.get('firmware', '?')}"
        )
        self.audio_button.configure(state="normal")
        self._synchronize_controls()

    def _synchronize_controls(self) -> None:
        self._send(f"SET MODE {self.mode_var.get()}")
        self._send(f"SET LFO_SHAPE {self.lfo_shape_var.get()}")
        for name, slider in self.sliders.items():
            self._send_parameter(name, slider.value)
        self._send_bool("TRIGGER", False)
        self._send_bool("HOLD", self.hold_var.get())
        self._send_bool("MOD_UP", False)
        self._send_bool("MOD_DOWN", False)
        self._send_bool("ECHO_CUT", False)

    def close(self) -> None:
        if self.transport.connected:
            self.disconnect()
        else:
            self.player.stop()
        self.root.destroy()


def main() -> None:
    root = tk.Tk()
    DubSirenApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
