from __future__ import annotations

import unittest

from dub_scene import DISPLAY_HEIGHT, DISPLAY_WIDTH, SceneRenderer


def state() -> dict[str, object]:
    return {
        "selected_pad": 2, "current_pad": 2, "active_pad": 0,
        "sound": "TWO TONE", "variant": "TRIANGLE", "hold": True,
        "echo_loop": False, "connected": True, "echo_tone": "CUSTOM",
        "parameters": {
            "TUNE_HZ": 330.0, "LFO_RATE_HZ": 1.4,
            "LFO_DEPTH_OCT": 0.75, "DELAY_MS": 420.0,
            "FEEDBACK": 0.62, "ECHO_LEVEL": 0.45, "MASTER": 0.5,
        },
    }


class SceneRendererTests(unittest.TestCase):
    def setUp(self) -> None:
        self.renderer = SceneRenderer()

    def test_home_and_focus_render_at_native_size(self) -> None:
        home = self.renderer.home_scene(state(), 0.4, None, "", 8.4)
        tune = self.renderer.home_scene(state(), 0.4, "tune", "440 Hz", 8.4)
        echo = self.renderer.home_scene(
            state(), 0.8, "echo_feedback", "82% FB", 8.4
        )
        self.assertEqual(home.size, (DISPLAY_WIDTH, DISPLAY_HEIGHT))
        self.assertNotEqual(home.tobytes(), tune.tobytes())
        self.assertNotEqual(tune.tobytes(), echo.tobytes())

    def test_menu_and_four_boot_shots(self) -> None:
        menu = self.renderer.menu_scene([("PAD", "2"), ("BACK", "")], 0, False)
        self.assertEqual(menu.size, (320, 170))
        indices = [self.renderer.boot_scene(t)[0] for t in (0.2, 0.9, 1.8, 2.7)]
        self.assertEqual(indices, [0, 1, 2, 3])


if __name__ == "__main__":
    unittest.main()
