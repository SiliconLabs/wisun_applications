/***************************************************************************//**
* @file app_check_neighbors.h
* @brief Header file for resources to retrieve neighbor information
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
#include <stdio.h>
#include <assert.h>
#include "cmsis_os2.h"
#include "sl_cmsis_os2_common.h"
#include "sl_wisun_api.h"


char *  _neighbor_info_str(sl_wisun_neighbor_info_t neighbor_info, uint8_t index, char *tag);
uint8_t app_get_neighbor_info(sl_wisun_neighbor_type_t neighbor_type,
                                                 uint8_t *index,
                                                 char *tag,
                                                 sl_wisun_neighbor_info_t * neighbor_info);
char * app_parent_info_str(void);
char * app_child_info_str(uint8_t index);
char * app_neighbor_info_str(uint8_t index);
