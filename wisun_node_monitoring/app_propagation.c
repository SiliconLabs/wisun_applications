/***************************************************************************//**
 * @file app_propagation.c
 * @brief Generic UDP propagation for Wi-SUN FFN routers
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 ******************************************************************************/

#include "app_propagation.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmsis_os2.h"
#include "sl_memory_manager.h"
#include "sl_wisun_api.h"
#include "sl_wisun_trace_util.h"

#include "app.h"

#ifndef APP_PROPAGATION_MAX_CHILDREN
#define APP_PROPAGATION_MAX_CHILDREN             22U
#endif

#ifndef APP_PROPAGATION_RX_QUEUE_LEN
#define APP_PROPAGATION_RX_QUEUE_LEN             4U
#endif

#ifndef APP_PROPAGATION_WORKER_STACK_SIZE_BYTES
#define APP_PROPAGATION_WORKER_STACK_SIZE_BYTES  8192U
#endif

#ifndef APP_PROPAGATION_OPTION_MAX_LEN
#define APP_PROPAGATION_OPTION_MAX_LEN           512U
#endif

#ifndef APP_PROPAGATION_FRAME_MAX_LEN
#define APP_PROPAGATION_FRAME_MAX_LEN            1232U
#endif

#ifndef APP_PROPAGATION_REGISTRY_LEN
#define APP_PROPAGATION_REGISTRY_LEN             8U
#endif

#ifndef APP_PROPAGATION_SEQUENCE_CACHE_LEN
#define APP_PROPAGATION_SEQUENCE_CACHE_LEN       16U
#endif

#ifndef APP_PROPAGATION_COMPLETED_SEQUENCE_CACHE_LEN
#define APP_PROPAGATION_COMPLETED_SEQUENCE_CACHE_LEN 4U
#endif

#ifndef APP_PROPAGATION_COMPLETED_SEQUENCE_DROP_MS
#define APP_PROPAGATION_COMPLETED_SEQUENCE_DROP_MS 600000U
#endif

#ifndef APP_PROPAGATION_EARLY_CONF_GROUP_LEN
#define APP_PROPAGATION_EARLY_CONF_GROUP_LEN    2U
#endif

#ifndef APP_PROPAGATION_EARLY_CONF_CHILD_LEN
#define APP_PROPAGATION_EARLY_CONF_CHILD_LEN    APP_PROPAGATION_MAX_CHILDREN
#endif

#ifndef APP_PROPAGATION_SEND_RETRY_COUNT
#define APP_PROPAGATION_SEND_RETRY_COUNT         9U
#endif

#ifndef APP_PROPAGATION_SEND_RETRY_DELAY_MS
#define APP_PROPAGATION_SEND_RETRY_DELAY_MS      100U
#endif

#ifndef APP_PROPAGATION_CHILD_SEND_GAP_MS
#define APP_PROPAGATION_CHILD_SEND_GAP_MS        20U
#endif

#ifndef APP_PROPAGATION_BROADCAST_CHILD_THRESHOLD
#define APP_PROPAGATION_BROADCAST_CHILD_THRESHOLD 10U
#endif

#ifndef APP_PROPAGATION_BROADCAST_WAIT_MS
#define APP_PROPAGATION_BROADCAST_WAIT_MS        200U
#endif

#ifndef APP_PROPAGATION_TIMEOUT_PER_HOP_MARGIN_MS
#define APP_PROPAGATION_TIMEOUT_PER_HOP_MARGIN_MS 1000U
#endif

#ifndef APP_PROPAGATION_TIMEOUT_MIN_MS
#define APP_PROPAGATION_TIMEOUT_MIN_MS           1000U
#endif

#define APP_PROPAGATION_PREFIX                   "PROPAG"
#define APP_PROPAGATION_PREFIX_LEN               6U
#define APP_PROPAGATION_MISSING_PREFIX           "MISS="
#define APP_PROPAGATION_TRUNCATED_TOKEN          "TRUNC"
#define APP_PROPAGATION_DEFAULT_CONF_PAYLOAD     "OK"
#define APP_PROPAGATION_DISCRIMINATOR_FIELD_LEN  ((int)(APP_PROPAGATION_DISCRIMINATOR_MAX_LEN - 1U))
#define APP_PROPAGATION_PAYLOAD_FIELD_LEN        ((int)(APP_PROPAGATION_PAYLOAD_MAX_LEN - 1U))
#define APP_PROPAGATION_OPTION_FIELD_LEN         ((int)(APP_PROPAGATION_OPTION_MAX_LEN - 1U))
#define APP_PROPAGATION_DISCRIMINATOR_LOG_LEN    APP_PROPAGATION_DISCRIMINATOR_FIELD_LEN
#define APP_PROPAGATION_OPTION_LOG_LEN           160
#define APP_PROPAGATION_CHILD_TAG_LOG_LEN        4

typedef enum {
  APP_PROPAGATION_FRAME_INVALID = 0,
  APP_PROPAGATION_FRAME_IND_CON,
  APP_PROPAGATION_FRAME_IND_NON,
  APP_PROPAGATION_FRAME_CONFIRMATION
} app_propagation_frame_type_t;

typedef struct {
  app_propagation_frame_type_t type;
  uint16_t sequence;
  uint8_t algo_selected;
  uint16_t pan_size;
  char discriminator[APP_PROPAGATION_DISCRIMINATOR_MAX_LEN];
  uint8_t payload[APP_PROPAGATION_PAYLOAD_MAX_LEN + 1U];
  uint16_t payload_len;
  uint32_t number;
  bool number_present;
  char option[APP_PROPAGATION_OPTION_MAX_LEN];
} app_propagation_frame_t;

typedef struct {
  uint32_t rx_tick;
  uint16_t payload_len;
  sockaddr_in6_t src_addr;
  sockaddr_in6_t dst_addr;
  bool dst_addr_valid;
  uint8_t payload[APP_PROPAGATION_FRAME_MAX_LEN];
} app_propagation_rx_msg_t;

typedef struct {
  sockaddr_in6_t tx_addr;
  in6_addr_t global_addr;
  in6_addr_t link_local_addr;
  char tag[5];
  bool confirmed;
} app_propagation_child_t;

typedef struct {
  char discriminator[APP_PROPAGATION_DISCRIMINATOR_MAX_LEN];
  uint16_t sequence;
  uint8_t algo_selected;
  uint16_t pan_size;
  uint8_t indication_payload[APP_PROPAGATION_PAYLOAD_MAX_LEN];
  uint16_t indication_payload_len;
  char confirmation_payload[APP_PROPAGATION_PAYLOAD_MAX_LEN];
  uint32_t timeout_ms;
  sockaddr_in6_t parent_addr;
  bool parent_addr_valid;
  app_propagation_child_t children[APP_PROPAGATION_MAX_CHILDREN];
  uint8_t child_count;
  uint8_t confirmed_children;
  uint32_t conf_count_total;
  char missing[APP_PROPAGATION_OPTION_MAX_LEN];
} app_propagation_context_t;

typedef struct {
  const app_propagation_context_t *ctx;
  const char *option;
  uint32_t rx_tick;
  uint32_t timeout_ms;
  uint32_t deadline;
} app_propagation_ind_con_tx_t;

typedef struct {
  bool used;
  char discriminator[APP_PROPAGATION_DISCRIMINATOR_MAX_LEN];
  app_propagation_callback_t callback;
} app_propagation_registry_entry_t;

typedef struct {
  bool used;
  uint16_t sequence;
  char discriminator[APP_PROPAGATION_DISCRIMINATOR_MAX_LEN];
} app_propagation_seen_indication_t;

typedef struct {
  bool used;
  uint16_t sequence;
  uint32_t expires_at;
} app_propagation_completed_sequence_t;

typedef struct {
  bool used;
  in6_addr_t src_addr;
  char tag[5];
  uint32_t count;
} app_propagation_early_child_conf_t;

typedef struct {
  bool used;
  uint16_t sequence;
  char discriminator[APP_PROPAGATION_DISCRIMINATOR_MAX_LEN];
  uint8_t child_count;
  char missing[APP_PROPAGATION_OPTION_MAX_LEN];
  app_propagation_early_child_conf_t children[APP_PROPAGATION_EARLY_CONF_CHILD_LEN];
} app_propagation_early_conf_t;

static int32_t propagation_sockid = SOCKET_INVALID_ID;
static uint16_t propagation_port = APP_PROPAGATION_PORT;
static osThreadId_t propagation_worker_thread_id = NULL;
static osMessageQueueId_t propagation_rx_queue_id = NULL;
static osMemoryPoolId_t propagation_rx_pool_id = NULL;
static app_propagation_registry_entry_t propagation_registry[APP_PROPAGATION_REGISTRY_LEN];
static app_propagation_context_t propagation_context;
static app_propagation_child_t propagation_children_scratch[APP_PROPAGATION_MAX_CHILDREN];
static app_propagation_seen_indication_t propagation_seen_indications[APP_PROPAGATION_SEQUENCE_CACHE_LEN];
static app_propagation_completed_sequence_t propagation_completed_sequences[APP_PROPAGATION_COMPLETED_SEQUENCE_CACHE_LEN];
static app_propagation_early_conf_t propagation_early_confirmations[APP_PROPAGATION_EARLY_CONF_GROUP_LEN];
static uint8_t propagation_seen_indication_next = 0U;
static uint8_t propagation_completed_sequence_next = 0U;
static uint8_t propagation_early_confirmation_next = 0U;
static uint8_t propagation_tx_frame[APP_PROPAGATION_FRAME_MAX_LEN];
static app_propagation_rx_msg_t *propagation_deferred_rx_msg = NULL;

static void _propagation_worker_task(void *argument);
static bool _starts_with_propagation(const uint8_t *payload, uint16_t payload_len);
static bool _parse_frame(const uint8_t *payload,
                         uint16_t payload_len,
                         app_propagation_frame_t *frame);
static bool _parse_frame_header(const uint8_t *payload,
                                uint16_t payload_len,
                                app_propagation_frame_type_t *type,
                                uint16_t *sequence);
static bool _parse_u32_token(const char *token, uint32_t *value);
static bool _next_token(const uint8_t *payload,
                        size_t payload_len,
                        size_t *offset,
                        char *token,
                        size_t token_len);
static bool _consume_payload_separator(const uint8_t *payload,
                                       size_t payload_len,
                                       size_t *offset);
static void _trim_right(char *str);
static void _copy_remaining_option(char *dst,
                                   size_t dst_len,
                                   const uint8_t *payload,
                                   size_t payload_len,
                                   size_t offset);
static void _handle_rx_msg(const app_propagation_rx_msg_t *rx_msg);
static void _handle_indication(const app_propagation_frame_t *frame,
                               uint32_t rx_tick);
static void _handle_confirmable_indication(const app_propagation_frame_t *frame,
                                           const char *confirmation_payload,
                                           uint32_t rx_tick);
static void _handle_non_confirmable_indication(const app_propagation_frame_t *frame);
static bool _handle_wait_rx(app_propagation_context_t *ctx,
                            const app_propagation_rx_msg_t *rx_msg);
