#!/bin/bash
# usage: sending a CoAP request to all connected devices
# direct_connect.sh <MAC>
if [ -z "$1" ]; then
    echo "direct_connect.sh <MAC address>"
    exit 1
fi

sudo silabs-ws-dc -F ~/direct_connect.conf --opt=target_eui64=${1}
