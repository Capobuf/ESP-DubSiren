"""Build and run the DSP regression test with Zig C++ or a local C++ compiler."""

import os
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
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


def compiler_command() -> list[str]:
    # CXX may hold arguments.
    if os.environ.get("CXX"):
        return shlex.split(os.environ["CXX"])

    try:
        import ziglang  # noqa: F401
    except ImportError:
        compiler = (
            shutil.which("c++")
            or shutil.which("g++")
            or shutil.which("clang++")
        )
        if compiler is None:
            raise SystemExit(
                "No host C++ compiler found. Set CXX or run "
                "python -m pip install -r tests/native/requirements.txt."
            )
        return [compiler]

    return [sys.executable, "-m", "ziglang", "c++"]


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="dubsiren-native-") as directory:
        executable = Path(directory) / ("dsp_test.exe" if os.name == "nt" else "dsp_test")
        command = compiler_command() + [
            "-w", "-std=c++17", "-O2",
            "-IAndroid/app/src/main/cpp/stubs", "-Ifirmware/src", "-Ifirmware/include",
            *SOURCES, "-o", str(executable),
        ]
        subprocess.run(command, cwd=ROOT, check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
