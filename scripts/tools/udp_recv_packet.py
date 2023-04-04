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

sock.setblocking(True)
sock.settimeout(1 / 1000.0)
sock.bind((ip, 4210))

cols = 64
rows = 20
pixels = bytearray(cols)
peaks = bytearray(cols)
peak_counter = bytearray(cols)
peak_hold = 80.0
ph_90 = peak_hold - (peak_hold * 0.3)
div = (255 / float(rows))

start = time.monotonic_ns()
update = start;
packets = 0
data = bytearray(1)
ul = 60
while True:
    try:
        msg = sock.recvfrom(1024)
        data = msg[0]
        dl = len(data)
        if dl>8:
            packets += 1
            for j in range(0, cols):
                if j>=dl:
                    break
                v = pixels[j]
                d = data[j]
                v = min(1, int(v - v * 0.58))
                v = min(255, v + d)
                c = peak_counter[j]
                p = peaks[j]
                if d>=p:
                    # a new data value is higher than the stored peak, restart
                    p = min(255, d + 2)
                    c = int(peak_hold)
                if c > 0:
                    # decrease counter
                    c -= 1
                else:
                    # the higher the points are the faster they drop down
                    p -= max(1, int(p / (255 / 8.0)))
                peaks[j] = p
                peak_counter[j] = c
                pixels[j] = v
                ul = 60
    except Exception:
        pass
    if ul>0:
        ul -= 1
        if ul==0:
            pixels = bytearray(cols)
            peaks = bytearray(cols)
            peak_counter = bytearray(cols)
            print("\033[2JNO DATA")
            time.sleep(1);
            continue
    now = time.monotonic_ns()
    dur_ms = (now - update) / (1000 * 1000.0)
    if now - update > 30:
        update = now
        page = "\033[2J"
        if ul==0:
            page += "NO DATA"
            print(page)
            time.sleep(1);
            continue
        for row in range(rows, 0, -1):
            pd = False
            ln = ''
            for col in range(0, cols):
                v = int(pixels[col] / div);
                p = peaks[col]
                c = peak_counter[col]
                # hold the first 10% at the same spot, then slowly let it fall down
                p = int(p / div)
                if row == p:
                    if c > ph_90:
                        ln += '▀ '
                    else:
                        ln += '. '
                    pd = True
                elif row < v or (row <= p and not pd):
                    if not pd:
                        pd = True
                        ln += '. '
                    else:
                        ln += '█ '
                else:
                        ln += '  '
            ln.rstrip(' ');
            page += ln
            page += '\n'
        print(page, end='', flush=True)

print()
print()

