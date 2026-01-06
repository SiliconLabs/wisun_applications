/***************************************************************************//**
* @file app_reporter.c
* @brief Pipe to report selected RTT traces to the UDP REPORTER_PORT
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
#include <stdio.h>
#include <string.h>
#include "cmsis_os2.h"
#include "SEGGER_RTT.h"
#include "socket/socket.h"
#include "sl_memory_manager.h"
#include "sl_sleeptimer.h"
#include "sl_wisun_common.h"
#include "sl_wisun_ip6string.h"
#include "app_reporter.h"


#if defined(SL_CATALOG_MICRIUMOS_KERNEL_PRESENT)
#include "os.h"
#endif
#if defined(SL_CATALOG_FREERTOS_KERNEL_PRESENT)
#include "FreeRTOS.h"
#endif

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
#define APP_REPORTER_TASK_STACK_SIZE 2500 // in units of CPU_INT32U
#define REPORTER_PORT                        3770
uint32_t reporter_period_ms;
uint32_t reporter_started = 0;
uint32_t reporter_active  = 0;

#define REPORT_PERIOD_S                      10
#define RTT_REPORT_TASK_FLAG_NONE            (0)
#define RTT_REPORT_TASK_FLAG_SEND       (1 << 0)
#define RTT_REPORT_TASK_FLAG_STOP       (1 << 1)
#define RTT_REPORT_TASK_FLAG_ALL    (1 << 2) - 1

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
sl_sleeptimer_timer_handle_t app_reporter_timer;
static uint32_t              rtt_report_task_flags;
static osEventFlagsId_t      rtt_report_task_flag_group;
static sl_wisun_socket_id_t  app_logs_socket_id = SOCKET_INVALID_ID;
static char                  reporter_match_string[MAX_MATCH_STRING_LEN];
static char                  lines_to_send[BUFFER_SIZE_UP*2];
static in6_addr_t            ipv6_dest;
static char                  ipv6_dest_string[40];

typedef struct reporter_match_struct reporter_match_struct_t;

struct reporter_match_struct {
  uint8_t nb_matches;                   ///< Number of match strings
   char match[10][MAX_MATCH_STRING_LEN];  ///< match strings array
};

reporter_match_struct_t reporter_matches;

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
// Application timestamp mutex
static osMutexId_t _app_reporter_mutex = NULL;

static const osMutexAttr_t _app_reporter_mutex_attr = {
  .name      = "AppReporterMutex",
  .attr_bits = osMutexRecursive,
  .cb_mem    = NULL,
  .cb_size   = 0U
};

/* Mutex acquire */
__STATIC_INLINE void _app_reporter_mutex_acquire(void)
{
  assert(osMutexAcquire(_app_reporter_mutex, osWaitForever) == osOK);
}

/* Mutex release */
__STATIC_INLINE void _app_reporter_mutex_release(void)
{
  assert(osMutexRelease(_app_reporter_mutex) == osOK);
}


void app_reporter_callback(sl_sleeptimer_timer_handle_t *handle, void *data) {
  (void)handle;
  (void)data;
}

/**
 * This function is an adaptation of SEGGER_RTT_ReadNoLock(). As segger does not provide
 * any API to read from up buffers, a little work around was necessary.
 *********************************************************************
*                    SEGGER Microcontroller GmbH                     *
*                        The Embedded Experts                        *
**********************************************************************
*                                                                    *
*            (c) 1995 - 2023 SEGGER Microcontroller GmbH             *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************
*                                                                    *
*       SEGGER SystemView * Real-time application analysis           *
*                                                                    *
**********************************************************************
*                                                                    *
* All rights reserved.                                               *
*                                                                    *
* SEGGER strongly recommends to not make any changes                 *
* to or modify the source code of this software in order to stay     *
* compatible with the SystemView and RTT protocol, and J-Link.       *
*                                                                    *
* Redistribution and use in source and binary forms, with or         *
* without modification, are permitted provided that the following    *
* condition is met:                                                  *
*                                                                    *
* o Redistributions of source code must retain the above copyright   *
*   notice, this condition and the following disclaimer.             *
*                                                                    *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND             *
* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,        *
* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF           *
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE           *
* DISCLAIMED. IN NO EVENT SHALL SEGGER Microcontroller BE LIABLE FOR *
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR           *
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT  *
* OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;    *
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF      *
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT          *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE  *
* USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH   *
* DAMAGE.                                                            *
*                                                                    *
**********************************************************************
*                                                                    *
*       SystemView version: 3.52                                    *
*                                                                    *
**********************************************************************

 */
