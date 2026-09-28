"""Native 320x170 diegetic display renderer for the desktop Virtual ESP."""

from __future__ import annotations

import math
import time
import tkinter as tk
from collections.abc import Callable
from tkinter import ttk


DISPLAY_WIDTH = 320
DISPLAY_HEIGHT = 170
BOOT_DURATION_S = 3.3

NIGHT = "#08130f"
NIGHT_HIGH = "#13251c"
GROUND = "#1b241d"
BLACK = "#050706"
CABINET = "#303d31"
CABINET_EDGE = "#665f43"
STEEL = "#4b5047"
STEEL_LIGHT = "#8d8b74"
CREAM = "#eadfb8"
AMBER = "#ffad22"
AMBER_DIM = "#805015"
RED = "#d74331"
GREEN = "#4c8358"
GREEN_BRIGHT = "#9bd66f"
SKIN = "#c98c66"
SHIRT = "#b83d30"
DENIM = "#31536c"
CABLE = "#cf5137"


class PanelDisplay(ttk.Frame):
    """Pixel-art scene whose physical components are the parameter UI."""

    def __init__(
        self,
        parent: tk.Misc,
        peak_provider: Callable[[], float] | None = None,
    ) -> None:
        super().__init__(parent)
        self.canvas = tk.Canvas(
            self,
            width=DISPLAY_WIDTH,
            height=DISPLAY_HEIGHT,
            bg=NIGHT,
            highlightbackground="#4c5550",
            highlightthickness=2,
        )
        self.canvas.pack()
        self._peak_provider = peak_provider
        self._boot_started = time.monotonic()
        self._screen = "home"
        self._focus: str | None = None
        self._focus_value = ""
        self._focus_until = 0.0
        self._home = {
            "selected_pad": 1,
            "current_pad": 1,
            "active_pad": 0,
            "sound": "CLASSIC",
            "variant": "SINE 1",
            "hold": False,
            "echo_loop": False,
            "connected": False,
            "parameters": {},
            "echo_tone": "CUSTOM",
        }
        self._menu: tuple[list[tuple[str, str]], int, bool] = ([], 0, False)
        self.after(100, self._animate)

    def replay_boot(self) -> None:
        self._focus = None
        self._boot_started = time.monotonic()
        self._draw()

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
        parameters: dict[str, float] | None = None,
        active_pad: int = 0,
        echo_tone: str = "CUSTOM",
    ) -> None:
        self._screen = "home"
        self._home = {
            "selected_pad": selected_pad,
            "current_pad": current_pad,
            "active_pad": active_pad,
            "sound": sound,
            "variant": variant,
            "depth": depth,
            "hold": hold,
            "echo_loop": echo_loop,
            "connected": connected,
            "parameters": dict(parameters or {}),
            "echo_tone": echo_tone,
        }
        self._draw()

    def render_overlay(self, title: str, value: str) -> None:
        mapping = {
            "TUNE": "tune",
            "RATE": "rate",
            "DEPTH": "depth",
            "ECHO LEVEL": "echo_level",
            "ECHO TIME": "echo_time",
            "ECHO FEEDBACK": "echo_feedback",
            "ECHO TONE": "echo_tone",
            "MASTER": "master",
        }
        clean_title = title.split(" ·", 1)[0]
        self._focus = mapping.get(clean_title)
        self._focus_value = value
        self._focus_until = time.monotonic() + 1.1
        self._screen = "home"
        self._draw()

    def render_menu(
        self,
        items: list[tuple[str, str]],
        index: int,
        editing: bool,
    ) -> None:
        self._screen = "menu"
        self._menu = (items, index, editing)
        self._draw_menu(items, index, editing)

    def _animate(self) -> None:
        self._draw()
        self.after(100, self._animate)

    def _draw(self) -> None:
        now = time.monotonic()
        if now - self._boot_started < BOOT_DURATION_S:
            self._draw_boot(now - self._boot_started)
            return
        if self._screen == "menu":
            self._draw_menu(*self._menu)
            return
        if self._focus_until <= now:
            self._focus = None
            self._focus_value = ""
        self._draw_scene(now)

    def _draw_boot(self, elapsed: float) -> None:
        self._backdrop()
        stage = (
            0 if elapsed < 0.65 else
            1 if elapsed < 1.25 else
            2 if elapsed < 1.85 else
            3 if elapsed < 2.45 else
            4 if elapsed < 2.85 else 5
        )
        if stage >= 1:
            self._stack(elapsed, 0.0, False, stage)
        if stage >= 3:
            self._rack(elapsed, 0.0, False, stage)
        if stage >= 4:
            self._cable(stage >= 5)
        boy_x = int(306 - elapsed * 77) if stage == 0 else (
            171 if stage == 1 else 228 if stage == 2 else 274
        )
        self._soundboy(elapsed, boy_x, stage == 0, stage >= 5, stage >= 3)
        if stage == 0:
            self._cabinet(boy_x - 42, 100, 39, 54, True, 0)
        if stage >= 5:
            self._text(
                149, 148, "POWER UP", 7,
                AMBER if int(elapsed / 0.12) & 1 else GREEN_BRIGHT,
                "nw",
            )

    def _draw_scene(self, now: float) -> None:
        peak = 0.0
        if self._peak_provider is not None:
            try:
                peak = max(0.0, min(1.0, float(self._peak_provider())))
            except (TypeError, ValueError, RuntimeError):
                peak = 0.0
        self._backdrop()
        self._stack(now, peak, True, 5)
        self._cable(True)
        self._rack(now, peak, True, 5)
        self._soundboy(
            now, 276, False, True,
            self._focus is not None or bool(self._home.get("active_pad")),
        )

    def _backdrop(self) -> None:
        c = self.canvas
        c.delete("all")
        c.create_rectangle(0, 0, 320, 170, fill=NIGHT, outline="")
        c.create_rectangle(0, 139, 320, 170, fill=GROUND, outline="")
        c.create_line(0, 138, 320, 138, fill=CABINET_EDGE)
        for x in range(4, 320, 24):
            y = 15 + (x // 24 % 2) * 7
            c.create_rectangle(x, y, x + 1, y + 1, fill=NIGHT_HIGH, outline="")
        c.create_rectangle(5, 157, 315, 159, fill=BLACK, outline="")

    def _stack(
        self, now: float, peak: float, powered: bool, build_stage: int
    ) -> None:
        if build_stage < 1:
            return
        cone = round(peak * 5) if powered else 0
        self._cabinet(8, 95, 126, 61, True, cone)
        if build_stage < 2:
            return
        self._cabinet(14, 58, 114, 38, False, max(0, cone - 1))
        c = self.canvas
        c.create_rectangle(21, 31, 121, 59, fill=CABINET, outline=CABINET_EDGE)
        c.create_polygon(31, 38, 31, 51, 70, 45, fill=STEEL, outline="")
        c.create_polygon(111, 38, 111, 51, 72, 45, fill=STEEL, outline="")
        c.create_rectangle(68, 40, 75, 50, fill=BLACK, outline="")
        c.create_line(25, 55, 117, 55, fill=BLACK)
        pulse = powered and int(now / 0.08) % 8 < 2
        c.create_oval(16, 34, 20, 38, fill=AMBER if pulse else AMBER_DIM, outline="")

    def _cabinet(
        self,
        x: int,
        y: int,
        width: int,
        height: int,
        sub: bool,
        cone: int,
    ) -> None:
        c = self.canvas
        c.create_rectangle(x, y, x + width, y + height, fill=BLACK, outline=CABINET_EDGE)
        c.create_rectangle(
            x + 3, y + 3, x + width - 3, y + height - 3,
            fill=BLACK, outline=CABINET,
        )
        count = 2 if sub else 3
        radius = min(height // 2 - 7, 25) if sub else 12
        cy = y + height // 2
        for side in range(count):
            cx = x + width // 4 + side * width // 2 if sub else x + 21 + side * 36
            c.create_oval(
                cx - radius, cy - radius, cx + radius, cy + radius,
                fill=STEEL, outline=STEEL_LIGHT,
            )
            inner = max(4, radius // 2 + cone) if sub else 4 + cone // 2
            c.create_oval(cx - inner, cy - inner, cx + inner, cy + inner, fill=BLACK, outline="")
            c.create_oval(cx - 5, cy - 5, cx - 1, cy - 1, fill=CABINET_EDGE, outline="")
        c.create_rectangle(x + 5, y + height - 4, x + 10, y + height - 2, fill=AMBER_DIM, outline="")

    def _rack(self, now: float, peak: float, powered: bool, build_stage: int) -> None:
        if build_stage < 3:
            return
        c = self.canvas
        x = 143
        c.create_rectangle(x, 28, x + 122, 156, fill=BLACK, outline=STEEL_LIGHT)
        c.create_rectangle(x + 4, 32, x + 118, 52, fill=STEEL, outline="")
        c.create_rectangle(x + 8, 35, x + 96, 49, fill=NIGHT_HIGH, outline="")
        sound = str(self._home.get("sound", "CLASSIC"))
        self._text(x + 11, 35, sound[:12], 6, GREEN_BRIGHT if powered else STEEL_LIGHT, "nw")
        self._text(x + 11, 43, str(self._home.get("variant", ""))[:12], 5, GREEN, "nw")
        c.create_rectangle(x + 100, 36, x + 114, 48, fill=BLACK, outline="")
        self._text(x + 103, 38, f"P{self._home.get('selected_pad', 1)}", 7, AMBER, "nw")

        c.create_rectangle(x + 4, 55, x + 118, 81, fill=CABINET, outline="")
        self._focus_frame(x + 5, 56, 55, 24, self._focus == "tune")
        self._focus_frame(x + 61, 56, 55, 24, self._focus == "rate")
        self._text(x + 9, 58, "TUNE", 6, STEEL_LIGHT, "nw")
        self._text(x + 65, 58, "RATE", 6, STEEL_LIGHT, "nw")
        params = self._home.get("parameters", {})
        tune = self._focus_value if self._focus == "tune" else f"{params.get('TUNE_HZ', 220):.0f}H"
        rate = self._focus_value if self._focus == "rate" else f"{params.get('LFO_RATE_HZ', .7):.1f}H"
        self._text(x + 9, 69, tune, 7, AMBER, "nw")
        self._text(x + 65, 69, rate, 7, GREEN_BRIGHT, "nw")

        c.create_rectangle(x + 4, 84, x + 118, 109, fill=STEEL, outline="")
        self._focus_frame(x + 5, 85, 112, 23, self._focus == "depth")
        self._text(x + 9, 88, "DEPTH", 6, STEEL_LIGHT, "nw")
        depth_value = float(params.get("LFO_DEPTH_OCT", 1.0))
        depth = self._focus_value if self._focus == "depth" else f"{depth_value:.2f}"
        self._text(x + 9, 98, depth, 7, CREAM, "nw")
        self._knob(x + 91, 96, depth_value / 2.0, AMBER if self._focus == "depth" else CREAM)
        if self._home.get("hold"):
            c.create_oval(x + 69, 88, x + 75, 94, fill=RED, outline="")
            self._text(x + 61, 99, "HOLD", 6, RED, "nw")

        c.create_rectangle(x + 4, 112, x + 118, 140, fill=CABINET, outline="")
        echo_focus = self._focus in {"echo_level", "echo_time", "echo_feedback", "echo_tone"}
        self._focus_frame(x + 5, 113, 112, 26, echo_focus)
        labels = (("T", 9, "echo_time"), ("FB", 36, "echo_feedback"),
                  ("LV", 68, "echo_level"), ("EQ", 98, "echo_tone"))
        for label, offset, focus in labels:
            self._text(x + offset, 115, label, 6, AMBER if self._focus == focus else STEEL_LIGHT, "nw")
        self._knob(x + 14, 131, self._norm(params.get("DELAY_MS", 360), 20, 1000), CREAM)
        self._knob(x + 45, 131, float(params.get("FEEDBACK", .62)) / 1.05, CREAM)
        self._knob(x + 76, 131, float(params.get("ECHO_LEVEL", .45)), CREAM)
        self._knob(x + 107, 131, self._tone_position(params), CREAM)
        if echo_focus and self._focus_value:
            c.create_rectangle(x + 7, 126, x + 111, 137, fill=NIGHT_HIGH, outline="")
            self._text(x + 10, 128, self._focus_value, 6, AMBER, "nw")

        c.create_rectangle(x + 4, 143, x + 118, 153, fill=STEEL, outline="")
        self._focus_frame(x + 5, 143, 112, 10, self._focus == "master")
        self._vu(x + 8, 146, peak if powered else 0.0, AMBER if self._focus == "master" else GREEN_BRIGHT)
        self._text(x + 79, 145, "LOOP" if self._home.get("echo_loop") else "AMP", 6,
                   RED if self._home.get("echo_loop") else STEEL_LIGHT, "nw")
        led = GREEN_BRIGHT if int(now / 0.16) & 1 else GREEN
        c.create_oval(x + 107, 146, x + 111, 150, fill=led if powered else STEEL, outline="")
        if self._home.get("connected"):
            c.create_rectangle(x + 113, 148, x + 114, 149, fill=AMBER, outline="")

    def _soundboy(
        self,
        now: float,
        x: int,
        carrying: bool,
        powered: bool,
        watching_rack: bool,
    ) -> None:
        c = self.canvas
        bob = int(now / 0.24) & 1 if powered else 0
        y = 76 + bob
        c.create_rectangle(x - 4, y - 12, x + 14, y - 7, fill=BLACK, outline="")
        c.create_rectangle(x - 7, y - 8, x - 2, y + 12, fill=BLACK, outline="")
        c.create_rectangle(x + 11, y - 8, x + 16, y + 10, fill=BLACK, outline="")
        c.create_rectangle(x, y - 9, x + 12, y + 4, fill=SKIN, outline="")
        eye_x = x + 1 if watching_rack else x + 9
        c.create_rectangle(eye_x, y - 4, eye_x + 1, y - 3, fill=BLACK, outline="")
        c.create_rectangle(x - 2, y + 4, x + 14, y + 35, fill=SHIRT, outline="")
        c.create_rectangle(x + 1, y + 35, x + 7, y + 64, fill=DENIM, outline="")
        c.create_rectangle(x + 9, y + 35, x + 15, y + 64, fill=DENIM, outline="")
        c.create_rectangle(x - 2, y + 63, x + 8, y + 67, fill=BLACK, outline="")
        c.create_rectangle(x + 9, y + 63, x + 19, y + 67, fill=BLACK, outline="")
        if carrying:
            c.create_line(x - 2, y + 12, x - 18, y + 28, fill=SKIN, width=2)
            c.create_line(x + 13, y + 13, x + 17, y + 28, fill=SKIN, width=2)
        elif watching_rack:
            c.create_line(x - 2, y + 12, x - 14, y + 23, fill=SKIN, width=2)
            c.create_oval(x - 17, y + 21, x - 13, y + 25, fill=SKIN, outline="")
        else:
            hand = int(now / 0.48) & 1
            c.create_line(x - 1, y + 12, x - 7, y + 30 - hand, fill=SKIN, width=2)
            c.create_line(x + 13, y + 12, x + 18, y + 29 + hand, fill=SKIN, width=2)

    def _cable(self, powered: bool) -> None:
        color = CABLE if powered else STEEL
        self.canvas.create_line(132, 148, 139, 148, 139, 160, 270, 160, 270, 132,
                                fill=color, width=2)

    def _knob(self, x: int, y: int, amount: float, color: str) -> None:
        amount = max(0.0, min(1.0, amount))
        self.canvas.create_oval(x - 7, y - 7, x + 7, y + 7, fill=BLACK, outline=STEEL_LIGHT)
        angle = math.radians(225 - amount * 270)
        self.canvas.create_line(x, y, x + math.cos(angle) * 5, y - math.sin(angle) * 5,
                                fill=color, width=2)

    def _vu(self, x: int, y: int, level: float, color: str) -> None:
        bars = round(max(0.0, min(1.0, level)) * 10)
        for index in range(10):
            fill = RED if index < bars and index > 7 else color if index < bars else NIGHT_HIGH
            self.canvas.create_rectangle(x + index * 6, y, x + index * 6 + 4, y + 4,
                                         fill=fill, outline="")

    def _focus_frame(self, x: int, y: int, width: int, height: int, active: bool) -> None:
        if active:
            self.canvas.create_rectangle(x, y, x + width, y + height, outline=AMBER, width=1)
            self.canvas.create_rectangle(x + 1, y + 1, x + 2, y + 2, fill=CREAM, outline="")

    def _draw_menu(self, items: list[tuple[str, str]], index: int, editing: bool) -> None:
        self._backdrop()
        c = self.canvas
        c.create_rectangle(16, 12, 304, 158, fill=BLACK, outline=STEEL_LIGHT)
        c.create_rectangle(20, 16, 300, 154, fill=BLACK, outline=CABINET_EDGE)
        for x in (20, 294):
            c.create_rectangle(x, 16, x + 6, 22, fill=STEEL_LIGHT, outline="")
            c.create_rectangle(x, 148, x + 6, 154, fill=STEEL_LIGHT, outline="")
        self._text(31, 23, "RACK LABEL / EDIT" if editing else "RACK LABEL / SELECT",
                   7, AMBER, "nw")
        start = max(0, min(index - 1, max(0, len(items) - 3)))
        y = 49
        for absolute, (label, value) in enumerate(items[start:start + 3], start=start):
            active = absolute == index
            if active:
                c.create_rectangle(27, y - 5, 293, y + 18, fill=NIGHT_HIGH, outline="")
            self._text(34, y, ("> " if active else "  ") + label, 8,
                       CREAM if active else STEEL_LIGHT, "nw")
            self._text(286, y, value, 8, AMBER if active else GREEN, "ne")
            y += 30
        self._text(34, 140, "TURN CHANGE  PUSH SAVE" if editing else "TURN SELECT  PUSH OPEN",
                   7, STEEL_LIGHT, "nw")

    @staticmethod
    def _norm(value: float, minimum: float, maximum: float) -> float:
        return max(0.0, min(1.0, (float(value) - minimum) / (maximum - minimum)))

    @staticmethod
    def _tone_position(parameters: dict[str, float]) -> float:
        hpf = max(60.0, float(parameters.get("HPF_HZ", 60.0)))
        lpf = max(200.0, float(parameters.get("LPF_HZ", 7000.0)))
        raw = math.log(hpf / 60.0) - math.log(lpf / 16000.0)
        return max(0.0, min(1.0, raw / 4.2))

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
            font=("Consolas", size, "bold"),
            anchor=anchor,
        )
