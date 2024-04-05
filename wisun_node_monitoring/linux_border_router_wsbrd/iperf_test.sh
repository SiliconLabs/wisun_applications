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
 
#set -ex

# Default values
TRACE=0
TIME=0
DURATION=10000
B=10
SILENT=0
ENABLE_SILENT=0
PING=0
PAUSE=0

# Command line parameters
while true; do
    case "$1" in
        # variable       name   value         shift(to pass over the value)
        --client)        client="$2";         shift  ;;
        --server)        server="$2";         shift  ;;
        --bandwidth)     BANDWIDTH="$2";      shift  ;;
        --duration)      DURATION="$2";       shift  ;;
        --interval)      INTERVAL="$2";       shift  ;;
        --pause)         PAUSE="$2";          shift  ;;
        # flag           name   1
        --ping)          PING="1";                   ;;
        --stop)          STOP="1";                   ;;
        --csv)           CSV="1";                    ;;
        --trace)         TRACE="1";                  ;;
        --foo)           FOO="1";                    ;;
        --time)          TIME="1";                   ;;
        -h|--help)       echo "$USAGE";       exit 0 ;;
        *) break;;
    esac
    shift
done

ipv6s=$(get_nodes_ipv6_address.py)

# use nickname to find server IPV6
for ipv6 in $ipv6s
do
  test=$(echo $ipv6 | grep $server)
  if [ ! -z "$test" ]; then
    server=$ipv6
    break
  fi
done

# use nickname to find client IPV6
for ipv6 in $ipv6s
do
  test=$(echo $ipv6 | grep $client)
  if [ ! -z "$test" ]; then
    client=$ipv6
    break
  fi
done

echo -n "client ${client}  server ${server}  bandwidth ${BANDWIDTH} "
echo "PAUSE ${PAUSE}  PING ${PING}  STOP ${STOP}  TRACE ${TRACE}  TIME ${TIME}  CSV ${CSV} "

set +ex

echo_date()
{
  if [ "${TIME}" = "1" ]; then
    echo $(date +"%H:%M:%S") ":  " $1
  else
    echo "  " $1
  fi
}

if [ "${DATE}" = "0" ]; then
    echo_date
fi

trace()
{
  if [ "${TRACE}" = "1" ]; then
    if [ "${TIME}" = "1" ]; then
      echo -n "$(date): "
    fi
    echo "${*}"
  fi
}

coap_cmd()
{
  cmd="coap-client -m get -N -B ${B} -t text coap://[${1}]:5683/cli/iperf -e \"${2}\""

  if [ "$SILENT" = "1" ];
  then
    coap_cmd_res=$(eval $cmd 1> /dev/null && wait)
  else
    coap_cmd_res=$(eval $cmd && wait)
  fi

  if [ -n "${coap_cmd_res}" ];
  then
    trace " ${cmd} > {$coap_cmd_res}"
    sleep 1 && wait
  else
    coap_cmd_res="no_response"
    trace " ${cmd} > ${coap_cmd_res}"
  fi
}

coap_cmd_grep_id()
{
  cmd="coap-client -m get -N -B ${B} -t text coap://[${1}]:5683/cli/iperf -e '${2}'"

  coap_cmd_res=$(eval $cmd && wait)

  if [ -n "${coap_cmd_res}" ];
  then
    coap_cmd_res=$(test_id ${coap_cmd_res})
    trace " ${cmd} > Test id ${coap_cmd_res}"
    sleep 1 && wait
  else
    coap_cmd_res="no_response"
    trace " ${cmd} > ${coap_cmd_res}"
  fi

}

test_id()
{
  # grep flags: 
  #  -o : return match
  #  -P : allow Perl syntax
  # regexp flags:
  #  \K : kill all previously matching text
  #  \d* : any number of decimal
  
  # Search for \d*, kill (\K) all previously matching text
  echo  ${*} | grep -oP '\"id\": \K\d*'
}

