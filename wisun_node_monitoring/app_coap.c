/***************************************************************************//**
* @file app_coap.c
* @brief CoAP implementation for Wi-SUN device node monitoring
* @version 1.0.0
*******************************************************************************
* It provides access to the following CoAP URIs
*
* "/info/device"                        Tag made of last 2 bytes of device's MAC address
* "/info/chip"                          Text matching the part number
* "/info/board"                         Text matching the board name
* "/info/device_type"                   'FFN' or 'LFN' + details on LFN support (for FFNs) or LFN profile (for LFNs)
* "/info/version"                       Version 'info'
* "/info/application"                   Application 'info'
* "/info/all"                           All 'info'
* "/status/running"                     How much time the application has been running
* "/status/parent"                      Tag made of last 2 bytes of parent's MAC address
* "/status/neighbor"                    Neighbor info (per index required by '-e <payload>')
* "/status/connected"                   How much time the device has been connected for the current connection
* "/status/all"                         All 'status'
* "/status/send"                        Trigger a Tx of _status_json_string (ASAP)
* "/statistics/app/join_state_secs"     How much seconds to jump to each join state
* "/statistics/app/disconnected_total"  How much time the device has been disconnected since the first connection
* "/statistics/app/connections"         How many times the device connected
* "/statistics/app/connected_total"     How much time the device has been connected since the first connection
* "/statistics/app/availability"        connected_total / (connected_total + disconnected_total) ratio
* "/statistics/app/all"                 All 'app' statistics
* "/statistics/stack/phy"               PHY statistics stored in sl_wisun_statistics_phy_t
* "/statistics/stack/mac"               MAC statistics stored in sl_wisun_statistics_mac_t
* "/statistics/stack/fhss"              FHSS statistics stored in sl_wisun_statistics_fhss_t
* "/statistics/stack/wisun"             WISUN statistics stored in sl_wisun_statistics_wisun_t
* "/statistics/stack/network"           NETWORK statistics stored in sl_wisun_statistics_network_t
* "/statistics/stack/regulation"        REGULATION statistics stored in sl_wisun_statistics_regulation_t
* "/settings/auto_send"                 Set auto_Send_sec period to send UDP notifications
* "/settings/trace_level"               Control RTT traces
* "/settings/parameter"                 Control application parameters
* "/reporter/crash"                     Report info on previous crash (if any)
* "/reporter/start"                     Start filtering RTT traces for selected strings and reporting then to REPORTER_PORT
* "/reporter/stop"                      Stop filtering RTT traces
*
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
// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include "sl_string.h"
#include "sl_wisun_api.h"
#include "sl_wisun_types.h"
#include "sl_wisun_config.h"
#include "sl_wisun_trace_util.h"
#include "sl_wisun_app_core.h"
#include "sl_wisun_version.h"
#include "sl_wisun_app_core_util.h"

#include "app.h"
#include "app_parameters.h"
#include "app_coap.h"
#include "app_check_neighbors.h"
#include "app_rtt_traces.h"
#include "app_wisun_multicast_ota.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Variables
//------------------------------------------------------------------------------
char coap_response[COAP_MAX_RESPONSE_LEN];
uint8_t coap_response_current_len = 0;

sl_status_t ret;
sl_wisun_statistics_t statistics;
// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------



void  print_coap_help (char* device_global_ipv6_string, char* border_router_ipv6_string) {
  printf("\n");
  printf("To start a CoAP server on the linux Border Router:\n");
  printf("  coap-server -A %s -p %d -d 10\n", border_router_ipv6_string, 5685);
  printf("CoAP discovery:\n");
  printf("  coap-client -m get -N -B 3 coap://[%s]:5683/.well-known/core\n", device_global_ipv6_string);
  printf("CoAP GET requests:\n");
  printf("  coap-client -m get -N -B 3 coap://[%s]:5683/<resource>, for the following resources:\n", device_global_ipv6_string);
  sl_wisun_coap_rhnd_print_resources();
  printf("  '/settings/auto_send'         returns the current notification duration in seconds\n");
  printf("  '/settings/auto_send' -e <d>' changes the notification duration to d seconds\n");
  printf("  '/settings/parameter' -e \"<name>\"' returns application parameter <name> current value\n");
  printf("  '/settings/parameter' -e \"<name> <value>\"' changes application parameter <name> to <value>\n");
  printf("  '/status/neighbor'            returns the neighbor_count\n");
  printf("  '/status/neighbor -e <n>'     returns the neighbor information for neighbor at index n\n");
  printf("  '/statistics/stack/<group> -e reset' clears the Stack statistics for the selected group\n");
  printf("  '/statistics/app/all       -e reset' clears all statistics\n");
  printf("\n");
}

sl_wisun_coap_packet_t * app_coap_reply(char *response_string,
                  const sl_wisun_coap_packet_t *const req_packet) {

  sl_wisun_coap_packet_t* resp_packet = NULL;
  // Prepare CoAP response packet with default response string
  resp_packet = sl_wisun_coap_build_response(req_packet, COAP_MSG_CODE_RESPONSE_BAD_REQUEST);
  if (resp_packet == NULL) {
    return NULL;
  }

  resp_packet->msg_code       = COAP_MSG_CODE_RESPONSE_CONTENT;
  resp_packet->content_format = COAP_CT_TEXT_PLAIN;
  resp_packet->payload_ptr    = (uint8_t *)response_string;
  resp_packet->payload_len    = (uint16_t)sl_strnlen(response_string, COAP_MAX_RESPONSE_LEN);

  return resp_packet;
}

// CoAP Callback functions definition (one callback function per URI)
sl_wisun_coap_packet_t * coap_callback_all_infos (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  const char *buf;
  #define JSON_ALL_INFOS_FORMAT_STR  \
    "{\n"                          \
    "  \"device\": \"%s\",\n"      \
    "  \"chip\": \"%s\",\n"        \
    "  \"board\": \"%s\",\n"       \
    "  \"device_type\": \"%s\",\n" \
    "  \"application\": \"%s\",\n" \
    "  \"version\": \"%s\",\n"     \
    "  \"stack_version\": \"%d.%d.%d_b%d\",\n"\
    "  \"MAC\": \"%s\"\n"          \
    "}\n"
  sl_wisun_mac_address_t device_mac;
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
  uint16_t build;
  sl_wisun_get_mac_address(&device_mac);
  buf = app_wisun_mac_addr_to_str(&device_mac);
  sl_wisun_get_stack_version(&major, &minor, &patch, &build);
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, JSON_ALL_INFOS_FORMAT_STR,
            device_tag,
            chip,
            SL_BOARD_NAME,
            device_type,
            application,
            version,
            major, minor, patch, build,
            buf
  );
  app_wisun_free((void*)buf);
  return app_coap_reply(coap_response, req_packet);
}

sl_wisun_coap_packet_t * coap_callback_all_statuses (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  #define JSON_ALL_STATUSES_FORMAT_STR      \
    "{\n"                                 \
    "  \"running\": \"%s\",\n"            \
    "  \"connected\": \"%s\",\n"          \
    "  \"parent\": \"%s\",\n"             \
    "  \"neighbor_count\": \"%d\"\n"      \
    "}\n"

  char running_str[40];
  char connected_str[40];
  uint8_t neighbor_count;
  sl_wisun_get_neighbor_count(&neighbor_count);

  snprintf(running_str  , 40, dhms(now_sec()) );
  snprintf(connected_str, 40, dhms(now_sec() - connection_time_sec) );

  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, JSON_ALL_STATUSES_FORMAT_STR,
            running_str,
            connected_str,
            parent_tag,
            neighbor_count
  );
  return app_coap_reply(coap_response, req_packet);
}

sl_wisun_coap_packet_t * coap_callback_send_status_msg (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  send_asap = true;
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "send_asap flag set to true" );
  return app_coap_reply(coap_response, req_packet);
}

sl_wisun_coap_packet_t * coap_callback_crash_report (
    const  sl_wisun_coap_packet_t *const req_packet)  {
  if (strlen(crash_info_string)) {
      snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", crash_info_string);
  } else {
      snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "No previous crash info");
  }
  return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_device (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", device_tag);
  return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_chip (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", chip);
  return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_board (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", SL_BOARD_NAME);
  return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_device_type (
    const  sl_wisun_coap_packet_t *const req_packet)  {
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", device_type);
  return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_application (
      const  sl_wisun_coap_packet_t *const req_packet)  {
#define MAX_APPLI_NAME 40
  int res = 0;
  char cmd[MAX_APPLI_NAME];
  char* payload_str = NULL;

  memset(cmd, 0, MAX_APPLI_NAME);
  if (req_packet->payload_len) {
    // Get payload in string format with last char = '\0'
    payload_str = sl_wisun_coap_get_payload_str(req_packet);
    if (payload_str != NULL){
        res = sscanf((char *)payload_str, "%39s", cmd);
        sl_wisun_coap_free(payload_str);
    }

    if (res) {
      if (strcmp(cmd, "clear_and_reconnect")==0) {
        sl_wisun_disconnect();
        //wait for disconnection complete
        sl_wisun_app_core_wait_state(SL_WISUN_MSG_DISCONNECTED_IND_ID,5000);
        sl_wisun_clear_credential_cache();
        sl_wisun_app_core_util_connect_and_wait();
        return NULL;
      }
      if (strcmp(cmd, "reconnect")==0) {
        sl_wisun_disconnect();
        //wait for disconnection complete
        sl_wisun_app_core_wait_state(SL_WISUN_MSG_DISCONNECTED_IND_ID,5000);
        sl_wisun_app_core_util_connect_and_wait();
        return NULL;
      }
      snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "Unknown '%s' command", cmd);
    }
    else{
      snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "Bad format command '%s", cmd);
    }
  } else {
    snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", application);
  }
  return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_version (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", version);
  return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_running (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", dhms(now_sec()));
  return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_parent (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  refresh_parent_tag();
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", parent_tag);
return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_neighbor (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  int index = 0;
  int res;
  uint8_t neighbor_count;
  if (req_packet->payload_len) {
    res = sscanf((char *)req_packet->payload_ptr, "%d", &index);
    if (res) {
      snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", app_neighbor_info_str(index));
      return app_coap_reply(coap_response, req_packet);
    }
  }
  sl_wisun_get_neighbor_count(&neighbor_count);
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "neighbor_count: %d", neighbor_count);
return app_coap_reply(coap_response, req_packet); }

bool _check_app_statistics_reset  (
                             const  sl_wisun_coap_packet_t *const req_packet) {
  if (req_packet->payload_len) {
    // We need to check using payload_len since it's not followed by a null
    if ( !strncmp( (char*)req_packet->payload_ptr, "reset", req_packet->payload_len) ) {
      app_reset_statistics();
      return true;
    }
  }
  return false;
}

#define   COAP_APP_STATISTICS
#ifdef    COAP_APP_STATISTICS
sl_wisun_coap_packet_t * coap_callback_join_states_sec (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "[%llu,%llu,%llu,%llu,%llu]",
          app_join_state_delay_sec[1],
          app_join_state_delay_sec[2],
          app_join_state_delay_sec[3],
          app_join_state_delay_sec[4],
          app_join_state_delay_sec[5]
  );
  return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_connections (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%d / %d", connection_count, network_connection_count);
  _check_app_statistics_reset(req_packet);
  return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_connected (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", dhms(now_sec() - connection_time_sec));
  _check_app_statistics_reset(req_packet);
  return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_connected_total (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", dhms(connected_total_sec + now_sec() - connection_time_sec));
  _check_app_statistics_reset(req_packet);
  return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_disconnected_total (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", dhms(disconnected_total_sec));
  _check_app_statistics_reset(req_packet);
  return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_availability (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%6.2f", 100.0*(connected_total_sec + now_sec() - connection_time_sec)/(connected_total_sec + now_sec() - connection_time_sec + disconnected_total_sec) );
  _check_app_statistics_reset(req_packet);
  return app_coap_reply(coap_response, req_packet); }

#ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
sl_wisun_coap_packet_t * coap_callback_leds_flash (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  // default: 30 sec at 2 per sec
  #define DEFAULT_COUNT   30
  #define DEFAULT_DELAY  250
  char* payload_str = NULL;
  uint16_t count;
  uint16_t delay_ms;
  int res = 0;
  if (req_packet->payload_len) {
    // Get payload in string format with last char = '\0'
    payload_str = sl_wisun_coap_get_payload_str(req_packet);
    if (payload_str != NULL ){
        res = sscanf((char *)payload_str, "%hd %hd", &count, &delay_ms);
        sl_wisun_coap_free(payload_str);
    }
  }
  if (res != 2) {
      count    = DEFAULT_COUNT;
      delay_ms = DEFAULT_DELAY;
  }
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "flashing leds %d times with %d ms delay", count, delay_ms);
  leds_flash(count, delay_ms);
return app_coap_reply(coap_response, req_packet); }
#endif /* SL_CATALOG_SIMPLE_LED_PRESENT */

