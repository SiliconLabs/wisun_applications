# Linux Border Router Scripts

The following bash and Python scripts are convenient to more easily manage `wsbrd`, `tftpd`, OTA/DFU and check the Wi-SUN network status.
To be used as documented below:

- Make sure they are all set as executable using `chmod a+x *.*`
- Accessible from anywhere by adding `export PATH=$PATH:~/wisun_applications/wisun_node_monitoring/linux_border_router_wsbrd/` at the end of ` ~/.bashrc`

> Use the `--help` option or check the scripts code for more details.

---

- [Linux Border Router Scripts](#linux-border-router-scripts)
  - [`wsbrd` control and monitoring](#wsbrd-control-and-monitoring)
    - [`wsbrd` Configuration](#wsbrd-configuration)
    - [`wsbrd` Start \&  Stop](#wsbrd-start---stop)
    - [`wsbrd` Storage/Cache check and cleanup](#wsbrd-storagecache-check-and-cleanup)
    - [`wsbrd` Monitoring](#wsbrd-monitoring)
  - [IPv6 networking](#ipv6-networking)
    - [`tun0` IPv6 addresses addition (for UDP/CoAP/OTA)](#tun0-ipv6-addresses-addition-for-udpcoapota)
    - [`tun0` multicast](#tun0-multicast)
    - [Network checks](#network-checks)
  - [UDP \& TCP](#udp--tcp)
  - [TFTP control and checks](#tftp-control-and-checks)
  - [OTA control and monitoring](#ota-control-and-monitoring)
  - [CoAP](#coap)
  - [Testing](#testing)
    - [Throughput testing between Wi-SUN nodes](#throughput-testing-between-wi-sun-nodes)
  - [Ease of use](#ease-of-use)
    - [IPv6 from Wi-SUN Node Nickname](#ipv6-from-wi-sun-node-nickname)

---

## `wsbrd` control and monitoring

### `wsbrd` Configuration

| Name | Language | Usage | Call | Result |
|------|----------|-------|------|--------|
| `wsbrd_conf.sh`    | bash | Show current `wsbrd` config file (`/etc/wsbrd.conf`) | `wsbrd_conf.sh` | All active lines of `/etc/wsbrd.conf` |
| `wsbrd_service.sh` | bash | Show current `wsbrd` service file | `wsbrd_service.sh` | cat of ` /usr/local/lib/systemd/system/wisun-borderrouter.service` and doc on: enabling capture, disabling auto-restart, applying updates |
| `wsbrd_reload.sh`  | bash | Reload `systemd` daemon after changing settings | `wsbrd_reload.sh` | `sudo systemctl daemon-reload` |

### `wsbrd` Start &  Stop

| Name | Language | Usage | Call | Result |
|------|----------|-------|------|--------|
| `wsbrd_enable.sh`  | bash | Allow wsbrd as a service | `wsbrd_enable.sh` | Necessary to start wsbrd from the GUI |
| `wsbrd_restart.sh` | bash | Start/Restart wsbrd as a service | `~/wsbrd_restart.sh` | Useful to test wsbrd start/restart |
| `wsbrd_disable.sh` | bash | Stop wsbrd (if started as a service) | `wsbrd_disable.sh` | Useful to test wsbrd restart |
| `wsbrd_restart.sh` | bash | Start/Restart wsbrd as a service | `wsbrd_restart.sh` | wsbrd restart if access from D-Bus (which is the case using `wsbrd_cli` or the GUI)! |
| `wsbrd_stop.sh`    | bash | Temporarily stop wsbrd as a service | `wsbrd_stop.sh` | wsbrd restarts if access from D-Bus (which is the case using `wsbrd_cli` or the GUI)! |
| `wsbrd_status.sh`  | bash | Check status of wsbrd as a service | `wsbrd_status.sh` | wsbrd status is printed |
| `wsbrd_manual_start.sh` | bash | Start wsbrd NOT as a service | `wsbrd_manual_start.sh` | wsbrd is started using the same command as in the service file. Convenient to check proper startup and get immediate access to traces. Requires stopping 'wsbrd as a service' first |

### `wsbrd` Storage/Cache check and cleanup

| Name | Language | Usage | Call | Result |
|------|----------|-------|------|--------|
| `wsbrd_files.sh`   | bash | Show wsbrd storage files: key files per device, etc.| `wsbrd_files.sh` | `ls` of `/var/lib/wsbrd/*` |
| `wsbrd_clear.sh`   | bash | Clear all `wsbrd` storage files | `wsbrd_clear.sh` | no file left in `/var/lib/wsbrd/*` |

### `wsbrd` Monitoring

| Name | Language | Usage | Call | Result |
|------|----------|-------|------|--------|
| `wsbrd_info.sh`    | bash | Check wsbrd lifetime | `wsbrd_info.sh` |`PID USER COMMAND %MEM ELAPSED` |
| `wsbrd_follow.sh`  | bash | Follow wsbrd traces from `journalctl`. All traces with no argument, otherwise filtering on device_tag | `wsbrd_follow.sh [device_tag]` | `sudo journalctl -u wisun-borderrouter.service -f` |
| `wsbrd_since_yesterday.sh`  | bash | Same as `wsbrd_follow.sh`, limited to 1 day | `wsbrd_since_yesterday.sh` | `sudo journalctl -u wisun-borderrouter.service --since $time` |
| `wsbrd_listen.sh`  | bash | Start UDP notification receiver | `wsbrd_listen.sh` | UDP notifications received from connected Wi-SUN devices, tracing messages in `~/monitoring/` |
| `wsbrd_monitor.sh` | bash | Start wsbrd monitoring | `wsbrd_monitor.sh` | Changes in connected devices and topology are traced, new IPs are listed in `~/monitoring` |
| `wsbrd_introspect.sh` | bash | First level DBus introspection of wsbrd DBus interface | `wsbrd_introspect.sh` | Displays the DBus properties/methods/interfaces for `com.silabs.Wisun.BorderRouter`|

## IPv6 networking

### `tun0` IPv6 addresses addition (for UDP/CoAP/OTA)

| Name | Language | Usage | Call | Result |
|------|----------|-------|------|--------|
| `wsbrd_add.sh`     | bash | Add fixed IPv6 Addresses to `tun0` | `wsbrd_add.sh` | `ip address show tun0` shows `fd00:6172:6d00::1/64` and `fd00:6172:6d00::2/64` |

### `tun0` multicast

| Name | Language | Usage | Call | Result |
|------|----------|-------|------|--------|
| `multicast_setup.sh` | bash | Allow multicast on `tun0` | `multicast_setup.sh` | 4 multicast routes added on `tun0` |
| `multicast_check.sh` | bash | Check multicast info for `tun0` | `multicast_check.sh` | 4 multicast routes should be listed on `tun0` |

### Network checks

| Name | Language | Usage | Call | Result |
|------|----------|-------|------|--------|
| `get_nodes_ipv6_address.py` | Python | List all IPv6s addresses of connected Wi-SUN nodes (from DBus) | `get_nodes_ipv6_address.py` | All Global IPv6 addresses |
| `ipv6s`                     | bash   | Shortcut to the above Python script                            | `ipv6s` | All Global IPv6 addresses |
| `ping_all`                  | bash   | Recursive call to all `ipv6s`. Useful to check if nodes are responding | `ping_all` | One ping per device, number of devices, number of responses |
| `ping_link_local_nodes`     | bash   | Multicast ping to `ff02::1` | `ping_link_local_nodes`   | One multicast ping request, one reply per device (max 1 hop)|
| `ping_link_local_routers`   | bash   | Multicast ping to `ff02::2` | `ping_link_local_routers` | One multicast ping request, one reply per FFN device (max 1 hop)|
| `ping_realm_local_nodes`    | bash   | Multicast ping to `ff03::1` | `ping_realm_local_nodes`  | One multicast ping request, one reply per device (any hop)|
| `ping_realm local_routers`  | bash   | Multicast ping to `ff03::2` | `ping_realm_local_routers`| One multicast ping request, one reply per FFN device  (any hop)|

## UDP & TCP

| Name | Language | Usage | Call | Result |
|------|----------|-------|------|--------|
| `TCP_receiver_server.py` | Python | Use TCP to listen to incoming messages     | `TCP_receiver_sender.py <port>` | Messages sent to tun0 will be traced |
| `TCP_sender_client.py`   | Python | Use TCP to send a text message to the destination     | `TCP_sender_client.py <IPv6> <port> "<message>"` | Message send to Wi-SUN node (TCP = no Multicast) |
| `UDP_sender_client.py`   | Python | Use UDP to send a text message to the destination(s) | `UDP_sender_client.py <IPv6> <port> "<message>"` | Message send to Wi-SUN node (UDP = Multicast compatible) |
| `UDP_notification_receiver.py`| Python | Listen to UDP incoming messages | `UDP_notification_receiver.py <port> <separator>` | Message sent to the UDP notification port are traced |

## TFTP control and checks

| Name | Language | Usage | Call | Result |
|------|----------|-------|------|--------|
| `tftp_config.sh`   | bash | Checking tftpd config             | `~tftp_config.sh`   | `cat` of `/etc/default/tftpd-hpa`       |
| `tftp_files.sh`    | bash | Checking tftpd files              | `~tftp_files.sh`    | `ls` of `/srv/tftp/*.gbl`               |
| `tftp_check.sh`    | bash | Checking for running tftpd daemon | `~tftp_check.sh`    | `ps -few \| grep tftpd \| grep -v grep` |
| `tftp_status.sh`   | bash | Checking tftpd status             | `~tftp_status.sh`   | `tftp localhost -c status`              |
| `tftpd_restart.sh` | bash | Checking tftpd status             | `~tftpd_restart.sh` | `/etc/init.d/tftpd-hpa restart`         |

## OTA control and monitoring

| Name | Language | Usage | Call | Result |
|------|----------|-------|------|--------|
| `ota_install.sh`                   | bash | Installing `libcoap2`| `~/ota_install.sh` | `sudo apt-get install libcoap2 libcoap2-bin tftpd-hpa tftp-hpa` |
| `ota_notification_server_start.sh` | bash | Start `coap server` to receive OTA notifications from connected devices | `~/ota_notification_server_start.sh` | `coap-server -A fd00:6172:6d00::2 -p 5685 -d 10` |
| `ota_start.sh`                     | bash | Start OTA DFU on <device_ipv6> | `~/ota_start.sh <device-ipv6>` | Initial OTA message from <device_ipv6>|
| `ota_status.sh`                    | bash | Check OTA DFU status for <device_ipv6> | `~/ota_status.sh <device-ipv6>` | OTA status message from <device_ipv6>|
| `ota_follow.sh`                    | bash | Ask `coap-server` for the last `/ota/dfu_notify` notification. Best used when called with `watch` | `~/ota_notification_server_start.sh` | `coap-client -m get -N -B 1 -t text coap://[fd00:6172:6d00::2]:5685/ota/dfu_notify` |
| `view_gbl.sh`                      | bash | Hex display of selected input `.gbl` file | `view_gbl.sh <gbl_file> | Hex dump of `.gbl` file content |

## CoAP

| Name | Language | Usage | Call | Result |
|------|----------|-------|------|--------|
| `coap_all`                             | bash | `coap_all <coap_uri>` example:  `coap_all /status/all`     | Sending a CoAP **GET** request (default `/.well-known/core`) to all connected devices | Recursive response to `coap-client -m get -N -B 3 coap://[${ipv6}]:5683${coap_uri}`|
| `coap_all -e <single_argument>`        | bash | `coap_all <coap_uri> -e <arg>` example:  `coap_all /ota/dfu -e gbl` | Sending a CoAP **GET** request to all connected devices for the `<single_argument_payload>` | Recursive response to `coap-client -m get -N -B 3 coap://[${ipv6}]:5683${coap_uri} -e <single_argument> |
| `coap_all -e <first_arg> <second_arg>` | bash | `coap_all <coap_uri> -e <arg1> <arg2>` example:  `coap_all /ota/dfu -e gbl version_2.gbl` | Sending a CoAP **POST** request to all connected devices to set the `<first_arg>` to `<second_arg>` | Recursive response to `coap-client -m get -N -B 3 coap://[${ipv6}]:5683${coap_uri} -e <first_argument> <second_arg>` |

> NB: `coap_all` traces all individual commands, such that the user can copy/paste them and execute a single command if need be.

## Testing

### Throughput testing between Wi-SUN nodes

| Name | Language | Usage | Call | Result |
|------|----------|-------|------|--------|
| `iperf_test.sh` | bash | `~/iperf_test.sh --client <client_ipv6> --server <server_ipv6> --bandwidth <bw_bps> --duration <ms> --interval <ms> --buffer_length <1232_by_default> [--ping] [--stop]` | Launching iperf test from client to server using COAP | Measured bandwidth vs required bandwidth |

## Ease of use

### IPv6 from Wi-SUN Node Nickname

| Name | Language | Usage | Call | Result |
|------|----------|-------|------|--------|
| `nick2ip` | bash | `nick2ip <nickname>` | Returning the corresponding IPv6. The `nickname` is that shown by the wsbrd GUI, i.e. the last 4 digits of the IPv6/MAC. Can be used as a replacement for the IPv6 in commands, via `$(nick2ip <nickname>)` | IPv6 for the Wi-SUN Node |
