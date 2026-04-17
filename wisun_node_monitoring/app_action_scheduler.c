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

#include <string.h>

#include "sl_sleeptimer.h"
#include "cmsis_os2.h"
#include "em_core.h"
#include "printf.h"

// -----------------------------------------------------------------------------
// Local state
// -----------------------------------------------------------------------------

static app_scheduler_action_state_t g_scheduler_queue[APP_SCHEDULER_MAX_SLOTS];
static uint8_t g_scheduler_count;
static sl_sleeptimer_timer_handle_t g_scheduler_timer;

static osThreadId_t g_scheduler_task_id;
static osEventFlagsId_t g_scheduler_flags;

#define APP_SCHEDULER_FLAG_EXECUTE         (1U << 0)
#define APP_SCHEDULER_TASK_SIZE_BYTES      (1 * 2048UL)

static const osThreadAttr_t g_scheduler_task_attr = {
  .name = "scheduler_action",
  .attr_bits = osThreadDetached,
  .cb_mem = NULL,
  .cb_size = 0,
  .stack_mem = NULL,
  .stack_size = APP_SCHEDULER_TASK_SIZE_BYTES,
  .priority = osPriorityAboveNormal,
  .tz_module = 0
};

static void scheduler_timer_cb(sl_sleeptimer_timer_handle_t *handle, void *data);

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

// Convert the shared sleeptimer tick source to milliseconds for queue deadlines.
static uint64_t now_ms(void)
{
  uint64_t ticks = sl_sleeptimer_get_tick_count64();
  uint32_t freq = sl_sleeptimer_get_timer_frequency();

  if (freq == 0U) {
    return 0U;
  }

  return (ticks * 1000ULL) / (uint64_t)freq;
}

// Remove one entry and keep the queue densely packed and deadline ordered (index 0 = next deadline)
static void queue_remove(uint8_t idx)
{
  uint8_t i;

  if (idx >= g_scheduler_count) {
    return;
  }

  for (i = idx; (i + 1U) < g_scheduler_count; ++i) {
    g_scheduler_queue[i] = g_scheduler_queue[i + 1U];
  }

  if (g_scheduler_count > 0U) {
    g_scheduler_count--;
    memset(&g_scheduler_queue[g_scheduler_count], 0, sizeof(g_scheduler_queue[0]));
  }
}

// Insert while preserving deadline order so queue[0] is always the next due item.
static void queue_insert(const app_scheduler_action_state_t *state)
{
  uint8_t i = g_scheduler_count;

  while ((i > 0U) && (g_scheduler_queue[i - 1U].deadline_ms > state->deadline_ms)) {
    g_scheduler_queue[i] = g_scheduler_queue[i - 1U];
    i--;
  }

  g_scheduler_queue[i] = *state;
  g_scheduler_count++;
}

// Must be called with the scheduler lock held. It always arms only the earliest deadline.
static void rearm_timer_locked(void)
{
  uint32_t delay_ms;
  uint64_t current_ms;
  sl_status_t status;

  (void)sl_sleeptimer_stop_timer(&g_scheduler_timer);

  if (g_scheduler_count == 0U) {
    return;
  }

  current_ms = now_ms();
  delay_ms = (g_scheduler_queue[0].deadline_ms > current_ms)
             ? (uint32_t)(g_scheduler_queue[0].deadline_ms - current_ms)
             : 0U;

  // If the deadline is already due, wake the worker task directly instead of
  // starting a 0 ms timer from inside the critical section.
  if (delay_ms == 0U) {
    (void)osEventFlagsSet(g_scheduler_flags, APP_SCHEDULER_FLAG_EXECUTE);
  } else {
    status = sl_sleeptimer_start_timer_ms(&g_scheduler_timer,
                                       delay_ms,
                                       scheduler_timer_cb,
                                       NULL,
                                       0,
                                       0);
    assert(status == SL_STATUS_OK);
  }
}

// Thin wrapper around the user callback so NULL handling stays in one place.
static uint32_t execute_action(const app_scheduler_action_state_t *local)
{
  if (local->action_fn == NULL) {
    return 1U;
  }

  return local->action_fn(local->context);
}

