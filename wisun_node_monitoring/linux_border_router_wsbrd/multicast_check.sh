#!/bin/bash

set -ex

ip addr show dev tun0 | grep MULTICAST
echo
ip -6 maddr show dev tun0
echo
ip -6 route show table local dev tun0 | grep ff0
