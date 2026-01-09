/***************************************************************************//**
* @file app.c
* @brief Application code of the Wi-SUN Node Monitoring example
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

#include "os.h"
#include "printf.h"
#include "app.h"

#include "sl_assert.h"
#include "sl_string.h"
#include "sl_memory_manager.h"

#include "sl_wisun_api.h"
#include "sl_wisun_app_core.h"
#include "sl_wisun_types.h"
#include "sl_wisun_version.h"
#include "sl_wisun_keychain.h"
#include "sl_wisun_trace_util.h"
#include "sl_wisun_crash_handler.h"
#include "sl_wisun_event_mgr.h"
#include "sl_wisun_config.h"
#include "sl_wisun_coap.h"
#include "sl_wisun_coap_config.h"

#include "app_parameters.h"
 /* Increase SL_APPLICATION_VERSION in app_properties_config.h to use DELTA DFU */
 #include "app_properties_config.h"

#ifdef    SL_CATALOG_SIMPLE_BUTTON_PRESENT
#include "sl_simple_button_instances.h"
#endif /* SL_CATALOG_SIMPLE_BUTTON_PRESENT */

#ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
 #include "sl_simple_led_instances.h"
 void *led0;
 void *led1;

 #ifndef START_FLASHES_A
  #define START_FLASHES_A 6
 #endif /* START_FLASHES_A */
  #ifndef START_FLASHES_B
   #define START_FLASHES_B  SL_APPLICATION_VERSION
  #endif /* START_FLASHES_B */
#endif /* SL_CATALOG_SIMPLE_LED_PRESENT */

#ifndef APP_VERSION_STRING
  #define APP_VERSION_STRING "V6.2"
#endif /* APP_VERSION_STRING */

#include "app_coap.h"
#include "app_check_neighbors.h"

#ifdef    SL_CATALOG_GECKO_BOOTLOADER_INTERFACE_PRESENT
#include "btl_interface.h"
#endif  /* SL_CATALOG_GECKO_BOOTLOADER_INTERFACE_PRESENT */

#define   APP_TRACK_HEAP
//#define   APP_TRACK_HEAP_DIFF
#define   APP_CHECK_PREVIOUS_CRASH

#ifdef    APP_CHECK_PREVIOUS_CRASH
  #define PREVIOUS_CRASH_FORMAT_STRING ""
#else  /* APP_CHECK_PREVIOUS_CRASH */
  #define PREVIOUS_CRASH_FORMAT_STRING ""
#endif /* APP_CHECK_PREVIOUS_CRASH */

#ifdef    APP_TRACK_HEAP
  #define TRACK_HEAP_FORMAT_STRING "\"heap_used\":\"%.2f\",\n"
  #define TRACK_HEAP_VALUE         1.0*app_heap_info.used_size / (app_heap_info.total_size / 100),
#else  /* APP_TRACK_HEAP */
  #define TRACK_HEAP_FORMAT_STRING /* */
  #define TRACK_HEAP_VALUE         /* */
#endif /* APP_TRACK_HEAP */

#define UDP_RECEIVER_BUFFER_SIZE 2048
#define UDP_NOTIFICATION_PORT_RX 5003
#define INET6_ADDRSTRLEN 46 // Maximum length of an IPv6 address string

#ifdef    SL_CATALOG_WISUN_APP_CORE_PRESENT
  #include "sl_wisun_app_core_util.h"
#endif /* SL_CATALOG_WISUN_APP_CORE_PRESENT */

#ifdef    SL_CATALOG_WISUN_APP_OS_STAT_PRESENT
  #include "app_os_stat_config.h"
#endif /* SL_CATALOG_WISUN_APP_OS_STAT_PRESENT */

#ifdef    SL_CATALOG_WISUN_OTA_DFU_PRESENT
  #include "sl_wisun_ota_dfu_config.h"
#endif /* SL_CATALOG_WISUN_OTA_DFU_PRESENT */

#ifdef WITH_TCP_SERVER
  #include "app_tcp_server.h"
#endif /* WITH_TCP_SERVER */

#ifdef WITH_UDP_SERVER
  #include "app_udp_server.h"
#endif /* WITH_UDP_SERVER */

#define   LIST_RF_CONFIGS
#ifdef    LIST_RF_CONFIGS
  #include "app_list_configs.h"
#endif /* LIST_RF_CONFIGS */

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
// Macros to treat possible errors
#define NO_ERROR(ret, ...)                   if (ret == SL_STATUS_OK) {printfBothTime(__VA_ARGS__);}
#define IF_ERROR(ret, ...)                   if (ret != SL_STATUS_OK) {printfBothTime("\n"); printfBoth(__VA_ARGS__);}
#define IF_ERROR_RETURN(ret, ...)            if (ret != SL_STATUS_OK) {printfBothTime("\n"); printfBoth(__VA_ARGS__); return ret;}
#define IF_ERROR_INCR(ret, error_count, ...) if (ret != SL_STATUS_OK) {printfBothTime("\n"); printfBoth(__VA_ARGS__); error_count++;}

#define TRACES_WHILE_CONNECTING \
    app_set_all_traces(SL_WISUN_TRACE_LEVEL_INFO, true); \
    app_set_trace(SL_WISUN_TRACE_GROUP_SOCK   , SL_WISUN_TRACE_LEVEL_INFO , true); \
    app_set_trace(SL_WISUN_TRACE_GROUP_BOOT   , SL_WISUN_TRACE_LEVEL_DEBUG, true); \
    app_set_trace(SL_WISUN_TRACE_GROUP_RF     , SL_WISUN_TRACE_LEVEL_ERROR, true); \
    app_set_trace(SL_WISUN_TRACE_GROUP_RPL    , SL_WISUN_TRACE_LEVEL_DEBUG, true); \
    app_set_trace(SL_WISUN_TRACE_GROUP_TIM_SRV, SL_WISUN_TRACE_LEVEL_ERROR, true); \
    app_set_trace(SL_WISUN_TRACE_GROUP_FHSS   , SL_WISUN_TRACE_LEVEL_ERROR, true); \
    app_set_trace(SL_WISUN_TRACE_GROUP_MAC    , SL_WISUN_TRACE_LEVEL_WARN , true); \
    app_set_trace(SL_WISUN_TRACE_GROUP_FSM    , SL_WISUN_TRACE_LEVEL_WARN , true);

#define TRACES_WHEN_CONNECTED \
    app_set_all_traces(SL_WISUN_TRACE_LEVEL_INFO, true); \
    app_set_trace(SL_WISUN_TRACE_GROUP_RF     , SL_WISUN_TRACE_LEVEL_ERROR, true); \
    app_set_trace(SL_WISUN_TRACE_GROUP_RPL    , SL_WISUN_TRACE_LEVEL_ERROR, true); \
    app_set_trace(SL_WISUN_TRACE_GROUP_TIM_SRV, SL_WISUN_TRACE_LEVEL_ERROR, true); \
    app_set_trace(SL_WISUN_TRACE_GROUP_FHSS   , SL_WISUN_TRACE_LEVEL_ERROR, true); \
    app_set_trace(SL_WISUN_TRACE_GROUP_MAC    , SL_WISUN_TRACE_LEVEL_WARN , true); \
    app_set_trace(SL_WISUN_TRACE_GROUP_FSM    , SL_WISUN_TRACE_LEVEL_WARN , true);

#ifdef    HISTORY
#define APPEND_TO_HISTORY(...) { \
  history_len = strlen(history_string); \
  snprintf(history_string + history_len, \
    SL_WISUN_COAP_RESOURCE_HND_SOCK_BUFF_SIZE - history_len, \
    __VA_ARGS__); \
    printfBothTime(__VA_ARGS__); }
#endif /* HISTORY */

// JSON common format strings
#define START_JSON "{\n"

#define END_JSON   "}\n"

#define DEVICE_CHIP_ITEMS \
  device_global_ipv6_string,\
  device_tag,\
  CHIP,\
  device_type,\
  device_mac_string

#define DEVICE_CHIP_JSON_FORMAT \
  "\"ipv6\":\"%s\",\n"   \
  "\"device\":\"%s\",\n" \
  "\"chip\":\"%s\",\n"   \
  "\"type\":\"%s\",\n"   \
  "\"MAC\":\"%s\",\n"    \

// List below the items from parent_info you want to return
// Update PARENT_JSON to match (in the same order, with the proper display format)
#define PARENT_INFO_ITEMS \
  parent_tag,               \
  parent_info.rpl_rank,     \
  parent_info.etx,          \
  parent_info.routing_cost, \
  parent_info.rsl_in,       \
  parent_info.rsl_out      \
//  parent_info.lifetime
//  secondary_tag,
//  secondary_info.rsl_in,
//  secondary_info.rsl_out

#define PARENT_JSON_FORMAT \
  "\"parent\":\"%s\",\n"       \
  "\"rpl_rank\":\"%d\",\n"     \
  "\"etx\":\"%d\",\n"          \
  "\"routing_cost\":\"%d\",\n" \
  "\"rsl_in\":\"%d\",\n"       \
  "\"rsl_out\":\"%d\",\n"      \
//  "\"lifetime\":\"%ld\",\n"
//  "\"secondary\":\"%s\",\n"
//  "\"sec_rsl_in\":\"%d\",\n"
//  "\"sec_rsl_out\":\"%d\",\n"

#define RUNNING_JSON_FORMAT \
  "\"running\":\"%s\",\n"

#define MSG_COUNT_JSON_FORMAT \
  "\"msg_count\":\"%lld\",\n"

#ifdef  _SILICON_LABS_32B_SERIES_1             /** Product Series Identifier */
  #ifdef _SILICON_LABS_32B_SERIES_1_CONFIG_2   /** Product Config Identifier */
    #define CHIP "xG12"
  #endif
#endif

