"""Render deterministic 320x170 PNG reference captures from the live scene code."""

from __future__ import annotations

import struct
import time
import zlib
from pathlib import Path

from control_ranges import CONTROL_SPECS
from dub_scene import DISPLAY_HEIGHT, DISPLAY_WIDTH, PanelDisplay


FONT = {
    " ": ("00000",) * 7,
    "A": ("01110", "10001", "10001", "11111", "10001", "10001", "10001"),
    "B": ("11110", "10001", "10001", "11110", "10001", "10001", "11110"),
    "C": ("01111", "10000", "10000", "10000", "10000", "10000", "01111"),
    "D": ("11110", "10001", "10001", "10001", "10001", "10001", "11110"),
    "E": ("11111", "10000", "10000", "11110", "10000", "10000", "11111"),
    "F": ("11111", "10000", "10000", "11110", "10000", "10000", "10000"),
    "G": ("01111", "10000", "10000", "10111", "10001", "10001", "01111"),
    "H": ("10001", "10001", "10001", "11111", "10001", "10001", "10001"),
    "I": ("11111", "00100", "00100", "00100", "00100", "00100", "11111"),
    "J": ("00111", "00010", "00010", "00010", "10010", "10010", "01100"),
    "K": ("10001", "10010", "10100", "11000", "10100", "10010", "10001"),
    "L": ("10000", "10000", "10000", "10000", "10000", "10000", "11111"),
    "M": ("10001", "11011", "10101", "10101", "10001", "10001", "10001"),
    "N": ("10001", "11001", "10101", "10011", "10001", "10001", "10001"),
    "O": ("01110", "10001", "10001", "10001", "10001", "10001", "01110"),
    "P": ("11110", "10001", "10001", "11110", "10000", "10000", "10000"),
    "Q": ("01110", "10001", "10001", "10001", "10101", "10010", "01101"),
    "R": ("11110", "10001", "10001", "11110", "10100", "10010", "10001"),
    "S": ("01111", "10000", "10000", "01110", "00001", "00001", "11110"),
    "T": ("11111", "00100", "00100", "00100", "00100", "00100", "00100"),
    "U": ("10001", "10001", "10001", "10001", "10001", "10001", "01110"),
    "V": ("10001", "10001", "10001", "10001", "10001", "01010", "00100"),
    "W": ("10001", "10001", "10001", "10101", "10101", "11011", "10001"),
    "X": ("10001", "10001", "01010", "00100", "01010", "10001", "10001"),
    "Y": ("10001", "10001", "01010", "00100", "00100", "00100", "00100"),
    "Z": ("11111", "00001", "00010", "00100", "01000", "10000", "11111"),
    "0": ("01110", "10001", "10011", "10101", "11001", "10001", "01110"),
    "1": ("00100", "01100", "00100", "00100", "00100", "00100", "01110"),
    "2": ("01110", "10001", "00001", "00010", "00100", "01000", "11111"),
    "3": ("11110", "00001", "00001", "01110", "00001", "00001", "11110"),
    "4": ("00010", "00110", "01010", "10010", "11111", "00010", "00010"),
    "5": ("11111", "10000", "10000", "11110", "00001", "00001", "11110"),
    "6": ("01110", "10000", "10000", "11110", "10001", "10001", "01110"),
    "7": ("11111", "00001", "00010", "00100", "01000", "01000", "01000"),
    "8": ("01110", "10001", "10001", "01110", "10001", "10001", "01110"),
    "9": ("01110", "10001", "10001", "01111", "00001", "00001", "01110"),
    ".": ("00000", "00000", "00000", "00000", "00000", "00110", "00110"),
    "%": ("11001", "11010", "00100", "01000", "10110", "00110", "00000"),
    "-": ("00000", "00000", "00000", "11111", "00000", "00000", "00000"),
    "/": ("00001", "00010", "00010", "00100", "01000", "01000", "10000"),
    ">": ("10000", "01000", "00100", "00010", "00100", "01000", "10000"),
    ":": ("00000", "00110", "00110", "00000", "00110", "00110", "00000"),
}