static void _handle_confirmation(app_propagation_context_t *ctx,
                                 const app_propagation_frame_t *frame,
                                 const sockaddr_in6_t *src_addr);
static bool _handle_compact_confirmation(app_propagation_context_t *ctx,
                                         const in6_addr_t *src_addr,
                                         const char *src_tag,
                                         uint32_t count,
                                         const char *option,
                                         const uint8_t *payload);
static app_propagation_rx_msg_t *_drain_ready_wait_rx(app_propagation_context_t *ctx);
static app_propagation_rx_msg_t *_wait_for_confirmations_until(app_propagation_context_t *ctx,
                                                               uint32_t deadline);
static bool _already_seen_indication(const app_propagation_frame_t *frame);
static void _remember_indication(const app_propagation_frame_t *frame);
static bool _completed_sequence_active(uint16_t sequence);
static void _remember_completed_sequence(uint16_t sequence);
static bool _should_drop_completed_sequence_frame(const uint8_t *payload,
                                                  uint16_t payload_len);
static void _purge_completed_sequence_from_queue(uint16_t sequence);
static void _finish_confirmable_context(app_propagation_context_t *ctx);
static void _store_early_confirmation(const app_propagation_frame_t *frame,
                                      const sockaddr_in6_t *src_addr);
static void _apply_early_confirmations(app_propagation_context_t *ctx);
static void _clear_early_confirmations(uint16_t sequence);
static app_propagation_early_conf_t *_find_early_confirmation(uint16_t sequence);
static bool _early_confirmation_has_child(const app_propagation_early_conf_t *pending,
                                          const in6_addr_t *src_addr);
static bool _should_defer_indication(const app_propagation_context_t *ctx,
                                     const app_propagation_frame_t *frame);
static bool _confirmation_matches_context(const app_propagation_context_t *ctx,
                                          const app_propagation_frame_t *frame);
static app_propagation_callback_t _find_callback(const char *discriminator);
static void _invoke_indication_callback(const app_propagation_frame_t *frame,
                                        char *confirmation_payload,
                                        size_t confirmation_payload_len);
static uint8_t _collect_children(app_propagation_child_t *children,
                                 uint8_t max_children);
static int _build_indication_frame(uint8_t *dst,
                                   size_t dst_len,
                                   app_propagation_frame_type_t type,
                                   uint16_t sequence,
                                   uint8_t algo_selected,
                                   uint16_t pan_size,
                                   const char *discriminator,
                                   const uint8_t *payload,
                                   uint16_t payload_len,
                                   uint32_t timeout_ms,
                                   const char *option);
static bool _send_linklocal_broadcast(const uint8_t *payload,
                                      size_t payload_len,
                                      const app_propagation_ind_con_tx_t *ind_con_tx);
static bool _send_frame(const sockaddr_in6_t *dest_addr,
                        const uint8_t *payload,
                        size_t payload_len,
                        const app_propagation_ind_con_tx_t *ind_con_tx);
static int _build_ind_con_tx_frame(uint8_t *dst,
                                   size_t dst_len,
                                   const app_propagation_ind_con_tx_t *ind_con_tx);
static void _delay_ms(uint32_t delay_ms);
static void _send_zero_confirmation(const app_propagation_frame_t *frame,
                                    const sockaddr_in6_t *src_addr);
static bool _should_zero_confirm_unicast_indication(const app_propagation_rx_msg_t *rx_msg,
                                                    const app_propagation_frame_t *frame);
static void _send_confirmation_to_parent(app_propagation_context_t *ctx);
static void _append_missing_children(app_propagation_context_t *ctx);
static void _append_missing_option(char *missing, size_t missing_len,
                                   const char *option);
static void _append_missing_token(char *missing, size_t missing_len,
                                  const char *token);
static bool _missing_has_token(const char *missing, const char *token);
static bool _get_primary_parent_addr(sockaddr_in6_t *parent_addr);
static bool _source_is_primary_parent(const sockaddr_in6_t *src_addr);
static bool _source_is_secondary_parent(const sockaddr_in6_t *src_addr);
static int16_t _find_child_by_addr(app_propagation_context_t *ctx,
                                   const in6_addr_t *src_addr);
static bool _ipv6_equal(const in6_addr_t *a, const in6_addr_t *b);
static bool _ipv6_is_multicast(const in6_addr_t *addr);
static bool _ipv6_is_unspecified(const in6_addr_t *addr);
static void _ipv6_last4(const in6_addr_t *addr, char tag[5]);
static uint32_t _adjust_timeout_for_hop(uint32_t timeout_ms);
static uint32_t _child_timeout_ms(uint32_t rx_tick, uint32_t timeout_ms);
static uint32_t _timeout_remaining_ms(uint32_t rx_tick, uint32_t timeout_ms);
static uint32_t _ms_to_ticks(uint32_t ms);
static uint32_t _ticks_to_ms(uint32_t ticks);
static bool _ticks_expired(uint32_t now, uint32_t deadline);

bool app_propagation_register(const char *discriminator,
                              app_propagation_callback_t callback)
{
  uint8_t i;

  if ((discriminator == NULL) || (discriminator[0] == '\0') || (callback == NULL)) {
    return false;
  }

  if (strlen(discriminator) >= APP_PROPAGATION_DISCRIMINATOR_MAX_LEN) {
    return false;
  }

  for (i = 0U; i < APP_PROPAGATION_REGISTRY_LEN; i++) {
    if (propagation_registry[i].used
        && (strcmp(propagation_registry[i].discriminator, discriminator) == 0)) {
      propagation_registry[i].callback = callback;
      return true;
    }
  }

  for (i = 0U; i < APP_PROPAGATION_REGISTRY_LEN; i++) {
    if (!propagation_registry[i].used) {
      propagation_registry[i].used = true;
      propagation_registry[i].callback = callback;
      snprintf(propagation_registry[i].discriminator,
               sizeof(propagation_registry[i].discriminator),
               "%s",
               discriminator);
      return true;
    }
  }

  return false;
}

bool app_propagation_init(int32_t udp_sockid, uint16_t udp_port)
{
  const osThreadAttr_t propagation_worker_attr = {
    .name       = "propag_worker",
    .attr_bits  = osThreadDetached,
    .cb_mem     = NULL,
    .cb_size    = 0U,
    .stack_mem  = NULL,
    .stack_size = APP_PROPAGATION_WORKER_STACK_SIZE_BYTES,
    .priority   = osPriorityNormal,
    .tz_module  = 0U
  };

  if (udp_sockid == SOCKET_INVALID_ID) {
    return false;
  }

  propagation_sockid = udp_sockid;
  propagation_port = udp_port;

  if (propagation_worker_thread_id != NULL) {
    return true;
  }

  propagation_rx_pool_id = osMemoryPoolNew(APP_PROPAGATION_RX_QUEUE_LEN,
                                           sizeof(app_propagation_rx_msg_t),
                                           NULL);
  if (propagation_rx_pool_id == NULL) {
    return false;
  }

  propagation_rx_queue_id = osMessageQueueNew(APP_PROPAGATION_RX_QUEUE_LEN,
                                              sizeof(app_propagation_rx_msg_t *),
                                              NULL);
  if (propagation_rx_queue_id == NULL) {
    return false;
  }

  printfTimeRTT("Propagation UDP port %u, socket %ld\n",
                 (unsigned int)propagation_port,
                 (long)propagation_sockid);

  propagation_worker_thread_id = osThreadNew(_propagation_worker_task,
                                             NULL,
                                             &propagation_worker_attr);
  return (propagation_worker_thread_id != NULL);
}

bool app_propagation_handle_udp_payload(const uint8_t *payload,
                                        uint16_t payload_len,
                                        const sockaddr_in6_t *src_addr,
                                        const sockaddr_in6_t *dst_addr)
{
  app_propagation_rx_msg_t *rx_msg;
  uint16_t copy_len;
  osStatus_t status;

  if ((payload == NULL) || (src_addr == NULL)) {
    return false;
  }

  if (!_starts_with_propagation(payload, payload_len)) {
    return false;
  }

  if (_should_drop_completed_sequence_frame(payload, payload_len)) {
    return true;
  }

  if ((propagation_rx_pool_id == NULL) || (propagation_rx_queue_id == NULL)) {
    printfTimeRTT("Propagation frame dropped: worker not ready\n");
    return true;
  }

  rx_msg = (app_propagation_rx_msg_t *)osMemoryPoolAlloc(propagation_rx_pool_id, 0U);
  if (rx_msg == NULL) {
    printfTimeRTT("Propagation frame dropped: RX pool full\n");
    return true;
  }

  memset(rx_msg, 0, sizeof(*rx_msg));
  copy_len = payload_len;
  if (copy_len > APP_PROPAGATION_FRAME_MAX_LEN) {
    copy_len = APP_PROPAGATION_FRAME_MAX_LEN;
  }

  memcpy(rx_msg->payload, payload, copy_len);
  rx_msg->rx_tick = osKernelGetTickCount();
  rx_msg->payload_len = copy_len;
  rx_msg->src_addr = *src_addr;
  if ((dst_addr != NULL) && !_ipv6_is_unspecified(&dst_addr->sin6_addr)) {
    rx_msg->dst_addr = *dst_addr;
    rx_msg->dst_addr_valid = true;
  }

  status = osMessageQueuePut(propagation_rx_queue_id, &rx_msg, 0U, 0U);
  if (status != osOK) {
    printfTimeRTT("Propagation frame dropped: RX queue full\n");
    (void)osMemoryPoolFree(propagation_rx_pool_id, rx_msg);
  }

  return true;
}

static void _propagation_worker_task(void *argument)
{
  app_propagation_rx_msg_t *rx_msg = NULL;

  (void)argument;

  for (;;) {
    if (osMessageQueueGet(propagation_rx_queue_id, &rx_msg, NULL, osWaitForever) != osOK) {
      continue;
    }

    while (rx_msg != NULL) {
      app_propagation_rx_msg_t *handled_msg = rx_msg;

      rx_msg = NULL;
      propagation_deferred_rx_msg = NULL;
      _handle_rx_msg(handled_msg);
      (void)osMemoryPoolFree(propagation_rx_pool_id, handled_msg);

      if (propagation_deferred_rx_msg != NULL) {
        rx_msg = propagation_deferred_rx_msg;
        propagation_deferred_rx_msg = NULL;
      }
    }
  }
}

static bool _starts_with_propagation(const uint8_t *payload, uint16_t payload_len)
{
  if (payload_len < APP_PROPAGATION_PREFIX_LEN) {
    return false;
  }

  if (memcmp(payload, APP_PROPAGATION_PREFIX, APP_PROPAGATION_PREFIX_LEN) != 0) {
    return false;
  }

  if (payload_len == APP_PROPAGATION_PREFIX_LEN) {
    return true;
  }

  return (payload[APP_PROPAGATION_PREFIX_LEN] == ' ')
         || (payload[APP_PROPAGATION_PREFIX_LEN] == '\t')
         || (payload[APP_PROPAGATION_PREFIX_LEN] == '\r')
         || (payload[APP_PROPAGATION_PREFIX_LEN] == '\n');
}