#ifdef  _SILICON_LABS_32B_SERIES_2             /** Product Series Identifier */
  #ifdef _SILICON_LABS_32B_SERIES_2_CONFIG_5   /** Product Config Identifier */
    #define CHIP "xG25"
  #endif
  #ifdef _SILICON_LABS_32B_SERIES_2_CONFIG_8                                                     /** Product Config Identifier */
    #define CHIP "xG28"
  #endif
#endif

#ifndef CHIP
  #define CHIP SL_BOARD_NAME
#endif

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------
void        _join_state_custom_callback(sl_wisun_evt_t *evt);
void        _check_neighbors(void);
char*       _connection_json_string();
char*       _status_json_string (char * start_text);
char        device_mac_string[40];
sl_wisun_network_info_t network_info;
sl_wisun_mac_address_t _get_parent_mac_address_and_update_parent_info(void);
sl_status_t _select_destinations(void);
sl_status_t _open_udp_sockets(void);
sl_status_t _coap_notify(char* json_string);

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
uint16_t connection_count = 0;       // number of connections (moving to Join State 5)
uint16_t network_connection_count = 0; // number of network connections (moving to Join State 5 from min Join State 3)
uint64_t connect_time_sec;           // time stamp of Wisun connect call
uint64_t connection_time_sec;        // last connection time stamp
uint64_t disconnection_time_sec;     // last disconnection time stamp
uint64_t connected_total_sec = 0;    // total time connected
uint64_t disconnected_total_sec = 0; // total time disconnected
uint64_t msg_count = 0;              // number of messages sent
sl_wisun_neighbor_info_t parent_info;   // local storage of the parent info
sl_wisun_neighbor_info_t secondary_info;// local storage of the secondary parent info
uint16_t preferred_pan_id = 0xffff;  // Preferred PAN Id (0xffff for 'none')
uint8_t  min_join_state   = 0;       // Used to log Join State changes and check how 'low' it goes
bool     send_asap;                  // Used to trigger sending the status as soon as possible

#ifdef    APP_TRACK_HEAP
sl_memory_heap_info_t app_heap_info;
 #ifdef    APP_TRACK_HEAP_DIFF
size_t app_previous_heap_free;
 #endif /* APP_TRACK_HEAP_DIFF */
bool   refresh_heap;
#endif /* APP_TRACK_HEAP */

bool print_keep_alive = true;

char chip[8];
char device_tag[8];
char parent_tag[8];
char secondary_tag[8];
char application[100];
char version[80];
char device_type[25];
uint32_t parent_rsl_in  = 0;
uint32_t parent_rsl_out = 0;

bool just_disconnected = false;
#ifdef    HISTORY
uint16_t history_len;
#endif /* HISTORY */

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
sl_wisun_join_state_t join_state = SL_WISUN_JOIN_STATE_DISCONNECTED;
static  uint64_t app_join_state_sec[6];
        uint64_t app_join_state_delay_sec[6];
static uint16_t previous_join_state = 0;
char json_string[SL_WISUN_COAP_RESOURCE_HND_SOCK_BUFF_SIZE];
#ifdef    HISTORY
char history_string[SL_WISUN_COAP_RESOURCE_HND_SOCK_BUFF_SIZE];
#endif /* HISTORY */
sl_wisun_mac_address_t device_mac;
sl_wisun_mac_address_t parent_mac;
sl_wisun_mac_address_t secondary_mac;

// IPv6 address structures
in6_addr_t device_global_ipv6;
in6_addr_t border_router_ipv6;

sockaddr_in6_t udp_notification_sockaddr_in6;
sockaddr_in6_t coap_notification_sockaddr_in6;

// IPv6 address strings (for printing)
char  device_global_ipv6_string[40];
char  border_router_ipv6_string[40];
char  udp_notification_ipv6_string[40];
char  coap_notification_ipv6_string[40];

// Notification sockets
sl_wisun_socket_id_t udp_notification_socket_id = 0;
sl_wisun_socket_id_t coap_notification_socket_id = 0;

char udp_msg[1024];
char coap_msg[1024];

uint16_t msg_len;
uint8_t  trace_level = SL_WISUN_TRACE_LEVEL_INFO;    // Trace level for all trace groups

// UDP ports
#define UDP_NOTIFICATION_PORT  1237
#define COAP_NOTIFICATION_PORT 5685

#define SL_WISUN_STATUS_CONNECTION_URI_PATH  "/status/connection"
#define SL_WISUN_STATUS_JSON_STR_MAX_LEN 512

#ifdef    SL_SIMPLE_BUTTON_INSTANCES_H
  bool B0;
  bool B1;
  bool check_buttons;
  #define BUTTON_CHECK_DELAY 2
#endif /* SL_SIMPLE_BUTTON_INSTANCES_H */

// CoAP Notification channel structure definition
typedef struct sl_wisun_coap_notify_ch {
  /// Notification socket
  int32_t sockid;
  /// Notification address
  sockaddr_in6_t addr;
  /// Notification packet
  sl_wisun_coap_packet_t pkt;
} sl_wisun_coap_notify_ch_t;

// CoAP Notification channel structure instance and defaults
static sl_wisun_coap_notify_ch_t coap_notify_ch = {
  .sockid = -1L,
  .addr = { 0U },
  .pkt = { 0U },
};

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------

#ifdef    SL_CATALOG_SIMPLE_BUTTON_PRESENT
char* _button_json_string (char * start_text) {
  #define BUTTON_JSON_FORMAT_STR  \
    "%s"                          \
    START_JSON                    \
    DEVICE_CHIP_JSON_FORMAT       \
    PARENT_JSON_FORMAT            \
    "\"BTN0\":\"%d\",\n"          \
    "\"BTN1\":\"%d\",\n"          \
    END_JSON

  snprintf(json_string, SL_WISUN_COAP_RESOURCE_HND_SOCK_BUFF_SIZE,
    BUTTON_JSON_FORMAT_STR,
    start_text,
    DEVICE_CHIP_ITEMS,
    PARENT_INFO_ITEMS,
    B0,
    B1
  );

  return json_string;
}
#endif /* SL_CATALOG_SIMPLE_BUTTON_PRESENT */

#ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
void set_leds(uint8_t led1, uint8_t led0) {
  if (led1 == 0) sl_led_turn_off(&sl_led_led1);
  if (led1 == 1) sl_led_turn_on (&sl_led_led1);
  if (led0 == 0) sl_led_turn_off(&sl_led_led0);
  if (led0 == 1) sl_led_turn_on (&sl_led_led0);
}

void leds_f_join_state(sl_wisun_join_state_t join_state) {
  // Led1/led0 represent (join_state & 0x11) in binary form, for easier interpretation
  if (join_state == SL_WISUN_JOIN_STATE_SELECT_PAN        ) set_leds(0, 1); /* JS 1: '01' */
  if (join_state == SL_WISUN_JOIN_STATE_AUTHENTICATE      ) set_leds(1, 0); /* JS 2: '10' */
  if (join_state == SL_WISUN_JOIN_STATE_ACQUIRE_PAN_CONFIG) set_leds(1, 1); /* JS 3: '11' */
  if (join_state == SL_WISUN_JOIN_STATE_CONFIGURE_ROUTING ) set_leds(0, 0); /* JS 4: '00' */
}

void leds_flash(uint16_t count, uint16_t delay_ms) {
  uint16_t i;
  uint32_t  ticks;
  ticks = (uint32_t)1.0*delay_ms;
  printfTime("leds_flash(%d, %d)\n", count, delay_ms);
  for (i=0; i<count; i++) {
      set_leds(0, 0);
      osDelay(ticks);
      set_leds(1, 1);
      osDelay(ticks);
  }
}
#endif /* SL_CATALOG_SIMPLE_LED_PRESENT */

static uint16_t _get_cert_str_len(const uint8_t *cert, const uint16_t max_cert_len)
{
  uint16_t n = 0;
  if (cert == NULL) {
    return 0U;
  }
  for (n = 0; n < max_cert_len && *cert; n++, cert++) {
    ;
  }
  return n;
}

