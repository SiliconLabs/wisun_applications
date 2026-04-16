/***************************************************************************//**
* @file app_parameters.c
* @brief Application parameters of the Wi-SUN Node Monitoring example
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
// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>

#include "printf.h"

#include "sl_common.h"
#include "sl_string.h"
#include "cmsis_nvic_virtual.h"
#include "nvm3_default_config.h"

#include "sl_wisun_types.h"
#include "sl_wisun_api.h"
#include "sl_wisun_config.h"

#include "app.h"

#if __has_include("app_parameters.h")
  #include "app_parameters.h"
#endif

#if __has_include("app_timestamp.h")
  #include "app_timestamp.h"
#endif

#if __has_include("app_rtt_traces.h")
  #include "app_rtt_traces.h"
#endif

#if __has_include("app_action_scheduler.h")
  #include "app_action_scheduler.h"
#endif

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

app_wisun_parameters_t app_parameters;
app_settings_wisun_t network[MAX_NETWORK_CONFIGS];
char res_string[1000];

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
// Application timestamp mutex
static osMutexId_t _app_parameters_mutex = NULL;

static const osMutexAttr_t _app_parameters_mutex_attr = {
  .name      = "AppParametersMutex",
  .attr_bits = osMutexRecursive,
  .cb_mem    = NULL,
  .cb_size   = 0U
};

/* Copies token into out, removing ONE surrounding quote pair if present.
 * Accepts:
 *          '2001:db8::1'
 *          "2001:db8::1"
 * Returns 0 on success, -1 on error/truncation.
 */
static int8_t unquote_ipv6(const char *in, char *out, size_t out_sz)
{
    size_t n;

    if (!in || !out || out_sz == 0) return -1;

    n = strlen(in);
    if (n == 0) { out[0] = '\0'; return -2; }

    /* Strip matching surrounding quotes */
    if (n >= 2 && ((in[0] == '\'' && in[n - 1] == '\'') ||
                   (in[0] == '\"' && in[n - 1] == '\"')))
    {
        in += 1;
        n  -= 2;
    }
    else{
        /* No surrounding quotes */
        return -3;
    }

    if (n >= out_sz) return -4; /* would truncate */

    memcpy(out, in, n);
    out[n] = '\0';
    return 0;
}

/* Mutex acquire */
void app_parameter_mutex_acquire(void)
{
  assert(osMutexAcquire(_app_parameters_mutex, osWaitForever) == osOK);
}

/* Mutex release */
void app_parameter_mutex_release(void)
{
  assert(osMutexRelease(_app_parameters_mutex) == osOK);
}

void print_network_parameters(int network_index) {
  int i = network_index;
  printfBoth("network[%d] network_name     %s\n"           , i, network[i].network_name);
  printfBoth("network[%d] FAN type         %ld\n"          , i, network[i].phy.type);
  printfBoth("network[%d] use_special_connect_param %d\n"  , i, network[i].use_special_connect_param);
  printfBoth("network[%d] network_size     %d\n"           , i, network[i].network_size);
  printfBoth("network[%d] Phy type         %ld\n"          , i, network[i].phy.type);
  printfBoth("network[%d] reg_domain       %d\n"           , i, network[i].phy.config.fan11.reg_domain);
  printfBoth("network[%d] phy_mode_id      %d\n"           , i, network[i].phy.config.fan11.phy_mode_id);
  printfBoth("network[%d] chan_plan_id     %d\n"           , i, network[i].phy.config.fan11.chan_plan_id);
  printfBoth("network[%d] device_type      %d\n"           , i, network[i].device_type);
#ifdef    SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT
  printfBoth("network[%d] lfn_profile      %d\n"           , i, network[i].lfn_profile);
#endif /* SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT */
  printfBoth("network[%d] auto_send_sec               %d\n", i, network[i].auto_send_sec);
  printfBoth("network[%d] tx_power_ddbm               %d\n", i, network[i].tx_power_ddbm);
  printfBoth("network[%d] rx_fifo_size                %d\n", i, network[i].rx_fifo_size);
  printfBoth("network[%d] fan_version                 %d\n", i, network[i].fan_version);
  printfBoth("network[%d] max_child_count             %d\n", i, network[i].max_child_count);
  printfBoth("network[%d] max_neighbor_count          %d\n", i, network[i].max_neighbor_count);
  printfBoth("network[%d] max_security_neighbor_count %d\n", i, network[i].max_security_neighbor_count);
  printfBoth("network[%d] udp_notification_dest  %s\n"     , i, network[i].udp_notification_dest);
  printfBoth("network[%d] coap_notification_dest %s\n"     , i, network[i].coap_notification_dest);
}

