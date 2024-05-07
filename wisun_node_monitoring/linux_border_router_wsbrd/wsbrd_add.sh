#!/bin/bash
# usage: adding IPv6 addresses to tun0 to receive messages from the Wi-SUN Node Monitoring application
# wsbrd_add.sh

# IPv6 address for UDP notifications
sudo ip address add fd00:6172:6d00::1/64 dev tun0
# Default IPv6 address for UDP notifications and TFTP server
sudo ip address add 2001:db8::1/64       dev tun0
# IPv6 address for TFTP Server (OTA files server)
sudo ip address add fd00:6172:6d00::2/64 dev tun0
# Check
ip address show tun0 | grep global
