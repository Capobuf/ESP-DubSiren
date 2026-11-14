"""Exercise real Tk widgets; no audio device or ESP board is required."""
import queue
import gc
import threading
import time
import tkinter as tk
import unittest
from types import SimpleNamespace
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
        self._status_callback({"protocol": 1, "i2sReady": True, "firmware": "test"})

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
        self.errors = patch("app.messagebox.showerror").start()
        # No board is attached: list one serial port so connect() has a device.
        patch("app.list_ports.comports", return_value=[SimpleNamespace(device="COM_TEST")]).start()
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
        with patch("app.SerialTransport", side_effect=transport.factory):
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
        failed.failure = RuntimeError("port unavailable")
        failed.release.set()
        with patch("app.SerialTransport", side_effect=failed.factory):
            self.app.connect()
        self.pump_until(lambda: not self.app._connecting)
        self.assertIn("port unavailable", self.app.connection_var.get())
        self.assertTrue(self.app.connect_button.instate(["!disabled"]))
        successful = SlowTransport(queue.Queue(), None)
        successful.release.set()
        with patch("app.SerialTransport", side_effect=successful.factory):
            self.app.connect()
        self.pump_until(lambda: self.app.protocol_ok)

    def test_close_during_connect_cleans_up_late_connection(self):
        self.app = PhysicalDubSirenApp(self.root)
        transport = SlowTransport(queue.Queue(), None)
        with patch("app.SerialTransport", side_effect=transport.factory):
            self.app.connect()
        self.app.close()
        self.closed = True
        transport.release.set()
        self.assertTrue(transport.closed.wait(2))
        self.assertFalse(transport.connected)

    def test_pot_keyboard_and_focus_scroll_at_125_percent(self):
        self.check_layout_and_keys(1.25)

    def test_shift_and_caps_lock_releases_with_real_tk(self):
        self.app = PhysicalDubSirenApp(self.root)
        sent = []
        self.app.transport = SimpleNamespace(connected=True, send=sent.append,
                                             disconnect=lambda: None)
        self.app._send = sent.append
        self.root.focus_force()
        self.root.update()
        # Shift turns the release keysym of "1" into "exclam"; Caps Lock "e" into "E".
        for press, release, expected in (
            (("1", 49), ("exclam", 49), ["PAD 1 1", "PAD 1 0"]),
            (("e", 69), ("E", 69), ["ENCODER_SWITCH 1", "ENCODER_SWITCH 0"]),
        ):
            sent.clear()
            self.root.event_generate("<KeyPress>", keysym=press[0], keycode=press[1], when="now")
            self.root.event_generate("<KeyRelease>", keysym=release[0], keycode=release[1], when="now")
            self.root.update()
            self.assertEqual(sent, expected)

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
