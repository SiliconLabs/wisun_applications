// Include this to check LFN-related settings.
#include "sl_component_catalog.h"
#ifdef SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT

#pragma message("For LFN low power, no message should be printed below. Otherwise, solve the following value checks")

#include "sl_wisun_config.h"
#if (WISUN_CONFIG_TX_POWER !=  0)
  #pragma message("WISUN_CONFIG_TX_POWER !=  0")
#endif

#if !defined(WISUN_CONFIG_DEVICE_TYPE)
  #pragma message("WISUN_CONFIG_DEVICE_TYPE not defined: Use the Wi-SUN Configurator to select 'LFN' as 'Device Type")
#endif

#if defined(WISUN_CONFIG_DEVICE_TYPE)
  #if (WISUN_CONFIG_DEVICE_TYPE != SL_WISUN_LFN)
    #pragma message("WISUN_CONFIG_DEVICE_TYPE not defined: Use the Wi-SUN Configurator to select 'LFN' as 'Device Type")
  #endif
  #if !defined(WISUN_CONFIG_DEVICE_PROFILE)
    #pragma message("WISUN_CONFIG_DEVICE_PROFILE not defined: Use the Wi-SUN Configurator to set the 'Device Profile'")
  #endif
#endif

#ifdef OS_CFG_SCHED_ROUND_ROBIN_EN
  #if (OS_CFG_SCHED_ROUND_ROBIN_EN !=  0)
      #pragma message("OS_CFG_SCHED_ROUND_ROBIN_EN !=  0")
  #endif
#endif

#ifdef OS_CFG_STAT_TASK_EN
  #if (OS_CFG_STAT_TASK_EN !=  0)
    #pragma message("OS_CFG_STAT_TASK_EN !=  0")
  #endif
#endif

#ifdef OS_CFG_TASK_PROFILE_EN
  #if (OS_CFG_TASK_PROFILE_EN !=  0)
    #pragma message("OS_CFG_TASK_PROFILE_EN !=  0")
  #endif
#endif

#include "nvm3_default_config.h"
#if (NVM3_DEFAULT_MAX_OBJECT_SIZE  !=  300)
  #pragma message("NVM3_DEFAULT_MAX_OBJECT_SIZE  !=  300")
#endif

#include "sl_clock_manager_tree_config.h"
#if (SL_CLOCK_MANAGER_EUSART0CLK_SOURCE != CMU_EUSART0CLKCTRL_CLKSEL_LFXO)
  #pragma Message("SL_CLOCK_MANAGER_EUSART0CLK_SOURCE != CMU_EUSART0CLKCTRL_CLKSEL_LFXO")
#endif

#include "sl_clock_manager_oscillator_config.h"
#if (SL_CLOCK_MANAGER_LFXO_EN !=  1)
  #pragma message("SL_CLOCK_MANAGER_LFXO_EN !=  1")
#endif

#if (SL_POWER_MANAGER_INIT_EMU_EM2_DEBUG_ENABLE !=  0)
  #pragma message("SL_POWER_MANAGER_INIT_EMU_EM2_DEBUG_ENABLE !=  0")
#endif

#include "sl_rail_util_pti_config.h"
#if (SL_RAIL_UTIL_PTI_BAUD_RATE_HZ !=  3200000)
  #pragma message("SL_RAIL_UTIL_PTI_BAUD_RATE_HZ !=  3200000")
#endif

#include "sl_sleeptimer_config.h"
#if (SL_SLEEPTIMER_WALLCLOCK_CONFIG !=  1)
  #pragma message("SL_SLEEPTIMER_WALLCLOCK_CONFIG !=  1")
#endif

#include "sl_wisun_coap_config.h"
#if (SL_WISUN_COAP_RESOURCE_HND_MAX_RESOURCES  !=  50U)
  #pragma message("SL_WISUN_COAP_RESOURCE_HND_MAX_RESOURCES  !=  50U")
#endif

#if (SL_WISUN_COAP_RESOURCE_HND_STACK_SIZE_WORD  !=  1024)
  #pragma message("SL_WISUN_COAP_RESOURCE_HND_STACK_SIZE_WORD  !=  1024")
#endif

#if (SL_WISUN_COAP_RESOURCE_HND_VERBOSE_MODE_ENABLE !=  0)
  #pragma message("SL_WISUN_COAP_RESOURCE_HND_VERBOSE_MODE_ENABLE !=  0")
