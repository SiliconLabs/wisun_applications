#!/usr/bin/env python
# Copyright (c) 2024, Silicon Laboratories
# See license terms contained in COPYING file

# UDP sender (client) program
# Call with
# python UDP_sender_client.py <IPv6> <port> <message>
# Example:
# UDP_sender_client.py  fd00:6172:6d00:0:92fd:9fff:fe00:333a 1237 "Message from UDP sender"

import socket
import sys


help_text = """
# Call with
# Direct_Connect_CLI <Direct_Connect_Ipv6>  <7777>  "<message string>"
# Direct_Connect_CLI  fe80::2adb:a7ff:fe77:2bc4  7777 "wisun set_trace_level 2"
# Direct_Connect_CLI  fe80::2adb:a7ff:fe77:2bc4  7777 "wisun set_trace_level 16 3"
"""


if len(sys.argv) < 4:
    print(help_text)
    
server  = in_server = sys.argv[1]
PORT    = int(sys.argv[2])
MESSAGE = sys.argv[3]

# Create UDP socket
with socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, socket.IPPROTO_UDP) as s:
    # send the message to the Server IPv6 and port
    nb_sent = s.sendto(MESSAGE.encode('utf-8'), (server, PORT))
    # print the destination address and message
    print('Sent ', nb_sent, ' bytes to ', in_server, ' port ', PORT, ' : ',  MESSAGE)

