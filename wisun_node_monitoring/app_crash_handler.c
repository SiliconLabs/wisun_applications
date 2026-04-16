/***************************************************************************//**
 * @file app_crash_handler.c
 * @brief Crash handler for Wi-SUN applications, 
 * used with wisun_crash_handler component
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include <stdio.h>
#include "app_crash_handler.h"

// Crash info string, filled by sl_wisun_check_previous_crash()
#define SL_CRASH_STR_MAX_LEN            300
char crash_info_string[SL_CRASH_STR_MAX_LEN];

int sl_wisun_check_previous_crash(void) {
  int ret = 0;
  const sl_wisun_crash_t *crash;
  crash = sl_wisun_crash_handler_read();
  if (crash) {
    switch (crash->type) {
      case SL_WISUN_CRASH_TYPE_ASSERT:
        snprintf(crash_info_string, SL_CRASH_STR_MAX_LEN,
          "[ASSERT in %s on line %u]\r\n",
          crash->u.assert.file,
          crash->u.assert.line);
        break;
      case SL_WISUN_CRASH_TYPE_RAIL_ASSERT:
        snprintf(crash_info_string, SL_CRASH_STR_MAX_LEN,
          "[RAIL ASSERT %lu]\r\n",
          crash->u.rail_assert.error_code);
        break;
      case SL_WISUN_CRASH_TYPE_STACK_OVERFLOW:
        snprintf(crash_info_string, SL_CRASH_STR_MAX_LEN,
          "[STACK OVERFLOW failure in task \"%s\"]\r\n",
          crash->u.stack_overflow.task);
        break;
      case SL_WISUN_CRASH_TYPE_STACK_PROTECTOR:
        snprintf(crash_info_string, SL_CRASH_STR_MAX_LEN,
          "[STACK PROTECTOR failure in 0x%08lx]\r\n",
          crash->u.stack_protector.lr);
        break;
      case SL_WISUN_CRASH_TYPE_FAULT:
        // In case of SL_WISUN_CRASH_TYPE_FAULT, look for LR in the .map file as the source of the fault
        #define CRASH_TYPE_FAULT_FORMAT \
          "[FAULT CFSR: 0x%08lx\r\n"                                        \
          "R0:   0x%08lx, R1:    0x%08lx, R2:   0x%08lx, R3:   0x%08lx\r\n" \
          "R12:  0x%08lx, LR:    0x%08lx, RET:  0x%08lx, XPSR: 0x%08lx\r\n" \
          "HFSR: 0x%08lx, MMFAR: 0x%08lx, BFAR: 0x%08lx, AFSR: 0x%08lx]\r\n"
        snprintf(crash_info_string, SL_CRASH_STR_MAX_LEN,
           CRASH_TYPE_FAULT_FORMAT,
           crash->u.fault.cfsr,
           crash->u.fault.r0, crash->u.fault.r1,
           crash->u.fault.r2, crash->u.fault.r3,
           crash->u.fault.r12, crash->u.fault.lr,
           crash->u.fault.return_address, crash->u.fault.xpsr,
           crash->u.fault.hfsr, crash->u.fault.mmfar,
           crash->u.fault.bfar, crash->u.fault.afsr
        );
        break;
      case SL_WISUN_CRASH_TYPE_CRUN_ERROR:
        snprintf(crash_info_string, SL_CRASH_STR_MAX_LEN,
          "[C-RUN error 0x%08lx]\r\n",
          crash->u.crun_error.error_code);
        break;
      case SL_WISUN_CRASH_TYPE_EXIT:
        snprintf(crash_info_string, SL_CRASH_STR_MAX_LEN,
          "[EXIT status %d]\r\n",
          crash->u.exit.status);
        break;
      default:
        break;
    }
    ret = crash->type;
    sl_wisun_crash_handler_clear();
  }
  return ret;
}
