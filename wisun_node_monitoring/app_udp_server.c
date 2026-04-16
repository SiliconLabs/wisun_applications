/***************************************************************************//**
 * @file app_udp_server.c
 * @brief UDP server for a Wi-SUN Node
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
 * This code has not been formally tested and is provided as-is. It is not
 * suitable for production environments.
 * This code will not be maintained.
 *
 ******************************************************************************/

// -----------------------------------------------------------------------------
// Includes
// -----------------------------------------------------------------------------

#include "app_udp_server.h"

#ifdef WITH_UDP_SERVER

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "cmsis_os2.h"
#include "sl_wisun_api.h"
#include "sl_wisun_version.h"
#include "sl_string.h"
#include "sl_wisun_app_core_util.h"
#include "sl_wisun_trace_util.h"

#include "app.h"

#if __has_include("app_wisun_multicast_ota.h")
#include "app_wisun_multicast_ota.h"
#endif

// -----------------------------------------------------------------------------
// Macros and Typedefs
// -----------------------------------------------------------------------------

#define SL_WISUN_UDP_SERVER_PORT_DEFAULT      7777U
#define SL_WISUN_UDP_SERVER_BUFF_SIZE         1232U

#if (WITH_UDP_SERVER == SO_EVENT_MODE)
#define UDP_WORKER_STACK_SIZE_BYTES           4096U
#define UDP_WORKER_QUEUE_LEN                  4U
#endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */

typedef struct {
  int32_t data_length;
  socklen_t addr_len;
  sockaddr_in6_t client_addr;
  char buff[SL_WISUN_UDP_SERVER_BUFF_SIZE];
} udp_rx_msg_t;


// -----------------------------------------------------------------------------
// Static Function Declarations
// -----------------------------------------------------------------------------

static void _udp_custom_callback(sl_wisun_evt_t *evt);
static void _udp_handle_rx_payload(udp_rx_msg_t *msg);

#if (WITH_UDP_SERVER == SO_EVENT_MODE)
static void _udp_worker_task(void *argument);
static bool _udp_worker_init(void);
#endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */

// -----------------------------------------------------------------------------
// Static Variables
// -----------------------------------------------------------------------------

static sockaddr_in6_t udp_server_addr = { 0U };

static int32_t udp_server_sockid = SOCKET_INVALID_ID;
static int32_t udp_r = SOCKET_RETVAL_ERROR;
static uint32_t count_udp_rx = 0U;
static uint32_t count_udp_drop = 0U;
static uint16_t udp_server_port = SL_WISUN_UDP_SERVER_PORT_DEFAULT;

const char *udp_ip_str = NULL;

#if (WITH_UDP_SERVER == SO_EVENT_MODE)
static osThreadId_t udp_worker_thread_id = NULL;
static osMessageQueueId_t udp_rx_queue_id = NULL;
static osMemoryPoolId_t udp_rx_pool_id = NULL;
#endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */

// -----------------------------------------------------------------------------
// Public Function Definitions
// -----------------------------------------------------------------------------

/* UDP Server initialization function, to be called once connected */
void init_udp_server(void)
{
  int32_t socket_type;

#if (WITH_UDP_SERVER == SO_EVENT_MODE)
  socket_type = SOCK_DGRAM;
  printfBoth("udp_server in blocking/Event mode\n");
#elif (WITH_UDP_SERVER == SO_NONBLOCK)
  socket_type = SOCK_DGRAM | SOCK_NONBLOCK;
  printfBoth("udp_server in non-blocking/Polling mode\n");
#else
  #error "Unsupported WITH_UDP_SERVER mode"
#endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */

  udp_server_sockid = socket(AF_INET6, socket_type, IPPROTO_UDP);
  printfBoth("udp_server_sockid %ld\n", udp_server_sockid);
  assert_res(udp_server_sockid, "UDP server socket()");

  // Fill the UDP server address structure
  udp_server_addr.sin6_family = AF_INET6;
  udp_server_addr.sin6_addr = in6addr_any;
  udp_server_addr.sin6_port = htons(udp_server_port);

  // Bind UDP server address to the socket
  udp_r = bind(udp_server_sockid,
               (const struct sockaddr *)&udp_server_addr,
                sizeof(udp_server_addr));
  assert_res(udp_r, "UDP server bind()");

#if (WITH_UDP_SERVER == SO_EVENT_MODE)
  app_wisun_em_custom_callback_register(SL_WISUN_MSG_SOCKET_DATA_AVAILABLE_IND_ID,
                                        _udp_custom_callback);
  assert_res(udp_r, "_udp_custom_callback() registration");
  /* MD: we don't notify anymore when a connection is done */

  assert(_udp_worker_init() == true);
#endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */

  {
    udp_ip_str = app_wisun_trace_util_get_ip_str((void *)&udp_server_addr.sin6_addr);
    if (udp_ip_str != NULL) {
      printfBoth("Waiting for UDP messages on %s port %d\n", udp_ip_str, udp_server_port);
      sl_free((void *)udp_ip_str);
    } else {
      printfBoth("Waiting for UDP messages on <ip conv failed> port %d\n", udp_server_port);
    }
  }
}

void check_udp_server_messages(void)
{
#if (WITH_UDP_SERVER == SO_NONBLOCK)
  static udp_rx_msg_t msg;
  socklen_t addr_len = sizeof(msg.client_addr);
  int32_t len;

  memset(&msg, 0, sizeof(msg));

  len = recvfrom(udp_server_sockid,
                  msg.buff,
                  sizeof(msg.buff) - 1U,
                  0,
                 (struct sockaddr *)&msg.client_addr,
                  &addr_len);

  if (len <= 0) {
    return;
  }

  msg.data_length = len;
  msg.addr_len = addr_len;
  msg.buff[len] = '\0';

  _udp_handle_rx_payload(&msg);
#endif /* (WITH_UDP_SERVER == SO_NONBLOCK) */
}

