#!/usr/bin/env python3

import socket

HOST = "0.0.0.0"
PORT = 514

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((HOST, PORT))

print(f"Listening for UDP syslog on {HOST}:{PORT}")

while True:
    data, addr = sock.recvfrom(65535)
    print(f"{addr[0]}:{addr[1]}: {data.decode('utf-8', errors='replace')}")
