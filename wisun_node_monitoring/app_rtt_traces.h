/***************************************************************************//**
* @file app_init.h
* @brief header file for RTT traces
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

/* 'Application rtt (stack) traces control utility
 * How to use this code
 * inclusion:
#include "app_rtt_traces.h"

Trace groups ids and trace levels are defined in gecko_sdk_x.y.z/protocol/wisun/stack/inc/sl_wisun_types.h
(in sl_wisun_trace_group_t and sl_wisun_trace_level_t, respectively)
The 'verbose' flag controls internal application console printing
Usage: Setting all traces groups to the same trace level
  It is convenient while connecting to set all traces to 'DEBUG',
   to monitor the connection in RTT traces using J-Link RTT Viewer
    app_set_all_traces(SL_WISUN_TRACE_LEVEL_DEBUG, true);

  Once connected, set all traces to 'INFO' or 'WARN', to avoid too many trace,
   then set selected traces to 'DEBUG', depending on what you want to observe
    app_set_all_traces(SL_WISUN_TRACE_LEVEL_WARN, true);
    app_set_trace(SL_WISUN_TRACE_GROUP_SOCK, SL_WISUN_TRACE_LEVEL_DEBUG, true);
*/

#include <stdio.h>
#include "sl_wisun_trace_api.h"

sl_status_t app_set_all_traces(uint8_t trace_level, bool verbose);
sl_status_t app_set_trace     (uint8_t group_id, uint8_t trace_level, bool verbose);
