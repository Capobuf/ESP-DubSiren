"""Build deterministic desktop PNG and firmware RGB565 display assets."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = ROOT / "assets" / "display" / "manifest.json"
CPP_HEADER = ROOT / "firmware" / "src" / "display" / "assets" / "DisplayAssets.h"
CPP_SOURCE = ROOT / "firmware" / "src" / "display" / "assets" / "DisplayAssets.cpp"


def save_png(image: Image.Image, path: Path) -> None:
    temporary = path.with_name(f"{path.name}.{os.getpid()}.tmp")
    image.save(temporary, format="PNG", optimize=True)
    temporary.replace(path)


def write_text(path: Path, content: str) -> None:
    temporary = path.with_name(f"{path.name}.{os.getpid()}.tmp")
    temporary.write_text(content, encoding="utf-8", newline="\n")
    temporary.replace(path)


def symbol(name: str) -> str:
    return "k" + "".join(part.capitalize() for part in name.split("_"))


def rgb565_bytes(image: Image.Image) -> bytes:
    pixels = image.convert("RGB").tobytes()
    output = bytearray()
    for index in range(0, len(pixels), 3):
        red, green, blue = pixels[index:index + 3]
        value = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
        output.extend((value & 0xFF, value >> 8))
    return bytes(output)


def mask_bytes(image: Image.Image, threshold: int) -> bytes:
    alpha = image.getchannel("A")
    output = bytearray((image.width * image.height + 7) // 8)
    for index, value in enumerate(alpha.tobytes()):
        if value >= threshold:
            output[index // 8] |= 0x80 >> (index & 7)
    return bytes(output)


def cpp_words(data: bytes) -> str:
    values = [f"0x{data[index + 1]:02x}{data[index]:02x}" for index in range(0, len(data), 2)]
    return ",\n    ".join(", ".join(values[start:start + 12]) for start in range(0, len(values), 12))


def cpp_bytes(data: bytes) -> str:
    values = [f"0x{value:02x}" for value in data]
    return ",\n    ".join(", ".join(values[start:start + 16]) for start in range(0, len(values), 16))


def contain_rgba(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    alpha_box = image.getchannel("A").getbbox()
    if alpha_box is None:
        raise ValueError("sprite frame has no visible alpha")
    cropped = image.crop(alpha_box)
    cropped.thumbnail(size, Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    canvas.alpha_composite(cropped, ((size[0] - cropped.width) // 2, size[1] - cropped.height))
    return canvas


def build() -> dict[str, int | str]:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    runtime = MANIFEST_PATH.parent / "runtime"
    runtime.mkdir(parents=True, exist_ok=True)
    CPP_HEADER.parent.mkdir(parents=True, exist_ok=True)

    arrays: list[tuple[str, bytes, int, int, bool]] = []
    for name, spec in manifest["backgrounds"].items():
        source = MANIFEST_PATH.parent / spec["source"]
        image = Image.open(source).convert("RGB")
        crop = tuple(spec["crop"])
        cropped = image.crop(crop)
        width, height = spec["runtime_size"]
        if cropped.width * height != cropped.height * width:
            raise ValueError(f"{name}: crop is not {width}:{height}")
        rendered = cropped.resize((width, height), Image.Resampling.LANCZOS)
        save_png(rendered, MANIFEST_PATH.parent / spec["output"])
        arrays.append((symbol(name), rgb565_bytes(rendered), width, height, False))

    for name, spec in manifest["sprites"].items():
        source = Image.open(MANIFEST_PATH.parent / spec["source"]).convert("RGBA")
        frame_count = int(spec["frames"])
        frame_width, frame_height = spec["frame_rect"][2:]
        target = tuple(spec["runtime_size"])
        if source.width < frame_count * frame_width or source.height < frame_height:
            raise ValueError(f"{name}: source is smaller than declared frames")
        for frame in range(frame_count):
            box = (frame * frame_width, 0, (frame + 1) * frame_width, frame_height)
            extracted = source.crop(box)
            if "crop" in spec:
                extracted = extracted.crop(tuple(spec["crop"]))
                if extracted.width * target[1] != extracted.height * target[0]:
                    raise ValueError(f"{name}: crop is not proportional to runtime size")
                rendered = extracted.resize(target, Image.Resampling.LANCZOS)
            else:
                rendered = contain_rgba(extracted, target)
            output_name = f"{name}_{frame}.png" if frame_count > 1 else f"{name}.png"
            save_png(rendered, runtime / output_name)
            base = f"{symbol(name)}Frame{frame}" if frame_count > 1 else symbol(name)
            arrays.append((base, rgb565_bytes(rendered), target[0], target[1], True))
            arrays.append((base + "Mask", mask_bytes(rendered, int(spec["alpha_threshold"])), target[0], target[1], True))

    declarations = [
        "#pragma once",
        "",
        "#include <Arduino.h>",
        "",
        "namespace DisplayAssets {",
    ]
    definitions = [
        '#include "DisplayAssets.h"',
        "",
        "namespace DisplayAssets {",
    ]
    total = 0
    for name, data, width, height, masked in arrays:
        is_mask = name.endswith("Mask")
        kind = "uint8_t" if is_mask else "uint16_t"
        declarations.extend([
            f"constexpr uint16_t {name}Width = {width};",
            f"constexpr uint16_t {name}Height = {height};",
            f"extern const {kind} {name}[] PROGMEM;",
        ])
        body = cpp_bytes(data) if is_mask else cpp_words(data)
        definitions.append(f"const {kind} {name}[] PROGMEM = {{\n    {body}\n}};")
        total += len(data)
    declarations.extend(["", "}  // namespace DisplayAssets", ""])
    definitions.extend(["", "}  // namespace DisplayAssets", ""])
    write_text(CPP_HEADER, "\n".join(declarations))
    write_text(CPP_SOURCE, "\n\n".join(definitions))
    digest = hashlib.sha256(CPP_SOURCE.read_bytes()).hexdigest()
    return {"firmware_asset_bytes": total, "cpp_sha256": digest}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="fail if generated C++ changes")
    args = parser.parse_args()
    before = CPP_SOURCE.read_bytes() if args.check and CPP_SOURCE.exists() else None
    report = build()
    if before is not None and before != CPP_SOURCE.read_bytes():
        raise SystemExit("generated DisplayAssets.cpp was stale")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
