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
// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define CLEAR_NVM_NO   0
#define CLEAR_NVM_APP  1
#define CLEAR_NVM_FULL 2

typedef enum {
  APP_SCHEDULER_NONE = 0,
  APP_SCHEDULER_REBOOT,
  APP_SCHEDULER_CLEAR_CRED_AND_REBOOT,
  APP_SCHEDULER_RECONNECT,
  APP_SCHEDULER_CLEAR_AND_RECONNECT,
  APP_SCHEDULER_OTA_REBOOT_INSTALL,
} app_scheduler_action_type_t;

// Small state struct, if you ever want to expose more info.
typedef struct {
  bool                       active;
  app_scheduler_action_type_t action;
  uint32_t                   delay_ms;
  uint64_t                   start_ms;
  uint64_t                   deadline_ms;
  uint8_t                    clear_nvm_mode;   // reuse CLEAR_NVM_* from multicast OTA
} app_scheduler_action_state_t;

void app_scheduler_action_init(void);

/**
 * Schedule a scheduler action.
 *
 * @param action       What to do when the delay expires.
 * @param delay_ms     Delay in milliseconds.
 * @param clear_nvm    CLEAR_NVM_NO / APP / FULL when relevant, 0 otherwise.
 * @return true on success (timer started), false on error.
 */
bool app_scheduler_action_schedule(app_scheduler_action_type_t action,
                                  uint32_t delay_ms,
                                  uint8_t clear_nvm);

/**
 * Get remaining time before the scheduled action.
 *
 * @param remaining_ms [out] remaining time, 0 if already due.
 * @param action       [out] current action type.
 * @return true if an action is scheduled, false otherwise.
 */
bool app_scheduler_action_get_remaining(uint32_t *remaining_ms,
                                       app_scheduler_action_type_t *action);