test_bandwidth()
{
  # grep flags: 
  #  -o : return match
  #  -P : allow Perl syntax
  # regexp flags:
  #  \K : kill all previously matching text
  #  \d* : any number of decimal
  #
  # Search for \d*, kill (\K) all previously matching text

  bws=$(echo  ${*} | grep -oP '\"bandwidth\": \K\d*' )
  bw=$( echo  ${bws} | awk -F " " -v idx=$1 '{ split($0,a); print a[idx] }' )
  echo ${bw}
}

flush_test_queue()
{
  coap_cmd_grep_id ${*} 'iperf get json'
  # When no test has be run, the result will be an empty string, translated as 'no_response'
  if [ -z "${coap_cmd_res}" ]
  then
    return
  fi

  if [ "no_response" = "${coap_cmd_res}" ]
  then
    return
  fi

  initial_test=${coap_cmd_res}

  coap_cmd_grep_id ${*} "iperf get json"
  new_test=${coap_cmd_res}

  while [ "${new_test}" != "${initial_test}" ]
  do
    coap_cmd_grep_id ${*} "iperf get json"
    new_test=${coap_cmd_res}
  done

  trace "${*} test queue flushed: Test id ${new_test}"
}

last_test_result()
{
  count=0
  coap_cmd ${*} 'iperf get json'
  initial_test=$(test_id {coap_cmd_res})

  coap_cmd ${*} 'iperf get json'
  new_test=$(test_id {coap_cmd_res})

  while [ "${new_test}" != "${initial_test}" ]
  do
    coap_cmd ${*} 'iperf get json'
    new_test=$(test_id {coap_cmd_res})
    count=$(( $count+1 ))
    if [ $count > 5 ]
    then
      coap_cmd_res="no test id change after 5 checks???"
      break
    fi
  done

  echo ${coap_cmd_res}
}

test_iperf()
{
  # Starting iperf test
  printf " %s -> %s  bw %8d : " ${client} ${server} ${BANDWIDTH}
  if [ "${TRACE}" = "1" ]; then
    printf "\n"
  fi

  #trace "Starting server"
  coap_cmd ${server} "iperf server"
  if [ "${coap_cmd_res}" = ""  ]; then
    echo "ERROR starting server (no response)"
    return
  fi

  sleep 1

  #trace "Starting client"
  coap_cmd ${client} "iperf client"

  sleep_time=$(( ${DURATION}/1000 + 2 ))
  trace "Waiting for the duration of the test +2 (${sleep_time} seconds)..."
  sleep ${sleep_time}
 
  #trace "# Retrieving test results from client, to flush its test queue"
  client_result=$( last_test_result ${client} )
  #trace "# Retrieving test results from server, now that the test is finished"
  server_result=$( last_test_result ${server} )

  if [ "${server_result}" = "" ]
  then
    echo "no result, aborting test"
    exit 1
  fi

  server_test=$( test_id ${server_result} )
  client_test=$( test_id ${client_result} )
  requested_bw=$( test_bandwidth 1 ${client_result} )
  client_bw=$( test_bandwidth 2 ${client_result} )
  server_bw=$( test_bandwidth 2 ${server_result} )

  echo -n " server" ${server_bw} "(client " ${client_bw} ")"
}

COUNT_DURATION=1

if [ ! -z "$(echo ${DURATION} | grep ".*,.*,.*")" ]; then
  # Duration Test MIN,MAX,STEP"
  echo "DURATION=\"${DURATION}\" is MIN,MAX,STEP"
  MIN_DURATION=$( echo ${DURATION} | awk '{ split($0,a,","); print a[1] }')
  MAX_DURATION=$( echo ${DURATION} | awk '{ split($0,a,","); print a[2] }')
  STEP_DURATION=$(echo ${DURATION} | awk '{ split($0,a,","); print a[3] }')
else
  if [ ! -z "$(echo ${DURATION} | grep ".*,.*")" ]
  then
    # Duration Test DURATION,COUNT"
    echo "DURATION=\"${DURATION}\" is DURATION,COUNT"
    COUNT_DURATION=$( echo ${DURATION} | awk '{ split($0,a,","); print a[2] }')
    DURATION=$( echo ${DURATION} | awk '{ split($0,a,","); print a[1] }')
  fi
  MIN_DURATION=${DURATION}
  MAX_DURATION=${DURATION}
  STEP_DURATION=${DURATION}
