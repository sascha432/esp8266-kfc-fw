import socket
import time
import os
import random
import itertools
import struct

multicast = False
data_size = 48
max_packets = 10
if multicast:
    ip = '192.168.0.255'
else:
    ip = '192.168.0.62'

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
if multicast:
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)

for i in range(0, max_packets):
    random.seed = int(time.monotonic() * 10);
    data = bytearray(map(random.getrandbits,(8,)*data_size))
    data = b'123456789012345678901234567890123456789012345678'
    print(sock.sendto(data, (ip, 4210)), '%.1f%%' % (i * 100 / max_packets))
    time.sleep(1)