static unsigned read_segger_up_buffer(unsigned BufferIndex, void* pData, unsigned BufferSize) {
  unsigned                NumBytesRem;
  unsigned                NumBytesRead;
  unsigned                RdOff;
  unsigned                WrOff;
  unsigned char*          pBuffer;
  SEGGER_RTT_BUFFER_UP*   pRing;
  //
  pRing = &_SEGGER_RTT.aUp[BufferIndex];
  pBuffer = (unsigned char*)pData;
  RdOff = pRing->RdOff;
  WrOff = pRing->WrOff;
  NumBytesRead = 0u;
  //
  // Read from current read position to wrap-around of buffer, first
  //
  if (RdOff > WrOff) {
    NumBytesRem = pRing->SizeOfBuffer - RdOff;
    NumBytesRem = MIN(NumBytesRem, BufferSize);
    memcpy(pBuffer, pRing->pBuffer + RdOff, NumBytesRem);
    NumBytesRead += NumBytesRem;
    pBuffer      += NumBytesRem;
    BufferSize   -= NumBytesRem;
    RdOff        += NumBytesRem;
    //
    // Handle wrap-around of buffer
    //
    if (RdOff == pRing->SizeOfBuffer) {
      RdOff = 0u;
    }
  }
  //
  // Read remaining items of buffer
  //
  NumBytesRem = WrOff - RdOff;
  NumBytesRem = MIN(NumBytesRem, BufferSize);
  if (NumBytesRem > 0u) {
    memcpy(pBuffer, pRing->pBuffer + RdOff, NumBytesRem);
    NumBytesRead += NumBytesRem;
    pBuffer      += NumBytesRem;
    BufferSize   -= NumBytesRem;
    RdOff        += NumBytesRem;
  }
  if (NumBytesRead) {
    pRing->RdOff = RdOff;
  }
  //
  return NumBytesRead;
}

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
static void reporter_timer_cb(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  assert((osEventFlagsSet(rtt_report_task_flag_group,
                          RTT_REPORT_TASK_FLAG_SEND) & CMSIS_RTOS_ERROR_MASK) == 0);
}

uint8_t filter_log_lines(char* log_lines, char* lines_to_send) {
  const char crlf[] = "\n";
  char *line;
  char *match_in_line;
  uint8_t matches;
  uint8_t i;
  uint8_t line_count = 0;

  // Clear the lines to send after filtering
  sprintf(lines_to_send, "%s ", device_mac_string);

  // Get the first line
  line = strtok(log_lines, crlf);

  // Walk through other tokens
  while (line != NULL) {
      matches = 0;
      if (strcmp(reporter_matches.match[0], "*") == 0) {
          matches++;
      } else {
      for (i = 0; i < reporter_matches.nb_matches ; i++) {
          if (strlen(reporter_matches.match[i]) > 1) {
              match_in_line = strstr(line, reporter_matches.match[i]);
              if (match_in_line != NULL) {
                  matches++;
              }
          }
        }
      }
      if (matches) {
          line_count++;
          if (line_count > 1) {
              strcat(lines_to_send, "\n");
          }
          strcat(lines_to_send, line);
      }
      // Get the next line
      line = strtok(NULL, crlf);
  }

  return strlen(lines_to_send) - strlen(device_mac_string) -1;
}

