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
* This code has not been formally tested and is provided as-is.  It is not suitable for production environments.
* This code will not be maintained.
*
******************************************************************************/
// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------

#include "app_udp_server.h"

#ifdef WITH_UDP_SERVER

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

#define SL_WISUN_UDP_SERVER_PORT_DEFAULT            7777
#define SL_WISUN_UDP_SERVER_BUFF_SIZE               1232

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------

void _udp_custom_callback(sl_wisun_evt_t *evt);

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------

static char udp_buff[SL_WISUN_UDP_SERVER_BUFF_SIZE] = { 0U };
static sockaddr_in6_t udp_server_addr           = { 0U };
static sockaddr_in6_t udp_client_addr           = { 0U };
socklen_t udp_addr_len                          = sizeof(sockaddr_in6_t);
int32_t udp_server_sockid                       = SOCKET_INVALID_ID;
int32_t udp_client_sockid                       = SOCKET_INVALID_ID;
#if       (WITH_UDP_SERVER == SO_EVENT_MODE)
int32_t udp_received_sockid                     = SOCKET_INVALID_ID;
#endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */
int32_t udp_r                                   = SOCKET_RETVAL_ERROR;
const char *udp_ip_str                          = NULL;
uint32_t count_udp_rx                           = 0;
uint16_t udp_server_port                        = SL_WISUN_UDP_SERVER_PORT_DEFAULT;
bool   udp_socket_data_received                 = false;
uint8_t _udp_socket_mode;
uint16_t udp_data_length;

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------

/* UDP Server initialization function, to be called once connected */
void init_udp_server(void) {
  int32_t socket_type;

  #if       (WITH_UDP_SERVER == SO_EVENT_MODE)
      // Open UDP server socket in blocking (event) mode
      socket_type = SOCK_DGRAM;
      printfBothTime("udp_server in blocking/Event mode\n");
  #endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */

  #if       (WITH_UDP_SERVER == SO_NONBLOCK)
    // Open UDP server socket in non-blocking (polling) mode
    socket_type = SOCK_DGRAM|SOCK_NONBLOCK;
    printfBothTime("udp_server in non-blocking/Polling mode\n");
  #endif /* (WITH_UDP_SERVER == SO_NONBLOCK) */

  udp_server_sockid = socket(AF_INET6, socket_type, IPPROTO_UDP);
  printfBothTime("udp_server_sockid %ld\n", udp_server_sockid);
  assert_res(udp_server_sockid, "UDP server socket()");

  // Fill the UDP server address structure
  udp_server_addr.sin6_family = AF_INET6;
  udp_server_addr.sin6_addr = in6addr_any;
  udp_server_addr.sin6_port = htons(udp_server_port);

  // Bind UDP server address to the socket
  udp_r = bind(udp_server_sockid, (const struct sockaddr *) &udp_server_addr, udp_addr_len);
  assert_res(udp_r, "UDP server bind()");

  #if       (WITH_UDP_SERVER == SO_EVENT_MODE)
    // Register the UDP_Server_Rx event callback function with the event manager (aka 'em')
    app_wisun_em_custom_callback_register(SL_WISUN_MSG_SOCKET_DATA_IND_ID, _udp_custom_callback);
    assert_res(udp_r, "_udp_custom_callback() will be called on SL_WISUN_MSG_SOCKET_CONNECTION_AVAILABLE_IND_ID ");
  #endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */

  udp_ip_str = app_wisun_trace_util_get_ip_str((void *) &udp_server_addr.sin6_addr);
  printfBothTime("Waiting for UDP messages on %s port %d\n", udp_ip_str, udp_server_port);
  app_wisun_trace_util_destroy_ip_str(udp_ip_str);
}