static void _handle_rx_msg(const app_propagation_rx_msg_t *rx_msg)
{
  app_propagation_frame_t frame;

  if (!_parse_frame(rx_msg->payload, rx_msg->payload_len, &frame)) {
    printfTimeRTT("Propagation frame ignored: parse failed len %u\n",
                   (unsigned int)rx_msg->payload_len);
    return;
  }

  if (_completed_sequence_active(frame.sequence)) {
    return;
  }

  if ((frame.type == APP_PROPAGATION_FRAME_IND_CON)
      || (frame.type == APP_PROPAGATION_FRAME_IND_NON)) {
    if ((frame.type == APP_PROPAGATION_FRAME_IND_CON)
        && _should_zero_confirm_unicast_indication(rx_msg, &frame)) {
      _send_zero_confirmation(&frame, &rx_msg->src_addr);
      return;
    }
    if (_already_seen_indication(&frame)) {
      printfTimeRTT("PROPAG %s '%.*s' seq %u duplicate ignored\n",
                     (frame.type == APP_PROPAGATION_FRAME_IND_CON) ? "IND_CON" : "IND_NON",
                     APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                     frame.discriminator,
                     (unsigned int)frame.sequence);
      return;
    }
    _remember_indication(&frame);
    _handle_indication(&frame, rx_msg->rx_tick);
  } else {
    _store_early_confirmation(&frame, &rx_msg->src_addr);
  }
}

static void _handle_indication(const app_propagation_frame_t *frame,
                               uint32_t rx_tick)
{
  char confirmation_payload[APP_PROPAGATION_PAYLOAD_MAX_LEN];

  confirmation_payload[0] = '\0';
  _invoke_indication_callback(frame,
                              confirmation_payload,
                              sizeof(confirmation_payload));

  if (frame->type == APP_PROPAGATION_FRAME_IND_NON) {
    _handle_non_confirmable_indication(frame);
  } else {
    if (confirmation_payload[0] == '\0') {
      snprintf(confirmation_payload,
               sizeof(confirmation_payload),
               "%s",
               APP_PROPAGATION_DEFAULT_CONF_PAYLOAD);
    }
    _handle_confirmable_indication(frame, confirmation_payload, rx_tick);
  }
}

static void _handle_confirmable_indication(const app_propagation_frame_t *frame,
                                           const char *confirmation_payload,
                                           uint32_t rx_tick)
{
  app_propagation_context_t *ctx = &propagation_context;
  app_propagation_ind_con_tx_t ind_con_tx;
  uint32_t timeout_ticks;
  uint32_t deadline;
  uint32_t now;
  uint32_t remaining_ticks;
  uint32_t broadcast_wait_ticks;
  uint32_t broadcast_deadline;
  uint8_t i;
  app_propagation_rx_msg_t *deferred_rx_msg = NULL;

  memset(ctx, 0, sizeof(*ctx));
  snprintf(ctx->discriminator, sizeof(ctx->discriminator), "%s", frame->discriminator);
  ctx->sequence = frame->sequence;
  ctx->algo_selected = frame->algo_selected;
  ctx->pan_size = frame->pan_size;
  ctx->indication_payload_len = frame->payload_len;
  if (ctx->indication_payload_len > APP_PROPAGATION_PAYLOAD_MAX_LEN) {
    ctx->indication_payload_len = APP_PROPAGATION_PAYLOAD_MAX_LEN;
  }
  memcpy(ctx->indication_payload, frame->payload, ctx->indication_payload_len);
  snprintf(ctx->confirmation_payload,
           sizeof(ctx->confirmation_payload),
           "%s",
           confirmation_payload);
  ctx->timeout_ms = _adjust_timeout_for_hop(frame->number);
  ctx->parent_addr_valid = _get_primary_parent_addr(&ctx->parent_addr);
  if (!ctx->parent_addr_valid) {
    printfTimeRTT("PROPAG IND_CON '%.*s' seq %u: primary parent address unavailable\n",
                   APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                   ctx->discriminator,
                   (unsigned int)ctx->sequence);
  }

  ctx->child_count = _collect_children(ctx->children, APP_PROPAGATION_MAX_CHILDREN);
  _apply_early_confirmations(ctx);

  printfTimeRTT("PROPAG IND_CON '%.*s' seq %u algo %u pan %u payload_len %u: %u direct children, timeout %lu ms (rx %lu ms)\n",
                 APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                 ctx->discriminator,
                 (unsigned int)ctx->sequence,
                 (unsigned int)ctx->algo_selected,
                 (unsigned int)ctx->pan_size,
                 (unsigned int)ctx->indication_payload_len,
                 (unsigned int)ctx->child_count,
                 (unsigned long)ctx->timeout_ms,
                 (unsigned long)frame->number);

  if ((ctx->child_count == 0U) || (ctx->confirmed_children >= ctx->child_count)) {
    _finish_confirmable_context(ctx);
    return;
  }

  timeout_ticks = _ms_to_ticks(ctx->timeout_ms);
  deadline = rx_tick + timeout_ticks;
  if (_ticks_expired(osKernelGetTickCount(), deadline)) {
    _append_missing_children(ctx);
    _finish_confirmable_context(ctx);
    return;
  }

  memset(&ind_con_tx, 0, sizeof(ind_con_tx));
  ind_con_tx.ctx = ctx;
  ind_con_tx.option = frame->option;
  ind_con_tx.rx_tick = rx_tick;
  ind_con_tx.timeout_ms = frame->number;
  ind_con_tx.deadline = deadline;

  if ((ctx->algo_selected == APP_PROPAGATION_ALGO_BROADCAST_MIXED)
      && (ctx->child_count > APP_PROPAGATION_BROADCAST_CHILD_THRESHOLD)) {
    if (_send_linklocal_broadcast(NULL, 0U, &ind_con_tx)) {
      printfTimeRTT("PROPAG IND_CON '%.*s' seq %u: sent link-local broadcast\n",
                     APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                     ctx->discriminator,
                     (unsigned int)ctx->sequence);
      now = osKernelGetTickCount();
      if (!_ticks_expired(now, deadline)) {
        remaining_ticks = deadline - now;
        broadcast_wait_ticks = _ms_to_ticks(APP_PROPAGATION_BROADCAST_WAIT_MS);
        if (broadcast_wait_ticks > remaining_ticks) {
          broadcast_wait_ticks = remaining_ticks;
        }
        if (broadcast_wait_ticks != 0U) {
          broadcast_deadline = now + broadcast_wait_ticks;
          deferred_rx_msg = _wait_for_confirmations_until(ctx, broadcast_deadline);
        }
      }
    } else {
      printfTimeRTT("PROPAG IND_CON '%.*s' seq %u: link-local broadcast send failed\n",
                     APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                     ctx->discriminator,
                     (unsigned int)ctx->sequence);
    }
  }

  if (deferred_rx_msg == NULL) {
    for (i = 0U; i < ctx->child_count; i++) {
      if (ctx->children[i].confirmed) {
        continue;
      }
      if (_ticks_expired(osKernelGetTickCount(), deadline)) {
        break;
      }
      if (!_send_frame(&ctx->children[i].tx_addr, NULL, 0U, &ind_con_tx)) {
        printfTimeRTT("PROPAG IND_CON '%.*s': send failed to child %.*s\n",
                       APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                       ctx->discriminator,
                       APP_PROPAGATION_CHILD_TAG_LOG_LEN,
                       ctx->children[i].tag);
      }
      deferred_rx_msg = _drain_ready_wait_rx(ctx);
      if ((deferred_rx_msg != NULL) || (ctx->confirmed_children >= ctx->child_count)) {
        break;
      }
      _delay_ms(APP_PROPAGATION_CHILD_SEND_GAP_MS);
      deferred_rx_msg = _drain_ready_wait_rx(ctx);
      if ((deferred_rx_msg != NULL) || (ctx->confirmed_children >= ctx->child_count)) {
        break;
      }
    }

    if ((deferred_rx_msg == NULL) && !_ticks_expired(osKernelGetTickCount(), deadline)) {
      deferred_rx_msg = _wait_for_confirmations_until(ctx, deadline);
    }
  }

  if (ctx->confirmed_children < ctx->child_count) {
    _append_missing_children(ctx);
  }

  _finish_confirmable_context(ctx);

  if (deferred_rx_msg != NULL) {
    propagation_deferred_rx_msg = deferred_rx_msg;
  }
}

static void _handle_non_confirmable_indication(const app_propagation_frame_t *frame)
{
  app_propagation_child_t *children = propagation_children_scratch;
  uint8_t *tx_frame = propagation_tx_frame;
  uint8_t child_count;
  uint8_t i;
  int tx_len;

  memset(children, 0, sizeof(propagation_children_scratch));
  child_count = _collect_children(children, APP_PROPAGATION_MAX_CHILDREN);

  printfTimeRTT("PROPAG IND_NON '%.*s' seq %u algo %u pan %u payload_len %u: %u direct children\n",
                 APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                 frame->discriminator,
                 (unsigned int)frame->sequence,
                 (unsigned int)frame->algo_selected,
                 (unsigned int)frame->pan_size,
                 (unsigned int)frame->payload_len,
                 (unsigned int)child_count);

  if (child_count == 0U) {
    return;
  }

  tx_len = _build_indication_frame(tx_frame,
                                   APP_PROPAGATION_FRAME_MAX_LEN,
                                   APP_PROPAGATION_FRAME_IND_NON,
                                   frame->sequence,
                                   frame->algo_selected,
                                   frame->pan_size,
                                   frame->discriminator,
                                   frame->payload,
                                   frame->payload_len,
                                   0U,
                                   frame->option);

  if ((tx_len < 0) || ((size_t)tx_len > APP_PROPAGATION_FRAME_MAX_LEN)) {
    printfTimeRTT("PROPAG IND_NON '%.*s' not propagated: frame too long\n",
                   APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                   frame->discriminator);
    return;
  }

  if ((frame->algo_selected == APP_PROPAGATION_ALGO_BROADCAST_MIXED)
      && (child_count > APP_PROPAGATION_BROADCAST_CHILD_THRESHOLD)) {
    if (_send_linklocal_broadcast(tx_frame, (size_t)tx_len, NULL)) {
      printfTimeRTT("PROPAG IND_NON '%.*s' seq %u: sent link-local broadcast\n",
                     APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                     frame->discriminator,
                     (unsigned int)frame->sequence);
    } else {
      printfTimeRTT("PROPAG IND_NON '%.*s' seq %u: link-local broadcast send failed\n",
                     APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                     frame->discriminator,
                     (unsigned int)frame->sequence);
    }
  }

  for (i = 0U; i < child_count; i++) {
    if (!_send_frame(&children[i].tx_addr, tx_frame, (size_t)tx_len, NULL)) {
      printfTimeRTT("PROPAG IND_NON '%.*s': send failed to child %.*s\n",
                     APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                     frame->discriminator,
                     APP_PROPAGATION_CHILD_TAG_LOG_LEN,
                     children[i].tag);
    }
    _delay_ms(APP_PROPAGATION_CHILD_SEND_GAP_MS);
  }
}

