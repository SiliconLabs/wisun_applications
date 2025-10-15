/***************************************************************************//**
* @file app_parameters.c
* @brief Application parameters of the Wi-SUN Node Monitoring example
*******************************************************************************
* # License
* <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
*******************************************************************************
*
* SPDX-License-Identifier: Zlib
*
* The licensor of this software is Silicon Laboratories Inc.
*
* This software is provided 'as-is', without any express or implied
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
******************************************************************************
*
* EXPERIMENTAL QUALITY
* This code has not been formally tested and is provided as-is.  It is not suitable for production environments.
* This code will not be maintained.
*
******************************************************************************/
// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>

#include "sl_common.h"
#include "sl_string.h"
#include "cmsis_nvic_virtual.h"

#include "sl_wisun_types.h"
#include "sl_wisun_api.h"
#include "sl_wisun_config.h"

#include "app.h"
#include "app_parameters.h"
#include "app_timestamp.h"
#include "app_rtt_traces.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

app_wisun_parameters_t app_parameters;

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
void print_app_parameters() {
  printfBothTime("app_parameters.app_params_version   %ld\n", app_parameters.app_params_version);
  printfBothTime("app_parameters.nb_boots             %d\n", app_parameters.nb_boots);
  printfBothTime("app_parameters.nb_crashes           %d\n", app_parameters.nb_crashes);
  printfBothTime("app_parameters.auto_send_sec        %d\n", app_parameters.auto_send_sec);
  printfBothTime("app_parameters.preferred_pan_id     %d\n", app_parameters.preferred_pan_id);
  printfBothTime("app_parameters.selected_device_type %d\n", app_parameters.selected_device_type);
  printfBothTime("app_parameters.set_leaf             %d\n", app_parameters.set_leaf);
  printfBothTime("app_parameters.tx_power_ddbm        %d\n", app_parameters.tx_power_ddbm);
  printfBothTime("app_parameters.max_child_count      %d\n", app_parameters.max_child_count);
  printfBothTime("app_parameters.max_neighbor_count   %d\n", app_parameters.max_neighbor_count);
  printfBothTime("app_parameters.max_security_neighbor_count %d\n", app_parameters.max_security_neighbor_count);
}

void set_app_parameters_defaults() {
  app_parameters.app_params_version   = NVM3_APP_PARAMS_VERSION;
  app_parameters.auto_send_sec        = 15*60;
  app_parameters.preferred_pan_id     = 0xffff;
  app_parameters.selected_device_type = SL_WISUN_ROUTER;
  app_parameters.set_leaf             = 0;
#ifdef    WISUN_CONFIG_TX_POWER
  app_parameters.tx_power_ddbm        = WISUN_CONFIG_TX_POWER;
#else  /* WISUN_CONFIG_TX_POWER */
  app_parameters.tx_power_ddbm        = 200;
#endif /* WISUN_CONFIG_TX_POWER */
  app_parameters.max_child_count      = 22;
  app_parameters.max_neighbor_count   = 32;
  app_parameters.max_security_neighbor_count = 500;
}

sl_status_t init_app_parameters() {
  sl_status_t status;

  status = nvm3_initDefault();
  if (status != SL_STATUS_OK) {
    printfBothTime("ERROR initializing NVM3\n");
  } else {
    status = read_app_parameters();
    if (status != SL_STATUS_OK) {
        set_app_parameters_defaults();
        app_parameters.nb_boots   = 1;
        app_parameters.nb_crashes = 0;
        status = save_app_parameters();
        printfBothTime("application parameters set to default values\n");
    }
    if (status == SL_STATUS_OK) {
        print_app_parameters();
    }
    sl_wisun_config_neighbor_table(
        app_parameters.max_child_count,
        app_parameters.max_neighbor_count,
        app_parameters.max_security_neighbor_count);
  }
  return status;
}

sl_status_t set_app_parameter(char* parameter_name, int value) {
  bool match = false;
  if  (!match) { match = (sl_strcasecmp(parameter_name, "auto_send_sec") == 0);
    if (match) { app_parameters.auto_send_sec = (uint16_t)value; }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "preferred_pan_id") == 0);
    if (match) {
        app_parameters.preferred_pan_id = (uint16_t)value;
        sl_wisun_set_preferred_pan(app_parameters.preferred_pan_id);
        printfBothTime("preferred_pan_id 0x%04x\n", app_parameters.preferred_pan_id);
    }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "selected_device_type") == 0);
    if (match) {
    #ifdef    SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT
        if (value == SL_WISUN_ROUTER) {
            app_parameters.selected_device_type = (uint8_t)value;
        }
    #endif /* SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT */
    #ifdef    SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT
        if (value == SL_WISUN_LFN) {
            app_parameters.selected_device_type = (uint8_t)value;
        }
    #endif /* SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT */
        value = app_parameters.selected_device_type;
    }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "set_leaf") == 0);
    if (match) {
    #ifdef    SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT
        if (value == SL_WISUN_ROUTER) {
            app_parameters.selected_device_type = (uint8_t)value;
        }
    #endif /* SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT */
    #ifdef    SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT
        if (value == SL_WISUN_LFN) {
            app_parameters.selected_device_type = (uint8_t)value;
        }
    #endif /* SL_CATALOG_WISUN_FFN_DEVICE_SUPPORT_PRESENT */
        value = app_parameters.selected_device_type;
    }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "tx_power_ddbm") == 0);
    if (match) {
       app_parameters.tx_power_ddbm = (int16_t)value;
    }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "defaults") == 0);
    if (match) {
        // Set all defaults
        set_app_parameters_defaults();
        // Save default settings if passed a value > 0
        if (value) { save_app_parameters(); }
    }
  }
  /* sl_wisun_config_neighbor_table parameters
  * Each entry in the neighbor table consumes about 450 bytes of RAM.
  * Each entry in the security neighbor table consumes about 50 bytes of RAM.
  */
  if  (!match) { match = (sl_strcasecmp(parameter_name, "max_child_count") == 0);
    if (match) {
        app_parameters.max_child_count = (uint8_t)value;
    }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "max_neighbor_count") == 0);
    if (match) {
        app_parameters.max_neighbor_count = (uint8_t)value;
    }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "max_security_neighbor_count") == 0);
    if (match) {
        app_parameters.max_security_neighbor_count = (uint16_t)value;
    }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "reboot") == 0);
    if (match) {
        // Reboot in <value> ms
        printfBothTime("Rebooting in %d ms\n", value);
        osDelay(value);
        NVIC_SystemReset();
    }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "clear_credential_cache_and_reboot") == 0);
    if (match) { // This is useful to test a full network restart, with credentials cleared on both ends
        sl_wisun_clear_credential_cache();
        // Reboot in <value> ms
        printfBothTime("Rebooting in %d ms\n", value);
        osDelay(value);
        NVIC_SystemReset();
    }
  }
  if  (!match) {
      printfBothTime("ERROR setting '%s': unknown application parameter!\n", parameter_name);
      return SL_STATUS_NOT_SUPPORTED;
  } else {
      printfBothTime("application parameter '%s' set to %d\n", parameter_name, value);
      return SL_STATUS_OK;
  }
}

