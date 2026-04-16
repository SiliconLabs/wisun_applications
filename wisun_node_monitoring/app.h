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

#include "sl_component_catalog.h"

#if __has_include("app_timestamp.h")
  #include "app_timestamp.h"
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

#ifndef printfBothTime
    #define printfTime     printf
    #define printfBothTime printf
#endif /*printfBothTime*/
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

// UDP and TCP server need to be set to either SO_NONBLOCK or SO_EVENT_MODE, if defined
// Comment the lines if the corresponding server is not required
// NB: Do NOT use SO_NONBLOCK for low-Power use, as it keeps the server loop active!
#define WITH_UDP_SERVER    SO_EVENT_MODE
#define WITH_TCP_SERVER    SO_EVENT_MODE

#ifndef   SL_CATALOG_WISUN_COAP_PRESENT
#define   SL_WISUN_COAP_RESOURCE_HND_SOCK_BUFF_SIZE 1024
#endif /* SL_CATALOG_WISUN_COAP_PRESENT */

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
extern char crash_info_string[];
extern bool send_asap;
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
void app_task(void *args);
void app_do_your_things();
void app_reset_statistics(void);
void refresh_parent_tag(void);

char* status_json_string (char * start_text);

uint8_t print_and_send_messages (char *in_msg, bool with_time, bool to_console, bool to_rtt, bool to_udp, bool to_coap);

#ifdef __cplusplus
}
#endif

#endif  // APP_H
