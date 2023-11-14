/***************************************************************************//**
* @file app_timestamp.c
* @brief 1 second time stamp and print utility
*
* How to use this code
* inclusion:
* #include "app_timestamp.h"
* once:
* sl_status_t status;
* status = app_timestamp_init();
* the 'app_timestamp' variable is the number of seconds since
*  app_timestamp_init(). It can be used to compute durations in seconds
*  and retrieved using 'now_ms()'
* example:
* printTime("Current app_timestamp = %6lu\n", now_ms());
* results in:
* [  0-00:10:32] Current app_timestamp =   632
* (where the application time stamp is visible in ddd-hh:mm:ss (0:00:10:32) and in seconds (632)
* Several options for printing are available (via macros):
*  print a message in the console:
*  printf("a message\n");
*  print a message in the console with [ddd-hh:mm:ss] prefix:
*  printfTime(formatString, ...);
*  print a message in the RTT traces:
*  printfRTT("a message\n");
*  print a message in the RTT traces with [ddd-hh:mm:ss] prefix:
*  printfRTT("a message\n");
*  print a message in both console and RTT traces:
*  printfBoth("a message\n");
*  print a message in both console and RTT traces with [ddd-hh:mm:ss] prefix:
*  printfBothTime("a message\n");
*
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
#include "sl_sleeptimer.h"
#include "SEGGER_RTT.h"

#define TIMESTAMP_MSG_LEN 1024
extern char timestamped_msg_buffer[TIMESTAMP_MSG_LEN];
extern char *timestamped_msg;

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
#define printfTime(...)     printf("[%s] ", now_str()); printf(__VA_ARGS__)
#ifdef    SEGGER_RTT_H
 #define printfRTT(...)      snprintf(timestamped_msg, TIMESTAMP_MSG_LEN, __VA_ARGS__); SEGGER_RTT_printf(0, timestamped_msg)
 #define printfTimeRTT(...)  snprintf(timestamped_msg, TIMESTAMP_MSG_LEN, __VA_ARGS__); SEGGER_RTT_printf(0, "[%s] %s", now_str(), timestamped_msg)
 #define printfBoth(...)     snprintf(timestamped_msg, TIMESTAMP_MSG_LEN, __VA_ARGS__); SEGGER_RTT_printf(0, timestamped_msg); printf(timestamped_msg)
 #define printfBothTime(...) snprintf(timestamped_msg, TIMESTAMP_MSG_LEN, __VA_ARGS__); SEGGER_RTT_printf(0, "[%s] %s", now_str(), timestamped_msg); printf("[%s] %s", now_str(), timestamped_msg)
#else  /* SEGGER_RTT_H */
 #define printfRTT(...)      printf(__VA_ARGS__)
 #define printfTimeRTT(...)  printf(__VA_ARGS__)
 #define printfBoth(...)     printf(__VA_ARGS__)
 #define printfBothTime(...) printf("[%s] ", now_str()); printf(__VA_ARGS__)
#endif /* SEGGER_RTT_H */
// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------
/**************************************************************************//**
 * Callback handler to increment the application timestamp
 *
 * @param handle The timer handle.
 *
 * @param data An extra parameter for the user application.
 *
 * @return SL_STATUS_OK if successful, an error code otherwise
 *
 * This function increases by 1 the app_timstamp public variable
 *****************************************************************************/
void app_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data);

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
// Application timestamp (seconds)
extern sl_sleeptimer_timestamp_64_t app_timestamp;

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------

/**************************************************************************//**
 * Init the timestamp timer
 *
 * @return SL_STATUS_OK if successful, an error code otherwise
 *
 * This function initializes the sleep timer then starts a 1 second
 * periodic timer.
 *****************************************************************************/
sl_status_t app_timestamp_init(void);

/**************************************************************************//**
 * Sleep Timer seconds timestamp expansion to total days/hours/mins/secs, not
 *  using modulo to limit hours to 24, mins to 60, secs to 60.
 *
 * @param timestamp_secs The timestamp value in seconds.
 *
 * @return The days, hours, minutes, secs total values
 *
 * This function converts a timestamp value in seconds to
 * days, hours, mins, secs
 *****************************************************************************/
sl_status_t d_h_m_s_total(sl_sleeptimer_timestamp_64_t timestamp_secs,
                    uint16_t* days,
                    uint64_t* hours,
                    uint64_t* mins,
                    uint64_t* secs
);

/**************************************************************************//**
 * Sleep Timer seconds timestamp expansion to days/hours/mins/secs
 *
 * @param timestamp_secs The timestamp value in seconds.
 *
 * @return The days, hours%24, minutes%60, secs%60 values
 *
 * Used to format durations in seconds in days/hours/mins/secs
 *****************************************************************************/
sl_status_t d_h_m_s(sl_sleeptimer_timestamp_64_t timestamp_secs,
                    uint16_t* days,
                    uint8_t* hours,
                    uint8_t* mins,
                    uint8_t* secs
);

/**************************************************************************//**
 * Sleep Timer seconds timestamp formatted to string
 *
 * @param timestamp_secs The timestamp value in seconds.
 *
 * @return The formatted string
 *
 * This function formats a timestamp value in seconds to a
 * [ddd:mm:hh:ss] string
 *****************************************************************************/
char* dhms(sl_sleeptimer_timestamp_64_t timestamp_secs);

/**************************************************************************//**
 * Sleep Timer seconds timestamp formatted to string
 *
 * @return The application timestamp formatted string
 *
 * It is mostly used to display the application time stamp in traces
 *****************************************************************************/
char*        now_str     (void);

/**************************************************************************//**
 * Sleep Timer seconds timestamp
 *
 * @return The application timestamp
 *
 * Used to store the application time stamp
 *****************************************************************************/
uint64_t     now_sec      (void);