char* network_string(int i) {
  #define NETWORK_FORMAT_STR \
  "\"network\": \"%d\",\n" \
  "\"network_name\": \"%s\",\n" \
  "\"udp_notification_dest\": \"%s\",\n" \
  "\"coap_notification_dest\": \"%s\",\n" \
  "\"use_special_connect_param\": \"%d\",\n" \
  "\"network_size\": \"%d\",\n" \
  "\"phy_type\": \"%ld\",\n" \
  "\"reg_domain\": \"%d\",\n" \
  "\"phy_mode_id\": \"%d\",\n" \
  "\"chan_plan_id\": \"%d\",\n" \
  "\"tx_power_ddbm\": \"%d\",\n" \
  "\"max_child_count\": \"%d\",\n" \
  "\"max_neighbor_count\": \"%d\",\n" \
  "\"max_security_neighbor_count\": \"%d\""
  snprintf(res_string, 1000, NETWORK_FORMAT_STR,
          i,
          network[i].network_name,
          network[i].udp_notification_dest,
          network[i].coap_notification_dest,
          network[i].use_special_connect_param,
          network[i].network_size,
          network[i].phy.type,
          network[i].phy.config.fan11.reg_domain,
          network[i].phy.config.fan11.phy_mode_id,
          network[i].phy.config.fan11.chan_plan_id,
          network[i].tx_power_ddbm,
          network[i].max_child_count,
          network[i].max_neighbor_count,
          network[i].max_security_neighbor_count);
  printf("[%d]%s\n", __LINE__, res_string);
  return res_string;
}

void print_app_parameters() {
  int i;
  printf("\n");
  printfBoth("app_parameters.app_params_version          %ld\n", app_parameters.app_params_version);
  printfBoth("app_parameters.nb_boots                    %d\n", app_parameters.nb_boots);
  printfBoth("app_parameters.nb_crashes                  %d\n", app_parameters.nb_crashes);
  printfBoth("app_parameters.network_count               %d\n", app_parameters.network_count);
  printfBoth("app_parameters.network_index               %d\n", app_parameters.network_index);
  printfBoth("app_parameters.network_struct_size         %d\n", app_parameters.network_struct_size);
  printf("\n");
  printf("network parameters (from app_parameters.h)\n");
  for (i=0; i<MAX_NETWORK_CONFIGS; i++) {
    print_network_parameters(i);
    printf("\n");
  }
}

char* app_parameters_string() {
  #define PARAMETERS_FORMAT_STR \
  "\"app_params_version\": \"%ld\",\n" \
  "\"nb_boots\": \"%d\",\n" \
  "\"nb_crashes\": \"%d\",\n" \
  "\"network_count\": \"%d\",\n" \
  "\"network_index\": \"%d\", \n" \
  "\"network_struct_size\": \"%d\"" \

  snprintf(res_string, 1000, PARAMETERS_FORMAT_STR,
          app_parameters.app_params_version,
          app_parameters.nb_boots,
          app_parameters.nb_crashes,
          app_parameters.network_count,
          app_parameters.network_index,
          app_parameters.network_struct_size);
  printf("[%d]%s\n", __LINE__, res_string);
  return res_string;
}

