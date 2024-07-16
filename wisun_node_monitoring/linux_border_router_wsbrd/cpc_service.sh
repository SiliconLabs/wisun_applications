#!/bin/bash
# usage: following cpcd execution when it is started as a service
# cpc_service.sh

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
echo -e "${GREEN}/usr/local/lib/systemd/system/cpcd.service ${NO}\n"
cat /usr/local/lib/systemd/system/cpcd.service
