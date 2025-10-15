#include <stdio.h>
#include <string.h>

#include "sl_wisun_api.h"
#include "sl_string.h"
#include "app_timestamp.h"
#include "app_rtt_traces.h"

#ifndef APP_WISUN_MULTICAST_OTA_H
#define APP_WISUN_MULTICAST_OTA_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MULTICAST_OTA_STORE_IN_FLASH
#define MULTICAST_OTA_STORE_IN_FLASH 1
#endif /* MULTICAST_OTA_STORE_IN_FLASH */

#define MAX_CHUNKS     1024
#define MAX_DATA_BYTES 1232

void clear_ota_data();

uint32_t last_index_rx(void);

uint32_t last_index();

void show_udp_data_for_index(int udp_chunk_index, uint8_t mode);

void show_udp_data(uint8_t mode);

uint32_t list_missed();

sl_status_t show_missed_from_list();

void show_missed();

char* missed_chunks();

char* rx_chunks();

char* ota_multicast_info();

char* now_timestamp();

int multicast_rx(char* udp_buff, uint32_t received_bytes);

#ifdef __cplusplus
}
#endif

#endif // APP_WISUN_MULTICAST_OTA_H