void set_app_parameters_defaults(int network_indexes) {
  // settings 'arrays' defined per network
  const char*                      NETWORK_NAME[MAX_NETWORK_CONFIGS] = NETWORK_NAMEs;
  const sl_wisun_regulatory_domain_t REG_DOMAIN[MAX_NETWORK_CONFIGS] = REG_DOMAINs;
  const uint8_t                     PHY_MODE_ID[MAX_NETWORK_CONFIGS] = PHY_MODE_IDs;
  const uint8_t                    CHAN_PLAN_ID[MAX_NETWORK_CONFIGS] = CHAN_PLAN_IDs;
  const uint8_t           SPECIAL_CONNECT_PARAM[MAX_NETWORK_CONFIGS] = SPECIAL_CONNECT_PARAMs;
  const sl_wisun_network_size_t    NETWORK_SIZE[MAX_NETWORK_CONFIGS] = NETWORK_SIZEs;
  const uint16_t               PREFERRED_PAN_ID[MAX_NETWORK_CONFIGS] = PREFERRED_PAN_IDs;
  const sl_wisun_device_type_t      DEVICE_TYPE[MAX_NETWORK_CONFIGS] = DEVICE_TYPEs;
  const sl_wisun_fan_version_t      FAN_VERSION[MAX_NETWORK_CONFIGS] = FAN_VERSIONs;
#ifdef    SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT
  const uint8_t                     LFN_PROFILE[MAX_NETWORK_CONFIGS] = LFN_PROFILEs;
#endif /* SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT */
  const uint8_t                   TX_POWER_DDBM[MAX_NETWORK_CONFIGS] = TX_POWER_DDBMs;
  const uint8_t                 MAX_CHILD_COUNT[MAX_NETWORK_CONFIGS] = MAX_CHILD_COUNTs;
  const uint8_t              MAX_NEIGHBOR_COUNT[MAX_NETWORK_CONFIGS] = MAX_NEIGHBOR_COUNTs;
  const uint16_t    MAX_SECURITY_NEIGHBOR_COUNT[MAX_NETWORK_CONFIGS] = MAX_SECURITY_NEIGHBOR_COUNTs;
  const uint16_t                  AUTO_SEND_SEC[MAX_NETWORK_CONFIGS] = AUTO_SEND_SECs;
  const char*      UDP_NOTIFICATION_DESTINATION[MAX_NETWORK_CONFIGS] = UDP_NOTIFICATION_DESTINATIONs;
  const char*     COAP_NOTIFICATION_DESTINATION[MAX_NETWORK_CONFIGS] = COAP_NOTIFICATION_DESTINATIONs;
  int i;

  // settings defined once for all networks
  app_parameters.app_params_version = NVM3_APP_PARAMS_VERSION;
  app_parameters.network_count      = MAX_NETWORK_CONFIGS;
  app_parameters.network_index      = DEFAULT_NETWORK_INDEX;
  app_parameters.network_struct_size = sizeof(app_settings_wisun_t);

  printfBoth("sizeof(app_wisun_parameters_t) %d\n", sizeof(app_wisun_parameters_t));

  // Prepare to init both if none is selected, selecting the first
  if (network_indexes == 0) {
      for (i=0; i<MAX_NETWORK_CONFIGS; i++) {
          network_indexes = network_indexes + (1 << i);
      }
      printfBoth("network_indexes 0x%02x\n", network_indexes);
  }
  // fill all networks, so that only diffs are required later on
  for (i = 0; i < MAX_NETWORK_CONFIGS; i++) {
    if (network_indexes & (1 << i)) {
      printfBoth("Network %d defaults\n", i);
      /* network */
      if (i == DEFAULT_NETWORK_INDEX) {
        snprintf(network[i].network_name, SL_WISUN_NETWORK_NAME_SIZE, "%s", WISUN_CONFIG_NETWORK_NAME);
        network[i].phy.config.fan11.reg_domain   = WISUN_CONFIG_REGULATORY_DOMAIN;
        network[i].phy.config.fan11.phy_mode_id  = WISUN_CONFIG_PHY_MODE_ID;
        network[i].phy.config.fan11.chan_plan_id = WISUN_CONFIG_CHANNEL_PLAN_ID;
        network[i].network_size                  = WISUN_CONFIG_NETWORK_SIZE;
      } else {
        snprintf(network[i].network_name, SL_WISUN_NETWORK_NAME_SIZE, "%s", NETWORK_NAME[i]);
        network[i].phy.config.fan11.reg_domain   = REG_DOMAIN[i];
        network[i].phy.config.fan11.phy_mode_id  = PHY_MODE_ID[i] ;
        network[i].phy.config.fan11.chan_plan_id = CHAN_PLAN_ID[i];
        network[i].network_size                  = NETWORK_SIZE[i];
      }
      network[i].use_special_connect_param             = SPECIAL_CONNECT_PARAM[i];
      network[i].phy.type                      = SL_WISUN_PHY_CONFIG_FAN11;
      network[i].preferred_pan_id              = PREFERRED_PAN_ID[i];
      network[i].regulation                    = REGULATION;
      network[i].regulation_warning_threshold  = REGULATION_WARNING_THRESHOLD; // see sl_wisun_set_regulation_tx_thresholds
      network[i].regulation_alert_threshold    = REGULATION_ALERT_THRESHOLD;// see sl_wisun_set_regulation_tx_thresholds
#ifdef     SL_WISUN_KEYCHAIN_H
      network[i].keychain_index                = 0;
      network[i].keychain                      = SL_WISUN_KEYCHAIN_AUTOMATIC;
#endif /*  SL_WISUN_KEYCHAIN_H */
#if       SL_RAIL_IEEE802154_SUPPORTS_G_MODE_SWITCH // see sl_wisun_set_pom_ie
      network[i].rx_phy_mode_ids_count          = 0;
      network[i].rx_phy_mode_ids[SL_WISUN_MAX_PHY_MODE_ID_COUNT];
      network[i].rx_mdr_capable                = 0;
#endif /* SL_RAIL_IEEE802154_SUPPORTS_G_MODE_SWITCH */
      /* device */
      if (i == DEFAULT_NETWORK_INDEX) {
        #ifdef    WISUN_CONFIG_DEVICE_TYPE
          network[i].device_type                   = WISUN_CONFIG_DEVICE_TYPE;
        #else  /* WISUN_CONFIG_DEVICE_TYPE */
          network[i].device_type                   = DEVICE_TYPE[i];
        #endif /* WISUN_CONFIG_DEVICE_TYPE */
      } else {
        network[i].device_type                   = DEVICE_TYPE[i];
      }
#ifdef    SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT
      if (i == DEFAULT_NETWORK_INDEX) {
        #ifdef    WISUN_CONFIG_DEVICE_PROFILE
          network[i].lfn_profile                   = WISUN_CONFIG_DEVICE_PROFILE;
        #else  /* WISUN_CONFIG_DEVICE_PROFILE*/
          #pragma message("Set the Device Type to LFN in the Wi-SUN Configurator to be able to select the Device Profile using the GUI")
          network[i].lfn_profile                   = LFN_PROFILE[i];
        #endif /* WISUN_CONFIG_DEVICE_PROFILE*/
      } else {
        network[i].lfn_profile                   = LFN_PROFILE[i];
      }
#endif /* SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT */
      network[i].fan_version                   = FAN_VERSION[i];
      network[i].tx_power_ddbm                 = TX_POWER_DDBM[i]; // 200 = 'MAX' (it's higher than the possible max)
      network[i].set_leaf                      = SET_LEAF; // see sl_wisun_set_leaf
      network[i].max_hop_count                 = 100; // see sl_wisun_set_max_hop_count
      network[i].uc_dwell_interval_ms          = 255; // 255 ms by default, see sl_wisun_set_unicast_settings
      network[i].max_child_count               = MAX_CHILD_COUNT[i];  // see sl_wisun_config_neighbor_table
      network[i].max_neighbor_count            = MAX_NEIGHBOR_COUNT[i];  // see sl_wisun_config_neighbor_table
      network[i].max_security_neighbor_count   = MAX_SECURITY_NEIGHBOR_COUNT[i]; // see sl_wisun_config_neighbor_table
      /* Application */
      network[i].auto_send_sec                 = AUTO_SEND_SEC[i];
      network[i].lowpan_mtu                    = 1576;
      network[i].ipv6_mru                      = 1504;
      network[i].max_edfe_fragment_count       = 5;
      network[i].rx_fifo_size                  = 4096; // See APP_SETTINGS_WISUN_DEFAULT_RX_FIFO_SIZE in CLI app_settings.c
      snprintf(network[i].udp_notification_dest , IPV6_STR_LEN, "%s", UDP_NOTIFICATION_DESTINATION[i] );
      snprintf(network[i].coap_notification_dest, IPV6_STR_LEN, "%s", COAP_NOTIFICATION_DESTINATION[i]);
      network[i].mac.min_be            = 3;
      network[i].mac.max_be            = 5;
      network[i].mac.backoff_period_us = 0 ;
      network[i].mac.max_cca_retries   = 8;
      network[i].mac.max_frame_retries = 7;
    }
    printf("\n");
  }
}

