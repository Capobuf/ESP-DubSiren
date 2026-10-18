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
        library.virtual_esp_dropped_activations.argtypes = []
        library.virtual_esp_dropped_activations.restype = ctypes.c_uint
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
        library.virtual_esp_display_width.argtypes = []
        library.virtual_esp_display_width.restype = ctypes.c_uint
        library.virtual_esp_display_height.argtypes = []
        library.virtual_esp_display_height.restype = ctypes.c_uint
        library.virtual_esp_display_mode.argtypes = []
        library.virtual_esp_display_mode.restype = ctypes.c_char_p
        library.virtual_esp_menu_index.argtypes = []
        library.virtual_esp_menu_index.restype = ctypes.c_uint
        library.virtual_esp_menu_editing.argtypes = []
        library.virtual_esp_menu_editing.restype = ctypes.c_int
        library.virtual_esp_menu_item_count.argtypes = []
        library.virtual_esp_menu_item_count.restype = ctypes.c_uint
        library.virtual_esp_menu_item_key.argtypes = [ctypes.c_uint]
        library.virtual_esp_menu_item_key.restype = ctypes.c_char_p
        for name in (
            "menu_item_label", "menu_item_value",
        ):
            function = getattr(library, "virtual_esp_" + name)
            function.argtypes = [ctypes.c_uint]
            function.restype = ctypes.c_char_p
        for name in (
            "menu_title", "menu_footer", "overlay_title", "overlay_value",
            "pot_behavior", "encoder_action",
            "echo_filter_name",
        ):
            function = getattr(library, "virtual_esp_" + name)
            function.argtypes = []
            function.restype = ctypes.c_char_p
        for name in (
            "settings_revision", "settings_schema", "encoder_step", "encoder_range",
        ):
            function = getattr(library, "virtual_esp_" + name)
            function.argtypes = []
            function.restype = ctypes.c_uint
        for name in ("pot_target", "pot_mode", "button_action", "button_sound"):
            function = getattr(library, "virtual_esp_" + name)
            function.argtypes = [ctypes.c_uint, ctypes.c_uint]
            function.restype = ctypes.c_char_p
        for name in ("pot_min", "pot_max"):
            function = getattr(library, "virtual_esp_" + name)
            function.argtypes = [ctypes.c_uint, ctypes.c_uint]
            function.restype = ctypes.c_float
        library.virtual_esp_pot_zone.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_uint]
        library.virtual_esp_pot_zone.restype = ctypes.c_float
        library.virtual_esp_button_variant.argtypes = [ctypes.c_uint, ctypes.c_uint]
        library.virtual_esp_button_variant.restype = ctypes.c_uint
        library.virtual_esp_button_latch.argtypes = [ctypes.c_uint, ctypes.c_uint]
        library.virtual_esp_button_latch.restype = ctypes.c_int
        library.virtual_esp_depth.argtypes = []
        library.virtual_esp_depth.restype = ctypes.c_float
        library.virtual_esp_echo_loop.argtypes = []
        library.virtual_esp_echo_loop.restype = ctypes.c_int
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
        for name in ("profile", "voice", "pitch", "rate"):
            function = getattr(library, "virtual_esp_" + name)
            function.argtypes = []
            function.restype = ctypes.c_char_p
        for name in (
            "tune", "rate_hz", "pitch_position", "rate_position",
            "echo_level", "delay_ms", "feedback", "echo_filter", "master",
        ):
            function = getattr(library, "virtual_esp_" + name)
            function.argtypes = []
            function.restype = ctypes.c_float
        for name in ("hold", "echo_cut"):
            function = getattr(library, "virtual_esp_" + name)
            function.argtypes = []
            function.restype = ctypes.c_int
        library.virtual_esp_live_transpose.argtypes = []
        library.virtual_esp_live_transpose.restype = ctypes.c_int
        for name in ("pitch_zones_active", "rate_zones_active"):
            function = getattr(library, "virtual_esp_" + name)
            function.argtypes = []
            function.restype = ctypes.c_int
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
        menu_items = []
        menu_rows = []
        for index in range(self._lib.virtual_esp_menu_item_count()):
            key = self._lib.virtual_esp_menu_item_key(index)
            label = self._lib.virtual_esp_menu_item_label(index)
            value = self._lib.virtual_esp_menu_item_value(index)
            decoded_key = key.decode("ascii") if key else "BACK"
            menu_items.append(decoded_key)
            menu_rows.append({
                "key": decoded_key,
                "label": label.decode("ascii") if label else "BACK",
                "value": value.decode("ascii") if value else "",
            })
        profiles = {}
        for profile_index, profile_name in enumerate(("CLASSIC", "EXTENDED")):
            pots = []
            for pot in range(1, 7):
                target = self._lib.virtual_esp_pot_target(profile_index, pot)
                pot_mode = self._lib.virtual_esp_pot_mode(profile_index, pot)
                pots.append({
                    "target": target.decode("ascii"),
                    "mode": pot_mode.decode("ascii"),
                    "min": self._lib.virtual_esp_pot_min(profile_index, pot),
                    "max": self._lib.virtual_esp_pot_max(profile_index, pot),
                    "zones": [self._lib.virtual_esp_pot_zone(profile_index, pot, zone)
                              for zone in range(3)],
                })
            buttons = []
            for button in range(1, 5):
                action = self._lib.virtual_esp_button_action(profile_index, button)
                sound = self._lib.virtual_esp_button_sound(profile_index, button)
                buttons.append({
                    "action": action.decode("ascii"),
                    "sound": sound.decode("ascii"),
                    "variant": self._lib.virtual_esp_button_variant(profile_index, button),
                    "latch": bool(self._lib.virtual_esp_button_latch(profile_index, button)),
                })
            profiles[profile_name] = {"pots": pots, "buttons": buttons}
        behavior = self._lib.virtual_esp_pot_behavior()
        encoder_action = self._lib.virtual_esp_encoder_action()
        settings = {
            "schema": self._lib.virtual_esp_settings_schema(),
            "revision": self._lib.virtual_esp_settings_revision(),
            "potBehavior": behavior.decode("ascii"),
            "encoder": {
                "holdTurn": encoder_action.decode("ascii"),
                "step": self._lib.virtual_esp_encoder_step(),
                "range": self._lib.virtual_esp_encoder_range(),
            },
            "profiles": profiles,
        }
        pitch_mode = "ZONES" if self._lib.virtual_esp_pitch_zones_active() else "CONTINUOUS"
        rate_mode = "ZONES" if self._lib.virtual_esp_rate_zones_active() else "CONTINUOUS"
        mode = self._lib.virtual_esp_display_mode()
        return {
            "name": "DubSiren",
            "protocol": self._lib.virtual_esp_protocol_version(),
            "sampleRate": self._sample_rate,
            "blockSamples": self._block_samples,
            "firmware": self._firmware_version,
            "output": "PC",
            "i2sReady": False,
            "virtual": True,
            "droppedActivations": self._lib.virtual_esp_dropped_activations(),
            "panel": {
                "revision": self._lib.virtual_esp_panel_revision(),
                "settings": settings,
                "persistenceStatus": "",
                "menuRevision": self._lib.virtual_esp_panel_menu_revision(),
                "selectedPad": self._lib.virtual_esp_selected_pad(),
                "currentPad": self._lib.virtual_esp_current_pad(),
                "activePad": self._lib.virtual_esp_active_pad(),
                "pads": pads,
                "display": {
                    "width": self._lib.virtual_esp_display_width(),
                    "height": self._lib.virtual_esp_display_height(),
                    "mode": mode.decode("ascii") if mode else "HOME",
                    "menuIndex": self._lib.virtual_esp_menu_index(),
                    "editing": bool(self._lib.virtual_esp_menu_editing()),
                    "menuItems": menu_items,
                    "rows": menu_rows,
                    "title": self._lib.virtual_esp_menu_title().decode("ascii"),
                    "footer": self._lib.virtual_esp_menu_footer().decode("ascii"),
                    "overlay": {
                        "title": self._lib.virtual_esp_overlay_title().decode("ascii"),
                        "value": self._lib.virtual_esp_overlay_value().decode("ascii"),
                    },
                    "buttons": [
                        button["action"]
                        for button in profiles[
                            self._lib.virtual_esp_profile().decode("ascii")
                        ]["buttons"]
                    ],
                    "profile": self._lib.virtual_esp_profile().decode("ascii"),
                    "voice": self._lib.virtual_esp_voice().decode("ascii"),
                    "pitch": self._lib.virtual_esp_pitch().decode("ascii"),
                    "rate": self._lib.virtual_esp_rate().decode("ascii"),
                    "pitchMode": pitch_mode,
                    "rateMode": rate_mode,
                    "hold": bool(self._lib.virtual_esp_hold()),
                    "echoCut": bool(self._lib.virtual_esp_echo_cut()),
                    "tuneHz": self._lib.virtual_esp_tune(),
                    "rateHz": self._lib.virtual_esp_rate_hz(),
                    "liveTranspose": self._lib.virtual_esp_live_transpose(),
                    "pitchPosition": self._lib.virtual_esp_pitch_position(),
                    "ratePosition": self._lib.virtual_esp_rate_position(),
                    "echoLevel": self._lib.virtual_esp_echo_level(),
                    "delayMs": self._lib.virtual_esp_delay_ms(),
                    "feedback": self._lib.virtual_esp_feedback(),
                    "echoFilter": self._lib.virtual_esp_echo_filter(),
                    "filter": self._lib.virtual_esp_echo_filter_name().decode("ascii"),
                    "master": self._lib.virtual_esp_master(),
                    "depth": self._lib.virtual_esp_depth(),
                    "echoLoop": bool(self._lib.virtual_esp_echo_loop()),
                },
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
