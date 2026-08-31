/***************************************************************************//**
* @file app_udp_server.h
* @brief UDP server Header file
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

typedef bool (*app_udp_server_rx_callback_t)(int32_t sockid,
                                             const uint8_t *payload,
                                             uint16_t payload_len,
                                             const sockaddr_in6_t *src_addr,
                                             const sockaddr_in6_t *dst_addr);

// -----------------------------------------------------------------------------
//                          Public Function Declarations
// -----------------------------------------------------------------------------

/* UDP Server initialization function, to be called once connected */
void init_udp_server(void);

/* UDP Server reception function, to be called from time to time */
void check_udp_server_messages(void);

int32_t app_udp_server_register(uint16_t port,
                                const uint8_t *prefix,
                                uint16_t prefix_len,
                                app_udp_server_rx_callback_t callback);

#endif /* APP_UDP_SERVER_H */
