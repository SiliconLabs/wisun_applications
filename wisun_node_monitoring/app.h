/***************************************************************************//**
* @file app.h
* @brief header file for application
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

#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "sl_component_catalog.h"

#if __has_include("app_timestamp.h")
  #include "app_timestamp.h"
#else
  #define printfRTT printf
  #define printfTimeRTT printf
  #define printfBoth printf
  #define printfBothTime printf
  #define printfTime printf
#endif

#if __has_include("app_parameters.h")
  #include "app_parameters.h"
#endif

#if __has_include("app_rtt_traces.h")
  #include "app_rtt_traces.h"
#endif

#if __has_include("app_check_neighbors.h")
  #include "app_check_neighbors.h"
#endif

#ifdef   SL_CATALOG_WISUN_COAP_PRESENT
  // app_coap.c/h can only be used if the WI-SUN CoAP Component is present
  #if __has_include("app_coap.h")
    #include "app_coap.h"
  #endif
#endif /* SL_CATALOG_WISUN_COAP_PRESENT */

#if __has_include("ltn_config.h")
  #include "ltn_config.h"
#endif

#if __has_include("app_reporter.h")
  #include "app_reporter.h"
#endif

#ifdef   SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT
  #ifdef    SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT
    #if __has_include("app_direct_connect.h")
      #include "app_direct_connect.h"
    #endif
  #else  /* SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT */
    // LFN-only: no direct connect support
  #endif /* SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT */
#else  /* SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT */
  #if __has_include("app_direct_connect.h")
    #include "app_direct_connect.h"
  #endif
#endif /* SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT */

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
#ifndef    SL_BOARD_NAME
  /* For Custom boards:
   *   We recommend setting this to the company-internal board name
   */
  #define  SL_BOARD_NAME "Custom_Board"
#endif /*  SL_BOARD_NAME */

#define DEFINE_string(s)   #s

#define HISTORY

#ifndef   SL_CATALOG_WISUN_COAP_PRESENT
#define   SL_WISUN_COAP_RESOURCE_HND_SOCK_BUFF_SIZE 1024
#endif /* SL_CATALOG_WISUN_COAP_PRESENT */

// -----------------------------------------------------------------------------
//                          Public Function Declarations
// -----------------------------------------------------------------------------
/**************************************************************************//**
 * @brief Application task function
 * @details This function is the main app task implementation
 * @param[in] args arguments
 *****************************************************************************/
#ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
  void leds_flash(uint16_t count, uint16_t delay_ms);
#endif /* SL_CATALOG_SIMPLE_LED_PRESENT */

uint8_t app_join_network(uint8_t network_index);
/** Copy the selected network into the active settings while holding the app parameter mutex. */
void app_update_current_network(void);
void app_get_udp_notification_destination(char *destination, size_t size);
sl_wisun_device_type_t app_get_current_network_device_type(void);
void app_task(void *args);
/** Replace pending status sends with the configured periodic schedule; disabled
 * when auto_send_sec is zero or the node is not operational.
 */
void app_restart_send_status(void);
/** Queue one immediate status send without changing the periodic schedule. */
void app_send_status_asap(void);
void app_get_heap_statistics(size_t *peak_before_join,
                             size_t *peak_after_join,
                             size_t *current_heap,
                             size_t *heap_peak,
                             size_t *heap_total);
#ifdef SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT
sl_status_t app_set_lfn_unicast_interval_temporary(uint32_t interval_ms,
                                                   uint32_t restore_delay_ms,
                                                   bool restore_after_delay);
#endif
#ifdef SL_CATALOG_POWER_MANAGER_PRESENT
size_t app_format_lfn_power_manager_statistics(char *buffer, size_t buffer_size);
#endif
/** Get the timestamp captured by the EMERGENCY propagation callback. */
const char *app_get_emergency_saved_running_time(void);
void app_reset_statistics(void);
void refresh_parent_tag(void);

char* status_json_string (char * start_text);

uint8_t print_and_send_messages (char *in_msg, bool with_time, bool to_console, bool to_rtt, bool to_udp, bool to_coap);

#ifdef __cplusplus
}
#endif

#endif  // APP_H
