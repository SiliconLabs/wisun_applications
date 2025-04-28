/***************************************************************************//**
* @file app_parameters.h
* @brief header file for application parameters (saved/retrieved from NVM)
* @version 1.0.0
*******************************************************************************
* # License
* <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
*******************************************************************************
*
* SPDX-License-Identifier: Zlib
*
* The licensor of this software is Silicon Laboratories Inc.
*
* This software is provided \'as-is\', without any express or implied
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
*******************************************************************************
*
* EXPERIMENTAL QUALITY
* This code has not been formally tested and is provided as-is.  It is not suitable for production environments.
* This code will not be maintained.
*
******************************************************************************/

#ifndef APP_PARAMETERS_H
#define APP_PARAMETERS_H
// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>

#include "nvm3_default.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
#define NVM3_APP_KEY   0xf012

// Application parameters
typedef struct {
  uint16_t nb_boots;             // Number of reboots since last NVM clear
  uint16_t nb_crashes;           // Number of crashes since last NVM clear
  uint16_t auto_send_sec;        // Notification period in seconds
  uint16_t preferred_pan_id;     // Preferred PAN Id (0xffff for 'none')
  uint8_t  selected_device_type; // SL_WISUN_ROUTER by default
  uint8_t  set_leaf;             // LEAF mode flag
  int16_t  tx_power_ddbm;        // TX Output power in deci-dBm
} app_wisun_parameters_t;

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
extern app_wisun_parameters_t app_parameters;

// -----------------------------------------------------------------------------
//                          Public Function Declarations
// -----------------------------------------------------------------------------

// NVM3 Init/Read/Write of key NVM3_APP_KEY
sl_status_t init_app_parameters();
sl_status_t read_app_parameters();
sl_status_t save_app_parameters();

// Set and Print application parameters
void        print_app_parameters();
void        set_app_parameters_defaults();
sl_status_t set_app_parameter(char* parameter_name, int  value);
sl_status_t get_app_parameter(char* parameter_name, int* value);

#endif  // APP_PARAMETERS_H
