"""Build and run the DSP regression test with Zig C++ or a local C++ compiler."""

import importlib.util
import os
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
# One compiler lookup for tests and Virtual ESP (CXX may hold arguments).
_spec = importlib.util.spec_from_file_location("virtual_esp_build", ROOT / "virtual_esp" / "build.py")
_build = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_build)
SOURCES = [
    "tests/native/dsp_test.cpp",
    "firmware/src/audio/AudioEngine.cpp",
    "firmware/src/audio/Biquad.cpp",
    "firmware/src/audio/Delay.cpp",
    "firmware/src/audio/Envelope.cpp",
    "firmware/src/audio/Lfo.cpp",
    "firmware/src/audio/Oscillator.cpp",
    "firmware/src/control/CommandParser.cpp",
    "firmware/src/control/PerformanceController.cpp",
]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="dubsiren-native-") as directory:
        executable = Path(directory) / ("dsp_test.exe" if os.name == "nt" else "dsp_test")
        command = _build.compiler_command() + [
            "-w", "-std=c++17", "-O2",
            "-Itests/native/stubs", "-Ifirmware/src", "-Ifirmware/include",
            *SOURCES, "-o", str(executable),
        ]
        subprocess.run(command, cwd=ROOT, check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
