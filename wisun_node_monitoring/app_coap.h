/***************************************************************************//**
* @file app_coap.h
* @brief CoAP variables and public prototypes
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
// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include "sl_wisun_coap_rhnd.h"
#include "app_timestamp.h"

extern uint16_t connection_count;           // number of connections
extern uint64_t connect_time_sec;           // time stamp of Wisun connect call
extern uint64_t connection_time_sec;        // last connection time stamp
extern uint64_t disconnection_time_sec;     // last disconnection time stamp
extern uint64_t connected_total_sec;        // total time connected
extern uint64_t disconnected_total_sec;     // total time disconnected
extern uint64_t app_join_state_delay_sec[]; // array of delays to go to join states
extern uint16_t auto_send_sec;              // auto-notification period
extern uint8_t  trace_level;                // Trace level for all trace groups


extern char chip[];
extern char application[];
extern char version[];
extern char device_tag[];
extern char parent_tag[];
extern char history_string[];
extern char device_type[];
extern sl_wisun_mac_address_t parent_mac;

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
#define COAP_MAX_RESPONSE_LEN 1000

uint8_t app_coap_resources_init();
void  print_coap_help (char* device_global_ipv6_string, char* border_router_ipv6_string);