class RasterCanvas:
    def __init__(self, width: int, height: int) -> None:
        self.width = width
        self.height = height
        self.pixels = bytearray(width * height * 3)

    @staticmethod
    def _color(value: str) -> tuple[int, int, int]:
        if not value:
            return (0, 0, 0)
        value = value.lstrip("#")
        return tuple(int(value[index:index + 2], 16) for index in (0, 2, 4))

    def _pixel(self, x: int, y: int, color: str) -> None:
        if 0 <= x < self.width and 0 <= y < self.height:
            offset = (y * self.width + x) * 3
            self.pixels[offset:offset + 3] = bytes(self._color(color))

    def delete(self, *_args) -> None:
        self.pixels[:] = b"\x00" * len(self.pixels)

    def create_rectangle(self, x1, y1, x2, y2, fill="", outline="", width=1, **_kwargs):
        left, right = sorted((round(x1), round(x2)))
        top, bottom = sorted((round(y1), round(y2)))
        if fill:
            for y in range(top, bottom + 1):
                for x in range(left, right + 1):
                    self._pixel(x, y, fill)
        if outline:
            for offset in range(max(1, round(width))):
                self.create_line(left + offset, top + offset, right - offset, top + offset, fill=outline)
                self.create_line(right - offset, top + offset, right - offset, bottom - offset, fill=outline)
                self.create_line(right - offset, bottom - offset, left + offset, bottom - offset, fill=outline)
                self.create_line(left + offset, bottom - offset, left + offset, top + offset, fill=outline)

    def create_line(self, *coords, fill="#ffffff", width=1, **_kwargs):
        points = [(round(coords[i]), round(coords[i + 1])) for i in range(0, len(coords), 2)]
        for start, end in zip(points, points[1:]):
            x1, y1 = start
            x2, y2 = end
            dx, dy = abs(x2 - x1), -abs(y2 - y1)
            sx, sy = (1 if x1 < x2 else -1), (1 if y1 < y2 else -1)
            error = dx + dy
            while True:
                radius = max(0, round(width) // 2)
                for py in range(y1 - radius, y1 + radius + 1):
                    for px in range(x1 - radius, x1 + radius + 1):
                        self._pixel(px, py, fill)
                if (x1, y1) == (x2, y2):
                    break
                twice = 2 * error
                if twice >= dy:
                    error += dy
                    x1 += sx
                if twice <= dx:
                    error += dx
                    y1 += sy

    def create_oval(self, x1, y1, x2, y2, fill="", outline="", **_kwargs):
        left, right = sorted((round(x1), round(x2)))
        top, bottom = sorted((round(y1), round(y2)))
        cx, cy = (left + right) / 2.0, (top + bottom) / 2.0
        rx, ry = max(0.5, (right - left) / 2.0), max(0.5, (bottom - top) / 2.0)
        for y in range(top, bottom + 1):
            for x in range(left, right + 1):
                distance = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
                if fill and distance <= 1.0:
                    self._pixel(x, y, fill)
                if outline and 0.78 <= distance <= 1.18:
                    self._pixel(x, y, outline)

    def create_polygon(self, *coords, fill="", outline="", **_kwargs):
        points = [(round(coords[i]), round(coords[i + 1])) for i in range(0, len(coords), 2)]
        if fill:
            for y in range(min(point[1] for point in points), max(point[1] for point in points) + 1):
                intersections = []
                for first, second in zip(points, points[1:] + points[:1]):
                    (x1, y1), (x2, y2) = first, second
                    if y1 == y2 or y < min(y1, y2) or y >= max(y1, y2):
                        continue
                    intersections.append(round(x1 + (y - y1) * (x2 - x1) / (y2 - y1)))
                intersections.sort()
                for left, right in zip(intersections[::2], intersections[1::2]):
                    for x in range(left, right + 1):
                        self._pixel(x, y, fill)
        if outline:
            for first, second in zip(points, points[1:] + points[:1]):
                self.create_line(*first, *second, fill=outline)

    def create_text(self, x, y, text="", fill="#ffffff", font=None, anchor="center", **_kwargs):
        scale = 1
        glyph_width = 6 * scale
        width = max(0, len(str(text)) * glyph_width - scale)
        height = 7 * scale
        x, y = round(x), round(y)
        if "e" in anchor:
            x -= width
        elif "w" not in anchor:
            x -= width // 2
        if "s" in anchor:
            y -= height
        elif "n" not in anchor:
            y -= height // 2
        for character in str(text).upper():
            rows = FONT.get(character, FONT[" "])
            for row, bits in enumerate(rows):
                for column, bit in enumerate(bits):
                    if bit == "1":
                        self._pixel(x + column, y + row, fill)
            x += glyph_width

    def save(self, path: Path) -> None:
        raw = b"".join(
            b"\x00" + bytes(self.pixels[y * self.width * 3:(y + 1) * self.width * 3])
            for y in range(self.height)
        )
        def chunk(kind: bytes, data: bytes) -> bytes:
            return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
        png = b"\x89PNG\r\n\x1a\n"
        png += chunk(b"IHDR", struct.pack(">IIBBBBB", self.width, self.height, 8, 2, 0, 0, 0))
        png += chunk(b"IDAT", zlib.compress(raw, 9))
        png += chunk(b"IEND", b"")
        path.write_bytes(png)


def scene() -> tuple[PanelDisplay, RasterCanvas]:
    display = object.__new__(PanelDisplay)
    canvas = RasterCanvas(DISPLAY_WIDTH, DISPLAY_HEIGHT)
    display.canvas = canvas
    display._peak_provider = lambda: 0.68
    display._boot_started = time.monotonic() - 10.0
    display._screen = "home"
    display._focus = None
    display._focus_value = ""
    display._focus_until = 0.0
    display._menu = ([], 0, False)
    parameters = {name: spec.default for name, spec in CONTROL_SPECS.items()}
    parameters.update({"HPF_HZ": 60.0, "LPF_HZ": 7000.0})
    display._home = {
        "selected_pad": 1,
        "current_pad": 1,
        "active_pad": 1,
        "sound": "CLASSIC",
        "variant": "SINE 1",
        "depth": "1.00 oct",
        "hold": False,
        "echo_loop": False,
        "connected": True,
        "parameters": parameters,
        "echo_tone": "CUSTOM",
    }
    return display, canvas


def main() -> None:
    output = Path(__file__).resolve().parents[1] / "docs" / "screenshots"
    output.mkdir(parents=True, exist_ok=True)

    display, canvas = scene()
    display._draw_scene(8.4)
    canvas.save(output / "display-idle.png")

    display, canvas = scene()
    display._focus = "tune"
    display._focus_value = "440 Hz"
    display._home["parameters"]["TUNE_HZ"] = 440.0
    display._draw_scene(8.4)
    canvas.save(output / "display-tune.png")

    display, canvas = scene()
    display._focus = "echo_feedback"
    display._focus_value = "82% FB"
    display._home["parameters"]["FEEDBACK"] = 0.82
    display._home["echo_loop"] = True
    display._draw_scene(8.4)
    canvas.save(output / "display-echo.png")

    display, canvas = scene()
    display._draw_menu(
        [("CONFIGURE PAD", "2"), ("ASSIGNMENT", "TWO TONE"),
         ("VARIANT", "TRIANGLE"), ("HOLD", "ON"),
         ("ECHO LOOP", "OFF"), ("BACK", "")],
        2,
        True,
    )
    canvas.save(output / "display-menu.png")

    display, canvas = scene()
    display._draw_boot(2.62)
    canvas.save(output / "display-boot.png")

    for path in sorted(output.glob("display-*.png")):
        print(path.relative_to(output.parents[1]), path.stat().st_size)


if __name__ == "__main__":
    main()
