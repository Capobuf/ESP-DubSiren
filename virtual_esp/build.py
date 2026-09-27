"""Build the host VirtualESP library from the firmware DSP sources."""

import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "virtual_esp" / "bin"

SOURCES = [
    ROOT / "virtual_esp" / "virtual_esp.cpp",
    ROOT / "firmware" / "src" / "audio" / "AudioEngine.cpp",
    ROOT / "firmware" / "src" / "audio" / "Biquad.cpp",
    ROOT / "firmware" / "src" / "audio" / "Delay.cpp",
    ROOT / "firmware" / "src" / "audio" / "Envelope.cpp",
    ROOT / "firmware" / "src" / "audio" / "Lfo.cpp",
    ROOT / "firmware" / "src" / "audio" / "Oscillator.cpp",
    ROOT / "firmware" / "src" / "control" / "CommandParser.cpp",
]


def library_path() -> Path:
    if sys.platform == "win32":
        return BUILD_DIR / "virtual_esp.dll"
    if sys.platform == "darwin":
        return BUILD_DIR / "libvirtual_esp.dylib"
    return BUILD_DIR / "libvirtual_esp.so"


def compiler_command() -> list[str]:
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
                "No host C++ compiler found. Run pc\\setup.bat to install ziglang."
            )
        return [compiler]

    return [sys.executable, "-m", "ziglang", "c++"]


def dependencies() -> list[Path]:
    paths = list(SOURCES)
    paths.append(Path(__file__))
    paths.append(ROOT / "tests" / "native" / "stubs" / "Arduino.h")
    paths.append(ROOT / "firmware" / "include" / "Config.h")
    paths.extend((ROOT / "firmware" / "src" / "audio").glob("*.h"))
    paths.extend((ROOT / "firmware" / "src" / "control").glob("*.h"))
    return paths


def needs_build(output: Path) -> bool:
    if not output.exists():
        return True
    output_mtime = output.stat().st_mtime
    return any(path.stat().st_mtime > output_mtime for path in dependencies())


def main() -> None:
    output = library_path()
    if "--if-needed" in sys.argv and not needs_build(output):
        print(output)
        return

    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    command = compiler_command() + [
        "-std=c++17",
        "-O2",
        "-shared",
        "-w",
        "-Itests/native/stubs",
        "-Ifirmware/src",
        "-Ifirmware/include",
        *[str(path.relative_to(ROOT)) for path in SOURCES],
        "-o",
        str(output),
    ]
    if sys.platform != "win32":
        command.insert(len(compiler_command()), "-fPIC")

    subprocess.run(command, cwd=ROOT, check=True)
    print(output)


if __name__ == "__main__":
    main()
