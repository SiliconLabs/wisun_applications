#!/usr/bin/env python
# Copyright (device_info) 2024, Silicon Laboratories
# See license terms contained in COPYING file
#
# Prerequisites:
# pip install pydbus
#

from pydbus import SystemBus

import re
import sys
import platform
import datetime
import traceback

LOG_FILE                  = 'wsbrd_topology.log'

"""
wsbrd_topology.py
Prints the IPv6 address of each node connected to the running WSBRD instance.

USAGE:
    wsbrd_topology.py <ipv6 or tag> <output_mode>
    wsbrd_topology.py  ipv6          dot
    wsbrd_topology.py  ipv6          svg
    wsbrd_topology.py  tag          json

NOTES:
    - This script can only be used with the WSBRD + RCP setup and must be executed on the host.
"""

if len(sys.argv) > 2:
    index_mode = sys.argv[1]
else:
    index_mode = 'tag'

if len(sys.argv) >= 3:
    output_mode = sys.argv[2]
else:
    output_mode = 'dot'

bus = SystemBus()
proxy = bus.get("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter")


# D-BUS API >= 2.0
nodes = dict()

RCP = ""

json_content = str()


def now(format="%Y-%m-%d %H:%M:%S"):
    return datetime.datetime.now().strftime(format)


def read_file_lines(file_name):
    try:
        with open(file_name) as f:
            lines = f.readlines()
        return lines
    except FileNotFoundError:
        return None


def save_to_file(file_path, msg, trace=False, mode='a'):
    try:
        f = open(file_path, mode)
        f.write(msg)
        f.close()
    except Exception as e:
        trace_exception(f"Exception {e} during save_to_file(file_path)", traceback.format_exc())

    if trace:
        print_and_log(f"saved {file_path}")


def print_and_log(msg, end='\n'):
    tagged_msg = now() + ": " + msg
    print(tagged_msg, end=end)
    save_to_file(LOG_FILE, tagged_msg + end)


def trace_exception(msg, traceback_info):
    print_and_log(msg + '\n')
    print_and_log(traceback_info + '\n')


def os_command(cmd):
    try:
        return platform.os.popen(cmd).read()
    except Exception as e:
        trace_exception(f"Exception {e} in os_command({cmd}) ", traceback.format_exc())


def sliceIPv6(source):
    return [source[i : i + 4] for i in range(0, len(source), 4)]


def prettyIPv6(ipv6):
    ipv6 = ":".join(sliceIPv6(ipv6))
    ipv6 = re.sub("0000:", ":", ipv6)
    ipv6 = re.sub(":{2,}", "::", ipv6)
    return ipv6


def IPv6_to_tag(ipv6):
    return ipv6.replace(':','')[-4:]


def EUI64_to_tag(eui64):
    return eui64.replace(':','')[-4:]


def edge_color_from_rsl(rsl_dBm):
    # rsl_dbm can be from -174 to +81 dBm
    # only the -100 to  0 dBm range is interesting to colorize
    range_max =  -20
    range_min = -100

    if rsl_dBm > range_max:
        green_cursor = 100
    else:
        if rsl_dBm < range_min:
            green_cursor = 0
        else:
            a = 100/(range_max - range_min)
            b = 100*a
            green_cursor = a*rsl_dBm + b

    g = int(green_cursor*255/100)
    r = 255 - g
    b = 0

    #print(f"rsl_dBm {rsl_dBm}  green_cursor {green_cursor} #{r:02x}{g:02x}{b:02x}")
    return f"#{r:02x}{g:02x}{b:02x}"


def read_graph():
    global nodes, RCP
    global json_content

    rpl_graph = proxy.RoutingGraph
    RCP_node = rpl_graph[0][0]
    RCP_ipv6 = bytes(RCP_node).hex()
    RCP_tag  = IPv6_to_tag(RCP_ipv6)
    RCP = RCP_tag if index_mode == 'tag' else RCP_ipv6

    for rpl_node in rpl_graph[1:]: # Skip over the first IPv6, which is the tun0 IPv6
        ipv6 = prettyIPv6(bytes(rpl_node[0]).hex())
        tag = IPv6_to_tag(ipv6)
        LFN_flag = rpl_node[1]
        if tag not in list(nodes):
            nodes[tag] = dict()
        nodes[tag]['ipv6'] = ipv6
        nodes[tag]['is_LFN'] = ('False' == LFN_flag)
        if len(rpl_node) > 2:
            parents_list = rpl_node[2]
            if len(parents_list) > 0:
                primary_parent = parents_list[0]
                parent_ipv6 = prettyIPv6( bytes(primary_parent).hex() )
                parent_tag  = IPv6_to_tag(parent_ipv6)
                parent = parent_tag
                nodes[tag]['parent'] = parent


