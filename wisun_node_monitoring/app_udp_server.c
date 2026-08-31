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
#include "app.h"

#ifdef WITH_UDP_SERVER

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cmsis_os2.h"
#include "sl_string.h"
#include "sl_wisun_api.h"
#include "sl_wisun_app_core_util.h"
#include "sl_wisun_trace_util.h"

// -----------------------------------------------------------------------------
// Macros and Typedefs
// -----------------------------------------------------------------------------

#define SL_WISUN_UDP_SERVER_BUFF_SIZE         1232U

#if (WITH_UDP_SERVER == SO_EVENT_MODE)
#define UDP_WORKER_STACK_SIZE_BYTES           4096U
#define UDP_WORKER_QUEUE_LEN                  4U
#endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */

typedef struct {
  bool used;
  int32_t sockid;
  uint16_t local_port;
  sockaddr_in6_t addr;
} udp_socket_entry_t;

typedef struct {
  bool used;
  uint16_t local_port;
  uint16_t prefix_len;
  uint8_t prefix[APP_UDP_SERVER_PREFIX_MAX_LEN];
  app_udp_server_rx_callback_t callback;
} udp_callback_entry_t;

typedef struct {
  int32_t sockid;
  uint16_t local_port;
  uint16_t data_length;
  socklen_t addr_len;
  sockaddr_in6_t client_addr;
  sockaddr_in6_t dest_addr;
  bool dest_addr_valid;
  uint8_t buff[SL_WISUN_UDP_SERVER_BUFF_SIZE];
} udp_rx_msg_t;

// -----------------------------------------------------------------------------
// Static Function Declarations
// -----------------------------------------------------------------------------

#if (WITH_UDP_SERVER == SO_EVENT_MODE)
static void _udp_custom_callback(sl_wisun_evt_t *evt);
#endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */
static void _udp_handle_rx_payload(udp_rx_msg_t *msg);
static bool _udp_dispatch_rx_payload(const udp_rx_msg_t *msg);
static int32_t _udp_ensure_socket(uint16_t port);
static bool _udp_open_socket(udp_socket_entry_t *entry,
                             uint16_t port,
                             int32_t socket_type);
static bool _udp_enable_pktinfo(int32_t sockid);
static int32_t _udp_socket_type(void);
static bool _udp_recv_socket(int32_t sockid, uint16_t local_port, udp_rx_msg_t *msg);
static bool _udp_extract_destination(const msghdr_t *rx_msg,
                                     uint16_t local_port,
                                     sockaddr_in6_t *dst_addr);
static bool _udp_is_rx_sockid(int32_t sockid);
static uint16_t _udp_port_from_sockid(int32_t sockid);
static bool _udp_prefix_matches(const uint8_t *prefix,
                                uint16_t prefix_len,
                                const uint8_t *payload,
                                uint16_t payload_len);
static void _udp_print_waiting(sockaddr_in6_t *addr, uint16_t port);

#if (WITH_UDP_SERVER == SO_EVENT_MODE)
static void _udp_worker_task(void *argument);
static bool _udp_worker_init(void);
static void _udp_drain_socket(int32_t sockid, uint16_t local_port);
#endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */

// -----------------------------------------------------------------------------
// Static Variables
// -----------------------------------------------------------------------------

static udp_socket_entry_t udp_sockets[APP_UDP_SERVER_MAX_SOCKETS];
static udp_callback_entry_t udp_callbacks[APP_UDP_SERVER_MAX_CALLBACKS];

static int32_t udp_r = SOCKET_RETVAL_ERROR;
static uint32_t count_udp_rx = 0U;
static uint32_t count_udp_drop = 0U;
static bool udp_server_initialized = false;

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
  if (udp_server_initialized) {
    return;
  }

#if (WITH_UDP_SERVER == SO_EVENT_MODE)
  printfBoth("udp_server in Event mode\n");
#elif (WITH_UDP_SERVER == SO_NONBLOCK)
  printfBoth("udp_server in non-blocking/Polling mode\n");
