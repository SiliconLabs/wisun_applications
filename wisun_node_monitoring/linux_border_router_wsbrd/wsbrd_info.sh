#!/bin/bash
# usage: following wsbrd execution when it is started as a service
# wsbrd_info.sh

ps -C wsbrd,wsbrd-fuzz -o  pid,user,command,%mem,etime
