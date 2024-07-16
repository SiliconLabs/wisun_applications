#!/bin/bash
# Copyright (c) 2023, Silicon Laboratories
# See license terms contained in COPYING file

# Usage:
# Sending a set of 'ota' messages to an IPv6 address (which can be multicast)
#  The message format is:
#  gbl_filename chunk_index chunk_data_offset tx_timestamp tag <data_bytes>
#  with                                                         ^
#     chunk_data_offset = --index of first data byte:-----------|
#
# At the end of the day, 
#set -ex
