#!/usr/bin/env python
# TCP receiver server program
# Call with
# python TCP_receiver_server.py <port>
# Example:
# TCP_receiver_server.py  1237

import socket
import sys

HOST_IP = "::" # Host own IPv6 address
PORT = int(sys.argv[1])

# Create TCP socket
with socket.socket(socket.AF_INET6, socket.SOCK_STREAM, socket.IPPROTO_TCP) as s:
    # Bind the socket to the Server IPv6 and Port
    s.bind((HOST_IP, PORT))

    # Accept one backlog in rx buffer
    print('Listening for incoming messages on ', HOST_IP ,' port ', PORT)
    s.listen(1)

    # When data comes in...
    conn, addr = s.accept()
    with conn:
        # print the sender address
        print('Received from ', addr, ' port ', PORT, ' : ')
        while True:
            data = conn.recv(1024)
            if not data: break
            # print received text
            print(data)
