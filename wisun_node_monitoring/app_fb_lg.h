/***************************************************************************//**
* @file app_fb_lg.h
* @brief First Breath and Last Gasp helpers for node monitoring
*******************************************************************************
* SPDX-License-Identifier: Zlib
******************************************************************************/

#ifndef APP_FB_LG_H
#define APP_FB_LG_H

#include <stdbool.h>
#include <stdint.h>

#define FIRST_BREATH_NOTIFICATION_PORT 1238U
#define LAST_GASP_NOTIFICATION_PORT    1239U

/** Initialize the First Breath event extension. */
void app_fb_lg_init(void);

/** Return the UDP notification destination used by First Breath and Last Gasp. */
const char *app_fb_lg_get_udp_notification_destination(void);

/** Schedule one Last Gasp test. Returns false if unsupported or already pending. */
bool app_fb_lg_schedule_last_gasp(uint32_t enable_delay_ms,
                                  uint32_t reboot_delay_ms);

#endif // APP_FB_LG_H
