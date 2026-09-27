import ctypes
import queue
import subprocess
import sys
import threading
import time
from collections.abc import Callable
from pathlib import Path


VIRTUAL_DEVICE = "Virtual ESP"
ROOT = Path(__file__).resolve().parents[1]
BUILD_SCRIPT = ROOT / "virtual_esp" / "build.py"


def _library_path() -> Path:
    build_dir = ROOT / "virtual_esp" / "bin"
    if sys.platform == "win32":
        return build_dir / "virtual_esp.dll"
    if sys.platform == "darwin":
        return build_dir / "libvirtual_esp.dylib"
    return build_dir / "libvirtual_esp.so"


class VirtualTransport:
    def __init__(
        self,
        audio_queue: queue.Queue[bytes],
        status_callback: Callable[[dict], None] | None = None,
    ) -> None:
        self._audio_queue = audio_queue
        self._status_callback = status_callback
        self._lib: ctypes.CDLL | None = None
        self._connected = False
        self._streaming = False
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self._lock = threading.Lock()
        self._sample_rate = 48_000
        self._block_samples = 480
        self._firmware_version = "?"

    @property
    def connected(self) -> bool:
        return self._connected

    def connect(self, port: str) -> None:
        if self.connected:
            raise RuntimeError("already connected")
        if port != VIRTUAL_DEVICE:
            raise ValueError(f"unsupported virtual device: {port}")

        self._build_if_needed()
        self._load_library()

        assert self._lib is not None
        if self._lib.virtual_esp_init() != 1:
            raise RuntimeError("Virtual ESP initialization failed")

        self._sample_rate = self._lib.virtual_esp_sample_rate()
        self._block_samples = self._lib.virtual_esp_block_samples()
        version = self._lib.virtual_esp_firmware_version()
        self._firmware_version = version.decode("ascii") if version else "?"

        self._connected = True
        self._streaming = False
        self._stop.clear()
        self._thread = threading.Thread(target=self._render_loop, daemon=True)
        self._thread.start()
        self.send("HELLO")

    def disconnect(self) -> None:
        self._connected = False
        self._streaming = False
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=1)
        self._thread = None

    def send(self, command: str) -> None:
        if not self.connected:
            raise RuntimeError("not connected")

        if command == "HELLO":
            if self._status_callback is not None:
                self._status_callback(self._status())
            return

        if command == "STREAM 1":
            self._streaming = True
            return
        if command == "STREAM 0":
            self._streaming = False
            return
        if command.startswith("SET OUTPUT "):
            return

        assert self._lib is not None
        with self._lock:
            previous_revision = self._lib.virtual_esp_panel_revision()
            accepted = self._lib.virtual_esp_command(command.encode("ascii"))
        if accepted != 1:
            raise RuntimeError(f"Virtual ESP rejected command: {command}")
        if (
            self._status_callback is not None
            and self._lib.virtual_esp_panel_revision() != previous_revision
        ):
            self._status_callback(self._status())

    def _build_if_needed(self) -> None:
        result = subprocess.run(
            [sys.executable, str(BUILD_SCRIPT), "--if-needed"],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            details = (result.stderr or result.stdout).strip()
            raise RuntimeError(f"Virtual ESP build failed:\n{details}")

    def _load_library(self) -> None:
        library = ctypes.CDLL(str(_library_path()))
        library.virtual_esp_init.argtypes = []
        library.virtual_esp_init.restype = ctypes.c_int
        library.virtual_esp_command.argtypes = [ctypes.c_char_p]
        library.virtual_esp_command.restype = ctypes.c_int
        library.virtual_esp_render.argtypes = [
            ctypes.POINTER(ctypes.c_int16),
            ctypes.c_size_t,
        ]
        library.virtual_esp_render.restype = ctypes.c_int
        library.virtual_esp_firmware_version.argtypes = []
        library.virtual_esp_firmware_version.restype = ctypes.c_char_p
        library.virtual_esp_protocol_version.argtypes = []
        library.virtual_esp_protocol_version.restype = ctypes.c_int
        library.virtual_esp_sample_rate.argtypes = []
        library.virtual_esp_sample_rate.restype = ctypes.c_int
        library.virtual_esp_block_samples.argtypes = []
        library.virtual_esp_block_samples.restype = ctypes.c_int
        library.virtual_esp_panel_revision.argtypes = []
        library.virtual_esp_panel_revision.restype = ctypes.c_uint
        library.virtual_esp_panel_menu_revision.argtypes = []
        library.virtual_esp_panel_menu_revision.restype = ctypes.c_uint
        library.virtual_esp_selected_pad.argtypes = []
        library.virtual_esp_selected_pad.restype = ctypes.c_int
        library.virtual_esp_current_pad.argtypes = []
        library.virtual_esp_current_pad.restype = ctypes.c_int
        library.virtual_esp_active_pad.argtypes = []
        library.virtual_esp_active_pad.restype = ctypes.c_int
        library.virtual_esp_pad_sound.argtypes = [ctypes.c_uint]
        library.virtual_esp_pad_sound.restype = ctypes.c_char_p
        library.virtual_esp_pad_variant_name.argtypes = [ctypes.c_uint]
        library.virtual_esp_pad_variant_name.restype = ctypes.c_char_p
        library.virtual_esp_pad_variant.argtypes = [ctypes.c_uint]
        library.virtual_esp_pad_variant.restype = ctypes.c_uint
        library.virtual_esp_pad_hold.argtypes = [ctypes.c_uint]
        library.virtual_esp_pad_hold.restype = ctypes.c_int
        library.virtual_esp_pad_hold_supported.argtypes = [ctypes.c_uint]
        library.virtual_esp_pad_hold_supported.restype = ctypes.c_int
        self._lib = library

    def _status(self) -> dict:
        assert self._lib is not None
        pads = []
        for pad in range(1, 5):
            sound = self._lib.virtual_esp_pad_sound(pad)
            variant = self._lib.virtual_esp_pad_variant_name(pad)
            pads.append(
                {
                    "sound": sound.decode("ascii") if sound else "UNKNOWN",
                    "variant": variant.decode("ascii") if variant else "UNKNOWN",
                    "variantIndex": self._lib.virtual_esp_pad_variant(pad),
                    "hold": bool(self._lib.virtual_esp_pad_hold(pad)),
                    "holdSupported": bool(
                        self._lib.virtual_esp_pad_hold_supported(pad)
                    ),
                }
            )
        return {
            "name": "DubSiren",
            "protocol": self._lib.virtual_esp_protocol_version(),
            "sampleRate": self._sample_rate,
            "blockSamples": self._block_samples,
            "firmware": self._firmware_version,
            "output": "PC",
            "i2sReady": False,
            "virtual": True,
            "panel": {
                "revision": self._lib.virtual_esp_panel_revision(),
                "menuRevision": self._lib.virtual_esp_panel_menu_revision(),
                "selectedPad": self._lib.virtual_esp_selected_pad(),
                "currentPad": self._lib.virtual_esp_current_pad(),
                "activePad": self._lib.virtual_esp_active_pad(),
                "pads": pads,
            },
        }

    def _render_loop(self) -> None:
        assert self._lib is not None
        buffer_type = ctypes.c_int16 * self._block_samples
        buffer = buffer_type()
        period = self._block_samples / self._sample_rate
        deadline = time.perf_counter()

        while not self._stop.is_set():
            with self._lock:
                rendered = self._lib.virtual_esp_render(
                    buffer, self._block_samples
                )
                streaming = self._streaming
            if rendered != 1:
                self._connected = False
                return

            if streaming:
                pcm = ctypes.string_at(
                    ctypes.addressof(buffer),
                    self._block_samples * ctypes.sizeof(ctypes.c_int16),
                )
                self._put_latest(pcm)

            deadline += period
            remaining = deadline - time.perf_counter()
            if remaining > 0:
                self._stop.wait(remaining)
            elif remaining < -period:
                deadline = time.perf_counter()

    def _put_latest(self, pcm: bytes) -> None:
        try:
            self._audio_queue.put_nowait(pcm)
        except queue.Full:
            try:
                self._audio_queue.get_nowait()
            except queue.Empty:
                pass
            try:
                self._audio_queue.put_nowait(pcm)
            except queue.Full:
                pass