static bool _handle_wait_rx(app_propagation_context_t *ctx,
                            const app_propagation_rx_msg_t *rx_msg)
{
  app_propagation_frame_t frame;

  if (!_parse_frame(rx_msg->payload, rx_msg->payload_len, &frame)) {
    return false;
  }

  if ((frame.type == APP_PROPAGATION_FRAME_IND_CON)
      && _should_zero_confirm_unicast_indication(rx_msg, &frame)) {
    _send_zero_confirmation(&frame, &rx_msg->src_addr);
    return false;
  }

  if (_should_defer_indication(ctx, &frame)) {
    printfTimeRTT("Propagation IND '%.*s' seq %u interrupts wait for '%.*s' seq %u\n",
                   APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                   frame.discriminator,
                   (unsigned int)frame.sequence,
                   APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                   ctx->discriminator,
                   (unsigned int)ctx->sequence);
    return true;
  }

  if (frame.type == APP_PROPAGATION_FRAME_CONFIRMATION) {
    if (_confirmation_matches_context(ctx, &frame)) {
      _handle_confirmation(ctx, &frame, &rx_msg->src_addr);
    } else {
      _store_early_confirmation(&frame, &rx_msg->src_addr);
    }
    return false;
  }

  if (strcmp(frame.discriminator, ctx->discriminator) != 0) {
    printfTimeRTT("Propagation frame ignored while waiting for '%.*s': '%.*s'\n",
                   APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                   ctx->discriminator,
                   APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                   frame.discriminator);
    return false;
  }

  if ((frame.sequence == ctx->sequence) && _already_seen_indication(&frame)) {
    printfTimeRTT("Propagation duplicate IND ignored for '%.*s' seq %u\n",
                   APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                   ctx->discriminator,
                   (unsigned int)ctx->sequence);
  } else {
    printfTimeRTT("Propagation IND ignored while waiting for '%.*s' seq %u\n",
                   APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                   ctx->discriminator,
                   (unsigned int)ctx->sequence);
  }

  return false;
}

static void _handle_confirmation(app_propagation_context_t *ctx,
                                 const app_propagation_frame_t *frame,
                                 const sockaddr_in6_t *src_addr)
{
  if ((frame == NULL) || (src_addr == NULL)) {
    return;
  }

  (void)_handle_compact_confirmation(ctx,
                                     &src_addr->sin6_addr,
                                     NULL,
                                     frame->number,
                                     frame->option,
                                     frame->payload);
}

static bool _handle_compact_confirmation(app_propagation_context_t *ctx,
                                         const in6_addr_t *src_addr,
                                         const char *src_tag,
                                         uint32_t count,
                                         const char *option,
                                         const uint8_t *payload)
{
  int16_t child_index;

  if ((ctx == NULL) || (src_addr == NULL)) {
    return false;
  }
  (void)payload;

  child_index = _find_child_by_addr(ctx, src_addr);
  if (child_index < 0) {
    if ((src_tag != NULL) && (src_tag[0] != '\0')) {
      printfTimeRTT("PROPAG CONF '%.*s' ignored from unknown child %.*s\n",
                     APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                     ctx->discriminator,
                     APP_PROPAGATION_CHILD_TAG_LOG_LEN,
                     src_tag);
    } else {
      printfTimeRTT("PROPAG CONF '%.*s' ignored from unknown child\n",
                     APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                     ctx->discriminator);
    }
    return false;
  }

  if (ctx->children[child_index].confirmed) {
    printfTimeRTT("PROPAG CONF '%.*s' duplicate from child %.*s ignored\n",
                   APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                   ctx->discriminator,
                   APP_PROPAGATION_CHILD_TAG_LOG_LEN,
                   ctx->children[child_index].tag);
    return false;
  }

  ctx->children[child_index].confirmed = true;
  ctx->confirmed_children++;
  ctx->conf_count_total += count;
  _append_missing_option(ctx->missing, sizeof(ctx->missing), option);

  printfTimeRTT("PROPAG CONF '%.*s' seq %u: child %.*s count %lu (%u/%u)\n",
                 APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                 ctx->discriminator,
                 (unsigned int)ctx->sequence,
                 APP_PROPAGATION_CHILD_TAG_LOG_LEN,
                 ctx->children[child_index].tag,
                 (unsigned long)count,
                 (unsigned int)ctx->confirmed_children,
                 (unsigned int)ctx->child_count);

  return true;
}

static app_propagation_rx_msg_t *_drain_ready_wait_rx(app_propagation_context_t *ctx)
{
  app_propagation_rx_msg_t *rx_msg = NULL;

  if ((ctx == NULL) || (propagation_rx_queue_id == NULL)) {
    return NULL;
  }

  while (ctx->confirmed_children < ctx->child_count) {
    rx_msg = NULL;
    if (osMessageQueueGet(propagation_rx_queue_id, &rx_msg, NULL, 0U) != osOK) {
      break;
    }

    if (rx_msg != NULL) {
      if (_handle_wait_rx(ctx, rx_msg)) {
        return rx_msg;
      }
      (void)osMemoryPoolFree(propagation_rx_pool_id, rx_msg);
    }
  }

  return NULL;
}

static app_propagation_rx_msg_t *_wait_for_confirmations_until(app_propagation_context_t *ctx,
                                                               uint32_t deadline)
{
  app_propagation_rx_msg_t *rx_msg = NULL;
  uint32_t now;
  uint32_t wait_ticks;

  if ((ctx == NULL) || (propagation_rx_queue_id == NULL)) {
    return NULL;
  }

  while (ctx->confirmed_children < ctx->child_count) {
    now = osKernelGetTickCount();
    if (_ticks_expired(now, deadline)) {
      break;
    }

    wait_ticks = deadline - now;
    rx_msg = NULL;
    if (osMessageQueueGet(propagation_rx_queue_id, &rx_msg, NULL, wait_ticks) != osOK) {
      break;
    }

    if (rx_msg != NULL) {
      if (_handle_wait_rx(ctx, rx_msg)) {
        return rx_msg;
      }
      (void)osMemoryPoolFree(propagation_rx_pool_id, rx_msg);
      rx_msg = NULL;
    }
  }

  return NULL;
}

static bool _already_seen_indication(const app_propagation_frame_t *frame)
{
  uint8_t i;

  if ((frame == NULL)
      || ((frame->type != APP_PROPAGATION_FRAME_IND_CON)
          && (frame->type != APP_PROPAGATION_FRAME_IND_NON))) {
    return false;
  }

  for (i = 0U; i < APP_PROPAGATION_SEQUENCE_CACHE_LEN; i++) {
    if (propagation_seen_indications[i].used
        && (propagation_seen_indications[i].sequence == frame->sequence)
        && (strcmp(propagation_seen_indications[i].discriminator, frame->discriminator) == 0)) {
      return true;
    }
  }

  return false;
}

static void _remember_indication(const app_propagation_frame_t *frame)
{
  app_propagation_seen_indication_t *entry;

  if ((frame == NULL)
      || ((frame->type != APP_PROPAGATION_FRAME_IND_CON)
          && (frame->type != APP_PROPAGATION_FRAME_IND_NON))) {
    return;
  }

  entry = &propagation_seen_indications[propagation_seen_indication_next];
  memset(entry, 0, sizeof(*entry));
  entry->used = true;
  entry->sequence = frame->sequence;
  snprintf(entry->discriminator,
           sizeof(entry->discriminator),
           "%s",
           frame->discriminator);

  propagation_seen_indication_next++;
  if (propagation_seen_indication_next >= APP_PROPAGATION_SEQUENCE_CACHE_LEN) {
    propagation_seen_indication_next = 0U;
  }
}

static bool _completed_sequence_active(uint16_t sequence)
{
  uint32_t now;
  uint8_t i;

  now = osKernelGetTickCount();
  for (i = 0U; i < APP_PROPAGATION_COMPLETED_SEQUENCE_CACHE_LEN; i++) {
    if (!propagation_completed_sequences[i].used) {
      continue;
    }

    if (_ticks_expired(now, propagation_completed_sequences[i].expires_at)) {
      propagation_completed_sequences[i].used = false;
      continue;
    }

    if (propagation_completed_sequences[i].sequence == sequence) {
      return true;
    }
  }

  return false;
}

static void _remember_completed_sequence(uint16_t sequence)
{
  app_propagation_completed_sequence_t *entry = NULL;
  uint32_t timeout_ticks;
  uint32_t now;
  uint8_t i;

  timeout_ticks = _ms_to_ticks(APP_PROPAGATION_COMPLETED_SEQUENCE_DROP_MS);
  if (timeout_ticks == 0U) {
    return;
  }

  now = osKernelGetTickCount();
  for (i = 0U; i < APP_PROPAGATION_COMPLETED_SEQUENCE_CACHE_LEN; i++) {
    if (propagation_completed_sequences[i].used
        && (propagation_completed_sequences[i].sequence == sequence)) {
      entry = &propagation_completed_sequences[i];
      break;
    }
  }

  if (entry == NULL) {
    entry = &propagation_completed_sequences[propagation_completed_sequence_next];
    propagation_completed_sequence_next++;
    if (propagation_completed_sequence_next >= APP_PROPAGATION_COMPLETED_SEQUENCE_CACHE_LEN) {
      propagation_completed_sequence_next = 0U;
    }
  }

  entry->used = true;
  entry->sequence = sequence;
  entry->expires_at = now + timeout_ticks;
}

static bool _should_drop_completed_sequence_frame(const uint8_t *payload,
                                                  uint16_t payload_len)
{
  app_propagation_frame_type_t type;
  uint16_t sequence;

  if (!_parse_frame_header(payload, payload_len, &type, &sequence)) {
    return false;
  }

  (void)type;
  return _completed_sequence_active(sequence);
}

static void _purge_completed_sequence_from_queue(uint16_t sequence)
{
  app_propagation_rx_msg_t *rx_msg = NULL;
  app_propagation_rx_msg_t *kept[APP_PROPAGATION_RX_QUEUE_LEN];
  app_propagation_frame_type_t type;
  uint16_t frame_sequence;
  uint32_t kept_count = 0U;
  uint32_t i;

  if ((propagation_rx_queue_id == NULL) || (propagation_rx_pool_id == NULL)) {
    return;
  }

  while (osMessageQueueGet(propagation_rx_queue_id, &rx_msg, NULL, 0U) == osOK) {
    if ((rx_msg != NULL)
        && _parse_frame_header(rx_msg->payload, rx_msg->payload_len, &type, &frame_sequence)
        && (frame_sequence == sequence)) {
      (void)osMemoryPoolFree(propagation_rx_pool_id, rx_msg);
      rx_msg = NULL;
      continue;
    }

    if ((rx_msg != NULL) && (kept_count < APP_PROPAGATION_RX_QUEUE_LEN)) {
      kept[kept_count] = rx_msg;
      kept_count++;
    } else if (rx_msg != NULL) {
      (void)osMemoryPoolFree(propagation_rx_pool_id, rx_msg);
    }
    rx_msg = NULL;
  }

  for (i = 0U; i < kept_count; i++) {
    rx_msg = kept[i];
    if (osMessageQueuePut(propagation_rx_queue_id, &rx_msg, 0U, 0U) != osOK) {
      (void)osMemoryPoolFree(propagation_rx_pool_id, rx_msg);
    }
  }
}

