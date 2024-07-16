/***************************************************************************//**
* @file app.h
* @brief header file for application
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

#ifndef APP_H
#define APP_H
// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include "cmsis_os2.h"
#include "sl_component_catalog.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
#ifndef    SL_BOARD_NAME
  /* For Custom boards:
   *   We recommend setting this to the company-internal board name
   */
  #define  SL_BOARD_NAME "Custom_Board"
#endif /*  SL_BOARD_NAME */

#define DEFINE_string(s)   #s

#define HISTORY
//#define LIST_RF_CONFIGS
// UDP and TCP server need to be set to either SO_NONBLOCK or SO_EVENT_MODE, if defined
// Comment the lines if the corresponding server is not required
#define WITH_UDP_SERVER    SO_NONBLOCK
#define WITH_TCP_SERVER    SO_EVENT_MODE

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Public Function Declarations
// -----------------------------------------------------------------------------
/**************************************************************************//**
 * @brief Application task function
 * @details This function is the main app task implementation
 * @param[in] args arguments
 *****************************************************************************/
void app_task(void *args);
void app_reset_statistics(void);
void refresh_parent_tag(void);

#endif  // APP_H
