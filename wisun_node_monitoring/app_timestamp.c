/***************************************************************************//**
* @file app_timestamp.c
* @brief 1 Second time stamp functions
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
// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include "app_timestamp.h"
#include "cmsis_os2.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
#define TIME_STRING_LEN 18

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
sl_sleeptimer_timestamp_64_t app_timestamp;
char timestamped_msg_buffer[TIMESTAMP_MSG_LEN];
char *timestamped_msg;
// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
char time_str[TIME_STRING_LEN];
sl_sleeptimer_timer_handle_t app_timer;

// Application timestamp mutex
static osMutexId_t _app_timestamp_mutex = NULL;

static const osMutexAttr_t _app_timestamp_mutex_attr = {
  .name      = "AppTimestampMutex",
  .attr_bits = osMutexRecursive,
  .cb_mem    = NULL,
  .cb_size   = 0U
};

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
/* Mutex acquire */
__STATIC_INLINE void _app_timestamp_mutex_acquire(void)
{
  assert(osMutexAcquire(_app_timestamp_mutex, osWaitForever) == osOK);
}

/* Mutex release */
__STATIC_INLINE void _app_timestamp_mutex_release(void)
{
  assert(osMutexRelease(_app_timestamp_mutex) == osOK);
}

void app_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data) {
  (void)handle;
  (void)data;
  app_timestamp++;
}

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
sl_status_t app_timestamp_init(void) {
  sl_status_t status;
  uint32_t app_timer_timeout;

  // init mutex
  _app_timestamp_mutex = osMutexNew(&_app_timestamp_mutex_attr);
  assert(_app_timestamp_mutex != NULL);
  timestamped_msg = timestamped_msg_buffer;

  app_timestamp =  0;

  status = sl_sleeptimer_init();
  if (status != SL_STATUS_OK) {
      printf("Error initializing sleeptimer. Status %lu\n", status);
      return status;
  }
  app_timer_timeout = (uint32_t)1.0*sl_sleeptimer_get_timer_frequency();
  status = sl_sleeptimer_start_periodic_timer(&app_timer,
                                              app_timer_timeout,
                                              app_timer_callback,
                                              NULL, 0, 0);
  if (status != SL_STATUS_OK) {
      printf("Error starting periodic timer 'app_timer'. Status %lu\n", status);
      return status;
  }
  return status;
}

sl_status_t  d_h_m_s_total(sl_sleeptimer_timestamp_64_t timestamp_secs,
                    uint16_t* days,
                    uint64_t* hours,
                    uint64_t* mins,
                    uint64_t* secs
) {
  *days  = timestamp_secs / 60 / 60 / 24;
  *hours = timestamp_secs / 60 / 60;
  *mins  = timestamp_secs / 60;
  *secs  = timestamp_secs;
  return SL_STATUS_OK;
}

sl_status_t  d_h_m_s      (sl_sleeptimer_timestamp_64_t timestamp_secs,
                    uint16_t* days,
                    uint8_t* hours,
                    uint8_t* mins,
                    uint8_t* secs) {
  uint64_t hours_total;
  uint64_t mins_total;
  uint64_t secs_total;

  d_h_m_s_total(timestamp_secs, days, &hours_total, &mins_total, &secs_total);

  *days = *days;
  *hours = hours_total % 24;
  *mins  = mins_total  % 60;
  *secs  = secs_total  % 60;

  return SL_STATUS_OK;
}

char*        dhms         (sl_sleeptimer_timestamp_64_t timestamp_secs) {
  uint16_t days;
  uint8_t  hours, mins, secs;

  d_h_m_s(timestamp_secs, &days, &hours, &mins, &secs);

  snprintf(time_str, TIME_STRING_LEN, "%3d-%02d:%02d:%02d", days, hours, mins, secs);

  return time_str;
}

uint64_t     now_sec      (void) {
  sl_sleeptimer_timestamp_64_t current_sec;
  _app_timestamp_mutex_acquire();
  current_sec = app_timestamp;
  _app_timestamp_mutex_release();
  return (uint64_t)current_sec;
}

char*        now_str     (void) {
  return dhms(now_sec());
}
