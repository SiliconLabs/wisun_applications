#!/usr/bin/env python
# Copyright (c) 2024, Silicon Laboratories
# See license terms contained in COPYING file

# TCP sender (client) program
# Call with
# python TCP_sender_client.py <IPv6> <port> <message>
# Example:
# TCP_sender_client.py  fd00:6172:6d00:0:92fd:9fff:fe00:333a 1237 "Message from TCP sender"

import socket
import sys

server  = in_server = sys.argv[1]
PORT    = int(sys.argv[2])
MESSAGE = sys.argv[3]

# Create TCP socket
with socket.socket(socket.AF_INET6, socket.SOCK_STREAM, socket.IPPROTO_TCP) as s:
    # Connect the socket to the TCP Server IPv6 and Port
    s.connect((server, PORT))
    # print the destination address and message
    s.sendall(MESSAGE.encode('utf-8'))
    s.shutdown(socket.SHUT_RDWR)
    s.close()
    print('Sent to ', in_server, ' port ', PORT, ' : ',  MESSAGE)