#endif

#if (SL_WISUN_COAP_NOTIFY_SERVICE_ENABLE !=  0)
  #pragma message("SL_WISUN_COAP_NOTIFY_SERVICE_ENABLE !=  0")
#endif

#if (SL_WISUN_OTA_DFU_AUTO_INSTALL_ENABLED  !=  0)
  #pragma message("SL_WISUN_OTA_DFU_AUTO_INSTALL_ENABLED  !=  0U")
#endif

#if (SL_WISUN_OTA_DFU_HOST_NOTIFY_ENABLED  !=  0)
  #pragma message("SL_WISUN_OTA_DFU_HOST_NOTIFY_ENABLED  !=  0U")
#endif

#include "sl_iostream_eusart_vcom_config.h"
#if (SL_IOSTREAM_EUSART_INSTANCE_RESTRICT_ENERGY_MODE_TO_ALLOW_RECEPTION  !=  0)
  #pragma message("SL_IOSTREAM_EUSART_INSTANCE_RESTRICT_ENERGY_MODE_TO_ALLOW_RECEPTION  !=  0")
#endif

#if (SL_IOSTREAM_EUSART_VCOM_ENABLE_HIGH_FREQUENCY  !=  0)
  #pragma message("SL_IOSTREAM_EUSART_VCOM_ENABLE_HIGH_FREQUENCY  !=  0")
#endif

#if (SL_IOSTREAM_EUSART_VCOM_BAUDRATE  !=  9600)
  #pragma message("SL_IOSTREAM_EUSART_VCOM_BAUDRATE  !=  '9600'")
#endif

#if (SL_IOSTREAM_EUSART_VCOM_RESTRICT_ENERGY_MODE_TO_ALLOW_RECEPTION  !=  0)
  #pragma message("SL_IOSTREAM_EUSART_VCOM_RESTRICT_ENERGY_MODE_TO_ALLOW_RECEPTION  !=  0")
#endif

#if (SL_IOSTREAM_EUSART_VCOM_FLOW_CONTROL_TYPE  !=  SL_IOSTREAM_EUSART_UART_FLOW_CTRL_NONE)
  #pragma message("SL_IOSTREAM_EUSART_VCOM_FLOW_CONTROL_TYPE  !=  SL_IOSTREAM_EUSART_UART_FLOW_CTRL_NONE")
#endif

#ifdef SL_IOSTREAM_USART_VCOM_CONVERT_BY_DEFAULT_LF_TO_CRLF
  #if (SL_IOSTREAM_USART_VCOM_CONVERT_BY_DEFAULT_LF_TO_CRLF  !=  1)
    #pragma message("SL_IOSTREAM_USART_VCOM_CONVERT_BY_DEFAULT_LF_TO_CRLF  !=  '1'")
  #endif
#endif

#if (SL_IOSTREAM_USART_VCOM_FLOW_CONTROL_TYPE  !=  SL_IOSTREAM_USART_UART_FLOW_CTRL_NONE)
  #pragma message("SL_IOSTREAM_USART_VCOM_FLOW_CONTROL_TYPE  !=  SL_IOSTREAM_USART_UART_FLOW_CTRL_NONE")
#endif

#include "sl_sleeptimer_config.h"

#if (SL_SLEEPTIMER_WALLCLOCK_CONFIG  !=  1)
  #pragma message("SL_SLEEPTIMER_WALLCLOCK_CONFIG  !=  '1'")
#endif

#include "sl_wisun_app_core.h"
#if (SL_WISUN_APP_CORE_THREAD_LP_DISPATCH_MS  ==  1000)
  #pragma message("(SL_WISUN_APP_CORE_THREAD_LP_DISPATCH_MS  ==  1000")
#endif

#ifdef    SL_CATALOG_SIMPLE_BUTTON_PRESENT
  #pragma message("SL_CATALOG_SIMPLE_BUTTON_PRESENT is not recommended for LFN, unless you agree on having the related GPIO consumption")
#endif

#ifdef    SL_CATALOG_SIMPLE_LED_PRESENT
  #pragma message("SL_CATALOG_SIMPLE_LED_PRESENT is not recommended for LFN, unless you agree on having the related LED consumption")
#endif

#endif /* SL_CATALOG_WISUN_LFN_DEVICE_SUPPORT_PRESENT */
