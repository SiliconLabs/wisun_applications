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
#include <string.h>
#pragma message ("Build date: " __DATE__ " " __TIME__)

#include "sl_assert.h"
#include "sl_memory_manager.h"
#include "sl_memory_manager_region_config.h"
#include "sl_string.h"

#include "sl_wisun_api.h"
#include "socket/socket.h"
#include "sl_wisun_keychain.h"
#include "sl_wisun_app_core_util.h"
#include "sl_wisun_trace_util.h"
#include "sl_wisun_types.h"
#include "sl_wisun_version.h"

#include "app.h"

#if __has_include("app_action_scheduler.h")
  #include "app_action_scheduler.h"
#endif

#include "sl_event_system.h"

#if __has_include("app_list_configs.h")
  /* app_list_configs.c/.h can be added/removed from the project */
  #include "app_list_configs.h"
#endif

#if __has_include("app_crash_handler.h")
  /* sl_wisun_crash_handler.h comes with the Wi-SUN Crash Handler component */
  #include "app_crash_handler.h"
#endif

#if __has_include("sl_board_control.h")
  #include "sl_board_control.h"
#endif

#ifdef SL_CATALOG_POWER_MANAGER_PRESENT
  /* sl_power_manager.h comes with the Power Manager component */
  #include "sl_power_manager.h"
#endif

#ifdef SL_CATALOG_GECKO_BOOTLOADER_INTERFACE_PRESENT
  /* btl_interface.h comes with the Bootloader Application Interface component */
  #include "btl_interface.h"
  /* config/app_properties_config.h comes with the Bootloader Application Interface component */
  /* Increase SL_APPLICATION_VERSION in config/app_properties_config.h to use DELTA DFU */
  #include "app_properties_config.h"
#endif

#ifdef    SL_CATALOG_SIMPLE_BUTTON_PRESENT
  /* config/sl_simple_button_instances.h comes with the Simple Button component */
  #include "sl_simple_button_instances.h"
#endif /* SL_CATALOG_SIMPLE_BUTTON_PRESENT */

#ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
  /* config/sl_simple_led_instances.h comes with the Simple Button component */
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

#ifndef   APP_VERSION_STRING
  #define APP_VERSION_STRING "V6.2"
#endif /* APP_VERSION_STRING */

#ifdef    SL_WISUN_CRASH_HANDLER_H
  #define PREVIOUS_CRASH_FORMAT_STRING ""
#else  /* SL_WISUN_CRASH_HANDLER_H */
  #define PREVIOUS_CRASH_FORMAT_STRING ""
#endif /* SL_WISUN_CRASH_HANDLER_H */

#define   APP_TRACK_HEAP
#define   APP_TRACK_HEAP_DIFF
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

#if __has_include("app_tcp_server.h")
  #ifdef WITH_TCP_SERVER
    #include "app_tcp_server.h"
  #endif /* WITH_TCP_SERVER */
#endif

#if __has_include("app_udp_server.h")
  #ifdef WITH_UDP_SERVER
    #include "app_udp_server.h"
  #endif /* WITH_UDP_SERVER */
#endif

#if __has_include("app_propagation.h")
  #include "app_propagation.h"
#endif

#if __has_include("app_wisun_multicast_ota.h") && __has_include("sl_wisun_ota_dfu.h")
  #include "app_wisun_multicast_ota.h"
#endif

#if !__has_include("printf.h")
// In case "Tiny printf" component uninstall to disable UART, printf.h is not present
// redirect printf() function else application crashes because uart not initialized
int printf(const char* format, ...){
  (void)format;
  return 0;
}
#endif

#if __has_include("lfn_checks.h")
  /* lfn_checks.h is used only for LFN, to check low-power settings */
  #include "lfn_checks.h"
#endif

#ifdef    APP_DIRECT_CONNECT_H
  #define WITH_DIRECT_CONNECT
#endif /* APP_DIRECT_CONNECT_H */

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
// Macros to treat possible errors
#define NO_ERROR(ret, ...)                   if (ret == SL_STATUS_OK) {printfBothTime(__VA_ARGS__);}
#define IF_ERROR(ret, ...)                   if (ret != SL_STATUS_OK) {printfBothTime("\n"); printfBoth(__VA_ARGS__);}
#define IF_ERROR_RETURN(ret, ...)            if (ret != SL_STATUS_OK) {printfBothTime("\n"); printfBoth(__VA_ARGS__); return ret;}
#define IF_ERROR_INCR(ret, error_count, ...) if (ret != SL_STATUS_OK) {printfBothTime("\n"); printfBoth(__VA_ARGS__); error_count++;}

#ifdef    APP_RTT_TRACES_H
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
#else  /* APP_RTT_TRACES_H */
  #define TRACES_WHILE_CONNECTING /* empty */
  #define TRACES_WHEN_CONNECTED   /* empty */
#endif /* APP_RTT_TRACES_H */

#ifdef    HISTORY
#define APPEND_TO_HISTORY(...) { \
  history_len = strlen(history_string); \
  snprintf(history_string + history_len, \
    SL_WISUN_COAP_RESOURCE_HND_SOCK_BUFF_SIZE - history_len, \
    __VA_ARGS__); \
    printfBoth(__VA_ARGS__); }
#endif /* HISTORY */

// JSON common format strings
#define START_JSON "{\n"

#define END_JSON   "}\n"

#ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
  uint8_t change_leds;
  uint8_t previous_change_leds;
#endif /* SL_CATALOG_SIMPLE_LED_PRESENT */

#define DEVICE_CHIP_ITEMS \
  device_global_ipv6_string,\
  device_tag,\
  chip,\
  device_type_string,\
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
  "\"rpl_rank\":\"%u\",\n"     \
  "\"etx\":\"%u\",\n"          \
  "\"routing_cost\":\"%u\",\n" \
  "\"rsl_in\":\"%u\",\n"       \
  "\"rsl_out\":\"%u\",\n"      \
//  "\"lifetime\":\"%ld\",\n"
//  "\"secondary\":\"%s\",\n"
//  "\"sec_rsl_in\":\"%d\",\n"
//  "\"sec_rsl_out\":\"%d\",\n"

#define RUNNING_JSON_FORMAT \
  "\"running\":\"%s\",\n"

#define MSG_COUNT_JSON_FORMAT \
  "\"msg_count\":\"%lu\",\n"

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
void        _first_breath_ready_custom_callback(sl_wisun_evt_t *evt);

#ifdef APP_ACTION_SCHEDULER_H
static void event_ext_init(void);
static void event_ext_task(void *argument);
static void event_ext_handle_wisun_ind(sl_wisun_evt_t *wisun_evt);
static uint32_t _first_breath_send_action(void *context);
static uint32_t _first_breath_close_action(void *context);
#endif /* APP_ACTION_SCHEDULER_H */

#ifdef    APP_CHECK_NEIGHBORS_H
void        _check_neighbors(void);
#endif /* APP_CHECK_NEIGHBORS_H */
#ifdef    APP_PROPAGATION_H
static char *_propagation_emergency_callback(app_propagation_indication_t indication,
                                             const uint8_t *payload,
                                             uint16_t payload_len);
#ifdef    APP_WISUN_MULTICAST_OTA_H
static char *_propagation_ota_callback(app_propagation_indication_t indication,
                                       const uint8_t *payload,
                                       uint16_t payload_len);
#endif /* APP_WISUN_MULTICAST_OTA_H */
#endif /* APP_PROPAGATION_H */
#ifdef    APP_UDP_SERVER_H
#ifdef    APP_PROPAGATION_H
static bool _udp_propagation_callback(int32_t sockid,
                                      const uint8_t *payload,
                                      uint16_t payload_len,
                                      const sockaddr_in6_t *src_addr,
                                      const sockaddr_in6_t *dst_addr);
#endif /* APP_PROPAGATION_H */
#ifdef    APP_WISUN_MULTICAST_OTA_H
static bool _udp_multicast_ota_callback(int32_t sockid,
                                        const uint8_t *payload,
                                        uint16_t payload_len,
                                        const sockaddr_in6_t *src_addr,
                                        const sockaddr_in6_t *dst_addr);
#endif /* APP_WISUN_MULTICAST_OTA_H */
#ifdef    WITH_DIRECT_CONNECT
static bool _udp_direct_connect_callback(int32_t sockid,
                                         const uint8_t *payload,
                                         uint16_t payload_len,
                                         const sockaddr_in6_t *src_addr,
                                         const sockaddr_in6_t *dst_addr);
#endif /* WITH_DIRECT_CONNECT */
#endif /* APP_UDP_SERVER_H */
char*       _connection_json_string();
char*       _status_json_string (char * start_text);
char        device_mac_string[40];
sl_wisun_network_info_t network_info;
sl_wisun_mac_address_t _get_parent_mac_address_and_update_parent_info(void);
sl_status_t _select_destinations(void);
sl_status_t _open_udp_sockets(void);

#ifdef    SL_WISUN_COAP_H
sl_status_t _coap_notify(char* json_string);
#endif /* SL_WISUN_COAP_H */

uint8_t print_and_send_messages (char *in_msg, bool _with_time,
                            bool _to_console, bool _to_rtt, bool _to_udp, bool _to_coap);

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
uint16_t connection_count = 0;       // number of connections (moving to Join State 5)
uint16_t network_connection_count = 0; // number of network connections (moving to Join State 5 from min Join State 3)
uint64_t join_call_time_sec;         // time stamp of Wisun join call
uint64_t connection_time_sec;        // last connection time stamp
uint64_t disconnection_time_sec;     // last disconnection time stamp
uint64_t connected_total_sec = 0;    // total time connected
uint64_t disconnected_total_sec = 0; // total time disconnected
uint32_t msg_count = 0;              // number of messages sent
sl_wisun_neighbor_info_t parent_info;   // local storage of the parent info
sl_wisun_neighbor_info_t secondary_info;// local storage of the secondary parent info
uint8_t  min_join_state   = 0;       // Used to log Join State changes and check how 'low' it goes
bool with_time, to_console, to_rtt, to_udp, to_coap;
bool     send_asap;                  // Used to trigger sending the status as soon as possible
uint16_t join_res = 0;
uint64_t now = 0;
uint64_t connection_timestamp;
uint64_t connected_delay_sec;
uint64_t next_status_sec;

#ifdef    APP_TRACK_HEAP
sl_memory_heap_info_t app_heap_info;
#define APP_TRACK_HEAP_PERIOD_S  5 //second
 #ifdef    APP_TRACK_HEAP_DIFF
size_t app_previous_heap_free;
 #endif /* APP_TRACK_HEAP_DIFF */
bool   refresh_heap;
#endif /* APP_TRACK_HEAP */

bool time_to_send_status = true;

char chip[8];
char device_tag[8];
char parent_tag[8];
char secondary_tag[8];
char application[100];
char version[80];
char device_type_string[25];
char emergency_saved_running_time[21] = "not_set";
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
        uint16_t app_join_state_count[6];     // number of times each join state (1..5) was entered