#else
  #error "Unsupported WITH_UDP_SERVER mode"
#endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */

#if (WITH_UDP_SERVER == SO_EVENT_MODE)
  udp_r = app_wisun_em_custom_callback_register(SL_WISUN_MSG_SOCKET_DATA_AVAILABLE_IND_ID,
                                                _udp_custom_callback);
  assert_res(udp_r, "_udp_custom_callback() registration");
  /* MD: we don't notify anymore when a connection is done */

  assert(_udp_worker_init() == true);
#endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */

  udp_server_initialized = true;
}

void check_udp_server_messages(void)
{
#if (WITH_UDP_SERVER == SO_NONBLOCK)
  static udp_rx_msg_t msg;
  uint32_t i;

  for (i = 0U; i < APP_UDP_SERVER_MAX_SOCKETS; i++) {
    if (!udp_sockets[i].used) {
      continue;
    }

    memset(&msg, 0, sizeof(msg));
    if (_udp_recv_socket(udp_sockets[i].sockid,
                         udp_sockets[i].local_port,
                         &msg)) {
      _udp_handle_rx_payload(&msg);
    }
  }
#endif /* (WITH_UDP_SERVER == SO_NONBLOCK) */
}

int32_t app_udp_server_register(uint16_t port,
                                const uint8_t *prefix,
                                uint16_t prefix_len,
                                app_udp_server_rx_callback_t callback)
{
  uint32_t i;
  uint32_t free_index = APP_UDP_SERVER_MAX_CALLBACKS;
  int32_t sockid;

  if ((port == 0U)
      || (prefix == NULL)
      || (prefix_len == 0U)
      || (callback == NULL)) {
    return SOCKET_INVALID_ID;
  }

  if (prefix_len > APP_UDP_SERVER_PREFIX_MAX_LEN) {
    return SOCKET_INVALID_ID;
  }

  for (i = 0U; i < APP_UDP_SERVER_MAX_CALLBACKS; i++) {
    if (udp_callbacks[i].used
        && (udp_callbacks[i].local_port == port)
        && (udp_callbacks[i].prefix_len == prefix_len)
        && (memcmp(udp_callbacks[i].prefix, prefix, prefix_len) == 0)) {
      sockid = _udp_ensure_socket(port);
      if (sockid == SOCKET_INVALID_ID) {
        return SOCKET_INVALID_ID;
      }
      udp_callbacks[i].callback = callback;
      return sockid;
    }

    if (!udp_callbacks[i].used && (free_index == APP_UDP_SERVER_MAX_CALLBACKS)) {
      free_index = i;
    }
  }

  if (free_index == APP_UDP_SERVER_MAX_CALLBACKS) {
    return SOCKET_INVALID_ID;
  }

  sockid = _udp_ensure_socket(port);
  if (sockid == SOCKET_INVALID_ID) {
    return SOCKET_INVALID_ID;
  }

  udp_callbacks[free_index].used = true;
  udp_callbacks[free_index].local_port = port;
  udp_callbacks[free_index].prefix_len = prefix_len;
  udp_callbacks[free_index].callback = callback;
  memcpy(udp_callbacks[free_index].prefix, prefix, prefix_len);

  printfBoth("Registered UDP prefix len %u on port %u socket %ld\n",
             (unsigned int)prefix_len,
             (unsigned int)port,
             (long)sockid);

  return sockid;
}

// -----------------------------------------------------------------------------
// Static Function Definitions
// -----------------------------------------------------------------------------

static void _udp_handle_rx_payload(udp_rx_msg_t *msg)
{
  const char *udp_ip_str;

  if (msg == NULL) {
    return;
  }

  count_udp_rx++;

  _udp_dispatch_rx_payload(msg);

  udp_ip_str = app_wisun_trace_util_get_ip_str((void *)&msg->client_addr.sin6_addr);
  if (udp_ip_str != NULL) {
    printfTimeRTT("UDP Rx %2lu from %s on port %u (%u bytes)\n",
                   (unsigned long)count_udp_rx,
                   udp_ip_str,
                   (unsigned int)msg->local_port,
                   (unsigned int)msg->data_length);
    sl_free((void *)udp_ip_str);
  } else {
    printfTimeRTT("UDP Rx %2lu from <ip conv failed> on port %u (%u bytes)\n",
                   (unsigned long)count_udp_rx,
                   (unsigned int)msg->local_port,
                   (unsigned int)msg->data_length);
  }
}

