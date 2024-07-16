#!/usr/bin/env python
# Used to receive UDP notifications strings
# Call with
# udp_notification_receiver.py 1237 " "

import socket
import os
import sys
import datetime

HOST_IP = "::" # Host own address (tun0 IPv6 address)

rcv_port = int(sys.argv[1])

newline = " "

now = datetime.datetime.now()
now_str = str(now.strftime('%Y-%m-%d %H:%M:%S'))

if (len(sys.argv) > 2):
  newline = sys.argv[2]
  space = ""

if (len(sys.argv) > 3):
  monitoring_path = sys.argv[3]
else:
  monitoring_path = False

if (len(sys.argv) > 4):
  logfile = sys.argv[4]
  # Redirect sys.stdout to the file
  print(f"[{now_str}] Logging to {logfile}")
  sys.stdout = open(logfile, 'w')
  print(f"[{now_str}] Logging to {logfile}")
else:
  logfile = None

PORT = rcv_port # Port used by the peer

sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
sock.bind((HOST_IP, PORT))

print(f"[{now_str}] Receiving on {HOST_IP}/{PORT}...", flush=True)

if monitoring_path:
  print(f"[{now_str}] monitoring path '{monitoring_path}'", flush=True)
  if not os.path.isdir(monitoring_path):
    os.mkdir(monitoring_path)

while True:
  data, addr = sock.recvfrom(2048) # buffer size is 2048 bytes
  addr_string = addr[0].replace(":","_")
  now = datetime.datetime.now()
  now_str = str(now.strftime('%Y-%m-%d %H:%M:%S'))

  try:
    message_string = data.decode("utf-8").replace(" ", space).replace("\n", newline)
  except Exception as e:
    print(f"Exception {e} (from {addr})", flush=True)

  print (f"[{now_str}] Rx {PORT}: {newline}", message_string, flush=True)

  if monitoring_path:
    try:
      device_path        = os.path.join(monitoring_path, f"{addr_string}")

      # First time we receive a notification from a device
      if not os.path.isdir(device_path):
        os.mkdir(device_path)

        first_msg_date_path = os.path.join(device_path, "first_notification_date")
        f = open(first_msg_date_path, 'w')
        f.write(now_str+"\n")
        f.close()

        first_msg_path = os.path.join(device_path, "first_notification")
        with open(first_msg_path, 'w') as f:
          f.write(message_string+"\n")
          f.close()

      # Each time we receive a notification from a device
      last_msg_date_path = os.path.join(device_path, "last_notification_date")
      with open(last_msg_date_path, 'w') as f:
        f.write(now_str+"\n")
        f.close()

      last_msg_path      = os.path.join(device_path, "last_notification")
      with open(last_msg_path, 'w') as f:
        f.write(message_string+"\n")
        f.close()

    except Exception as e:
      print(f"Exception in monitoring {e} (from {addr})")
      pass


