"""Render shared-core comparisons without an ESP; abort on the first failed command."""

import argparse
import array
import ctypes
import json
import math
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import wave

from virtual_transport import VirtualTransport, BUILD_SCRIPT, ROOT


def main():
    arguments = argparse.ArgumentParser()
    arguments.add_argument("--output-dir", type=Path)
    options = arguments.parse_args()
    output = options.output_dir or Path(tempfile.mkdtemp(prefix="dubsiren-offline-"))
    output.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(BUILD_SCRIPT), "--if-needed"], cwd=ROOT, check=True)
    transport = VirtualTransport(queue.Queue())
    transport._load_library()
    lib = transport._lib
    block = (ctypes.c_int16 * 480)()

    def command(text):
        if lib.virtual_esp_command(text.encode("ascii")) != 1:
            raise RuntimeError(f"Rejected: {text}")

    scenarios = []
    for articulation, rate, release in (("SHOT", 2, 1), ("SHOT", 2, 200),
                                        ("ROLL", 2, 200), ("ROLL", 8, 200), ("HORN", 2, 200)):
        scenarios.append((f"lickshot-{articulation.lower()}-{rate}hz-release{release}",
                          ["SET MODE LICKSHOT", f"SET LICKSHOT_MODE {articulation}",
                           f"SET LFO_RATE_HZ {rate}"], {release: ["TRIGGER 0"]}))
    scenarios.append(("lickshot-close-hits", ["SET MODE LICKSHOT", "SET LICKSHOT_MODE SHOT"],
                      {1: ["TRIGGER 0"], 8: ["TRIGGER 1"], 9: ["TRIGGER 0"],
                       16: ["TRIGGER 1"], 17: ["TRIGGER 0"]}))
    for shape in ("CLASSIC", "TRIANGLE", "SQUARE"):
        scenarios.append((f"two-tone-{shape.lower()}", ["SET MODE SQUARE", f"SET LFO_SHAPE {shape}",
                          "SET LFO_RATE_HZ 2", "MODULATION 1"], {200: ["TRIGGER 0"]}))
    for voice in ("SINE1", "SINE2", "SQUARE", "LICKSHOT"):
        for pitch in (80, 220, 1600):
            for tone in (19000, 700):
                scenarios.append((f"tone-{voice.lower()}-{pitch}hz-{tone}",
                                  [f"SET MODE {voice}", f"SET TUNE_HZ {pitch}",
                                   f"SET DRY_TONE_HZ {tone}"], {100: ["TRIGGER 0"]}))
    for contour in (-3, 3):
        scenarios.append((f"contour-{contour}", ["SET DRY_TONE_HZ 1200",
                          f"SET FILTER_CONTOUR_OCT {contour}", "SET FILTER_CONTOUR_MS 300"],
                          {40: ["TRIGGER 0"]}))
    for color in (0, 1):
        scenarios.append((f"echo-color-{color}", ["SET ECHO_LEVEL 1", "SET DELAY_MS 120",
                          "SET FEEDBACK 0.8", f"SET ECHO_COLOR {color}"],
                          {20: ["TRIGGER 0"], 80: ["ECHO_CUT 1"], 100: ["ECHO_CUT 0"]}))

    renders = []
    for name, settings, events in scenarios:
        if lib.virtual_esp_init() != 1:
            raise RuntimeError("Core initialization failed")
        for text in ["SET PROFILE EXTENDED", "SET ECHO_LEVEL 0", "SET MASTER 0.5",
                     "SET TUNE_HZ 220", "MODULATION 0", *settings]:
            command(text)
        for _ in range(100):
            if lib.virtual_esp_render(block, 480) != 1:
                raise RuntimeError("Render failed")
        command("TRIGGER 1")
        samples = array.array("h")
        for index in range(300):
            for text in events.get(index, []):
                command(text)
            if lib.virtual_esp_render(block, 480) != 1:
                raise RuntimeError("Render failed")
            samples.extend(block)
        peak = max(abs(value) for value in samples)
        active = [value for value in samples if abs(value) > peak * .01]
        if not active:
            raise RuntimeError(f"Silent comparison: {name}")
        rms = math.sqrt(sum(value * value for value in active) / len(active))
        renders.append((name, samples, peak, rms))

    # One common active-sample RMS, low enough that every file stays below -1 dBFS.
    target = min(4915.0, *(rms * 29203 / peak for _, _, peak, rms in renders))
    metrics = {"sampleRate": 48000, "commonActiveRms": target,
               "normalization": "RMS of samples above 1% of each raw peak; no clipping",
               "listeningValidated": False, "hardwareValidated": False, "files": []}
    for name, samples, peak, rms in renders:
        gain = target / rms
        normalized = array.array("h", (round(value * gain) for value in samples))
        with wave.open(str(output / f"{name}.wav"), "wb") as wav:
            wav.setnchannels(1)
            wav.setsampwidth(2)
            wav.setframerate(48000)
            wav.writeframes(normalized.tobytes())
        metrics["files"].append({"name": name, "rawPeak": peak, "rawActiveRms": rms,
                                 "rawMean": sum(samples) / len(samples), "gain": gain,
                                 "normalizedPeak": max(abs(value) for value in normalized)})
    (output / "metrics.json").write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
    print(f"{len(renders)} WAV comparisons and metrics: {output}")


if __name__ == "__main__":
    main()
