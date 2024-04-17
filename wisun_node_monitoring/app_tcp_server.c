/***************************************************************************//**
* @file app_tcp_server.c
* @brief TCP server for a Wi-SUN Node
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

#include "app_tcp_server.h"

#ifdef WITH_TCP_SERVER

#include <stdio.h>
#include <assert.h>
#include "sl_wisun_api.h"
#include "sl_wisun_version.h"
#include "sl_string.h"
#include "sl_wisun_app_core_util.h"
#include "sl_wisun_trace_util.h"

#include "app_timestamp.h"
#include "app_rtt_traces.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

  #define SL_WISUN_TCP_SERVER_PORT_DEFAULT            4444
  #define SL_WISUN_TCP_SERVER_BUFF_SIZE               1232

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------

void _tcp_custom_callback(sl_wisun_evt_t *evt);

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------

static char tcp_buff[SL_WISUN_TCP_SERVER_BUFF_SIZE] = { 0U };
static sockaddr_in6_t tcp_server_addr           = { 0U };
static sockaddr_in6_t tcp_client_addr           = { 0U };
socklen_t tcp_addr_len                          = sizeof(sockaddr_in6_t);
int32_t tcp_server_sockid                       = SOCKET_INVALID_ID;
int32_t tcp_client_sockid                       = SOCKET_INVALID_ID;
#if       (WITH_TCP_SERVER == SO_EVENT_MODE)
int32_t tcp_received_sockid                     = SOCKET_INVALID_ID;
#endif /* (WITH_TCP_SERVER == SO_EVENT_MODE) */
int32_t tcp_r                                   = SOCKET_RETVAL_ERROR;
bool tcp_client_connected                       = false;
const char *tcp_ip_str                          = NULL;
uint32_t count_tcp_rx                           = 0;
uint16_t tcp_server_port                        = SL_WISUN_TCP_SERVER_PORT_DEFAULT;
bool tcp_socket_data_received                   = false;
uint16_t tcp_data_length;

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------

/* TCP Server initialization function, to be called once connected */
void init_tcp_server(void) {
  int32_t socket_type;

  #if       (WITH_TCP_SERVER == SO_EVENT_MODE)
    // Open TCP server socket in blocking (event) mode
    socket_type = SOCK_STREAM;
    printfBothTime("tcp_server in blocking/Event mode\n");
  #endif /* (WITH_TCP_SERVER == SO_EVENT_MODE) */

  #if       (WITH_TCP_SERVER == SO_NONBLOCK)
    // Open TCP server socket in non-blocking (polling) mode
    socket_type = SOCK_STREAM|SOCK_NONBLOCK;
    printfBothTime("udp_server in non-blocking/Polling mode\n");
  #endif /* (WITH_TCP_SERVER == SO_NONBLOCK) */

  tcp_server_sockid = socket(AF_INET6, socket_type, IPPROTO_TCP);
  printfBothTime("tcp_server_sockid %ld\n", tcp_server_sockid);
  assert_res(tcp_server_sockid, "TCP server socket()");

  // Fill the TCP server address structure
  tcp_server_addr.sin6_family = AF_INET6;
  tcp_server_addr.sin6_addr = in6addr_any;
  tcp_server_addr.sin6_port = htons(tcp_server_port);

  // Bind TCP server address to the socket
  tcp_r = bind(tcp_server_sockid, (const struct sockaddr *) &tcp_server_addr, tcp_addr_len);
  assert_res(tcp_r, "TCP server bind()");

#if       (WITH_TCP_SERVER == SO_EVENT_MODE)
  // Register the TCP Server callback function with the event manager (aka 'em')
  tcp_r = app_wisun_em_custom_callback_register(SL_WISUN_MSG_SOCKET_CONNECTION_AVAILABLE_IND_ID, _tcp_custom_callback);
  assert_res(tcp_r, "_tcp_custom_callback() will be called on SL_WISUN_MSG_SOCKET_CONNECTION_AVAILABLE_IND_ID ");

  tcp_r = app_wisun_em_custom_callback_register(SL_WISUN_MSG_SOCKET_DATA_AVAILABLE_IND_ID, _tcp_custom_callback);
  assert_res(tcp_r, "_tcp_custom_callback() will be called on SL_WISUN_MSG_SOCKET_DATA_AVAILABLE_IND_ID ");
#endif /* (WITH_TCP_SERVER == SO_EVENT_MODE) */

  // Listen on TCP server socket
  tcp_r = listen(tcp_server_sockid, 0);
  assert_res(tcp_r, "TCP server listen()");
  tcp_ip_str = app_wisun_trace_util_get_ip_str((void *) &tcp_server_addr.sin6_addr);
  printfBothTime("Waiting for TCP connection requests on %s port %d\n", tcp_ip_str, tcp_server_port);
  app_wisun_trace_util_destroy_ip_str(tcp_ip_str);
}

