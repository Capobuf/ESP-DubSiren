"""Text display for the desktop panel, driven by confirmed controller state."""

import tkinter as tk
from tkinter import ttk


DISPLAY_WIDTH = 320
DISPLAY_HEIGHT = 170
BLACK = "#050806"
TEXT = "#e5e8d8"
ACCENT = "#c9ff37"
MUTED = "#748077"
AMBER = "#ffb12b"


class PanelDisplay(ttk.Frame):
    def __init__(self, parent: tk.Misc) -> None:
        super().__init__(parent)
        self.canvas = tk.Canvas(
            self, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT,
            bg=BLACK, highlightthickness=0,
        )
        self.canvas.pack()

    def _text(self, x, y, text, size=10, color=TEXT, anchor="nw"):
        self.canvas.create_text(
            x, y, text=str(text), anchor=anchor,
            font=("Consolas", -size), fill=color,
        )

    def render_home(
        self, selected_pad: int, current_pad: int, sound: str, variant: str,
        depth: str, hold: bool, echo_loop: bool, connected: bool,
        parameters: dict[str, object] | None = None,
        echo_tone: str = "CUSTOM",
    ) -> None:
        self.canvas.delete("all")
        params = parameters or {}
        if params.get("PROFILE") == "CLASSIC":
            self._text(16, 10, "DS01E / CLASSIC", 11, MUTED)
            self._text(16, 31, sound, 24, ACCENT)
            self._text(16, 73, "PITCH  " + str(params.get("PITCH", "MID")), 17)
            self._text(16, 99, "RATE   " + str(params.get("RATE", "MID")), 17)
            status = ["HOLD"] if hold else []
            status.append("ECHO CUT" if params.get("ECHO_CUT") else
                          "ECHO" if float(params.get("ECHO_LEVEL", 0)) > 0 else "DRY")
        else:
            self._text(16, 10, f"P{selected_pad} {sound}", 18, ACCENT)
            self._text(16, 32, variant)
            self._text(16, 51, f"TUNE {params.get('TUNE_HZ', 220):.0f} Hz  "
                       f"RATE {params.get('LFO_RATE_HZ', 2):.1f} Hz")
            self._text(16, 69, f"DEPTH {depth}  "
                       f"MASTER {params.get('MASTER', 0.5):.0%}")
            self._text(16, 87, f"ECHO {params.get('DELAY_MS', 300):.0f}ms  "
                       f"FB {params.get('FEEDBACK', 0):.0%}  "
                       f"LV {params.get('ECHO_LEVEL', 0):.0%}")
            self._text(16, 111, f"FILTER {echo_tone}")
            status = [f"PLAY P{current_pad}"]
            if hold:
                status.append("HOLD")
            if echo_loop:
                status.append("LOOP")
            if connected:
                status.append("LINK")
        self._text(16, 149, " / ".join(status), 11, AMBER)

    def render_overlay(self, title: str, value: str) -> None:
        self.canvas.delete("all")
        self._text(16, 40, title, 14, MUTED)
        self._text(16, 80, value, 24, ACCENT)

    def render_menu(self, items: list[tuple[str, str]], index: int, editing: bool) -> None:
        self.canvas.delete("all")
        self._text(16, 12, "EDIT" if editing else "SELECT", 18, ACCENT)
        start = max(0, min(index - 1, len(items) - 3))
        for row, (label, value) in enumerate(items[start:start + 3]):
            active = start + row == index
            y = 48 + row * 28
            self._text(16, y, ("> " if active else "  ") + label,
                       10, TEXT if active else MUTED)
            self._text(304, y, value, 10, AMBER if active else MUTED, "ne")
        self._text(16, 149, "TURN CHANGE / PUSH SAVE" if editing else
                   "TURN SELECT / PUSH OPEN")
