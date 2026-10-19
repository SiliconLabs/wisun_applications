/***************************************************************************//**
* @file app_fb_lg.c
* @brief First Breath event handling and Last Gasp test support
*******************************************************************************
* SPDX-License-Identifier: Zlib
******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "app_action_scheduler.h"
#include "app_fb_lg.h"
#include "app_timestamp.h"
#include "cmsis_os2.h"
#include "sl_event_system.h"
#include "sl_wisun_api.h"
#include "sl_wisun_types.h"
#include "sl_wisun_ip6string.h"

#if defined(SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT)
  #include "cmsis_nvic_virtual.h"
#endif

#define FIRST_BREATH_SOCKET_CLOSE_DELAY_MS  (60U * 1000U)
#define EVENT_EXT_QUEUE_SIZE                8U
#define EVENT_EXT_TASK_STACK_SIZE_BYTES     (1U * 2048UL)
#define IPV6_STRING_LENGTH                  41U

static char fb_lg_device_tag[5] = "0000";

SL_WEAK const char *app_fb_lg_get_udp_notification_destination(void)
{
  return "fd00:6172:6d00::1";
}

static sl_event_queue_t event_ext_queue = NULL;
static osThreadId_t event_ext_task_id = NULL;

static const osThreadAttr_t event_ext_task_attr = {
  .name = "event_ext",
  .attr_bits = osThreadDetached,
  .cb_mem = NULL,
  .cb_size = 0,
  .stack_mem = NULL,
  .stack_size = EVENT_EXT_TASK_STACK_SIZE_BYTES,
  .priority = osPriorityNormal,
  .tz_module = 0
};

static sl_wisun_socket_id_t first_breath_socket_id = SOCKET_INVALID_ID;
static sockaddr_in6_t first_breath_sockaddr_in6;
static char first_breath_ipv6_string[IPV6_STRING_LENGTH];
static char first_breath_msg[256];

static void event_ext_task(void *argument);
static void event_ext_handle_wisun_ind(sl_wisun_evt_t *wisun_evt);
static void first_breath_ready_custom_callback(sl_wisun_evt_t *evt);
static uint32_t first_breath_send_action(void *context);
static uint32_t first_breath_close_action(void *context);

// Last Gasp only supported by FFN
#if defined(SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT)
static bool last_gasp_test_pending;
static sl_wisun_socket_id_t last_gasp_socket_id = SOCKET_INVALID_ID;
static sockaddr_in6_t last_gasp_sockaddr_in6;
static char last_gasp_ipv6_string[IPV6_STRING_LENGTH];
static char last_gasp_msg[256];

static uint32_t last_gasp_prepare_and_send_action(void *context);
static uint32_t last_gasp_reboot_action(void *context);
static void last_gasp_clear_pending(void);
#endif

void app_fb_lg_init(void)
{
  sl_status_t status;
  sl_wisun_mac_address_t device_mac;

  status = sl_wisun_get_mac_address(&device_mac);
  if (status == SL_STATUS_OK) {
    snprintf(fb_lg_device_tag,
             sizeof(fb_lg_device_tag),
             "%02x%02x",
             (unsigned int)device_mac.address[6],
             (unsigned int)device_mac.address[7]);
  } else {
    printfBothTime("Failed to get MAC address for First Breath/Last Gasp device tag: 0x%04x\n",
                   (uint16_t)status);
  }

  status = sl_event_queue_create(EVENT_EXT_QUEUE_SIZE, &event_ext_queue);
  if (status != SL_STATUS_OK) {
    printfBothTime("Failed to create event_ext queue: 0x%04x\n",
                   (uint16_t)status);
    return;
  }

  status = sl_event_subscribe(SL_EVENT_CLASS_WISUN,
                              SL_WISUN_EVENT_IND_MASK,
                              event_ext_queue);
  if (status != SL_STATUS_OK) {
    printfBothTime("Failed to subscribe event_ext queue: 0x%04x\n",
                   (uint16_t)status);
    (void)sl_event_queue_delete(event_ext_queue);
    event_ext_queue = NULL;
    return;
  }

  event_ext_task_id = osThreadNew(event_ext_task, NULL, &event_ext_task_attr);
  if (event_ext_task_id == NULL) {
    printfBothTime("Failed to create event_ext task\n");
    (void)sl_event_queue_delete(event_ext_queue);
    event_ext_queue = NULL;
  }
}

static void event_ext_task(void *argument)
{
  uint8_t event_prio = 0U;
  sl_event_t *event = NULL;

  (void)argument;

  for (;;) {
    if (sl_event_queue_get(event_ext_queue,
                           &event_prio,
                           osWaitForever,
                           &event) != SL_STATUS_OK) {
      continue;
    }

    if ((event != NULL) && (event->event_data != NULL)) {
      event_ext_handle_wisun_ind((sl_wisun_evt_t *)event->event_data);
    }

    // This queue shares stack event objects; always release after inspection.
    (void)sl_event_process(&event);
  }
}

static void event_ext_handle_wisun_ind(sl_wisun_evt_t *wisun_evt)
{
  switch (wisun_evt->header.id) {
    case SL_WISUN_MSG_FB_READY_IND_ID:
      first_breath_ready_custom_callback(wisun_evt);
      break;
    case SL_WISUN_MSG_RX_FRAME_IND_ID:
    case SL_WISUN_MSG_LOGGER_EVENT_IND_ID:
    case SL_WISUN_MSG_DIRECT_CONNECT_ID_SOLICIT_IND_ID:
    case SL_WISUN_MSG_DIRECT_CONNECT_ID_RECEIVED_IND_ID:
    case SL_WISUN_MSG_DIRECT_CONNECT_CLIENT_STATE_CHANGED_IND_ID:
      printfBothTime("event_ext: Wi-SUN ind 0x%02x not handled by SDK event manager\n",
                     (unsigned int)wisun_evt->header.id);
      break;
    default:
      break;
  }
}

static uint32_t first_breath_close_action(void *context)
{
  (void)context;

  if (first_breath_socket_id == SOCKET_INVALID_ID) {
    return 0U;
  }

  printfBothTime("Closing First Breath socket (id %d)\n",
                 (int)first_breath_socket_id);
  if (close(first_breath_socket_id) == SOCKET_RETVAL_ERROR) {
    first_breath_socket_id = SOCKET_INVALID_ID;
    return (uint32_t)SL_STATUS_FAIL;
  }

  first_breath_socket_id = SOCKET_INVALID_ID;
  return 0U;
}

static uint32_t first_breath_send_action(void *context)
{
  const char *dest = app_fb_lg_get_udp_notification_destination();
  char running_sec_string[21];
  int tclass;
  int msg_len;
  int socket_ret;

  (void)context;

  if (first_breath_socket_id != SOCKET_INVALID_ID) {
    (void)app_scheduler_action_stop(first_breath_close_action);
    (void)first_breath_close_action(NULL);
  }

  memset(&first_breath_sockaddr_in6, 0, sizeof(first_breath_sockaddr_in6));
  if ((dest == NULL)
      || !sl_wisun_stoip6(dest,
                          strlen(dest),
                          first_breath_sockaddr_in6.sin6_addr.address)) {
    printfBothTime("First Breath invalid UDP destination: %s\n",
                   (dest != NULL) ? dest : "(null)");
    return (uint32_t)SL_STATUS_INVALID_PARAMETER;
  }

  sl_wisun_ip6tos(first_breath_sockaddr_in6.sin6_addr.address,
                  first_breath_ipv6_string);
  first_breath_sockaddr_in6.sin6_family = AF_INET6;
  first_breath_sockaddr_in6.sin6_port = htons(FIRST_BREATH_NOTIFICATION_PORT);

  first_breath_socket_id = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  if (first_breath_socket_id == SOCKET_INVALID_ID) {
    printfBothTime("Failed to open First Breath socket\n");
    return (uint32_t)SL_STATUS_FAIL;
  }

  tclass = IPV6_TCLASS_FROM_DSCP(DSCP_EF);
  socket_ret = setsockopt(first_breath_socket_id,
                          IPPROTO_IPV6,
                          IPV6_TCLASS,
                          &tclass,
                          (socklen_t)sizeof(tclass));
  if (socket_ret == SOCKET_RETVAL_ERROR) {
    printfBothTime("Failed to set First Breath socket DSCP_EF\n");
    (void)first_breath_close_action(NULL);
    return (uint32_t)SL_STATUS_FAIL;
  }

  snprintf(running_sec_string, sizeof(running_sec_string), "%s", dhms(now_sec()));
  msg_len = snprintf(first_breath_msg,
                     sizeof(first_breath_msg),
                     "{\n"
                     "\"first_breath\":\"true\",\n"
                     "\"running\":\"%s\",\n"
                     "\"device\":\"%s\"\n"
                     "}\n",
                     running_sec_string,
                     fb_lg_device_tag);
  if ((msg_len < 0) || (msg_len >= (int)sizeof(first_breath_msg))) {
    (void)first_breath_close_action(NULL);
    return (uint32_t)SL_STATUS_WOULD_OVERFLOW;
  }

  if (sendto(first_breath_socket_id,
             (uint8_t *)first_breath_msg,
             (size_t)msg_len,
             0,
             (const struct sockaddr *)&first_breath_sockaddr_in6,
             sizeof(first_breath_sockaddr_in6)) == SOCKET_RETVAL_ERROR) {
    printfBothTime("Failed to send First Breath UDP to %s/%d\n",
                   first_breath_ipv6_string,
                   FIRST_BREATH_NOTIFICATION_PORT);
    (void)first_breath_close_action(NULL);
    return (uint32_t)SL_STATUS_TRANSMIT;
  }

  printfBothTime("First Breath UDP sent to %s/%d (%d bytes)\n",
                 first_breath_ipv6_string,
                 FIRST_BREATH_NOTIFICATION_PORT,
                 msg_len);

  if (!app_scheduler_action_schedule(first_breath_close_action,
                                     FIRST_BREATH_SOCKET_CLOSE_DELAY_MS,
                                     0U,
                                     NULL)) {
    printfBothTime("Failed to schedule First Breath socket close\n");
    return (uint32_t)SL_STATUS_FAIL;
  }

  return 0U;
}

static void first_breath_ready_custom_callback(sl_wisun_evt_t *evt)
{
  sl_status_t status = (sl_status_t)evt->evt.fb_ready.status;

  if (status != SL_STATUS_OK) {
    printfBothTime("First Breath ready event status 0x%04x\n", (uint16_t)status);
    return;
  }

  if (!app_scheduler_action_schedule(first_breath_send_action, 0U, 0U, NULL)) {
    printfBothTime("Failed to schedule First Breath UDP send\n");
  }
}

#if defined(SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT)
static void last_gasp_clear_pending(void)
{
  CORE_DECLARE_IRQ_STATE;

  CORE_ENTER_CRITICAL();
  last_gasp_test_pending = false;
  CORE_EXIT_CRITICAL();
}

bool app_fb_lg_schedule_last_gasp(uint32_t enable_delay_ms,
                                  uint32_t reboot_delay_ms)
{
  CORE_DECLARE_IRQ_STATE;
  bool scheduled;

  CORE_ENTER_CRITICAL();
  if (last_gasp_test_pending) {
    CORE_EXIT_CRITICAL();
    return false;
  }
  last_gasp_test_pending = true;
  CORE_EXIT_CRITICAL();

  scheduled = app_scheduler_action_schedule(last_gasp_prepare_and_send_action,
                                            enable_delay_ms,
                                            0U,
                                            (void *)(uintptr_t)reboot_delay_ms);
  if (!scheduled) {
    last_gasp_clear_pending();
  }
  return scheduled;
}

static uint32_t last_gasp_prepare_and_send_action(void *context)
{
  uint32_t reboot_delay_ms = (uint32_t)(uintptr_t)context;
  const char *dest = app_fb_lg_get_udp_notification_destination();
  sl_status_t status;
  char running_sec_string[21];
  int tclass;
  int msg_len;
  int socket_ret;
  int send_ret;

  memset(&last_gasp_sockaddr_in6, 0, sizeof(last_gasp_sockaddr_in6));
  if ((dest == NULL)
      || !sl_wisun_stoip6(dest,
                          strlen(dest),
                          last_gasp_sockaddr_in6.sin6_addr.address)) {
    printfBothTime("Last Gasp invalid UDP destination: %s\n",
                   (dest != NULL) ? dest : "(null)");
    last_gasp_clear_pending();
    return (uint32_t)SL_STATUS_INVALID_PARAMETER;
  }

  sl_wisun_ip6tos(last_gasp_sockaddr_in6.sin6_addr.address,
                  last_gasp_ipv6_string);
  last_gasp_sockaddr_in6.sin6_family = AF_INET6;
  last_gasp_sockaddr_in6.sin6_port = htons(LAST_GASP_NOTIFICATION_PORT);

  // Prepare the EF socket before enabling Last Gasp, which switches the stack
  // into low-energy mode.
  last_gasp_socket_id = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  if (last_gasp_socket_id == SOCKET_INVALID_ID) {
    printfBothTime("Failed to open Last Gasp socket\n");
    last_gasp_clear_pending();
    return (uint32_t)SL_STATUS_FAIL;
  }

  tclass = IPV6_TCLASS_FROM_DSCP(DSCP_EF);
  socket_ret = setsockopt(last_gasp_socket_id,
                          IPPROTO_IPV6,
                          IPV6_TCLASS,
                          &tclass,
                          (socklen_t)sizeof(tclass));
  if (socket_ret == SOCKET_RETVAL_ERROR) {
    printfBothTime("Failed to set Last Gasp socket DSCP_EF\n");
    (void)close(last_gasp_socket_id);
    last_gasp_socket_id = SOCKET_INVALID_ID;
    last_gasp_clear_pending();
    return (uint32_t)SL_STATUS_FAIL;
  }

  snprintf(running_sec_string, sizeof(running_sec_string), "%s", dhms(now_sec()));
  msg_len = snprintf(last_gasp_msg,
                     sizeof(last_gasp_msg),
                     "{\n"
                     "\"last_gasp\":\"true\",\n"
                     "\"running\":\"%s\",\n"
                     "\"device\":\"%s\"\n"
                     "}\n",
                     running_sec_string,
                     fb_lg_device_tag);
  if ((msg_len < 0) || (msg_len >= (int)sizeof(last_gasp_msg))) {
    (void)close(last_gasp_socket_id);
    last_gasp_socket_id = SOCKET_INVALID_ID;
    last_gasp_clear_pending();
    return (uint32_t)SL_STATUS_WOULD_OVERFLOW;
  }

  status = sl_wisun_set_last_gasp(true);
  if (status != SL_STATUS_OK) {
    printfBothTime("Failed to enable Last Gasp: 0x%04x\n", (uint16_t)status);
    (void)close(last_gasp_socket_id);
    last_gasp_socket_id = SOCKET_INVALID_ID;
    last_gasp_clear_pending();
    return (uint32_t)status;
  }

  send_ret = sendto(last_gasp_socket_id,
                    (uint8_t *)last_gasp_msg,
                    (size_t)msg_len,
                    0,
                    (const struct sockaddr *)&last_gasp_sockaddr_in6,
                    sizeof(last_gasp_sockaddr_in6));

  // Keep the socket open and do not perform cleanup after enabling Last Gasp.
  if (!app_scheduler_action_schedule(last_gasp_reboot_action,
                                     reboot_delay_ms,
                                     0U,
                                     NULL)) {
    printfBothTime("Failed to schedule Last Gasp reboot; resetting now\n");
    NVIC_SystemReset();
  }

  if (send_ret == SOCKET_RETVAL_ERROR) {
    return (uint32_t)SL_STATUS_TRANSMIT;
  }

  return (uint32_t)SL_STATUS_OK;
}

static uint32_t last_gasp_reboot_action(void *context)
{
  (void)context;
  NVIC_SystemReset();
  return 0U;
}
#else
bool app_fb_lg_schedule_last_gasp(uint32_t enable_delay_ms,
                                  uint32_t reboot_delay_ms)
{
  (void)enable_delay_ms;
  (void)reboot_delay_ms;
  printfBothTime("Last Gasp is not available in this LFN-only build\n");
  return false;
}
#endif