// Per-sub-state tracking for join state 4 (entries 1..4 used, mapped from
// SL_WISUN_JOIN_STATE_PARENT_SELECT=41 .. SL_WISUN_JOIN_STATE_DAO=44).
// Index 0 is unused so [join_state - 40] indexes directly.
static  uint64_t app_join_state_4x_sec[5];
        uint64_t app_join_state_4x_delay_sec[5];
        uint16_t app_join_state_4x_count[5];  // number of times each join state 4 sub-state (4.1..4.4) was entered
static uint16_t previous_join_state = 0;
static uint16_t previous_join_state_4x = 0;
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
sockaddr_in6_t first_breath_sockaddr_in6;

// IPv6 address strings (for printing)
char  device_global_ipv6_string[40];
char  border_router_ipv6_string[40];
char  udp_notification_ipv6_string[40];
char  coap_notification_ipv6_string[40];
char  first_breath_ipv6_string[40];

// Notification sockets
sl_wisun_socket_id_t udp_notification_socket_id = 0;
sl_wisun_socket_id_t coap_notification_socket_id = 0;
sl_wisun_socket_id_t first_breath_socket_id = SOCKET_INVALID_ID;

char udp_msg[1024];
char coap_msg[1024];
char first_breath_msg[256];

uint8_t  trace_level = SL_WISUN_TRACE_LEVEL_INFO;    // Trace level for all trace groups

// UDP ports
#define UDP_NOTIFICATION_PORT  1237
#define FIRST_BREATH_NOTIFICATION_PORT 1238
#define FIRST_BREATH_SOCKET_CLOSE_DELAY_MS (60U * 1000U)
#define EVENT_EXT_QUEUE_SIZE 8U
#define EVENT_EXT_TASK_STACK_SIZE_BYTES (1 * 2048UL)
#define COAP_NOTIFICATION_PORT 5685

#ifdef APP_ACTION_SCHEDULER_H
static sl_event_queue_t event_ext_queue = NULL;
static osThreadId_t event_ext_task_id = NULL;

static const osThreadAttr_t event_ext_task_attr = {
  .name = "event_ext",
  .attr_bits = osThreadDetached,
  .cb_mem = NULL,
  .cb_size = 0,
  .stack_mem = NULL,
  .stack_size = EVENT_EXT_TASK_STACK_SIZE_BYTES,
  .priority = osPriorityNormal,
  .tz_module = 0
};
#endif /* APP_ACTION_SCHEDULER_H */

#define SL_WISUN_STATUS_CONNECTION_URI_PATH  "/status/connection"
#define SL_WISUN_STATUS_JSON_STR_MAX_LEN 512

#ifdef    SL_SIMPLE_BUTTON_INSTANCES_H
  bool B0;
  bool B1;
  bool check_buttons;
  #define BUTTON_CHECK_DELAY 2
#endif /* SL_SIMPLE_BUTTON_INSTANCES_H */

// CoAP Notification channel structure definition
#ifdef   SL_WISUN_COAP_H
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
#endif /* SL_WISUN_COAP_H */

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------

#ifdef    APP_PROPAGATION_H
static char *_propagation_emergency_callback(app_propagation_indication_t indication,
                                             const uint8_t *payload,
                                             uint16_t payload_len)
{
  static char confirmation_payload[APP_PROPAGATION_PAYLOAD_MAX_LEN];

  if (payload == NULL) {
    payload = (const uint8_t *)"";
    payload_len = 0U;
  }

  sprintf(emergency_saved_running_time, "%s", dhms(now_sec()));

  printfBothTime("PROPAG EMERGENCY %s payload_len %u payload: %.*s\n",
                 (indication == APP_PROPAGATION_IND_CON) ? "IND_CON" : "IND_NON",
                 (unsigned int)payload_len,
                 (int)payload_len,
                 (const char *)payload);

  if (indication == APP_PROPAGATION_IND_CON) {
    snprintf(confirmation_payload,
             sizeof(confirmation_payload),
             "my_emergency_conf_payload");
    return confirmation_payload;
  }

  return NULL;
}

#ifdef    APP_WISUN_MULTICAST_OTA_H
static char *_propagation_ota_callback(app_propagation_indication_t indication,
                                       const uint8_t *payload,
                                       uint16_t payload_len)
{
  static char ota_payload[APP_PROPAGATION_PAYLOAD_MAX_LEN + 1U];
  static char confirmation_payload[APP_PROPAGATION_PAYLOAD_MAX_LEN];
  int rx_result;

  if (payload == NULL) {
    payload = (const uint8_t *)"";
    payload_len = 0U;
  }

  if (payload_len > APP_PROPAGATION_PAYLOAD_MAX_LEN) {
    payload_len = APP_PROPAGATION_PAYLOAD_MAX_LEN;
  }
  memcpy(ota_payload, payload, payload_len);
  ota_payload[payload_len] = '\0';

  printfBothTime("PROPAG OTA %s payload_len %u\n",
                 (indication == APP_PROPAGATION_IND_CON) ? "IND_CON" : "IND_NON",
                 (unsigned int)payload_len);

  rx_result = multicast_rx(ota_payload, (uint32_t)payload_len, "PROPAG");

  if (indication == APP_PROPAGATION_IND_CON) {
    snprintf(confirmation_payload,
             sizeof(confirmation_payload),
             "OTA_RX_%d",
             rx_result);
    return confirmation_payload;
  }

  return NULL;
}
#endif /* APP_WISUN_MULTICAST_OTA_H */
#endif /* APP_PROPAGATION_H */

#ifdef    APP_UDP_SERVER_H
#ifdef    APP_PROPAGATION_H
static bool _udp_propagation_callback(int32_t sockid,
                                      const uint8_t *payload,
                                      uint16_t payload_len,
                                      const sockaddr_in6_t *src_addr,
                                      const sockaddr_in6_t *dst_addr)
{
  (void)sockid;

  return app_propagation_handle_udp_payload(payload, payload_len, src_addr, dst_addr);
}
#endif /* APP_PROPAGATION_H */

#ifdef    APP_WISUN_MULTICAST_OTA_H
static bool _udp_multicast_ota_callback(int32_t sockid,
                                        const uint8_t *payload,
                                        uint16_t payload_len,
                                        const sockaddr_in6_t *src_addr,
                                        const sockaddr_in6_t *dst_addr)
{
  static char udp_payload[UDP_RECEIVER_BUFFER_SIZE];
  const char *udp_ip_str = NULL;
  uint16_t copy_len;
  int rx_result;

  (void)sockid;
  (void)dst_addr;

  if ((payload == NULL) || (src_addr == NULL)) {
    return false;
  }

  copy_len = payload_len;
  if (copy_len >= sizeof(udp_payload)) {
    copy_len = (uint16_t)(sizeof(udp_payload) - 1U);
  }
  memcpy(udp_payload, payload, copy_len);
  udp_payload[copy_len] = '\0';

  udp_ip_str = app_wisun_trace_util_get_ip_str((void *)&src_addr->sin6_addr);
  rx_result = multicast_rx(udp_payload, (uint32_t)copy_len, udp_ip_str);
  if (udp_ip_str != NULL) {
    sl_free((void *)udp_ip_str);
  }

  return (rx_result != 0);
}
#endif /* APP_WISUN_MULTICAST_OTA_H */

#ifdef    WITH_DIRECT_CONNECT
static bool _udp_direct_connect_callback(int32_t sockid,
                                         const uint8_t *payload,
                                         uint16_t payload_len,
                                         const sockaddr_in6_t *src_addr,
                                         const sockaddr_in6_t *dst_addr)
{
  static char udp_payload[UDP_RECEIVER_BUFFER_SIZE];
  uint16_t copy_len;

  (void)sockid;
  (void)src_addr;
  (void)dst_addr;

  if (payload == NULL) {
    return false;
  }

  copy_len = payload_len;
  if (copy_len >= sizeof(udp_payload)) {
    copy_len = (uint16_t)(sizeof(udp_payload) - 1U);
  }
  memcpy(udp_payload, payload, copy_len);
  udp_payload[copy_len] = '\0';

  printfBothTime(app_direct_connect_cli(udp_payload));
  return true;
}
#endif /* WITH_DIRECT_CONNECT */
#endif /* APP_UDP_SERVER_H */

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
  printf("leds_flash(%d, %d)\n", count, delay_ms);
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

#ifdef    SL_CATALOG_POWER_MANAGER_PRESENT
/** Power Manager Energy Mode Transition checking
 * Inclusion (if Power Manager Component is installed):
#if __has_include("sl_power_manager.h")
  #include "sl_power_manager.h"
#endif
 * Usage:
  1) call init_power_manager_stats() once at init time
  2) call print_power_manager_pm_transitions() to print the EM transitions table (count of transitions)
      Example output:;
EM transitions
            to EM0   to EM1   to EM2
from EM0:        0       57     1155
from EM1:     1086        0      105
from EM2:      126     1134        0
  3) call print_power_manager_delays() to print the time spent in each Energy Mode
      Example output:;
EM Ticks:   2166947    (      66.130 sec: 0-00:01:06)
 in EM0:     515284    (      15.725 sec: 0-00:00:15   23.8 %)
 in EM1:      34487    (       1.052 sec: 0-00:00:01    1.6 %)
 in EM2:    1617176    (      49.352 sec: 0-00:00:49   74.6 %)
*/

  uint32_t pm_transitions[3][3];
  uint32_t pm_ticks_in_EM[3] = { 0, 0, 0 };
  uint32_t pm_tick_freq_hz;
  uint64_t pm_tick_64, previous_pm_tick_64;

  #define EM_EVENT_MASK_ALL      ( \
    SL_POWER_MANAGER_EVENT_TRANSITION_LEAVING_EM0  | \
    SL_POWER_MANAGER_EVENT_TRANSITION_LEAVING_EM1  | \
    SL_POWER_MANAGER_EVENT_TRANSITION_LEAVING_EM2  | \
    SL_POWER_MANAGER_EVENT_TRANSITION_ENTERING_EM0 | \
    SL_POWER_MANAGER_EVENT_TRANSITION_ENTERING_EM1 | \
    SL_POWER_MANAGER_EVENT_TRANSITION_ENTERING_EM2 \
)

  #define EM_EVENT_MASK_MIN      ( \
    SL_POWER_MANAGER_EVENT_TRANSITION_LEAVING_EM2  | \
    SL_POWER_MANAGER_EVENT_TRANSITION_ENTERING_EM0 | \
    SL_POWER_MANAGER_EVENT_TRANSITION_ENTERING_EM1 | \
)

  void my_power_manager_callback(
    sl_power_manager_em_t from,
    sl_power_manager_em_t to) {
      pm_transitions[from][to]++;
      previous_pm_tick_64 = pm_tick_64;
      pm_tick_64 = sl_sleeptimer_get_tick_count64();
      pm_ticks_in_EM[from] += pm_tick_64 - previous_pm_tick_64;
  }

  sl_power_manager_em_transition_event_handle_t event_handle;
  sl_power_manager_em_transition_event_info_t   event_info = {
    .event_mask = EM_EVENT_MASK_ALL,
    .on_event = my_power_manager_callback,
  };

  void init_power_manager_stats(void) {
    sl_power_manager_subscribe_em_transition_event(&event_handle, &event_info);
    pm_tick_freq_hz = sl_sleeptimer_get_timer_frequency();
    pm_tick_64 = previous_pm_tick_64 = sl_sleeptimer_get_tick_count64();
    printf("Power Manager: following Energy Modes transitions\n");
  }

  void clear_power_manager_stats(void) {
    pm_tick_64 = previous_pm_tick_64 = sl_sleeptimer_get_tick_count64();
    for (uint16_t l=0; l<3; l++) {
      pm_ticks_in_EM[l] = 0;
      for (uint16_t c=0; c<3; c++) {
        pm_transitions[l][c] = 0;
      }
    }
    printf("Power Manager: cleared stats\n");
  }

  void print_power_manager_pm_transitions(void) {
    printfTime("EM transitions\n            to EM0   to EM1   to EM2\nfrom EM0: %8ld %8ld %8ld \nfrom EM1: %8ld %8ld %8ld \nfrom EM2: %8ld %8ld %8ld\n",
      pm_transitions[0][0], pm_transitions[0][1], pm_transitions[0][2],
      pm_transitions[1][0], pm_transitions[1][1], pm_transitions[1][2],
      pm_transitions[2][0], pm_transitions[2][1], pm_transitions[2][2]
    );
  }

  void print_power_manager_delays(void) {
    uint64_t total_ticks = pm_ticks_in_EM[0] + pm_ticks_in_EM[1] + pm_ticks_in_EM[2];
    printfTime("EM Ticks:%10lld    (%12.03f sec: %s)\n",
      total_ticks,    (float)total_ticks/pm_tick_freq_hz,      dhms((sl_sleeptimer_timestamp_64_t)(total_ticks / pm_tick_freq_hz)));
    printfTime(" in EM0: %10ld    (%12.03f sec: %s %6.01f %%)\n",
      pm_ticks_in_EM[0], (float)pm_ticks_in_EM[0] / pm_tick_freq_hz, dhms((sl_sleeptimer_timestamp_64_t)((float)pm_ticks_in_EM[0] / pm_tick_freq_hz)), (float)pm_ticks_in_EM[0] / total_ticks * 100 );
    printfTime(" in EM1: %10ld    (%12.03f sec: %s %6.01f %%)\n",
      pm_ticks_in_EM[1], (float)pm_ticks_in_EM[1] / pm_tick_freq_hz, dhms((sl_sleeptimer_timestamp_64_t)((float)pm_ticks_in_EM[1] / pm_tick_freq_hz)), (float)pm_ticks_in_EM[1] / total_ticks * 100 );
    printfTime(" in EM2: %10ld    (%12.03f sec: %s %6.01f %%)\n",
      pm_ticks_in_EM[2], (float)pm_ticks_in_EM[2] / pm_tick_freq_hz, dhms((sl_sleeptimer_timestamp_64_t)((float)pm_ticks_in_EM[2] / pm_tick_freq_hz)), (float)pm_ticks_in_EM[2] / total_ticks * 100 );
  }
