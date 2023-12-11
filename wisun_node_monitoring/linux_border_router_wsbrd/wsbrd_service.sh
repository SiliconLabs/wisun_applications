#!/bin/bash
# usage: following wsbrd execution when it is started as a service
# ./wsbrd_service.sh
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

echo -e "${GREEN}/usr/local/lib/systemd/system/wisun-borderrouter.service ${NO}\n"
cat /usr/local/lib/systemd/system/wisun-borderrouter.service

echo -e "\n${YELLOW}Use: ExecStart=/bin/bash -c '/usr/local/bin/wsbrd-fuzz -F /etc/wsbrd.conf --capture /home/pi/wsbrd_capture_$(/bin/date +%%y_%%m_%%d__%%H_%%M_%%S).raw --delete-storage' to use wsbrd-fuzz"
echo -e "${BLUE}Comment 'Restart=on-failure' to avoid having systemd restart wsbrd automatically"
echo -e "${RED}call 'sudo systemctl daemon-reload' after changing /usr/local/lib/systemd/system/wisun-borderrouter.service${NO}"
