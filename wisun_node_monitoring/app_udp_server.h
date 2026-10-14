/***************************************************************************//**
* @file app_udp_server.h
 * @brief Prefix-dispatch UDP server API for the Wi-SUN node.
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
#ifndef   APP_UDP_SERVER_H
#define   APP_UDP_SERVER_H
// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stdbool.h>
#include <stdint.h>

#include "socket/socket.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

// UDP server need to be set to either SO_NONBLOCK or SO_EVENT_MODE, if defined
// Comment the lines if the corresponding server is not required
// NB: Do NOT use SO_NONBLOCK for low-Power use, as it keeps the server loop active!
#define WITH_UDP_SERVER    SO_EVENT_MODE

#ifndef APP_UDP_SERVER_PORT_DEFAULT
#define APP_UDP_SERVER_PORT_DEFAULT      7777U
#endif

#ifndef APP_UDP_SERVER_MAX_SOCKETS
#define APP_UDP_SERVER_MAX_SOCKETS       4U
#endif

#ifndef APP_UDP_SERVER_MAX_CALLBACKS
#define APP_UDP_SERVER_MAX_CALLBACKS     8U
#endif

#ifndef APP_UDP_SERVER_PREFIX_MAX_LEN
#define APP_UDP_SERVER_PREFIX_MAX_LEN    16U
#endif

/**
 * Handler invoked for a received UDP datagram whose local port and payload
 * prefix match a registration.
 *
 * In event mode, handlers run in the UDP worker thread. They must return
 * promptly and must not retain @p payload, @p src_addr, or @p dst_addr after
 * returning. @p dst_addr is NULL when the stack does not provide packet-info.
 *
 * @return true when the datagram was handled; false otherwise.
 */
typedef bool (*app_udp_server_rx_callback_t)(int32_t sockid,
                                             const uint8_t *payload,
                                             uint16_t payload_len,
                                             const sockaddr_in6_t *src_addr,
                                             const sockaddr_in6_t *dst_addr);

// -----------------------------------------------------------------------------
//                          Public Function Declarations
// -----------------------------------------------------------------------------

/**
 * Initialise the UDP server once during application startup, before registering
 * any prefix handlers.
 *
 * With SO_EVENT_MODE, this registers a socket-data event handler and starts
 * the worker thread; no application polling is required. With SO_NONBLOCK,
 * the application must call check_udp_server_messages().
 */
void init_udp_server(void);

/**
 * Poll registered sockets in SO_NONBLOCK mode.
 *
 * This function is intentionally a no-op in SO_EVENT_MODE, where received
 * datagrams are dispatched by the socket-data event callback.
 */
void check_udp_server_messages(void);

/**
 * Register a payload-prefix handler on a UDP port.
 *
 * A socket is opened and bound for a previously unused port. Re-registering
 * the same port/prefix replaces its callback. The function returns the socket
 * id on success or SOCKET_INVALID_ID for invalid input, capacity exhaustion,
 * or a socket/bind failure.
 *
 * @param port       Local UDP port; must not be zero.
 * @param prefix     First bytes required for dispatch.
 * @param prefix_len Length of @p prefix, from 1 through
 *                   APP_UDP_SERVER_PREFIX_MAX_LEN.
 * @param callback   Handler to invoke for a matching datagram.
 * @return Bound socket id, or SOCKET_INVALID_ID on failure.
 */
int32_t app_udp_server_register(uint16_t port,
                                const uint8_t *prefix,
                                uint16_t prefix_len,
                                app_udp_server_rx_callback_t callback);

#endif /* APP_UDP_SERVER_H */
