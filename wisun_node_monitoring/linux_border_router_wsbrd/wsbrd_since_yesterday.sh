#!/bin/bash

if [ -z ${1} ]
then
  time="yesterday"
else
  time=${1}
fi

sudo journalctl -u wisun-borderrouter.service --since $time
