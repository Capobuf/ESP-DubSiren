import queue
import threading

import sounddevice

SAMPLE_RATE = 48_000
BLOCK_SAMPLES = 480
SILENCE = bytes(BLOCK_SAMPLES * 2)


class AudioPlayer:
    def __init__(self, audio_queue: queue.Queue[bytes]) -> None:
        self._audio_queue = audio_queue
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self.error: Exception | None = None

    @property
    def running(self) -> bool:
        return self._thread is not None and self._thread.is_alive()

    def start(self) -> None:
        if self.running:
            return
        self.error = None
        self._stop.clear()
        self._thread = threading.Thread(target=self._play, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=2)
        self._thread = None

    def _play(self) -> None:
        try:
            with sounddevice.RawOutputStream(
                samplerate=SAMPLE_RATE,
                channels=1,
                dtype="int16",
                blocksize=BLOCK_SAMPLES,
            ) as stream:
                while not self._stop.is_set():
                    try:
                        pcm = self._audio_queue.get(timeout=0.01)
                    except queue.Empty:
                        pcm = SILENCE
                    stream.write(pcm)
        except Exception as exc:
            self.error = exc
