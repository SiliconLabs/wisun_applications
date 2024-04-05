#!/bin/bash
coap-client -m get -N -B 1 -t text coap://[fd00:6172:6d00::2]:5685/ota/dfu_notify
