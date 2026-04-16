#!/usr/bin/env python
# Copyright (c) 2024, Silicon Laboratories
# See license terms contained in COPYING file

# Usage:
# Sending a GBL firmware file using 'OTA-like' messages to an IPv6 address (which can be a multicast such as ff03::2 for 'realm local routers')

# Calling this script:
# multicast_ota.py    ipv6    port   gbl_filename                           tag       interval_s   chunks_selection
# multicast_ota.py    ff03::1 7777   xG25_node_monitoring_BRD4271A_6_5.gbl  BRD4271A  45           0
# multicast_ota.py    ff03::1 7777   xG25_node_monitoring_BRD4271A_6_5.gbl  BRD4271A  45           194 min
# multicast_ota.py    ff03::1 7777   xG25_node_monitoring_BRD4271A_6_5.gbl  BRD4271A  45           194 only
# multicast_ota.py    ff03::1 7777   xG25_node_monitoring_BRD4271A_6_5.gbl  BRD4271A  45           194 and 196 197 ...
# multicast_ota.py    ff03::1 7777   xG25_node_monitoring_BRD4271A_6_5.gbl  BRD4271A  45           194 and 196 197 ...

# Checking Application version
#  coap-client -m get -N -B 7 -t text coap://[fd12:3456::da7a:3bff:fe41:75ba]:5683/info/all
#{
#  "device": "75ba",
#  "chip": "xG25",
#  "board": "BRD4271A",
#  "device_type": "FFN with No LFN support",
#  "application": "Wi-SUN Node Monitoring V3.2.0 2025_6 build 40 6.4",
#  "version": "Compiled on May 21 2025 at 18:09:46"
#  "MAC": "D8:7A:3B:FF:FE:41:75:BA"
#}
# Specifying which file to accept from tftp_path. (use 'tftp_files.sh to check which files exist)
# coap-client -m post -N -B 7 -t text coap://[fd12:3456::da7a:3bff:fe41:75ba]:5683/ota/dfu -e "gbl xG25_2025_06_build_40_BRD4271A_5_4.gbl"
# Sending a FW to a single device
#  python md_share/scripts/multicast_ota.py fd12:3456::da7a:3bff:fe41:75ba 7777 xG25_2025_06_build_40_BRD4271A_5_4.gbl BRD4271A 1 0
# Checking the results
#  python md_share/scripts/multicast_ota.py fd12:3456::da7a:3bff:fe41:75ba 7777 "show_missed_from_list()" BRD4271A 45 0
# Re-sending missing chunks
#  python md_share/scripts/multicast_ota.py fd12:3456::da7a:3bff:fe41:75ba 7777 xG25_2025_06_build_40_BRD4271A_5_4.gbl BRD4271A 1 58 only
#  python md_share/scripts/multicast_ota.py fd12:3456::da7a:3bff:fe41:75ba 7777 xG25_2025_06_build_40_BRD4271A_5_4.gbl BRD4271A 1 78 only
#  python md_share/scripts/multicast_ota.py fd12:3456::da7a:3bff:fe41:75ba 7777 xG25_2025_06_build_40_BRD4271A_5_4.gbl BRD4271A 1 78 and 100 112 235
# Checking the results
#  python md_share/scripts/multicast_ota.py fd12:3456::da7a:3bff:fe41:75ba 7777 "show_missed_from_list()" BRD4271A 1 0
# Verifying FW integrity
#  python md_share/scripts/multicast_ota.py fd12:3456::da7a:3bff:fe41:75ba 7777 "verify_image_in_flash()" BRD4271A 1 0
# Set FW to boot from
#  python md_share/scripts/multicast_ota.py fd12:3456::da7a:3bff:fe41:75ba 7777 "setImageToBootload()"    BRD4271A 1 0
# Reboot
#  python md_share/scripts/multicast_ota.py fd12:3456::da7a:3bff:fe41:75ba 7777 "rebootAndInstall()"      BRD4271A 1 <delay_sec>

# Checking Application version after reboot
#  coap-client -m get -N -B 7 -t text coap://[fd12:3456::da7a:3bff:fe41:75ba]:5683/info/all


#  The payload format is:
#  'gbl_filename chunk_index chunk_data_offset tx_timestamp <data_bytes>'
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

def send_UDP_bytes(DEST, PORT, BYTES, end="\n"):
    # Create UDP socket
    with socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, socket.IPPROTO_UDP) as s:
        # send the message to the Server IPv6 and port
        nb_sent = s.sendto(BYTES, (DEST, PORT))
        # print the destination address and message
        print(f"Sent {nb_sent} bytes to {DEST}/{port}  ", end=end )

if len(sys.argv) < 5:
    #                   argv[0]  argv[1] argv[2] argv[3]        argv[4] argv[5]      argv[6]
    print(f"Usage Send chunk: {sys.argv[0]} <ipv6>  <port>  <gbl_filename> <tag>   <interval_s> <last_chunk>")
    print(f"Usage clear_ota_data: {sys.argv[0]} <ipv6> <port> clear_ota_data() <unused> <unused> <unused>")
    print(f"Usage rebootAndInstall: {sys.argv[0]} <ipv6> <port> rebootAndInstall() <unused> <unused> <time_s_before_reboot>")
    sys.exit(1)

