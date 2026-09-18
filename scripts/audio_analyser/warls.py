#
# Author: sascha_lammers@gmx.de
#
"""WARLS (WLED Audio Realtime Led Strip) packet builder.

    offset  size  content
    0       1     protocol id, 1 = WARLS
    1       1     idle timeout in seconds, 0 = packet is ignored, 255 = max
    2       2     magic 0x4DDE (little endian: 222, 77)
    4       1     left channel level, 0-255
    5       1     right channel level, 0-255
    6       1     magic 0x42 (66)
    7       1     number of bands, 2-32 (kVisualizerPacketSize)
    8       n     spectrum, one byte per band
    8+n     pad   zero bytes, (size - 2) must be dividable by 4
"""

from __future__ import annotations

PROTOCOL_WARLS = 1
DEFAULT_TIMEOUT = 5  # seconds
MAGIC1 = (77 << 8) | 222  # 0x4dde, little endian on the wire
MAGIC2 = 66  # 0x42
MIN_BANDS = 2
MAX_BANDS = 32  # VisualizerAnimation::kVisualizerPacketSize
MIN_LEVEL = 0
MAX_LEVEL = 255
DEFAULT_PORT = 21324  # IOT_LED_MATRIX_ENABLE_VISUALIZER_UDP_PORT


def packet_size(bands: int) -> int:
    """Size of a WARLS packet with `bands` values (42 bytes for 32 bands)."""
    return 2 + 4 + 2 + ((bands + 3) & ~3) + 2


def build_packet(bands_values, left: int = 0, right: int = 0, timeout: int = DEFAULT_TIMEOUT) -> bytes:
    """Build one WARLS packet.

    `bands_values` is a sequence of 2-32 values (0-255). Out of range values are
    clamped, same as the level values.
    """
    bands = len(bands_values)
    if not MIN_BANDS <= bands <= MAX_BANDS:
        raise ValueError(f'bands must be {MIN_BANDS}..{MAX_BANDS}, got {bands}')
    if not 0 <= timeout <= 255:
        raise ValueError(f'timeout must be 0..255 seconds, got {timeout}')

    packet = bytearray(packet_size(bands))
    packet[0] = PROTOCOL_WARLS
    packet[1] = timeout
    packet[2] = MAGIC1 & 0xFF  # 222
    packet[3] = MAGIC1 >> 8  # 77
    packet[4] = max(MIN_LEVEL, min(MAX_LEVEL, int(left)))
    packet[5] = max(MIN_LEVEL, min(MAX_LEVEL, int(right)))
    packet[6] = MAGIC2  # 66
    packet[7] = bands
    packet[8 : 8 + bands] = bytes(max(MIN_LEVEL, min(MAX_LEVEL, int(value))) for value in bands_values)
    # the remaining bytes stay 0 (4 byte alignment)
    return bytes(packet)


def build_silent_packet(bands: int = MAX_BANDS, timeout: int = DEFAULT_TIMEOUT) -> bytes:
    """A packet with zero levels and zero bands, sent when the tool exits."""
    return build_packet([0] * bands, 0, 0, timeout)
