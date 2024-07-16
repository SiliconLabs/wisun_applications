#!/bin/bash
# usage: following wsbrd execution when it is started as a service
# wsbrd_disable.sh
set -x

sudo systemctl enable cpcd.service
