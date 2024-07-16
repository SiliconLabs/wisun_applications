#!/usr/bin/env python
# Copyright (c) 2024, Silicon Laboratories
# See license terms contained in COPYING file

# Usage:
# Sending a GBL firmware file using 'OTA-like' messages to an IPv6 address (which can be a multicast such as ff03::2 for 'realm local routers')

# Calling this script:
# UDP_sender_large_test.sh   ipv6   port   gbl_filename   chunk_size   nb_chunks

#  The message format is:
#  'gbl_filename chunk_index chunk_data_offset tx_timestamp tag <data_bytes>'
#  with                                                         ^
#     chunk_data_offset = --index of first data byte:-----------|
#

import socket
import sys
import os
import time

from time import localtime, strftime

def now(format="%H:%M:%S"):
    return strftime(format, localtime())

def send_UDP_bytes(DEST, PORT, BYTES):
    # Create UDP socket
    with socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, socket.IPPROTO_UDP) as s:
        # send the message to the Server IPv6 and port
        nb_sent = s.sendto(BYTES, (DEST, PORT))
        # print the destination address and message
        print('Sent ', nb_sent, ' bytes to ', DEST, ' port ', PORT)

# 'ipv6' = destination for the messages
ipv6 = sys.argv[1]
port = int(sys.argv[2])
gbl_filename = sys.argv[3]

chunk_size   = int(sys.argv[4])
nb_chunks    = int(sys.argv[5])

only_mode = False
if len(sys.argv) > 6:
    if sys.argv[6] == "only":
        print(F"only mode for chunk {nb_chunks}")
        only_mode = True
        only_chunk = nb_chunks

tag = "TAG"
tftp_folder  = "/srv/tftp/"

test_options = ["show_missed_from_list()", 
                "show_missed()", 
                "show_udp_data(ALL)", 
                "show_udp_data(VALID)", 
                "show_udp_data(INVALID)", 
                "show_udp_data(EMPTY)", 
                "clear_udp_data()"]

if gbl_filename in test_options:
    message = f"{gbl_filename} " + 50*"+"
    print(f"Message mode: {message}")
    send_UDP_bytes(ipv6, port, message.encode('utf-8'))
    quit()

if not os.path.isfile(tftp_folder + gbl_filename):
    print(f"not such file as '{gbl_filename}' under {tftp_folder}")
    quit()
else:
    f = open("/srv/tftp/" + gbl_filename, 'rb')
    filebytes = f.read()
    f.close()

file_data_len     = len(filebytes)
chunk_index = 1
chunk_data_offset = 0 # position of data in the file
chunk_data_len    = 0 # number of bytes in the chunk's data

print(f"file size: {file_data_len} bytes, to be sent in {1.0*file_data_len/chunk_size} chunks of {chunk_size} bytes")

chunk_offset      = 0 # position of data from the message start
while chunk_offset < file_data_len:
    chunk_data = filebytes[chunk_offset:chunk_offset + chunk_size]
    chunk_data_len = len(chunk_data)
    pre_header = f"{gbl_filename} {chunk_index:4d} {chunk_data_offset:4d} {now()} {tag} "
    chunk_data_offset = len(pre_header)
    header = f"{gbl_filename} {chunk_index:4d} {chunk_data_offset:4d} {now()} {tag} "
    data_string_info = f": {chunk_data[0]:02x} {chunk_data[1]:02x} ---({chunk_data_len} bytes)--- {chunk_data[-2]:02x} {chunk_data[-1]:02x}"
    data_range_info  = f": [{chunk_offset:4d}:{chunk_offset + chunk_size:4d}]/[{chunk_offset:08x}:{chunk_offset + chunk_size:08x}] "

    chunk_offset += chunk_data_len

    if not only_mode:
        print(header + data_string_info + data_range_info)
        send_UDP_bytes(ipv6, port, header.encode('utf-8') + chunk_data)
        time.sleep(2)
    else:
        if chunk_index == only_chunk:
            print(header + data_string_info + data_range_info)
            send_UDP_bytes(ipv6, port, header.encode('utf-8') + chunk_data)
            file_data_len = 0
            quit()

    chunk_index += 1

    if nb_chunks:
        if chunk_index > nb_chunks:
            # end
            file_data_len = 0

print(f"File {gbl_filename} sent in {chunk_index-1} chunks of {chunk_size} bytes + {chunk_data_len} bytes")

