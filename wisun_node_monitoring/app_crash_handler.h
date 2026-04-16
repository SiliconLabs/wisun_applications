/***************************************************************************//**
 * @file app_crash_handler.h
 * @brief Header for crash handler for Wi-SUN applications, 
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

#include "sl_wisun_crash_handler.h"

// Crash info string, filled by sl_wisun_check_previous_crash()
extern char crash_info_string[];

int sl_wisun_check_previous_crash(void);
