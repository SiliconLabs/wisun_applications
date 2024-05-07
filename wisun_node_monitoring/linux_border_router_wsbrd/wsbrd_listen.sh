#!/bin/bash
# usage: listening for incoming UDP text messages from connected devices
# wsbrd_listen.sh

set -ex
cd ~

monitoring_path="/home/pi/monitoring"
rm -rf $monitoring_path/*/*notification*

udp_notification_receiver.py 1237 " " $monitoring_path