// Execute every action that is already due. The queue entry is removed before
// the callback runs so stop/query APIs only operate on still-pending instances.
static void process_due_actions(void)
{
  for (;;) {
    app_scheduler_action_state_t local;
    bool have_due = false;
    bool requeue = false;
    uint64_t current_ms;

    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_CRITICAL();

    if (g_scheduler_count > 0U) {
      current_ms = now_ms();
      if (g_scheduler_queue[0].deadline_ms <= current_ms) {
        local = g_scheduler_queue[0];
        queue_remove(0U);
        have_due = true;
      }
    }

    if (!have_due) {
      rearm_timer_locked();
      CORE_EXIT_CRITICAL();
      break;
    }

    // User code runs outside the critical section to avoid blocking scheduling.
    CORE_EXIT_CRITICAL();

    uint32_t result = execute_action(&local);
    if (result != 0U) {
      printf("scheduler: action callback failed with code %lu\n",
                     (unsigned long)result);
    }

    // Periodic actions use fixed-delay scheduling: the next period starts after
    // the current callback finishes.
    if (local.periodic) {
      local.start_ms = now_ms();
      local.deadline_ms = local.start_ms + (uint64_t)local.period_ms;
      requeue = true;
    }

    if (requeue) {
      CORE_ENTER_CRITICAL();
      if (g_scheduler_count < APP_SCHEDULER_MAX_SLOTS) {
        queue_insert(&local);
      }
      rearm_timer_locked();
      CORE_EXIT_CRITICAL();
    }
  }
}

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
      process_due_actions();
    }
  }
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

void app_scheduler_action_init(void)
{
  memset(g_scheduler_queue, 0, sizeof(g_scheduler_queue));
  g_scheduler_count = 0U;
  memset(&g_scheduler_timer, 0, sizeof(g_scheduler_timer));

  g_scheduler_flags = osEventFlagsNew(NULL);
  g_scheduler_task_id = osThreadNew(scheduler_task, NULL, &g_scheduler_task_attr);
  (void)g_scheduler_task_id;
}

bool app_scheduler_action_schedule(app_scheduler_action_fn_t action_fn,
                                   uint32_t delay_ms,
                                   uint32_t period_ms,
                                   void *context)
{
  app_scheduler_action_state_t state;

  if (action_fn == NULL) {
    return false;
  }

  memset(&state, 0, sizeof(state));
  state.active = true;
  state.periodic = (period_ms != 0U);
  state.action_fn = action_fn;
  state.delay_ms = delay_ms;
  state.period_ms = period_ms;
  state.start_ms = now_ms();
  state.deadline_ms = state.start_ms + (uint64_t)delay_ms;
  state.context = context;

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_CRITICAL();
  if (g_scheduler_count >= APP_SCHEDULER_MAX_SLOTS) {
    CORE_EXIT_CRITICAL();
    return false;
  }
  queue_insert(&state);
  rearm_timer_locked();
  CORE_EXIT_CRITICAL();

  return true;
}

bool app_scheduler_action_stop(app_scheduler_action_fn_t action_fn)
{
  bool stopped = false;
  uint8_t i;

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_CRITICAL();

  for (i = 0U; i < g_scheduler_count;) {
    if (g_scheduler_queue[i].action_fn == action_fn) {
      queue_remove(i);
      stopped = true;
    } else {
      i++;
    }
  }

  if (stopped) {
    rearm_timer_locked();
  }

  CORE_EXIT_CRITICAL();
  return stopped;
}

bool app_scheduler_action_get_remaining(app_scheduler_action_fn_t action_fn,
                                        uint32_t *remaining_ms)
{
  uint8_t i;
  uint64_t current_ms;
  bool found = false;

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_CRITICAL();
  current_ms = now_ms();
  for (i = 0U; i < g_scheduler_count; ++i) {
    if (g_scheduler_queue[i].action_fn == action_fn) {
      uint64_t remaining = (g_scheduler_queue[i].deadline_ms > current_ms)
                           ? (g_scheduler_queue[i].deadline_ms - current_ms)
                           : 0U;
      if (remaining_ms != NULL) {
        *remaining_ms = (remaining > 0xFFFFFFFFULL) ? 0xFFFFFFFFUL : (uint32_t)remaining;
      }
      found = true;
      break;
    }
  }
  CORE_EXIT_CRITICAL();

  return found;
}
