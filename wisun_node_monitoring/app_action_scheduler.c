/***************************************************************************//**
* @file app_action_scheduler.c
* @brief Action scheduler for a Wi-SUN Node
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


#include "app_action_scheduler.h"

#include "sl_wisun_app_core_util.h"
#include "sl_sleeptimer.h"
#include "cmsis_os2.h"
#include "sl_wisun_api.h"
#include "sl_wisun_app_core.h"
#include "app_parameters.h"
#include "app_wisun_multicast_ota.h"
#include "btl_interface.h"
#include "nvm3.h"
#include "nvm3_default.h"
#include "em_core.h"
#include "printf.h"

// -----------------------------------------------------------------------------
// Local state
// -----------------------------------------------------------------------------

static app_scheduler_action_state_t g_scheduler;

static sl_sleeptimer_timer_handle_t g_scheduler_timer;

static osThreadId_t      g_scheduler_task_id;
static osEventFlagsId_t  g_scheduler_flags;

#define APP_SCHEDULER_FLAG_EXECUTE   (1U << 0)

// Optional: tune stack size/prio as needed.
static uint8_t g_scheduler_task_stack[512];

static const osThreadAttr_t g_scheduler_task_attr = {
  .name       = "scheduler_action",
  .attr_bits   = osThreadDetached,
  .cb_mem      = NULL,
  .cb_size     = 0,
  .stack_mem  = NULL,
  .stack_size = 512,
  .priority   = osPriorityAboveNormal,
  .tz_module   = 0
};

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static uint64_t now_ms(void)
{
  uint64_t ticks = sl_sleeptimer_get_tick_count64();
  uint32_t freq  = sl_sleeptimer_get_timer_frequency();
  if (freq == 0U) {
    return 0U;
  }
  return (ticks * 1000ULL) / (uint64_t)freq;
}

// Forward declaration
static void app_scheduler_action_execute(void);

// Timer callback: runs in ISR context, only signals the task.
static void scheduler_timer_cb(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;

  if (g_scheduler_flags != NULL) {
    (void)osEventFlagsSet(g_scheduler_flags, APP_SCHEDULER_FLAG_EXECUTE);
  }
}

// Worker task: executes the heavy work in thread context.
static void scheduler_task(void *argument)
{
  (void)argument;

  for (;;) {
    uint32_t flags = osEventFlagsWait(g_scheduler_flags,
                                      APP_SCHEDULER_FLAG_EXECUTE,
                                      osFlagsWaitAny,
                                      osWaitForever);
    if ((flags & APP_SCHEDULER_FLAG_EXECUTE) != 0U) {
      app_scheduler_action_execute();
    }
  }
}

// Do the actual action (called from task context).
static void app_scheduler_action_execute(void)
{
  app_scheduler_action_state_t local;

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_CRITICAL();
  local = g_scheduler;      // copy under lock
  g_scheduler.active = false;
  CORE_EXIT_CRITICAL();

  if (!local.active) {
    return;
  }

  switch (local.action) {
  case APP_SCHEDULER_REBOOT:
    // simple reset
    printf("scheduler: NVIC_SystemReset()\n");
    NVIC_SystemReset();
    break;

  case APP_SCHEDULER_CLEAR_CRED_AND_REBOOT:
    printf("scheduler: clear credential cache + reset\n");
    sl_wisun_clear_credential_cache();
    NVIC_SystemReset();
    break;

  case APP_SCHEDULER_RECONNECT:
    printf("scheduler: disconnect + reconnect\n");
    sl_wisun_disconnect();
    //wait for disconnection complete
    sl_wisun_app_core_wait_state(SL_WISUN_MSG_DISCONNECTED_IND_ID,5000);
    sl_wisun_app_core_util_connect_and_wait();
    break;

  case APP_SCHEDULER_CLEAR_AND_RECONNECT:
    printf("scheduler: clear credential cache + reconnect\n");
    sl_wisun_disconnect();
    //wait for disconnection complete
    sl_wisun_app_core_wait_state(SL_WISUN_MSG_DISCONNECTED_IND_ID,5000);
    sl_wisun_clear_credential_cache();
    sl_wisun_app_core_util_connect_and_wait();
    break;

  case APP_SCHEDULER_OTA_REBOOT_INSTALL:
    printf("scheduler: OTA rebootAndInstall clear_nvm=%u\n", local.clear_nvm_mode);
    // optional NVM cleanup, same logic as in original rebootAndInstall()
    if (local.clear_nvm_mode == CLEAR_NVM_APP) {
      (void)delete_app_parameters();
    } else if (local.clear_nvm_mode == CLEAR_NVM_FULL) {
      (void)nvm3_eraseAll(nvm3_defaultHandle);
    }
    printf("scheduler: bootloader_rebootAndInstall()\n");
    bootloader_rebootAndInstall();
    break;

  default:
    break;
  }
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

void app_scheduler_action_init(void)
{
  memset(&g_scheduler, 0, sizeof(g_scheduler));

  g_scheduler_flags = osEventFlagsNew(NULL);
  g_scheduler_task_id = osThreadNew(scheduler_task, NULL, &g_scheduler_task_attr);
}

bool app_scheduler_action_schedule(app_scheduler_action_type_t action,
                                  uint32_t delay_ms,
                                  uint8_t clear_nvm_mode)
{
  sl_status_t status;

  // Stop any currently running timer
  (void)sl_sleeptimer_stop_timer(&g_scheduler_timer);

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_CRITICAL();
  g_scheduler.action        = action;
  g_scheduler.clear_nvm_mode= clear_nvm_mode;
  g_scheduler.delay_ms      = delay_ms;
  g_scheduler.start_ms      = now_ms();
  g_scheduler.deadline_ms   = g_scheduler.start_ms + (uint64_t)delay_ms;
  g_scheduler.active        = true;
  CORE_EXIT_CRITICAL();

  if (delay_ms == 0U) {
    // immediate: skip sleeptimer, just wake the task
    (void)osEventFlagsSet(g_scheduler_flags, APP_SCHEDULER_FLAG_EXECUTE);
    return true;
  }

  status = sl_sleeptimer_start_timer_ms(&g_scheduler_timer,
                                        delay_ms,
                                        scheduler_timer_cb,
                                        NULL,
                                        0,
                                        0);
  return (status == SL_STATUS_OK);
}

bool app_scheduler_action_get_remaining(uint32_t *remaining_ms,
                                       app_scheduler_action_type_t *action)
{
  if (!g_scheduler.active) {
    return false;
  }

  uint64_t now = now_ms();
  uint64_t rem = (g_scheduler.deadline_ms > now)
                 ? (g_scheduler.deadline_ms - now)
                 : 0U;

  if (remaining_ms != NULL) {
    *remaining_ms = (rem > 0xFFFFFFFFULL) ? 0xFFFFFFFFUL : (uint32_t)rem;
  }
  if (action != NULL) {
    *action = g_scheduler.action;
  }
  return true;
}

