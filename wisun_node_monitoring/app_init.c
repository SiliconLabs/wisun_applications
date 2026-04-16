/***************************************************************************//**
* @file app_init.c
* @brief Application init
*******************************************************************************
* # License
* <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
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
*******************************************************************************
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
#include <assert.h>

#include "sl_hal_sysrtc.h"
#include "sl_main_init.h"
#include "cmsis_os2.h"
#include "sl_cmsis_os2_common.h"

#include "app.h"
#include "sl_component_catalog.h"

#ifdef    SL_CATALOG_WISUN_COAP_PRESENT
  #include "app_coap.h"
#endif

#if __has_include("sl_wisun_crash_handler.h")
  #include "sl_wisun_crash_handler.h"
#endif

#if __has_include("app_action_scheduler.h")
    #include "app_action_scheduler.h"
#endif

#if __has_include("app_coap.h")
  #include "app_coap.h"
#endif

#if __has_include("sl_mx25_flash_shutdown.h")
  #include "sl_mx25_flash_shutdown.h"
#endif

#if __has_include("btl_interface.h")
  #include "btl_interface.h"
#endif

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
#define APP_STACK_SIZE_BYTES   (5*2048UL)

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
void app_init(void)
{
  int i;

  // print an easily visible banner to detect reboots
  const char* banner = "============ app_init() ============";
  printf("\n\n");
  for (i=5;i>0;i--)  { printf("%s\n", banner+i); }
  for (i=0;i<=5;i++) { printf("%s\n", banner+i); }
  printf("\n\n");

  sl_hal_sysrtc_config_t sysrtc_config = SYSRTC_CONFIG_DEFAULT;
  uint32_t status;

#ifdef BTL_INTERFACE_H
  //bootloader_deinit();
#endif /* BTL_INTERFACE_H */

#ifdef    SL_MX25_FLASH_SHUTDOWN_H
  //sl_mx25_flash_shutdown(); // Needs refining to allow bootloader_getStorageInfo(), etc. in OTA code
#endif /* SL_MX25_FLASH_SHUTDOWN_H */

  status = sl_hal_sysrtc_get_status();
  if (!(status & SYSRTC_STATUS_RUNNING)) {
    /**
     * SYSRTC is needed for RAIL timer synchronization. If it's not started
     * by sleep timer, it must be started manually. Peripheral bus clock is
     * enabled by device init/clock manager.
     */
    sl_hal_sysrtc_init(&sysrtc_config);
    sl_hal_sysrtc_enable();
  }

#ifdef    SL_WISUN_CRASH_HANDLER_H
  sl_wisun_crash_handler_init();
#endif /* SL_WISUN_CRASH_HANDLER_H */

#ifdef    SL_CATALOG_WISUN_COAP_PRESENT
  #ifdef APP_COAP_H
    #if SL_WISUN_COAP_RESOURCE_HND_MAX_RESOURCES < 20
      #pragma message("SL_WISUN_COAP_RESOURCE_HND_MAX_RESOURCES needs to be increased to avoid an assert during app_coap_resources_init(); Count the number of calls to sl_wisun_coap_rhnd_resource_add)( in app_coap.c to get an idea of the required value)")
    #endif
    app_coap_resources_init();
  #endif /* APP_COAP_H */
#endif /* SL_CATALOG_WISUN_COAP_PRESENT */

#ifdef    APP_ACTION_SCHEDULER_H
  app_scheduler_action_init();
#endif /* APP_ACTION_SCHEDULER_H */

  /* Creating App main thread */
  const osThreadAttr_t app_task_attr = {
    .name        = "app_task",
    .attr_bits   = osThreadDetached,
    .cb_mem      = NULL,
    .cb_size     = 0,
    .stack_mem   = NULL,
    .stack_size  = APP_STACK_SIZE_BYTES,
    .priority    = osPriorityNormal,
    .tz_module   = 0
  };

  printf("%s/%s starting '%s' thread with stack_size of %4ld words\n",
        __FILE__, __FUNCTION__,
        app_task_attr.name,
        app_task_attr.stack_size);

  osThreadId_t app_thr_id = osThreadNew(app_task,
                                        NULL,
                                        &app_task_attr);
  assert(app_thr_id != NULL);

  printf("'%s' thread started\n",
        app_task_attr.name);
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
