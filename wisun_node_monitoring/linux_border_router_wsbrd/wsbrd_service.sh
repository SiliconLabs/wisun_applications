#!/bin/bash
# usage: following wsbrd execution when it is started as a service
# ./wsbrd_service.sh

set -ex
cat /usr/local/lib/systemd/system/wisun-borderrouter.service

echo "Use 'ExecStart=/usr/local/bin/wsbrd-fuzz -F /etc/wsbrd.conf --capture /tmp/wsrbd_capture.raw -D' to use wsbrd-fuzz"
echo "Comment 'Restart=on-failure' to avoid having systemd restart wsbrd automatically"
echo "call 'sudo systemctl daemon-reload' after changing /usr/local/lib/systemd/system/wisun-borderrouter.service"