fi

trace "MIN_DURATION   ${MIN_DURATION}"
trace "MAX_DURATION   ${MAX_DURATION}"
trace "STEP_DURATION  ${STEP_DURATION}"
trace "COUNT_DURATION ${COUNT_DURATION}"

COUNT_BW=1

if [ ! -z "$(echo ${BANDWIDTH} | grep ".*,.*,.*")" ]; then
  # BW Test MIN,MAX,STEP
  echo "BANDWIDTH=\"${BANDWIDTH}\" is MIN,MAX,STEP; ${BANDWIDTH}"
  MIN_BW=$( echo ${BANDWIDTH} | awk '{ split($0,a,","); print a[1] }')
  MAX_BW=$( echo ${BANDWIDTH} | awk '{ split($0,a,","); print a[2] }')
  STEP_BW=$(echo ${BANDWIDTH} | awk '{ split($0,a,","); print a[3] }')
  COUNT_BW=1
else
  if [ ! -z "$(echo ${BANDWIDTH} | grep ".*,.*")" ]; then
    # BW Test BANDWITH,COUNT
    echo "BANDWIDTH=\"${BANDWIDTH}\" is BANDWIDTH,COUNT"
    bw=${BANDWIDTH}
    BANDWIDTH=$( echo ${bw} | awk '{ split($0,a,","); print a[1] }')
    MIN_BW=${BANDWIDTH}
    MAX_BW=${BANDWIDTH}
    STEP_BW=${BANDWIDTH}
    COUNT_BW=$(     echo ${bw} | awk '{ split($0,a,","); print a[2] }')
  fi
  MIN_BW=${BANDWIDTH}
  MAX_BW=${BANDWIDTH}
  STEP_BW=${BANDWIDTH}
fi

trace "MIN_BW   ${MIN_BW}"
trace "MAX_BW   ${MAX_BW}"
trace "STEP_BW  ${STEP_BW}"
trace "COUNT_BW ${COUNT_BW}"

COUNT=$(( $COUNT_BW * $COUNT_DURATION ))

trace "COUNT    ${COUNT}"

flush_test_queue    ${client}
initial_client_test=${new_test}

flush_test_queue    ${server}
initial_server_test=${new_test}

if [ ! -z ${INTERVAL} ]; then
  coap_cmd ${server} "iperf set options.interval ${INTERVAL}"
  coap_cmd ${client} "iperf set options.interval ${INTERVAL}"
fi

if [   -z ${STOP} ]; then

  coap_cmd ${client} "iperf set options.remote_addr ${server}"

  N=1
    
  for (( BANDWIDTH=$MIN_BW ; BANDWIDTH <= $MAX_BW ; BANDWIDTH=$(( $BANDWIDTH + $STEP_BW )) ));  do
    # echo -n " requested bandwidth $BANDWIDTH "
    coap_cmd ${client} "iperf set options.bandwidth ${BANDWIDTH}"
    for (( DURATION=$MIN_DURATION ; DURATION <= $MAX_DURATION ; DURATION=$(( $DURATION + $STEP_DURATION )) ));  do
      for (( C=1 ; C <= $COUNT ; C++ )); do
        coap_cmd ${client} "iperf set options.duration ${DURATION}"
        coap_cmd ${server} "iperf set options.duration ${DURATION}"
        printf "$(date): test %3d: duration %6d" ${N} ${DURATION}
        N=$(( $N + 1 ))
        server_bw=""
        client_bw=""
        test_iperf && wait
        if [   -z ${CSV} ]; then
          echo ""
        else
          echo "   " ${DURATION} ";" ${BANDWIDTH} ";" ${server_bw} ";" ${client_bw}
        fi
        sleep $PAUSE
      done
    done
  done
fi

echo_date "test complete"