sl_status_t init_app_parameters() {
  sl_status_t status;
  // init mutex
  _app_parameters_mutex = osMutexNew(&_app_parameters_mutex_attr);
  assert(_app_parameters_mutex != NULL);

  _Static_assert(
      sizeof(app_settings_wisun_t) <= NVM3_DEFAULT_MAX_OBJECT_SIZE,
      "app_settings_wisun_t exceeds NVM3_DEFAULT_MAX_OBJECT_SIZE"
  );

  status = nvm3_initDefault();
  if (status != SL_STATUS_OK) {
    printfBoth("ERROR initializing NVM3\n");
  } else {
    app_parameter_mutex_acquire();
    status = read_app_parameters();
    if (status != SL_STATUS_OK) {
        printfBoth("set application parameters to default values\n");
        set_app_parameters_defaults(0x0000);
        app_parameters.nb_boots   = 1;
        app_parameters.nb_crashes = 0;
        status = save_app_parameters();
        if (status != SL_STATUS_OK) {
            printfBoth("Issue saving app_parameters: 0x%02x\n", (uint16_t)status);
        }
    }
    print_app_parameters();
    app_parameter_mutex_release();
  }
  return status;
}

sl_status_t set_app_parameter(char* parameter_name, int index, uint32_t value, char* value_str) {
  bool match = false;

  printfBothTime("set_app_parameter(%s, index %d, value %ld, %s)\n", parameter_name, index, value, value_str);

  if  (!match) { match = (sl_strcasecmp(parameter_name, "network_index") == 0);
    if (match) {
        app_parameters.network_index = (uint16_t)value;
        save_app_parameters();
        printfBothTime("Prepared to reboot on network %ld\n", value);
    }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "defaults") == 0);
    if (match) {
        // Set all defaults
        set_app_parameters_defaults(value);
        // Save default settings if passed a value different from 0
        if (value & ((1 << MAX_NETWORK_CONFIGS) -1)) {
            save_app_parameters();
            sprintf(value_str, "set defaults and autosaved for networks matching 0x%02lx bitfield", value);
        } else {
            sprintf(value_str, "set all defaults (no autosave), use 'save' before rebooting");
        }
        printfBothTime("%s\n", value_str);
        return SL_STATUS_OK;
    }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "save") == 0);
    if (match) {
      value = (uint16_t)save_app_parameters();
      if (value == SL_STATUS_OK) {
          sprintf(value_str, "saved to nvm3 with success %d networks", MAX_NETWORK_CONFIGS);
      } else {
          sprintf(value_str, "nvm3 save  error: %ld", value);
      }
      printfBothTime("%s\n", value_str);
      return SL_STATUS_OK;
    }
  }
  #ifdef    APP_ACTION_SCHEDULER_H
  // reboot options
  if  (!match) { match = (sl_strcasecmp(parameter_name, "reboot") == 0);
    if (match) {
      if (app_scheduler_action_schedule(APP_SCHEDULER_REBOOT, value, CLEAR_NVM_NO)) {
        uint32_t remaining;
        app_scheduler_action_get_remaining(&remaining, NULL);
        sprintf(value_str,
                "reboot scheduled in %lu ms (remaining=%lu ms)",
                (unsigned long)value,
                (unsigned long)remaining);
      } else {
        sprintf(value_str, "Failed to schedule reboot");
      }
    }
  }
  #endif /* APP_ACTION_SCHEDULER_H */
  #ifdef    APP_ACTION_SCHEDULER_H
  if  (!match) { match = (sl_strcasecmp(parameter_name, "clear_credential_cache_and_reboot") == 0);
    if (match) { // This is useful to test a full network restart, with credentials cleared on both ends
      if (app_scheduler_action_schedule(APP_SCHEDULER_CLEAR_CRED_AND_REBOOT,
                                      value,
                                      CLEAR_NVM_NO)) {
        uint32_t remaining;
        app_scheduler_action_get_remaining(&remaining, NULL);
        sprintf(value_str,
                "clear_credential_cache_and_reboot scheduled in %lu ms (remaining=%lu ms)",
                (unsigned long)value,
                (unsigned long)remaining);
      } else {
        sprintf(value_str, "Failed to schedule clear_credential_cache_and_reboot");
      }
    }
  }
  #endif /* APP_ACTION_SCHEDULER_H */
  if  (!match) {
    // Network settings (require the network_index, provide as 'index')
    if (index < MAX_NETWORK_CONFIGS) {
        if  (!match) { match = (sl_strcasecmp(parameter_name, "network_name") == 0);
          if (match) {
              snprintf(network[index].network_name, SL_WISUN_NETWORK_NAME_SIZE, "%s", value_str);
              sprintf(value_str, "\"network[%d].%s\": \"%s\"", index, parameter_name, network[index].network_name);
              printfBothTime("%s\n", value_str);
              return SL_STATUS_OK;
          }
        }
        if  (!match) { match = (sl_strcasecmp(parameter_name, "auto_send_sec") == 0);
          if (match) { network[index].auto_send_sec = (uint16_t)value; }
        }
        if  (!match) { match = (sl_strcasecmp(parameter_name, "udp_notif_dest") == 0);
          if (match) {
              if (unquote_ipv6(value_str, network[index].udp_notification_dest, 41) == 0) {
                  sprintf(value_str, "\"network[%d].%s\": \"%s\"", index, parameter_name, network[index].udp_notification_dest);
                  printfBothTime("%s\n", value_str);
                  return SL_STATUS_OK;
              } else {
                  sprintf(value_str, "ERROR, Use quote around  IPV6: \"udp_notif_dest 0 'ff02::1'\"\n");
                  printfBothTime("ERROR setting '%s': invalid IPv6 string '%s'! Use quotes\n", parameter_name, value_str);
                  return SL_STATUS_INVALID_PARAMETER;
              }
          }
        }
        if  (!match) { match = (sl_strcasecmp(parameter_name, "coap_notif_dest") == 0);
          if (match) {
              if (unquote_ipv6(value_str, network[index].coap_notification_dest, 41) == 0) {
                  sprintf(value_str, "\"network[%d].%s\": \"%s\"", index, parameter_name, network[index].coap_notification_dest);
                  printfBothTime("%s\n", value_str);
                  return SL_STATUS_OK;
              } else {
                  sprintf(value_str, "ERROR, Use quote around  IPV6: \"coap_notif_dest 0 'ff02::1'\"\n");
                  printfBothTime("ERROR setting '%s': invalid IPv6 string '%s'! Use quotes\n", parameter_name, value_str);
                  return SL_STATUS_INVALID_PARAMETER;
              }
          }
        }
        if  (!match) { match = (sl_strcasecmp(parameter_name, "use_special_connect_param") == 0);
          if (match) { network[index].use_special_connect_param = (uint8_t)value; }
        }
        if  (!match) { match = (sl_strcasecmp(parameter_name, "network_size") == 0);
          if (match) { network[index].network_size = (uint8_t)value; }
        }
        if  (!match) { match = (sl_strcasecmp(parameter_name, "tx_power_ddbm") == 0);
          if (match) { network[index].tx_power_ddbm = (uint16_t)value; }
        }
        if  (!match) { match = (sl_strcasecmp(parameter_name, "device_type") == 0);
          if (match) { network[index].device_type = (sl_wisun_device_type_t)value; }
        }
  #ifdef    SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT
        if  (!match) { match = (sl_strcasecmp(parameter_name, "lfn_profile") == 0);
          if (match) { network[index].lfn_profile = (sl_wisun_lfn_profile_t)value; }
        }
  #endif /* SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT */
        if  (!match) { match = (sl_strcasecmp(parameter_name, "fan_version") == 0);
          if (match) { network[index].fan_version = (sl_wisun_fan_version_t)value; }
        }
        if  (!match) { match = (sl_strcasecmp(parameter_name, "preferred_pan_id") == 0);
          if (match) { network[index].preferred_pan_id = (uint16_t)value; }
        }
        if  (!match) { match = (sl_strcasecmp(parameter_name, "max_hop_count") == 0);
          if (match) { network[index].max_hop_count = (uint8_t)value; }
        }
        if  (!match) { match = (sl_strcasecmp(parameter_name, "set_leaf") == 0);
          if (match) { network[index].set_leaf = (uint8_t)value; }
        }
        if  (!match) { match = (sl_strcasecmp(parameter_name, "type") == 0);
          if (match) { network[index].phy.type = (uint8_t)value; }
        }
        if  (!match) { match = (sl_strcasecmp(parameter_name, "reg_domain") == 0);
          if (match) { network[index].phy.config.fan11.reg_domain = (uint8_t)value; }
        }
        if  (!match) { match = (sl_strcasecmp(parameter_name, "phy_mode_id") == 0);
          if (match) { network[index].phy.config.fan11.phy_mode_id = (uint8_t)value; }
        }
        if  (!match) { match = (sl_strcasecmp(parameter_name, "chan_plan_id") == 0);
          if (match) { network[index].phy.config.fan11.chan_plan_id = (uint8_t)value; }
        }
        if  (!match) { match = (sl_strcasecmp(parameter_name, "max_child_count") == 0);
          if (match) { network[index].max_child_count = (uint8_t)value; }
        }
        if  (!match) { match = (sl_strcasecmp(parameter_name, "max_neighbor_count") == 0);
          if (match) { network[index].max_neighbor_count = (uint8_t)value; }
        }
        if  (!match) { match = (sl_strcasecmp(parameter_name, "max_security_neighbor_count") == 0);
          if (match) { network[index].max_security_neighbor_count = (uint16_t)value; }
        }
        // Conclusion
        if (match) {
            sprintf(value_str, "\"network[%d].%s\": \"%ld\"", index, parameter_name, value);
            printfBothTime("%s\n", value_str);
            return SL_STATUS_OK;
        } else {
            printfBothTime("Unknown network parameter '%s' for network %d\n", parameter_name, index);
        }
    } else {
        printfBothTime("ERROR setting '%s': incorrect index %d (above %d)!\n", parameter_name, index, MAX_NETWORK_CONFIGS);
        return SL_STATUS_NOT_SUPPORTED;
    }
  }
  // completion
  if  (!match) {
      sprintf(value_str, "ERROR setting '%s': unknown application parameter!\n", parameter_name);
      printfBothTime("%s\n", value_str);
      return SL_STATUS_NOT_SUPPORTED;
  } else {
      sprintf(value_str, "\"%s\": \"%ld\"", parameter_name, value);
      printfBothTime("%s\n", value_str);
      return SL_STATUS_OK;
  }
}

