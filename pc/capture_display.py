"""Generate deterministic 320x170 reference captures from the live renderer."""

from __future__ import annotations

from pathlib import Path

from control_ranges import CONTROL_SPECS
from dub_scene import SceneRenderer


def state() -> dict[str, object]:
    parameters = {name: spec.default for name, spec in CONTROL_SPECS.items()}
    parameters.update({"HPF_HZ": 60.0, "LPF_HZ": 7000.0})
    return {
        "selected_pad": 1,
        "current_pad": 1,
        "active_pad": 0,
        "sound": "CLASSIC",
        "variant": "SINE 1",
        "hold": False,
        "echo_loop": False,
        "connected": True,
        "parameters": parameters,
        "echo_tone": "CUSTOM",
    }


def main() -> None:
    output = Path(__file__).resolve().parents[1] / "docs" / "screenshots"
    output.mkdir(parents=True, exist_ok=True)
    renderer = SceneRenderer()

    for number, elapsed in enumerate((0.32, 1.02, 1.82, 2.72), start=1):
        shot, image = renderer.boot_scene(elapsed)
        assert shot == number - 1
        image.save(output / f"display-boot-{number:02d}.png", optimize=True)

    home = state()
    renderer.home_scene(home, 0.68, None, "", 8.4).save(
        output / "display-idle.png", optimize=True
    )

    tune = state()
    tune["parameters"]["TUNE_HZ"] = 440.0
    renderer.home_scene(tune, 0.42, "tune", "440 Hz", 8.4).save(
        output / "display-tune.png", optimize=True
    )

    echo = state()
    echo["parameters"]["FEEDBACK"] = 0.82
    echo["echo_loop"] = True
    renderer.home_scene(echo, 0.76, "echo_feedback", "82% FB", 8.4).save(
        output / "display-echo.png", optimize=True
    )

    renderer.menu_scene(
        [("CONFIGURE PAD", "2"), ("ASSIGNMENT", "TWO TONE"),
         ("VARIANT", "TRIANGLE"), ("HOLD", "ON"),
         ("ECHO LOOP", "OFF"), ("BACK", "")],
        2,
        True,
    ).save(output / "display-menu.png", optimize=True)

    old_boot = output / "display-boot.png"
    if old_boot.exists():
        old_boot.unlink()
    for path in sorted(output.glob("display-*.png")):
        print(path.relative_to(output.parents[1]), path.stat().st_size)


if __name__ == "__main__":
    main()
