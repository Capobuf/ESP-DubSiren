"""Exercise real Tk widgets; no audio device or ESP board is required."""
import queue
import gc
import tempfile
import threading
import time
import tkinter as tk
import unittest
from pathlib import Path
from unittest.mock import patch

from physical_ui import PhysicalDubSirenApp


class SlowTransport:
    def __init__(self, _audio, callback):
        self._status_callback = callback
        self._command_callback = None
        self.connected = False
        self.release = threading.Event()
        self.closed = threading.Event()
        self.failure = None

    def factory(self, _audio, status_callback, command_callback):
        self._status_callback = status_callback
        self._command_callback = command_callback
        return self

    def connect(self, _port):
        if not self.release.wait(3):
            raise RuntimeError("test connection timed out")
        if self.failure:
            raise self.failure
        self.connected = True
        self._status_callback({"protocol": 1, "virtual": True, "firmware": "test"})

    def send(self, command):
        if self._command_callback:
            self._command_callback({"command": command, "accepted": True})

    def disconnect(self):
        self.connected = False
        self.closed.set()


class TkRuntimeTests(unittest.TestCase):
    def setUp(self):
        try:
            self.root = tk.Tk()
        except tk.TclError as exc:
            self.skipTest(f"Tk display unavailable: {exc}")
        self.temporary = tempfile.TemporaryDirectory()
        self.config = patch("physical_ui.panel_config_path", return_value=Path(self.temporary.name) / "panel.json")
        self.config.start()
        self.errors = patch("app.messagebox.showerror").start()
        self.addCleanup(patch.stopall)
        self.app = None
        self.closed = False

    def tearDown(self):
        if self.closed:
            pass
        elif self.app and self.root.winfo_exists():
            self.app.close()
        elif self.root.winfo_exists():
            self.root.destroy()
        self.config.stop()
        self.temporary.cleanup()
        self.app = None
        self.root = None
        # Collect destroyed Tcl interpreters on their owner thread before a
        # subsequent test starts a transport worker that may trigger Python GC.
        gc.collect()

    def pump_until(self, condition, timeout=2):
        deadline = time.monotonic() + timeout
        while not condition() and time.monotonic() < deadline:
            self.root.update()
            time.sleep(0.005)
        self.assertTrue(condition())

    def test_slow_connect_keeps_tk_alive_and_delivers_handshake(self):
        self.app = PhysicalDubSirenApp(self.root)
        transport = SlowTransport(queue.Queue(), None)
        with patch("app.VirtualTransport", side_effect=transport.factory):
            self.app.connect()
            self.app.connect()  # Duplicate input must not start another worker.
        tick = []
        self.root.after(20, lambda: tick.append(True))
        self.pump_until(lambda: bool(tick))
        self.assertTrue(self.app._connecting)
        self.assertTrue(self.app.connect_button.instate(["disabled"]))
        transport.release.set()
        self.pump_until(lambda: self.app.protocol_ok)
        self.assertEqual(self.app.connect_button.cget("text"), "Disconnect")
        self.assertTrue(self.app.audio_button.instate(["!disabled"]))

    def test_failed_connect_can_be_retried(self):
        self.app = PhysicalDubSirenApp(self.root)
        failed = SlowTransport(queue.Queue(), None)
        failed.failure = RuntimeError("compiler unavailable")
        failed.release.set()
        with patch("app.VirtualTransport", side_effect=failed.factory):
            self.app.connect()
        self.pump_until(lambda: not self.app._connecting)
        self.assertIn("compiler unavailable", self.app.connection_var.get())
        self.assertTrue(self.app.connect_button.instate(["!disabled"]))
        successful = SlowTransport(queue.Queue(), None)
        successful.release.set()
        with patch("app.VirtualTransport", side_effect=successful.factory):
            self.app.connect()
        self.pump_until(lambda: self.app.protocol_ok)

    def test_close_during_build_cleans_up_late_connection(self):
        self.app = PhysicalDubSirenApp(self.root)
        transport = SlowTransport(queue.Queue(), None)
        with patch("app.VirtualTransport", side_effect=transport.factory):
            self.app.connect()
        self.app.close()
        self.closed = True
        transport.release.set()
        self.assertTrue(transport.closed.wait(2))
        self.assertFalse(transport.connected)

    def test_pot_keyboard_and_focus_scroll_at_125_percent(self):
        self.check_layout_and_keys(1.25)

    def test_virtual_feedback_and_pcm_with_real_tk(self):
        import array
        self.app = PhysicalDubSirenApp(self.root)
        self.app.connect()
        # CI compiles the shared core from scratch before the first connection.
        self.pump_until(lambda: self.app.protocol_ok, timeout=300)
        for command in ("TEMPO BPM 120", "TEMPO LFO SYNC", "TEMPO ECHO SYNC"):
            self.app._send(command)
        self.root.update()
        self.app._send("SET LFO_RATE_HZ 2")
        self.pump_until(lambda: self.app.command_var.get().startswith("Rejected:"))
        confirmed = self.app.confirmed_var.get()
        self.app.transport.send("HELLO")
        self.root.update()
        self.assertTrue(self.app.command_var.get().startswith("Rejected:"))
        self.assertEqual(self.app.confirmed_var.get(), confirmed)
        self.app._send("TEMPO ECHO FREE")
        self.app._send("SET DELAY_MS 400")
        self.pump_until(lambda: "Delay 400ms" in self.app.confirmed_var.get())
        self.assertIn("Accepted: SET DELAY_MS 400", self.app.command_var.get())
        self.app._send("TRIGGER 1")
        self.app._send("STREAM 1")
        self.pump_until(lambda: not self.app.audio_queue.empty())
        samples = array.array("h", self.app.audio_queue.get_nowait())
        self.assertTrue(any(samples))
        self.app._send("TRIGGER 0")
        self.app.disconnect()

    def test_pot_keyboard_and_focus_scroll_at_150_percent(self):
        self.check_layout_and_keys(1.5)

    def check_layout_and_keys(self, scale):
        self.root.tk.call("tk", "scaling", 96 / 72 * scale)
        self.app = PhysicalDubSirenApp(self.root)
        self.root.geometry("1366x720+0+0")  # Leave taskbar/title space on a 1366x768 screen.
        self.root.update()
        top = self.app.dials["POT_1"]
        bottom = self.app.dials["POT_4"]
        self.assertLess(top.winfo_rooty(), self.app.display.winfo_rooty())
        self.assertLess(self.app.display.winfo_rooty(), bottom.winfo_rooty())
        self.assertLess(bottom.winfo_rooty(), self.app.pad_buttons[0].winfo_rooty())
        dial = self.app.master_dial
        dial.canvas.focus_force()
        self.root.update()
        canvas = self.app.viewport.canvas
        self.assertGreaterEqual(dial.canvas.winfo_rooty(), canvas.winfo_rooty())
        self.assertLessEqual(dial.canvas.winfo_rooty() + dial.canvas.winfo_height(),
                             canvas.winfo_rooty() + canvas.winfo_height())
        dial.canvas.event_generate("<KeyPress-Home>")
        self.root.update()
        self.assertEqual(dial.value, dial.spec.minimum)
        dial.canvas.event_generate("<KeyPress-Up>")
        self.root.update()
        self.assertGreater(dial.value, dial.spec.minimum)
        dial.canvas.event_generate("<KeyPress-End>")
        self.root.update()
        self.assertEqual(dial.value, dial.spec.maximum)
        dial.set_active(False)
        dial.canvas.event_generate("<KeyPress-Home>")
        self.root.update()
        self.assertEqual(dial.value, dial.spec.maximum)
        self.assertEqual(str(dial.canvas.cget("takefocus")), "0")
        # At small windows both scrollbars retain access to the complete panel.
        self.root.geometry("640x480+0+0")
        self.root.update()
        self.assertLessEqual(self.app.connect_button.winfo_rootx() + self.app.connect_button.winfo_width(),
                             self.root.winfo_rootx() + self.root.winfo_width())
        canvas.yview_moveto(1)
        canvas.xview_moveto(1)
        self.root.update()
        self.assertAlmostEqual(canvas.yview()[1], 1)
        self.assertAlmostEqual(canvas.xview()[1], 1)


if __name__ == "__main__":
    unittest.main()
