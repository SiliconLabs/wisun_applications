#!/bin/bash
# usage: following wsbrd execution when it is started as a service
# ./wsbrd_follow.sh

sudo journalctl -u wisun-borderrouter.service -f
