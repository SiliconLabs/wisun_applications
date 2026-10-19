#!/usr/bin/env python3
"""Print Last Gasp UDP notifications received by the wsbrd host."""

import datetime
import socket


HOST = "::"
PORT = 1239

sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
sock.bind((HOST, PORT))
print(f"Listening for Last Gasp notifications on [{HOST}]:{PORT}", flush=True)

while True:
    data, address = sock.recvfrom(2048)
    timestamp = datetime.datetime.now().astimezone().isoformat(timespec="seconds")
    message = data.decode("utf-8", errors="replace").strip()
    print(f"[{timestamp}] Last Gasp from [{address[0]}]: {message}", flush=True)