#endif /* SL_CATALOG_POWER_MANAGER_PRESENT */


static uint8_t app_set_option(app_settings_wisun_t *nwk_setting) {
  sl_status_t ret = SL_STATUS_OK;

  ret |= sl_wisun_set_option(SL_WISUN_OPTION_JOIN_NODE_COUNT, &nwk_setting->join_node_count, sizeof(nwk_setting->join_node_count));
  ret |= sl_wisun_set_option(SL_WISUN_OPTION_TRAFFIC_LOWPAN_MTU_BYTES, &nwk_setting->lowpan_mtu, sizeof(nwk_setting->lowpan_mtu));
  ret |= sl_wisun_set_option(SL_WISUN_OPTION_TRAFFIC_IPV6_MRU_BYTES, &nwk_setting->ipv6_mru, sizeof(nwk_setting->ipv6_mru));
  ret |= sl_wisun_set_option(SL_WISUN_OPTION_TRAFFIC_MAX_EDFE_FRAGMENT_COUNT, &nwk_setting->max_edfe_fragment_count, sizeof(nwk_setting->max_edfe_fragment_count));
  ret |= sl_wisun_set_option(SL_WISUN_OPTION_MAC_MIN_BE, &nwk_setting->mac.min_be, sizeof(nwk_setting->mac.min_be));
  ret |= sl_wisun_set_option(SL_WISUN_OPTION_MAC_MAX_BE, &nwk_setting->mac.max_be, sizeof(nwk_setting->mac.max_be));
  ret |= sl_wisun_set_option(SL_WISUN_OPTION_MAC_BACKOFF_PERIOD_US, &nwk_setting->mac.backoff_period_us, sizeof(nwk_setting->mac.backoff_period_us));
  ret |= sl_wisun_set_option(SL_WISUN_OPTION_MAC_MAX_CCA_RETRIES, &nwk_setting->mac.max_cca_retries, sizeof(nwk_setting->mac.max_cca_retries));
  ret |= sl_wisun_set_option(SL_WISUN_OPTION_MAC_MAX_FRAME_RETRIES, &nwk_setting->mac.max_frame_retries, sizeof(nwk_setting->mac.max_frame_retries));

  return ret;
}

uint8_t app_join_network(uint8_t network_index) {
  sl_status_t ret;
  sl_wisun_connection_params_t connection_params;
  sl_wisun_ffn_advanced_parameters_t connection_params_adv;
  sl_wisun_join_state_t join_state;
  //sl_wisun_channel_mask_t channel_mask;
#ifdef    SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT
  sl_wisun_lfn_params_t lfn_params;
#endif /* SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT */
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
  const char *phy_str = NULL;

  app_settings_wisun_t this_network;

#if SL_RAIL_IEEE802154_SUPPORTS_G_MODE_SWITCH
  bool set_pom_ie = false;
#endif

  app_parameter_mutex_acquire();
  sl_wisun_get_join_state(&join_state);

  app_parameters.network_index = network_index % MAX_NETWORK_CONFIGS;
  this_network = network[app_parameters.network_index];
  if (join_state != SL_WISUN_JOIN_STATE_DISCONNECTED) {
    printfBoth("Not disconnected: disconnecting...\r\n");
    sl_wisun_disconnect();
  }

  ret = sl_wisun_set_device_type((sl_wisun_device_type_t)this_network.device_type);
  if (ret != SL_STATUS_OK) {
    printfBoth("[Failed: unable to set device type to %d: %lu]\r\n", this_network.device_type, ret);
    ret = __LINE__; goto cleanup;
  }

  // Set device_type based on application settings
#ifdef    SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT
  if (this_network.device_type == SL_WISUN_LFN ) {
      sprintf(device_type_string, "LFN (null profile)");
      switch (this_network.lfn_profile) {
        case SL_WISUN_LFN_PROFILE_TEST: {
          sprintf(device_type_string, "LFN (Test Profile)");
          break;
        }
        case SL_WISUN_LFN_PROFILE_BALANCED: {
          sprintf(device_type_string, "LFN (Balanced Profile)");
          break;
        }
        case SL_WISUN_LFN_PROFILE_ECO: {
          sprintf(device_type_string, "LFN (Eco Profile)");
          break;
        }
        default: {
          sprintf(device_type_string, "LFN (NO Profile)");
          break;
        }
      }
  }
#endif /* SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT */
#ifdef    SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT
  if (this_network.device_type == SL_WISUN_ROUTER ) {
    sprintf(device_type_string, "FFN with LFN support");
  }
#endif /* SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT */
  printfBoth("network[%d].device_type %d (%s)\r\n",
    app_parameters.network_index,
    this_network.device_type,
    device_type_string);

#ifdef TRACK_HEAP_PER_THREAD
    printfBoth("TRACK_HEAP_PER_THREAD\n");
#endif /* TRACK_HEAP_PER_THREAD */

  // For router with network_size != SL_WISUN_NETWORK_SIZE_AUTOMATIC or with special connection parameters
  // set connection parameters else keep it automatic (no call to sl_wisun_set_connection_parameters() API)
  if ((this_network.device_type == SL_WISUN_ROUTER)&&
       ((this_network.network_size != SL_WISUN_NETWORK_SIZE_AUTOMATIC)||(this_network.use_special_connect_param))) {
    if (this_network.use_special_connect_param)
    {
      printfBoth("=== use_special_connect_param true, set connection param to SL_WISUN_PARAMS_PROFILE_SPECIAL ====\r\n");
      connection_params_adv = SL_WISUN_PARAMS_PROFILE_SPECIAL;
      printfBoth("WARNING: sl_wisun_set_ffn_advanced_parameters() AUTO connection parameter overrided\r\n");
      ret = sl_wisun_set_ffn_advanced_parameters(&connection_params_adv);
      if (ret != SL_STATUS_OK) {
        printfBoth("[Failed: unable to set advanced connection parameters: %lu]\r\n", ret);
        ret = __LINE__; goto cleanup;
      }
    } else {
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
          printfBoth("[Failed: unsupported network size %d ]\r\n", this_network.network_size);
          ret = __LINE__; goto cleanup;
      }
      connection_params.traffic.lowpan_mtu = this_network.lowpan_mtu;
      connection_params.traffic.ipv6_mru = this_network.ipv6_mru;
      connection_params.traffic.max_edfe_fragment_count = this_network.max_edfe_fragment_count;
      connection_params.mac.min_be = this_network.mac.min_be;
      connection_params.mac.max_be = this_network.mac.max_be;
      connection_params.mac.backoff_period_us = this_network.mac.backoff_period_us;
      connection_params.mac.max_cca_retries = this_network.mac.max_cca_retries;
      connection_params.mac.max_frame_retries = this_network.mac.max_frame_retries;

      printfBoth("WARNING: sl_wisun_set_connection_parameters() AUTO connection parameter overrided\r\n");
      ret = sl_wisun_set_connection_parameters(&connection_params);
      if (ret != SL_STATUS_OK) {
        printfBoth("[Failed: unable to set connection parameters: %lu]\r\n", ret);
        ret = __LINE__; goto cleanup;
      }
    }
  }

  if ((this_network.device_type == SL_WISUN_ROUTER)&&
       (this_network.network_size == SL_WISUN_NETWORK_SIZE_AUTOMATIC)&&
       (this_network.use_special_connect_param == false)) {
    printfBoth("app_set_option()\r\n");
    // Set option for network size AUTO
    ret = app_set_option(&this_network);
    if (ret != SL_STATUS_OK) {
      printfBoth("[Failed: unable to set option : %lu]\r\n", ret);
      ret = __LINE__; goto cleanup;
    }
  }

  ret = sl_wisun_config_neighbor_table(this_network.max_child_count, this_network.max_neighbor_count, this_network.max_security_neighbor_count);
  if (ret != SL_STATUS_OK) {
    printfBoth("[Failed: unable to set neighbor table sizes (%d, %d, %d): %lu]\r\n", this_network.max_child_count, this_network.max_neighbor_count, this_network.max_security_neighbor_count, ret);
    ret = __LINE__; goto cleanup;
  }

  if (this_network.device_type == SL_WISUN_ROUTER) {
    ret = sl_wisun_set_preferred_pan(this_network.preferred_pan_id);
    if (ret != SL_STATUS_OK) {
      printfBoth("[Failed: unable to set preferred PAN: %lu]\r\n", ret);
      ret = __LINE__; goto cleanup;
    }
  }

