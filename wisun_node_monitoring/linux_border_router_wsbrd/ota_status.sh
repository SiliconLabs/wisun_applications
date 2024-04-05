#!/bin/bash
set -ex
coap-client -m get -N -B 10 -t text coap://[$1]:5683/ota/dfu
