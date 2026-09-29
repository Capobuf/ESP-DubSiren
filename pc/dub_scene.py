"""320x170 cyber-dub display renderer shared by Virtual ESP and captures."""

from __future__ import annotations

import math
import time
import tkinter as tk
from collections.abc import Callable
from pathlib import Path
from tkinter import ttk

from PIL import Image, ImageDraw, ImageFont, ImageTk


DISPLAY_WIDTH = 320
DISPLAY_HEIGHT = 170
BOOT_DURATION_S = 3.35
ASSET_ROOT = Path(__file__).resolve().parents[1] / "assets" / "display" / "runtime"

CHARTREUSE = "#c9ff37"
CHARTREUSE_DIM = "#6f8b31"
OFF_WHITE = "#e5e8d8"
AMBER = "#ffb12b"
RED = "#ff4b3e"
BLACK = "#050806"
MUTED = "#748077"

HOME_LAYOUT = {
    "header": (159, 25, 80, 18),
    "tune": (160, 61, 46, 16),
    "rate": (207, 61, 46, 16),
    "depth": (160, 78, 46, 15),
    "master": (207, 78, 46, 15),
    "echo": (160, 94, 93, 21),
    "status": (23, 154, 274, 12),
}

MENU_LAYOUT = {
    "title": (83, 21, 155, 14),
    "rows": (63, 48, 196, 82),
    "footer": (22, 155, 276, 11),
}

BOOT_SHOTS = (
    (0.00, 0.65, "boot_01_init.png", "SYS/INIT"),
    (0.65, 1.50, "boot_02_rollin.png", "RIG/ROLL-IN"),
    (1.50, 2.25, "boot_03_connect.png", "SIGNAL/CONNECT"),
    (2.25, 3.35, "boot_04_power.png", "POWER/ONLINE"),
)


def _font(size: int, bold: bool = False) -> ImageFont.ImageFont:
    candidates = (
        "DejaVuSansMono-Bold.ttf" if bold else "DejaVuSansMono.ttf",
        "consolab.ttf" if bold else "consola.ttf",
    )
    for candidate in candidates:
        try:
            return ImageFont.truetype(candidate, size)
        except OSError:
            pass
    return ImageFont.load_default()


FONT_5 = _font(5, True)
FONT_6 = _font(6, True)
FONT_7 = _font(7, True)
FONT_8 = _font(8, True)


