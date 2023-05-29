import socket
import time
import os
import random
import itertools
import struct

class PixelColorFormat:
    RGB565 = 0
    RGB24 = 1
    BGR24 = 2

multicast = False
cols = 32
rows = 16
fps = 10.0

format = PixelColorFormat.RGB24
pixel_size = format == PixelColorFormat.RGB565 and 2 or 3

num_pixels = cols * rows
data_size =  num_pixels * pixel_size
chunk_size = min(512, data_size // 2)
max_frames = 100000
video_id = int(time.time() / 300);
total_data = 0
if multicast:
    ip = '192.168.0.255'
else:
    ip = '192.168.0.15'

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
if multicast:
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)

print('dst_host: %s video id %u' % (ip, video_id))
print('num_pixels: %d pixel_size: %d chunk_size: %d' % (num_pixels, pixel_size, chunk_size))
start_time = time.time()

try:
    with open('C:/Users/sascha/Downloads/test.rgb', 'rb') as f:
        for frame in range(0, max_frames):
            position = 0
            size = data_size
            while size>0:
                to_read = min(chunk_size, size)
                data = f.read(to_read);
                if len(data)!=to_read:
                    print("EOF")
                    exit(1)
                size -= to_read
                if position == -1:
                    continue
                header = struct.pack('IHBB', video_id, position, format, 0)
                total_data += sock.sendto(header + data, (ip, 21324))
                position += to_read // pixel_size
            time.sleep(1 / fps)
            video_id += 1
            dur = (time.time() - start_time)
            d = (total_data / 1024.0)
            print('video id %d frame %d sent %.0fKB / %.2fKBit/s' % (video_id, frame, d, (d / dur)), end='\r')
except KeyboardInterrupt:
    print()
    print('aborted... %.0fKB' % (total_data / 1024.0))
    exit(1)

print()
