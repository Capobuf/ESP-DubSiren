import unittest

from app import DubSirenApp


class FakeVariable:
    def __init__(self, value):
        self.value = value

    def get(self):
        return self.value

    def set(self, value):
        self.value = value


class FakePlayer:
    def __init__(self):
        self.starts = 0
        self.stops = 0
        self.error = None

    def start(self):
        self.starts += 1

    def stop(self):
        self.stops += 1


class FakeWidget:
    def __init__(self):
        self.config = {}

    def configure(self, **kwargs):
        self.config.update(kwargs)


class FakeRoot:
    def __init__(self):
        self.scheduled = []

    def after(self, delay, callback):
        self.scheduled.append((delay, callback))


class FakeTransport:
    connected = True


def make_app(output: str) -> DubSirenApp:
    app = object.__new__(DubSirenApp)
    app.output_var = FakeVariable(output)
    app.i2s_ready = True
    app.protocol_ok = True
    app.streaming = False
    app.active_output = None
    app.transport = FakeTransport()
    app.player = FakePlayer()
    app.audio_button = FakeWidget()
    app.root = FakeRoot()
    app.commands = []
    app.drains = 0
    app._send = app.commands.append

    def drain():
        app.drains += 1

    app._drain_audio_queue = drain
    return app


class OutputRoutingTests(unittest.TestCase):
    def test_gpio_start_does_not_start_pc_player(self):
        app = make_app("PCM5102A")

        app.start_audio()

        self.assertEqual(app.player.starts, 0)
        self.assertEqual(app.commands, ["SET OUTPUT GPIO", "STREAM 1"])
        self.assertTrue(app.streaming)

    def test_both_start_starts_pc_player(self):
        app = make_app("Both")

        app.start_audio()

        self.assertEqual(app.player.starts, 1)
        self.assertEqual(app.commands, ["SET OUTPUT BOTH", "STREAM 1"])

    def test_live_gpio_to_pc_starts_player_before_routing(self):
        app = make_app("PC")
        app.streaming = True
        app.active_output = "GPIO"

        app._output_changed()

        self.assertEqual(app.player.starts, 1)
        self.assertEqual(app.commands, ["SET OUTPUT PC"])
        self.assertEqual(app.drains, 1)
        self.assertEqual(len(app.root.scheduled), 1)

    def test_live_pc_to_gpio_stops_player_and_drains_queue(self):
        app = make_app("PCM5102A")
        app.streaming = True
        app.active_output = "PC"

        app._output_changed()

        self.assertEqual(app.commands, ["SET OUTPUT GPIO"])
        self.assertEqual(app.player.stops, 1)
        self.assertEqual(app.drains, 1)


if __name__ == "__main__":
    unittest.main()
