#!/bin/bash
# usage: following wsbrd execution when it is started as a service
# wsbrd_introspect.sh
set -x

busctl introspect com.silabs.Wisun.BorderRouter /com/silabs/Wisun/BorderRouter