static void check_and_send_reporter_logs(char *log_buffer)
{
  /** Retrieve logs from SEGGER_RTT
   * SEGGER does not provide any API to read the content of its up buffers (target to host).
   * However, the _SEGGER_RTT structure is globally accessible from the SEGGER_RTT.h file.
   * It contains every buffer structure and metadata.
   * The stack writes logs in the up buffer 0.
   * We need to lock RTT logs writing and read data inside the stack's RTT up buffer.
   */
  int32_t socket_retval = SOCKET_RETVAL_ERROR;
  uint16_t read_bytes;
  uint16_t filtered_bytes;

  memset(log_buffer, 0x00, BUFFER_SIZE_UP);
  _app_reporter_mutex_acquire();
  SEGGER_RTT_LOCK();
  read_bytes = read_segger_up_buffer(0, log_buffer, BUFFER_SIZE_UP);
  SEGGER_RTT_UNLOCK();

  if (read_bytes == 0) {
      _app_reporter_mutex_release();
      return;
  }

  filtered_bytes = filter_log_lines(log_buffer, lines_to_send);

  if (filtered_bytes > 0) {
    /* Send on socket to the destination */
    sockaddr_in6_t dest_ipv6_addr = {
      .sin6_family = AF_INET6,
      .sin6_port = htons(REPORTER_PORT),
      .sin6_flowinfo = 0,
      .sin6_addr = ipv6_dest,
      .sin6_scope_id = 0,
    };
    // sendto() needs to be outside of the RTT lock to be able to send data
    socket_retval = sendto(app_logs_socket_id,
                           (const void *)lines_to_send,
                           strlen(lines_to_send),
                           0,
                           (const struct sockaddr *)&dest_ipv6_addr,
                            sizeof(dest_ipv6_addr));

    if (socket_retval == SOCKET_RETVAL_ERROR) {
      printf("Could not send log\n");
    } else {
      printf("%s\n", lines_to_send );
    }
  }
  _app_reporter_mutex_release();
}

void app_reporter_task(void *args)
{
  (void)args;
  sl_status_t ret;
  int32_t socket_retval = SOCKET_RETVAL_ERROR;
  char *log_buffer = NULL;

  log_buffer = (char *)sl_malloc(BUFFER_SIZE_UP);
  if (!log_buffer) {
    goto cleanup;
  }

  //setup the socket
  uint32_t flags = SL_WISUN_SOCKET_EVENT_MODE_INDICATION;
  const sockaddr_in6_t udp_server_bind_addr = {
    .sin6_family = AF_INET6,
    .sin6_port = htons(REPORTER_PORT),
    .sin6_flowinfo = 0,
    .sin6_addr = IN6ADDR_ANY_INIT,
    .sin6_scope_id = 0,
  };

  app_logs_socket_id = socket(AF_INET6, (SOCK_DGRAM | SOCK_NONBLOCK), IPPROTO_UDP);
  if (app_logs_socket_id == SOCKET_RETVAL_ERROR) {
    printf("could not open reporter socket: %d\n", app_logs_socket_id);
    goto cleanup;
  }

  socket_retval = bind(app_logs_socket_id, (const struct sockaddr *)&udp_server_bind_addr, sizeof(udp_server_bind_addr));
  if (socket_retval == SOCKET_RETVAL_ERROR) {
    printf("could not bind reporter socket: %ld\n", socket_retval);
    goto cleanup;
  }

  socket_retval = setsockopt(app_logs_socket_id, APP_LEVEL_SOCKET, SOCKET_EVENT_MODE, &flags, sizeof(uint32_t));
  if (socket_retval == SOCKET_RETVAL_ERROR) {
    printf("could not set reporter socket option: %ld", socket_retval);
    goto cleanup;
  }

  // Setup periodic report
  const osEventFlagsAttr_t rtt_report_task_flags_attr = {
    "RTT reporter Task Flags",
    0,
    NULL,
    0
  };

  rtt_report_task_flag_group = osEventFlagsNew(&rtt_report_task_flags_attr);
  assert(rtt_report_task_flag_group != NULL);
  ret = sl_sleeptimer_start_periodic_timer_ms(&app_reporter_timer,
                                        reporter_period_ms,
                                        reporter_timer_cb,
                                        NULL,
                                        0,
                                        0);
  if(ret != SL_STATUS_OK) {
    goto cleanup;
  }

  while (1) {
    if (reporter_active == 1) {
      rtt_report_task_flags = osEventFlagsWait(rtt_report_task_flag_group,
                                               RTT_REPORT_TASK_FLAG_ALL,
                                               osFlagsWaitAny,
                                               osWaitForever);
      assert((rtt_report_task_flags & CMSIS_RTOS_ERROR_MASK) == 0);
      if (rtt_report_task_flags & RTT_REPORT_TASK_FLAG_SEND) {
        check_and_send_reporter_logs(log_buffer);
      } else if (rtt_report_task_flags & RTT_REPORT_TASK_FLAG_STOP) {
        goto cleanup;
      }
    }
  }

cleanup:
  if (app_logs_socket_id != SOCKET_INVALID_ID) {
    socket_retval = close(app_logs_socket_id);
    if (socket_retval == SOCKET_RETVAL_ERROR) {
      printf("could not close reporter socket: %ld\n", socket_retval);
    } else {
      printf("reporter socket closed\n");
    }
  }
  printf("Reporter task done\n");
  if (log_buffer){
    free(log_buffer);
  }
  osThreadExit();
}