class SceneRenderer:
    """Composites source-derived bitmaps with small live rack UI regions."""

    def __init__(self, asset_root: Path = ASSET_ROOT) -> None:
        self.asset_root = asset_root
        self._images: dict[str, Image.Image] = {}
        self._speaker_level = 0

    def _asset(self, name: str) -> Image.Image:
        if name not in self._images:
            self._images[name] = Image.open(self.asset_root / name).convert("RGBA")
        return self._images[name]

    def boot_scene(self, elapsed: float) -> tuple[int, Image.Image]:
        elapsed = max(0.0, min(BOOT_DURATION_S - 0.001, elapsed))
        shot_index = next(
            index for index, (start, end, _name, _label) in enumerate(BOOT_SHOTS)
            if start <= elapsed < end
        )
        start, _end, name, label = BOOT_SHOTS[shot_index]
        image = self._asset(name).copy()
        draw = ImageDraw.Draw(image)
        draw.rectangle((7, 7, 92, 18), fill=(3, 8, 5, 205))
        draw.text((10, 9), label, font=FONT_7, fill=CHARTREUSE)
        draw.line((10, 20, 68 + shot_index * 13, 20), fill=CHARTREUSE, width=1)
        phase_ms = int((elapsed - start) * 1000)
        if phase_ms < 120 or phase_ms > 610:
            image.alpha_composite(self._asset("glitch.png"))
        scanlines = Image.new("RGBA", image.size, (0, 0, 0, 0))
        scan_draw = ImageDraw.Draw(scanlines)
        for y in range((phase_ms // 17) % 9, DISPLAY_HEIGHT, 12):
            scan_draw.line((0, y, DISPLAY_WIDTH - 1, y), fill=(150, 210, 45, 22))
        image.alpha_composite(scanlines)
        if shot_index == 2:
            pulse = 3 + int(abs(math.sin(elapsed * 14.0)) * 4)
            draw.ellipse((207 - pulse, 112 - pulse, 207 + pulse, 112 + pulse), outline=AMBER)
        if shot_index == 3:
            bars = 4 + int(abs(math.sin(elapsed * 10.0)) * 8)
            for index in range(bars):
                draw.rectangle((170 + index * 5, 144, 173 + index * 5, 147), fill=CHARTREUSE)
        return shot_index, image.convert("RGB")

    def home_scene(
        self,
        state: dict[str, object],
        peak: float,
        focus: str | None,
        focus_value: str,
        now: float,
        glitch: bool = False,
    ) -> Image.Image:
        if state.get("parameters", {}).get("PROFILE") == "CLASSIC":
            return self.classic_scene(state, peak, focus, focus_value)
        image = self._asset("home_bg.png").copy()
        peak = max(0.0, min(1.0, peak))
        self._speaker_level = self._next_speaker_level(peak)
        image.alpha_composite(
            self._asset(f"speaker_reactive_{self._speaker_level}.png"),
            (8, 21),
        )
        interacting = bool(focus or state.get("active_pad"))
        if interacting:
            frame = int(now / 0.22) % 3
            sprite = self._asset(f"soundboy_interact_{frame}.png")
            position = (248, 77)
        else:
            cadence = (0, 0, 1, 0, 2, 0)
            frame = cadence[int(now / 0.48) % len(cadence)]
            sprite = self._asset(f"soundboy_idle_{frame}.png")
            position = (271, 68)
        image.alpha_composite(sprite, position)
        self._draw_home_ui(image, state, peak, focus, focus_value)
        if glitch:
            image.alpha_composite(self._asset("glitch.png"))
        return image.convert("RGB")

    def classic_scene(self, state, peak, focus, focus_value):
        image = Image.new("RGB", (DISPLAY_WIDTH, DISPLAY_HEIGHT), BLACK)
        draw = ImageDraw.Draw(image)
        params = state.get("parameters", {})
        draw.text((16, 10), "DS01E / CLASSIC", font=_font(11), fill=MUTED)
        draw.text((16, 31), str(state.get("sound", "SINE 1")), font=_font(24, True), fill=CHARTREUSE)
        draw.text((16, 73), "PITCH  " + str(params.get("PITCH", "MID")), font=_font(17, True), fill=OFF_WHITE)
        draw.text((16, 99), "RATE   " + str(params.get("RATE", "MID")), font=_font(17, True), fill=OFF_WHITE)
        status = []
        if state.get("hold"): status.append("HOLD")
        if params.get("ECHO_CUT"): status.append("ECHO CUT")
        elif float(params.get("ECHO_LEVEL", 0)) > 0: status.append("ECHO")
        draw.text((16, 139), "   ".join(status) or "DRY", font=_font(13, True), fill=AMBER)
        if focus:
            labels = {"tune": "PITCH", "rate": "RATE", "echo_time": "ECHO TIME",
                      "echo_level": "ECHO LEVEL", "echo_feedback": "ECHO FEEDBACK",
                      "echo_tone": "FILTER", "master": "MASTER"}
            draw.rectangle((10, 61, 309, 132), fill=BLACK, outline=CHARTREUSE_DIM)
            draw.text((22, 68), labels.get(focus, focus.upper()), font=_font(13), fill=MUTED)
            draw.text((22, 92), focus_value, font=_font(23, True), fill=OFF_WHITE)
        return image

    def menu_scene(
        self,
        items: list[tuple[str, str]],
        index: int,
        editing: bool,
        glitch: bool = False,
    ) -> Image.Image:
        image = self._asset("menu_bg.png").copy()
        draw = ImageDraw.Draw(image)
        tx, ty, tw, th = MENU_LAYOUT["title"]
        draw.rectangle((tx, ty, tx + tw, ty + th), fill=(3, 9, 6, 220))
        title = "RACK/EDIT" if editing else "RACK/SELECT"
        draw.text((tx + 4, ty + 3), title, font=FONT_7, fill=AMBER if editing else CHARTREUSE)
        start = max(0, min(index - 1, max(0, len(items) - 3)))
        x, y, width, _height = MENU_LAYOUT["rows"]
        for row, (label, value) in enumerate(items[start:start + 3]):
            absolute = start + row
            top = y + row * 26
            active = absolute == index
            fill = (16, 31, 19, 238) if active else (4, 11, 7, 210)
            outline = CHARTREUSE if active else CHARTREUSE_DIM
            draw.rectangle((x, top, x + width, top + 21), fill=fill, outline=outline)
            draw.text((x + 5, top + 6), ("> " if active else "  ") + label[:18],
                      font=FONT_7, fill=OFF_WHITE if active else MUTED)
            if value:
                right = x + width - 5
                box = draw.textbbox((0, 0), value, font=FONT_7)
                draw.text((right - (box[2] - box[0]), top + 6), value,
                          font=FONT_7, fill=AMBER if active else CHARTREUSE_DIM)
        fx, fy, fw, fh = MENU_LAYOUT["footer"]
        draw.rectangle((fx, fy, fx + fw, fy + fh), fill=(2, 7, 4, 225))
        footer = "TURN CHANGE / PUSH SAVE" if editing else "TURN SELECT / PUSH OPEN"
        draw.text((fx + 5, fy + 2), footer, font=FONT_6, fill=OFF_WHITE)
        if glitch:
            image.alpha_composite(self._asset("glitch.png"))
        return image.convert("RGB")

    def _next_speaker_level(self, peak: float) -> int:
        level = self._speaker_level
        if peak >= 0.65:
            return 2
        if peak >= 0.30:
            return max(1, level if peak >= 0.52 else 1)
        if level == 2 and peak >= 0.52:
            return 2
        if level == 1 and peak >= 0.22:
            return 1
        return 0

    def _draw_home_ui(
        self,
        image: Image.Image,
        state: dict[str, object],
        peak: float,
        focus: str | None,
        focus_value: str,
    ) -> None:
        draw = ImageDraw.Draw(image)
        params = state.get("parameters", {})
        if not isinstance(params, dict):
            params = {}
        hx, hy, hw, hh = HOME_LAYOUT["header"]
        draw.rectangle((hx, hy, hx + hw, hy + hh), fill=(2, 9, 5, 230))
        draw.text((hx + 3, hy + 2), str(state.get("sound", "CLASSIC"))[:12],
                  font=FONT_7, fill=CHARTREUSE)
        draw.text((hx + 3, hy + 10), str(state.get("variant", ""))[:14],
                  font=FONT_5, fill=OFF_WHITE)
        draw.text((hx + hw - 16, hy + 5), f"P{state.get('selected_pad', 1)}",
                  font=FONT_7, fill=AMBER)

        tune = focus_value if focus == "tune" else f"{float(params.get('TUNE_HZ', 220.0)):.0f}H"
        rate = focus_value if focus == "rate" else f"{float(params.get('LFO_RATE_HZ', 0.7)):.1f}H"
        depth = focus_value if focus == "depth" else f"{float(params.get('LFO_DEPTH_OCT', 1.0)):.2f}"
        master = focus_value if focus == "master" else f"{float(params.get('MASTER', 0.5)) * 100:.0f}%"
        self._module(draw, "tune", "TUNE", tune, focus)
        self._module(draw, "rate", "RATE", rate, focus)
        self._module(draw, "depth", "DEPTH", depth, focus)
        self._module(draw, "master", "MASTER", master, focus)

        ex, ey, ew, eh = HOME_LAYOUT["echo"]
        echo_active = focus in {"echo_time", "echo_feedback", "echo_level", "echo_tone"}
        draw.rectangle((ex, ey, ex + ew, ey + eh), fill=(3, 11, 7, 235),
                       outline=AMBER if echo_active else CHARTREUSE_DIM)
        labels = (
            ("T", "echo_time", f"{float(params.get('DELAY_MS', 360.0)):.0f}"),
            ("FB", "echo_feedback", f"{float(params.get('FEEDBACK', .62)) * 100:.0f}"),
            ("LV", "echo_level", f"{float(params.get('ECHO_LEVEL', .45)) * 100:.0f}"),
            ("EQ", "echo_tone", str(state.get("echo_tone", "CUSTOM"))[:3]),
        )
        for column, (label, key, value) in enumerate(labels):
            left = ex + 3 + column * 23
            color = AMBER if focus == key else MUTED
            draw.text((left, ey + 2), label, font=FONT_5, fill=color)
            shown = focus_value[:6] if focus == key else value
            draw.text((left, ey + 10), shown, font=FONT_6, fill=OFF_WHITE)

        sx, sy, sw, sh = HOME_LAYOUT["status"]
        draw.rectangle((sx, sy, sx + sw, sy + sh), fill=(2, 8, 4, 230))
        status = []
        if state.get("hold"):
            status.append("HOLD")
        if state.get("echo_loop"):
            status.append("LOOP")
        if state.get("connected"):
            status.append("LINK")
        draw.text((sx + 4, sy + 2), " / ".join(status) or "SYSTEM READY",
                  font=FONT_6, fill=RED if state.get("echo_loop") else CHARTREUSE)
        bars = round(peak * 16)
        for bar in range(16):
            color = RED if bar >= 13 else AMBER if bar >= 10 else CHARTREUSE
            draw.rectangle((sx + 168 + bar * 6, sy + 4, sx + 172 + bar * 6, sy + 7),
                           fill=color if bar < bars else (31, 45, 34))

    @staticmethod
    def _module(draw: ImageDraw.ImageDraw, key: str, label: str, value: str,
                focus: str | None) -> None:
        x, y, width, height = HOME_LAYOUT[key]
        active = focus == key
        draw.rectangle((x, y, x + width, y + height), fill=(3, 11, 7, 235),
                       outline=AMBER if active else CHARTREUSE_DIM)
        draw.text((x + 2, y + 1), label, font=FONT_5, fill=AMBER if active else MUTED)
        draw.text((x + 2, y + 7), value[:9], font=FONT_7,
                  fill=AMBER if active else OFF_WHITE)


class PanelDisplay(ttk.Frame):
    """Tk widget that presents frames produced by :class:`SceneRenderer`."""

    def __init__(
        self,
        parent: tk.Misc,
        peak_provider: Callable[[], float] | None = None,
    ) -> None:
        super().__init__(parent)
        self.canvas = tk.Canvas(self, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT,
                                bg=BLACK, highlightthickness=2,
                                highlightbackground="#4c5550")
        self.canvas.pack()
        self._peak_provider = peak_provider
        self._renderer = SceneRenderer()
        self._boot_started = time.monotonic()
        self._screen = "home"
        self._focus: str | None = None
        self._focus_value = ""
        self._focus_until = 0.0
        self._transition_until = 0.0
        self._photo: ImageTk.PhotoImage | None = None
        self._home: dict[str, object] = self._default_home()
        self._menu: tuple[list[tuple[str, str]], int, bool] = ([], 0, False)
        self.after(100, self._animate)

    @staticmethod
    def _default_home() -> dict[str, object]:
        return {
            "selected_pad": 1, "current_pad": 1, "active_pad": 0,
            "sound": "CLASSIC", "variant": "SINE 1", "hold": False,
            "echo_loop": False, "connected": False, "parameters": {},
            "echo_tone": "CUSTOM",
        }

    def _get_renderer(self) -> SceneRenderer:
        renderer = getattr(self, "_renderer", None)
        if renderer is None:
            renderer = SceneRenderer()
            self._renderer = renderer
        return renderer

    def replay_boot(self) -> None:
        self._focus = None
        self._boot_started = time.monotonic()
        self._draw()

    def render_home(
        self, selected_pad: int, current_pad: int, sound: str, variant: str,
        depth: str, hold: bool, echo_loop: bool, connected: bool,
        parameters: dict[str, float] | None = None, active_pad: int = 0,
        echo_tone: str = "CUSTOM",
    ) -> None:
        previous = getattr(self, "_screen", "home")
        self._screen = "home"
        self._home = {
            "selected_pad": selected_pad, "current_pad": current_pad,
            "active_pad": active_pad, "sound": sound, "variant": variant,
            "depth": depth, "hold": hold, "echo_loop": echo_loop,
            "connected": connected, "parameters": dict(parameters or {}),
            "echo_tone": echo_tone,
        }
        if previous != "home":
            self._transition_until = time.monotonic() + 0.12
        self._draw()

    def render_overlay(self, title: str, value: str) -> None:
        mapping = {
            "TUNE": "tune", "PITCH": "tune", "FILTER": "echo_tone", "RATE": "rate", "DEPTH": "depth",
            "ECHO LEVEL": "echo_level", "ECHO TIME": "echo_time",
            "ECHO FEEDBACK": "echo_feedback", "ECHO TONE": "echo_tone",
            "MASTER": "master",
        }
        clean_title = title.split(" ·", 1)[0].split(" Â·", 1)[0]
        self._focus = mapping.get(clean_title)
        self._focus_value = value
        self._focus_until = time.monotonic() + 1.1
        self._screen = "home"
        self._draw()

    def render_menu(self, items: list[tuple[str, str]], index: int, editing: bool) -> None:
        previous = getattr(self, "_screen", "home")
        self._screen = "menu"
        self._menu = (items, index, editing)
        if previous != "menu":
            self._transition_until = time.monotonic() + 0.12
        self._draw()

    def render_image(self, now: float | None = None) -> Image.Image:
        now = time.monotonic() if now is None else now
        boot_started = getattr(self, "_boot_started", now - BOOT_DURATION_S)
        if now - boot_started < BOOT_DURATION_S:
            return self._get_renderer().boot_scene(now - boot_started)[1]
        glitch = now < getattr(self, "_transition_until", 0.0)
        if getattr(self, "_screen", "home") == "menu":
            return self._get_renderer().menu_scene(*self._menu, glitch=glitch)
        if getattr(self, "_focus_until", 0.0) <= now:
            self._focus = None
            self._focus_value = ""
        peak = 0.0
        provider = getattr(self, "_peak_provider", None)
        if provider is not None:
            try:
                peak = float(provider())
            except (TypeError, ValueError, RuntimeError):
                pass
        return self._get_renderer().home_scene(
            getattr(self, "_home", self._default_home()), peak,
            getattr(self, "_focus", None), getattr(self, "_focus_value", ""),
            now, glitch,
        )

    def _animate(self) -> None:
        self._draw()
        self.after(100, self._animate)

    def _draw(self) -> None:
        self._present(self.render_image())

    def _draw_boot(self, elapsed: float) -> None:
        self._present(self._get_renderer().boot_scene(elapsed)[1])

    def _draw_scene(self, now: float) -> None:
        provider = getattr(self, "_peak_provider", None)
        peak = float(provider()) if provider else 0.0
        self._present(self._get_renderer().home_scene(
            self._home, peak, self._focus, self._focus_value, now,
        ))

    def _draw_menu(self, items: list[tuple[str, str]], index: int, editing: bool) -> None:
        self._present(self._get_renderer().menu_scene(items, index, editing))

    def _present(self, image: Image.Image) -> None:
        if not hasattr(self.canvas, "create_image"):
            return
        self._photo = ImageTk.PhotoImage(image)
        self.canvas.delete("all")
        self.canvas.create_image(0, 0, image=self._photo, anchor="nw")