#ifdef    HISTORY
sl_wisun_coap_packet_t * coap_callback_history (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", history_string );
  return app_coap_reply(coap_response, req_packet); }
#endif /* HISTORY */
#endif /* COAP_APP_STATISTICS */

sl_wisun_coap_packet_t * coap_callback_all_app_statistics (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  #define JSON_ALL_STATISTICS_FORMAT_STR  \
    "{\n"                                 \
    "  \"join_states_sec\":[%llu,%llu,%llu,%llu,%llu],\n" \
    "  \"connections\": \"%d\",\n"         \
    "  \"network_connections\": \"%d\",\n" \
    "  \"connected_total\": \"%s\",\n"     \
    "  \"disconnected_total\": \"%s\",\n"  \
    "  \"availability\": \"%6.2f\"\n"      \
    "}\n"
  char connected_total_str[40];
  char disconnected_total_str[40];
  float availability;

  snprintf(connected_total_str   , 40, dhms(connected_total_sec + now_sec() - connection_time_sec) );
  snprintf(disconnected_total_str, 40, dhms(disconnected_total_sec) );
  availability = 100.0*(connected_total_sec + now_sec() - connection_time_sec)/
      (connected_total_sec + now_sec() - connection_time_sec + disconnected_total_sec);

  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, JSON_ALL_STATISTICS_FORMAT_STR,
            app_join_state_delay_sec[1],
            app_join_state_delay_sec[2],
            app_join_state_delay_sec[3],
            app_join_state_delay_sec[4],
            app_join_state_delay_sec[5],
            connection_count,
            network_connection_count,
            connected_total_str,
            disconnected_total_str,
            availability
  );
  _check_app_statistics_reset(req_packet);
  return app_coap_reply(coap_response, req_packet);
}