void check_udp_server_messages(void) {
  #if       (WITH_UDP_SERVER == SO_EVENT_MODE)
    if (count_udp_rx == 0) {printfBothTime("UDP rx in Event mode\n"); count_udp_rx++;}
    if (udp_socket_data_received) {
      printfBothTime("UDP rx in Event mode %ld\n", count_udp_rx);
      count_udp_rx++;
      // Make sure the last byte is 0x00 (end of string).
      udp_buff[udp_data_length] = 0;
      udp_ip_str = app_wisun_trace_util_get_ip_str((void *) &udp_client_addr.sin6_addr);
      // Print the received message
      printfBothTime("UDP Rx %2ld from %s (%d bytes): %s\n", count_udp_rx, udp_ip_str, udp_data_length, udp_buff);
      app_wisun_trace_util_destroy_ip_str(udp_ip_str);
      udp_socket_data_received = false;
    }
  #endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */

  #if       (WITH_UDP_SERVER == SO_NONBLOCK)
    if (count_udp_rx == 0) {printfBothTime("UDP rx in Polling mode\n"); count_udp_rx++;}
    // 'Poll Check' for received UDP messages. The socket must be in non-blocking mode
    //  otherwise recv() will wait forever
    udp_r = recvfrom(udp_server_sockid, udp_buff, SL_WISUN_UDP_SERVER_BUFF_SIZE - 1, 0, (struct sockaddr *) &udp_client_addr, &udp_addr_len);
    if (udp_r > 0) {
        count_udp_rx++;
        // Make sure the last byte is 0x00 (end of string).
        udp_buff[udp_r] = 0;
        udp_ip_str = app_wisun_trace_util_get_ip_str((void *) &udp_client_addr.sin6_addr);
        // Print the received message
        printfBothTime("UDP Rx %2ld from %s (%ld bytes): %s\n", count_udp_rx, udp_ip_str, udp_r, udp_buff);
        app_wisun_trace_util_destroy_ip_str(udp_ip_str);
    }
  #endif /* (WITH_UDP_SERVER == SO_NONBLOCK) */
}

void _udp_custom_callback(sl_wisun_evt_t *evt) {
  #if       (WITH_UDP_SERVER == SO_EVENT_MODE)
    if (evt->header.id == SL_WISUN_MSG_SOCKET_CONNECTION_AVAILABLE_IND_ID) {
        udp_client_sockid = accept(udp_server_sockid, (struct sockaddr *)&udp_client_addr, &udp_addr_len);
        udp_ip_str = app_wisun_trace_util_get_ip_str((void *) &udp_client_addr.sin6_addr);
        printfBothTime("UDP client connection available from %s on client socket id %ld\n",
                       udp_ip_str,
                       udp_client_sockid);
    }
    if (evt->header.id == SL_WISUN_MSG_SOCKET_DATA_AVAILABLE_IND_ID) {
        udp_received_sockid = evt->evt.socket_data_available.socket_id;
      if (udp_client_sockid == udp_received_sockid) {
        udp_data_length = recv(udp_client_sockid, udp_buff, SL_WISUN_UDP_SERVER_BUFF_SIZE - 1, 0);
        udp_socket_data_received = (udp_data_length > 0);
        if (udp_data_length == 0) {
          udp_r = close(udp_client_sockid);
        }
      } else {
          printfBothTime("socket_data_received on socket %ld: not for the UDP server\n", evt->evt.socket_data_available.socket_id);
      }
    }
    return;
  #endif /* (WITH_UDP_SERVER == SO_EVENT_MODE) */

  #if       (WITH_UDP_SERVER == SO_NONBLOCK)
    if (evt->header.id == SL_WISUN_MSG_SOCKET_DATA_IND_ID) {
      udp_data_length = evt->evt.socket_data.data_length;
        udp_socket_data_received = (udp_data_length > 0);
        udp_client_sockid = evt->evt.socket_data.socket_id;
        ip6tos(&udp_client_addr, (char*)evt->evt.socket_data.remote_address.address);
        sl_strcpy_s(udp_buff, udp_data_length, (char*)evt->evt.socket_data.data);
    }
    return;
  #endif /* (WITH_UDP_SERVER == SO_NONBLOCK) */

  // Un-managed Indications for which the callback is registered
  printfBothTime("_udp_data_received_custom_callback() indication id 0x%02x (not managed)\n", evt->header.id);
}

#endif /* WITH_UDP_SERVER */
