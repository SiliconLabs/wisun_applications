#!/bin/bash
set -ex
coap-server -A fd00:6172:6d00::2 -p 5685 -d 10