#define   COAP_STACK_STATISTICS
#ifdef    COAP_STACK_STATISTICS
char * phy_statistics_str        (sl_wisun_statistics_t statistics)  {
  #define JSON_PHY_STATISTICS_FORMAT_STR  \
  "{\n"                            \
  "  \"crc_fails\": \"%ld\",\n"    \
  "  \"tx_timeouts\": \"%ld\",\n"  \
  "  \"rx_timeouts\": \"%ld\"\n"   \
  "}\n"

  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, JSON_PHY_STATISTICS_FORMAT_STR,
           statistics.phy.crc_fails,
           statistics.phy.tx_timeouts,
           statistics.phy.rx_timeouts
  );
  return coap_response;
}

char * mac_statistics_str        (sl_wisun_statistics_t statistics)  {
  #define JSON_MAC_STATISTICS_FORMAT_STR    \
    "{\n"                                   \
    "  \"tx_queue_size\": \"%d\",\n"        \
    "  \"tx_queue_peak\": \"%d\",\n"        \
    "  \"rx_count\": \"%lu\",\n"            \
    "  \"tx_count\": \"%lu\",\n"            \
    "  \"bc_rx_count\": \"%lu\",\n"         \
    "  \"bc_tx_count\": \"%lu\",\n"         \
    "  \"tx_bytes\": \"%lu\",\n"            \
    "  \"rx_bytes\": \"%lu\",\n"            \
    "  \"tx_failed_count\": \"%lu\",\n"     \
    "  \"retry_count\": \"%lu\",\n"         \
    "  \"cca_attempts_count\": \"%lu\",\n"  \
    "  \"failed_cca_count\": \"%lu\",\n"    \
    "  \"rx_ms_count\": \"%lu\",\n"         \
    "  \"tx_ms_count\": \"%lu\",\n"         \
    "  \"rx_ms_failed_count\": \"%lu\",\n"  \
    "  \"tx_ms_failed_count\": \"%lu\",\n"  \
    "  \"rx_availability_percentage\": \"%u\"\n"  \
    "}\n"


  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, JSON_MAC_STATISTICS_FORMAT_STR,
           statistics.mac.tx_queue_size,
           statistics.mac.tx_queue_peak,
           statistics.mac.rx_count,
           statistics.mac.tx_count,
           statistics.mac.bc_rx_count,
           statistics.mac.bc_tx_count,
           statistics.mac.tx_bytes,
           statistics.mac.rx_bytes,
           statistics.mac.tx_failed_count,
           statistics.mac.retry_count,
           statistics.mac.cca_attempts_count,
           statistics.mac.failed_cca_count,
           statistics.mac.rx_ms_count,
           statistics.mac.tx_ms_count,
           statistics.mac.rx_ms_failed_count,
           statistics.mac.tx_ms_failed_count,
           statistics.mac.rx_availability_percentage
  );
  return coap_response;
}

