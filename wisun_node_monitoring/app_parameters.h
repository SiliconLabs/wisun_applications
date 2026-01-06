/***************************************************************************//**
* @file app_parameters.h
* @brief header file for application parameters (saved/retrieved from NVM)
* @version 1.0.0
*******************************************************************************
* # License
* <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
*******************************************************************************
*
* SPDX-License-Identifier: Zlib
*
* The licensor of this software is Silicon Laboratories Inc.
*
* This software is provided \'as-is\', without any express or implied
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
*******************************************************************************
*
* EXPERIMENTAL QUALITY
* This code has not been formally tested and is provided as-is.  It is not suitable for production environments.
* This code will not be maintained.
*
******************************************************************************/

#ifndef APP_PARAMETERS_H
#define APP_PARAMETERS_H
// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>

#include "nvm3_default.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
#define NVM3_APP_KEY   0xf013

#if __has_include("ltn_config.h")
#include "ltn_config.h"
#endif

#ifndef DEFAULT_NETWORK_INDEX
  #define DEFAULT_NETWORK_INDEX 0
#endif /* DEFAULT_NETWORK_INDEX */

#ifndef   TX_POWER_DDBM
  #ifndef WISUN_CONFIG_TX_POWER
    #define TX_POWER_DDBM  0
  #else /* WISUN_CONFIG_TX_POWER */
    #define TX_POWER_DDBM  WISUN_CONFIG_TX_POWER
  #endif /* WISUN_CONFIG_TX_POWER */
#endif /* TX_POWER_DDBM */
#ifndef   NVM3_APP_PARAMS_VERSION
  #define NVM3_APP_PARAMS_VERSION   10000
#endif /* NVM3_APP_PARAMS_VERSION */
#ifndef   AUTO_SEND_SEC
  #define AUTO_SEND_SEC             15*60
#endif /* AUTO_SEND_SEC */
#ifndef   PREFERRED_PAN_ID
  #define PREFERRED_PAN_ID         0xffff
#endif /* PREFERRED_PAN_ID */
#ifndef   DEVICE_TYPE
  #define DEVICE_TYPE          SL_WISUN_ROUTER
#endif /* DEVICE_TYPE */
#ifndef   SET_LEAF
  #define SET_LEAF                      0
#endif /* SET_LEAF */
#ifndef   MAX_CHILD_COUNT
  #define MAX_CHILD_COUNT              22
#endif /* MAX_CHILD_COUNT */
#ifndef   MAX_NEIGHBOR_COUNT
  #define MAX_NEIGHBOR_COUNT           32
#endif /* MAX_NEIGHBOR_COUNT */
#ifndef   MAX_SECURITY_NEIGHBOR_COUNT
  #define MAX_SECURITY_NEIGHBOR_COUNT 500
#endif /* MAX_SECURITY_NEIGHBOR_COUNT */

#ifndef UDP_NOTIFICATION_DEST
  #define UDP_NOTIFICATION_DEST  "fd00:6172:6d00::1" // fixed IPv6 string
#endif /* UDP_NOTIFICATION_DEST */

#ifndef COAP_NOTIFICATION_DEST
  #define COAP_NOTIFICATION_DEST "fd00:6172:6d00::2" // fixed IPv6 string
#endif /* COAP_NOTIFICATION_DEST */


#ifndef UDP_NOTIFICATION_DEST_2
  #define UDP_NOTIFICATION_DEST_2  "fd00:6172:6d00::3" // fixed IPv6 string
#endif /* UDP_NOTIFICATION_DEST */

#ifndef COAP_NOTIFICATION_DEST_2
  #define COAP_NOTIFICATION_DEST_2 "fd00:6172:6d00::4" // fixed IPv6 string
#endif /* COAP_NOTIFICATION_DEST */

#define MAX_NETWORK_CONFIGS 3
#define APP_UTIL_PRINTABLE_DATA_MAX_LENGTH 64

