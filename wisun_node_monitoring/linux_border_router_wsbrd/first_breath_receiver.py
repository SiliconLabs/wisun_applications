#!/usr/bin/env python3
"""Print First Breath UDP notifications received by the wsbrd host."""

import datetime
import socket


HOST = "::"
PORT = 1238

sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
sock.bind((HOST, PORT))
print(f"Listening for First Breath notifications on [{HOST}]:{PORT}", flush=True)

while True:
    data, address = sock.recvfrom(2048)
    timestamp = datetime.datetime.now().astimezone().isoformat(timespec="seconds")
    message = data.decode("utf-8", errors="replace").strip()
    print(f"[{timestamp}] First Breath from [{address[0]}]: {message}", flush=True)