char * fhss_statistics_str       (sl_wisun_statistics_t statistics)  {
  #define JSON_FHSS_STATISTICS_FORMAT_STR \
    "{\n"                                 \
    "  \"drift_compensation\": \"%d\",\n" \
    "  \"hop_count\": \"%d\",\n"          \
    "  \"synch_interval\": \"%d\",\n"     \
    "  \"prev_avg_synch_fix\": \"%d\",\n" \
    "  \"synch_lost\": \"%lu\",\n"        \
    "  \"unknown_neighbor\": \"%lu\"\n" \
    "}\n"

  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, JSON_FHSS_STATISTICS_FORMAT_STR,
           statistics.fhss.drift_compensation,
           statistics.fhss.hop_count,
           statistics.fhss.synch_interval,
           statistics.fhss.prev_avg_synch_fix,
           statistics.fhss.synch_lost,
           statistics.fhss.unknown_neighbor
  );
  return coap_response;
}

char * wisun_statistics_str      (sl_wisun_statistics_t statistics)  {
  #define JSON_WISUN_STATISTICS_FORMAT_STR   \
    "{\n"                                    \
    "  \"pan_control_tx_count\": \"%lu\",\n" \
    "  \"pan_control_rx_count\": \"%lu\"\n"  \
  "}\n"

  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, JSON_WISUN_STATISTICS_FORMAT_STR,
           statistics.wisun.pan_control_tx_count,
           statistics.wisun.pan_control_rx_count
  );
  return coap_response;
}