void check_tcp_server_messages(void) {
  #if       (WITH_TCP_SERVER == SO_EVENT_MODE)
    if (count_tcp_rx == 0) {printfBothTime("TCP rx in Event mode\n"); count_tcp_rx++;}
    if (tcp_socket_data_received) {
      count_tcp_rx++;
      // Make sure the last byte is 0x00 (end of string).
      tcp_buff[tcp_data_length] = 0;
      tcp_ip_str = app_wisun_trace_util_get_ip_str((void *) &tcp_client_addr.sin6_addr);
      // Print the received message
      printfBothTime("TCP Rx %2ld from %s (%d bytes): %s\n", count_tcp_rx, tcp_ip_str, tcp_data_length, tcp_buff);
      app_wisun_trace_util_destroy_ip_str(tcp_ip_str);
      tcp_socket_data_received = false;
    }
  #endif /* (WITH_TCP_SERVER == SO_EVENT_MODE) */

  #if       (WITH_TCP_SERVER == SO_NONBLOCK)
    // 'Poll Check' for connected TCP clients. The socket must be in non-blocking mode
    //  otherwise accept() will wait forever
    app_set_trace(SL_WISUN_TRACE_GROUP_SOCK   , SL_WISUN_TRACE_LEVEL_ERROR, false);
    tcp_client_sockid = accept(tcp_server_sockid, (struct sockaddr *)&tcp_client_addr, &tcp_addr_len);
    app_set_trace(SL_WISUN_TRACE_GROUP_SOCK   , SL_WISUN_TRACE_LEVEL_INFO, false);
    if (tcp_client_sockid != SOCKET_INVALID_ID) {
      tcp_client_connected = true;
      //printfBothTime("TCP client connected to server with client socket id [%ld]\n", tcp_client_sockid);
    }
    while (tcp_client_connected) {
      // Receiver loop. The socket must be in non-blocking mode
      //  otherwise recv() will wait forever
      tcp_r = recv(tcp_client_sockid, tcp_buff, SL_WISUN_TCP_SERVER_BUFF_SIZE - 1, 0);
      switch (tcp_r) {
        case -1:
          sl_wisun_app_core_util_dispatch_thread();
          continue;
        case 0: // EOF: TCP socket closed
          //printfBothTime("[Closing TCP client socket : %ld]\n", tcp_client_sockid);
          close(tcp_client_sockid);
          tcp_client_connected = false;
          break;
        default: // default behavior: print received message
          count_tcp_rx++;
          // Make sure the last byte is 0x00 (end of string)
          tcp_buff[tcp_r] = 0;
          tcp_ip_str = app_wisun_trace_util_get_ip_str((void *) &tcp_client_addr.sin6_addr);
          // Print the received message
          printfBothTime("TCP Rx %2ld from %s (%ld bytes on client socket %ld): %s\n", count_tcp_rx, tcp_ip_str, tcp_r, tcp_client_sockid, tcp_buff);
          app_wisun_trace_util_destroy_ip_str(tcp_ip_str);
          break;
      }
    }
  #endif /* (WITH_TCP_SERVER == SO_NONBLOCK) */
}

void _tcp_custom_callback(sl_wisun_evt_t *evt) {
  #if       (WITH_TCP_SERVER == SO_EVENT_MODE)
    if (evt->header.id == SL_WISUN_MSG_SOCKET_CONNECTION_AVAILABLE_IND_ID) {
        tcp_client_sockid = accept(tcp_server_sockid, (struct sockaddr *)&tcp_client_addr, &tcp_addr_len);
        tcp_ip_str = app_wisun_trace_util_get_ip_str((void *) &tcp_client_addr.sin6_addr);
    }
    if (evt->header.id == SL_WISUN_MSG_SOCKET_DATA_AVAILABLE_IND_ID) {
        tcp_received_sockid = evt->evt.socket_data_available.socket_id;
      if (tcp_client_sockid == tcp_received_sockid) {
        tcp_data_length = recv(tcp_client_sockid, tcp_buff, SL_WISUN_TCP_SERVER_BUFF_SIZE - 1, 0);
        tcp_socket_data_received = (tcp_data_length > 0);
        if (tcp_data_length == 0) {
          tcp_r = close(tcp_client_sockid);
        }
      } else {
        // printfBothTime("socket_data_received on socket %ld: not for the TCP server\n", evt->evt.socket_data_available.socket_id);
      }
    }
    return;
  #endif /* (WITH_TCP_SERVER == SO_EVENT_MODE) */

  #if       (WITH_TCP_SERVER == SO_NONBLOCK)
    if (evt->header.id == SL_WISUN_MSG_SOCKET_DATA_IND_ID) {
      tcp_data_length = evt->evt.socket_data.data_length;
      if (evt->evt.socket_data.remote_port == tcp_server_port) {
        tcp_socket_data_received = (tcp_data_length > 0);
        tcp_client_sockid = evt->evt.socket_data.socket_id;
        ip6tos(&tcp_client_addr, (char*)evt->evt.socket_data.remote_address.address);
        sl_strcpy_s(tcp_buff, tcp_data_length, (char*)evt->evt.socket_data.data);
      }
    }
    return;
  #endif /* (WITH_TCP_SERVER == SO_NONBLOCK) */

  // Un-managed Indications for which the callback is registered
  printfBothTime("_tcp_data_received_custom_callback() indication id 0x%02x (not managed)\n", evt->header.id);
}

#endif /* WITH_TCP_SERVER */
