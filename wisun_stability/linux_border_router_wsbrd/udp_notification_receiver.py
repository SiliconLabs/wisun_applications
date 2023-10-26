# Used to receive UDP notifications strings
# Call with
# python udp_notification_receiver.py 1237 " "

import socket
import sys
import datetime

HOST_IP = "::" # Host own address (tun0 IPv6 address)

rcv_port = int(sys.argv[1])
newline = " "

if (len(sys.argv) > 2):
  newline = sys.argv[2]
  space = ""

PORT = rcv_port # Port used by the peer

sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
sock.bind((HOST_IP, PORT))

print(f"Receiving on {HOST_IP}/{PORT}...")

while True:
  data, addr = sock.recvfrom(2048) # buffer size is 2048 bytes
  now = datetime.datetime.now()
  now_str = str(now.strftime('%Y-%m-%d %H:%M:%S'))

  try:
    message_string = data.decode("utf-8").replace(" ", space).replace("\n", newline)
  except Exception as e:
    print(f"Exception {e} (from {addr})")

  print (f"[{now_str}] Rx {PORT}: {newline}", message_string)
