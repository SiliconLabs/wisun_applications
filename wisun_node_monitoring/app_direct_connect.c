/***************************************************************************//**
* @file app_direct_connect.c
* @brief Wi-SUN Direct Connect add-on
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
*******************************************************************************
*
* EXPERIMENTAL QUALITY
* This code has not been formally tested and is provided as-is.  It is not suitable for production environments.
* This code will not be maintained.
*
******************************************************************************/

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include "printf.h"
#include "psa/crypto.h"
#include "em_system.h"

#include "sl_string.h"
#include "sl_wisun_api.h"

#ifdef    SEMAILBOX_PRESENT
 #include "sl_se_manager.h"
 #include "sl_se_manager_util.h"
#endif /* SEMAILBOX_PRESENT */

#include "app_timestamp.h"
#include "app_rtt_traces.h"
#include "app_reporter.h"
#include "app_direct_connect.h"


// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
uint32_t direct_connect_reporter_period_ms = 100;
// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
uint8_t direct_connect_pmk[SL_WISUN_PMK_LEN] = {
    0x34, 0xba, 0x32, 0x26, 0xa0, 0xb2, 0xad, 0x66, 0x7c, 0x9f, 0x66, 0x02, 0xe5, 0xdb, 0x75, 0x77,
    0xdd, 0xbd, 0x5d, 0x2b, 0x34, 0x3a, 0x93, 0x06, 0x2b, 0x90, 0xc0, 0x7b, 0xe2, 0x8e, 0x4e, 0x54 };

extern char crash_info_string[];

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
static uint32_t app_direct_connect_pmk_key_id = MBEDTLS_SVC_KEY_ID_INIT;
static char direct_connect_cli_response[1024];

// -----------------------------------------------------------------------------
//                          Public Functions Declarations
// -----------------------------------------------------------------------------
sl_status_t app_direct_connect(bool is_enabled)
{
  psa_key_attributes_t pmk_key_attributes = psa_key_attributes_init();
  psa_key_location_t pmk_location = PSA_KEY_LOCATION_LOCAL_STORAGE;
  sl_status_t status;
  psa_status_t ret;
  if (is_enabled) {
    #ifdef    SEMAILBOX_PRESENT
    if (SYSTEM_GetSecurityCapability() == securityCapabilityVault)
    {
      // If the device has Secure Vault, always use wrapped keys
      pmk_location = SL_PSA_KEY_LOCATION_WRAPPED;
    }
    #endif /* SEMAILBOX_PRESENT */

    psa_set_key_lifetime(&pmk_key_attributes,
                         PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_LIFETIME_VOLATILE, pmk_location));

    if (app_direct_connect_pmk_key_id != MBEDTLS_SVC_KEY_ID_INIT) {
      psa_destroy_key(app_direct_connect_pmk_key_id);
    }

    app_direct_connect_pmk_key_id = MBEDTLS_SVC_KEY_ID_INIT;

    psa_set_key_usage_flags(&pmk_key_attributes, PSA_KEY_USAGE_SIGN_HASH);
    psa_set_key_type       (&pmk_key_attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_algorithm  (&pmk_key_attributes, PSA_ALG_HMAC(PSA_ALG_SHA_1));
    ret = psa_import_key   (&pmk_key_attributes, direct_connect_pmk, SL_WISUN_PMK_LEN, &app_direct_connect_pmk_key_id);
    if (ret != PSA_SUCCESS) {
        printfBothTime("PMK import failed: psa_import_key: %"PRIu32"\r\n", ret);
      status = SL_STATUS_FAIL;
      goto error_handler;
    }

    status = sl_wisun_set_direct_connect_pmk(app_direct_connect_pmk_key_id);
    if (status != SL_STATUS_OK) {
        printfBothTime("PMK import failed: sl_wisun_set_direct_connect_pmk: %"PRIu32"\r\n", ret);
      goto error_handler;
    }
  }

  status = sl_wisun_set_direct_connect_state(is_enabled);
  if (status != SL_STATUS_OK) {
      printfBothTime("Failed: sl_wisun_set_direct_connect_state(%d): %"PRIu32"\r\n", is_enabled, status);
  } else {
      printfBothTime("Direct Connect %s\r\n", is_enabled ? "enabled" : "disabled");
  }

error_handler:
  psa_reset_key_attributes(&pmk_key_attributes);
  return status;
}

