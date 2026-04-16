/***************************************************************************//**
* @file ltn_config.h
* @brief header file to have all default application parameters in a single header file
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

#ifndef LTN_CONFIG_H
#define LTN_CONFIG_H

#include "sl_wisun_connection_params_api.h"

// for easier comparison we use the same syntax below as in
// app_parameters.h
// app.c
// app_wisun_multicast_ota.h

#ifndef   START_FLASHES_A
  #define START_FLASHES_A     3
#endif /* START_FLASHES_A */

#ifndef   START_FLASHES_B
  #define START_FLASHES_B     6
#endif /* START_FLASHES_B */

#ifndef   APP_VERSION_STRING
  #define APP_VERSION_STRING "F"
#endif /* APP_VERSION_STRING */

#ifndef   NVM3_APP_PARAMS_VERSION
  #define NVM3_APP_PARAMS_VERSION   10011
#endif /* NVM3_APP_PARAMS_VERSION */

#ifndef   MAX_NETWORK_CONFIGS
  #define MAX_NETWORK_CONFIGS 4
#endif /* MAX_NETWORK_CONFIGS*/

#ifndef   DEFAULT_NETWORK_INDEX
  #define DEFAULT_NETWORK_INDEX 1
#endif /* DEFAULT_NETWORK_INDEX */

#ifndef   MULTICAST_OTA_STORE_IN_FLASH
#define   MULTICAST_OTA_STORE_IN_FLASH 1
#endif /* MULTICAST_OTA_STORE_IN_FLASH */

#ifndef   SET_LEAF
#define   SET_LEAF 0
#endif /* SET_LEAF */


/* network */
#ifndef   NETWORK_NAMEs
  #define NETWORK_NAMEs                   {          "large_test_network",                   "rns-wisun-197",                       "rns-wisun-197" ,                       "rns-wisun-197" }
#endif /* NETWORK_NAMEs */

#ifndef   REG_DOMAINs
  #define REG_DOMAINs                     { SL_WISUN_REGULATORY_DOMAIN_EU,     SL_WISUN_REGULATORY_DOMAIN_EU,         SL_WISUN_REGULATORY_DOMAIN_NA ,         SL_WISUN_REGULATORY_DOMAIN_EU }
#endif /* REG_DOMAINs */

#ifndef   PHY_MODE_IDs
  #define PHY_MODE_IDs                    {                             3,                                 1,                                    54 ,                                     1 }
#endif /* PHY_MODE_IDs */

#ifndef   CHAN_PLAN_IDs
  #define CHAN_PLAN_IDs                   {                            33,                                34,                                     4 ,                                    34 }
#endif /* CHAN_PLAN_IDs */

#ifndef   SPECIAL_CONNECT_PARAMs
  #define SPECIAL_CONNECT_PARAMs          {                            0,                                 0,                                     0,                                       0 }
#endif /* SPECIAL_CONNECT_PARAMs */

#ifndef   NETWORK_SIZEs
  #define NETWORK_SIZEs                   {   SL_WISUN_NETWORK_SIZE_LARGE,      SL_WISUN_NETWORK_SIZE_MEDIUM,           SL_WISUN_NETWORK_SIZE_SMALL ,           SL_WISUN_NETWORK_SIZE_SMALL }
#endif /* NETWORK_SIZEs */

#ifndef   PREFERRED_PAN_IDs
  #define PREFERRED_PAN_IDs               {                        0xffff,                            0xffff,                                0xffff ,                                0xffff }
#endif /* PREFERRED_PAN_IDs */

#ifndef   REGULATION
  #define REGULATION  SL_WISUN_REGULATION_NONE
#endif /* REGULATION */

#ifndef   REGULATION_WARNING_THRESHOLD
  #define REGULATION_WARNING_THRESHOLD 50
#endif /* REGULATION_WARNING_THRESHOLD */

#ifndef   REGULATION_ALERT_THRESHOLD
  #define REGULATION_ALERT_THRESHOLD 100
#endif /* REGULATION_ALERT_THRESHOLD */

/* device */
#ifndef   DEVICE_TYPEs
  #define DEVICE_TYPEs                    {               SL_WISUN_ROUTER,                      SL_WISUN_LFN,                       SL_WISUN_ROUTER ,                       SL_WISUN_ROUTER }
#endif /* DEVICE_TYPEs */

#ifndef   FAN_VERSIONs
  #define FAN_VERSIONs                    {      SL_WISUN_FAN_VERSION_1_1,             SL_WISUN_FAN_VERSION_1_1,               SL_WISUN_FAN_VERSION_1_1 ,             SL_WISUN_FAN_VERSION_1_1 }
#endif /* FAN_VERSIONs */

#ifdef    SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT
#ifndef   LFN_PROFILEs
  #define LFN_PROFILEs                    {      SL_WISUN_LFN_PROFILE_ECO,          SL_WISUN_LFN_PROFILE_TEST,        SL_WISUN_LFN_PROFILE_BALANCED ,        SL_WISUN_LFN_PROFILE_BALANCED }
#endif /* LFN_PROFILEs */
#endif /* SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT */

#ifndef   TX_POWER_DDBMs
  #define TX_POWER_DDBMs                  {                             0,                                 0,                                     0 ,                                     0 }
#endif /* TX_POWER_DDBMs */

#ifndef   MAX_CHILD_COUNTs
  #define MAX_CHILD_COUNTs                {                            22,                                22,                                    22 ,                                    22 }
#endif /* MAX_CHILD_COUNTs */

#ifndef   MAX_NEIGHBOR_COUNTs
  #define MAX_NEIGHBOR_COUNTs             {                            32,                                32,                                    32 ,                                    32 }