static void _finish_confirmable_context(app_propagation_context_t *ctx)
{
  uint16_t sequence;

  if (ctx == NULL) {
    return;
  }

  sequence = ctx->sequence;
  _send_confirmation_to_parent(ctx);
  _clear_early_confirmations(sequence);
  _remember_completed_sequence(sequence);
  _purge_completed_sequence_from_queue(sequence);
}

static void _store_early_confirmation(const app_propagation_frame_t *frame,
                                      const sockaddr_in6_t *src_addr)
{
  app_propagation_early_conf_t *pending = NULL;
  app_propagation_early_child_conf_t *child;
  char src_tag[5];
  uint8_t i;

  if ((frame == NULL) || (src_addr == NULL)
      || (frame->type != APP_PROPAGATION_FRAME_CONFIRMATION)
      || _completed_sequence_active(frame->sequence)) {
    return;
  }

  src_tag[0] = '\0';
  _ipv6_last4(&src_addr->sin6_addr, src_tag);

  pending = _find_early_confirmation(frame->sequence);
  if ((pending != NULL) && _early_confirmation_has_child(pending, &src_addr->sin6_addr)) {
    return;
  }

  if (pending == NULL) {
    for (i = 0U; i < APP_PROPAGATION_EARLY_CONF_GROUP_LEN; i++) {
      if (!propagation_early_confirmations[i].used) {
        pending = &propagation_early_confirmations[i];
        break;
      }
    }
  }

  if (pending == NULL) {
    pending = &propagation_early_confirmations[propagation_early_confirmation_next];
    propagation_early_confirmation_next++;
    if (propagation_early_confirmation_next >= APP_PROPAGATION_EARLY_CONF_GROUP_LEN) {
      propagation_early_confirmation_next = 0U;
    }
  }

  if (!pending->used
      || (pending->sequence != frame->sequence)) {
    memset(pending, 0, sizeof(*pending));
    pending->used = true;
    pending->sequence = frame->sequence;
    snprintf(pending->discriminator,
             sizeof(pending->discriminator),
             "%s",
             frame->discriminator);
  }

  if (pending->child_count >= APP_PROPAGATION_EARLY_CONF_CHILD_LEN) {
    printfTimeRTT("PROPAG CONF '%.*s' seq %u early cache full, child %.*s dropped\n",
                  APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                  frame->discriminator,
                  (unsigned int)frame->sequence,
                  APP_PROPAGATION_CHILD_TAG_LOG_LEN,
                  src_tag);
    return;
  }

  child = &pending->children[pending->child_count];
  memset(child, 0, sizeof(*child));
  child->used = true;
  child->src_addr = src_addr->sin6_addr;
  snprintf(child->tag, sizeof(child->tag), "%s", src_tag);
  child->count = frame->number;
  pending->child_count++;
  _append_missing_option(pending->missing, sizeof(pending->missing), frame->option);
}

static void _apply_early_confirmations(app_propagation_context_t *ctx)
{
  app_propagation_early_conf_t *pending;
  bool accepted;
  uint8_t i;
  uint8_t j;

  if (ctx == NULL) {
    return;
  }

  for (i = 0U; i < APP_PROPAGATION_EARLY_CONF_GROUP_LEN; i++) {
    pending = &propagation_early_confirmations[i];
    if (!pending->used) {
      continue;
    }

    if (pending->sequence != ctx->sequence) {
      memset(pending, 0, sizeof(*pending));
      continue;
    }

    accepted = false;
    for (j = 0U; j < pending->child_count; j++) {
      if (pending->children[j].used
          && _handle_compact_confirmation(ctx,
                                          &pending->children[j].src_addr,
                                          pending->children[j].tag,
                                          pending->children[j].count,
                                          NULL,
                                          NULL)) {
        accepted = true;
      }
    }

    if (accepted) {
      _append_missing_option(ctx->missing, sizeof(ctx->missing), pending->missing);
    }

    memset(pending, 0, sizeof(*pending));
  }
}

static void _clear_early_confirmations(uint16_t sequence)
{
  uint8_t i;

  for (i = 0U; i < APP_PROPAGATION_EARLY_CONF_GROUP_LEN; i++) {
    if (propagation_early_confirmations[i].used
        && (propagation_early_confirmations[i].sequence == sequence)) {
      memset(&propagation_early_confirmations[i], 0, sizeof(propagation_early_confirmations[i]));
    }
  }
}

static app_propagation_early_conf_t *_find_early_confirmation(uint16_t sequence)
{
  uint8_t i;

  for (i = 0U; i < APP_PROPAGATION_EARLY_CONF_GROUP_LEN; i++) {
    if (propagation_early_confirmations[i].used
        && (propagation_early_confirmations[i].sequence == sequence)) {
      return &propagation_early_confirmations[i];
    }
  }

  return NULL;
}

static bool _early_confirmation_has_child(const app_propagation_early_conf_t *pending,
                                          const in6_addr_t *src_addr)
{
  uint8_t i;

  if ((pending == NULL) || (src_addr == NULL)) {
    return false;
  }

  for (i = 0U; i < pending->child_count; i++) {
    if (pending->children[i].used
        && _ipv6_equal(&pending->children[i].src_addr, src_addr)) {
      return true;
    }
  }

  return false;
}

static bool _should_defer_indication(const app_propagation_context_t *ctx,
                                     const app_propagation_frame_t *frame)
{
  if ((ctx == NULL) || (frame == NULL)
      || ((frame->type != APP_PROPAGATION_FRAME_IND_CON)
          && (frame->type != APP_PROPAGATION_FRAME_IND_NON))) {
    return false;
  }

  return (frame->sequence != ctx->sequence) && !_already_seen_indication(frame);
}

static bool _confirmation_matches_context(const app_propagation_context_t *ctx,
                                          const app_propagation_frame_t *frame)
{
  if ((ctx == NULL) || (frame == NULL)
      || (frame->type != APP_PROPAGATION_FRAME_CONFIRMATION)) {
    return false;
  }

  return (frame->sequence == ctx->sequence);
}

static app_propagation_callback_t _find_callback(const char *discriminator)
{
  uint8_t i;

  if (discriminator == NULL) {
    return NULL;
  }

  for (i = 0U; i < APP_PROPAGATION_REGISTRY_LEN; i++) {
    if (propagation_registry[i].used
        && (strcmp(propagation_registry[i].discriminator, discriminator) == 0)) {
      return propagation_registry[i].callback;
    }
  }

  return NULL;
}

static void _invoke_indication_callback(const app_propagation_frame_t *frame,
                                        char *confirmation_payload,
                                        size_t confirmation_payload_len)
{
  app_propagation_callback_t callback;
  app_propagation_indication_t indication;
  char *callback_response;

  if ((frame == NULL) || (confirmation_payload == NULL) || (confirmation_payload_len == 0U)) {
    return;
  }

  callback = _find_callback(frame->discriminator);
  if (callback == NULL) {
    printfTimeRTT("PROPAG %s '%.*s': no registered callback\n",
                   (frame->type == APP_PROPAGATION_FRAME_IND_CON) ? "IND_CON" : "IND_NON",
                   APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                   frame->discriminator);
    return;
  }

  indication = (frame->type == APP_PROPAGATION_FRAME_IND_CON)
               ? APP_PROPAGATION_IND_CON
               : APP_PROPAGATION_IND_NON;
  callback_response = callback(indication, frame->payload, frame->payload_len);

  if ((indication == APP_PROPAGATION_IND_CON)
      && (callback_response != NULL)
      && (callback_response[0] != '\0')) {
    snprintf(confirmation_payload,
             confirmation_payload_len,
             "%s",
             callback_response);
  }
}

static uint8_t _collect_children(app_propagation_child_t *children,
                                 uint8_t max_children)
{
  sl_status_t ret;
  uint8_t neighbor_count = 0U;
  uint8_t read_count;
  uint8_t child_count = 0U;
  uint8_t skipped_children = 0U;
  uint8_t i;
  sl_wisun_mac_address_t *neighbor_mac_addresses = NULL;

  ret = sl_wisun_get_neighbor_count(&neighbor_count);
  if (ret != SL_STATUS_OK) {
    printfTimeRTT("Propagation: sl_wisun_get_neighbor_count() failed 0x%04x\n",
                   (uint16_t)ret);
    return 0U;
  }

  if (neighbor_count == 0U) {
    return 0U;
  }

  neighbor_mac_addresses = sl_malloc(sizeof(sl_wisun_mac_address_t) * neighbor_count);
  if (neighbor_mac_addresses == NULL) {
    printfTimeRTT("Propagation: cannot allocate neighbor list for %u entries\n",
                   (unsigned int)neighbor_count);
    return 0U;
  }

  read_count = neighbor_count;
  ret = sl_wisun_get_neighbors(&read_count, neighbor_mac_addresses);
  if (ret != SL_STATUS_OK) {
    printfTimeRTT("Propagation: sl_wisun_get_neighbors() failed 0x%04x\n",
                   (uint16_t)ret);
    sl_free(neighbor_mac_addresses);
    return 0U;
  }

  for (i = 0U; i < read_count; i++) {
    sl_wisun_neighbor_info_t neighbor_info;
    const in6_addr_t *tx_addr;

    ret = sl_wisun_get_neighbor_info(&neighbor_mac_addresses[i], &neighbor_info);
    if (ret != SL_STATUS_OK) {
      continue;
    }

    if (neighbor_info.type != SL_WISUN_NEIGHBOR_TYPE_CHILD) {
      continue;
    }

    if (child_count >= max_children) {
      skipped_children++;
      continue;
    }

    tx_addr = !_ipv6_is_unspecified(&neighbor_info.global_address)
              ? &neighbor_info.global_address
              : &neighbor_info.link_local_address;
    if (_ipv6_is_unspecified(tx_addr)) {
      skipped_children++;
      continue;
    }

    memset(&children[child_count], 0, sizeof(children[child_count]));
    children[child_count].global_addr = neighbor_info.global_address;
    children[child_count].link_local_addr = neighbor_info.link_local_address;
    children[child_count].tx_addr.sin6_family = AF_INET6;
    children[child_count].tx_addr.sin6_port = htons(propagation_port);
    children[child_count].tx_addr.sin6_addr = *tx_addr;
    _ipv6_last4(tx_addr, children[child_count].tag);
    child_count++;
  }

  if (skipped_children != 0U) {
    printfTimeRTT("Propagation: %u children skipped or over APP_PROPAGATION_MAX_CHILDREN\n",
                   (unsigned int)skipped_children);
  }

  sl_free(neighbor_mac_addresses);
  return child_count;
}

