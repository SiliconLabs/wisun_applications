#!/bin/bash
# usage: listening for incoming UDP text messages from connected devices
# wsbrd_monitor.sh

monitoring_path="/home/pi/monitoring"

# by default, monitor every minute
if [ -z "$1" ]; then
   delay=60
else
   delay="$1"
fi

previous_ipv6s=""
previous_topology=""

rm -rf $monitoring_path/*/*wsbrd*

while [ 1 ]
do
  current_ipv6s=$(get_nodes_ipv6_address.py)
  now_string=$(date +"%Y-%m-%d %H:%M:%S")
  
  if [ "$current_ipv6s" != "$previous_ipv6s" ]; then
    echo "[${now_string}] There are IPv6 changes..."

    # Check for new IPv6s
    for ipv6 in $current_ipv6s
    do
      test=$(grep $ipv6 <<< $previous_ipv6s)
      if [ -z "${test}" ]; then
        # New IPv6
        ipv6_=$(echo $ipv6  | sed -r "s/::/_0_/g")
        ipv6_=$(echo $ipv6_ | sed -r "s/:/_/g")
        device_path="${monitoring_path}/${ipv6_}"
        if [ ! -d "${device_path}" ]; then
          mkdir "${device_path}"
        fi
        echo "[${now_string}] $ipv6 appeared"
        echo "${now_string}" > "${device_path}/wsbrd_in_date"
      else
        echo "${now_string}" > "${device_path}/wsbrd_last_date"
      fi
    done

    # Check for lost IPv6s
    for ipv6 in $previous_ipv6s
    do
      test=$(grep $ipv6 <<< $current_ipv6s )
      if [ -z "${test}" ]; then
        # Lost IPv6
        ipv6_=$(echo $ipv6  | sed -r "s/::/_0_/g")
        ipv6_=$(echo $ipv6_ | sed -r "s/:/_/g")
        device_path="${monitoring_path}/${ipv6_}"
        if [ ! -d "${device_path}" ]; then
          mkdir "${device_path}"
        fi
        echo "[${now_string}] $ipv6 removed"
        echo "${now_string}" > "${device_path}/wsbrd_out_date"
      fi
    done

  else
    echo -n -e "\r[${now_string}] no change"
  fi

  previous_ipv6s=${current_ipv6s}

  current_topology=$(wsbrd_cli status | grep "^\s.*")

  if [ "$current_topology" != "$previous_topology" ]; then
    echo "[${now_string}] There are topology changes..."
    echo "$current_topology"  > "${monitoring_path}/current_topology"
    echo "$previous_topology" > "${monitoring_path}/previous_topology"
    changes=$(diff -y ${monitoring_path}/previous_topology ${monitoring_path}/current_topology)
    echo "${changes}"
    echo "${changes}" > "${monitoring_path}/$(date +"%Y-%m-%d_%H:%M:%S")_topology"
  fi

  previous_topology=${current_topology}

  sleep $delay
done
