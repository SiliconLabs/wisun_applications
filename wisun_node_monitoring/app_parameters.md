# app_parameters #

The app_parameters code is split in 2 files:

- app_parameters.c
- app_parameters.h

## General features ##

| CoAP request | CoAP URI           | payload                                         | usage                                                       |
|--------------|------------------- |-------------------------------------------------|-------------------------------------------------------------|
| -m put       | settings/parameter | -e "defaults  `value`"                          | set app_parameters to the defaults, using `value` as a bitfield to select networks. Use `0` to set all |
| -m put       | settings/parameter | -e "save"                                       | save all app_parameters to nvm3                             |
| -m put       | settings/parameter | -e "reboot  `value`"                            | reboot in `values` ms                                       |
| -m put       | settings/parameter | -e "clear_credential_cache_and_reboot  `value`" | clear_credential_cache_and_reboot then reboot in `value` ms |

## Multiple networks ##

Since version V6.2, the ability to store settings for multiple ([MAX_NETWORK_CONFIGS](app_parameters.h#line=89), default 3) networks has been added to the code.

## `app_wisun_parameters_t` set/get ##

The [app_wisun_parameters_t](app_parameters.h#L162) structure contains the items common to all networks:

```text
nb_boots           (read-only)
nb_crashes         (read-only)
auto_send_sec
network_count      (read-only)
network_index
```

There is an additional `app_parameters` option to retrieve all at once

| CoAP request | CoAP URI           | payload                                       | usage                                                  |
|--------------|------------------- |-----------------------------------------------|--------------------------------------------------------|
| -m put       | settings/parameter | -e "`parameter_name`  `value`"                | set `app_parameters.parameter_name` to `value`         |
| -m get       | settings/parameter | -e "`parameter_name`  `value`"                | return `app_parameters.parameter_name` value           |
| -m get       | settings/parameter | -e "`app_parameters`"                         | return all `app_parameters` values                     |
| -m get       | settings/parameter | -e "network_index     `value`"                | Set the network_index, save the parameters (to get ready to use this network) and return `app_parameters.parameter_name` value           |

## `app_wisun_network_settings_t` set/get ##

The [app_settings_wisun_t](app_parameters.h#L143) structure is inspired by the [app_settings_wisun_t](https://github.com/SiliconLabs/simplicity_sdk/blob/sisdk-2025.6/protocol/wisun/app/wisun_soc_cli/app_settings.h#L79) structure from the Wi-SUN SoC CLI code, with support only for FAN1.1.

The remotely configurable parameters are:

```text
network[i].network_name
network[i].network_size
network[i].tx_power_ddbm
network[i].regulation
network[i].device_type
network[i].phy.type
network[i].phy.config.fan11.reg_domain
network[i].phy.config.fan11.chan_plan_id
network[i].phy.config.fan11.phy_mode_id
network[i].max_child_count
network[i].max_neighbor_count
network[i].max_security_neighbor_count
network[i].preferred_pan_id
network[i].max_hop_count
```

| CoAP request | CoAP URI           | payload                                       | usage                                                     |
|--------------|------------------- |-----------------------------------------------|-----------------------------------------------------------|
| -m put       | settings/parameter | -e "`parameter_name`  `network_index`  value" | set `network[network_index].parameter_name` to `value`    |
| -m get       | settings/parameter | -e "`parameter_name`  `network_index`"        | return `network[network_index].parameter_name` value      |
| -m get       | settings/parameter | -e "network  `network_index`"                 | return all `network[network_index].parameter_name` values |

## Example Use Cases ##

### Checking the common settings ##

```bash
pi@rns-wisun-200:~ $ coap-client-notls -m get -N -B 10 -t text coap://[fd12:3456::62a4:23ff:fe37:aee6]:5683/settings/parameter
Missing payload. Use: put settings/parameter -e "parameter_name [int] [int|str]" or  get settings/parameter -e "parameter_name [int]"
pi@rns-wisun-200:~ $ coap-client-notls -m get -N -B 10 -t text coap://[fd12:3456::62a4:23ff:fe37:aee6]:5683/settings/parameter -e "nb_boots"
"nb_boots": "1"
pi@rns-wisun-200:~ $ coap-client-notls -m get -N -B 10 -t text coap://[fd12:3456::62a4:23ff:fe37:aee6]:5683/settings/parameter -e "nb_crashes"
"nb_crashes": "0"
pi@rns-wisun-200:~ $ coap-client-notls -m get -N -B 10 -t text coap://[fd12:3456::62a4:23ff:fe37:aee6]:5683/settings/parameter -e "auto_send_sec"
"auto_send_sec": "900"
pi@rns-wisun-200:~ $ coap-client-notls -m get -N -B 10 -t text coap://[fd12:3456::62a4:23ff:fe37:aee6]:5683/settings/parameter -e "app_parameters"
"app_params_version": "10000",
"nb_boots": "1",
"nb_crashes": "0",
"auto_send_sec": "900",
"network_count": "3",
"network_index": "0"
```

### Checking the current network settings ###

```bash
pi@rns-wisun-200:~ $ coap-client-notls -m get -N -B 10 -t text coap://[fd12:3456::62a4:23ff:fe37:aee6]:5683/settings/parameter -e "network_count"
"network_count": "3"
pi@rns-wisun-200:~ $ coap-client-notls -m get -N -B 10 -t text coap://[fd12:3456::62a4:23ff:fe37:aee6]:5683/settings/parameter -e "network_index"
"network_index": "1"
pi@rns-wisun-200:~ $ coap-client-notls -m get -N -B 10 -t text coap://[fd12:3456::62a4:23ff:fe37:aee6]:5683/settings/parameter -e "network 1"
"network": "1",
"network_name": "Wi-SUN Network AFA",
"network_size": "1",
"type": "1",
"reg_domain": "1",
"phy_mode_id": "2",
"chan_plan_id": "1",
"tx_power_ddbm": "200",
"max_child_count": "22",
"max_neighbor_count": "32",
"max_security_neighbor_count": "300"
```

### Changing network[0] settings and connecting to it ###

```bash
pi@rns-wisun-200:~ $ coap-client-notls -m get -N -B 10 -t text coap://[fd12:3456::62a4:23ff:fe37:aee6]:5683/settings/parameter -e "network 0"
"network": "0",
"network_name": "network_0",
"network_size": "2",
"type": "1",
"reg_domain": "1",
"phy_mode_id": "5",
"chan_plan_id": "35",
"tx_power_ddbm": "200",
"max_child_count": "22",
"max_neighbor_count": "32",
"max_security_neighbor_count": "300",

pi@rns-wisun-200:~ $ coap-client-notls -m put -N -B 10 -t text coap://[fd12:3456::62a4:23ff:fe37:aee6]:5683/settings/parameter -e "network_name 0 large_test_network"
"network[0].network_name": "large_test_network"
pi@rns-wisun-200:~ $ coap-client-notls -m put -N -B 10 -t text coap://[fd12:3456::62a4:23ff:fe37:aee6]:5683/settings/parameter -e "reg_domain   0 3"
"network[0].reg_domain": "3"
pi@rns-wisun-200:~ $ coap-client-notls -m put -N -B 10 -t text coap://[fd12:3456::62a4:23ff:fe37:aee6]:5683/settings/parameter -e "phy_mode_id  0 3"
"network[0].phy_mode_id": "3"
pi@rns-wisun-200:~ $ coap-client-notls -m put -N -B 10 -t text coap://[fd12:3456::62a4:23ff:fe37:aee6]:5683/settings/parameter -e "chan_plan_id 0 33"
"network[0].chan_plan_id": "33"
pi@rns-wisun-200:~ $ coap-client-notls -m get -N -B 10 -t text coap://[fd12:3456::62a4:23ff:fe37:aee6]:5683/settings/parameter -e "network 0"
"network": "0",
"network_name": "large_test_network",
"network_size": "2",
"type": "1",
"reg_domain": "3",
"phy_mode_id": "3",
"chan_plan_id": "33",
"tx_power_ddbm": "200",
"max_child_count": "22",
"max_neighbor_count": "32",
"max_security_neighbor_count": "300"
pi@rns-wisun-200:~ $
pi@rns-wisun-200:~ $ coap-client-notls -m put -N -B 10 -t text coap://[fd12:3456::62a4:23ff:fe37:aee6]:5683/settings/parameter -e "network_index 0"
"network_index": "0"
pi@rns-wisun-200:~ $ coap-client-notls -m put -N -B 10 -t text coap://[fd12:3456::62a4:23ff:fe37:aee6]:5683/settings/parameter -e "reboot 5000"
```