static bool _udp_dispatch_rx_payload(const udp_rx_msg_t *msg)
{
  uint32_t i;

  if (msg == NULL) {
    return false;
  }

  for (i = 0U; i < APP_UDP_SERVER_MAX_CALLBACKS; i++) {
    if (!udp_callbacks[i].used
        || (udp_callbacks[i].local_port != msg->local_port)
        || !_udp_prefix_matches(udp_callbacks[i].prefix,
                                udp_callbacks[i].prefix_len,
                                msg->buff,
                                msg->data_length)) {
      continue;
    }

    return udp_callbacks[i].callback(msg->sockid,
                                     msg->buff,
                                     msg->data_length,
                                     &msg->client_addr,
                                     msg->dest_addr_valid ? &msg->dest_addr : NULL);
  }

  return false;
}

static int32_t _udp_ensure_socket(uint16_t port)
{
  uint32_t i;

  for (i = 0U; i < APP_UDP_SERVER_MAX_SOCKETS; i++) {
    if (udp_sockets[i].used && (udp_sockets[i].local_port == port)) {
      return udp_sockets[i].sockid;
    }
  }

  for (i = 0U; i < APP_UDP_SERVER_MAX_SOCKETS; i++) {
    if (!udp_sockets[i].used) {
      if (_udp_open_socket(&udp_sockets[i], port, _udp_socket_type())) {
        return udp_sockets[i].sockid;
      }
      return SOCKET_INVALID_ID;
    }
  }

  printfBoth("No free UDP socket slot for port %u\n", (unsigned int)port);
  return SOCKET_INVALID_ID;
}

static bool _udp_open_socket(udp_socket_entry_t *entry,
                             uint16_t port,
                             int32_t socket_type)
{
  if (entry == NULL) {
    return false;
  }

  memset(entry, 0, sizeof(*entry));
  entry->sockid = socket(AF_INET6, socket_type, IPPROTO_UDP);
  printfBoth("UDP port %u sockid %ld\n",
             (unsigned int)port,
             (long)entry->sockid);
  assert_res(entry->sockid, "UDP socket()");

  if (entry->sockid == SOCKET_INVALID_ID) {
    return false;
  }

  entry->addr.sin6_family = AF_INET6;
  entry->addr.sin6_addr = in6addr_any;
  entry->addr.sin6_port = htons(port);

  udp_r = bind(entry->sockid, (const struct sockaddr *)&entry->addr, sizeof(entry->addr));
  assert_res(udp_r, "UDP bind()");
  if (udp_r == SOCKET_RETVAL_ERROR) {
    return false;
  }

  if (!_udp_enable_pktinfo(entry->sockid)) {
    return false;
  }

  entry->used = true;
  entry->local_port = port;
  _udp_print_waiting(&entry->addr, port);

  return true;
}

static bool _udp_enable_pktinfo(int32_t sockid)
{
  int enabled = 1;

  udp_r = setsockopt(sockid,
                     IPPROTO_IPV6,
                     IPV6_RECVPKTINFO,
                     &enabled,
                     (socklen_t)sizeof(enabled));
  assert_res(udp_r, "UDP setsockopt(IPV6_RECVPKTINFO)");

  return (udp_r != SOCKET_RETVAL_ERROR);
}

static int32_t _udp_socket_type(void)
{
#if (WITH_UDP_SERVER == SO_EVENT_MODE)
  return SOCK_DGRAM | SOCK_NONBLOCK;
#elif (WITH_UDP_SERVER == SO_NONBLOCK)
  return SOCK_DGRAM | SOCK_NONBLOCK;
#else
  #error "Unsupported WITH_UDP_SERVER mode"
#endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */
}

