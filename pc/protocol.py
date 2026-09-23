import json
import struct
from dataclasses import dataclass

MAGIC = b"DS"
PROTOCOL_VERSION = 1
TYPE_AUDIO = 0x01
TYPE_STATUS = 0x02
HEADER = struct.Struct("<2sBBHI")
AUDIO_PAYLOAD_BYTES = 480 * 2


@dataclass(frozen=True)
class Packet:
    packet_type: int
    sequence: int
    payload: bytes


class PacketParser:
    def __init__(self) -> None:
        self._buffer = bytearray()

    def feed(self, data: bytes) -> list[Packet]:
        self._buffer.extend(data)
        packets: list[Packet] = []
        while True:
            magic_index = self._buffer.find(MAGIC)
            if magic_index < 0:
                self._buffer[:] = self._buffer[-1:]
                break
            if magic_index:
                del self._buffer[:magic_index]
            if len(self._buffer) < HEADER.size:
                break
            magic, version, packet_type, length, sequence = HEADER.unpack_from(
                self._buffer
            )
            if version != PROTOCOL_VERSION or packet_type not in (
                TYPE_AUDIO,
                TYPE_STATUS,
            ):
                del self._buffer[0]
                continue
            total_length = HEADER.size + length
            if len(self._buffer) < total_length:
                break
            payload = bytes(self._buffer[HEADER.size:total_length])
            del self._buffer[:total_length]
            if packet_type == TYPE_AUDIO and length != AUDIO_PAYLOAD_BYTES:
                continue
            packets.append(Packet(packet_type, sequence, payload))
        return packets


def decode_status(packet: Packet) -> dict:
    if packet.packet_type != TYPE_STATUS:
        raise ValueError("not a status packet")
    return json.loads(packet.payload.decode("utf-8"))
