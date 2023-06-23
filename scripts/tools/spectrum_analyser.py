#
# Author: sascha_lammers@gmx.de
#

import math
import pyaudio
import struct
import numpy as np
import matplotlib.pyplot as plt
from scipy.fft import fft
import socket

DEST_IP = '192.168.0.21'

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)


CHUNK = 1024 * 2
FORMAT = pyaudio.paInt16
CHANNELS = 1
RATE = 44100
VOLUME = 256 * 1.0      # increase volume by 100%

p = pyaudio.PyAudio()

# print(p.get_default_output_device_info())
# exit(1)

# enable "Stereo Mix" under advanced recording settings
# https://www.howtogeek.com/39532/how-to-enable-stereo-mix-in-windows-7-to-record-audio/

stream = p.open(
    format=FORMAT,
    channels=CHANNELS,
    rate=RATE,
    input=True,
    frames_per_buffer=CHUNK
)

# stream = p.open(
#     input_device_index=1,
#     format=FORMAT,
#     channels=CHANNELS,
#     rate=RATE,
#     input=True,
#     frames_per_buffer=CHUNK
# )

try:
    while True:
        data = stream.read(CHUNK)
        data_int = struct.unpack(str(2 * CHUNK) + 'B', data)

        data = fft(data_int)
        lin_fft_data = np.abs(data[0:CHUNK]) * 2 / (256 * CHUNK)

        bands = 10
        px = 2
        b0 = 0
        loudness = 0
        out = np.linspace(0, 255, 32, dtype='B')
        for i in range(0, bands):
            b1 = int(math.pow(px, i + 1) - 1)
            n = max(lin_fft_data[b0:b1]) * VOLUME
            if n < 0:
                n = 0
            elif n > 255:
                n = 255
            j = (i  - 1) * 4;
            if j >=0:
                for k in range(j, min(32, j + 4)):
                    out[k] = n
                    # loudness += n
            b0 = b1 + 1

        loudness = math.sqrt(max(lin_fft_data[0:CHUNK // 2])) * 128
        if loudness > 255:
            loudness = 255
        left = int(loudness)
        right = left
        hdr = np.array((1, 5, 222, 77, left, right, 66, len(out)), dtype='B')
        out = np.append(hdr, out);
        while ((len(out) - 2) % 4) != 0:
            out = np.append(out, np.array((0), dtype='B'))
        sock.sendto(out, (DEST_IP, 21324))
except KeyboardInterrupt:
    pass

print()
sock.sendto(np.linspace(0, 0, 32, dtype='B'), (DEST_IP, 21324))
