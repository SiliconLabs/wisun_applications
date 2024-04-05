#!/bin/bash
# Copyright (c) 2024, Silicon Laboratories
# See license terms contained in COPYING file

set -x

sudo ip link set tun0 multicast on

# Link local Nodes
sudo ip -6 route add ff02::1 dev tun0 table local

# Link local Routers
sudo ip -6 route add ff02::2 dev tun0 table local

# Realm Local Nodes
sudo ip -6 route add ff03::1 dev tun0 table local

# Realm Local Routers
sudo ip -6 route add ff03::2 dev tun0 table local
