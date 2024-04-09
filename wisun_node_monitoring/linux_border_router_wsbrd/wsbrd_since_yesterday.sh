#!/bin/bash
# usage: following wsbrd execution when it is started as a service
# wsbrd_since_yesterday.sh <date_time>
# wsbrd_since_yesterday.sh 2024-04-05
# wsbrd_since_yesterday.sh            08:30:00
# wsbrd_since_yesterday.sh 2024-04-05 08:30:00

if [ -z ${1} ]
then
  time="yesterday"
else
  time=${*}
fi

set -x

sudo journalctl -u wisun-borderrouter.service --since "$time"
