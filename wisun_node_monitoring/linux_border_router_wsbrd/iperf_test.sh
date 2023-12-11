#!/bin/bash
# Copyright (c) 2023, Silicon Laboratories
# See license terms contained in COPYING file

USAGE='This script is used to test iperf performance between devices running the Wi-SUN SoC Network Measurement example application
Getting help:
 $ ./iperf_test.sh --help
 $ ./iperf_test.sh --client <client_ipv6> --server <server_ipv6> --bandwidth <bw_bps> --duration <ms> --interval <ms> --buffer_length <1232_by_default> [--ping] [--stop]
Setting up the test parameters:
 $ ./iperf_test.sh --client fd12:3456::1311:1111:1111:1111 --server fd12:3456::1311:1111:1111:1112 --bandwidth 300000 --duration 10000 --interval 1000 --stop
Testing a bandwidth: 
 $ ./iperf_test.sh --client fd12:3456::1311:1111:1111:1111 --server fd12:3456::1311:1111:1111:1112 --bandwidth 300000
Testing a bandwidth range: MIN,MAX,STEP 
 $ ./iperf_test.sh --client fd12:3456::1311:1111:1111:1111 --server fd12:3456::1311:1111:1111:1112 --bandwidth 100000,50000,10000
Testing a bandwidth N times: BANDWIDTH,COUNT
 $ ./iperf_test.sh --client fd12:3456::1311:1111:1111:1111 --server fd12:3456::1311:1111:1111:1112 --bandwidth 100000,5
'
if [ -z ${1} ]
then
  echo "$USAGE"
  exit 0
fi
 
date
#set -ex

# Default values
TRACE=0
DATE=0
DURATION=10000
B=30
SILENT=0
ENABLE_SILENT=1

# Command line parameters
while true; do
    case "$1" in
        --client)        client="$2";         shift ;;
        --server)        server="$2";         shift ;;
        --bandwidth)     BANDWIDTH="$2";      shift ;;
        --buffer_length) BUFFER_LENGTH="S2";  shift ;;
        --duration)      DURATION="$2";       shift ;;
        --interval)      INTERVAL="$2";       shift ;;
        --ping)          PING="1";            shift ;;
        --stop)          STOP="1";            shift ;;
        -h|--help)       echo "$USAGE";       exit 0 ;;
        *) break;;
    esac
    shift
done

set +ex

echo_date()
{
  if [ "${DATE}" = "1" ]; then
    echo $(date)
  fi
}

trace()
{
  if [ "${TRACE}" = "1" ]; then
    echo ${*}
  fi
}

coap_cmd()
{
  cmd="coap-client -m get -N -B ${B} -t text coap://[${1}]:5683/cli/iperf -e \"${2}\""
  trace $cmd

  if [ "$SILENT" = "1" ]
  then
#    echo "---------------- $cmd"
    eval $cmd 1> /dev/null
  else
#    echo "++++++++++++++++ $cmd"
    eval $cmd
  fi

  echo_date
}

get_results()
{
  res=$(coap_cmd ${server} "iperf get result")
  echo_date
  trace $res
  echo $res | awk '{split($0,a,","); print a[36] }'
}

test_iperf()
{
  # Making sure both client and server are responding
  if [ -z ${client} ]; then
    echo "No client... "
  fi

  if [ -z ${server} ]; then
    echo "No server... "
  fi

  if [ -z ${client} ]; then
    echo "iperf testing impossible, sorry!"
    return
  else
    if [ -z ${server} ]; then
      echo "iperf testing impossible, sorry!"
      return
    fi
  fi

  # Starting iperf test
  echo -n "Test iperf ${client} -> ${server} ${BANDWIDTH} : "
  SILENT=${ENABLE_SILENT}

  coap_cmd ${server} "iperf server"
  coap_cmd ${client} "iperf client"

  SILENT=0
  sleep $(( ${DURATION}/1000 + 5 ))
  trace "Test end"
  echo_date
  res=$(get_results)
  echo_date
  if [ "${res}" = "" ]
  then
    echo "no result, aborting test"
    exit 1
  else
    echo ${res}
  fi
}