# 'ipv6' = destination for the messages
ipv6 = sys.argv[1]
print(f"IPv6: {ipv6} ")
port = int(sys.argv[2])
print(f"Port: {port} ")
gbl_filename = sys.argv[3]
print(f"GBL Filename: {gbl_filename} ")
test_option = sys.argv[3]
tag         = sys.argv[4]
print(f"Tag: {tag} ")
interval_s  = float(sys.argv[5])
print(f"Interval (s): {interval_s} ")

if len(sys.argv) > 6:
    last_chunk  = int(sys.argv[6])
else:
    try:
        last_chunk  = int(sys.argv[-1])
    except TypeError:
        last_chunk  = 0


test_options_1arg = ["show_missed_from_list()",
                "show_missed()",
                "show_repeated()",
                "clear_ota_data()",
                "verify_image_in_flash()",
                "setImageToBootload()",
                ]

test_options_2arg = ["rebootAndInstall()",
                "rebootAndInstallClearNVMApp()",
                "rebootAndInstallClearNVMFull()",
                ]

if test_option in test_options_1arg:
    message = f"OTA {test_option} " + 50*"+"
    print(f"Message mode: {message}")
    send_UDP_bytes(ipv6, port, message.encode('utf-8'))
    quit()


if test_option in test_options_2arg:
    #in case of rebootAndInstall last_chunk contain time in second before reboot
    message = f"OTA {test_option} {last_chunk} " + 50*"+"
    print(f"Message mode: {message}")
    send_UDP_bytes(ipv6, port, message.encode('utf-8'))
    quit()

chunk_size = 1024

# flags
only_mode = False
and_mode = False
min_mode = False
min_chunk = 0
max_chunk = 1000

if len(sys.argv) > 7:
    if sys.argv[7] == "only":
        only_mode = True
        only_chunk = int(sys.argv[6])
        print(F"only mode for chunk {only_chunk}")

    if sys.argv[7] == "min":
        min_mode = True
        min_chunk = int(sys.argv[6])
        last_chunk = 0
        print(F"min mode starting at chunk {min_chunk}")

    if sys.argv[7] == "and":
        and_chunks = list()
        and_chunks.append(int(sys.argv[6]))
        for item in sys.argv[8:]:
            and_chunks.append(int(item))
        last_chunk = int(item)
        print(F"and_mode for {and_chunks}")
        and_mode = True
else:
    last_chunk = int(sys.argv[6])

tftp_folder  = "/srv/tftp/"

# Only reaching this part if attempting to send a .gbl file

if not os.path.isfile(tftp_folder + gbl_filename):
    print(f"No such file as '{gbl_filename}' under {tftp_folder}")
    quit()
else:
    f = open("/srv/tftp/" + gbl_filename, 'rb')
    filebytes = f.read()
    f.close()

file_data_len     = len(filebytes)
chunk_index = 1
chunk_data_offset = 0 # position of data in the file
chunk_data_len    = 0 # number of bytes in the chunk's data
max_chunk         = int(1.0*file_data_len/chunk_size + 1)

print(f"file size: {file_data_len} bytes, to be sent in {max_chunk} chunks of {chunk_size} bytes")

if only_mode:
        print(f"only_mode for chunk {only_chunk}")
else:
    if and_mode:
        print(f"and_mode for chunks {and_chunks}")
    else:
        if min_mode:
            print(f"min_mode for chunks {min_chunk}-{last_chunk}")
        else:
            if last_chunk:
                print(f"normal mode for chunks {min_chunk}-{last_chunk}")
            else:
                print(f"normal mode for all chunks")

chunk_offset      = 0 # position of data from the message start

while chunk_offset < file_data_len:
    chunk_data = filebytes[chunk_offset:chunk_offset + chunk_size]
    chunk_data_len = len(chunk_data)
    pre_header = f"OTA {gbl_filename} {chunk_index:4d} {chunk_data_offset:4d} {now()} {tag} "
    chunk_data_offset = len(pre_header)
    header     = f"OTA {gbl_filename} {chunk_index:4d} {chunk_data_offset:4d} {now()} {tag} "
    data_string_info = f": {chunk_data[0]:02x} {chunk_data[1]:02x} ---({chunk_data_len} bytes)--- {chunk_data[-2]:02x} {chunk_data[-1]:02x}"
    data_range_info  = f": [{chunk_offset:8d}:{chunk_offset + chunk_size - 1:8d}]/[{chunk_offset:08x}:{chunk_offset + chunk_size - 1:08x}] "
    chunk_offset += chunk_data_len

    if only_mode:
        if chunk_index == only_chunk:
            send_UDP_bytes(ipv6, port, header.encode('utf-8') + chunk_data, end="")
            print(header + data_string_info + data_range_info)
    else:
        if and_mode:
            if chunk_index in and_chunks:
                send_UDP_bytes(ipv6, port, header.encode('utf-8') + chunk_data, end="")
                print(header + data_string_info + data_range_info)
                time.sleep(interval_s)
        else:
            if chunk_index >= min_chunk:
                # normal mode
                send_UDP_bytes(ipv6, port, header.encode('utf-8') + chunk_data, end="")
                print(header + data_string_info + data_range_info)
                time.sleep(interval_s)

    chunk_index += 1

    if last_chunk:
        if chunk_index > last_chunk:
            # end
            file_data_len = 0

print(f"File {gbl_filename} sent in {int(chunk_index-1)} chunks of {chunk_size} bytes + {chunk_data_len} bytes")
