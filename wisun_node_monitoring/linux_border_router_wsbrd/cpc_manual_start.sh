#!/bin/bash
# usage: following cpcd execution when it is started as a service
# cpc_manual_start.sh [<conf_file>]

# by default, use /usr/local/etc/cpcd.conf as the configuration file
if [ -z "$1" ]; then
    conf_file="/usr/local/etc/cpcd.conf"
else
    conf_file="$1"
fi

set -ex
sudo cpcd --conf ${conf_file}