sl_status_t get_app_parameter(char* parameter_name, int index, uint32_t* value, char* value_str) {
  bool match = false;
  *value = 0xffffffff;
  index = index;
  value_str = value_str;
  printfBothTime("get_app_parameter(%s, index %d, *value, *value_str)\n", parameter_name, index);
  if  (!match) { match = (sl_strcasecmp(parameter_name, "nb_boots") == 0);
    if (match) { *value = (uint32_t)app_parameters.nb_boots; }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "nb_crashes") == 0);
    if (match) { *value = (uint32_t)app_parameters.nb_crashes; }
  }

  if  (!match) { match = (sl_strcasecmp(parameter_name, "network_count") == 0);
    if (match) { *value = (uint32_t)app_parameters.network_count; }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "network_index") == 0);
    if (match) { *value = (uint32_t)app_parameters.network_index; }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "app_parameters") == 0);
    if (match) {
        sprintf(value_str, "%s", app_parameters_string());
        *value = (uint32_t)app_parameters.network_index;
        printfBothTime("%s\n", value_str);
        return SL_STATUS_OK;
    }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "network") == 0);
    if (match) {
        sprintf(value_str, "%s", network_string(index));
        *value = (uint16_t)index;
        printfBothTime("%s\n", value_str);
        return SL_STATUS_OK;
    }
  }
  if  (!match) {
    if (index < MAX_NETWORK_CONFIGS) {
      if  (!match) { match = (sl_strcasecmp(parameter_name, "network_name") == 0);
        if (match) {
            *value = (uint16_t)index;
            sprintf(value_str, "\"network[%d].%s\": \"%s\"", index, parameter_name, network[index].network_name);
            printfBothTime("%s\n", value_str);
            return SL_STATUS_OK;
        }
      }
      if  (!match) { match = (sl_strcasecmp(parameter_name, "auto_send_sec") == 0);
        if (match) { *value = (uint32_t)network[index].auto_send_sec; }
      }
      if  (!match) { match = (sl_strcasecmp(parameter_name, "udp_notif_dest") == 0);
        if (match) {
            *value = (uint16_t)index;
            sprintf(value_str, "\"network[%d].%s\": \"%s\"", index, parameter_name, network[index].udp_notification_dest);
            printfBothTime("%s\n", value_str);
            return SL_STATUS_OK;
        }
      }
      if  (!match) { match = (sl_strcasecmp(parameter_name, "coap_notif_dest") == 0);
        if (match) {
            *value = (uint16_t)index;
            sprintf(value_str, "\"network[%d].%s\": \"%s\"", index, parameter_name, network[index].coap_notification_dest);
            printfBothTime("%s\n", value_str);
            return SL_STATUS_OK;
        }
      }
      if  (!match) { match = (sl_strcasecmp(parameter_name, "use_special_connect_param") == 0);
        if (match) { *value = (uint32_t)network[index].use_special_connect_param; }
      }
      if  (!match) { match = (sl_strcasecmp(parameter_name, "network_size") == 0);
        if (match) { *value = (uint32_t)network[index].network_size; }
      }
      if  (!match) { match = (sl_strcasecmp(parameter_name, "tx_power_ddbm") == 0);
        if (match) { *value = (uint32_t)network[index].tx_power_ddbm; }
      }
      if  (!match) { match = (sl_strcasecmp(parameter_name, "device_type") == 0);
        if (match) { *value = (uint32_t)network[index].device_type; }
      }
  #ifdef    SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT
      if  (!match) { match = (sl_strcasecmp(parameter_name, "lfn_profile") == 0);
        if (match) { *value = (uint32_t)network[index].lfn_profile; }
      }
  #endif /* SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT */
      if  (!match) { match = (sl_strcasecmp(parameter_name, "fan_version") == 0);
        if (match) { *value = (uint32_t)network[index].fan_version; }
      }
      if  (!match) { match = (sl_strcasecmp(parameter_name, "preferred_pan_id") == 0);
        if (match) { *value = (uint32_t)network[index].preferred_pan_id; }
      }
      if  (!match) { match = (sl_strcasecmp(parameter_name, "max_hop_count") == 0);
        if (match) { *value = (uint32_t)network[index].max_hop_count; }
      }
      if  (!match) { match = (sl_strcasecmp(parameter_name, "set_leaf") == 0);
        if (match) { *value = (uint32_t)network[index].set_leaf; }
      }
      if  (!match) { match = (sl_strcasecmp(parameter_name, "type") == 0);
        if (match) { *value = (uint32_t)network[index].phy.type; }
      }
      if  (!match) { match = (sl_strcasecmp(parameter_name, "reg_domain") == 0);
        if (match) { *value = (uint32_t)network[index].phy.config.fan11.reg_domain; }
      }
      if  (!match) { match = (sl_strcasecmp(parameter_name, "phy_mode_id") == 0);
        if (match) { *value = (uint32_t)network[index].phy.config.fan11.phy_mode_id; }
      }
      if  (!match) { match = (sl_strcasecmp(parameter_name, "chan_plan_id") == 0);
        if (match) { *value = (uint32_t)network[index].phy.config.fan11.chan_plan_id; }
      }
      if  (!match) { match = (sl_strcasecmp(parameter_name, "max_child_count") == 0);
        if (match) { *value = (uint32_t)network[index].max_child_count; }
      }
      if  (!match) { match = (sl_strcasecmp(parameter_name, "max_neighbor_count") == 0);
        if (match) { *value = (uint32_t)network[index].max_neighbor_count; }
      }
      if  (!match) { match = (sl_strcasecmp(parameter_name, "max_security_neighbor_count") == 0);
        if (match) { *value = (uint32_t)network[index].max_security_neighbor_count; }
      }
      if  (match) {
          sprintf(value_str, "\"network[%d].%s\": \"%ld\"", index, parameter_name, *value);
          printfBothTime("%s\n", value_str);
          return SL_STATUS_OK;
      }
    }
  }
  #ifdef    APP_ACTION_SCHEDULER_H
  if  (!match) { match = (sl_strcasecmp(parameter_name, "reboot") == 0);
    if (match) {app_scheduler_action_get_remaining(value, NULL); }
  }
  #endif /* APP_ACTION_SCHEDULER_H */
  #ifdef    APP_ACTION_SCHEDULER_H
  if  (!match) { match = (sl_strcasecmp(parameter_name, "clear_credential_cache_and_reboot") == 0);
    if (match) {app_scheduler_action_get_remaining(value, NULL); }
  }
  #endif /* APP_ACTION_SCHEDULER_H */
  if  (!match) {
      sprintf(value_str, "ERROR getting '%s': unknown application parameter!", parameter_name);
      printfBothTime("%s\n", value_str);
      return SL_STATUS_NOT_SUPPORTED;
  } else {
      sprintf(value_str, "\"%s\": \"%ld\"", parameter_name, *value);
      printfBothTime("%s\n", value_str);
      return SL_STATUS_OK;
  }
}

