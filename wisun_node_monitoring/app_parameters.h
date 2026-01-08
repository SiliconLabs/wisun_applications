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
#define IPV6_STR_LEN       41 // + trailing null
#define NVM3_APP_KEY   0xf013

#if __has_include("ltn_config.h")
  #include "ltn_config.h"
#endif

#ifndef   START_FLASHES_A
  #define START_FLASHES_A     3
#endif /* START_FLASHES_A */

#ifndef   START_FLASHES_B
  #define START_FLASHES_B     5
#endif /* START_FLASHES_B */

#ifndef   APP_VERSION_STRING
  #define APP_VERSION_STRING "F"
#endif /* APP_VERSION_STRING */

#ifndef   NVM3_APP_PARAMS_VERSION
  #define NVM3_APP_PARAMS_VERSION   10000
#endif /* NVM3_APP_PARAMS_VERSION */

#ifndef   MAX_NETWORK_CONFIGS
  #define MAX_NETWORK_CONFIGS 3
#endif /* MAX_NETWORK_CONFIGS*/

#ifndef   DEFAULT_NETWORK_INDEX
  #define DEFAULT_NETWORK_INDEX 0
#endif /* DEFAULT_NETWORK_INDEX */

#ifndef   MULTICAST_OTA_STORE_IN_FLASH
#define   MULTICAST_OTA_STORE_IN_FLASH 1
#endif /* MULTICAST_OTA_STORE_IN_FLASH */

#ifndef   SET_LEAF
#define   SET_LEAF 0
#endif /* SET_LEAF */


/* network */
#ifndef   NETWORK_NAMEs
  #define NETWORK_NAMEs                   {               "small_EU_3_33",                  "medium_EU_1_34",                         "large_EU_1_1" }
#endif /* NETWORK_NAMEs */

#ifndef   REG_DOMAINs
  #define REG_DOMAINs                     { SL_WISUN_REGULATORY_DOMAIN_EU,     SL_WISUN_REGULATORY_DOMAIN_EU,         SL_WISUN_REGULATORY_DOMAIN_EU }
#endif /* REG_DOMAINs */

#ifndef   PHY_MODE_IDs
  #define PHY_MODE_IDs                    {                             3,                                 1,                                     1 }
#endif /* PHY_MODE_IDs */

#ifndef   CHAN_PLAN_IDs
  #define CHAN_PLAN_IDs                   {                            33,                                34,                                     1 }
#endif /* CHAN_PLAN_IDs */

#ifndef   NETWORK_SIZEs
  #define NETWORK_SIZEs                   {   SL_WISUN_NETWORK_SIZE_SMALL,      SL_WISUN_NETWORK_SIZE_MEDIUM,           SL_WISUN_NETWORK_SIZE_LARGE }
#endif /* NETWORK_SIZEs */

#ifndef   PREFERRED_PAN_IDs
  #define PREFERRED_PAN_IDs               {                        0xffff,                            0xffff,                                0xffff }
#endif /* PREFERRED_PAN_IDs */

#ifndef   REGULATIONs
  #define REGULATION  SL_WISUN_REGULATION_NONE
#endif /* REGULATIONs */

#ifndef   REGULATION_WARNING_THRESHOLD
  #define REGULATION_WARNING_THRESHOLD 50
#endif /* REGULATION_WARNING_THRESHOLD */

#ifndef   REGULATION_ALERT_THRESHOLD
  #define REGULATION_ALERT_THRESHOLD 100
#endif /* REGULATION_ALERT_THRESHOLD */

/* device */
#ifndef   DEVICE_TYPEs
  #define DEVICE_TYPEs                    {               SL_WISUN_ROUTER,                   SL_WISUN_ROUTER,                       SL_WISUN_ROUTER }
#endif /* DEVICE_TYPEs */

#ifndef   TX_POWER_DDBMs
  #define TX_POWER_DDBMs                  {                             0,                                 1,                                     2 }
#endif /* TX_POWER_DDBMs */

#ifndef   MAX_CHILD_COUNTs
  #define MAX_CHILD_COUNTs                {                            22,                                22,                                    22 }
#endif /* MAX_CHILD_COUNTs */

#ifndef   MAX_NEIGHBOR_COUNTs
  #define MAX_NEIGHBOR_COUNTs             {                            32,                                32,                                    32 }
#endif /* MAX_NEIGHBOR_COUNTs */

#ifndef   MAX_SECURITY_NEIGHBOR_COUNTs
  #define MAX_SECURITY_NEIGHBOR_COUNTs    {                           500,                               500,                                   500 }
#endif /* MAX_SECURITY_NEIGHBOR_COUNTs */

/* Application */
#ifndef   AUTO_SEND_SECs
  #define AUTO_SEND_SECs                  {                        (1*60),                            (5*60),                               (15*60) }
#endif /* AUTO_SEND_SECs */

#ifndef   UDP_NOTIFICATION_DESTINATIONs
  #define UDP_NOTIFICATION_DESTINATIONs   {           "fd00:6172:6d00::1",               "fd00:6172:6d00::1",                   "fd00:6172:6d00::1" }
#endif /* UDP_NOTIFICATION_DESTINATIONs */

#ifndef   COAP_NOTIFICATION_DESTINATIONs
  #define COAP_NOTIFICATION_DESTINATIONs  {           "fd00:6172:6d00::2",               "fd00:6172:6d00::2",                   "fd00:6172:6d00::2" }
#endif /* COAP_NOTIFICATION_DESTINATIONs */


#define APP_UTIL_PRINTABLE_DATA_MAX_LENGTH 64

// app_settings_wisun_t structure similar to Wi-SUN SoC CLI (only FAN1.1 support)
typedef struct {
  char allowed_channels[APP_UTIL_PRINTABLE_DATA_MAX_LENGTH+1];
  char network_name[SL_WISUN_NETWORK_NAME_SIZE+1];
//  uint8_t operating_class;
//  uint16_t operating_mode;
  uint8_t network_size;
  int16_t tx_power_ddbm;
  int16_t auto_send_sec;
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
  uint8_t  max_neighbor_count;
  uint8_t  max_child_count;
  uint16_t max_security_neighbor_count;
  uint16_t preferred_pan_id;
  uint8_t  keychain;
  uint8_t  keychain_index;
//  uint8_t direct_connect_pmk[SL_WISUN_PMK_LEN];
  uint8_t  max_hop_count;
  uint8_t  set_leaf;             // LEAF mode flag
  uint16_t lowpan_mtu;
  uint16_t ipv6_mru;
  uint8_t  max_edfe_fragment_count;
  char udp_notification_dest[IPV6_STR_LEN];
  char coap_notification_dest[IPV6_STR_LEN];
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
void        set_app_parameters_defaults(int network_indexes);
sl_status_t set_app_parameter(char* parameter_name, int index, uint32_t  value, char* value_str);
sl_status_t get_app_parameter(char* parameter_name, int index, uint32_t* value, char* value_str);

#endif  // APP_PARAMETERS_H
