from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import unittest
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "assets" / "display" / "manifest.json"
BUILDER = ROOT / "tools" / "display_assets" / "build_assets.py"


class DisplayAssetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        subprocess.run([sys.executable, str(BUILDER)], check=True, cwd=ROOT)
        cls.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

    def test_backgrounds_are_native_size(self) -> None:
        for spec in self.manifest["backgrounds"].values():
            with Image.open(MANIFEST.parent / spec["output"]) as image:
                self.assertEqual(image.size, (320, 170))

    def test_sprite_frame_counts_and_sizes(self) -> None:
        runtime = MANIFEST.parent / "runtime"
        for name, spec in self.manifest["sprites"].items():
            for frame in range(spec["frames"]):
                suffix = f"_{frame}" if spec["frames"] > 1 else ""
                with Image.open(runtime / f"{name}{suffix}.png") as image:
                    self.assertEqual(image.size, tuple(spec["runtime_size"]))
                    self.assertEqual(image.mode, "RGBA")

    def test_generation_is_deterministic(self) -> None:
        generated = ROOT / "firmware" / "src" / "display" / "assets" / "DisplayAssets.cpp"
        before = hashlib.sha256(generated.read_bytes()).digest()
        subprocess.run([sys.executable, str(BUILDER)], check=True, cwd=ROOT)
        self.assertEqual(before, hashlib.sha256(generated.read_bytes()).digest())


if __name__ == "__main__":
    unittest.main()