// -----------------------------------------------------------------------------
// Static Function Definitions
// -----------------------------------------------------------------------------

static void _udp_handle_rx_payload(udp_rx_msg_t *msg)
{
  if (msg == NULL) {
    return;
  }

  count_udp_rx++;

  udp_ip_str = app_wisun_trace_util_get_ip_str((void *)&msg->client_addr.sin6_addr);

#ifdef APP_WISUN_MULTICAST_OTA_H
  if (strncmp(msg->buff, "OTA ", 4) == 0) {
    if (multicast_rx(msg->buff, msg->data_length, udp_ip_str) != 0) {
      sl_free((void *)udp_ip_str);
      return;
    }
  }
#endif /* APP_WISUN_MULTICAST_OTA_H */


  if (udp_ip_str != NULL) {
    printfBothTime("UDP Rx %2lu from %s (%ld bytes): %s\n",
                    (unsigned long)count_udp_rx,
                    udp_ip_str,
                    (long)msg->data_length,
                    msg->buff);
    sl_free((void *)udp_ip_str);
  } else {
    printfBothTime("UDP Rx %2lu from <ip conv failed> (%ld bytes): %s\n",
                    (unsigned long)count_udp_rx,
                    (long)msg->data_length,
                    msg->buff);
  }

#ifdef APP_DIRECT_CONNECT_H
  if (strncmp(msg->buff, "wisun", 5) == 0) {
    printfBothTime(app_direct_connect_cli(msg->buff));
  }
#endif /* APP_DIRECT_CONNECT_H */
}

#if (WITH_UDP_SERVER == SO_EVENT_MODE)

static bool _udp_worker_init(void)
{
  const osThreadAttr_t udp_worker_attr = {
    .name       = "udp_worker",
    .attr_bits  = osThreadDetached,
    .cb_mem     = NULL,
    .cb_size    = 0U,
    .stack_mem  = NULL,
    .stack_size = UDP_WORKER_STACK_SIZE_BYTES,
    .priority   = osPriorityNormal,
    .tz_module  = 0U
  };

  udp_rx_pool_id = osMemoryPoolNew(UDP_WORKER_QUEUE_LEN,
                                    sizeof(udp_rx_msg_t),
                                    NULL);
  if (udp_rx_pool_id == NULL) {
    return false;
  }

  udp_rx_queue_id = osMessageQueueNew(UDP_WORKER_QUEUE_LEN,
                                      sizeof(udp_rx_msg_t *),
                                      NULL);
  if (udp_rx_queue_id == NULL) {
    return false;
  }

  printf("%s/%s starting '%s' thread with stack_size of %4lu bytes\n",
          __FILE__,
          __FUNCTION__,
          udp_worker_attr.name,
          (unsigned long)udp_worker_attr.stack_size);

  udp_worker_thread_id = osThreadNew(_udp_worker_task, NULL, &udp_worker_attr);
  if (udp_worker_thread_id == NULL) {
    return false;
  }

  return true;
}

static void _udp_custom_callback(sl_wisun_evt_t *evt)
{
  udp_rx_msg_t *msg;
  socklen_t addr_len;
  int32_t len;
  osStatus_t st;

  if (evt == NULL) {
    return;
  }

  if (evt->header.id != SL_WISUN_MSG_SOCKET_DATA_AVAILABLE_IND_ID) {
    return;
  }

  if (evt->evt.socket_data_available.socket_id != udp_server_sockid) {
    return;
  }

  msg = (udp_rx_msg_t *)osMemoryPoolAlloc(udp_rx_pool_id, 0U);
  if (msg == NULL) {
    count_udp_drop++;
    return;
  }

  memset(msg, 0, sizeof(*msg));
  addr_len = sizeof(msg->client_addr);

  len = recvfrom(udp_server_sockid,
                  msg->buff,
                  sizeof(msg->buff) - 1U,
                  0,
                 (struct sockaddr *)&msg->client_addr,
                  &addr_len);

  if (len < 0) {
    (void)osMemoryPoolFree(udp_rx_pool_id, msg);
    return;
  }

  msg->data_length = len;
  msg->addr_len = addr_len;
  msg->buff[len] = '\0';

  st = osMessageQueuePut(udp_rx_queue_id, &msg, 0U, 0U);
  if (st != osOK) {
    count_udp_drop++;
    (void)osMemoryPoolFree(udp_rx_pool_id, msg);
    /* MD: Free more generic?*/
  }
}

static void _udp_worker_task(void *argument)
{
  udp_rx_msg_t *msg = NULL;

  (void)argument;

  for (;;) {
    if (osMessageQueueGet(udp_rx_queue_id, &msg, NULL, osWaitForever) != osOK) {
      continue;
    }

    if (msg == NULL) {
      continue;
    }

    if ((msg->data_length < 0)
        || ((uint32_t)msg->data_length >= sizeof(msg->buff))) {
      (void)osMemoryPoolFree(udp_rx_pool_id, msg);
      continue;
    }

    msg->buff[msg->data_length] = '\0';
    _udp_handle_rx_payload(msg);

    (void)osMemoryPoolFree(udp_rx_pool_id, msg);
    msg = NULL;
  }
}

#endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */

#endif /* WITH_UDP_SERVER */