static bool _udp_recv_socket(int32_t sockid, uint16_t local_port, udp_rx_msg_t *msg)
{
  uint8_t control[CMSG_SPACE(sizeof(in6_pktinfo_t))];
  msghdr_t rx_hdr;
  iovec_t iov;
  ssize_t len;

  if ((sockid == SOCKET_INVALID_ID) || (msg == NULL)) {
    return false;
  }

  memset(control, 0, sizeof(control));
  memset(&rx_hdr, 0, sizeof(rx_hdr));
  memset(&iov, 0, sizeof(iov));

  iov.iov_base = msg->buff;
  iov.iov_len = sizeof(msg->buff);
  rx_hdr.msg_name = &msg->client_addr;
  rx_hdr.msg_namelen = sizeof(msg->client_addr);
  rx_hdr.msg_iov = &iov;
  rx_hdr.msg_iovlen = 1;
  rx_hdr.msg_control = control;
  rx_hdr.msg_controllen = sizeof(control);

  len = recvmsg(sockid, &rx_hdr, 0);

  if (len < 0) {
    return false;
  }

  msg->sockid = sockid;
  msg->local_port = local_port;
  msg->data_length = (uint16_t)len;
  msg->addr_len = rx_hdr.msg_namelen;
  msg->dest_addr_valid = _udp_extract_destination(&rx_hdr,
                                                  local_port,
                                                  &msg->dest_addr);

  return true;
}

static bool _udp_extract_destination(const msghdr_t *rx_msg,
                                     uint16_t local_port,
                                     sockaddr_in6_t *dst_addr)
{
  cmsghdr_t *cmsg;
  const in6_pktinfo_t *pktinfo;

  if ((rx_msg == NULL) || (dst_addr == NULL)) {
    return false;
  }

  for (cmsg = CMSG_FIRSTHDR(rx_msg);
       cmsg != NULL;
       cmsg = CMSG_NXTHDR(rx_msg, cmsg)) {
    if ((cmsg->cmsg_level == IPPROTO_IPV6)
        && (cmsg->cmsg_type == IPV6_PKTINFO)
        && (cmsg->cmsg_len >= CMSG_LEN(sizeof(in6_pktinfo_t)))) {
      pktinfo = (const in6_pktinfo_t *)CMSG_DATA(cmsg);
      memset(dst_addr, 0, sizeof(*dst_addr));
      dst_addr->sin6_family = AF_INET6;
      dst_addr->sin6_port = htons(local_port);
      dst_addr->sin6_addr = pktinfo->ipi6_addr;
      dst_addr->sin6_scope_id = (uint32_t)pktinfo->ipi6_ifindex;
      return true;
    }
  }

  return false;
}

static bool _udp_is_rx_sockid(int32_t sockid)
{
  return (_udp_port_from_sockid(sockid) != 0U);
}

static uint16_t _udp_port_from_sockid(int32_t sockid)
{
  uint32_t i;

  for (i = 0U; i < APP_UDP_SERVER_MAX_SOCKETS; i++) {
    if (udp_sockets[i].used && (udp_sockets[i].sockid == sockid)) {
      return udp_sockets[i].local_port;
    }
  }

  return 0U;
}

static bool _udp_prefix_matches(const uint8_t *prefix,
                                uint16_t prefix_len,
                                const uint8_t *payload,
                                uint16_t payload_len)
{
  if ((prefix == NULL) || (payload == NULL)) {
    return false;
  }

  if ((prefix_len == 0U) || (prefix_len > payload_len)) {
    return false;
  }

  return (memcmp(payload, prefix, prefix_len) == 0);
}

