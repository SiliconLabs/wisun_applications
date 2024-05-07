#!/bin/bash
# usage: checking wsbrd conf file
# wsbrd_conf.sh

echo "only active lines are displayed:"
set -ex
cat /etc/wsbrd.conf | grep -v ^$ | grep -v ^#
