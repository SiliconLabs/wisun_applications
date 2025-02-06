#!/usr/bin/env python
# Used to receive UDP notifications strings
# Call with
# direct_connect_receiver.py 3770

import socket
import os
import sys
import datetime
import traceback

HOST_IP = "::" # Host own address (tun0 IPv6 address)

help_text = """
# Call with
#  direct_connect_receiver.py <REPORTER_PORT>
#  direct_connect_receiver.py 3770
"""

if len(sys.argv) < 2:
  print(f"{help_text}")
  quit()

reporter_port = int(sys.argv[1])

now = datetime.datetime.now()
now_str = str(now.strftime('%Y-%m-%d %H:%M:%S'))

PORT = reporter_port # Port used by the peer

sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
sock.bind((HOST_IP, PORT))

print(f"[{now_str}] Receiving on {HOST_IP}/{PORT}...", flush=True)

rx_count = 0
previous_line = 0
previous_tag = ""

try:
  while True:
    data, addr = sock.recvfrom(2048) # buffer size is 2048 bytes
    addr_string = addr[0]
    now = datetime.datetime.now()
    now_str = str(now.strftime('%Y-%m-%d %H:%M:%S'))
    rx_count += 1

    try:
      message_string = data.decode("utf-8")
      nb_data = len(data)
    except Exception as e:
      print(f"Exception {e} (from {addr})", flush=True)
      print(traceback.format_exc())

    tag = addr_string.replace(':',"")[-4:]
    if tag != previous_tag:
      print(f"\n \n new device: {tag}\n")
      previous_tag = tag
    info_string = f"[{now_str}] [{tag}] "
    len_info_string = len(info_string)
    line_count = 0
    for line in message_string.split('\n'):
      split_line = line.split('_')
      if line[0] == '_':
        try:
          line_number = int(split_line[1])
          line_delta = line_number - previous_line
          if line_delta > 1 and line_count > 0:
            print(f"{previous_line:05d} {line_delta:2d} ","!"*40, end="")
          previous_line = line_number
        except:
            pass
      print (info_string, line, flush=True, end="\n")
      line_count += 1
      if "Rx" in info_string:
        info_string = " "*len_info_string
except KeyboardInterrupt:
    quit()