char * network_statistics_str    (sl_wisun_statistics_t statistics)  {
  #define JSON_NETWORK_STATISTICS_FORMAT_STR \
    "{\n"                                     \
    "  \"ip_rx_count\": \"%lu\",\n"           \
    "  \"ip_tx_count\": \"%lu\",\n"           \
    "  \"ip_rx_drop\": \"%lu\",\n"            \
    "  \"ip_cksum_error\": \"%lu\",\n"        \
    "  \"ip_tx_bytes\": \"%lu\",\n"           \
    "  \"ip_rx_bytes\": \"%lu\",\n"           \
    "  \"ip_routed_up\": \"%lu\",\n"          \
    "  \"ip_no_route\": \"%lu\",\n"           \
    "  \"frag_rx_errors\": \"%lu\",\n"        \
    "  \"frag_tx_errors\": \"%lu\",\n"        \
    "  \"rpl_route_routecost_better_change\": \"%lu\",\n" \
    "  \"ip_routeloop_detect\": \"%lu\",\n"   \
    "  \"rpl_memory_overflow\": \"%lu\",\n"   \
    "  \"rpl_parent_tx_fail\": \"%lu\",\n"    \
    "  \"rpl_unknown_instance\": \"%lu\",\n"  \
    "  \"rpl_local_repair\": \"%lu\",\n"      \
    "  \"rpl_global_repair\": \"%lu\",\n"     \
    "  \"rpl_malformed_message\": \"%lu\",\n" \
    "  \"rpl_time_no_next_hop\": \"%lu\",\n"  \
    "  \"rpl_total_memory\": \"%lu\",\n"      \
    "  \"buf_alloc\": \"%lu\",\n"             \
    "  \"buf_headroom_realloc\": \"%lu\",\n"  \
    "  \"buf_headroom_shuffle\": \"%lu\",\n"  \
    "  \"buf_headroom_fail\": \"%lu\",\n"     \
    "  \"etx_1st_parent\": \"%d\",\n"         \
    "  \"etx_2nd_parent\": \"%d\",\n"         \
    "  \"adapt_layer_tx_queue_size\": \"%d\",\n" \
    "  \"adapt_layer_tx_queue_peak\": \"%d\"\n" \
    "}\n"


  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, JSON_NETWORK_STATISTICS_FORMAT_STR,
           statistics.network.ip_rx_count,
           statistics.network.ip_tx_count,
           statistics.network.ip_rx_drop,
           statistics.network.ip_cksum_error,
           statistics.network.ip_tx_bytes,
           statistics.network.ip_rx_bytes,
           statistics.network.ip_routed_up,
           statistics.network.ip_no_route,
           statistics.network.frag_rx_errors,
           statistics.network.frag_tx_errors,
           statistics.network.rpl_route_routecost_better_change,
           statistics.network.ip_routeloop_detect,
           statistics.network.rpl_memory_overflow,
           statistics.network.rpl_parent_tx_fail,
           statistics.network.rpl_unknown_instance,
           statistics.network.rpl_local_repair,
           statistics.network.rpl_global_repair,
           statistics.network.rpl_malformed_message,
           statistics.network.rpl_time_no_next_hop,
           statistics.network.rpl_total_memory,
           statistics.network.buf_alloc,
           statistics.network.buf_headroom_realloc,
           statistics.network.buf_headroom_shuffle,
           statistics.network.buf_headroom_fail,
           statistics.network.etx_1st_parent,
           statistics.network.etx_2nd_parent,
           statistics.network.adapt_layer_tx_queue_size,
           statistics.network.adapt_layer_tx_queue_peak  );
  return coap_response;
}

char * regulation_statistics_str (sl_wisun_statistics_t statistics)  {
  #define JSON_REGULATION_STATISTICS_FORMAT_STR \
    "{\n"                       \
    "  \"arib.tx_duration_ms\": \"%lu\"" \
    "}\n"                       \

  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, JSON_REGULATION_STATISTICS_FORMAT_STR,
           statistics.regulation.arib.tx_duration_ms
  );
  return coap_response;
}

bool _check_stack_statistics_reset(sl_wisun_statistics_type_t statistics_type,
                             const  sl_wisun_coap_packet_t *const req_packet) {
  if (req_packet->payload_len) {
    // We need to check using payload_len since it's not followed by a null
    if ( !strncmp( (char*)req_packet->payload_ptr, "reset", req_packet->payload_len) ) {
      sl_wisun_reset_statistics(statistics_type);
      return true;
    }
  }
  return false;
}

