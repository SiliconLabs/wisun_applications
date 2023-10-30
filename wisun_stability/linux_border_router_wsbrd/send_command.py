#!/usr/bin/env python3
import sys
import datetime
import socket
import select
import struct

dst_host = sys.argv[1]
dst_port = 1235
command = ' '.join(sys.argv[2:])
data = bytes(command + '\0', 'ascii') 

sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_DONTFRAG, 1)

if True: # FIXME: be smart here
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_MULTICAST_HOPS, 10)

sock.sendto(data, (dst_host, dst_port))
print(f"{str(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))} sent     {len(data)} bytes to  {dst_host} port {dst_port}:\n{data}")
quit()
