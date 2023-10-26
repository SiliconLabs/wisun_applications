#!/bin/bash
# usage: clearing files stored by wsbrd, which need to be deleted to start a fresh session
# ./wsbrd_files_clear.sh

sudo rm -rf /var/lib/wsbrd/*