sl_wisun_coap_packet_t * coap_callback_phy_statistics (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  ret = sl_wisun_get_statistics (SL_WISUN_STATISTICS_TYPE_PHY, &statistics);
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", phy_statistics_str(statistics) );
  _check_stack_statistics_reset(SL_WISUN_STATISTICS_TYPE_PHY, req_packet);
  return app_coap_reply(coap_response, req_packet);
}

sl_wisun_coap_packet_t * coap_callback_mac_statistics (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  ret = sl_wisun_get_statistics (SL_WISUN_STATISTICS_TYPE_MAC, &statistics);
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", mac_statistics_str(statistics) );
  _check_stack_statistics_reset(SL_WISUN_STATISTICS_TYPE_MAC, req_packet);
  return app_coap_reply(coap_response, req_packet);
}

sl_wisun_coap_packet_t * coap_callback_fhss_statistics (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  ret = sl_wisun_get_statistics (SL_WISUN_STATISTICS_TYPE_FHSS, &statistics);
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", fhss_statistics_str(statistics) );
  _check_stack_statistics_reset(SL_WISUN_STATISTICS_TYPE_FHSS, req_packet);
  return app_coap_reply(coap_response, req_packet);
}

sl_wisun_coap_packet_t * coap_callback_wisun_statistics (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  ret = sl_wisun_get_statistics (SL_WISUN_STATISTICS_TYPE_WISUN, &statistics);
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", wisun_statistics_str(statistics) );
  _check_stack_statistics_reset(SL_WISUN_STATISTICS_TYPE_WISUN, req_packet);
  return app_coap_reply(coap_response, req_packet);
}

sl_wisun_coap_packet_t * coap_callback_network_statistics (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  ret = sl_wisun_get_statistics (SL_WISUN_STATISTICS_TYPE_NETWORK, &statistics);
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", network_statistics_str(statistics) );
  _check_stack_statistics_reset(SL_WISUN_STATISTICS_TYPE_NETWORK, req_packet);
  return app_coap_reply(coap_response, req_packet);
}

sl_wisun_coap_packet_t * coap_callback_regulation_statistics (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  ret = sl_wisun_get_statistics (SL_WISUN_STATISTICS_TYPE_REGULATION, &statistics);
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%s", regulation_statistics_str(statistics) );
  _check_stack_statistics_reset(SL_WISUN_STATISTICS_TYPE_REGULATION, req_packet);
  return app_coap_reply(coap_response, req_packet);
}
#endif /* COAP_STACK_STATISTICS */

sl_wisun_coap_packet_t * coap_callback_auto_send (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  char* payload_str = NULL;
  int sec = 0;
  int res = 0;
  if (req_packet->payload_len) {
    // Get payload in string format with last char = '\0'
    payload_str = sl_wisun_coap_get_payload_str(req_packet);
    if (payload_str != NULL ){
        res = sscanf((char *)payload_str, "%d", &sec);
        sl_wisun_coap_free(payload_str);
    }
    if (res) {
        app_parameters.auto_send_sec = (uint16_t)sec;
    }
  }
  snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%u", app_parameters.auto_send_sec);
return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_trace_level (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  char* payload_str = NULL;
  int level = 0;
  int group = 0;
  int res = 0;

  ret = 1;

  if (req_packet->payload_len) {
    // Get payload in string format with last char = '\0'
    payload_str = sl_wisun_coap_get_payload_str(req_packet);
    if (payload_str != NULL ){
        res = sscanf((char *)payload_str, "%d %d", &group, &level);
    }
    if (res == 2) {
      // Process /settings/trace_level -e "<group> <level>"
      trace_level = (uint8_t)level;
      ret = app_set_trace(group, level, true);
    } else {
      // Process /settings/trace_level -e "<level>" (all groups)
      if (payload_str != NULL ){
        res = sscanf((char *)payload_str, "%d", &level);
      }
      if (res == 1) {
        trace_level = (uint8_t)level;
        ret = app_set_all_traces(level, true);
      }
    }
    sl_wisun_coap_free(payload_str);
  }
  if (ret == SL_STATUS_OK){
      snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%u", trace_level);
  }
  else{
      snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "Error app_set_all_traces 0x%lx", ret);
  }
return app_coap_reply(coap_response, req_packet); }


