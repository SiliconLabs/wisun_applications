#!/bin/bash
# usage: adding IPv6 addresses to eth0 to receive messages from the Wi-SUN Node Monitoring application
# wsbrd_add.sh

set -x

# Check dad (duplicate address detection) status (resulting in 'dadfailed' for)
# accept_dad - INTEGER
#    Whether to accept DAD (Duplicate Address Detection).
#        0: Disable DAD
#        1: Enable DAD (default)
#        2: Enable DAD, and disable IPv6 operation if MAC-based duplicate
#            link-local address has been found.
# Check
ip address show eth0 | grep dadfailed
sudo sysctl -a | grep eth0 | grep accept_dad

# Disable dad (duplicate address detection) on eth0
sudo sysctl -w net.ipv6.conf.eth0.accept_dad=0

# IPv6 address for UDP notifications
sudo ip address add fd00:6172:6d00::1/64 dev eth0

# Default IPv6 address for UDP notifications
sudo ip address add       2001:db8::1/64 dev eth0
# IPv6 address for TFTP Server (OTA files server)
sudo ip address add fd00:6172:6d00::2/64 dev eth0

# Check
ip address show eth0 | grep global