static int _build_indication_frame(uint8_t *dst,
                                   size_t dst_len,
                                   app_propagation_frame_type_t type,
                                   uint16_t sequence,
                                   uint8_t algo_selected,
                                   uint16_t pan_size,
                                   const char *discriminator,
                                   const uint8_t *payload,
                                   uint16_t payload_len,
                                   uint32_t timeout_ms,
                                   const char *option)
{
  int header_len;
  size_t used;
  size_t option_len;

  if ((dst == NULL) || (dst_len == 0U) || (discriminator == NULL)
      || ((payload == NULL) && (payload_len != 0U))) {
    return -1;
  }

  if (type == APP_PROPAGATION_FRAME_IND_CON) {
    header_len = snprintf((char *)dst,
                          dst_len,
                          "PROPAG IND_CON %u %u %u %.*s %u %lu ",
                          (unsigned int)sequence,
                          (unsigned int)algo_selected,
                          (unsigned int)pan_size,
                          APP_PROPAGATION_DISCRIMINATOR_FIELD_LEN,
                          discriminator,
                          (unsigned int)payload_len,
                          (unsigned long)timeout_ms);
  } else if (type == APP_PROPAGATION_FRAME_IND_NON) {
    header_len = snprintf((char *)dst,
                          dst_len,
                          "PROPAG IND_NON %u %u %u %.*s %u ",
                          (unsigned int)sequence,
                          (unsigned int)algo_selected,
                          (unsigned int)pan_size,
                          APP_PROPAGATION_DISCRIMINATOR_FIELD_LEN,
                          discriminator,
                          (unsigned int)payload_len);
  } else {
    return -1;
  }

  if ((header_len < 0) || ((size_t)header_len >= dst_len)) {
    return -1;
  }

  used = (size_t)header_len;
  if ((used + payload_len) > dst_len) {
    return -1;
  }

  if (payload_len != 0U) {
    memcpy(&dst[used], payload, payload_len);
    used += payload_len;
  }

  if ((option != NULL) && (option[0] != '\0')) {
    option_len = strlen(option);
    if (option_len > APP_PROPAGATION_OPTION_FIELD_LEN) {
      option_len = APP_PROPAGATION_OPTION_FIELD_LEN;
    }
    if ((used + 1U + option_len) > dst_len) {
      return -1;
    }
    dst[used++] = ' ';
    memcpy(&dst[used], option, option_len);
    used += option_len;
  }

  return (int)used;
}

static bool _send_linklocal_broadcast(const uint8_t *payload,
                                      size_t payload_len,
                                      const app_propagation_ind_con_tx_t *ind_con_tx)
{
  sockaddr_in6_t dest_addr;

  if ((ind_con_tx == NULL) && ((payload == NULL) || (payload_len == 0U))) {
    return false;
  }

  memset(&dest_addr, 0, sizeof(dest_addr));
  dest_addr.sin6_family = AF_INET6;
  dest_addr.sin6_port = htons(propagation_port);
  dest_addr.sin6_addr.address[0] = 0xffU;
  dest_addr.sin6_addr.address[1] = 0x02U;
  dest_addr.sin6_addr.address[15] = 0x01U;

  return _send_frame(&dest_addr, payload, payload_len, ind_con_tx);
}

static bool _send_frame(const sockaddr_in6_t *dest_addr,
                        const uint8_t *payload,
                        size_t payload_len,
                        const app_propagation_ind_con_tx_t *ind_con_tx)
{
  const uint8_t *send_payload;
  size_t send_payload_len;
  ssize_t sent;
  uint32_t attempt;
  int tx_len;

  if ((dest_addr == NULL) || (propagation_sockid == SOCKET_INVALID_ID)) {
    return false;
  }

  if ((ind_con_tx == NULL) && ((payload == NULL) || (payload_len == 0U))) {
    return false;
  }

  for (attempt = 0U; attempt <= APP_PROPAGATION_SEND_RETRY_COUNT; attempt++) {
    send_payload = payload;
    send_payload_len = payload_len;
    if (ind_con_tx != NULL) {
      if (_ticks_expired(osKernelGetTickCount(), ind_con_tx->deadline)) {
        return false;
      }
      tx_len = _build_ind_con_tx_frame(propagation_tx_frame,
                                       APP_PROPAGATION_FRAME_MAX_LEN,
                                       ind_con_tx);
      if ((tx_len < 0) || ((size_t)tx_len > APP_PROPAGATION_FRAME_MAX_LEN)) {
        return false;
      }
      send_payload = propagation_tx_frame;
      send_payload_len = (size_t)tx_len;
    }

    sent = sendto(propagation_sockid,
                  send_payload,
                  send_payload_len,
                  0,
                  (const struct sockaddr *)dest_addr,
                  sizeof(sockaddr_in6_t));

    if ((sent >= 0) && ((size_t)sent == send_payload_len)) {
      return true;
    }

    if (attempt < APP_PROPAGATION_SEND_RETRY_COUNT) {
      _delay_ms(APP_PROPAGATION_SEND_RETRY_DELAY_MS);
    }
  }

  return false;
}

static int _build_ind_con_tx_frame(uint8_t *dst,
                                   size_t dst_len,
                                   const app_propagation_ind_con_tx_t *ind_con_tx)
{
  const app_propagation_context_t *ctx;

  if ((dst == NULL) || (ind_con_tx == NULL) || (ind_con_tx->ctx == NULL)) {
    return -1;
  }

  ctx = ind_con_tx->ctx;
  return _build_indication_frame(dst,
                                 dst_len,
                                 APP_PROPAGATION_FRAME_IND_CON,
                                 ctx->sequence,
                                 ctx->algo_selected,
                                 ctx->pan_size,
                                 ctx->discriminator,
                                 ctx->indication_payload,
                                 ctx->indication_payload_len,
                                 _child_timeout_ms(ind_con_tx->rx_tick,
                                                   ind_con_tx->timeout_ms),
                                 ind_con_tx->option);
}

static void _delay_ms(uint32_t delay_ms)
{
  uint32_t delay_ticks;

  if (delay_ms == 0U) {
    return;
  }

  delay_ticks = _ms_to_ticks(delay_ms);
  if (delay_ticks != 0U) {
    (void)osDelay(delay_ticks);
  }
}

static void _send_zero_confirmation(const app_propagation_frame_t *frame,
                                    const sockaddr_in6_t *src_addr)
{
  sockaddr_in6_t secondary_parent_addr;
  uint8_t *tx_frame = propagation_tx_frame;
  int tx_len;

  if ((frame == NULL) || (src_addr == NULL)) {
    return;
  }

  secondary_parent_addr = *src_addr;
  secondary_parent_addr.sin6_family = AF_INET6;
  secondary_parent_addr.sin6_port = htons(propagation_port);

  tx_len = snprintf((char *)tx_frame, APP_PROPAGATION_FRAME_MAX_LEN,
                    "PROPAG CONF %u %u %u %.*s %s 0",
                    (unsigned int)frame->sequence,
                    (unsigned int)frame->algo_selected,
                    (unsigned int)frame->pan_size,
                    APP_PROPAGATION_DISCRIMINATOR_FIELD_LEN,
                    frame->discriminator,
                    APP_PROPAGATION_DEFAULT_CONF_PAYLOAD);
  if ((tx_len < 0) || ((size_t)tx_len >= APP_PROPAGATION_FRAME_MAX_LEN)) {
    return;
  }

  if (_send_frame(&secondary_parent_addr, tx_frame, (size_t)tx_len, NULL)) {
    printfTimeRTT("PROPAG CONF '%.*s' seq %u: sent zero count to non-primary unicast sender\n",
                   APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                   frame->discriminator,
                   (unsigned int)frame->sequence);
  }
}

static bool _should_zero_confirm_unicast_indication(const app_propagation_rx_msg_t *rx_msg,
                                                    const app_propagation_frame_t *frame)
{
  if ((rx_msg == NULL) || (frame == NULL)
      || (frame->type != APP_PROPAGATION_FRAME_IND_CON)) {
    return false;
  }

  if (!rx_msg->dst_addr_valid) {
    return _source_is_secondary_parent(&rx_msg->src_addr);
  }

  if (_ipv6_is_multicast(&rx_msg->dst_addr.sin6_addr)) {
    return false;
  }

  return !_source_is_primary_parent(&rx_msg->src_addr);
}

static void _send_confirmation_to_parent(app_propagation_context_t *ctx)
{
  uint8_t *tx_frame = propagation_tx_frame;
  uint32_t aggregated_count = ctx->conf_count_total + 1U;
  int tx_len;

  if (!ctx->parent_addr_valid) {
    printfTimeRTT("PROPAG CONF '%.*s' seq %u not sent: no primary parent address\n",
                   APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                   ctx->discriminator,
                   (unsigned int)ctx->sequence);
    return;
  }

  if (ctx->missing[0] != '\0') {
    tx_len = snprintf((char *)tx_frame, APP_PROPAGATION_FRAME_MAX_LEN,
                      "PROPAG CONF %u %u %u %.*s %.*s %lu " APP_PROPAGATION_MISSING_PREFIX "%.*s",
                      (unsigned int)ctx->sequence,
                      (unsigned int)ctx->algo_selected,
                      (unsigned int)ctx->pan_size,
                      APP_PROPAGATION_DISCRIMINATOR_FIELD_LEN,
                      ctx->discriminator,
                      APP_PROPAGATION_PAYLOAD_FIELD_LEN,
                      ctx->confirmation_payload,
                      (unsigned long)aggregated_count,
                      APP_PROPAGATION_OPTION_FIELD_LEN,
                      ctx->missing);
  } else {
    tx_len = snprintf((char *)tx_frame, APP_PROPAGATION_FRAME_MAX_LEN,
                      "PROPAG CONF %u %u %u %.*s %.*s %lu",
                      (unsigned int)ctx->sequence,
                      (unsigned int)ctx->algo_selected,
                      (unsigned int)ctx->pan_size,
                      APP_PROPAGATION_DISCRIMINATOR_FIELD_LEN,
                      ctx->discriminator,
                      APP_PROPAGATION_PAYLOAD_FIELD_LEN,
                      ctx->confirmation_payload,
                      (unsigned long)aggregated_count);
  }

  if ((tx_len < 0) || ((size_t)tx_len >= APP_PROPAGATION_FRAME_MAX_LEN)) {
    printfTimeRTT("PROPAG CONF '%.*s' not sent: frame too long\n",
                   APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                   ctx->discriminator);
    return;
  }

  if (_send_frame(&ctx->parent_addr, tx_frame, (size_t)tx_len, NULL)) {
    printfTimeRTT("PROPAG CONF '%.*s' seq %u: sent count %lu, missing '%.*s'\n",
                   APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                   ctx->discriminator,
                   (unsigned int)ctx->sequence,
                   (unsigned long)aggregated_count,
                   APP_PROPAGATION_OPTION_LOG_LEN,
                   (ctx->missing[0] != '\0') ? ctx->missing : "none");
  } else {
    printfTimeRTT("PROPAG CONF '%.*s' seq %u: send to parent failed\n",
                   APP_PROPAGATION_DISCRIMINATOR_LOG_LEN,
                   ctx->discriminator,
                   (unsigned int)ctx->sequence);
  }
}

