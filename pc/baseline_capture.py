"""Render repeatable Classic listening clips through the real Virtual ESP core."""

import argparse
import array
import ctypes
import json
import queue
import wave
from pathlib import Path

from virtual_transport import VirtualTransport


def capture(directory: Path) -> dict:
    directory.mkdir(parents=True, exist_ok=True)
    transport = VirtualTransport(queue.Queue())
    transport._build_if_needed()
    transport._load_library()
    core = transport._lib
    block = (ctypes.c_int16 * 480)()

    def send(command):
        if core.virtual_esp_command(command.encode("ascii")) != 1:
            raise RuntimeError(command)

    def render(blocks):
        pcm = array.array("h")
        for _ in range(blocks):
            assert core.virtual_esp_render(block, 480) == 1
            pcm.extend(block)
        return pcm

    results = {}
    for mode in ("SINE1", "SINE2", "TEST_TONE", "SQUARE"):
        for label, level, feedback in (("dry", 0, 0), ("single", 0.6, 0),
                                       ("feedback_050", 0.6, 0.5),
                                       ("feedback_080", 0.6, 0.8)):
            assert core.virtual_esp_init() == 1
            for command in (f"SET MODE {mode}", "SET CLASSIC_PITCH MID",
                            "SET CLASSIC_MOD MID", f"SET ECHO_LEVEL {level}",
                            f"SET FEEDBACK {feedback}", "SET DELAY_MS 300",
                            "SET ECHO_FILTER 0", "SET MASTER 0.5"):
                send(command)
            # Settle controls, then start in an audible half-cycle for Test Tone.
            render(150)
            pcm = render(10)
            send("PAD 1 1")
            pcm.extend(render(12))
            send("PAD 1 0")
            release = render(230)
            pcm.extend(release)
            name = f"{mode.lower()}_{label}"
            with wave.open(str(directory / f"{name}.wav"), "wb") as output:
                output.setnchannels(1)
                output.setsampwidth(2)
                output.setframerate(48000)
                output.writeframes(pcm.tobytes())
            results[name] = {
                "peak": max(abs(value) for value in pcm),
                "release_after_3_05ms_peak": max(abs(value) for value in release[146:]),
                "repeat_peaks": [
                    max(abs(value) for value in release[
                        round((repeat * 0.3 - 0.12) * 48000):
                        round((repeat * 0.3 + 0.004) * 48000)
                    ]) for repeat in range(1, 7)
                ],
            }
            if label == "dry":
                assert results[name]["release_after_3_05ms_peak"] == 0
            if label == "single":
                assert all(peak == 0 for peak in results[name]["repeat_peaks"][1:])
        send("SET ECHO_LEVEL 0")
        send("PAD 1 1")
        performance = render(220)
        send("PAD 1 0")
        performance.extend(render(50))
        with wave.open(str(directory / f"{mode.lower()}_performance.wav"), "wb") as output:
            output.setnchannels(1)
            output.setsampwidth(2)
            output.setframerate(48000)
            output.writeframes(performance.tobytes())
    (directory / "metrics.json").write_text(json.dumps(results, indent=2) + "\n")
    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=Path("artifacts/baseline"))
    args = parser.parse_args()
    print(json.dumps(capture(args.output_dir), indent=2))