sl_status_t get_app_parameter(char* parameter_name, int* value) {
  bool match = false;
  *value = 0xffff;
  if  (!match) { match = (sl_strcasecmp(parameter_name, "save") == 0);
    if (match) { *value = (uint16_t)save_app_parameters(); }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "nb_boots") == 0);
    if (match) { *value = (uint16_t)app_parameters.nb_boots; }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "nb_crashes") == 0);
    if (match) { *value = (uint16_t)app_parameters.nb_crashes; }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "auto_send_sec") == 0);
    if (match) { *value = (uint16_t)app_parameters.auto_send_sec; }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "preferred_pan_id") == 0);
    if (match) { *value = (uint16_t)app_parameters.preferred_pan_id; }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "selected_device_type") == 0);
    if (match) { *value = (uint16_t)app_parameters.selected_device_type; }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "set_leaf") == 0);
    if (match) { *value = (uint16_t)app_parameters.set_leaf; }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "tx_power_ddbm") == 0);
    if (match) { *value = (int16_t)app_parameters.tx_power_ddbm; }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "max_child_count") == 0);
    if (match) { *value = (int8_t)app_parameters.max_child_count; }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "max_neighbor_count") == 0);
    if (match) { *value = (int8_t)app_parameters.max_neighbor_count; }
  }
  if  (!match) { match = (sl_strcasecmp(parameter_name, "max_security_neighbor_count") == 0);
    if (match) { *value = (int16_t)app_parameters.max_security_neighbor_count; }
  }
  if  (!match) {
      printfBothTime("ERROR getting '%s': unknown application parameter!\n", parameter_name);
      return SL_STATUS_NOT_SUPPORTED;
  } else {
      printfBothTime("application parameter '%s' %d\n", parameter_name, *value);
      return SL_STATUS_OK;
  }
}

sl_status_t read_app_parameters() {
  sl_status_t status;
  status = nvm3_readData(nvm3_defaultHandle, NVM3_APP_KEY, &app_parameters, sizeof(app_parameters));
  if (status != SL_STATUS_OK) {
      if (status == SL_STATUS_NOT_FOUND) {
          printfBothTime("nvm3_readData(nvm3_defaultHandle, 0x%04x, app_parameters, %d) returned 0x%04lX/NOT_FOUND, (This key is not set yet)\n",
                       NVM3_APP_KEY, sizeof(app_parameters), status);
      } else {
          if (status == SL_STATUS_NVM3_READ_DATA_SIZE) {
              printfBothTime("nvm3_readData(nvm3_defaultHandle, 0x%04x, app_parameters, %d) returned 0x%04lX/SL_STATUS_NVM3_READ_DATA_SIZE, (Trying to read with a length different from actual object size)\n",
                           NVM3_APP_KEY, sizeof(app_parameters), status);
          } else {
                     // What to do here? Assert?
              printfBothTime("nvm3_readData(nvm3_defaultHandle, 0x%04x, app_parameters, %d) returned 0x%04lX, (check sl_status.h)\n",
                       NVM3_APP_KEY, sizeof(app_parameters), status);
          }
      }
  }
  return status;
}

sl_status_t save_app_parameters() {
  sl_status_t status;
  status = nvm3_writeData(nvm3_defaultHandle, NVM3_APP_KEY, &app_parameters, sizeof(app_parameters));
  if (status != SL_STATUS_OK) {
      // What to do here? Assert?
      printfBothTime("nvm3_writeData(nvm3_defaultHandle, 0x%04x, app_parameters, %d) returned 0x%04lX, (check sl_status.h)\n",
                     NVM3_APP_KEY, sizeof(app_parameters), status);
  } else {
      printfBothTime("application parameters saved\n");
  }
  return status;
}


sl_status_t delete_app_parameters() {
  sl_status_t status;
  status = nvm3_deleteObject(nvm3_defaultHandle, NVM3_APP_KEY);
  if (status != SL_STATUS_OK) {
      // What to do here? Assert?
      printfBothTime("nvm3_deleteObject(nvm3_defaultHandle, 0x%04x) returned 0x%04lX, (check sl_status.h)\n",
                     NVM3_APP_KEY, status);
  } else {
      printfBothTime("application parameters deleted\n");
  }
  return status;
}