#ifdef    SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT
  // For LFN with lfn_profile != SL_WISUN_LFN_PROFILE_AUTOMATIC
  // set lfn parameters else keep it automatic (no call to sl_wisun_set_lfn_parameters() API)
  if ((this_network.device_type == SL_WISUN_LFN) && (this_network.lfn_profile != SL_WISUN_LFN_PROFILE_AUTOMATIC)) {
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
        printfBoth("[Failed: unsupported LFN profile %d]\r\n", this_network.lfn_profile);
        ret = __LINE__; goto cleanup;
    }

    printfBoth("WARNING: sl_wisun_set_lfn_parameters() AUTO lfn parameter overrided\r\n");
    ret = sl_wisun_set_lfn_parameters(&lfn_params);
    if (ret != SL_STATUS_OK) {
      printfBoth("[Failed: unable to set LFN parameters: %lu]\r\n", ret);
      ret = __LINE__; goto cleanup;
    }
  }
#endif /* SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT */

  ret = sl_wisun_set_rx_fifo_size(this_network.rx_fifo_size);
  if (ret != SL_STATUS_OK) {
    printfBoth("[Failed: unable to set RX FIFO size to %"PRId16": %lu]\r\n", this_network.rx_fifo_size, ret);
  }

  ret = sl_wisun_set_tx_power_ddbm(this_network.tx_power_ddbm);
  if (ret != SL_STATUS_OK) {
    printfBoth("[Failed: unable to set TX power to %d: %lu]\r\n", this_network.tx_power_ddbm, ret);
    ret = __LINE__; goto cleanup;
  }

  ret = sl_wisun_set_fan_tps_version(this_network.fan_version);
  if (ret != SL_STATUS_OK) {
    printf("[Failed: unable to set FAN TPS version %d: %lu]\r\n", this_network.fan_version, ret);
    ret = __LINE__; goto cleanup;
  }

#if defined(WISUN_CONFIG_DDP)
  trustedca_count = sl_wisun_keychain_get_trustedca_count();
#else    /* WISUN_CONFIG_DDP */
  trustedca_count = 0;
#endif   /* WISUN_CONFIG_DDP */
  certificate_options = SL_WISUN_CERTIFICATE_OPTION_IS_REF;
  if (trustedca_count == 0) {
    printfBoth("No trusted CA keychain, init from builtin credentials\r\n");
    const uint32_t max_cert_str_len = 2048U;

    // set the trusted certificate
    ret = sl_wisun_set_trusted_certificate(SL_WISUN_CERTIFICATE_OPTION_IS_REF,
                                          _get_cert_str_len(wisun_config_ca_certificate, max_cert_str_len) + 1,
                                          wisun_config_ca_certificate);
    if (ret != SL_STATUS_OK) {
      printfBoth("[Failed: unable to set the trusted certificate: %lu]\n", ret);
      ret = __LINE__; goto cleanup;
    }

    // set the device certificate
    ret = sl_wisun_set_device_certificate(SL_WISUN_CERTIFICATE_OPTION_IS_REF | SL_WISUN_CERTIFICATE_OPTION_HAS_KEY,
                                          _get_cert_str_len(wisun_config_device_certificate, max_cert_str_len) + 1,
                                          wisun_config_device_certificate);
    if (ret != SL_STATUS_OK) {
      printfBoth("[Failed: unable to set the device certificate: %lu]\n", ret);
      ret = __LINE__; goto cleanup;
    }

    // set the device private key
    // NOTE: to use a wrapped PSA private key, the app needs to import the key
    // and use the API sl_wisun_set_device_private_key_id() instead of the one below
    ret = sl_wisun_set_device_private_key(SL_WISUN_PRIVATE_KEY_OPTION_IS_REF,
                                          _get_cert_str_len(wisun_config_device_private_key, max_cert_str_len) + 1,
                                          wisun_config_device_private_key);
    if (ret != SL_STATUS_OK) {
      printfBoth("[Failed: unable to set the device private key: %lu]\n", ret);
      ret = __LINE__; goto cleanup;
    }

  } else {
    printfBoth("%d trusted CA keychains\r\n", trustedca_count);
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
        printfBoth("[Failed: unable to set the trusted certificate: %lu]\r\n", ret);
        ret = __LINE__; goto cleanup;
      }

      sl_free(trustedca);
      trustedca = NULL;
      certificate_options |= SL_WISUN_CERTIFICATE_OPTION_APPEND;
    }

    credential = sl_wisun_keychain_get_credential((sl_wisun_keychain_t)this_network.keychain, this_network.keychain_index);
    if (!credential) {
      printfBoth("[Failed: unable to load credential for keychain %d]\r\n", this_network.keychain_index);
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
      printfBoth("[Failed: unable to set the device certificate: %lu]\r\n", ret);
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
      printfBoth("[Failed: unable to set the device private key: %lu]\r\n", ret);
      ret = __LINE__; goto cleanup;
    }
  }

