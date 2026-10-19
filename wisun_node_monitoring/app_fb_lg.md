# First Breath and Last Gasp

`app_fb_lg.c` handles the Wi-SUN First Breath ready event and the Last Gasp test. Both send a UDP notification to the active network's `udp_notification_dest`, which defaults to `fd00:6172:6d00::1`. First Breath uses port `1238`; Last Gasp uses port `1239`.

## First Breath

First Breath is configured per network profile and enabled before the node joins. It is supported by router profiles.

For build-time defaults, set the desired values in `FIRST_BREATHs` in `app_parameters.h`.

To enable First Breath on a running node through CoAP, set the flag for the selected profile and save the settings to NVM. If you are unsure of the active network index, query it first:

```sh
coap-client-notls -m get -N -B 10 -t text coap://[<node-ip>]:5683/settings/parameter -e "network_index"
```

The following commands enable First Breath for network index `0` and save the setting:

```sh
coap-client-notls -m put -N -B 10 -t text coap://[<node-ip>]:5683/settings/parameter -e "first_breath 0 1"
coap-client-notls -m put -N -B 10 -t text coap://[<node-ip>]:5683/settings/parameter -e "save"
```

Start the receiver on the Border Router and leave it running:

```sh
python3 linux_border_router_wsbrd/first_breath_receiver.py
```

Then restart the node with reset button. After reboot, the node starts joining the network and sends a First Breath frame when the stack reports First Breath ready. For example, the receiver prints:

```text
Listening for First Breath notifications on [::]:1238
[<local-timestamp>] First Breath from [<node-ipv6>]: {
"first_breath":"true",
"running":"0-00:00:08",
"device":"a1b2"
}
```

## Last Gasp

It is a one-shot test scheduled through CoAP and is supported in FFN builds when the active profile is a router. The active network's `udp_notification_dest` must point to the Border Router running the receiver.

On the Border Router, start the receiver and leave it running:

```sh
python3 linux_border_router_wsbrd/last_gasp_receiver.py
```

From another terminal, schedule the test:

```sh
coap-client-notls -m put -N -B 10 -t text coap://[<node-ip>]:5683/test/lastgasp -e "1000 5000"
```

The first value is the delay between receiving the CoAP command and enabling Last Gasp and sending the frame. The second is the delay from sending the frame to reboot. For example, the receiver prints:

```text
Listening for Last Gasp notifications on [::]:1239
[<local-timestamp>] Last Gasp from [<node-ipv6>]: {
"last_gasp":"true",
"running":"1-02:05:08",
"device":"a1b2"
}
```

The node reboots after the selected reboot delay. Vary that delay to determine how much time the node needs to transmit the notification before rebooting. A manual reset does not run the Last Gasp test; use `/test/lastgasp` to enable Last Gasp and schedule the reboot.