sl_wisun_coap_packet_t * coap_callback_application_parameter (
      const  sl_wisun_coap_packet_t *const req_packet)  {
  #define MAX_PARAMETER_NAME 40
  char parameter_name[MAX_PARAMETER_NAME];
  char* payload_str = NULL;
  int value = 0;
  int res;
  sl_status_t set_get_res = SL_STATUS_NOT_SUPPORTED;

  // Reset parameter_name tab
  memset(parameter_name, 0, MAX_PARAMETER_NAME);
  if (req_packet->payload_len) {
    // Get payload in string format with last char = '\0'
    payload_str = sl_wisun_coap_get_payload_str(req_packet);
    if (payload_str != NULL ){
      res = sscanf((char *)payload_str, "%39s %d",
                   parameter_name, &value);



      if (res == 2) {
          // Process /settings/parameter -e "<name> <value>"
          set_get_res = set_app_parameter(parameter_name,  value);
      } else {
          res = sscanf((char *)payload_str, "%39s", parameter_name);
        if (res == 1) {
            // Process /settings/parameter -e "<name>"
            set_get_res = get_app_parameter(parameter_name, &value);
        }
      }
      // Free payload_str
      sl_wisun_coap_free(payload_str);
    }
    else{
        set_get_res = SL_STATUS_ALLOCATION_FAILED;
    }
  }
  if (set_get_res == SL_STATUS_OK) {
      snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "%u", value);
  } else {
      snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "Error : 0x%lX", set_get_res);
  }
return app_coap_reply(coap_response, req_packet); }


#ifdef    __APP_REPORTER_H__
sl_wisun_coap_packet_t * coap_callback_reporter_start (
    const  sl_wisun_coap_packet_t *const req_packet)  {
  char* payload_str = NULL;
  if (req_packet->payload_len) {
      // Get payload in string format with last char = '\0'
      payload_str = sl_wisun_coap_get_payload_str(req_packet);
      if (payload_str != NULL ){
          app_start_reporter(UDP_NOTIFICATION_DEST, 1000, (char *)payload_str);
          sl_wisun_coap_free(payload_str);
      }
    } else {
        // if no payload, accept all lines
      app_start_reporter(UDP_NOTIFICATION_DEST, 1000, (char *)"*");
    }
    snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "started");
  return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_reporter_stop (
    const  sl_wisun_coap_packet_t *const req_packet)  {
    app_stop_reporter();
    snprintf(coap_response, COAP_MAX_RESPONSE_LEN, "stopped");
  return app_coap_reply(coap_response, req_packet); }
  #endif /* __APP_REPORTER_H__ */

