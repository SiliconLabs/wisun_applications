#!/bin/bash
# usage: checking cpcd conf file
# /usr/local/etc/cpcd.conf

echo "only active lines are displayed:"
set -ex
cat /usr/local/etc/cpcd.conf | grep -v ^$ | grep -v ^#