sl_status_t read_app_parameters()   {
  sl_status_t status;
  int i;
  status = nvm3_readData(nvm3_defaultHandle, NVM3_APP_KEY, &app_parameters, sizeof(app_parameters));
  if (status == SL_STATUS_OK) {
      printfBoth("read_app_parameters(): There are %d networks in NVM (key 0x%04X, %d bytes)\n",
                    app_parameters.network_count, NVM3_APP_KEY, sizeof(app_parameters));
      if (app_parameters.network_count != MAX_NETWORK_CONFIGS) {
          printfBoth("WARNING: read_app_parameters(): app_parameters.network_count (%d) != MAX_NETWORK_CONFIGS (%d)\n",
                      app_parameters.network_count, MAX_NETWORK_CONFIGS);
          status = SL_STATUS_INVALID_PARAMETER;
          return status;
      }

      if (app_parameters.app_params_version != NVM3_APP_PARAMS_VERSION) {
          printfBoth("WARNING: read_app_parameters(): app_parameters.app_params_version (%ld) != NVM3_APP_PARAMS_VERSION (%ld)\n",
                      app_parameters.app_params_version, (uint32_t)NVM3_APP_PARAMS_VERSION);
          status = SL_STATUS_INVALID_PARAMETER;
          return status;
      }

      if (app_parameters.network_struct_size != (uint16_t)sizeof(app_settings_wisun_t)) {
          printfBoth("WARNING: read_app_parameters(): app_parameters.network_struct_size (%d) != sizeof(app_settings_wisun_t) (%d)\n",
                      app_parameters.network_struct_size, (uint16_t)sizeof(app_settings_wisun_t));
          status = SL_STATUS_INVALID_PARAMETER;
          return status;
      }

      for (i=0; i<MAX_NETWORK_CONFIGS; i++) {
          status = nvm3_readData(nvm3_defaultHandle, NVM3_APP_KEY+1+i, &network[i], sizeof(app_settings_wisun_t));
          if (status != SL_STATUS_OK) {
              printfBoth("nvm3_readData(nvm3_defaultHandle, 0x%04x, app_parameters, %d) returned 0x%04lX\n",
                          NVM3_APP_KEY+1+i, sizeof(app_settings_wisun_t), status);
          } else {
              printfBoth("read_app_parameters(): network %d settings read from nvm3 (key 0x%04x, %d bytes)\n",
                          i, NVM3_APP_KEY+1+i, sizeof(app_settings_wisun_t));
          }
      }
  }
  if (status != SL_STATUS_OK) {
      if (status == SL_STATUS_NOT_FOUND) {
          printfBoth("nvm3_readData(nvm3_defaultHandle, 0x%04x, app_parameters, %d) returned 0x%04lX/NOT_FOUND, (The 0x%04x key is not set yet)\n",
                      NVM3_APP_KEY, sizeof(app_parameters), status, NVM3_APP_KEY);
      } else {
          if (status == SL_STATUS_NVM3_READ_DATA_SIZE) {
              printfBoth("nvm3_readData(nvm3_defaultHandle, 0x%04x, app_parameters, %d) returned 0x%04lX/SL_STATUS_NVM3_READ_DATA_SIZE, (Trying to read with a length different from actual object size)\n",
                          NVM3_APP_KEY, sizeof(app_parameters), status);
          } else {
                     // What to do here? Assert?
              printfBoth("nvm3_readData(nvm3_defaultHandle, 0x%04x, app_parameters, %d) returned 0x%04lX, (check sl_status.h)\n",
                      NVM3_APP_KEY, sizeof(app_parameters), status);
          }
      }
  }
  return status;
}

