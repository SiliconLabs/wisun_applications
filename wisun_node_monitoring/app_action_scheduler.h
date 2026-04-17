/***************************************************************************//**
* @file app_action_scheduler.h
* @brief Action scheduler Header file
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

#ifndef APP_ACTION_SCHEDULER_H
#define APP_ACTION_SCHEDULER_H

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define APP_SCHEDULER_MAX_SLOTS 8U

typedef uint32_t (*app_scheduler_action_fn_t)(void *context);

// Small state struct, if you ever want to expose more info.
typedef struct {
  bool                      active;
  bool                      periodic;
  app_scheduler_action_fn_t action_fn;
  uint32_t                  delay_ms;
  uint32_t                  period_ms;
  uint64_t                  start_ms;
  uint64_t                  deadline_ms;
  void                      *context;
} app_scheduler_action_state_t;

void app_scheduler_action_init(void);

/**
 * Schedule a scheduler action.
 *
 * @param action_fn    Callback to execute when the delay expires.
 * @param delay_ms     Delay in milliseconds.
 * @param period_ms    Period for repeated execution, 0 for one-shot scheduling.
 * @param context      Optional user context passed to the action callback.
 * @return true on success (timer started), false on error.
 */
bool app_scheduler_action_schedule(app_scheduler_action_fn_t action_fn,
                                   uint32_t delay_ms,
                                   uint32_t period_ms,
                                   void *context);

/**
 * Stop all scheduled instances of the given callback.
 *
 * @param action_fn Callback to stop.
 * @return true if at least one instance was stopped.
 */
bool app_scheduler_action_stop(app_scheduler_action_fn_t action_fn);

/**
 * Get remaining time for the earliest scheduled instance of a callback.
 *
 * @param action_fn    Callback to query.
 * @param remaining_ms [out] remaining time, 0 if already due.
 * @return true if at least one matching callback is scheduled.
 */
bool app_scheduler_action_get_remaining(app_scheduler_action_fn_t action_fn, 
                                        uint32_t *remaining_ms);


#endif /* APP_ACTION_SCHEDULER_H */
