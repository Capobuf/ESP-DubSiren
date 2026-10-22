import queue
import unittest
from unittest.mock import Mock, patch

import serial

from protocol import (
    AUDIO_PAYLOAD_BYTES, HEADER, MAGIC, PROTOCOL_VERSION, TYPE_AUDIO,
    TYPE_STATUS, TYPE_COMMAND_RESULT, PacketParser,
)
from transport import SerialTransport


def packet(kind, payload, length=None):
    return HEADER.pack(MAGIC, PROTOCOL_VERSION, kind,
                       len(payload) if length is None else length, 0) + payload


class TransportTests(unittest.TestCase):
    def test_results_are_separate_from_status_and_invalid_results_are_ignored(self):
        statuses, results = [], []
        transport = SerialTransport(queue.Queue(), statuses.append, results.append)
        port = Mock(is_open=True)
        data = (packet(TYPE_COMMAND_RESULT, b"\x01SET RATE_HZ 2") +
                packet(TYPE_COMMAND_RESULT, b"\x00SET DELAY_MS 400") +
                packet(TYPE_COMMAND_RESULT, b"\x02INVALID") +
                packet(TYPE_COMMAND_RESULT, b"\x01\xff") +
                packet(TYPE_STATUS, b'{"protocol":1}'))
        port.read.side_effect = [data[:13], data[13:], serial.SerialException("done")]
        transport._serial = port
        transport._reader()
        self.assertEqual(results, [
            {"command": "SET RATE_HZ 2", "accepted": True},
            {"command": "SET DELAY_MS 400", "accepted": False},
        ])
        self.assertEqual(statuses, [{"protocol": 1}])

    def test_impossible_length_does_not_hide_next_valid_packet(self):
        for kind in (TYPE_AUDIO, TYPE_STATUS):
            parser = PacketParser()
            data = packet(kind, b"", 65535) + packet(TYPE_AUDIO, bytes(AUDIO_PAYLOAD_BYTES))
            packets = parser.feed(data)
            self.assertEqual(len(packets), 1)
            self.assertEqual(packets[0].packet_type, TYPE_AUDIO)

    def test_fragmented_audio_and_status_still_decode(self):
        parser = PacketParser()
        data = packet(TYPE_AUDIO, bytes(AUDIO_PAYLOAD_BYTES)) + packet(TYPE_STATUS, b'{}')
        packets = []
        for start in range(0, len(data), 7):
            packets.extend(parser.feed(data[start:start + 7]))
        self.assertEqual([p.packet_type for p in packets], [TYPE_AUDIO, TYPE_STATUS])

    def test_invalid_status_does_not_kill_reader_and_unplug_is_reported(self):
        statuses = []
        audio = queue.Queue()
        transport = SerialTransport(audio, statuses.append)
        port = Mock(is_open=True)
        port.read.side_effect = [
            packet(TYPE_STATUS, b'{bad'),
            packet(TYPE_STATUS, b'[]'),
            packet(TYPE_STATUS, b'\xff'),
            packet(TYPE_STATUS, b'{"protocol":1}'),
            packet(TYPE_AUDIO, bytes(AUDIO_PAYLOAD_BYTES)),
            serial.SerialException("unplugged"),
        ]
        transport._serial = port
        transport._reader()
        self.assertEqual(statuses, [{"protocol": 1}])
        self.assertEqual(audio.qsize(), 1)
        self.assertFalse(transport.connected)
        self.assertIn("unplugged", str(transport.error))

    def test_failed_connect_closes_open_port(self):
        transport = SerialTransport(queue.Queue())
        port = Mock(is_open=True)
        port.reset_input_buffer.side_effect = serial.SerialException("setup failed")
        with patch("transport.serial.Serial", return_value=port):
            with self.assertRaises(serial.SerialException):
                transport.connect("COM-test")
        port.close.assert_called_once()
        self.assertFalse(transport.connected)


if __name__ == "__main__":
    unittest.main()
