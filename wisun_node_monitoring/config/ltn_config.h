/***************************************************************************//**
* @file ltn_config.h
* @brief header file to have all default application parameters in a single header file
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

#ifndef LTN_CONFIG_H
#define LTN_CONFIG_H

// for easier comparison we use the same syntax below as in
// app_parameters.h
// app.c
// app_wisun_multicast_ota.h

#ifndef   START_FLASHES_A
  #define START_FLASHES_A     3
#endif /* START_FLASHES_A */

#ifndef   START_FLASHES_B
  #define START_FLASHES_B     5
#endif /* START_FLASHES_B */

#ifndef   APP_VERSION_STRING
  #define APP_VERSION_STRING "F"
#endif /* APP_VERSION_STRING */

#ifndef   NVM3_APP_PARAMS_VERSION
  #define NVM3_APP_PARAMS_VERSION   10000
#endif /* NVM3_APP_PARAMS_VERSION */

#ifndef   MAX_NETWORK_CONFIGS
  #define MAX_NETWORK_CONFIGS 3
#endif /* MAX_NETWORK_CONFIGS*/

#ifndef   DEFAULT_NETWORK_INDEX
  #define DEFAULT_NETWORK_INDEX 0
#endif /* DEFAULT_NETWORK_INDEX */

#ifndef   MULTICAST_OTA_STORE_IN_FLASH
#define   MULTICAST_OTA_STORE_IN_FLASH 1
#endif /* MULTICAST_OTA_STORE_IN_FLASH */

#ifndef   SET_LEAF
#define   SET_LEAF 0
#endif /* SET_LEAF */


/* network */
#ifndef   NETWORK_NAMEs
  #define NETWORK_NAMEs                   {          "large_test_network",                   "rns-wisun-197",                       "rns-wisun-200" }
#endif /* NETWORK_NAMEs */

#ifndef   REG_DOMAINs
  #define REG_DOMAINs                     { SL_WISUN_REGULATORY_DOMAIN_EU,     SL_WISUN_REGULATORY_DOMAIN_EU,         SL_WISUN_REGULATORY_DOMAIN_EU }
#endif /* REG_DOMAINs */

#ifndef   PHY_MODE_IDs
  #define PHY_MODE_IDs                    {                             3,                                 1,                                     1 }
#endif /* PHY_MODE_IDs */

#ifndef   CHAN_PLAN_IDs
  #define CHAN_PLAN_IDs                   {                            33,                                34,                                    32 }
#endif /* CHAN_PLAN_IDs */

#ifndef   NETWORK_SIZEs
  #define NETWORK_SIZEs                   {   SL_WISUN_NETWORK_SIZE_LARGE,      SL_WISUN_NETWORK_SIZE_MEDIUM,           SL_WISUN_NETWORK_SIZE_SMALL }
#endif /* NETWORK_SIZEs */

#ifndef   PREFERRED_PAN_IDs
  #define PREFERRED_PAN_IDs               {                        0xffff,                            0xffff,                                0xffff }
#endif /* PREFERRED_PAN_IDs */

#ifndef   REGULATION
  #define REGULATION  SL_WISUN_REGULATION_NONE
#endif /* REGULATION s */

#ifndef   REGULATION_WARNING_THRESHOLD
  #define REGULATION_WARNING_THRESHOLD 50
#endif /* REGULATION_WARNING_THRESHOLD */

#ifndef   REGULATION_ALERT_THRESHOLD
  #define REGULATION_ALERT_THRESHOLD 100
#endif /* REGULATION_ALERT_THRESHOLD */

/* device */
#ifndef   DEVICE_TYPEs
  #define DEVICE_TYPEs                    {               SL_WISUN_ROUTER,                   SL_WISUN_ROUTER,                       SL_WISUN_ROUTER }
#endif /* DEVICE_TYPEs */

#ifndef   TX_POWER_DDBMs
  #define TX_POWER_DDBMs                  {                             0,                                 0,                                     0 }
#endif /* TX_POWER_DDBMs */

#ifndef   MAX_CHILD_COUNTs
  #define MAX_CHILD_COUNTs                {                            22,                                22,                                    22 }
#endif /* MAX_CHILD_COUNTs */

#ifndef   MAX_NEIGHBOR_COUNTs
  #define MAX_NEIGHBOR_COUNTs             {                            32,                                32,                                    32 }
#endif /* MAX_NEIGHBOR_COUNTs */

#ifndef   MAX_SECURITY_NEIGHBOR_COUNTs
  #define MAX_SECURITY_NEIGHBOR_COUNTs    {                           500,                               500,                                   500 }
#endif /* MAX_SECURITY_NEIGHBOR_COUNTs */

/* Application */
#ifndef   AUTO_SEND_SECs
  #define AUTO_SEND_SECs                  {                       (15*60),                            (1*60),                                (5*60) }
#endif /* AUTO_SEND_SECs */

#ifndef   UDP_NOTIFICATION_DESTINATIONs
  #define UDP_NOTIFICATION_DESTINATIONs   {           "fd00:6172:6d00::1",               "fd00:6172:6d00::1",     "2001:db8:0:2:d47:e4c8:60ad:b4ab" }
#endif /* UDP_NOTIFICATION_DESTINATIONs */

#ifndef   COAP_NOTIFICATION_DESTINATIONs
  #define COAP_NOTIFICATION_DESTINATIONs  {           "fd00:6172:6d00::2",               "fd00:6172:6d00::2",     "2001:db8:0:2:d47:e4c8:60ad:b4ab" }
#endif /* COAP_NOTIFICATION_DESTINATIONs */

#endif /* LTN_CONFIG_H */
