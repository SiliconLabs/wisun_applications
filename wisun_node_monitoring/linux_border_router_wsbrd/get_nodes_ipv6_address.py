#!/usr/bin/env python
# Copyright (c) 2024, Silicon Laboratories
# See license terms contained in COPYING file

from pydbus import SystemBus
import re

"""
get_nodes_ipv6_address.py
Prints the IPv6 address of each node connected to the running WSBRD instance.

USAGE:
    get_nodes_ipv6_address.py

NOTES:
    - This script can only be used with the WSBRD + RCP setup and must be executed on the host.
"""

bus = SystemBus()
proxy = bus.get("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter")

nodes = proxy.Nodes

def sliceIPv6(source):
    return [source[i : i + 4] for i in range(0, len(source), 4)]

def prettyIPv6(ipv6):
    ipv6 = ":".join(sliceIPv6(ipv6))
    ipv6 = re.sub("0000:", ":", ipv6)
    ipv6 = re.sub(":{2,}", "::", ipv6)
    return ipv6
    
if "ipv6" in nodes[0][1]:
    # D-BUS API < 2.0
    for node in nodes:
        # D-Bus API < 2.0
        if len(node[1]["ipv6"]) != 2:
            continue
        if "parent" not in node[1] and (
            "node_role" not in node[1] or node[1]["node_role"] != 2
        ):
            continue

        ipv6 = bytes(node[1]["ipv6"][1]).hex()
        print(prettyIPv6(ipv6))
else:
    # D-BUS API >= 2.0
    topology = proxy.RoutingGraph
    for node in topology:
        ipv6 = bytes(node[0]).hex()
        print(prettyIPv6(ipv6))