static void _append_missing_children(app_propagation_context_t *ctx)
{
  uint8_t i;

  for (i = 0U; i < ctx->child_count; i++) {
    if (!ctx->children[i].confirmed) {
      _append_missing_token(ctx->missing,
                            sizeof(ctx->missing),
                            ctx->children[i].tag);
    }
  }
}

static void _append_missing_option(char *missing, size_t missing_len,
                                   const char *option)
{
  const char *cursor;
  char token[24];
  size_t token_len;

  if ((missing == NULL) || (option == NULL) || (option[0] == '\0')) {
    return;
  }

  cursor = option;
  if (strncmp(cursor, APP_PROPAGATION_MISSING_PREFIX,
              strlen(APP_PROPAGATION_MISSING_PREFIX)) == 0) {
    cursor += strlen(APP_PROPAGATION_MISSING_PREFIX);
  }

  while (*cursor != '\0') {
    while ((*cursor == ' ') || (*cursor == ',') || (*cursor == ';')) {
      cursor++;
    }

    if (*cursor == '\0') {
      break;
    }

    if (strncmp(cursor, APP_PROPAGATION_MISSING_PREFIX,
                strlen(APP_PROPAGATION_MISSING_PREFIX)) == 0) {
      cursor += strlen(APP_PROPAGATION_MISSING_PREFIX);
      continue;
    }

    token_len = 0U;
    while ((cursor[token_len] != '\0')
           && (cursor[token_len] != ' ')
           && (cursor[token_len] != ',')
           && (cursor[token_len] != ';')
           && (token_len < (sizeof(token) - 1U))) {
      token[token_len] = cursor[token_len];
      token_len++;
    }
    token[token_len] = '\0';

    while ((cursor[token_len] != '\0')
           && (cursor[token_len] != ' ')
           && (cursor[token_len] != ',')
           && (cursor[token_len] != ';')) {
      token_len++;
    }
    cursor += token_len;

    if (token[0] != '\0') {
      _append_missing_token(missing, missing_len, token);
    }
  }
}

static void _append_missing_token(char *missing, size_t missing_len,
                                  const char *token)
{
  size_t missing_used;
  size_t token_len;
  size_t needed;

  if ((missing == NULL) || (token == NULL) || (token[0] == '\0')) {
    return;
  }

  if (_missing_has_token(missing, token)) {
    return;
  }

  missing_used = strlen(missing);
  token_len = strlen(token);
  needed = missing_used + token_len + ((missing_used == 0U) ? 1U : 2U);

  if (needed > missing_len) {
    if (strcmp(token, APP_PROPAGATION_TRUNCATED_TOKEN) != 0) {
      _append_missing_token(missing, missing_len, APP_PROPAGATION_TRUNCATED_TOKEN);
    }
    return;
  }

  if (missing_used != 0U) {
    missing[missing_used] = ',';
    missing_used++;
    missing[missing_used] = '\0';
  }

  (void)snprintf(&missing[missing_used],
                 missing_len - missing_used,
                 "%s",
                 token);
}

static bool _missing_has_token(const char *missing, const char *token)
{
  const char *cursor = missing;
  size_t token_len;

  if ((missing == NULL) || (token == NULL)) {
    return false;
  }

  token_len = strlen(token);
  while (*cursor != '\0') {
    while (*cursor == ',') {
      cursor++;
    }

    if ((strncmp(cursor, token, token_len) == 0)
        && ((cursor[token_len] == '\0') || (cursor[token_len] == ','))) {
      return true;
    }

    while ((*cursor != '\0') && (*cursor != ',')) {
      cursor++;
    }
  }

  return false;
}

static bool _get_primary_parent_addr(sockaddr_in6_t *parent_addr)
{
  sl_status_t ret;
  in6_addr_t primary_parent_addr;

  if (parent_addr == NULL) {
    return false;
  }

  memset(parent_addr, 0, sizeof(*parent_addr));

  ret = sl_wisun_get_ip_address(SL_WISUN_IP_ADDRESS_TYPE_PRIMARY_PARENT,
                                &primary_parent_addr);
  if ((ret != SL_STATUS_OK) || _ipv6_is_unspecified(&primary_parent_addr)) {
    return false;
  }

  parent_addr->sin6_family = AF_INET6;
  parent_addr->sin6_port = htons(propagation_port);
  parent_addr->sin6_addr = primary_parent_addr;

  return true;
}

static bool _source_is_primary_parent(const sockaddr_in6_t *src_addr)
{
  sl_status_t ret;
  in6_addr_t primary_parent_addr;
  sl_wisun_mac_address_t *neighbor_mac_addresses = NULL;
  uint8_t neighbor_count = 0U;
  uint8_t read_count;
  uint8_t i;
  bool matched = false;

  if (src_addr == NULL) {
    return false;
  }

  ret = sl_wisun_get_ip_address(SL_WISUN_IP_ADDRESS_TYPE_PRIMARY_PARENT,
                                &primary_parent_addr);
  if ((ret == SL_STATUS_OK)
      && !_ipv6_is_unspecified(&primary_parent_addr)
      && _ipv6_equal(&src_addr->sin6_addr, &primary_parent_addr)) {
    return true;
  }

  ret = sl_wisun_get_neighbor_count(&neighbor_count);
  if ((ret != SL_STATUS_OK) || (neighbor_count == 0U)) {
    return false;
  }

  neighbor_mac_addresses = sl_malloc(sizeof(sl_wisun_mac_address_t) * neighbor_count);
  if (neighbor_mac_addresses == NULL) {
    return false;
  }

  read_count = neighbor_count;
  ret = sl_wisun_get_neighbors(&read_count, neighbor_mac_addresses);
  if (ret != SL_STATUS_OK) {
    sl_free(neighbor_mac_addresses);
    return false;
  }

  for (i = 0U; i < read_count; i++) {
    sl_wisun_neighbor_info_t neighbor_info;

    ret = sl_wisun_get_neighbor_info(&neighbor_mac_addresses[i], &neighbor_info);
    if (ret != SL_STATUS_OK) {
      continue;
    }

    if (neighbor_info.type != SL_WISUN_NEIGHBOR_TYPE_PRIMARY_PARENT) {
      continue;
    }

    matched = _ipv6_equal(&src_addr->sin6_addr, &neighbor_info.global_address)
              || _ipv6_equal(&src_addr->sin6_addr, &neighbor_info.link_local_address);
    break;
  }

  sl_free(neighbor_mac_addresses);
  return matched;
}

static bool _source_is_secondary_parent(const sockaddr_in6_t *src_addr)
{
  sl_status_t ret;
  in6_addr_t secondary_parent_addr;
  sl_wisun_mac_address_t *neighbor_mac_addresses = NULL;
  uint8_t neighbor_count = 0U;
  uint8_t read_count;
  uint8_t i;
  bool matched = false;

  if (src_addr == NULL) {
    return false;
  }

  ret = sl_wisun_get_ip_address(SL_WISUN_IP_ADDRESS_TYPE_SECONDARY_PARENT,
                                &secondary_parent_addr);
  if ((ret == SL_STATUS_OK)
      && !_ipv6_is_unspecified(&secondary_parent_addr)
      && _ipv6_equal(&src_addr->sin6_addr, &secondary_parent_addr)) {
    return true;
  }

  ret = sl_wisun_get_neighbor_count(&neighbor_count);
  if ((ret != SL_STATUS_OK) || (neighbor_count == 0U)) {
    return false;
  }

  neighbor_mac_addresses = sl_malloc(sizeof(sl_wisun_mac_address_t) * neighbor_count);
  if (neighbor_mac_addresses == NULL) {
    return false;
  }

  read_count = neighbor_count;
  ret = sl_wisun_get_neighbors(&read_count, neighbor_mac_addresses);
  if (ret != SL_STATUS_OK) {
    sl_free(neighbor_mac_addresses);
    return false;
  }

  for (i = 0U; i < read_count; i++) {
    sl_wisun_neighbor_info_t neighbor_info;

    ret = sl_wisun_get_neighbor_info(&neighbor_mac_addresses[i], &neighbor_info);
    if (ret != SL_STATUS_OK) {
      continue;
    }

    if (neighbor_info.type != SL_WISUN_NEIGHBOR_TYPE_SECONDARY_PARENT) {
      continue;
    }

    matched = _ipv6_equal(&src_addr->sin6_addr, &neighbor_info.global_address)
              || _ipv6_equal(&src_addr->sin6_addr, &neighbor_info.link_local_address);
    break;
  }

  sl_free(neighbor_mac_addresses);
  return matched;
}

static int16_t _find_child_by_addr(app_propagation_context_t *ctx,
                                   const in6_addr_t *src_addr)
{
  uint8_t i;

  if ((ctx == NULL) || (src_addr == NULL)) {
    return -1;
  }

  for (i = 0U; i < ctx->child_count; i++) {
    if (_ipv6_equal(src_addr, &ctx->children[i].tx_addr.sin6_addr)
        || _ipv6_equal(src_addr, &ctx->children[i].global_addr)
        || _ipv6_equal(src_addr, &ctx->children[i].link_local_addr)) {
      return (int16_t)i;
    }
  }

  return -1;
}