uint8_t app_join_network(uint8_t network_index) {
  sl_status_t ret;
  sl_wisun_connection_params_t connection_params;
  sl_wisun_join_state_t join_state;
  //sl_wisun_channel_mask_t channel_mask;
  //sl_wisun_lfn_params_t lfn_params;
  uint8_t phy_mode_id_count, is_mdr_command_capable;
  uint8_t phy_mode_id[SL_WISUN_MAX_PHY_MODE_ID_COUNT];
  uint8_t *phy_mode_id_p, *phy_mode_id_count_p;
#if SL_RAIL_IEEE802154_SUPPORTS_G_MODE_SWITCH
  bool set_pom_ie = false;
#endif
  uint8_t trustedca_count;
  uint8_t idx;
  sl_wisun_keychain_entry_t *trustedca = NULL;
  sl_wisun_keychain_credential_t *credential = NULL;
  uint16_t certificate_options;
  const sl_wisun_regulation_params_t *regulation_params;

  app_settings_wisun_t this_network;

#if SL_RAIL_IEEE802154_SUPPORTS_G_MODE_SWITCH
  bool set_pom_ie = false;
#endif

  app_parameter_mutex_acquire();
  sl_wisun_get_join_state(&join_state);

  app_parameters.network_index = network_index % MAX_NETWORK_CONFIGS;
  this_network = network[app_parameters.network_index];
  if (join_state != SL_WISUN_JOIN_STATE_DISCONNECTED) {
    printfBothTime("Not disconnected: disconnect\r\n");
    sl_wisun_disconnect();
  }

  ret = sl_wisun_set_device_type((sl_wisun_device_type_t)this_network.device_type);
  if (ret != SL_STATUS_OK) {
    printfBothTime("[Failed: unable to set device type to %d: %lu]\r\n", this_network.device_type, ret);
    ret = __LINE__; goto cleanup;
  }

  if (this_network.device_type == SL_WISUN_ROUTER) {
    switch (this_network.network_size) {
      case SL_WISUN_NETWORK_SIZE_SMALL:
        connection_params = SL_WISUN_PARAMS_PROFILE_SMALL;
        break;
      case SL_WISUN_NETWORK_SIZE_MEDIUM:
        connection_params = SL_WISUN_PARAMS_PROFILE_MEDIUM;
        break;
      case SL_WISUN_NETWORK_SIZE_LARGE:
        connection_params = SL_WISUN_PARAMS_PROFILE_LARGE;
        break;
      case SL_WISUN_NETWORK_SIZE_TEST:
        connection_params = SL_WISUN_PARAMS_PROFILE_TEST;
        break;
      case SL_WISUN_NETWORK_SIZE_CERTIFICATION:
        connection_params = SL_WISUN_PARAMS_PROFILE_CERTIF;
        break;
      default:
        printfBothTime("[Failed: unsupported network size]\r\n");
        ret = __LINE__; goto cleanup;
    }
    connection_params.traffic.lowpan_mtu = this_network.lowpan_mtu;
    connection_params.traffic.ipv6_mru = this_network.ipv6_mru;
    connection_params.traffic.max_edfe_fragment_count = this_network.max_edfe_fragment_count;

    ret = sl_wisun_set_connection_parameters(&connection_params);
  }

  if (ret != SL_STATUS_OK) {
    printfBothTime("[Failed: unable to set network size: %lu]\r\n", ret);
    ret = __LINE__; goto cleanup;
  }

  ret = sl_wisun_config_neighbor_table(this_network.max_child_count, this_network.max_neighbor_count, this_network.max_security_neighbor_count);
  if (ret != SL_STATUS_OK) {
    printfBothTime("[Failed: unable to set neighbor table sizes (%d, %d, %d): %lu]\r\n", this_network.max_child_count, this_network.max_neighbor_count, this_network.max_security_neighbor_count, ret);
    ret = __LINE__; goto cleanup;
  }

  if (this_network.device_type == SL_WISUN_ROUTER) {
    ret = sl_wisun_set_preferred_pan(this_network.preferred_pan_id);
    if (ret != SL_STATUS_OK) {
      printfBothTime("[Failed: unable to set preferred PAN: %lu]\r\n", ret);
      ret = __LINE__; goto cleanup;
    }
  }
#ifdef    SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT
  if (this_network.device_type == SL_WISUN_LFN) {
    switch (this_network.lfn_profile) {
      case SL_WISUN_LFN_PROFILE_TEST:
        lfn_params = SL_WISUN_PARAMS_LFN_TEST;
        break;
      case SL_WISUN_LFN_PROFILE_BALANCED:
        lfn_params = SL_WISUN_PARAMS_LFN_BALANCED;
        break;
      case SL_WISUN_LFN_PROFILE_ECO:
        lfn_params = SL_WISUN_PARAMS_LFN_ECO;
        break;
      default:
        printfBothTime("[Failed: unsupported LFN profile]\r\n");
        ret = __LINE__; goto cleanup;
    }
    ret = sl_wisun_set_lfn_parameters(&lfn_params);
    if (ret != SL_STATUS_OK) {
      printfBothTime("[Failed: unable to set LFN parameters: %lu]\r\n", ret);
      ret = __LINE__; goto cleanup;
    }
  }
#endif /* SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT */

  ret = sl_wisun_set_tx_power_ddbm(this_network.tx_power_ddbm);
  if (ret != SL_STATUS_OK) {
    printfBothTime("[Failed: unable to set TX power: %lu]\r\n", ret);
    ret = __LINE__; goto cleanup;
  }

#if defined(WISUN_CONFIG_DDP)
  trustedca_count = sl_wisun_keychain_get_trustedca_count();
#else    /* WISUN_CONFIG_DDP */
  trustedca_count = 0;
#endif   /* WISUN_CONFIG_DDP */
  certificate_options = SL_WISUN_CERTIFICATE_OPTION_IS_REF;
  if (trustedca_count == 0) {
    printfBothTime("No trusted CA keychain, init from builtin credentials\r\n");
    const uint32_t max_cert_str_len = 2048U;

    // set the trusted certificate
    ret = sl_wisun_set_trusted_certificate(SL_WISUN_CERTIFICATE_OPTION_IS_REF,
                                           _get_cert_str_len(wisun_config_ca_certificate, max_cert_str_len) + 1,
                                           wisun_config_ca_certificate);
    if (ret != SL_STATUS_OK) {
      printf("[Failed: unable to set the trusted certificate: %lu]\n", ret);
      ret = __LINE__; goto cleanup;
    }

    // set the device certificate
    ret = sl_wisun_set_device_certificate(SL_WISUN_CERTIFICATE_OPTION_IS_REF | SL_WISUN_CERTIFICATE_OPTION_HAS_KEY,
                                          _get_cert_str_len(wisun_config_device_certificate, max_cert_str_len) + 1,
                                          wisun_config_device_certificate);
    if (ret != SL_STATUS_OK) {
      printf("[Failed: unable to set the device certificate: %lu]\n", ret);
      ret = __LINE__; goto cleanup;
    }

    // set the device private key
    // NOTE: to use a wrapped PSA private key, the app needs to import the key
    // and use the API sl_wisun_set_device_private_key_id() instead of the one below
    ret = sl_wisun_set_device_private_key(SL_WISUN_PRIVATE_KEY_OPTION_IS_REF,
                                          _get_cert_str_len(wisun_config_device_private_key, max_cert_str_len) + 1,
                                          wisun_config_device_private_key);
    if (ret != SL_STATUS_OK) {
      printf("[Failed: unable to set the device private key: %lu]\n", ret);
      ret = __LINE__; goto cleanup;
    }

  } else {
    printfBothTime("%d trusted CA keychains\r\n", trustedca_count);
    for (idx = 0; idx < trustedca_count; ++idx) {
      trustedca = sl_wisun_keychain_get_trustedca(idx);
      if (!trustedca) {
        printf("[Failed to load trusted CA]\r\n");
        ret = __LINE__; goto cleanup;
      }
      if (trustedca->keychain == SL_WISUN_KEYCHAIN_NVM) {
        printf("[Using NVM trusted CA #%u]\r\n", idx);
      } else if (trustedca->keychain == SL_WISUN_KEYCHAIN_BUILTIN) {
        printf("[Using built-in trusted CA #%u]\r\n", idx);
      }

      ret = sl_wisun_set_trusted_certificate(certificate_options,
                                             trustedca->data_length,
                                             trustedca->data);
      if (ret != SL_STATUS_OK) {
        printfBothTime("[Failed: unable to set the trusted certificate: %lu]\r\n", ret);
        ret = __LINE__; goto cleanup;
      }

      sl_free(trustedca);
      trustedca = NULL;
      certificate_options |= SL_WISUN_CERTIFICATE_OPTION_APPEND;
    }

    credential = sl_wisun_keychain_get_credential((sl_wisun_keychain_t)this_network.keychain, this_network.keychain_index);
    if (!credential) {
      printfBothTime("[Failed: unable to load credential for keychain %d]\r\n", this_network.keychain_index);
      //ret = __LINE__; goto cleanup;
    }
    if (credential->certificate.keychain == SL_WISUN_KEYCHAIN_NVM) {
      printf("[Using NVM device credentials]\r\n");
    } else if (credential->certificate.keychain == SL_WISUN_KEYCHAIN_BUILTIN) {
      printf("[Using built-in device credentials]\r\n");
    }

    ret = sl_wisun_set_device_certificate(SL_WISUN_CERTIFICATE_OPTION_IS_REF | SL_WISUN_CERTIFICATE_OPTION_HAS_KEY,
                                          credential->certificate.data_length,
                                          credential->certificate.data);
    if (ret != SL_STATUS_OK) {
      printfBothTime("[Failed: unable to set the device certificate: %lu]\r\n", ret);
      ret = __LINE__; goto cleanup;
    }

    if (credential->pk.type == SL_WISUN_KEYCHAIN_KEY_TYPE_PLAINTEXT) {
      ret = sl_wisun_set_device_private_key(SL_WISUN_PRIVATE_KEY_OPTION_IS_REF,
                                            credential->pk.u.plaintext.data_length,
                                            credential->pk.u.plaintext.data);
    } else {
      ret = sl_wisun_set_device_private_key_id(credential->pk.u.key_id);
    }
    if (ret != SL_STATUS_OK) {
      printfBothTime("[Failed: unable to set the device private key: %lu]\r\n", ret);
      ret = __LINE__; goto cleanup;
    }
  }

/*
  ret = app_settings_get_channel_mask(this_network.allowed_channels, &channel_mask);
  ret = sl_wisun_set_channel_mask(&channel_mask);
  if (ret != SL_STATUS_OK) {
    printfBothTime("[Failed: unable to set channel mask: %lu]\r\n", ret);
    res = __LINE__; goto cleanup;
  }
*/
  if (this_network.device_type == SL_WISUN_ROUTER) {
    ret = sl_wisun_set_unicast_settings(this_network.uc_dwell_interval_ms);
    if (ret != SL_STATUS_OK) {
      printfBothTime("[Failed: unable to set unicast settings: %lu]\r\n", ret);
      ret = __LINE__; goto cleanup;
    }
  }
  switch(this_network.regulation) {
    case SL_WISUN_REGULATION_NONE:
      regulation_params = &SL_WISUN_REGULATION_PARAMS_NONE;
      break;
    case SL_WISUN_REGULATION_ARIB:
      regulation_params = &SL_WISUN_REGULATION_PARAMS_ARIB;
      break;
    case SL_WISUN_REGULATION_WPC:
      regulation_params = &SL_WISUN_REGULATION_PARAMS_WPC;
      break;
    case SL_WISUN_REGULATION_ETSI:
      regulation_params = &SL_WISUN_REGULATION_PARAMS_ETSI;
      break;
    default:
      printfBothTime("[Failed: unsupported regulation]\r\n");
      ret = __LINE__; goto cleanup;
  }
  ret = sl_wisun_set_regulation_parameters(regulation_params);
  if (ret != SL_STATUS_OK) {
    printfBothTime("[Failed: unable to set regional regulation parameters: %lu]\r\n", ret);
    ret = __LINE__; goto cleanup;
  }

  ret = sl_wisun_set_regulation_tx_thresholds(this_network.regulation_warning_threshold,
                                              this_network.regulation_alert_threshold);
  if (ret != SL_STATUS_OK) {
    printfBothTime("[Failed: unable to set regulation TX thresholds: %lu]\r\n", ret);
  }

#if 0
  ret = sl_wisun_set_pti_state(this_network.pti_state);
  if (ret != SL_STATUS_OK) {
    printf("[Failed to set PTI state]\r\n");
    ret = __LINE__; goto cleanup;
  }
#endif

  #if SLI_WISUN_DISABLE_SECURITY
  ret = sl_wisun_set_security_state(app_security_state);
  if (ret != SL_STATUS_OK) {
    printf("[Failed to set security state %"PRIu32"]\r\n", app_security_state);
    ret = __LINE__; goto cleanup;
  }
#endif
/*
  // As per RFC3748, "The Identity Response field MUST NOT be null terminated"
  ret = sl_wisun_set_eap_identity(strlen(this_network.eap_identity),
                                  (const uint8_t *)this_network.eap_identity);
  if (ret != SL_STATUS_OK) {
    printf("[Failed to set EAP identity]\r\n");
    ret = __LINE__; goto cleanup;
  }
*/
  ret = sl_wisun_join((const uint8_t *)this_network.network_name, &this_network.phy);
  if (ret == SL_STATUS_OK) {
    printfBothTime("[Connecting to Network[%d]: \"%s\": %s]\r\n",
          app_parameters.network_index,
          this_network.network_name,
          app_wisun_phy_to_str(&(network[app_parameters.network_index].phy)));
  } else {
    printf("[Join failed: %lu]\r\n", ret);
    if (ret == SL_STATUS_FAIL) {
        sl_wisun_get_join_state(&join_state);
        if (join_state == SL_WISUN_JOIN_STATE_DISCONNECTED) {
            printfBothTime("Network[%d]: Incorrect PHY selection: Will never connect on %s\n",
                app_parameters.network_index,
                app_wisun_phy_to_str(&(network[app_parameters.network_index].phy)));
        }
        ret = __LINE__; goto cleanup;
    }
  }

#if SL_RAIL_IEEE802154_SUPPORTS_G_MODE_SWITCH
  // Configure POM-IE
  // If PhyModeIds are set by user, send them to the stack, otherwise
  // retrieve the default PhyModeIds from the stack first
  if (app_settings_wisun.rx_phy_mode_ids_count == 0) {
    // Check if default PhyList can be retrieved from device
    if (sl_wisun_get_pom_ie(&phy_mode_id_count, phy_mode_id, &is_mdr_command_capable) == SL_STATUS_OK) {
      phy_mode_id_p = phy_mode_id;
      phy_mode_id_count_p = &phy_mode_id_count;

      if (is_mdr_command_capable != app_settings_wisun.rx_mdr_capable) {
        set_pom_ie = true;
      }
    } else {
      // POM-IE not available
      ret = __LINE__; goto cleanup;
    }
  } else {
    // Set by user
    phy_mode_id_p = app_settings_wisun.rx_phy_mode_ids;
    phy_mode_id_count_p = &app_settings_wisun.rx_phy_mode_ids_count;
    set_pom_ie = true;
  }

  if (set_pom_ie) {
    ret = sl_wisun_set_pom_ie(*phy_mode_id_count_p,
                              phy_mode_id_p,
                              app_settings_wisun.rx_mdr_capable);
    if (ret != SL_STATUS_OK) {
      printfBothTime("[Failed: unable to set RX PhyModeId list in POM-IE for Mode Switch: %lu]\r\n", ret);
      ret = __LINE__; goto cleanup;
    }
  }
#else
  (void)phy_mode_id_count;
  (void)is_mdr_command_capable;
  (void)phy_mode_id;
  (void)phy_mode_id_p;
  (void)phy_mode_id_count_p;
#endif

cleanup:

  if (trustedca) {
    sl_free(trustedca);
  }
  if (credential) {
    sl_free(credential);
  }

  app_parameter_mutex_release();

  if (ret != SL_STATUS_OK) {
      if (ret != SL_STATUS_FAIL) {
          printfBothTime("app_join_network(%d) failed around line %ld\n", app_parameters.network_index, ret);
      } else {
          printfBothTime("sl_wisun_join() failed: the most probable cause is an unknown PHY. Check you Radio configuration\n");
      }
  }

  return ret;
}

