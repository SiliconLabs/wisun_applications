#!/bin/bash
# Copyright (c) 2023, Silicon Laboratories
# See license terms contained in COPYING file

# Usage:
# Following wsbrd messages for a given device
#  $ wsbrd_follow.sh <tag>
# Example:
#  $ wsbrd_follow.sh
#  $ wsbrd_follow.sh ac96

#set -ex

TAG=${1}

if [ ! -z "${TAG}" ] ; then
  TAG_BEG=${1:0:2}
  TAG_END=${1:2:2}
  sudo journalctl -u wisun-borderrouter.service -f | grep "${TAG_BEG}[ :]*${TAG_END}"
else
  sudo journalctl -u wisun-borderrouter.service -f
fi
