# Linux Border Router Scripts

The following bash and Python scripts are convenient to more easily manage `wsbrd`, `tftpd`, OTA/DFU and check the Wi-SUN network status.
To be used as documented below, they should be

- Copied to the Linux Border Router user's directory.
- Set as executable using `chmod a+x *.sh`

> Use the `--help` option or check the scripts code for more details.

## `wsbrd` control and monitoring

| Name | Language | Usage | Call | Result |
|------|----------|-------|------|--------|
| `wsbrd_conf.sh`    | bash | Show current `wsbrd` config file (`/etc/wsbrd.conf`) | `~/wsbrd_conf.sh` | All active lines of `/etc/wsbrd.conf` |
| `wsbrd_service.sh` | bash | Show current `wsbrd` service file | `~/wsbrd_service.sh` | cat of ` /usr/local/lib/systemd/system/wisun-borderrouter.service` and doc on: enabling capture, disabling auto-restart, applying updates |
| `wsbrd_reload.sh`  | bash | Reload `systemd` daemon after changing settings | `~wsbrd_reload.sh` | `sudo systemctl daemon-reload` |
| `wsbrd_files.sh`   | bash | Show wsbrd storage files: key files per device, etc.| `~/wsbrd_files.sh` | `ls` of `/var/lib/wsbrd/*` |
| `wsbrd_clear.sh`   | bash | Clear all `wsbrd` storage files | `./wsbrd_clear.sh` | no file left in `/var/lib/wsbrd/* |
| `wsbrd_add.sh`     | bash | Add fixed IPv6 Addresses to `tun0` | `~/wsbrd_add.sh` | `ip address show tun0` shows `fd00:6172:6d00::1/64` and `fd00:6172:6d00::2/64` |
| `wsbrd_info.sh`    | bash | Check wsbrd lifetime | `~/wsbrd_info.sh` |`PID USER COMMAND %MEM ELAPSED` |
| `wsbrd_follow.sh`  | bash | Follow wsbrd traces from `journalctl`. All traces with no argument, otherwise filtering on device_tag | `~/wsbrd_follows.sh [device_tag]` | `sudo journalctl -u wisun-borderrouter.service -f` |
| `wsbrd_listen.sh`  | bash | Start UDP notification receiver | `~/wsbrd_listen.sh` | UDP notifications received from connected Wi-SUN devices |

## Network checks

| Name | Language | Usage | Call | Result |
|------|----------|-------|------|--------|
| `get_nodes_ipv6_address.py` | Python | List all IPv6s addresses of connected Wi-SUN nodes (from DBus) | `python ~/get_nodes_ipv6_address.py` | All Global IPv6 addresses |
| `ipv6s`                     | bash   | Shortcut to the above Python script                            | `~/ipv6s` |  All Global IPv6 addresses |

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
| `ota_follow.sh`                    | bash | Ask `coap-server` for the last `/ota/dfu_notify` notification. Best used when called with `watch` | `~/ota_notification_server_start.sh` | `coap-client -m get -N -B 1 -t text coap://[fd00:6172:6d00::2]:5685/ota/dfu_notify` |

## CoAP

| Name | Language | Usage | Call | Result |
|------|----------|-------|------|--------|
| `coap_all <coap_uri> [-e <coap_payload>]` | bash | `~/coap_all /status/all` | Sending a CoAP request (default `/.well-known/core`) to all connected devices | Recursive response to `coap-client -m get -N -B 3 coap://[${ipv6}]:5683${coap_uri} ${coap_payload}` |

## Testing

| Name | Language | Usage | Call | Result |
|------|----------|-------|------|--------|
| `iperf_test.sh --client <client_ipv6> --server <server_ipv6> --bandwidth <bw_bps> --duration <ms> --interval <ms> --buffer_length <1232_by_default> [--ping] [--stop]` | bash | `~/iperf_test.sh --client <client_ipv6> --server <server_ipv6> --bandwidth <bw_bps> --duration <ms> --interval <ms> --buffer_length <1232_by_default> [--ping] [--stop]` | Launching iperf test from client to server using COAP | Measured bandwidth vs required bandwidth |