/* App task function*/
void app_task(void *args)
{
  (void) args;
  uint16_t join_res = 0;
  uint64_t now = 0;
  uint64_t connection_timestamp;
  uint64_t connected_delay_sec;
  bool print_keep_alive = true;
#ifdef    SL_CATALOG_SIMPLE_BUTTON_PRESENT
  uint8_t startup_option = 0;
#endif /* SL_CATALOG_SIMPLE_BUTTON_PRESENT */
#ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
  uint8_t change_leds;
  uint8_t previous_change_leds = 5;
#endif /* SL_CATALOG_SIMPLE_LED_PRESENT */
  bool with_time, to_console, to_rtt, to_udp, to_coap;
#ifdef    SL_CATALOG_GECKO_BOOTLOADER_INTERFACE_PRESENT
  BootloaderStorageInformation_t storage_info;
#endif  /* SL_CATALOG_GECKO_BOOTLOADER_INTERFACE_PRESENT */

  app_timestamp_init();
  init_app_parameters();

  with_time = to_console = to_rtt = true;
  to_udp = to_coap = false;

#ifdef    APP_CHECK_PREVIOUS_CRASH
  sl_wisun_check_previous_crash();
  if (strlen(crash_info_string)) {
      app_parameters.nb_crashes++;
      save_app_parameters();
      printfBothTime("Info on previous crash: %s\n", crash_info_string);
  }
#endif /* APP_CHECK_PREVIOUS_CRASH */

  osDelay(1000);
  printf("\n");
#ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
  snprintf(application, 100, "%s %s %s %s %d.%d", chip, SL_BOARD_NAME, "Wi-SUN Node Monitoring", APP_VERSION_STRING, START_FLASHES_A, START_FLASHES_B);
#else  /* SL_CATALOG_SIMPLE_LED_PRESENT */
  snprintf(application, 100, "%s %s %s %s", chip, SL_BOARD_NAME, "Wi-SUN Node Monitoring", APP_VERSION_STRING);
#endif /* SL_CATALOG_SIMPLE_LED_PRESENT */
  printfBothTime("%s\n", application);
  snprintf(version, 80, "Compiled on %s at %s", __DATE__, __TIME__);

#ifdef    SL_CATALOG_GECKO_BOOTLOADER_INTERFACE_PRESENT
  bootloader_getStorageInfo(&storage_info);
  #ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
    if (storage_info.info->partSize == 0) {
        leds_flash(100, 25);
        set_leds(0, 0);
        osDelay(1000);
    }
  #endif /* SL_CATALOG_SIMPLE_LED_PRESENT */
  snprintf(version, 80, "Compiled on %s at %s (flash partSize %ld)", __DATE__, __TIME__, storage_info.info->partSize);
#else /* SL_CATALOG_GECKO_BOOTLOADER_INTERFACE_PRESENT */
  snprintf(version, 80, "Compiled on %s at %s (no bootloader)", __DATE__, __TIME__);
#endif /* SL_CATALOG_GECKO_BOOTLOADER_INTERFACE_PRESENT */

  printfBothTime("%s\n", application);
  printfBothTime("%s\n", version);

  printfBothTime("Network %s\n",
    network[app_parameters.network_index].network_name);
#ifdef    SL_CATALOG_APP_OS_STAT_PRESENT
#ifdef APP_OS_STAT_UPDATE_PERIOD_TIME_MS
printfBothTime("with app_os_stat every %d ms\n", APP_OS_STAT_UPDATE_PERIOD_TIME_MS);
#endif /* APP_OS_STAT_UPDATE_PERIOD_TIME_MS */
#endif /* SL_CATALOG_APP_OS_STAT_PRESENT */
printfBothTime("network_size %s\n", app_wisun_trace_util_nw_size_to_str(
    network[app_parameters.network_index].network_size));
#ifdef    WISUN_CONFIG_BROADCAST_RETRIES
  printfBothTime("Broadcast Retries %d\n", WISUN_CONFIG_BROADCAST_RETRIES);
#endif /* WISUN_CONFIG_BROADCAST_RETRIES */
  TRACES_WHILE_CONNECTING;

#ifdef    HISTORY
  snprintf(history_string, SL_WISUN_COAP_RESOURCE_HND_SOCK_BUFF_SIZE, "%s", "");
#endif /* HISTORY */

  #ifdef    SL_CATALOG_WISUN_COAP_PRESENT
    printfBothTime("With     CoAP Support\n");
  #endif /* SL_CATALOG_WISUN_COAP_PRESENT */

  #ifdef    SL_CATALOG_WISUN_OTA_DFU_PRESENT
    printfBothTime("With     OTA DFU Support\n");
  #endif /* SL_CATALOG_WISUN_OTA_DFU_PRESENT */

  #ifdef WITH_TCP_SERVER
    printfBothTime("With     TCP Server %s\n", DEFINE_string(WITH_TCP_SERVER));
  #endif /* WITH_TCP_SERVER */

  #ifdef WITH_UDP_SERVER
    printfBothTime("With     UDP Server %s\n", DEFINE_string(WITH_UDP_SERVER));
  #endif /* WITH_UDP_SERVER */

  #ifdef WITH_DIRECT_CONNECT
    printfBothTime("With     Direct Connect\n");
  #endif /* WITH_DIRECT_CONNECT */

  // Set device_tag to last 2 bytes of MAC address
  sl_wisun_get_mac_address(&device_mac);
  sprintf(device_mac_string, "%s", app_wisun_mac_addr_to_str(&device_mac));
  sprintf(device_tag, "%02x%02x", device_mac.address[6], device_mac.address[7]);
  printfBothTime("device MAC %s\n", device_mac_string);
  printfBothTime("device_tag %s\n", device_tag);

  // Set device_type based on application settings
#ifdef    WISUN_CONFIG_DEVICE_TYPE
  if (WISUN_CONFIG_DEVICE_TYPE == SL_WISUN_LFN ) {
    #if !defined(WISUN_CONFIG_DEVICE_PROFILE)
      sprintf(device_type, "LFN (null profile)");
      #else /* WISUN_CONFIG_DEVICE_PROFILE */
      switch (WISUN_CONFIG_DEVICE_PROFILE) {
        case SL_WISUN_LFN_PROFILE_TEST: {
          sprintf(device_type, "LFN (Test Profile)");
          break;
        }
        case SL_WISUN_LFN_PROFILE_BALANCED: {
          sprintf(device_type, "LFN (Balanced Profile)");
          break;
        }
        case SL_WISUN_LFN_PROFILE_ECO: {
          sprintf(device_type, "LFN (Eco Profile)");
          break;
        }
        default: {
          sprintf(device_type, "LFN (NO Profile)");
          break;
        }
      }
    #endif  /* WISUN_CONFIG_DEVICE_PROFILE */
  } else if (WISUN_CONFIG_DEVICE_TYPE == SL_WISUN_ROUTER ) {
#ifdef   SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT
      sprintf(device_type, "FFN with LFN support");
#else /* SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT */
    sprintf(device_type, "FFN with No LFN support");
#endif /* SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT */
  }
  printfBothTime("device_type %s (%d)\n", device_type, WISUN_CONFIG_DEVICE_TYPE);
#else  /* WISUN_CONFIG_DEVICE_TYPE */
  #ifdef   SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT
    sprintf(device_type, "FFN with LFN support");
#else /* SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT */
  sprintf(device_type, "FFN with No LFN support");
#endif /* SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT */
#endif /* WISUN_CONFIG_DEVICE_TYPE */

#ifdef    SL_CATALOG_SIMPLE_BUTTON_PRESENT
  B0 = ( sl_button_get_state(&sl_button_btn0) == SL_SIMPLE_BUTTON_PRESSED );
  B1 = ( sl_button_get_state(&sl_button_btn1) == SL_SIMPLE_BUTTON_PRESSED );
  startup_option = B0 + (B1 << 1);
  printfBothTime("Startup option %d ('%d%d')\n", startup_option, B1, B0);
  check_buttons = true;
  network[app_parameters.network_index].device_type = SL_WISUN_ROUTER;
  if (startup_option == 1) {
    sl_wisun_clear_credential_cache();
    printfBothTime("Cleared credential cache (startup_option 1)\n");
  }
  if (startup_option == 2) {
      network[app_parameters.network_index].device_type = SL_WISUN_LFN;
    printfBothTime("Starting as LFN (startup_option = 2)\n");
    sprintf(device_type, "LFN by user choice");
  }
#endif /* SL_CATALOG_SIMPLE_BUTTON_PRESENT */

  printfBothTime("device_type %s\n", device_type);

#ifdef    WITH_DIRECT_CONNECT
  if (network[app_parameters.network_index].device_type == SL_WISUN_ROUTER) { // Only FFNs support Direct Connect
    // Register our Direct Connect custom callback function with the event manager (aka 'em')
    app_wisun_em_custom_callback_register(SL_WISUN_MSG_DIRECT_CONNECT_LINK_AVAILABLE_IND_ID, app_direct_connect_custom_callback);
    app_wisun_em_custom_callback_register(SL_WISUN_MSG_DIRECT_CONNECT_LINK_STATUS_IND_ID   , app_direct_connect_custom_callback);
    app_direct_connect(true);
  }
#endif /* WITH_DIRECT_CONNECT */

  // Register our join state custom callback function with the event manager (aka 'em')
  app_wisun_em_custom_callback_register(SL_WISUN_MSG_JOIN_STATE_IND_ID, _join_state_custom_callback);

#ifdef    AUTO_CLEAR_CREDENTIAL_CACHE
  sl_wisun_clear_credential_cache();
  printfBothTime("Cleared credential cache\n");
#endif /* AUTO_CLEAR_CREDENTIAL_CACHE */

#ifdef    LIST_RF_CONFIGS
  list_rf_configs();
#endif /* LIST_RF_CONFIGS */

  printfBothTime("app_parameters.auto_send_sec %d\n", app_parameters.auto_send_sec);

#ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
  // LEDs indicate the 'version' in 2 steps
  leds_flash(START_FLASHES_A, 250);
  set_leds(0, 0);
  osDelay(1000);
  leds_flash(START_FLASHES_B, 250);
  set_leds(0, 0);
  osDelay(1000);
  // LEDs show startup option for 1 sec
  set_leds(B1, B0);
  osDelay(1000);
  // LEDs cleared to follow the join state (must be 1 if no credentials, 3 otherwise)
  // If LEDs stay at 0: check selected PHY vs Radio Config
  set_leds(0, 0);
#endif /* SL_CATALOG_SIMPLE_LED_PRESENT */

#ifdef WITH_TCP_SERVER
  init_tcp_server();
#endif /* WITH_TCP_SERVER */

#ifdef WITH_UDP_SERVER
  init_udp_server();
#endif /* WITH_UDP_SERVER */

  // Store the time where we call app_join_network()
  connect_time_sec = now_sec();
  // connect to the wisun network
  join_res = app_join_network(app_parameters.network_index);

  while (1) { // To allow a Direct Connect connection, regularly check UDP messages
      #ifdef    WITH_DIRECT_CONNECT
        #ifdef    WITH_UDP_SERVER
        check_udp_server_messages();
        #endif /* WITH_UDP_SERVER */
      #endif /* WITH_DIRECT_CONNECT */
      now = now_sec();
      sl_wisun_get_join_state(&join_state);
      if (join_state == SL_WISUN_JOIN_STATE_OPERATIONAL) break;
      if (join_res == SL_STATUS_OK) {
        if (now % 60 == 0) {
            printfBothTime("Waiting for connection to network[%d]: \"%s\": %s. join_state %d\n",
                       app_parameters.network_index,
                       network[app_parameters.network_index].network_name,
                       app_wisun_phy_to_str(&network[app_parameters.network_index].phy),
                       join_state);
        }
      } else {
        if (join_res != SL_STATUS_OK) {
            if (join_res == SL_STATUS_FAIL) {
                printfBothTime("Invalid PHY\n");
            } else {
                printfBothTime("Check the traces and the app_join_network() code around line %d\n", join_res);
            }
            printfBothTime("Will not connect to \"%s\": %s. join_res %d\n",
                       network[app_parameters.network_index].network_name,
                       app_wisun_phy_to_str(&network[app_parameters.network_index].phy),
                       join_res);
            app_parameters.network_index = (app_parameters.network_index + 1) % MAX_NETWORK_CONFIGS;
            printfBothTime("Attempting to connect to network[%d]: \"%s\": %s\n",
                       app_parameters.network_index,
                       network[app_parameters.network_index].network_name,
                       app_wisun_phy_to_str(&network[app_parameters.network_index].phy));
            connect_time_sec = now_sec();
            join_res = app_join_network(app_parameters.network_index);
        }
      }
      osDelay(1000);
  }

  /*******************************************
  /  We only reach this part once connected  /
  *******************************************/
  connection_timestamp = now_sec();
  // Once connected for the first time, reduce RTT traces to the minimum
  TRACES_WHEN_CONNECTED;

  // Get ready to listen to and send notifications to the Border Router
  //  also get ready for CoAP communication
  _open_udp_sockets();

#ifdef    SL_CATALOG_WISUN_OTA_DFU_PRESENT
  in6_addr_t global_ipv6;

  printf("OTA DFU will download chunks of '<TFTP_DIRECTORY>/%s' from %s/%d\n",
        SL_WISUN_OTA_DFU_GBL_FILE,
        SL_WISUN_OTA_DFU_HOST_ADDR,
        SL_WISUN_OTA_DFU_TFTP_PORT
      );

  sl_wisun_get_ip_address(SL_WISUN_IP_ADDRESS_TYPE_GLOBAL, &global_ipv6);
  printf("OTA DFU 'start' command:\n");
  sprintf(device_global_ipv6_string, app_wisun_trace_util_get_ip_str(&global_ipv6));
  printf(" coap-client -m post -N -B 10 -t text coap://[%s]:%d%s -e \"start\"\n",
        device_global_ipv6_string,
        5683,
        SL_WISUN_OTA_DFU_URI_PATH
      );
  printf("Follow OTA DFU progress (from node, intrusive) using:\n");
  printf(" coap-client -m get -N -B 10 -t text coap://[%s]:%d%s\n",
      device_global_ipv6_string,
      SL_WISUN_COAP_RESOURCE_HND_SERVICE_PORT,
      SL_WISUN_OTA_DFU_URI_PATH
  );

  if (SL_WISUN_OTA_DFU_HOST_NOTIFY_ENABLED) {
      printf("OTA DFU notifications enabled (every %d chunks)\n",
        SL_WISUN_OTA_DFU_NOTIFY_DOWNLOAD_CHUNK_CNT
      );
      printf("OTA DFU notifications will be POSTed to notification server coap://[%s]:%d%s\n",
        SL_WISUN_OTA_DFU_NOTIFY_HOST_ADDR,
        SL_WISUN_OTA_DFU_NOTIFY_PORT,
        SL_WISUN_OTA_DFU_NOTIFY_URI_PATH
      );
    printf("Follow OTA DFU progress (from notification server) using:\n");
    printf(" coap-client -m get -N -B 1 -t text coap://[%s]:%d%s\n",
      SL_WISUN_OTA_DFU_NOTIFY_HOST_ADDR,
      SL_WISUN_OTA_DFU_NOTIFY_PORT,
      SL_WISUN_OTA_DFU_NOTIFY_URI_PATH
    );
  }
#endif /* SL_CATALOG_WISUN_OTA_DFU_PRESENT */

  // Print info on possible CoAP commands, now that CoAP communication is set
  print_coap_help(device_global_ipv6_string, border_router_ipv6_string);

  // Print and send initial connection message
  to_udp  = true;
  to_coap = false;
  print_and_send_messages (_connection_json_string(""),
              with_time, to_console, to_rtt, to_udp, to_coap);

#ifdef    APP_CHECK_PREVIOUS_CRASH
  if (strlen(crash_info_string)) {
      osDelay(2UL);
      print_and_send_messages (crash_info_string,
                  with_time, to_console, to_rtt, to_udp, to_coap);
  }
#endif /* APP_CHECK_PREVIOUS_CRASH */

#ifdef    APP_TRACK_HEAP
  sl_memory_get_heap_info(&app_heap_info);
  #ifdef    APP_TRACK_HEAP_DIFF
  app_previous_heap_free = app_heap_info.free_size;
  #endif /* APP_TRACK_HEAP_DIFF */
  refresh_heap = true;
#endif /* APP_TRACK_HEAP */

  osDelay(2UL);

  while (1) {
    ///////////////////////////////////////////////////////////////////////////
    // Put your application code here!                                       //
    ///////////////////////////////////////////////////////////////////////////

      now = now_sec();
      // Use the connection time as reference, in order to spread messages in time
      // when several devices are powered on at the same time
      connected_delay_sec = now - connection_timestamp;

    // We can only send messages outside if connected
    if (join_state == SL_WISUN_JOIN_STATE_OPERATIONAL) {
      to_udp  = true;
      to_coap = false;

      #ifdef    WITH_TCP_SERVER
        check_tcp_server_messages();
      #endif /* WITH_TCP_SERVER */

      #ifdef    WITH_UDP_SERVER
        check_udp_server_messages();
      #endif /* WITH_UDP_SERVER */

#ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
      // 1 Sec join state 5 indicator
      change_leds = connected_delay_sec % 4;
      if (change_leds != previous_change_leds) {
          if (change_leds == 0) set_leds(0, 1);
          if (change_leds == 1) set_leds(1, 1);
          if (change_leds == 2) set_leds(1, 0);
          if (change_leds == 3) set_leds(0, 0);
          previous_change_leds = change_leds;
      }
#endif /* SL_CATALOG_SIMPLE_LED_PRESENT */
    } else {

#ifdef    WITH_DIRECT_CONNECT
  #ifdef    WITH_UDP_SERVER
  // Direct Connect connection is possible even when not connected to the Wi-SUN network
  check_udp_server_messages();
  #endif /* WITH_UDP_SERVER */
#endif /* WITH_DIRECT_CONNECT */

#ifdef    AUTO_CLEAR_CREDENTIAL_CACHE
      if (just_disconnected) {
          just_disconnected = false;
          sl_wisun_disconnect();
          sl_wisun_clear_credential_cache();
          app_wisun_network_connect();
      }
#endif /* AUTO_CLEAR_CREDENTIAL_CACHE */
      to_udp = to_coap = false;
#ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
      // 1 Sec non join state 5 indicator
      change_leds = connected_delay_sec % 4;
      if (change_leds != previous_change_leds) {
          if (change_leds == 0         ) leds_f_join_state(join_state);
          if (change_leds == join_state) set_leds(0, 0);
          previous_change_leds = change_leds;
      }
#endif /* SL_CATALOG_SIMPLE_LED_PRESENT */
    }

    // Print status message once then disable status
    if ((connected_delay_sec % app_parameters.auto_send_sec == 0) || (send_asap)) {
        if ((print_keep_alive == true) || (send_asap)) {
          print_and_send_messages (_status_json_string(""),
                    with_time, to_console, to_rtt, to_udp, to_coap);
          print_keep_alive = false;
          send_asap = false;
          #ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
          sl_led_toggle(&sl_led_led0);
          sl_led_toggle(&sl_led_led1);
          #endif /* SL_CATALOG_SIMPLE_LED_PRESENT */
        }
    }
    // Enable status for next time
    if (connected_delay_sec % app_parameters.auto_send_sec == 1) {
        if (print_keep_alive == false) {
            print_keep_alive = true;
        }
    }

#ifdef    APP_TRACK_HEAP
    // Refresh heap info once then disable the refresh
    if (connected_delay_sec % 5 == 0) {
        if (refresh_heap) {
          sl_memory_get_heap_info(&app_heap_info);
#ifdef    APP_TRACK_HEAP_DIFF
          if (app_previous_heap_free == 0) { app_previous_heap_free = app_heap_info.free_size; }
          if (app_previous_heap_free != app_heap_info.free_size) {
            printfBothTime("heap free %8d used %8d %6.2f%% (diff %5d)\n",
                        app_heap_info.free_size,
                         app_heap_info.used_size,
                        1.0*app_heap_info.used_size / (app_heap_info.total_size / 100),
                        app_heap_info.free_size - app_previous_heap_free
            );
          }
          app_previous_heap_free = app_heap_info.free_size;
#endif /* APP_TRACK_HEAP_DIFF */
          refresh_heap = false;
        }
    }
    // Enable heap info refresh for next time
    if (connected_delay_sec % 5 == 1) { refresh_heap = true; }
#endif /* APP_TRACK_HEAP */

#ifdef    SL_CATALOG_SIMPLE_BUTTON_PRESENT
    if (connected_delay_sec % BUTTON_CHECK_DELAY == 0) {
      if (check_buttons == true) {
        check_buttons = false;
        B0 = ( sl_button_get_state(&sl_button_btn0) == SL_SIMPLE_BUTTON_PRESSED );
        B1 = ( sl_button_get_state(&sl_button_btn1) == SL_SIMPLE_BUTTON_PRESSED );
        #ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
         if (B0) sl_led_turn_on(&sl_led_led0);
         if (B1) sl_led_turn_on(&sl_led_led1);
        #endif /* SL_CATALOG_SIMPLE_BUTTON_PRESENT */
        if (B0 + B1) {
          print_and_send_messages (_button_json_string(""),
                    with_time, to_console, to_rtt, to_udp, to_coap);
        #ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
           sl_led_turn_off(&sl_led_led0);
           sl_led_turn_off(&sl_led_led1);
        #endif /* SL_CATALOG_SIMPLE_BUTTON_PRESENT */
        }
      }
    }
    if (connected_delay_sec % BUTTON_CHECK_DELAY == 1) {
      check_buttons = true;
    }
#endif /* SL_CATALOG_SIMPLE_BUTTON_PRESENT */

    if (network[app_parameters.network_index].device_type == SL_WISUN_LFN) {
      osDelay(1000UL);
    } else {
      osDelay(1UL);
    }
  }
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------

void app_reset_statistics(void) {
  connection_time_sec = now_sec();
  disconnection_time_sec = connection_time_sec;
  connection_count = 0;
  connected_total_sec = 0;
  disconnected_total_sec = 0;
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
sl_wisun_mac_address_t _get_parent_mac_address_and_update_parent_info(void) {
  sl_status_t ret;
  uint8_t neighbor_count;
  uint8_t i;
  sl_wisun_neighbor_info_t neighbor_info;
  for ( i = 0 ; i<  SL_WISUN_MAC_ADDRESS_SIZE ; i++) {
      parent_mac.address[i] = 0;
      secondary_mac.address[i] = 0;
  }

  ret = sl_wisun_get_neighbor_count(&neighbor_count);
  if (ret) {printfBothTime("[Failed: sl_wisun_get_neighbor_count() returned 0x%04x]\n", (uint16_t)ret);}
  sl_wisun_mac_address_t neighbor_mac_addresses[neighbor_count];
  ret = sl_wisun_get_neighbors(&neighbor_count, neighbor_mac_addresses);
  if (ret) {printfBothTime("[Failed: sl_wisun_get_neighbors() returned 0x%04x]\n", (uint16_t)ret);}
  for (i = 0 ; i < neighbor_count; i++) {
      sl_wisun_get_neighbor_info(&neighbor_mac_addresses[i], &neighbor_info);
      if (neighbor_info.type == SL_WISUN_NEIGHBOR_TYPE_PRIMARY_PARENT) {
        sl_wisun_get_neighbor_info(&neighbor_mac_addresses[i], &parent_info);
        parent_mac = neighbor_mac_addresses[i];
      }
      if (neighbor_info.type == SL_WISUN_NEIGHBOR_TYPE_SECONDARY_PARENT) {
        sl_wisun_get_neighbor_info(&neighbor_mac_addresses[i], &secondary_info);
        secondary_mac = neighbor_mac_addresses[i];
      }
  }
  return parent_mac;
}

void refresh_parent_tag(void) {
  _get_parent_mac_address_and_update_parent_info();
  sprintf(parent_tag, "%02x%02x", parent_mac.address[6], parent_mac.address[7]);
  sprintf(secondary_tag, "%02x%02x", secondary_mac.address[6], secondary_mac.address[7]);
};

void  _join_state_custom_callback(sl_wisun_evt_t *evt) {
  int i;
  uint64_t delay;
  // Use flags to select trace destinations
  // Traces can only be sent over UDP or CoAP once connected, so
  //  to_udp and to_coap are false by default.
  bool with_time, to_console, to_rtt, to_udp, to_coap;
  with_time = to_console = to_rtt = true;
  to_udp = to_coap = false;

  join_state = (sl_wisun_join_state_t)evt->evt.join_state.join_state;
  if (join_state >  SL_WISUN_JOIN_STATE_OPERATIONAL) {
      // Do not process intermediate join states, whose values are > 5
      return;
  }
  if (join_state != previous_join_state) {
    // join_state changed...
    // print current join_state
    printfBothTime("[Join state %u->%u]\n", previous_join_state, join_state);
    if (join_state < min_join_state) { min_join_state = join_state; }
    if ((join_state > SL_WISUN_JOIN_STATE_DISCONNECTED) && (join_state <= SL_WISUN_JOIN_STATE_OPERATIONAL)) {
      app_join_state_sec[join_state] = now_sec();
      // Store transition delay
      delay = app_join_state_delay_sec[join_state] = app_join_state_sec[join_state] - app_join_state_sec[join_state-1];
      printfBothTime("app_join_state_delay_sec[%d] = %llu sec\n", join_state, delay);
    }

    if (join_state == SL_WISUN_JOIN_STATE_OPERATIONAL) {
      connection_time_sec = app_join_state_sec[join_state];
      connection_count ++;
      if (min_join_state <= SL_WISUN_JOIN_STATE_ACQUIRE_PAN_CONFIG) {
          network_connection_count ++;
      }
      // reset min_join_state to follow the next network reconnection
      min_join_state = SL_WISUN_JOIN_STATE_OPERATIONAL;
      // Count disconnected time only once connected for the first time
      // (it would be unfair to count time before if the Border
      //  Router is off when the node is started)
      if (connection_count == 1) {
        printfBothTime("First connection after %llu sec\n", connection_time_sec);
        disconnected_total_sec = 0;
      } else {
        printfBothTime("Reconnected after %llu sec\n", connection_time_sec - disconnection_time_sec);
        disconnected_total_sec += connection_time_sec - disconnection_time_sec;
      }

#ifdef    HISTORY
      APPEND_TO_HISTORY(" (%d) %s |", join_state , now_str());
#endif /* HISTORY */
      parent_mac = _get_parent_mac_address_and_update_parent_info();
      sprintf(parent_tag, "%02x%02x", parent_mac.address[6], parent_mac.address[7]);
      // if sockets are opened, print and send connection message
      // This will occur in case of a reconnection
      if (udp_notification_socket_id) {
        to_udp = to_coap = true;
        print_and_send_messages (_connection_json_string(""),
            with_time, to_console, to_rtt, to_udp, to_coap);
      }
      TRACES_WHEN_CONNECTED;
    }
    // Prepare counting disconnected time
    if (previous_join_state == SL_WISUN_JOIN_STATE_OPERATIONAL) {
      for (i=0; i<=join_state; i++) {
        // Clear join_state info for lower and equal join states
        app_join_state_sec[i] = now_sec();
      }
      for (i=0; i<=join_state; i++) {
        app_join_state_delay_sec[i+1] = app_join_state_sec[i+1] - app_join_state_sec[i];
      }
      disconnection_time_sec = app_join_state_sec[join_state];
      printfBothTime("Disconnected after %llu sec\n", disconnection_time_sec - connection_time_sec);
      connected_total_sec += disconnection_time_sec - connection_time_sec;
#ifdef    HISTORY
      APPEND_TO_HISTORY(" (%d) %s /", join_state , now_str());
#endif /* HISTORY */
      just_disconnected = true;
      TRACES_WHILE_CONNECTING;
    }
    previous_join_state = join_state;
#ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
    leds_f_join_state(join_state);
#endif /* SL_CATALOG_SIMPLE_LED_PRESENT */
  }
}

void  _check_neighbors(void) {
  sl_status_t ret;
  uint8_t neighbor_count;
  uint8_t i;
  ret = sl_wisun_get_neighbor_count(&neighbor_count);
  if (ret) {
      printfBothTime("[Failed: sl_wisun_get_neighbor_count() returned 0x%04x]\n", (uint16_t)ret);
  }
  if (neighbor_count == 0) {
    printf(" no neighbor\n");
  } else {
    for (i = 0 ; i < neighbor_count; i++) {
      printf("%s\n", app_neighbor_info_str(i));
    }
  }
}

char* _connection_json_string () {
  #define CONNECTION_JSON_FORMAT_STR                    \
    START_JSON                                          \
    DEVICE_CHIP_JSON_FORMAT                             \
    PARENT_JSON_FORMAT                                  \
    RUNNING_JSON_FORMAT                                 \
    MSG_COUNT_JSON_FORMAT                               \
    "\"PAN_ID\":\"0x%04x (%d)\",\n"                     \
    "\"preferred_pan_id\":\"0x%04x (%d)\",\n"           \
    "\"hop_count\":\"%d\",\n"                           \
    "\"join_states_sec\": \"%llu %llu %llu %llu %llu\",\n"\
    "\"application\": \"%s\"\n"                           \
    END_JSON

  char sec_string[20];

  sl_wisun_get_network_info(&network_info);
  sprintf(sec_string, "%s", now_str());
  refresh_parent_tag();
  msg_count++;

  snprintf(json_string, SL_WISUN_COAP_RESOURCE_HND_SOCK_BUFF_SIZE,
    CONNECTION_JSON_FORMAT_STR,
    DEVICE_CHIP_ITEMS,
    PARENT_INFO_ITEMS,
    sec_string,
    msg_count,
    network_info.pan_id, network_info.pan_id,
    network[app_parameters.network_index].preferred_pan_id, network[app_parameters.network_index].preferred_pan_id,
    network_info.hop_count,
    app_join_state_delay_sec[1],
    app_join_state_delay_sec[2],
    app_join_state_delay_sec[3],
    app_join_state_delay_sec[4],
    app_join_state_delay_sec[5],
    application
  );
  return json_string;
};

char* _status_json_string (char * start_text) {
  #define CONNECTED_JSON_FORMAT_STR        \
    "%s"                                   \
    START_JSON                             \
    DEVICE_CHIP_JSON_FORMAT                \
    PARENT_JSON_FORMAT                     \
    RUNNING_JSON_FORMAT                    \
    MSG_COUNT_JSON_FORMAT                  \
    TRACK_HEAP_FORMAT_STRING               \
    "\"connected\":\"%s\",\n"              \
    "\"disconnected\":\"%s\",\n"           \
    "\"connections\":\"%d\",\n"            \
    "\"network_connections\":\"%d\",\n"    \
    "\"availability\":\"%6.2f\",\n"        \
    "\"connected_total\":\"%s\",\n"        \
    "\"disconnected_total\":\"%s\",\n"     \
    "\"hop_count\":\"%d\",\n"              \
    "\"phy.crc_fails\": \"%ld\",\n"        \
    "\"phy.tx_timeouts\": \"%ld\",\n"      \
    "\"phy.rx_timeouts\": \"%ld\",\n"      \
    "\"mac.failed_cca_count\": \"%ld\",\n" \
    "\"mac.tx_count\": \"%ld\",\n"         \
    "\"mac.tx_failed_count\": \"%ld\",\n"  \
    "\"mac.rx_count\": \"%ld\",\n"         \
    "\"mac.rx_availability_percentage\": \"%d\",\n" \
    "\"network.ip_no_route\":\"%ld\",\n"   \
    "\"network.ip_routeloop_detect\": \"%ld\"\n" \
    END_JSON

  char running_sec_string[20];
  char connected_string[20];
  char disconnected_string[20];
  char connected_sec_string[20];
  char disconnected_sec_string[20];
  float availability;
  // sl_wisun_statistics_t is a union, so we need one per statistics type
  sl_wisun_statistics_t         network_statistics;
  sl_wisun_statistics_t         mac_statistics;
  sl_wisun_statistics_t         phy_statistics;

  uint64_t current_state_sec;

  sprintf(running_sec_string, "%s", now_str());
  refresh_parent_tag();
  // Make sure of the join state
  sl_wisun_get_join_state(&join_state);
  sl_wisun_get_network_info(&network_info);
  sl_wisun_get_statistics (SL_WISUN_STATISTICS_TYPE_NETWORK, &network_statistics);
  sl_wisun_get_statistics (SL_WISUN_STATISTICS_TYPE_MAC    , &mac_statistics);
  sl_wisun_get_statistics (SL_WISUN_STATISTICS_TYPE_PHY    , &phy_statistics);
  msg_count++;

  if (join_state == SL_WISUN_JOIN_STATE_OPERATIONAL) {
    current_state_sec = now_sec() - connection_time_sec;
    sprintf(connected_string,       "%s", dhms(current_state_sec));
    sprintf(disconnected_string,    "no");
    sprintf(connected_sec_string,   "%s", dhms(connected_total_sec + current_state_sec));
    sprintf(disconnected_sec_string,"%s", dhms(disconnected_total_sec));
    if (connected_total_sec + current_state_sec + disconnected_total_sec) {
        availability = 100.0*(connected_total_sec + current_state_sec)/(connected_total_sec + current_state_sec + disconnected_total_sec);
    } else {
        availability = 100.0;
    }
  } else {
    current_state_sec = now_sec() - disconnection_time_sec;
    sprintf(connected_string, " no (join_state %d)", join_state);
    sprintf(disconnected_string,    "%s", dhms(current_state_sec));
    sprintf(connected_sec_string,   "%s", dhms(connected_total_sec));
    sprintf(disconnected_sec_string,"%s", dhms(disconnected_total_sec + current_state_sec));
    if (connected_total_sec + disconnected_total_sec + current_state_sec) {
        availability = 100.0*(connected_total_sec)/(connected_total_sec + disconnected_total_sec + current_state_sec);
    } else {
        availability = 100.0;
    }
  }

  snprintf(json_string, SL_WISUN_COAP_RESOURCE_HND_SOCK_BUFF_SIZE,
    CONNECTED_JSON_FORMAT_STR,
    start_text,
    DEVICE_CHIP_ITEMS,
    PARENT_INFO_ITEMS,
    running_sec_string,
    msg_count,
    TRACK_HEAP_VALUE
    connected_string,
    disconnected_string,
    connection_count,
    network_connection_count,
    availability,
    connected_sec_string,
    disconnected_sec_string,
    network_info.hop_count,
    phy_statistics.phy.crc_fails,
    phy_statistics.phy.tx_timeouts,
    phy_statistics.phy.rx_timeouts,
    mac_statistics.mac.failed_cca_count,
    mac_statistics.mac.tx_count,
    mac_statistics.mac.tx_failed_count,
    mac_statistics.mac.rx_count,
    mac_statistics.mac.rx_availability_percentage,
    network_statistics.network.ip_no_route,
    network_statistics.network.ip_routeloop_detect
  );

  return json_string;
}

sl_status_t _select_destinations(void) {
  sl_status_t ret = SL_STATUS_OK;

  // Store device IPV6 and set the corresponding string
  ret = sl_wisun_get_ip_address(SL_WISUN_IP_ADDRESS_TYPE_GLOBAL, &device_global_ipv6);
  sl_wisun_ip6tos(device_global_ipv6.address, device_global_ipv6_string);
  NO_ERROR(ret, "Device Global IPv6:            %s\n", device_global_ipv6_string);
  IF_ERROR(ret, "[Failed: unable to retrieve the Device Global IPv6: 0x%04x]\n", (uint16_t)ret);

  // Set the UDP notification destination
  printfBothTime("UDP_NOTIFICATION_DEST: %s\n", network[app_parameters.network_index].udp_notification_dest);
  sl_wisun_stoip6(network[app_parameters.network_index].udp_notification_dest, strlen(network[app_parameters.network_index].udp_notification_dest)
                , udp_notification_sockaddr_in6.sin6_addr.address);
  sl_wisun_ip6tos(udp_notification_sockaddr_in6.sin6_addr.address, udp_notification_ipv6_string);
  printfBothTime("UDP  Notification destination: %s/%5d\n" , udp_notification_ipv6_string, UDP_NOTIFICATION_PORT);

  // Set the CoAP notification destination
  printfBothTime("COAP_NOTIFICATION_DEST: %s\n", network[app_parameters.network_index].coap_notification_dest);
  sl_wisun_stoip6(network[app_parameters.network_index].coap_notification_dest   , strlen(network[app_parameters.network_index].coap_notification_dest)
                , coap_notification_sockaddr_in6.sin6_addr.address);
  sl_wisun_ip6tos(coap_notification_sockaddr_in6.sin6_addr.address, coap_notification_ipv6_string);
  printfBothTime("COAP Notification destination: %s/%5d\n"  , coap_notification_ipv6_string, COAP_NOTIFICATION_PORT);

  return ret;
}

sl_status_t _open_udp_sockets(void){
  sl_status_t ret;

  _select_destinations();

  // UDP Notifications (autonomously sent by the device)
  udp_notification_socket_id = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  ret =(udp_notification_socket_id == SOCKET_INVALID_ID) ? 1 : 0;
  NO_ERROR(ret, "Opened    the UDP  notification socket (id %d)\n", (int)udp_notification_socket_id);
  IF_ERROR_RETURN(ret, "[Failed: unable to open the UDP notification socket]\n");

  udp_notification_sockaddr_in6.sin6_family = AF_INET6;
#if (SL_WISUN_VERSION_MAJOR >= 2)
       // API_2.+
  udp_notification_sockaddr_in6.sin6_port = htons(UDP_NOTIFICATION_PORT);
#else  /* API_2.+ */
  udp_notification_sockaddr_in6.sin6_port = UDP_NOTIFICATION_PORT;
#endif /* API_2.+ */

  // (UDP) CoAP Notifications (autonomously sent by the device)
  ret = coap_notification_socket_id = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  ret =(coap_notification_socket_id == SOCKET_INVALID_ID) ? 1 : 0;
  NO_ERROR(ret, "Opened    the COAP notification socket (id %d)\n", (int)coap_notification_socket_id);
  IF_ERROR_RETURN(ret, "[Failed: unable to open the COAP notification socket]\n");

  coap_notification_sockaddr_in6.sin6_family = AF_INET6;
#if (SL_WISUN_VERSION_MAJOR >= 2)
       // API_2.+
  coap_notification_sockaddr_in6.sin6_port = htons(COAP_NOTIFICATION_PORT);
#else  /* API_2.+ */
  coap_notification_sockaddr_in6.sin6_port = COAP_NOTIFICATION_PORT;
#endif /* API_2.+ */
  coap_notify_ch.sockid = coap_notification_socket_id;
  coap_notify_ch.addr = coap_notification_sockaddr_in6;

  // Prepare CoAP notification packet
  coap_notify_ch.pkt.msg_code = COAP_MSG_CODE_REQUEST_PUT;
  coap_notify_ch.pkt.msg_id = 9002U;
  coap_notify_ch.pkt.msg_type = COAP_MSG_TYPE_NON_CONFIRMABLE;
  coap_notify_ch.pkt.content_format = COAP_CT_JSON;
  coap_notify_ch.pkt.uri_path_ptr = (uint8_t *)SL_WISUN_STATUS_CONNECTION_URI_PATH;
  coap_notify_ch.pkt.uri_path_len = sl_strnlen(
      (char *) SL_WISUN_STATUS_CONNECTION_URI_PATH,
               SL_WISUN_STATUS_JSON_STR_MAX_LEN);

  coap_notify_ch.pkt.token_ptr = NULL;
  coap_notify_ch.pkt.token_len = 0U;
  coap_notify_ch.pkt.options_list_ptr = NULL;

  return SL_STATUS_OK;

};

sl_status_t _coap_notify(char* json_string)
{
  sl_status_t ret = SL_STATUS_OK;
  uint16_t req_buff_size = 0UL;
  uint8_t * buff = NULL;

  coap_notify_ch.pkt.payload_ptr = (uint8_t *)json_string;
  coap_notify_ch.pkt.payload_len = sl_strnlen((char *) coap_notify_ch.pkt.payload_ptr,
                                              SL_WISUN_STATUS_JSON_STR_MAX_LEN);

  req_buff_size = sl_wisun_coap_builder_calc_size(&coap_notify_ch.pkt);

  buff = (uint8_t *) sl_malloc(req_buff_size);
  if (buff == NULL) {
    printfBothTime("_coap_notify() error on line %d: sl_wisun_coap_malloc buff(%d)\n", __LINE__,
                  req_buff_size);
    return __LINE__;
  }
  if (sl_wisun_coap_builder(buff, &coap_notify_ch.pkt) < 0) {
    printfBothTime("_coap_notify() error on line %d: sl_wisun_coap_builder()\n", __LINE__);
  } else {
      /* Casting udp_notification_sockaddr_in6 (type sockaddr_in6_t) to (const struct sockaddr *) to match POSIX socket interface */
      if (sendto(coap_notify_ch.sockid,
                  buff,
                  req_buff_size,
                  0,
                  (const struct sockaddr *) &coap_notification_sockaddr_in6,
                  sizeof(sockaddr_in6_t)) == -1) {
          printfBothTime("_coap_notify() error on line %d: sendto(%ld)\n", __LINE__, coap_notify_ch.sockid);
          ret = SL_STATUS_TRANSMIT;
      }
  }
  sl_free(buff);
  return ret;
}

uint8_t print_and_send_messages (char *in_msg, bool with_time,
                            bool to_console, bool to_rtt, bool to_udp, bool to_coap) {
  sl_status_t ret = SL_STATUS_OK;
  uint8_t messages_processed = 0;
  uint16_t udp_msg_len;
  uint16_t coap_msg_len;

  if (to_console == true) { // Print to console
      if (with_time == true) {
        printfTime(in_msg);
      } else {
        printf(in_msg);
      }
    messages_processed++;
  }
  if (to_rtt == true) {     // Print to RTT traces
      if (with_time == true) {
        printfTimeRTT(in_msg);
      } else {
        printfRTT(in_msg);
      }
    messages_processed++;
  }
  if (to_udp == true) {     // Send to UDP port
    udp_msg_len  = snprintf(udp_msg,  1024, "%s", in_msg);
    if (sendto(udp_notification_socket_id,
                (uint8_t *)udp_msg,
                udp_msg_len,
                0L,
                (const struct sockaddr *) &udp_notification_sockaddr_in6,
                sizeof(sockaddr_in6_t)) == -1) {
      printfBothTime("\n[Failed (line %d): unable to send to the UDP notification socket (%d %s/%d)] udp_msg_len %d\n", __LINE__,
              (int)udp_notification_socket_id, udp_notification_ipv6_string , UDP_NOTIFICATION_PORT, udp_msg_len);
    } else {
      messages_processed++;
    }
  }
  if (to_coap == true) {    // Send to CoAP notification port
    coap_msg_len = snprintf(coap_msg, 1024, "%s", in_msg);
    if (coap_msg_len > SL_WISUN_STATUS_JSON_STR_MAX_LEN) {
        printfBothTime("\n[Failed (line %d): CoAP message len %d is higher than MAX %d]. Message not sent because it would overflow\n", __LINE__,
                coap_msg_len, SL_WISUN_STATUS_JSON_STR_MAX_LEN);
    } else {
      ret = _coap_notify(coap_msg);
      IF_ERROR(ret, "[Failed (line %d): unable to send to the CoAP notification socket (%d %s/%d): 0x%04x. Check sl_status.h]\n", __LINE__,
              (int)coap_notification_socket_id, coap_notification_ipv6_string, COAP_NOTIFICATION_PORT, (uint16_t)ret);
      if (ret == SL_STATUS_OK) messages_processed++;
    }
  }

  return messages_processed;
}
