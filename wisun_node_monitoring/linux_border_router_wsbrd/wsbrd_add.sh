# IPv6 address for UDP notifications
sudo ip address add fd00:6172:6d00::1/64 dev tun0
# IPv6 address for TFTP Server (OTA files server)
sudo ip address add fd00:6172:6d00::2/64 dev tun0
# Check
ip address show tun0 | grep global
