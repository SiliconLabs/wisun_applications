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

#if __has_include("ltn_config.h")
#include "ltn_config.h"
#endif

#ifndef   TX_POWER_DDBM
  #ifndef WISUN_CONFIG_TX_POWER
    #define TX_POWER_DDBM  0
  #else /* WISUN_CONFIG_TX_POWER */
    #define TX_POWER_DDBM  WISUN_CONFIG_TX_POWER
  #endif /* WISUN_CONFIG_TX_POWER */
#endif /* TX_POWER_DDBM */
#ifndef  NVM3_APP_PARAMS_VERSION
  #define NVM3_APP_PARAMS_VERSION   10000
#endif /* NVM3_APP_PARAMS_VERSION */
#ifndef  AUTO_SEND_SEC
  #define AUTO_SEND_SEC             15*60
#endif /* AUTO_SEND_SEC */
#ifndef  PREFERRED_PAN_ID
  #define PREFERRED_PAN_ID         0xffff
#endif /* PREFERRED_PAN_ID */
#ifndef  SELECTED_DEVICE_TYPE
  #define SELECTED_DEVICE_TYPE          SL_WISUN_ROUTER
#endif /* SELECTED_DEVICE_TYPE */
#ifndef  SET_LEAF
  #define SET_LEAF                      0
#endif /* SET_LEAF */
#ifndef  MAX_CHILD_COUNT
  #define MAX_CHILD_COUNT              22
#endif /* MAX_CHILD_COUNT */
#ifndef  MAX_NEIGHBOR_COUNT
  #define MAX_NEIGHBOR_COUNT           32
#endif /* MAX_NEIGHBOR_COUNT */
#ifndef  MAX_SECURITY_NEIGHBOR_COUNT
  #define MAX_SECURITY_NEIGHBOR_COUNT 500
#endif /* MAX_SECURITY_NEIGHBOR_COUNT */


// Application parameters
typedef struct {
  uint32_t app_params_version;   // Read at boot, set all to defaults
                                 //  if not matching NVM3_APP_VERSION
                                 //    This is to avoid clearing the Wi-SUN stack cache
  uint16_t nb_boots;             // Number of reboots since last NVM clear
  uint16_t nb_crashes;           // Number of crashes since last NVM clear
  uint16_t auto_send_sec;        // Notification period in seconds
  uint16_t preferred_pan_id;     // Preferred PAN Id (0xffff for 'none')
  uint8_t  selected_device_type; // SL_WISUN_ROUTER by default
  uint8_t  set_leaf;             // LEAF mode flag
  int16_t  tx_power_ddbm;        // TX Output power in deci-dBm
  /* sl_wisun_config_neighbor_table parameters
  * max_security_neighbor_count(300) >= max_neighbor_count(32) > max_child_count(22)
  * Each entry in the neighbor table consumes about 450 bytes of RAM.
  * Each entry in the security neighbor table consumes about 50 bytes of RAM.
  */
  uint8_t  max_child_count;      //  Maximum number of RPL children
  uint8_t  max_neighbor_count;   //  Maximum number of neighbors including children, parent, and temporary neighbors
  uint16_t max_security_neighbor_count; // Maximum number of neighbors in the security table
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
sl_status_t delete_app_parameters();

// Set and Print application parameters
void        print_app_parameters();
void        set_app_parameters_defaults();
sl_status_t set_app_parameter(char* parameter_name, int  value);
sl_status_t get_app_parameter(char* parameter_name, int* value);

#endif  // APP_PARAMETERS_H
