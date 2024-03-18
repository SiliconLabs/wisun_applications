#!/bin/bash
# usage: following wsbrd execution when it is started as a service
# ./wsbrd_disable.sh

sudo systemctl enable wisun-borderrouter.service
