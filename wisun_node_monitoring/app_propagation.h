/***************************************************************************//**
 * @file app_propagation.h
 * @brief Generic UDP propagation for Wi-SUN FFN routers
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 ******************************************************************************/

#ifndef APP_PROPAGATION_H
#define APP_PROPAGATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "socket/socket.h"

#ifndef APP_PROPAGATION_PORT
#define APP_PROPAGATION_PORT 7778U
#endif

#ifndef APP_PROPAGATION_DISCRIMINATOR_MAX_LEN
#define APP_PROPAGATION_DISCRIMINATOR_MAX_LEN 32U
#endif

#ifndef APP_PROPAGATION_PAYLOAD_MAX_LEN
#define APP_PROPAGATION_PAYLOAD_MAX_LEN 1200U
#endif

#ifndef APP_PROPAGATION_FRAME_MAX_LEN
#define APP_PROPAGATION_FRAME_MAX_LEN 1232U
#endif

typedef enum {
  APP_PROPAGATION_IND_CON = 0,
  APP_PROPAGATION_IND_NON
} app_propagation_indication_t;

#define APP_PROPAGATION_ALGO_UNICAST          0U
#define APP_PROPAGATION_ALGO_BROADCAST_MIXED  1U

typedef char *(*app_propagation_callback_t)(app_propagation_indication_t indication,
                                            const uint8_t *payload,
                                            uint16_t payload_len);

bool app_propagation_register(const char *discriminator,
                              app_propagation_callback_t callback);

bool app_propagation_init(int32_t udp_sockid, uint16_t udp_port);

bool app_propagation_handle_udp_payload(const uint8_t *payload,
                                        uint16_t payload_len,
                                        const sockaddr_in6_t *src_addr,
                                        const sockaddr_in6_t *dst_addr);

#ifdef __cplusplus
}
#endif

#endif /* APP_PROPAGATION_H */