static bool _parse_frame(const uint8_t *payload,
                         uint16_t payload_len,
                         app_propagation_frame_t *frame)
{
  size_t offset = 0U;
  char token[APP_PROPAGATION_PAYLOAD_MAX_LEN];
  uint32_t parsed_payload_len;

  if ((payload == NULL) || (frame == NULL)) {
    return false;
  }

  memset(frame, 0, sizeof(*frame));

  if (!_next_token(payload, payload_len, &offset, token, sizeof(token))) {
    return false;
  }

  if (strcmp(token, APP_PROPAGATION_PREFIX) != 0) {
    return false;
  }

  if (!_next_token(payload, payload_len, &offset, token, sizeof(token))) {
    return false;
  }

  if (strcmp(token, "IND_CON") == 0) {
    frame->type = APP_PROPAGATION_FRAME_IND_CON;
  } else if (strcmp(token, "IND_NON") == 0) {
    frame->type = APP_PROPAGATION_FRAME_IND_NON;
  } else if (strcmp(token, "CONF") == 0) {
    frame->type = APP_PROPAGATION_FRAME_CONFIRMATION;
  } else {
    return false;
  }

  if (!_next_token(payload, payload_len, &offset, token, sizeof(token))) {
    return false;
  }
  if (!_parse_u32_token(token, &parsed_payload_len) || (parsed_payload_len > UINT16_MAX)) {
    return false;
  }
  frame->sequence = (uint16_t)parsed_payload_len;

  if (!_next_token(payload, payload_len, &offset, token, sizeof(token))) {
    return false;
  }
  if (!_parse_u32_token(token, &parsed_payload_len) || (parsed_payload_len > UINT8_MAX)) {
    return false;
  }
  frame->algo_selected = (uint8_t)parsed_payload_len;

  if (!_next_token(payload, payload_len, &offset, token, sizeof(token))) {
    return false;
  }
  if (!_parse_u32_token(token, &parsed_payload_len) || (parsed_payload_len > UINT16_MAX)) {
    return false;
  }
  frame->pan_size = (uint16_t)parsed_payload_len;

  if (!_next_token(payload, payload_len, &offset, frame->discriminator, sizeof(frame->discriminator))) {
    return false;
  }

  if ((frame->type == APP_PROPAGATION_FRAME_IND_CON)
      || (frame->type == APP_PROPAGATION_FRAME_IND_NON)) {
    if (!_next_token(payload, payload_len, &offset, token, sizeof(token))) {
      return false;
    }
    if (!_parse_u32_token(token, &parsed_payload_len)
        || (parsed_payload_len > APP_PROPAGATION_PAYLOAD_MAX_LEN)) {
      return false;
    }
    frame->payload_len = (uint16_t)parsed_payload_len;

    if (frame->type == APP_PROPAGATION_FRAME_IND_CON) {
      if (!_next_token(payload, payload_len, &offset, token, sizeof(token))) {
        return false;
      }
      if (!_parse_u32_token(token, &frame->number)) {
        return false;
      }
      frame->number_present = true;
    }

    if ((frame->payload_len == 0U) && (offset >= payload_len)) {
      return true;
    }

    if (!_consume_payload_separator(payload, payload_len, &offset)) {
      return false;
    }
    if ((offset + frame->payload_len) > payload_len) {
      return false;
    }

    if (frame->payload_len != 0U) {
      memcpy(frame->payload, &payload[offset], frame->payload_len);
    }
    frame->payload[frame->payload_len] = '\0';
    offset += frame->payload_len;

    _copy_remaining_option(frame->option,
                           sizeof(frame->option),
                           payload,
                           payload_len,
                           offset);
    _trim_right(frame->option);
    return true;
  }

  if (!_next_token(payload, payload_len, &offset, token, sizeof(token))) {
    return false;
  }
  frame->payload_len = (uint16_t)strlen(token);
  memcpy(frame->payload, token, frame->payload_len);
  frame->payload[frame->payload_len] = '\0';

  if (!_next_token(payload, payload_len, &offset, token, sizeof(token))) {
    return false;
  }
  if (!_parse_u32_token(token, &frame->number)) {
    return false;
  }
  frame->number_present = true;

  _copy_remaining_option(frame->option,
                         sizeof(frame->option),
                         payload,
                         payload_len,
                         offset);
  _trim_right(frame->option);

  return true;
}

static bool _parse_frame_header(const uint8_t *payload,
                                uint16_t payload_len,
                                app_propagation_frame_type_t *type,
                                uint16_t *sequence)
{
  size_t offset = 0U;
  char token[APP_PROPAGATION_DISCRIMINATOR_MAX_LEN];
  uint32_t parsed_sequence;

  if ((payload == NULL) || (type == NULL) || (sequence == NULL)) {
    return false;
  }

  if (!_next_token(payload, payload_len, &offset, token, sizeof(token))
      || (strcmp(token, APP_PROPAGATION_PREFIX) != 0)) {
    return false;
  }

  if (!_next_token(payload, payload_len, &offset, token, sizeof(token))) {
    return false;
  }

  if (strcmp(token, "IND_CON") == 0) {
    *type = APP_PROPAGATION_FRAME_IND_CON;
  } else if (strcmp(token, "IND_NON") == 0) {
    *type = APP_PROPAGATION_FRAME_IND_NON;
  } else if (strcmp(token, "CONF") == 0) {
    *type = APP_PROPAGATION_FRAME_CONFIRMATION;
  } else {
    return false;
  }

  if (!_next_token(payload, payload_len, &offset, token, sizeof(token))) {
    return false;
  }

  if (!_parse_u32_token(token, &parsed_sequence) || (parsed_sequence > UINT16_MAX)) {
    return false;
  }

  *sequence = (uint16_t)parsed_sequence;
  return true;
}

static bool _parse_u32_token(const char *token, uint32_t *value)
{
  char *endptr;
  unsigned long parsed;

  if ((token == NULL) || (value == NULL)) {
    return false;
  }

  parsed = strtoul(token, &endptr, 10);
  if ((*token == '\0') || (*endptr != '\0') || (parsed > UINT32_MAX)) {
    return false;
  }

  *value = (uint32_t)parsed;
  return true;
}

static bool _next_token(const uint8_t *payload,
                        size_t payload_len,
                        size_t *offset,
                        char *token,
                        size_t token_len)
{
  size_t used = 0U;
  size_t pos;

  if ((payload == NULL) || (offset == NULL) || (token == NULL) || (token_len == 0U)) {
    return false;
  }

  pos = *offset;
  while ((pos < payload_len)
         && ((payload[pos] == ' ')
             || (payload[pos] == '\t')
             || (payload[pos] == '\r')
             || (payload[pos] == '\n'))) {
    pos++;
  }

  if (pos >= payload_len) {
    token[0] = '\0';
    *offset = pos;
    return false;
  }

  while ((pos < payload_len)
         && (payload[pos] != ' ')
         && (payload[pos] != '\t')
         && (payload[pos] != '\r')
         && (payload[pos] != '\n')) {
    if (used < (token_len - 1U)) {
      token[used] = (char)payload[pos];
      used++;
    }
    pos++;
  }

  token[used] = '\0';
  *offset = pos;
  return true;
}

static bool _consume_payload_separator(const uint8_t *payload,
                                       size_t payload_len,
                                       size_t *offset)
{
  if ((payload == NULL) || (offset == NULL) || (*offset >= payload_len)) {
    return false;
  }

  if ((payload[*offset] != ' ')
      && (payload[*offset] != '\t')
      && (payload[*offset] != '\r')
      && (payload[*offset] != '\n')) {
    return false;
  }

  (*offset)++;
  return true;
}

static void _trim_right(char *str)
{
  size_t len;

  if (str == NULL) {
    return;
  }

  len = strlen(str);
  while ((len > 0U)
         && ((str[len - 1U] == ' ')
             || (str[len - 1U] == '\t')
             || (str[len - 1U] == '\r')
             || (str[len - 1U] == '\n'))) {
    str[len - 1U] = '\0';
    len--;
  }
}

static void _copy_remaining_option(char *dst,
                                   size_t dst_len,
                                   const uint8_t *payload,
                                   size_t payload_len,
                                   size_t offset)
{
  size_t used = 0U;

  if ((dst == NULL) || (dst_len == 0U)) {
    return;
  }

  dst[0] = '\0';

  while ((payload != NULL)
         && (offset < payload_len)
         && ((payload[offset] == ' ') || (payload[offset] == '\t'))) {
    offset++;
  }

  while ((payload != NULL) && (offset < payload_len) && (used < (dst_len - 1U))) {
    dst[used] = (char)payload[offset];
    used++;
    offset++;
  }

  dst[used] = '\0';
}

static bool _ipv6_equal(const in6_addr_t *a, const in6_addr_t *b)
{
  if ((a == NULL) || (b == NULL)) {
    return false;
  }

  return memcmp(a->address, b->address, sizeof(a->address)) == 0;
}

static bool _ipv6_is_multicast(const in6_addr_t *addr)
{
  if (addr == NULL) {
    return false;
  }

  return (addr->address[0] == 0xffU);
}

static bool _ipv6_is_unspecified(const in6_addr_t *addr)
{
  uint8_t i;

  if (addr == NULL) {
    return true;
  }

  for (i = 0U; i < sizeof(addr->address); i++) {
    if (addr->address[i] != 0U) {
      return false;
    }
  }

  return true;
}

static void _ipv6_last4(const in6_addr_t *addr, char tag[5])
{
  if ((addr == NULL) || (tag == NULL)) {
    return;
  }

  (void)snprintf(tag,
                 5U,
                 "%02x%02x",
                 addr->address[14],
                 addr->address[15]);
}

static uint32_t _adjust_timeout_for_hop(uint32_t timeout_ms)
{
  sl_wisun_network_info_t network_info;
  uint32_t hop_margin_ms = 0U;

  if (sl_wisun_get_network_info(&network_info) == SL_STATUS_OK) {
    hop_margin_ms = (uint32_t)network_info.hop_count * APP_PROPAGATION_TIMEOUT_PER_HOP_MARGIN_MS;
  }

  if (timeout_ms > (hop_margin_ms + APP_PROPAGATION_TIMEOUT_MIN_MS)) {
    timeout_ms -= hop_margin_ms;
  } else {
    timeout_ms = APP_PROPAGATION_TIMEOUT_MIN_MS;
  }

  if (timeout_ms < APP_PROPAGATION_TIMEOUT_MIN_MS) {
    timeout_ms = APP_PROPAGATION_TIMEOUT_MIN_MS;
  }

  return timeout_ms;
}

static uint32_t _child_timeout_ms(uint32_t rx_tick, uint32_t timeout_ms)
{
  uint32_t remaining_ms = _timeout_remaining_ms(rx_tick, timeout_ms);

  if (remaining_ms < APP_PROPAGATION_TIMEOUT_MIN_MS) {
    remaining_ms = APP_PROPAGATION_TIMEOUT_MIN_MS;
  }

  return remaining_ms;
}

static uint32_t _timeout_remaining_ms(uint32_t rx_tick, uint32_t timeout_ms)
{
  uint32_t timeout_ticks = _ms_to_ticks(timeout_ms);
  uint32_t now = osKernelGetTickCount();
  uint32_t deadline = rx_tick + timeout_ticks;

  if (_ticks_expired(now, deadline)) {
    return 0U;
  }

  return _ticks_to_ms(deadline - now);
}

static uint32_t _ms_to_ticks(uint32_t ms)
{
  uint32_t freq = osKernelGetTickFreq();
  uint64_t ticks;

  if (freq == 0U) {
    return ms;
  }

  ticks = (((uint64_t)ms * (uint64_t)freq) + 999U) / 1000U;
  if (ticks > (uint64_t)(UINT32_MAX - 1U)) {
    return UINT32_MAX - 1U;
  }

  return (uint32_t)ticks;
}

static uint32_t _ticks_to_ms(uint32_t ticks)
{
  uint32_t freq = osKernelGetTickFreq();
  uint64_t ms;

  if (freq == 0U) {
    return ticks;
  }

  ms = (((uint64_t)ticks * 1000U) + ((uint64_t)freq - 1U)) / (uint64_t)freq;
  if (ms > (uint64_t)UINT32_MAX) {
    return UINT32_MAX;
  }

  return (uint32_t)ms;
}

static bool _ticks_expired(uint32_t now, uint32_t deadline)
{
  return ((int32_t)(now - deadline) >= 0);
}