void app_direct_connect_custom_callback(sl_wisun_evt_t *evt) {
  sl_status_t status;
  char ipv6_string[40];

  switch (evt->header.id) {
    case SL_WISUN_MSG_DIRECT_CONNECT_LINK_AVAILABLE_IND_ID:
      ip6tos(&evt->evt.direct_connect_link_available.link_local_ipv6, ipv6_string);
      printfBothTime("Direct Connect Link Available with %s\r\n", ipv6_string);
      /* Demonstrating auto-connection to Direct Connect */
      status = sl_wisun_accept_direct_connect_link(&evt->evt.direct_connect_link_available.link_local_ipv6);
      if (status == SL_STATUS_OK) {
        printfBothTime("Direct Connect Link Accepted  with %s\r\n", ipv6_string);
      } else {
        printfBothTime("Failed: error %"PRIu32" accepting Direct Connect connection with %s\r\n", status, ipv6_string);
      }
      break;
    case SL_WISUN_MSG_DIRECT_CONNECT_LINK_STATUS_IND_ID:
      ip6tos(&evt->evt.direct_connect_link_status.link_local_ipv6, ipv6_string);
      switch (evt->evt.direct_connect_link_status.link_status) {
        case SL_WISUN_DIRECT_CONNECT_LINK_STATUS_CONNECTED:
          app_start_reporter(ipv6_string, direct_connect_reporter_period_ms, (char *)"*");
          printfBothTime("Direct Connect Link Connected with %s\r\n", ipv6_string);
          printfBothTime("Sending RTT traces every %ld ms to %s\r\n", direct_connect_reporter_period_ms, ipv6_string);
          break;
        case SL_WISUN_DIRECT_CONNECT_LINK_STATUS_ERROR:
          printfBothTime("Direct Connect Link %s: error\r\n", ipv6_string);
          break;
        case SL_WISUN_DIRECT_CONNECT_LINK_STATUS_DISCONNECTED:
          app_stop_reporter();
          printfBothTime("Direct Connect Link %s: disconnected\r\n", ipv6_string);
          printfBothTime("Stopped reporting RTT traces\r\n");
          break;
        default:
          break;
      }
      break;
    default:
      printfBothTime("Unknown event: %d in app_direct_connect_custom_callback()\r\n", evt->header.id);
  }
}

char *      app_direct_connect_cli(char *cli_msg) {
  int level = 0;
  int group = 0;

  snprintf(direct_connect_cli_response, 1024, "No '%s' cli command is implemented\n", cli_msg);
  if (sscanf(cli_msg, "wisun set_trace_level %d %d", &group, &level) == 2) {
    app_set_trace(group, level, true);
    snprintf(direct_connect_cli_response, 1024, "trace group %d set to trace level %d\n", group, level);
    return direct_connect_cli_response;
  }
  if (sscanf(cli_msg, "wisun set_trace_level %d", &level) == 1) {
    group = level; // Trick to store the required level without declaring a new variable
    if (level > 3) level = 3;
    app_set_all_traces(level, true);
    if (group != level) {
        snprintf(direct_connect_cli_response, 1024, "all trace groups set to trace level %d. If all traces are set at level 4 (DEBUG) or above, there will be a crash\n", level);
    } else {
    snprintf(direct_connect_cli_response, 1024, "all trace groups set to trace level %d\n", level);
    }
    return direct_connect_cli_response;
  }
  if (strcmp(cli_msg, "wisun rtt_traces on") == 0) {
    snprintf(direct_connect_cli_response, 1024, "starting RTT traces (weak)\n");
    return direct_connect_cli_response;
  }
  if (strcmp(cli_msg, "wisun rtt_traces off") == 0) {
      snprintf(direct_connect_cli_response, 1024, "stopping RTT traces (weak)\n");
      return direct_connect_cli_response;
  }
  if (strcmp(cli_msg, "wisun crash_report") == 0) {
      if (strlen(crash_info_string)) {
          return crash_info_string;
      } else {
          return "No previous crash info";
      }
    return crash_info_string;
  }
  return direct_connect_cli_response;
}