sl_status_t save_app_parameters()   {
  sl_status_t status;
  int i;
  status = nvm3_writeData(nvm3_defaultHandle, NVM3_APP_KEY, &app_parameters, sizeof(app_parameters));
  if (status != SL_STATUS_OK) {
      // What to do here? Assert?
      printfBoth("nvm3_writeData(nvm3_defaultHandle, 0x%04x, app_parameters, %d bytes) returned 0x%04lX, (check sl_status.h)\n",
                     NVM3_APP_KEY, sizeof(app_parameters), status);
  } else {
      for (i=0; i<MAX_NETWORK_CONFIGS; i++) {
          status = nvm3_writeData(nvm3_defaultHandle, NVM3_APP_KEY+1+i, &network[i], sizeof(app_settings_wisun_t));
          if (status != SL_STATUS_OK) {
              printfBoth("nvm3_writeData(nvm3_defaultHandle, 0x%04x, app_parameters, %d) returned 0x%04lX\n",
                           NVM3_APP_KEY+1+i, sizeof(app_settings_wisun_t), status);
          } else {
              printfBoth("network %d parameters saved to nvm3 (key 0x%04x, %d bytes)\n",
                           i, NVM3_APP_KEY+1+i, sizeof(app_settings_wisun_t));
          }
      }
      printfBoth("application parameters saved (%d networks)\n", i);
  }
  return status;
}

sl_status_t delete_app_parameters() {
  sl_status_t status;
  int i;
  status = nvm3_deleteObject(nvm3_defaultHandle, NVM3_APP_KEY);
  if (status != SL_STATUS_OK) {
      // What to do here? Assert?
      printfBoth("nvm3_deleteObject(nvm3_defaultHandle, 0x%04x) returned 0x%04lX, (check sl_status.h)\n",
                     NVM3_APP_KEY, status);
  } else {
      for (i=0; i<MAX_NETWORK_CONFIGS; i++) {
          status = nvm3_deleteObject(nvm3_defaultHandle, NVM3_APP_KEY+1+i);
          if (status != SL_STATUS_OK) {
              printfBoth("nvm3_deleteObject(nvm3_defaultHandle, 0x%04x) returned 0x%04lX\n",
                           NVM3_APP_KEY+1+i, status);
          } else {
              printfBoth("nvm3_deleteObject(nvm3_defaultHandle, 0x%04x) returned 0x%04lX\n",
                           NVM3_APP_KEY+1+i, status);
          }
      }
      printfBoth("application parameters deleted (%d networks)\n", i);
  }
  return status;
}

