/***************************************************************************//**
* @file app_wisun_multicast_ota.h
 * @brief Receives multicast OTA firmware chunks over UDP, tracks download status,
 *        and schedules image installation through the bootloader.
*******************************************************************************
* # License
* <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
*******************************************************************************
*
* SPDX-License-Identifier: Zlib
*
* The licensor of this software is Silicon Laboratories Inc.
*
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
*
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
*
* 1. The origin of this software must not be misrepresented; you must not
*    claim that you wrote the original software. If you use this software
*    in a product, an acknowledgment in the product documentation would be
*    appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
*    misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
*
******************************************************************************
*
* EXPERIMENTAL QUALITY
* This code has not been formally tested and is provided as-is.  It is not suitable for production environments.
* This code will not be maintained.
*
******************************************************************************/
#ifndef   APP_WISUN_MULTICAST_OTA_H
#define   APP_WISUN_MULTICAST_OTA_H

#include <stdio.h>
#include <string.h>

#if __has_include("app_parameters.h")
  #include "app_parameters.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MULTICAST_OTA_STORE_IN_FLASH
#define MULTICAST_OTA_STORE_IN_FLASH 1
#endif /* MULTICAST_OTA_STORE_IN_FLASH */

#define MAX_CHUNKS     1024
#define MAX_DATA_BYTES 1232

sl_status_t delete_app_parameters(void);

void multicast_ota_init(void);

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

int multicast_rx(char* udp_buff, uint32_t received_bytes, const char* udp_ip_str);

#ifdef __cplusplus
}
#endif

#endif /* APP_MULTICAST_OTA_H */