/*
  ret = app_settings_get_channel_mask(this_network.allowed_channels, &channel_mask);
  ret = sl_wisun_set_channel_mask(&channel_mask);
  if (ret != SL_STATUS_OK) {
    printfBoth("[Failed: unable to set channel mask: %lu]\r\n", ret);
    res = __LINE__; goto cleanup;
  }
*/
  if (this_network.device_type == SL_WISUN_ROUTER) {
    ret = sl_wisun_set_unicast_settings(this_network.uc_dwell_interval_ms);
    if (ret != SL_STATUS_OK) {
      printfBoth("[Failed: unable to set unicast settings: %lu]\r\n", ret);
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
      printfBoth("[Failed: unsupported regulation]\r\n");
      ret = __LINE__; goto cleanup;
  }
  ret = sl_wisun_set_regulation_parameters(regulation_params);
  if (ret != SL_STATUS_OK) {
    printfBoth("[Failed: unable to set regional regulation parameters: %lu]\r\n", ret);
    ret = __LINE__; goto cleanup;
  }

  ret = sl_wisun_set_regulation_tx_thresholds(this_network.regulation_warning_threshold,
                                              this_network.regulation_alert_threshold);
  if (ret != SL_STATUS_OK) {
    printfBoth("[Failed: unable to set regulation TX thresholds: %lu]\r\n", ret);
  }

#if 0
  ret = sl_wisun_set_pti_state(this_network.pti_state);
  if (ret != SL_STATUS_OK) {
    printf("[Failed to set PTI state]\r\n");
    ret = __LINE__; goto cleanup;
  }
#endif

#if       SLI_WISUN_DISABLE_SECURITY
  ret = sl_wisun_set_security_state(app_security_state);
  if (ret != SL_STATUS_OK) {
    printfBoth("[Failed to set security state %"PRIu32"]\r\n", app_security_state);
    ret = __LINE__; goto cleanup;
  }
#endif /* SLI_WISUN_DISABLE_SECURITY */
/*
  // As per RFC3748, "The Identity Response field MUST NOT be null terminated"
  ret = sl_wisun_set_eap_identity(strlen(this_network.eap_identity),
                                  (const uint8_t *)this_network.eap_identity);
  if (ret != SL_STATUS_OK) {
    printfBoth("[Failed to set EAP identity]\r\n");
    ret = __LINE__; goto cleanup;
  }
*/
  if (this_network.device_type == SL_WISUN_ROUTER) {
    ret = sl_wisun_set_first_breath(this_network.first_breath);
    if (ret != SL_STATUS_OK) {
      printfBoth("[Failed: unable to set First Breath to %d: %lu]\r\n",
                 this_network.first_breath,
                 ret);
      ret = __LINE__; goto cleanup;
    }
    printfBothTime("First Breath %s before join\r\n",
                   this_network.first_breath ? "enabled" : "disabled");
  }

  phy_str = app_wisun_phy_to_str(&(network[app_parameters.network_index].phy));
  printfBothTime("Joining Network[%d]: \"%s\": %s]\r\n",
        app_parameters.network_index,
        this_network.network_name,
        phy_str);
  sl_free((void *)phy_str);
  ret = sl_wisun_join((const uint8_t *)this_network.network_name, &this_network.phy);
  if (ret == SL_STATUS_OK) {
    phy_str = app_wisun_phy_to_str(&(network[app_parameters.network_index].phy));
    printfBothTime("[Connecting to Network[%d]: \"%s\": %s]\r\n",
          app_parameters.network_index,
          this_network.network_name,
          phy_str);
    sl_free((void *)phy_str);
  } else {
    printf("[Join failed: %lu]\r\n", ret);
    if (ret == SL_STATUS_FAIL) {
        sl_wisun_get_join_state(&join_state);
        if (join_state == SL_WISUN_JOIN_STATE_DISCONNECTED) {
            phy_str = app_wisun_phy_to_str(&(network[app_parameters.network_index].phy));
            printfBoth("Network[%d]: Incorrect PHY selection: Will never connect on %s\n",
                app_parameters.network_index,
                phy_str);
            sl_free((void *)phy_str);
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
      printfBoth("[Failed: unable to set RX PhyModeId list in POM-IE for Mode Switch: %lu]\r\n", ret);
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
          printfBoth("app_join_network(%d) failed around line %ld\n", app_parameters.network_index, ret);
      } else {
          printfBoth("sl_wisun_join() failed: the most probable cause is an unknown PHY. Check you Radio configuration\n");
      }
  }

  return ret;
}

/* App task function */
void app_task(void *args)
{
  (void) args;
  uint32_t osdelay_msec;
  const char *phy_str = NULL;

#ifdef    SL_CATALOG_SIMPLE_BUTTON_PRESENT
  uint8_t startup_option = 0;
#endif /* SL_CATALOG_SIMPLE_BUTTON_PRESENT */

#ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
  previous_change_leds = 5;
#endif /* SL_CATALOG_SIMPLE_LED_PRESENT */

#ifdef    SL_CATALOG_GECKO_BOOTLOADER_INTERFACE_PRESENT
  BootloaderStorageInformation_t storage_info;
#endif  /* SL_CATALOG_GECKO_BOOTLOADER_INTERFACE_PRESENT */

#ifdef    APP_TIMESTAMP_H
  app_timestamp_init();
#endif /* APP_TIMESTAMP_H */

#ifdef    APP_ACTION_SCHEDULER_H
  event_ext_init();
#endif /* APP_ACTION_SCHEDULER_H */

#ifdef    SL_CATALOG_POWER_MANAGER_PRESENT
  printfBoth("With     Power Manager (for low power)\n");
  init_power_manager_stats();
#endif /* SL_CATALOG_POWER_MANAGER_PRESENT */

#ifdef    APP_PARAMETERS_H
  init_app_parameters();
#endif /* APP_PARAMETERS_H */

#ifdef    SL_WISUN_CRASH_HANDLER_H
  sl_wisun_check_previous_crash();
  if (strlen(crash_info_string)) {
      app_parameters.nb_crashes++;
      save_app_parameters();
      printfBoth("Info on previous crash: %s\n", crash_info_string);
  }
#endif /* SL_WISUN_CRASH_HANDLER_H */

  printf("\n");
  snprintf(chip, 8, "%s", CHIP);

#ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
  snprintf(application, 100, "%s %s %s %s %d.%d", chip, SL_BOARD_NAME, "Wi-SUN Node Monitoring", APP_VERSION_STRING, START_FLASHES_A, START_FLASHES_B);
#else  /* SL_CATALOG_SIMPLE_LED_PRESENT */
  snprintf(application, 100, "%s %s %s %s", chip, SL_BOARD_NAME, "Wi-SUN Node Monitoring", APP_VERSION_STRING);
#endif /* SL_CATALOG_SIMPLE_LED_PRESENT */


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

  printfBoth("%s\n", application);
  printfBoth("%s\n", version);
  printfBoth("Wi-SUN Stack V%d.%d.%d\n", SL_WISUN_VERSION_MAJOR, SL_WISUN_VERSION_MINOR, SL_WISUN_VERSION_PATCH);

  printfBoth("Network[%d] %s\n",
    app_parameters.network_index,
    network[app_parameters.network_index].network_name);


#ifdef    SL_CATALOG_APP_OS_STAT_PRESENT
  #ifdef APP_OS_STAT_UPDATE_PERIOD_TIME_MS
  printfBoth("with app_os_stat every %d ms\n", APP_OS_STAT_UPDATE_PERIOD_TIME_MS);
  #endif /* APP_OS_STAT_UPDATE_PERIOD_TIME_MS */
#endif /* SL_CATALOG_APP_OS_STAT_PRESENT */
  printfBoth("network_size %s\n", app_wisun_trace_util_nw_size_to_str(
    network[app_parameters.network_index].network_size));

#ifdef    WISUN_CONFIG_BROADCAST_RETRIES
  printfBoth("Broadcast Retries %d\n", WISUN_CONFIG_BROADCAST_RETRIES);
#endif /* WISUN_CONFIG_BROADCAST_RETRIES */
  TRACES_WHILE_CONNECTING;


#ifdef    HISTORY
  snprintf(history_string, SL_WISUN_COAP_RESOURCE_HND_SOCK_BUFF_SIZE, "%s", "");
#endif /* HISTORY */

  #ifdef    SL_CATALOG_WISUN_COAP_PRESENT
    printfBoth("With     CoAP Support\n");
  #endif /* SL_CATALOG_WISUN_COAP_PRESENT */

  #ifdef    SL_CATALOG_WISUN_OTA_DFU_PRESENT
    printfBoth("With     OTA DFU Support\n");
  #endif /* SL_CATALOG_WISUN_OTA_DFU_PRESENT */

  #ifdef    APP_TCP_SERVER_H
    printfBoth("With     TCP Server %s\n", DEFINE_string(WITH_TCP_SERVER));
  #endif /* APP_TCP_SERVER_H */

  #ifdef    APP_UDP_SERVER_H
    printfBoth("With     UDP Server %s\n", DEFINE_string(WITH_UDP_SERVER));
  #endif /* APP_UDP_SERVER_H */

  #ifdef WITH_DIRECT_CONNECT
    printfBoth("With     Direct Connect\n");
  #endif /* WITH_DIRECT_CONNECT */

  // Set device_tag to last 2 bytes of MAC address
  sl_wisun_get_mac_address(&device_mac);
  sprintf(device_mac_string, "%s", app_wisun_mac_addr_to_str(&device_mac));
  sprintf(device_tag, "%02x%02x", device_mac.address[6], device_mac.address[7]);
  printfBoth("device MAC %s\n", device_mac_string);
  printfBoth("device_tag %s\n", device_tag);


#ifdef    SL_CATALOG_SIMPLE_BUTTON_PRESENT
  B0 = ( sl_button_get_state(&sl_button_btn0) == SL_SIMPLE_BUTTON_PRESSED );
  B1 = ( sl_button_get_state(&sl_button_btn1) == SL_SIMPLE_BUTTON_PRESSED );
  startup_option = (B1 << 1) + B0;
  printfBoth("Startup option %d ('%d%d')\n", startup_option, B1, B0);
  check_buttons = true;
  if (startup_option > 0) {
    if (startup_option <= 3) {
      printfBoth("Changing network_index from %d to %d based on buttons\n",
                      app_parameters.network_index, startup_option);
      app_parameters.network_index = startup_option;
    }
  }
#endif /* SL_CATALOG_SIMPLE_BUTTON_PRESENT */

  // Use flags to select trace destinations
  // Traces can only be sent over UDP or CoAP once connected, so
  //  to_udp and to_coap are false by default.
  with_time = to_console = to_rtt = true;
  to_udp = to_coap = false;

  // Register our join state custom callback function with the event manager (aka 'em')
  app_wisun_em_custom_callback_register(SL_WISUN_MSG_JOIN_STATE_IND_ID , _join_state_custom_callback);


#ifdef    APP_LIST_CONFIGS_H
  printf("RAIL PHYs (as in config/rail/radio_settings.radioconf, from Wi-SUN Radio Configurator)\n");
  list_rf_configs();
#endif /* APP_LIST_CONFIGS_H */

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

#ifdef    APP_TCP_SERVER_H
  init_tcp_server();
#endif /* APP_TCP_SERVER_H */

#ifdef APP_UDP_SERVER_H
  init_udp_server();

#ifdef    APP_PROPAGATION_H
  int32_t propagation_sockid = app_udp_server_register(APP_PROPAGATION_PORT,
                                                       (const uint8_t *)"PROPAG",
                                                       (uint16_t)(sizeof("PROPAG") - 1U),
                                                       _udp_propagation_callback);
  if (app_propagation_init(propagation_sockid, APP_PROPAGATION_PORT)) {
    printfBoth("Registered UDP PROPAG handler on port %u socket %ld\n",
               (unsigned int)APP_PROPAGATION_PORT,
               (long)propagation_sockid);
  } else {
    printfBoth("Failed to register UDP PROPAG handler on port %u\n",
               (unsigned int)APP_PROPAGATION_PORT);
  }
#endif /* APP_PROPAGATION_H */
#ifdef    APP_WISUN_MULTICAST_OTA_H
  if (app_udp_server_register(APP_UDP_SERVER_PORT_DEFAULT,
                              (const uint8_t *)"OTA ",
                              (uint16_t)(sizeof("OTA ") - 1U),
                              _udp_multicast_ota_callback) != SOCKET_INVALID_ID) {
    printfBoth("Registered UDP OTA handler on port %u\n",
               (unsigned int)APP_UDP_SERVER_PORT_DEFAULT);
  } else {
    printfBoth("Failed to register UDP OTA handler on port %u\n",
               (unsigned int)APP_UDP_SERVER_PORT_DEFAULT);
  }
#endif /* APP_WISUN_MULTICAST_OTA_H */
#ifdef    WITH_DIRECT_CONNECT
  if (app_udp_server_register(APP_UDP_SERVER_PORT_DEFAULT,
                              (const uint8_t *)"wisun",
                              (uint16_t)(sizeof("wisun") - 1U),
                              _udp_direct_connect_callback) != SOCKET_INVALID_ID) {
    printfBoth("Registered UDP Direct Connect handler on port %u\n",
               (unsigned int)APP_UDP_SERVER_PORT_DEFAULT);
  } else {
    printfBoth("Failed to register UDP Direct Connect handler on port %u\n",
               (unsigned int)APP_UDP_SERVER_PORT_DEFAULT);
  }
#endif /* WITH_DIRECT_CONNECT */

#ifdef    APP_PROPAGATION_H
  if (app_propagation_register("EMERGENCY", _propagation_emergency_callback)) {
    printfBoth("Registered PROPAG discriminator EMERGENCY\n");
  } else {
    printfBoth("Failed to register PROPAG discriminator EMERGENCY\n");
  }
#ifdef    APP_WISUN_MULTICAST_OTA_H
  if (app_propagation_register("OTA", _propagation_ota_callback)) {
    printfBoth("Registered PROPAG discriminator OTA\n");
  } else {
    printfBoth("Failed to register PROPAG discriminator OTA\n");
  }
#endif /* APP_WISUN_MULTICAST_OTA_H */
#endif /* APP_PROPAGATION_H */
#endif /* APP_UDP_SERVER_H */




  printf("\n%s Compiled on %s at %s\n", __FILE__, __DATE__, __TIME__);

  // Reset the time to when we call app_join_network()
  join_call_time_sec = app_timestamp_reset();
  // connect to the wisun network
  join_res = app_join_network(app_parameters.network_index);

#ifdef    WITH_DIRECT_CONNECT
  if (network[app_parameters.network_index].device_type == SL_WISUN_ROUTER) { // Only FFNs support Direct Connect
    // Register our Direct Connect custom callback function with the event manager (aka 'em')
    app_wisun_em_custom_callback_register(SL_WISUN_MSG_DIRECT_CONNECT_LINK_AVAILABLE_IND_ID, app_direct_connect_custom_callback);
    app_wisun_em_custom_callback_register(SL_WISUN_MSG_DIRECT_CONNECT_LINK_STATUS_IND_ID   , app_direct_connect_custom_callback);
    app_direct_connect(true);
  }
#endif /* WITH_DIRECT_CONNECT */


#ifdef TRACK_HEAP_PER_THREAD
  sl_memory_manager_print_per_thread_alloc_free(0x00);
#endif /* TRACK_HEAP_PER_THREAD */

  while (1) {
      #ifdef    WITH_DIRECT_CONNECT
      if (network[app_parameters.network_index].device_type == SL_WISUN_ROUTER) { // Only FFNs support Direct Connect
       // To allow a Direct Connect connection, regularly check UDP messages
        #ifdef    APP_UDP_SERVER_H
        #if (WITH_UDP_SERVER == SO_NONBLOCK)
        check_udp_server_messages();
        #endif /* (WITH_UDP_SERVER == SO_NONBLOCK) */
        #endif /* APP_UDP_SERVER_H */
      }
      #endif /* WITH_DIRECT_CONNECT */
      if (join_state == SL_WISUN_JOIN_STATE_OPERATIONAL) break;
      now = now_sec();
      if (join_res == SL_STATUS_OK) {
          phy_str = app_wisun_phy_to_str(&network[app_parameters.network_index].phy);
          printfBothTime("Waiting for %s connection to network[%d]: \"%s\": %s. join_state %d\n",
                    device_type_string,
                    app_parameters.network_index,
                    network[app_parameters.network_index].network_name,
                    phy_str,
                    join_state);
          sl_free((void *)phy_str);

#ifdef   SL_CATALOG_POWER_MANAGER_PRESENT
          print_power_manager_delays();
#endif /* SL_CATALOG_POWER_MANAGER_PRESENT */

      } else {
        if (join_res != SL_STATUS_OK) {
            if (join_res == SL_STATUS_FAIL) {
                printfBoth("Invalid PHY\n");
                printfBoth("Check that this PHY is listed as a RAIL PHY (see above)\n");
            } else {
                printfBoth("Check the traces and the app_join_network() code around line %d\n", join_res);
            }
            phy_str = app_wisun_phy_to_str(&network[app_parameters.network_index].phy);
            printfBoth("Will not connect to \"%s\": %s. join_res %d\n",
                      network[app_parameters.network_index].network_name,
                      phy_str,
                      join_res);
            sl_free((void *)phy_str);
            app_parameters.network_index = (app_parameters.network_index + 1) % MAX_NETWORK_CONFIGS;
            phy_str = app_wisun_phy_to_str(&network[app_parameters.network_index].phy);
            printfBoth("Attempting to connect to network[%d]: \"%s\": %s\n",
                      app_parameters.network_index,
                      network[app_parameters.network_index].network_name,
                      phy_str);
            sl_free((void *)phy_str);
            join_call_time_sec = app_timestamp_reset();
            join_res = app_join_network(app_parameters.network_index);
        }
      }

    if (network[app_parameters.network_index].device_type == SL_WISUN_ROUTER) { // Only FFNs support Direct Connect
      // If using Direct connect, the wait time must be lower than the direct connect timeout (check udp server)
      (void) sl_wisun_app_core_wait_state((1 << SL_WISUN_APP_CORE_STATE_NETWORK_CONNECTED),
                                      20*1000);
    }
    else
    {
      // In case of LFN wait for connected state and wake-up every 60 sec
      (void) sl_wisun_app_core_wait_state((1 << SL_WISUN_APP_CORE_STATE_NETWORK_CONNECTED),
                                      60*1000);
    }

    #ifdef TRACK_HEAP_PER_THREAD
      sl_memory_manager_print_per_thread_alloc_free(0x01);
    #endif /* TRACK_HEAP_PER_THREAD */

  }


  /*******************************************
  /  We only reach this part once connected  /
  *******************************************/
  connection_timestamp = now_sec();

  // Get ready to listen to and send notifications to the Border Router
  //  also get ready for CoAP communication
  _open_udp_sockets();

  // Once connected for the first time, reduce RTT traces to the minimum
  TRACES_WHEN_CONNECTED;


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
  printf(" coap-client -m post -N -B 10 -t text/plain coap://[%s]:%d%s -e \"start\"\n",
        device_global_ipv6_string,
        5683,
        SL_WISUN_OTA_DFU_URI_PATH
      );
  printf("Follow OTA DFU progress (from node, intrusive) using:\n");
  printf(" coap-client -m get -N -B 10 -t text/plain coap://[%s]:%d%s\n",
      device_global_ipv6_string,
      SL_WISUN_COAP_RESOURCE_HND_SERVICE_PORT,
      SL_WISUN_OTA_DFU_URI_PATH
  );

  #if SL_WISUN_OTA_DFU_HOST_NOTIFY_ENABLED
    printf("OTA DFU notifications enabled (every %d chunks)\n",
      SL_WISUN_OTA_DFU_NOTIFY_DOWNLOAD_CHUNK_CNT
    );
    printf("OTA DFU notifications will be POSTed to notification server coap://[%s]:%d%s\n",
      SL_WISUN_OTA_DFU_NOTIFY_HOST_ADDR,
      SL_WISUN_OTA_DFU_NOTIFY_PORT,
      SL_WISUN_OTA_DFU_NOTIFY_URI_PATH
    );
    printf("Follow OTA DFU progress (from notification server) using:\n");
    printf(" coap-client -m get -N -B 1 -t text/plain coap://[%s]:%d%s\n",
      SL_WISUN_OTA_DFU_NOTIFY_HOST_ADDR,
      SL_WISUN_OTA_DFU_NOTIFY_PORT,
      SL_WISUN_OTA_DFU_NOTIFY_URI_PATH
    );
  #endif /* SL_WISUN_OTA_DFU_HOST_NOTIFY_ENABLED */
#endif /* SL_CATALOG_WISUN_OTA_DFU_PRESENT */

#ifdef    SL_CATALOG_WISUN_COAP_PRESENT
  // Print info on possible CoAP commands, now that CoAP communication is set
  print_coap_help(device_global_ipv6_string, border_router_ipv6_string);
#endif /* SL_CATALOG_WISUN_COAP_PRESENT */

  // Print and send initial connection message
  to_udp  = true;
  to_coap = false;
  print_and_send_messages (_connection_json_string(""),
              with_time, to_console, to_rtt, to_udp, to_coap);



#ifdef    SL_WISUN_CRASH_HANDLER_H
  if (strlen(crash_info_string)) {
      osDelay(2UL);
      print_and_send_messages (crash_info_string,
                  with_time, to_console, to_rtt, to_udp, to_coap);
  }
#endif /* SL_WISUN_CRASH_HANDLER_H */

#ifdef    APP_TRACK_HEAP
  sl_memory_get_heap_info(&app_heap_info);
  #ifdef    APP_TRACK_HEAP_DIFF
  app_previous_heap_free = app_heap_info.free_size;
  #endif /* APP_TRACK_HEAP_DIFF */
  refresh_heap = true;
#endif /* APP_TRACK_HEAP */

  printfBothTime("network[%d].auto_send_sec %d\n", app_parameters.network_index, network[app_parameters.network_index].auto_send_sec);

  if (network[app_parameters.network_index].device_type == SL_WISUN_LFN) {
  #ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
    set_leds(0, 0);
  #endif /* SL_CATALOG_SIMPLE_LED_PRESENT */

  }

  #ifdef   SL_CATALOG_POWER_MANAGER_PRESENT
  print_power_manager_pm_transitions();
  print_power_manager_delays();
  #endif /* SL_CATALOG_POWER_MANAGER_PRESENT */

  previous_join_state = join_state;

    ///////////////////////////////////////////////////////////////////////////
    // Put your application code here!                                       //
    ///////////////////////////////////////////////////////////////////////////
  next_status_sec = now_sec() - connection_timestamp;
  while (1) {
    app_do_your_things();
    // Wait auto_send_sec
    if (network[app_parameters.network_index].auto_send_sec == 0) {
      osdelay_msec = 60*1000;
    } else {
      osdelay_msec = network[app_parameters.network_index].auto_send_sec*1000;
    }

    //Router Device never sleep decrease polling time for best performances
    if (network[app_parameters.network_index].device_type == SL_WISUN_ROUTER) {
      osdelay_msec = 1;
    }

    osDelay(osdelay_msec);
  }
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------

void app_do_your_things() {
  now = now_sec();
  // Use the connection time as reference, in order to spread messages in time
  // when several devices are powered on at the same time
  connected_delay_sec = now - connection_timestamp;

  // We can only send messages outside if connected
  if (join_state == SL_WISUN_JOIN_STATE_OPERATIONAL) {
    to_udp  = true;
    to_coap = false;

    #ifdef    APP_TCP_SERVER_H
      check_tcp_server_messages();
    #endif /* APP_TCP_SERVER_H */

    #ifdef    APP_UDP_SERVER_H
    #if (WITH_UDP_SERVER == SO_NONBLOCK)
    check_udp_server_messages();
    #endif /* (WITH_UDP_SERVER == SO_NONBLOCK) */
    #endif /* APP_UDP_SERVER_H */

  #ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
    if (network[app_parameters.network_index].device_type == SL_WISUN_ROUTER) {
      // 1 Sec join state 5 indicator
      change_leds = connected_delay_sec % 4;
      if (change_leds != previous_change_leds) {
          if (change_leds == 0) set_leds(0, 1);
          if (change_leds == 1) set_leds(1, 1);
          if (change_leds == 2) set_leds(1, 0);
          if (change_leds == 3) set_leds(0, 0);
          previous_change_leds = change_leds;
      }
    }
  #endif /* SL_CATALOG_SIMPLE_LED_PRESENT */

  } else {

#ifdef    WITH_DIRECT_CONNECT
  // Direct Connect connection is possible even when not connected to the Wi-SUN network
  #ifdef    APP_UDP_SERVER_H
  #if (WITH_UDP_SERVER == SO_NONBLOCK)
  check_udp_server_messages();
  #endif /* (WITH_UDP_SERVER == SO_NONBLOCK) */
  #endif /* APP_UDP_SERVER_H */
#endif /* WITH_DIRECT_CONNECT */

    to_udp = to_coap = false;
  #ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
    if (network[app_parameters.network_index].device_type == SL_WISUN_ROUTER) {
      // 1 Sec non join state 5 indicator
      change_leds = connected_delay_sec % 4;
      if (change_leds != previous_change_leds) {
          if (change_leds == 0         ) leds_f_join_state(join_state);
          if (change_leds == join_state) set_leds(0, 0);
          previous_change_leds = change_leds;
      }
    }
  #endif /* SL_CATALOG_SIMPLE_LED_PRESENT */

  }

  if (connected_delay_sec >= next_status_sec || send_asap) {
    time_to_send_status = true;
    next_status_sec = connected_delay_sec + network[app_parameters.network_index].auto_send_sec;
  } else {
    time_to_send_status = false;
  }
  send_asap = false;

  // Print status message once then disable status
  if (time_to_send_status == true) {
#ifdef   SL_CATALOG_POWER_MANAGER_PRESENT
    if (to_console) {
      print_power_manager_delays();
    }
#endif /* SL_CATALOG_POWER_MANAGER_PRESENT */

    print_and_send_messages (_status_json_string(""), with_time, to_console, to_rtt, to_udp, to_coap);
    #ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
    if (network[app_parameters.network_index].device_type == SL_WISUN_ROUTER) {
      sl_led_toggle(&sl_led_led0);
      sl_led_toggle(&sl_led_led1);
    }
    #endif /* SL_CATALOG_SIMPLE_LED_PRESENT */
  }

#ifdef    APP_TRACK_HEAP
  // Refresh heap info once then disable the refresh
  if ((refresh_heap) && (connected_delay_sec % APP_TRACK_HEAP_PERIOD_S != 1)) {
    sl_memory_get_heap_info(&app_heap_info);
  #ifdef    APP_TRACK_HEAP_DIFF
    if (app_previous_heap_free == 0) { app_previous_heap_free = app_heap_info.free_size; }
    if (app_previous_heap_free != app_heap_info.free_size) {
      printfBothTime("heap free %8d used %8d %6.2f%% (free diff %5d)\n",
                  app_heap_info.free_size,
                  app_heap_info.used_size,
                  1.0*app_heap_info.used_size / (app_heap_info.total_size / 100),
                  app_heap_info.free_size - app_previous_heap_free
      );
      refresh_heap = false;
    }
    app_previous_heap_free = app_heap_info.free_size;
  #endif /* APP_TRACK_HEAP_DIFF */
    refresh_heap = false;
  }
  // Enable heap info refresh for next time
  if (connected_delay_sec % APP_TRACK_HEAP_PERIOD_S == 1) { refresh_heap = true; }
#endif /* APP_TRACK_HEAP */

#ifdef    SL_CATALOG_SIMPLE_BUTTON_PRESENT
  if (connected_delay_sec % BUTTON_CHECK_DELAY == 0) {
    if (check_buttons == true) {
      check_buttons = false;
      B0 = ( sl_button_get_state(&sl_button_btn0) == SL_SIMPLE_BUTTON_PRESSED );
      B1 = ( sl_button_get_state(&sl_button_btn1) == SL_SIMPLE_BUTTON_PRESSED );
      #ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
        if (network[app_parameters.network_index].device_type == SL_WISUN_ROUTER) {
          if (B0) sl_led_turn_on(&sl_led_led0);
          if (B1) sl_led_turn_on(&sl_led_led1);
        }
      #endif /* SL_CATALOG_SIMPLE_BUTTON_PRESENT */
      if (B0 + B1) {
        print_and_send_messages (_button_json_string(""),
                  with_time, to_console, to_rtt, to_udp, to_coap);
      #ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
        if (network[app_parameters.network_index].device_type == SL_WISUN_ROUTER) {
          sl_led_turn_off(&sl_led_led0);
          sl_led_turn_off(&sl_led_led1);
        }
      #endif /* SL_CATALOG_SIMPLE_BUTTON_PRESENT */
      }
    }
  }
  if (connected_delay_sec % BUTTON_CHECK_DELAY == 1) {
    check_buttons = true;
  }
#endif /* SL_CATALOG_SIMPLE_BUTTON_PRESENT */

}

void app_reset_statistics(void) {
  connection_time_sec = now_sec();
  disconnection_time_sec = connection_time_sec;
  connection_count = 0;
  connected_total_sec = 0;
  disconnected_total_sec = 0;
}

sl_wisun_mac_address_t _get_parent_mac_address_and_update_parent_info(void) {
  sl_status_t ret;
  uint8_t neighbor_count;
  uint8_t i;
  sl_wisun_neighbor_info_t neighbor_info;

  // fill with zeros
  for ( i = 0 ; i<  SL_WISUN_MAC_ADDRESS_SIZE ; i++) {
      parent_mac.address[i] = 0;
      secondary_mac.address[i] = 0;
  }

  ret = sl_wisun_get_neighbor_count(&neighbor_count);
  if (ret) {
    printfBothTime("[Failed: sl_wisun_get_neighbor_count() returned 0x%04x]\n", (uint16_t)ret);
    neighbor_count = 0;
  }

  if (neighbor_count) {
    sl_wisun_mac_address_t *neighbor_mac_addresses = NULL;
    neighbor_mac_addresses = sl_malloc(sizeof(sl_wisun_mac_address_t) * neighbor_count);
    if (neighbor_mac_addresses == NULL) {
      printfBothTime("[Failed: memory allocation for %d sl_wisun_mac_address_t in neighbor_mac_addresses returned NULL]\n", (uint16_t)neighbor_count);
    } else {
      ret = sl_wisun_get_neighbors(&neighbor_count, neighbor_mac_addresses);
      if (ret) {printfBothTime("[Failed: sl_wisun_get_neighbors() returned 0x%04x]\n", (uint16_t)ret);}
      for (i = 0 ; i < neighbor_count; i++) {
          sl_wisun_get_neighbor_info(&neighbor_mac_addresses[i], &neighbor_info);
          if (neighbor_info.type == SL_WISUN_NEIGHBOR_TYPE_PRIMARY_PARENT  ) {
            sl_wisun_get_neighbor_info(&neighbor_mac_addresses[i], &parent_info);
            parent_mac = neighbor_mac_addresses[i];
          }
          if (neighbor_info.type == SL_WISUN_NEIGHBOR_TYPE_SECONDARY_PARENT) {
            sl_wisun_get_neighbor_info(&neighbor_mac_addresses[i], &secondary_info);
            secondary_mac = neighbor_mac_addresses[i];
          }
      }
      sl_free(neighbor_mac_addresses);
    }
  }

  return parent_mac;
}

void refresh_parent_tag(void) {
  _get_parent_mac_address_and_update_parent_info();
  sprintf(parent_tag   , "%02x%02x", parent_mac.address[6]   , parent_mac.address[7]   );
  sprintf(secondary_tag, "%02x%02x", secondary_mac.address[6], secondary_mac.address[7]);
};

void  _join_state_custom_callback(sl_wisun_evt_t *evt) {
  uint64_t delay;
  uint64_t now = now_sec();

  join_state = (sl_wisun_join_state_t)evt->evt.join_state.join_state;
  if (join_state >  SL_WISUN_JOIN_STATE_OPERATIONAL) {
    if ((join_state >= SL_WISUN_JOIN_STATE_PARENT_SELECT) && (join_state <= SL_WISUN_JOIN_STATE_DAO)) {
      // Join state 4 sub-state (4.1 = parent select, 4.2 = DHCP,
      // 4.3 = address registration, 4.4 = DAO registration).
      printfBothTime("[Join state %d.%d]\n", join_state/10, join_state%10);
      if (join_state != previous_join_state_4x) {
        // Accumulate the time spent in the previous sub-state (if any), so
        // app_join_state_4x_delay_sec[X] holds the total time spent in
        // sub-state 4.X across all visits (not reset on fall back).
        if (previous_join_state_4x != 0) {
          delay = now - app_join_state_4x_sec[previous_join_state_4x - 40];
          app_join_state_4x_delay_sec[previous_join_state_4x - 40] += delay;
          printfBothTime("app_join_state_4x_delay_sec[%u] += %llu sec (total %llu sec)\n",
                         (unsigned)(previous_join_state_4x - 40), delay,
                         app_join_state_4x_delay_sec[previous_join_state_4x - 40]);
        }
        // Mark the latest entry time of the new sub-state and count the visit.
        app_join_state_4x_sec[join_state - 40] = now;
        app_join_state_4x_count[join_state - 40]++;
        previous_join_state_4x = (uint16_t)join_state;
      }
    } else {
      // E.g. SL_WISUN_JOIN_STATE_DISCONNECTING (6).
      printfBothTime("[Join state %d]\n", join_state);
    }
    // Do not process intermediate join states, whose values are > 5
    return;
  }
  if (join_state != previous_join_state) {
    // join_state changed...
    // print current join_state
    printfBothTime("[Join state %u->%u]\n", previous_join_state, join_state);
    if (join_state < min_join_state) { min_join_state = join_state; }

    // If we were tracking a join state 4 sub-state, close it out now so its
    // remaining time is added to its accumulator before leaving sub-state 4.
    if (previous_join_state_4x != 0) {
      delay = now - app_join_state_4x_sec[previous_join_state_4x - 40];
      app_join_state_4x_delay_sec[previous_join_state_4x - 40] += delay;
      printfBothTime("app_join_state_4x_delay_sec[%u] += %llu sec (total %llu sec)\n",
                     (unsigned)(previous_join_state_4x - 40), delay,
                     app_join_state_4x_delay_sec[previous_join_state_4x - 40]);
      previous_join_state_4x = 0;
    }

    // Accumulate the time spent in the previous join state, so that
    // app_join_state_delay_sec[X] holds the *total* time spent in state X
    // across all visits (not reset when falling back to a previous state).
    if ((previous_join_state > SL_WISUN_JOIN_STATE_DISCONNECTED) && (previous_join_state <= SL_WISUN_JOIN_STATE_OPERATIONAL)) {
      delay = now - app_join_state_sec[previous_join_state];
      app_join_state_delay_sec[previous_join_state] += delay;
      printfBothTime("app_join_state_delay_sec[%u] += %llu sec (total %llu sec)\n",
                     (unsigned)previous_join_state, delay, app_join_state_delay_sec[previous_join_state]);
    }

    if ((join_state > SL_WISUN_JOIN_STATE_DISCONNECTED) && (join_state <= SL_WISUN_JOIN_STATE_OPERATIONAL)) {
      // Mark the latest entry time of the new join state (so the next
      // transition can compute how long we stayed in it) and count the visit.
      app_join_state_sec[join_state] = now;
      app_join_state_count[join_state]++;
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
        to_udp = true;
        to_coap = false;
        print_and_send_messages (_connection_json_string(""),
            with_time, to_console, to_rtt, to_udp, to_coap);
      }
      TRACES_WHEN_CONNECTED;
    }
    // Prepare counting disconnected time
    if (previous_join_state == SL_WISUN_JOIN_STATE_OPERATIONAL) {
      // Time spent in the OPERATIONAL state has already been accumulated
      // into app_join_state_delay_sec[OPERATIONAL] above, so per-state
      // counters do not need to be reset here on disconnection.
      disconnection_time_sec = now;
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


#ifdef    APP_CHECK_NEIGHBORS_H
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
#endif /* APP_CHECK_NEIGHBORS_H */

char* _connection_json_string () {
  #define CONNECTION_JSON_FORMAT_STR                    \
    START_JSON                                          \
    DEVICE_CHIP_JSON_FORMAT                             \
    PARENT_JSON_FORMAT                                  \
    RUNNING_JSON_FORMAT                                 \
    MSG_COUNT_JSON_FORMAT                               \
    "\"PAN_ID\":\"0x%04x (%u)\",\n"                     \
    "\"preferred_pan_id\":\"0x%04x (%u)\",\n"           \
    "\"hop_count\":\"%u\",\n"                           \
    "\"join_states_sec\": \"%lu %lu %lu %lu %lu\",\n"\
    "\"join_states_4x_sec\": \"%lu %lu %lu %lu\",\n"\
    "\"join_states_count\": \"%u %u %u %u %u\",\n"        \
    "\"join_states_4x_count\": \"%u %u %u %u\",\n"        \
    "\"application\": \"%s\"\n"                           \
    END_JSON

  char sec_string[20];
  uint64_t connection_sec;

  sl_wisun_get_network_info(&network_info);
  connection_sec = now_sec();
  sprintf(sec_string, "%s", dhms(connection_sec));
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
    (uint32_t)app_join_state_delay_sec[1],
    (uint32_t)app_join_state_delay_sec[2],
    (uint32_t)app_join_state_delay_sec[3],
    (uint32_t)app_join_state_delay_sec[4],
    (uint32_t)app_join_state_delay_sec[5],
    (uint32_t)app_join_state_4x_delay_sec[1],
    (uint32_t)app_join_state_4x_delay_sec[2],
    (uint32_t)app_join_state_4x_delay_sec[3],
    (uint32_t)app_join_state_4x_delay_sec[4],
    app_join_state_count[1],
    app_join_state_count[2],
    app_join_state_count[3],
    app_join_state_count[4],
    app_join_state_count[5],
    app_join_state_4x_count[1],
    app_join_state_4x_count[2],
    app_join_state_4x_count[3],
    app_join_state_4x_count[4],
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

  char running_sec_string[21];
  char connected_string[21];
  char disconnected_string[21];
  char connected_sec_string[21];
  char disconnected_sec_string[21];
  float availability;
  // sl_wisun_statistics_t is a union, so we need one per statistics type
  sl_wisun_statistics_t         network_statistics;
  sl_wisun_statistics_t         mac_statistics;
  sl_wisun_statistics_t         phy_statistics;

  uint64_t status_sec;
  uint64_t current_state_sec;

  // Make sure of the join state
  sl_wisun_get_join_state(&join_state);
  msg_count++;

  status_sec = now;

  if (join_state == SL_WISUN_JOIN_STATE_OPERATIONAL) {
    current_state_sec = status_sec - connection_time_sec;
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
    current_state_sec = status_sec - disconnection_time_sec;
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
  // Refresh network info
  sl_wisun_get_network_info(&network_info);
  // Refresh statistics used in status
  sl_wisun_get_statistics (SL_WISUN_STATISTICS_TYPE_NETWORK, &network_statistics);
  sl_wisun_get_statistics (SL_WISUN_STATISTICS_TYPE_MAC    , &mac_statistics);
  sl_wisun_get_statistics (SL_WISUN_STATISTICS_TYPE_PHY    , &phy_statistics);

  refresh_parent_tag();

  sprintf(running_sec_string, "%s", dhms(status_sec));

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

#ifdef APP_ACTION_SCHEDULER_H
static void event_ext_init(void)
{
  sl_status_t status;

  status = sl_event_queue_create(EVENT_EXT_QUEUE_SIZE,
                                 &event_ext_queue);
  if (status != SL_STATUS_OK) {
    printfBothTime("Failed to create event_ext queue: 0x%04x\n",
                   (uint16_t)status);
    return;
  }

  status = sl_event_subscribe(SL_EVENT_CLASS_WISUN,
                              SL_WISUN_EVENT_IND_MASK,
                              event_ext_queue);
  if (status != SL_STATUS_OK) {
    printfBothTime("Failed to subscribe event_ext queue: 0x%04x\n",
                   (uint16_t)status);
    (void)sl_event_queue_delete(event_ext_queue);
    event_ext_queue = NULL;
    return;
  }

  event_ext_task_id = osThreadNew(event_ext_task,
                                  NULL,
                                  &event_ext_task_attr);
  if (event_ext_task_id == NULL) {
    printfBothTime("Failed to create event_ext task\n");
    (void)sl_event_queue_delete(event_ext_queue);
    event_ext_queue = NULL;
  }
}

static void event_ext_task(void *argument)
{
  uint8_t event_prio = 0U;
  sl_event_t *event = NULL;
  sl_wisun_evt_t *wisun_evt;

  (void)argument;

  for (;;) {
    if (sl_event_queue_get(event_ext_queue,
                           &event_prio,
                           osWaitForever,
                           &event) != SL_STATUS_OK) {
      continue;
    }

    wisun_evt = (sl_wisun_evt_t *)event->event_data;
    if (wisun_evt != NULL) {
      event_ext_handle_wisun_ind(wisun_evt);
    }

    // This queue shares stack event objects; always release after inspection.
    (void)sl_event_process(&event);
  }
}

static void event_ext_handle_wisun_ind(sl_wisun_evt_t *wisun_evt)
{
  switch (wisun_evt->header.id) {
    case SL_WISUN_MSG_FB_READY_IND_ID:
      _first_breath_ready_custom_callback(wisun_evt);
      break;
    case SL_WISUN_MSG_RX_FRAME_IND_ID:
    case SL_WISUN_MSG_LOGGER_EVENT_IND_ID:
    case SL_WISUN_MSG_DIRECT_CONNECT_ID_SOLICIT_IND_ID:
    case SL_WISUN_MSG_DIRECT_CONNECT_ID_RECEIVED_IND_ID:
    case SL_WISUN_MSG_DIRECT_CONNECT_CLIENT_STATE_CHANGED_IND_ID:
      printfBothTime("event_ext: Wi-SUN ind 0x%02x not handled by SDK event manager\n",
                     (unsigned int)wisun_evt->header.id);
      break;
    default:
      break;
  }
}

static uint32_t _first_breath_close_action(void *context)
{
  (void)context;

  if (first_breath_socket_id == SOCKET_INVALID_ID) {
    return 0U;
  }

  printfBothTime("Closing First Breath socket (id %d)\n",
                 (int)first_breath_socket_id);
  if (close(first_breath_socket_id) == SOCKET_RETVAL_ERROR) {
    first_breath_socket_id = SOCKET_INVALID_ID;
    return (uint32_t)SL_STATUS_FAIL;
  }

  first_breath_socket_id = SOCKET_INVALID_ID;
  return 0U;
}

static uint32_t _first_breath_send_action(void *context)
{
  bool enabled;
  uint8_t network_index;
  char dest[IPV6_STR_LEN];
  char running_sec_string[21];
  int tclass;
  int msg_len;
  int socket_ret;

  (void)context;

  app_parameter_mutex_acquire();
  network_index = app_parameters.network_index;
  enabled = network[network_index].first_breath;
  snprintf(dest, sizeof(dest), "%s", network[network_index].udp_notification_dest);
  app_parameter_mutex_release();

  if (!enabled) {
    return 0U;
  }

  if (first_breath_socket_id != SOCKET_INVALID_ID) {
    (void)app_scheduler_action_stop(_first_breath_close_action);
    (void)_first_breath_close_action(NULL);
  }

  memset(&first_breath_sockaddr_in6, 0, sizeof(first_breath_sockaddr_in6));
  if (!sl_wisun_stoip6(dest,
                       strlen(dest),
                       first_breath_sockaddr_in6.sin6_addr.address)) {
    printfBothTime("First Breath invalid UDP destination: %s\n", dest);
    return (uint32_t)SL_STATUS_INVALID_PARAMETER;
  }

  sl_wisun_ip6tos(first_breath_sockaddr_in6.sin6_addr.address,
                  first_breath_ipv6_string);
  first_breath_sockaddr_in6.sin6_family = AF_INET6;
  first_breath_sockaddr_in6.sin6_port = htons(FIRST_BREATH_NOTIFICATION_PORT);

  first_breath_socket_id = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  if (first_breath_socket_id == SOCKET_INVALID_ID) {
    printfBothTime("Failed to open First Breath socket\n");
    return (uint32_t)SL_STATUS_FAIL;
  }

  tclass = IPV6_TCLASS_FROM_DSCP(DSCP_EF);
  socket_ret = setsockopt(first_breath_socket_id,
                          IPPROTO_IPV6,
                          IPV6_TCLASS,
                          &tclass,
                          (socklen_t)sizeof(tclass));
  if (socket_ret == SOCKET_RETVAL_ERROR) {
    printfBothTime("Failed to set First Breath socket DSCP_EF\n");
    (void)_first_breath_close_action(NULL);
    return (uint32_t)SL_STATUS_FAIL;
  }

  snprintf(running_sec_string, sizeof(running_sec_string), "%s", dhms(now_sec()));
  msg_len = snprintf(first_breath_msg,
                     sizeof(first_breath_msg),
                     "{\n"
                     "\"first_breath\":\"true\",\n"
                     "\"running\":\"%s\",\n"
                     "\"device\":\"%s\",\n"
                     "\"network\":\"%d\"\n"
                     "}\n",
                     running_sec_string,
                     device_tag,
                     network_index);
  if ((msg_len < 0) || (msg_len >= (int)sizeof(first_breath_msg))) {
    (void)_first_breath_close_action(NULL);
    return (uint32_t)SL_STATUS_WOULD_OVERFLOW;
  }

  if (sendto(first_breath_socket_id,
             (uint8_t *)first_breath_msg,
             (size_t)msg_len,
             0,
             (const struct sockaddr *)&first_breath_sockaddr_in6,
             sizeof(first_breath_sockaddr_in6)) == SOCKET_RETVAL_ERROR) {
    printfBothTime("Failed to send First Breath UDP to %s/%d\n",
                   first_breath_ipv6_string,
                   FIRST_BREATH_NOTIFICATION_PORT);
    (void)_first_breath_close_action(NULL);
    return (uint32_t)SL_STATUS_TRANSMIT;
  }

  printfBothTime("First Breath UDP sent to %s/%d (%d bytes)\n",
                 first_breath_ipv6_string,
                 FIRST_BREATH_NOTIFICATION_PORT,
                 msg_len);

  if (!app_scheduler_action_schedule(_first_breath_close_action,
                                     FIRST_BREATH_SOCKET_CLOSE_DELAY_MS,
                                     0U,
                                     NULL)) {
    printfBothTime("Failed to schedule First Breath socket close\n");
    return (uint32_t)SL_STATUS_FAIL;
  }

  return 0U;
}
#endif /* APP_ACTION_SCHEDULER_H */

void _first_breath_ready_custom_callback(sl_wisun_evt_t *evt)
{
  sl_status_t status = (sl_status_t)evt->evt.fb_ready.status;

  if (status != SL_STATUS_OK) {
    printfBothTime("First Breath ready event status 0x%04x\n", (uint16_t)status);
    return;
  }

#ifdef APP_ACTION_SCHEDULER_H
  if (!app_scheduler_action_schedule(_first_breath_send_action, 0U, 0U, NULL)) {
    printfBothTime("Failed to schedule First Breath UDP send\n");
  }
#else
  printfBothTime("First Breath ready, but action scheduler is not present\n");
#endif /* APP_ACTION_SCHEDULER_H */
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
  udp_notification_sockaddr_in6.sin6_port = htons(UDP_NOTIFICATION_PORT);

#ifdef    SL_CATALOG_WISUN_COAP_PRESENT
  // (UDP) CoAP Notifications (autonomously sent by the device)
  coap_notification_socket_id = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  ret =(coap_notification_socket_id == SOCKET_INVALID_ID) ? 1 : 0;
  NO_ERROR(ret, "Opened    the COAP notification socket (id %d)\n", (int)coap_notification_socket_id);
  IF_ERROR_RETURN(ret, "[Failed: unable to open the COAP notification socket]\n");

  coap_notification_sockaddr_in6.sin6_family = AF_INET6;
  coap_notification_sockaddr_in6.sin6_port = htons(COAP_NOTIFICATION_PORT);

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
#endif /* SL_CATALOG_WISUN_COAP_PRESENT */

  return SL_STATUS_OK;

};

#ifdef    SL_WISUN_COAP_H
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
          printfBothTime("_coap_notify() error on line %d\n", __LINE__);
          ret = SL_STATUS_TRANSMIT;
      }
  }
  sl_free(buff);
  return ret;
}
#endif /* SL_WISUN_COAP_H */

uint8_t print_and_send_messages (char *in_msg, bool _with_time,
                            bool _to_console, bool _to_rtt, bool _to_udp, bool _to_coap) {
#ifdef    SL_WISUN_COAP_H
  sl_status_t ret = SL_STATUS_OK;
  uint16_t coap_msg_len;
#else
  to_coap = false;
#endif /* SL_WISUN_COAP_H */
  uint8_t messages_processed = 0;
  uint16_t udp_msg_len;

  if (_to_console == true) { // Print to console
      if (_with_time == true) {
        printfTime("%s", in_msg);
      } else {
        printf("%s", in_msg);
      }
    messages_processed++;
  }
#ifdef    SEGGER_RTT_printf
  if (_to_rtt == true) {     // Print to RTT traces
      if (_with_time == true) {
        printfTimeRTT("%s", in_msg);
      } else {
        printfRTT("%s", in_msg);
      }
    messages_processed++;
  }
#else /* SEGGER_RTT_printf */
  (void) _to_rtt;
#endif /* SEGGER_RTT_printf */
  if (_to_udp == true) {     // Send to UDP port
    udp_msg_len  = snprintf(udp_msg,  1024, "%s", in_msg);
    if (sendto(udp_notification_socket_id,
                (uint8_t *)udp_msg,
                udp_msg_len,
                0L,
                (const struct sockaddr *) &udp_notification_sockaddr_in6,
                sizeof(sockaddr_in6_t)) == -1) {
      printfBothTime("\n[Failed (%s line %d): unable to send to the UDP notification socket (%d %s/%d)] udp_msg_len %d\n", __FILE__, __LINE__,
              (int)udp_notification_socket_id, udp_notification_ipv6_string , UDP_NOTIFICATION_PORT, udp_msg_len);
    } else {
      messages_processed++;
    }
  }
#ifdef    SL_WISUN_COAP_H
  if (_to_coap == true) {    // Send to CoAP notification port
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
#endif /* SL_WISUN_COAP_H */

  return messages_processed;
}