void app_start_reporter_thread()
{
  if (reporter_started == 0) {
    // init mutex
    _app_reporter_mutex = osMutexNew(&_app_reporter_mutex_attr);
    assert(_app_reporter_mutex != NULL);

    const osThreadAttr_t rtt_report_attr = {
      .name = "rtt_report",
      .attr_bits = osThreadDetached,
      .cb_mem = NULL,
      .cb_size = 0,
      .stack_mem = NULL,
      .stack_size = (APP_REPORTER_TASK_STACK_SIZE * sizeof(void *)) & 0xFFFFFFF8u,
      .priority = osPriorityLow3,
      .tz_module = 0,
    };

    osThreadId_t rtt_thr_id = osThreadNew(app_reporter_task, NULL, &rtt_report_attr);
    assert(rtt_thr_id != NULL);

    reporter_started = 1;
  }
}

void app_start_reporter(char *report__dest_ipv6_str,
                        uint32_t report_period_ms,
                        char *match_string)
{
  int i;
  if (reporter_started == 0) {
    app_start_reporter_thread();
  }

    sprintf(ipv6_dest_string, report__dest_ipv6_str);
    sl_wisun_stoip6(report__dest_ipv6_str, strlen(report__dest_ipv6_str), &ipv6_dest);

  strncpy((char*)reporter_match_string, match_string, MAX_MATCH_STRING_LEN);

  reporter_matches.nb_matches = 0;
  const char pipe[] = "|";
  char *match;

  // Get the first match string
  match = strtok(match_string, pipe);
  if (match == NULL) {
    strncpy(reporter_matches.match[reporter_matches.nb_matches], match_string, MAX_MATCH_STRING_LEN);
  }
  // Walk through other matches
  while (match != NULL) {
      strncpy(reporter_matches.match[reporter_matches.nb_matches], match, MAX_MATCH_STRING_LEN);
      reporter_matches.nb_matches++;
      // get next match
      match = strtok(NULL, pipe);
  }
  printf("Reporting RTT lines matching %d patterns to UDP port %d on %s\n",
         reporter_matches.nb_matches, REPORTER_PORT, report__dest_ipv6_str);

  for (i=0; i<reporter_matches.nb_matches ; i++) {
      printf("reporter_matches.match[%d] %s\n", i, reporter_matches.match[i]);
  }

  reporter_period_ms = report_period_ms;
  reporter_active  = 1;
}

void app_stop_reporter(void)
{
  reporter_active  = 0;
}