// app_settings_wisun_t structure similar to Wi-SUN SoC CLI (only FAN1.1 support)
typedef struct {
  char allowed_channels[APP_UTIL_PRINTABLE_DATA_MAX_LENGTH+1];
  char network_name[SL_WISUN_NETWORK_NAME_SIZE+1];
//  uint8_t operating_class;
//  uint16_t operating_mode;
  uint8_t network_size;
  int16_t tx_power_ddbm;
  uint8_t uc_dwell_interval_ms;
//  uint16_t number_of_channels;
//  uint32_t ch0_frequency;
//  uint16_t channel_spacing; // channel spacing in kHz
//  uint8_t trace_filter[SL_WISUN_FILTER_BITFIELD_SIZE];
  sl_wisun_regulation_t regulation;
  int8_t regulation_warning_threshold;
  int8_t regulation_alert_threshold;
  sl_wisun_device_type_t device_type;
  sl_wisun_phy_config_t phy;
//  uint8_t fec;
#if       SL_RAIL_IEEE802154_SUPPORTS_G_MODE_SWITCH // see sl_wisun_set_pom_ie
  uint8_t rx_phy_mode_ids[SL_WISUN_MAX_PHY_MODE_ID_COUNT];
  uint8_t rx_phy_mode_ids_count;
  uint8_t rx_mdr_capable;
#endif /* SL_RAIL_IEEE802154_SUPPORTS_G_MODE_SWITCH */
//  uint16_t protocol_id;
//  uint16_t channel_id;
#ifdef    SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT
  sl_wisun_lfn_profile_t lfn_profile;
#endif /* SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT */
//  uint8_t crc_type;
//  uint8_t preamble_length;
//  uint8_t stf_length;
  /* sl_wisun_config_neighbor_table parameters
  * max_security_neighbor_count(300) >= max_neighbor_count(32) > max_child_count(22)
  * Each entry in the neighbor table consumes about 450 bytes of RAM.
  * Each entry in the security neighbor table consumes about 50 bytes of RAM.
  */
  uint8_t max_neighbor_count;
  uint8_t max_child_count;
  uint16_t max_security_neighbor_count;
  uint16_t preferred_pan_id;
  uint8_t keychain;
  uint8_t keychain_index;
//  uint8_t direct_connect_pmk[SL_WISUN_PMK_LEN];
  uint8_t max_hop_count;
  uint8_t  set_leaf;             // LEAF mode flag
  uint16_t lowpan_mtu;
  uint16_t ipv6_mru;
  uint8_t max_edfe_fragment_count;
  char udp_notification_dest[41];
  char coap_notification_dest[41];
//  uint16_t socket_rx_buffer_size;
//  char eap_identity[SL_WISUN_EAP_IDENTITY_SIZE+1];
} app_settings_wisun_t;

// Wi-SUN settings to join
typedef struct app_wisun_join_settings {
  char    network_name[SL_WISUN_NETWORK_NAME_SIZE + 1];
  uint8_t network_size;
  sl_wisun_phy_config_t phy;
} app_wisun_network_settings_t;

// Application parameters
typedef struct {
  uint32_t app_params_version;   // Read at boot, set all to defaults
                                 //  if not matching NVM3_APP_VERSION
                                 //    This is to avoid clearing the Wi-SUN stack cache
  uint16_t nb_boots;             // Number of reboots since last NVM clear
  uint16_t nb_crashes;           // Number of crashes since last NVM clear
  uint16_t auto_send_sec;        // Notification period in seconds
  uint8_t  network_count;        // Number of network settings
  uint8_t  network_index;        // Selector for network settings
} app_wisun_parameters_t;

extern app_settings_wisun_t network[MAX_NETWORK_CONFIGS];

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
extern app_wisun_parameters_t app_parameters;

// -----------------------------------------------------------------------------
//                          Public Function Declarations
// -----------------------------------------------------------------------------

void app_parameter_mutex_acquire();
void app_parameter_mutex_release();

// NVM3 Init/Read/Write of key NVM3_APP_KEY
sl_status_t init_app_parameters();
sl_status_t read_app_parameters();
sl_status_t save_app_parameters();
sl_status_t delete_app_parameters();

// Set and Print application parameters
void        print_network_parameters(int network_index);
char*       network_string(int i);
void        print_app_parameters();
char*       app_parameters_string();
void        set_app_parameters_defaults();
sl_status_t set_app_parameter(char* parameter_name, int index, uint32_t value, char* value_str);
sl_status_t get_app_parameter(char* parameter_name, int index, uint32_t* value, char* value_str);

#endif  // APP_PARAMETERS_H
