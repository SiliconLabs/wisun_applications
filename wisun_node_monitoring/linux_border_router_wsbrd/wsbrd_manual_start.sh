#!/bin/bash

sudo $(cat /usr/local/lib/systemd/system/wisun-borderrouter.service | grep -oP '^ExecStart=\K(.*)')