#ifdef    APP_WISUN_MULTICAST_OTA_H
sl_wisun_coap_packet_t * coap_callback_multicast_ota (
    const  sl_wisun_coap_packet_t *const req_packet)  {
    snprintf(coap_response, COAP_MAX_RESPONSE_LEN, missed_chunks());
  return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_multicast_ota_rx (
    const  sl_wisun_coap_packet_t *const req_packet)  {
    snprintf(coap_response, COAP_MAX_RESPONSE_LEN, rx_chunks());
  return app_coap_reply(coap_response, req_packet); }

sl_wisun_coap_packet_t * coap_callback_multicast_ota_info (
    const  sl_wisun_coap_packet_t *const req_packet)  {
    snprintf(coap_response, COAP_MAX_RESPONSE_LEN, ota_multicast_info());
  return app_coap_reply(coap_response, req_packet); }

#endif /* APP_WISUN_MULTICAST_OTA_H */

// CoAP resources init in resource handler (one block per URI)
uint8_t app_coap_resources_init() {
  sl_wisun_coap_rhnd_resource_t coap_resource = { 0 };
  uint8_t count = 0;

  // Add CoAP resources (one per item)

  coap_resource.data.uri_path = "/info/all";
  coap_resource.data.resource_type = "json";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_all_infos;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/info/device";
  coap_resource.data.resource_type = "tag";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_device;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/info/chip";
  coap_resource.data.resource_type = "tag";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_chip;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/info/board";
  coap_resource.data.resource_type = "txt";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_board;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/info/device_type";
  coap_resource.data.resource_type = "text";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_device_type;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/info/application";
  coap_resource.data.resource_type = "text";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_application;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/info/version";
  coap_resource.data.resource_type = "text";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_version;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/status/all";
  coap_resource.data.resource_type = "json";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_all_statuses;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/status/send";
  coap_resource.data.resource_type = "text";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_send_status_msg;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/status/running";
  coap_resource.data.resource_type = "dhms";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_running;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/status/parent";
  coap_resource.data.resource_type = "tag";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_parent;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/status/neighbor";
  coap_resource.data.resource_type = "json";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_neighbor;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

#ifdef    COAP_APP_STATISTICS
  coap_resource.data.uri_path = "/status/connected";
  coap_resource.data.resource_type = "dhms";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_connected;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/statistics/app/join_states_sec";
  coap_resource.data.resource_type = "array";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_join_states_sec;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/statistics/app/disconnected_total";
  coap_resource.data.resource_type = "dhms";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_disconnected_total;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/statistics/app/connections";
  coap_resource.data.resource_type = "int";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_connections;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/statistics/app/connected_total";
  coap_resource.data.resource_type = "dhms";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_connected_total;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/statistics/app/availability";
  coap_resource.data.resource_type = "ratio";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_availability;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

#ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
  coap_resource.data.uri_path = "/leds/flash";
  coap_resource.data.resource_type = "leds";
  coap_resource.data.interface = "leds";
  coap_resource.auto_response = coap_callback_leds_flash;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;
#endif /* SL_CATALOG_SIMPLE_LED_PRESENT */

#ifdef    HISTORY
  coap_resource.data.uri_path = "/history";
  coap_resource.data.resource_type = "text";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_history;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;
#endif /* HISTORY */

#endif /* COAP_APP_STATISTICS */

  coap_resource.data.uri_path = "/statistics/app/all";
  coap_resource.data.resource_type = "json";
  coap_resource.data.interface = "node";
  coap_resource.auto_response = coap_callback_all_app_statistics;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

#ifdef    COAP_STACK_STATISTICS
  coap_resource.data.uri_path = "/statistics/stack/phy";
  coap_resource.data.resource_type = "json";
  coap_resource.data.interface = "phy";
  coap_resource.auto_response = coap_callback_phy_statistics;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/statistics/stack/mac";
  coap_resource.data.resource_type = "json";
  coap_resource.data.interface = "mac";
  coap_resource.auto_response = coap_callback_mac_statistics;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/statistics/stack/fhss";
  coap_resource.data.resource_type = "json";
  coap_resource.data.interface = "fhss";
  coap_resource.auto_response = coap_callback_fhss_statistics;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/statistics/stack/wisun";
  coap_resource.data.resource_type = "json";
  coap_resource.data.interface = "wisun";
  coap_resource.auto_response = coap_callback_wisun_statistics;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/statistics/stack/network";
  coap_resource.data.resource_type = "json";
  coap_resource.data.interface = "network";
  coap_resource.auto_response = coap_callback_network_statistics;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/statistics/stack/regulation";
  coap_resource.data.resource_type = "json";
  coap_resource.data.interface = "regulation";
  coap_resource.auto_response = coap_callback_regulation_statistics;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;
#endif /* COAP_STACK_STATISTICS */

  coap_resource.data.uri_path = "/settings/auto_send";
  coap_resource.data.resource_type = "sec";
  coap_resource.data.interface = "settings";
  coap_resource.auto_response = coap_callback_auto_send;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/settings/trace_level";
  coap_resource.data.resource_type = "level";
  coap_resource.data.interface = "settings";
  coap_resource.auto_response = coap_callback_trace_level;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/settings/parameter";
  coap_resource.data.resource_type = "int";
  coap_resource.data.interface = "settings";
  coap_resource.auto_response = coap_callback_application_parameter;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

#ifdef    __APP_REPORTER_H__
  coap_resource.data.uri_path = "/reporter/crash";
  coap_resource.data.resource_type = "text";
  coap_resource.data.interface = "reporter";
  coap_resource.auto_response = coap_callback_crash_report;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/reporter/start";
  coap_resource.data.resource_type = "text";
  coap_resource.data.interface = "test";
  coap_resource.auto_response = coap_callback_reporter_start;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/reporter/stop";
  coap_resource.data.resource_type = "text";
  coap_resource.data.interface = "test";
  coap_resource.auto_response = coap_callback_reporter_stop;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;
#endif /* __APP_REPORTER_H__ */


#ifdef    APP_WISUN_MULTICAST_OTA_H
  coap_resource.data.uri_path = "/multicast_ota/missed";
  coap_resource.data.resource_type = "text";
  coap_resource.data.interface = "multicast_ota";
  coap_resource.auto_response = coap_callback_multicast_ota;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/multicast_ota/rx";
  coap_resource.data.resource_type = "text";
  coap_resource.data.interface = "multicast_ota";
  coap_resource.auto_response = coap_callback_multicast_ota_rx;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;

  coap_resource.data.uri_path = "/multicast_ota/info";
  coap_resource.data.resource_type = "text";
  coap_resource.data.interface = "multicast_ota";
  coap_resource.auto_response = coap_callback_multicast_ota_info;
  coap_resource.discoverable = true;
  assert(sl_wisun_coap_rhnd_resource_add(&coap_resource) == SL_STATUS_OK);
  count++;
#endif /* APP_WISUN_MULTICAST_OTA_H */

  printf("  %d/%d CoAP resources added to CoAP Resource handler\n", count, SL_WISUN_COAP_RESOURCE_HND_MAX_RESOURCES);
  return count;
}
