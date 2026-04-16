# Wi-SUN Multicast OTA code

The present code is at **experimental** level.

It showcases **Wi-SUN OTA multicast** using

- [multicast_ota.py](linux_border_router_wsbrd/multicast_ota.py), a Python script running on the Linux host to send a selected FW file by chunks of 1024 bytes (matching the External SPI Flash page size), one chunk every n seconds
- [app_wisun_multicast_ota.c](app_wisun_multicast_ota.c) the code to be added to the application to receive/store the chunks in external SPI Flash
- [app_wisun_multicast_ota.h](linux_border_router_wsbrd/app_wisun_multicast_ota.h) the matching header file

---

## Table of Content

- [Wi-SUN Multicast OTA code](#wi-sun-multicast-ota-code)
  - [Table of Content](#table-of-content)
  - [How it works](#how-it-works)
    - [New firmware Development](#new-firmware-development)
    - [New Firmware Transmission](#new-firmware-transmission)
    - [Firmware chunk reception](#firmware-chunk-reception)
    - [Transmission checking](#transmission-checking)
    - [Retransmitting missing chunks](#retransmitting-missing-chunks)
    - [Applying the new Firmware](#applying-the-new-firmware)
      - [Checking the current firmware version](#checking-the-current-firmware-version)
      - [Verify/Set/Install](#verifysetinstall)
      - [Checking the new firmware version](#checking-the-new-firmware-version)
  - [Setting the code up](#setting-the-code-up)
    - [Adding multicast OTA to the application code](#adding-multicast-ota-to-the-application-code)
  - [Seeing it in Action](#seeing-it-in-action)

---

## How it works

The Wi-SUN Multicast OTA PoC requires:

- A bootloader with `lzma` compression support (lzma-compressed FW files are about 40% smaller than non-compressed files). To create and flash bootloader, refer to [OTA DFU Bootloader Application doc](https://docs.silabs.com/wisun/latest/wisun-ota-dfu/#bootloader-application).
  - `lz4` (byte-oriented compression) is also available as part of the Platform/Bootloader/Core compression options.
  - `lzma` is selected here because of superior compression capabilities due to its dictionary-based compression scheme.
- An application with
  - The Wi-SUN OTA DFU component
    - The code re-uses part of the available CoAP OTA control and status URIs.
  - The bootloader interface component

It is implemented on top of the [Wi-SUN Node Monitoring application](https://github.com/SiliconLabs/wisun_applications/tree/main) and can be ported to any Wi-SUN application.

### New firmware Development

- A new application firmware is prepared and built for selected devices.
  - It is highly recommended to change the `application` and/or `version` strings to allow checking the firmware version after an update.
    >NB: the `version` string is automatically updated with the compilation date and time if `app.c` is recompiled. If not, performing a 'clean' on the project is a good way to make sure it's updated.
- The FW is converted to lzma-compressed `.gbl` format using
  - `commander.exe gbl3 create  --compress lzma --app <new_fw>.s37 <new_fw>.gbl`
- The GBL file is copied on the linux host to the `/srv/tftp` folder
  - Storing to a temporary folder (`/tmp`) is often required because of access rights.
  - Copying to `/srv/tftp` can then be done locally with the correct credentials.
- The `/ota/dfu -e gbl <filename>` command is sent over CoAP to each device that we want to update, in order to set the name of the new firmware to accept.
  >If another name is present in the received payload, the chunk is discarded by the application (the Wi-SUN stack still forwards it to other nodes).

### New Firmware Transmission

From the linux host, we use coap to set **GBL File name** for all nodes:

```bash
Multicast:
coap-client -m post -N -B 4 -t text coap://[ff03::01]:5683/ota/dfu  -e "gbl xG25_12_4_lzma.gbl"
```

From the linux host, we use the [multicast_ota.py](linux_border_router_wsbrd/multicast_ota.py) python script to :

- Prepare firmware download (Clear OTA data from a previous download)

```bash
python multicast_ota.py <ipv6>    <UDP_port>  <function_name>      <tag>     <seconds_between_chunks> <unused> [options]
python multicast_ota.py ff03::01  7777     "clear_ota_data()"    BRD4271A        1                       0
```

- send firmware by chunk

```bash
python multicast_ota.py <ipv6>    <UDP_port>  <gbl_filename>      <tag>     <seconds_between_chunks> <chunk_index> [options]
python multicast_ota.py ff03::01  7777        xG25_12_4_lzma.gbl  BRD4271A  45                       0
```

(when `<chunk_index>` == `0`, all chunks are transmitted, otherwise only the chunk specified will be sent)

- [multicast_ota.py](linux_border_router_wsbrd/multicast_ota.py) reads the `gbl` file (by default from the [/srv/tftp/](linux_border_router_wsbrd/multicast_ota.py#L162) folder).
  - Each 1024 bytes chunk is sent to `ff03::01` on UDP port `7777` for multicast OTA to all 'realm_local_nodes', i.e. all nodes (FFN + LFN) in the Wi-SUN network.
    >Use `ff03::02` for 'realm_local_routers', i.e. all FFN nodes in the Wi-SUN network.
  - A unicast IPv6 address can also be used to update a single device.
    - This is convenient for
      - Testing on a single device before going for a network-wise multicast update.
      - Sending missing chunks to a single device to complete file transfer
  - Each chunk is transmitted with a header containing `OTA {gbl_filename} {chunk_index} {chunk_data_offset} {tx_timestamp} {tag}`, where:
    - `OTA` is a fixed string used to route received UDP packets to [multicast_rx()](app_wisun_multicast_ota.c#L431) from the UDP server.
    - `gbl_filename` is the filename as set by `/ota/dfu -e gbl <gbl_filename>`
      - Setting this individually allows selecting which application will run on which device
    - `chunk_index` is the number of the chunk. It is used to:
      - Store the chunk at the proper location
      - Set the number of bytes received per chunk in the `udp_chunk_rx_count[]` array.
        - This is used later to return the list of missed chunks
        - It is also used to avoid storing already received chunks
    - `chunk_data_offset` is the offset of the first data byte in the received payload.
    - `tx_timestamp` is the time the chunk has been sent by the host. It is useful to assess the multicast propagation time in the network.
    - `tag` is a user-defined string which must match the `expected_tag` hardcoded in the application (by default, set to `SL_BOARD_NAME`)
      - `tag` can be considered as the 'hardware identifier' used to avoid having some devices store firmware they can't use, without any control required by the network manager, even thought the `gbl_filename` is identical.
  - A delay of 45 seconds between chunks is recommended when transmitting to `ff03::01`, based on multicast tests done with various combinations of settings. It gives reliable results when used together with the optimized Border Router and device settings.

### Firmware chunk reception

On the device side:

- In [app_udp_server.c/_udp_handle_rx_payload()](app_udp_server.c#L213), a UDP message received with `OTA ` as the first 4 bytes is sent to [multicast_rx()](app_wisun_multicast_ota.c#L431),
- In [`multicast_rx()`](app_wisun_multicast_ota.c#L431), the received message is
  - Parsed and checked to make sure it contains
    - ['OTA' on line 469](app_wisun_multicast_ota.c#L469)
    - A matching ['gbl_file' on line 475](app_wisun_multicast_ota.c#L475)
    - A matching ['tag_str' on line 477](app_wisun_multicast_ota.c#L477)
    - if one of these is not matching, the chunk is not taken into account
  - If ok, the `chunk_index` is [checked to make sure it's not a duplicate on line 493](app_wisun_multicast_ota.c#L493) (which can easily happen in multicast)
  - If it's the first reception of this chunk, the raw data content is [stored in Flash on line 503](app_wisun_multicast_ota.c#L503)

### Transmission checking

Once all chunks have been sent from the host, it is necessary to check if all chunks have been received by each device.

To do that, you can use the added `/multicast_ota/missed` CoAP command (device per device) to check the results.

```bash
coap-client -m get -N -B 10 -t text coap://[fd12:3456::da7a:3bff:fe41:75ba]:5683/multicast_ota/missed
```

The resulting string is similar to

```text
[device_id] [missed/max index received] missed chunks   [list of missed chunks]
[75ba]      7/392                       missed chunks   6 212 247 286 313 343 388
```

Or, once all chunks are received

```text
[75ba]   0/392 missed chunks
```

### Retransmitting missing chunks

Using the chunk_index it is easy to send a single chunk and store it at the proper Flash location. This is used to retransmit missing chunks until there is no missing chunk.

It is recommended to check on several devices which chunks are missing, and decide which strategy is best to send these.

- If many devices missed the same chunks, it is interesting to send them again using `ff03::01`.
  - Devices having already stored a chunk will disregard it anyway.
- If only a single device misses a couple chunks, it is better to use its unicast IPv6 address.
  - In this case, a much shorter delay (a couple seconds) can be used between chunks.

If there are missing chunks, it's possible to use [multicast_ota.py](linux_border_router_wsbrd/multicast_ota.py) options to send

- A single chunk (`only` option)
  - `multicast_ota.py  <ipv6>  7777   xG25_12_4_lzma.gbl  BRD4271A  45  6  only`
    >This needs to be repeated for all missing chunks
- All chunks above a given chunk index  (`min` option)
  - `multicast_ota.py  <ipv6>  7777   xG25_12_4_lzma.gbl  BRD4271A  45  225  min`
    >This is particularly useful if, for any reason, transmission has been stopped on the host
- Several chunks (`and` option)
  - `multicast_ota.py  <ipv6>  7777   xG25_12_4_lzma.gbl  BRD4271A  45  6  and  212 247 286 313 343 388`
    >This is the most commonly used option, used to send all missing chunks in a single call

This process ends once all chunks are received, and the reply to `/multicast_ota/missed` is

  ```text
  [ad36]   0/392 missed chunks
  ```

with a number of received chunks matching the FW size (each chunk contains 1024 bytes).

Once this is complete, the firmware transmission phase in complete, and we can move to the next phase, where the new firmware will be applied.

### Applying the new Firmware

#### Checking the current firmware version

```bash
pi@rns-wisun-196:~ $ coap-client -m get -N -B 7 -t text 'coap://[fd12:3456::62a4:23ff:fe37:a511]:5683/info/all'
+ coap-client -m get -N -B 7 -t text 'coap://[fd12:3456::62a4:23ff:fe37:a511]:5683/info/all'
{
  "device": "a511",
  "chip": "xG25",
  "board": "BRD4271A",
  "device_type": "FFN with No LFN support",
  "application": "Wi-SUN Node Monitoring V3.2.0 2025_6 build 40 lzma 9.4",
  "version": "Compiled on May 22 2025 at 17:30:04"
  "MAC": "60:A4:23:FF:FE:37:A5:11"
}
```

#### Verify/Set/Install

To apply the new firmware on a device with no missing chunk, the interface is a bit crude for the time being. It can be refined later on, adding new `/multicast_ota` CoAP commands.

It uses the [multicast_ota.py](linux_border_router_wsbrd/multicast_ota.py) script with [specific strings](app_wisun_multicast_ota.c#L582) replacing the `<gbl_filename>`:

Verify image in flash (performed by bootloader by checking gbl format):

```bash
python multicast_ota.py <ipv6> <UDP_port> "verify_image_in_flash()" <unused> <unused> <unused>
```

Set image to bootload if image has been verified, once image has been set to bootload if device reset new application is applied by bootloader:

```bash
python multicast_ota.py <ipv6> <UDP_port> "setImageToBootload()" <unused> <unused> <unused>
```

Verify image, set image to bootload and reboot:

```bash
python multicast_ota.py <ipv6> <UDP_port> "rebootAndInstall()" <unused> <unused> <time_s_before_reboot>
python multicast_ota.py <ipv6> <UDP_port> "rebootAndInstallClearNVMApp()" <unused> <unused> <time_s_before_reboot>
python multicast_ota.py <ipv6> <UDP_port> "rebootAndInstallClearNVMFull()" <unused> <unused> <time_s_before_reboot>
```

#### Checking the new firmware version

```bash
pi@rns-wisun-196:~ $ coap-client -m get -N -B 7 -t text 'coap://[fd12:3456::62a4:23ff:fe37:a511]:5683/info/all'
+ coap-client -m get -N -B 7 -t text 'coap://[fd12:3456::62a4:23ff:fe37:a511]:5683/info/all'
{
  "device": "a511",
  "chip": "xG25",
  "board": "BRD4271A",
  "device_type": "FFN with No LFN support",
  "application": "Wi-SUN Node Monitoring V3.2.0 2025_6 build 40 lzma 10.4",
  "version": "Compiled on May 26 2025 at 12:20:23"
  "MAC": "60:A4:23:FF:FE:37:A5:11"
}
```

If the 'application' string has changed, then the update was successful

### Summary of a complete OTA multicast sequence

```bash
commander gbl3 create  --compress lzma --app xG25_12_4.s37 xG25_12_4_lzma.gbl

scp xG25_12_4_lzma.gbl <linux_user>@<linux_hostname>:/srv/tftp/

python multicast_ota.py ff03::01 7777 "clear_ota_data()" BRD4271A 1 0

coap-client -m post -N -B 4 -t text coap://[ff03::01]:5683/ota/dfu  -e "gbl xG25_12_4_lzma.gbl"

python multicast_ota.py ff03::01  7777  xG25_12_4_lzma.gbl  BRD4271A  45   0

=> check for missed packet on all nodes (example with one)
coap-client -m get -N -B 10 -t text coap://[fd12:3456::da7a:3bff:fe41:75ba]:5683/multicast_ota/missed
=> 0 missed

multicast_ota.py ff03::01 7777 "rebootAndInstall()" BRD4271A 1 10
```

## Setting the code up

### Adding multicast OTA to the application code

In the application code:

- Add OTA DFU component
- Add the multicast OTA code to the project (app_wisun_multicast_ota.c/.h)
- Add `app_wisun_multicast_ota.c` to the `cmake_gcc/CMakeLists.txt` file, in the `add_executable` section.

```C
add_executable(<project_name>
. . .
"../app_wisun_multicast_ota.c"
. . .
)
```

- Add a check for the presence of `OTA ` at the start of any UDP message in your UDP receiver, and route it to `multicast_rx()` if yes.
  - In the Wi-SUN Node Monitoring application, the corresponding code is already added in `app_udp_server.c/_udp_handle_rx_payload()`

```C
#ifdef APP_WISUN_MULTICAST_OTA_H
  if (strncmp(msg->buff, "OTA ", 4) == 0) {
    if (multicast_rx(msg->buff, msg->data_length, udp_ip_str) != 0) {
      sl_free((void *)udp_ip_str);
      return;
    }
  }
#endif /* APP_WISUN_MULTICAST_OTA_H */
```

**Note:** To perform a firmware update, a bootloader is required. Please refer to the [OTA DFU Bootloader Application doc](https://docs.silabs.com/wisun/latest/wisun-ota-dfu/#bootloader-application).

## Seeing it in Action

It all comes down to a single picture:

![multicast_ota.png](image/multicast_ota.png)
