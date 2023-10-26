#!/bin/bash
# usage: listening for incoming UDP text messages from connected devices
# ./wsbrd_listen.sh

set -ex
python udp_notification_receiver.py 1237 " "
