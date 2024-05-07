#!/bin/bash
# usage: following wsbrd execution when it is started as a service
# wsbrd_service.sh

NO="\e[0m"
DEF="\e[39m"
WHITE="\e[97m"
RED="\e[31m"
GREEN="\e[32m"
YELLOW="\e[33m"
BLUE="\e[34m"
LRED="\e[91m"
LGREEN="\e[92m"
LYELLOW="\e[93m"
LBLUE="\e[94m"

echo
echo -e "${GREEN}/usr/local/lib/systemd/system/wisun-borderrouter.service ${NO}\n"
cat /usr/local/lib/systemd/system/wisun-borderrouter.service

echo -e "\n${YELLOW}Use: ExecStart=/bin/bash -c '/usr/local/bin/wsbrd-fuzz -F /etc/wsbrd.conf --capture /tmp/wsbrd_capture.raw' to do a capture with wsbrd <  v2.0"
echo -e   "${YELLOW}Use: ExecStart=/bin/bash -c '/usr/local/bin/wsbrd      -F /etc/wsbrd.conf --capture /tmp/wsbrd_capture.raw' to do a capture with wsbrd >= v2.0"
echo -e "${BLUE}Comment 'BusName=com.silabs.Wisun.BorderRouter' to avoid DBus start wsbrd when using DBus on com.silabs.Wisun.BorderRouter (if wsbrd is stopped)"
echo -e "${BLUE}Comment 'Restart=on-failure' to avoid having systemd restart wsbrd automatically in case it crashes"
echo -e "${BLUE}Do not use '-D'/'--delete-storage' to allow wsbrd to restart with minimal impact on the Wi-SUN devices"
echo -e "${RED}call 'sudo systemctl daemon-reload' after changing /usr/local/lib/systemd/system/wisun-borderrouter.service${NO} (= wsbrd_reload.sh)"