static void _udp_print_waiting(sockaddr_in6_t *addr, uint16_t port)
{
  const char *udp_ip_str;

  udp_ip_str = app_wisun_trace_util_get_ip_str((void *)&addr->sin6_addr);
  if (udp_ip_str != NULL) {
    printfBoth("Waiting for UDP messages on %s port %d\n", udp_ip_str, port);
    sl_free((void *)udp_ip_str);
  } else {
    printfBoth("Waiting for UDP messages on <ip conv failed> port %d\n", port);
  }
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
    .priority   = osPriorityAboveNormal,
    .tz_module  = 0U
  };

  if (udp_worker_thread_id != NULL) {
    return true;
  }

  if (udp_rx_pool_id == NULL) {
    udp_rx_pool_id = osMemoryPoolNew(UDP_WORKER_QUEUE_LEN,
                                     sizeof(udp_rx_msg_t),
                                     NULL);
    if (udp_rx_pool_id == NULL) {
      return false;
    }
  }

  if (udp_rx_queue_id == NULL) {
    udp_rx_queue_id = osMessageQueueNew(UDP_WORKER_QUEUE_LEN,
                                        sizeof(udp_rx_msg_t *),
                                        NULL);
    if (udp_rx_queue_id == NULL) {
      return false;
    }
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
  osStatus_t st;

  if (evt == NULL) {
    return;
  }

  if (evt->header.id != SL_WISUN_MSG_SOCKET_DATA_AVAILABLE_IND_ID) {
    return;
  }

  if (!_udp_is_rx_sockid(evt->evt.socket_data_available.socket_id)) {
    return;
  }

  if ((udp_rx_pool_id == NULL) || (udp_rx_queue_id == NULL)) {
    count_udp_drop++;
    return;
  }

  msg = (udp_rx_msg_t *)osMemoryPoolAlloc(udp_rx_pool_id, 0U);
  if (msg == NULL) {
    printfTimeRTT("UDP cstm callback alloc fail\n");
    count_udp_drop++;
    return;
  }

  memset(msg, 0, sizeof(*msg));
  if (!_udp_recv_socket(evt->evt.socket_data_available.socket_id,
                        _udp_port_from_sockid(evt->evt.socket_data_available.socket_id),
                        msg)) {
    (void)osMemoryPoolFree(udp_rx_pool_id, msg);
    return;
  }

  st = osMessageQueuePut(udp_rx_queue_id, &msg, 0U, 0U);
  if (st != osOK) {
    count_udp_drop++;
    (void)osMemoryPoolFree(udp_rx_pool_id, msg);
  }
}

static void _udp_worker_task(void *argument)
{
  udp_rx_msg_t *msg = NULL;
  int32_t sockid;
  uint16_t local_port;

  (void)argument;

  for (;;) {
    if (osMessageQueueGet(udp_rx_queue_id, &msg, NULL, osWaitForever) != osOK) {
      continue;
    }

    if (msg == NULL) {
      continue;
    }

    if ((uint32_t)msg->data_length > sizeof(msg->buff)) {
      (void)osMemoryPoolFree(udp_rx_pool_id, msg);
      continue;
    }

    sockid = msg->sockid;
    local_port = msg->local_port;
    _udp_handle_rx_payload(msg);

    (void)osMemoryPoolFree(udp_rx_pool_id, msg);
    msg = NULL;

    //if (osMessageQueueGetCount(udp_rx_queue_id) == 0U) {
      _udp_drain_socket(sockid, local_port);
    //}
  }
}

static void _udp_drain_socket(int32_t sockid, uint16_t local_port)
{
  udp_rx_msg_t *msg;

  if ((sockid == SOCKET_INVALID_ID)
      || (local_port == 0U)
      || (udp_rx_pool_id == NULL)) {
    return;
  }

  for (;;) {
    msg = (udp_rx_msg_t *)osMemoryPoolAlloc(udp_rx_pool_id, 0U);
    if (msg == NULL) {
      count_udp_drop++;
      return;
    }

    memset(msg, 0, sizeof(*msg));
    if (!_udp_recv_socket(sockid, local_port, msg)) {
      (void)osMemoryPoolFree(udp_rx_pool_id, msg);
      return;
    }

    _udp_handle_rx_payload(msg);
    (void)osMemoryPoolFree(udp_rx_pool_id, msg);
  }
}

#endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */

#endif /* WITH_UDP_SERVER */
