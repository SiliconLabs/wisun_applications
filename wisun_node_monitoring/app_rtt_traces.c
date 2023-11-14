/***************************************************************************//**
* @file app_rtt_traces.c
* @brief Resources to easily control RTT traces
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
#include "app_rtt_traces.h"
#include "sl_malloc.h"

sl_status_t app_set_all_traces(uint8_t trace_level, bool verbose) {
  sl_status_t ret;
  sl_wisun_trace_group_config_t *trace_config;
  uint8_t group_count;
  uint8_t i;
  trace_config = sl_malloc(SL_WISUN_TRACE_GROUP_COUNT * sizeof(sl_wisun_trace_group_config_t));
  group_count = SL_WISUN_TRACE_GROUP_RF+1;

  for (i = 0; i < group_count; i++) {
      trace_config[i].group_id = i;
      trace_config[i].trace_level = trace_level;
  }
  ret = sl_wisun_set_trace_level(group_count, trace_config);
  if (verbose) printf("\nSet all %d trace groups to level %d: %lu\n", group_count, trace_level, ret);
  sl_free(trace_config);
  return ret;
}

sl_status_t app_set_trace(uint8_t group_id, uint8_t trace_level, bool verbose)
{
  sl_status_t ret;
  sl_wisun_trace_group_config_t trace_config;

  trace_config.group_id = group_id;
  trace_config.trace_level = trace_level;
  ret = sl_wisun_set_trace_level(1, &trace_config);
  if (verbose) printf("Set trace group %u to level %u: %lu\n", group_id, trace_level, ret);
  return ret;
}