#endif /* MAX_NEIGHBOR_COUNTs */

#ifndef   MAX_SECURITY_NEIGHBOR_COUNTs
  #define MAX_SECURITY_NEIGHBOR_COUNTs    {                           500,                               500,                                   500 ,                                   500 }
#endif /* MAX_SECURITY_NEIGHBOR_COUNTs */

/* Application */
#ifndef   AUTO_SEND_SECs
  #define AUTO_SEND_SECs                  {                       (15*60),                            (1*60),                                (5*60) ,                                (1*60) }
#endif /* AUTO_SEND_SECs */

#ifndef   UDP_NOTIFICATION_DESTINATIONs
  #define UDP_NOTIFICATION_DESTINATIONs   {           "fd00:6172:6d00::1",               "fd00:6172:6d00::1",     "2001:db8:0:2:d47:e4c8:60ad:b4ab" ,                    "fd00:6172:6d00::1"}
#endif /* UDP_NOTIFICATION_DESTINATIONs */

#ifndef   COAP_NOTIFICATION_DESTINATIONs
  #define COAP_NOTIFICATION_DESTINATIONs  {           "fd00:6172:6d00::2",               "fd00:6172:6d00::2",     "2001:db8:0:2:d47:e4c8:60ad:b4ab" ,                    "fd00:6172:6d00::2"}
#endif /* COAP_NOTIFICATION_DESTINATIONs */

#ifndef SL_WISUN_PARAMS_PROFILE_SPECIAL
/// Special Profile for network
static const sl_wisun_connection_params_t sl_wisun_params_profile_special = {
  .version = SL_WISUN_PARAMS_API_VERSION,
  .discovery = {
    .trickle_pa = {
      .imin_s = 15,
      .imax_s = 60,
      .k = 1
    },
    .trickle_pas = {
      .imin_s = 15,
      .imax_s = 60,
      .k = 1
    },
    .eapol_target_min_sens = DBM_TO_RSL_RANGE(-60),
    .allow_skip = true
  },
  .configuration = {
    .trickle_pc = {
      .imin_s = 15,
      .imax_s = 60,
      .k = 1
    },
    .trickle_pcs = {
      .imin_s = 15,
      .imax_s = 60,
      .k = 1
    }
  },
  .eapol = {
    .sec_prot_trickle = {
      .imin_s = 0,
      .imax_s = 0,
      .k = 0,
    },
    .pmk_lifetime_m = 0,
    .ptk_lifetime_m = 0,
    .sec_prot_retry_timeout_s = 0,
    .initial_key_min_s = 0,
    .initial_key_max_s = 60,
    .initial_key_retry_min_s = 60,
    .initial_key_retry_max_s = 0,
    .initial_key_retry_max_limit_s = 180,
    .temp_min_timeout_s = 0,
    .gtk_request_imin_m = 0,
    .gtk_request_imax_m = 0,
    .gtk_max_mismatch_m = 64,
    .lgtk_max_mismatch_m = 60,
    .sec_prot_trickle_expirations = 0,
    .initial_key_retry_limit = 3,
    .allow_skip = true
  },
  .rpl = {
    .dao_txalg = {
      .rand = 0.1f,
      .max_delay_s = 1,
      .irt_s = 15,
      .mrt_s = 0,
      .mrd_s = 0,
      .mrc = 3,
    },
    .dis_max_delay_first_s = 2,
    .dis_max_delay_s = 300,
    .init_parent_selection_s = 10,
    .etx_probe_period_max_s = 30,
    .address_registration_lifetime_s = 2220,
    .etx_samples_init = 1,
    .etx_samples_refresh = 4,
    .candidate_parents_max = 5,
    .parents_max = 2,
  },
  .mpl = {
    .trickle = {
      .imin_s = 1,
      .imax_s = 10,
      .k = 8,
    },
    .seed_set_entry_lifetime_s = 180,
    .trickle_expirations = 2,
    .seed_id_type = 0,
  },
  .dhcp = {
    .sol_txalg = {
      .rand = 0.1f,
      .max_delay_s = 10,
      .irt_s = 10,
      .mrt_s = HOUR_TO_SEC(1),
      .mrd_s = 0,
      .mrc = 3,
    },
  },
  .lfn_parent = {
    .lfn_pan_timeout_m = 0,
    .lfn_lpc_retry_count = 5,
    .lfn_na_wait_duration_m = 0,
  },
  .misc = {
    .temp_link_min_timeout_s = 260,
    .pan_timeout_m = 30,
  },
  .direct_connect_eapol = {
    .pmk_lifetime_m = 0,
    .ptk_lifetime_m = 0,
    .sec_prot_retry_timeout_s = 0,
    .initial_key_min_s = 0,
    .initial_key_max_s = 3,
    .initial_key_retry_min_s = 10,
    .initial_key_retry_max_s = 0,
    .initial_key_retry_max_limit_s = 30,
    .gtk_request_imin_m = 0,
    .gtk_request_imax_m = 0,
    .gtk_max_mismatch_m = 64,
    .initial_key_retry_limit = 3,
    .allow_skip = false
  },
  .traffic = {
    .lowpan_mtu = 1576,
    .ipv6_mru = 1504,
    .max_edfe_fragment_count = 5,
  },
  .mac = {
    .backoff_period_us = 0, // calculate from PHY by default
    .min_be = 3,
    .max_be = 5,
    .max_cca_retries = 8,
    .max_frame_retries = 7,
  }
};
#define SL_WISUN_PARAMS_PROFILE_SPECIAL sl_wisun_params_profile_special
#endif /* SL_WISUN_PARAMS_PROFILE_SPECIAL */

#endif /* LTN_CONFIG_H */