def read_nodes_properties():
    global nodes
    dbus_properties = proxy.Nodes
    for node_props in dbus_properties:
        eui64 = bytes(node_props[0]).hex()
        tag = EUI64_to_tag(eui64)
        if tag not in list(nodes):
            nodes[tag] = dict()
        nodes_property_dict = node_props[1]
        for p in list(nodes_property_dict):
            nodes[tag][p] = nodes_property_dict[p]


def fill_dot():
    global dot_nodes, dot_edges
    dot_content = str()

    dot_header = f"""
    digraph wisun_network {{
        rankdir=TB;  // Top to Bottom layout
        nodesep=0.05
        overlap="true"
        edge        [fontsize=8 dir=both arrowtail=none arrowhead=none penwidth=1]
        node        [style=filled fillcolor=white fontcolor=black]
        node        [penwidth=1] # pendwidth for nodes is the thickness of the outside border
    """
    dot_nodes = ""
    dot_edges = ""
    dot_footer = "}\n"
    network_info = ""

    network_name   = os_command("wsbrd_cli status | grep network_name").split()[1]
    phy_mode_id    = os_command("wsbrd_cli status | grep phy_mode_id")
    chan_plan_id   = os_command("wsbrd_cli status | grep chan_plan_id")
    pan_id         = os_command("wsbrd_cli status | grep panid")
    network_uptime = os_command("ps -C wsbrd,wsbrd-fuzz -o etime").split('\n')[1].strip()
    dot_nodes += f'    "{RCP}" [shape="circle" fillcolor="gold"] \n'

    dot_edges += f'    "network" -> "{RCP}"\n'

    edge_bi_color = dict()
    routed_devices  = 0
    non_routed_devices = 0
    long_lost_devices = 0

    for node in list(nodes):
        props = nodes[node].copy()
        if 'node_role' in list(props):
            if props['node_role'] == 0:
                props['node_role'] = 'BR'
            if props['node_role'] == 1:
                props['node_role'] = 'FFN-FAN1.1'
            if props['node_role'] == 2:
                props['node_role'] = 'LFN'
            if type(props['node_role']) == int:
                props['node_role'] = 'FFN-FAN1.0'

        edge_in_color = edge_out_color = "#000000"
        if 'rsl'  in list(props):
            edge_in_color = edge_color_from_rsl(props['rsl'])
        if 'rsl_adv' in list(props):
            edge_out_color = edge_color_from_rsl(props['rsl_adv'])
        edge_bi_color[node] = f"{edge_in_color};0.5:{edge_out_color}"

        if 'is_neighbor'  in list(props):
            props['RCP_neighbor'] = props['is_neighbor']
            del props['is_neighbor']

        if 'is_LFN' in list(props):
            shape = 'hexagon' if nodes[node]['is_LFN'] else 'circle'
        else:
            if node != RCP:
                shape = 'star'
            else:
                shape = 'circle'

        fillcolor="green"
        if node == RCP:
            fillcolor="gold"
        else:
            if 'is_authenticated' in list(props) and not 'parent' in list(props):
                if 'ipv6' in list(props):
                    fillcolor = 'orange'
                    props['routed'] = 'has an ipv6 but is not routed: recently turned off?'
                else:
                    fillcolor = 'red'
                    props['routed'] = 'has no ipv6, is not routed: long lost device?'
                    long_lost_devices += 1
                non_routed_devices += 1
            else:
                props['routed'] = 'has an ipv6, is routed'
                routed_devices += 1

        dot_nodes += f'    "{node}" [shape="{shape}" fillcolor={fillcolor} tooltip="{props}"]\n'

    #dot_nodes += f'    "{RCP}" [label="{RCP}]\n'

    for node in list(nodes):
        if 'parent' in list(nodes[node]):
            parent = nodes[node]['parent']
            dot_edges += f'    "{parent}" -> "{node}" [color = "{edge_bi_color[node]}"]\n'

    network_info += f'{network_name}\\n'
    network_info += f'{phy_mode_id}\\n'
    network_info += f'{chan_plan_id}\\n'
    network_info += f'{pan_id}\\n'
    network_info += f'{now()}\\n'
    network_info += f'wsbrd running for {network_uptime}\\n'
    network_info += f'{routed_devices} routed_devices (green)\\n{non_routed_devices} non_routed_devices (orange)\\n\\{long_lost_devices} long_lost_devices (red)'

    dot_nodes += f'    network     [label="{network_info}" shape=rectangle] \n'
    dot_content = dot_header + dot_nodes + dot_edges + dot_footer
    return dot_content


def list_nodes():
    for tag in list(nodes):
        print(f'node[{tag}] {nodes[tag]}')


read_graph()
read_nodes_properties()
#list_nodes()

dot_content = fill_dot()
print(dot_content)

