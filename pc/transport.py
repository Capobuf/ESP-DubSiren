import queue
import threading
from collections.abc import Callable

import serial

from protocol import PacketParser, TYPE_AUDIO, TYPE_COMMAND_RESULT, decode_status


class SerialTransport:
    def __init__(
        self,
        audio_queue: queue.Queue[bytes],
        status_callback: Callable[[dict], None] | None = None,
        command_callback: Callable[[dict], None] | None = None,
    ) -> None:
        self._audio_queue = audio_queue
        self._status_callback = status_callback
        self._command_callback = command_callback
        self._serial: serial.Serial | None = None
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self._write_lock = threading.Lock()
        self.error: Exception | None = None

    @property
    def connected(self) -> bool:
        return self._serial is not None and self._serial.is_open and not self._stop.is_set()

    def connect(self, port: str) -> None:
        if self.connected:
            raise RuntimeError("already connected")
        self.error = None
        try:
            self._serial = serial.Serial(port, 115200, timeout=0.05, write_timeout=1)
            self._serial.dtr = False
            self._serial.rts = False
            self._serial.reset_input_buffer()
            self._stop.clear()
            self._thread = threading.Thread(target=self._reader, daemon=True)
            self._thread.start()
            self.send("HELLO")
        except Exception:
            self.disconnect()
            raise

    def disconnect(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=1)
        if self._serial is not None:
            self._serial.close()
        self._serial = None
        self._thread = None
        self.error = None

    def send(self, command: str) -> None:
        if not self.connected:
            raise RuntimeError("not connected")
        data = (command + "\n").encode("ascii")
        with self._write_lock:
            assert self._serial is not None
            self._serial.write(data)

    def _reader(self) -> None:
        parser = PacketParser()
        assert self._serial is not None
        while not self._stop.is_set():
            try:
                data = self._serial.read(4096)
            except (serial.SerialException, OSError) as exc:
                self.error = exc
                self._stop.set()
                break
            for packet in parser.feed(data):
                if packet.packet_type == TYPE_AUDIO:
                    self._put_latest(packet.payload)
                else:
                    try:
                        status = decode_status(packet)
                    except (ValueError, UnicodeDecodeError):
                        continue
                    if packet.packet_type == TYPE_COMMAND_RESULT:
                        if self._command_callback is not None:
                            self._command_callback(status["commandResult"])
                    elif self._status_callback is not None:
                        self._status_callback(status)

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