ping_test()
{
  res=$(ping -c 1 -w 3 ${1})
  test=$(echo $res | grep "100% packet loss")

  if [ ! -z "$test" ]; then
    echo 0
  else
    echo 1
  fi
}


if [ ! -z ${PING} ]; then
  if [ ! -z ${client} ]; then
    client_ping=$(ping_test ${client})
    if [ "${client_ping}" = "0" ]; then
      echo "client ${client} not responding to ping: removing it"
      unset client
    else
      echo "client ${client} responding to ping..."
    fi
  fi
  if [ ! -z ${server} ]; then
    server_ping=$(ping_test ${server})
    if [ "${server_ping}" = "0" ]; then
      echo "server ${server} not responding to ping: removing it"
      unset server
    else
      echo "server ${server} responding to ping..."
    fi
  fi
fi

if [ ! -z ${BUFFER_LENGTH} ]; then
  coap_cmd ${server} "iperf set options.buffer_length ${BUFFER_LENGTH}"
  coap_cmd ${client} "iperf set options.buffer_length ${BUFFER_LENGTH}"
fi

if [ ! -z ${DURATION} ]; then
  coap_cmd ${server} "iperf set options.duration ${DURATION}"
  coap_cmd ${client} "iperf set options.duration ${DURATION}"
fi

if [ ! -z ${INTERVAL} ]; then
  coap_cmd ${server} "iperf set options.interval ${INTERVAL}"
  coap_cmd ${client} "iperf set options.interval ${INTERVAL}"
fi

if [   -z ${STOP} ]; then

  coap_cmd ${client} "iperf set options.remote_addr ${server}"

  if [ ! -z "$(echo ${BANDWIDTH} | grep ".*,.*,.*")" ]
  then
    # Test MIN,MAX,STEP
    echo "BANDWIDTH=\"${BANDWIDTH}\" is MIN,MAX,STEP; ${BANDWIDTH}"
    MIN=$( echo ${BANDWIDTH} | awk '{ split($0,a,","); print a[1] }')
    MAX=$( echo ${BANDWIDTH} | awk '{ split($0,a,","); print a[2] }')
    STEP=$(echo ${BANDWIDTH} | awk '{ split($0,a,","); print a[3] }')
    trace "MIN ${MIN}"
    trace "MAX  ${MAX}"
    trace "STEP ${STEP}"
    BANDWIDTH=$MIN

    for (( BANDWIDTH=$MIN ; BANDWIDTH <= $MAX ; BANDWIDTH=$(($BANDWIDTH + $STEP)) ));  do
      # echo -n " requested bandwidth $BANDWIDTH "
      coap_cmd ${client} "iperf set options.bandwidth ${BANDWIDTH}"
      test_iperf
    done
  else
    if [ ! -z "$(echo ${BANDWIDTH} | grep ".*,.*")" ]
    then
      # Test BANDWITH,COUNT
      echo "BANDWIDTH=\"${BANDWIDTH}\" is BANDWIDTH,COUNT"
      bw=${BANDWIDTH}
      BANDWIDTH=$( echo ${bw} | awk '{ split($0,a,","); print a[1] }')
      COUNT=$(     echo ${bw} | awk '{ split($0,a,","); print a[2] }')
      echo "BANDWIDTH ${BANDWIDTH}"
      echo "COUNT  ${COUNT}"

      coap_cmd ${client} "iperf set options.bandwidth ${BANDWIDTH}"

      for (( N=1 ; N <= $COUNT ; N++ )); do
        echo -n " test ${N}/${COUNT} "
        test_iperf
      done

    else
        # Normal single shot test
      echo -n " requested bandwidth $BANDWIDTH "
      test_iperf

    fi
  fi
fi

echo "test complete"
date
