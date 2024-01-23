/******************************************************************************

 @file  hal_drivers.h

 @brief This file contains the interface to the Drivers service.

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2005 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef HAL_DRIVER_H
#define HAL_DRIVER_H

#ifdef __cplusplus
extern "C"
{
#endif

/**************************************************************************************************
 * INCLUDES
 **************************************************************************************************/

#include "hal_types.h"

/**************************************************************************************************
 * CONSTANTS
 **************************************************************************************************/

#define HAL_BUZZER_EVENT                    0x0080
#define PERIOD_RSSI_RESET_EVT               0x0040
#define HAL_LED_BLINK_EVENT                 0x0020
#define HAL_KEY_EVENT                       0x0010

#if defined POWER_SAVING
#define HAL_SLEEP_TIMER_EVENT               0x0004
#define HAL_PWRMGR_HOLD_EVENT               0x0002
#define HAL_PWRMGR_CONSERVE_EVENT           0x0001
#endif

#define HAL_PWRMGR_CONSERVE_DELAY           10
#define PERIOD_RSSI_RESET_TIMEOUT           10

/**************************************************************************************************
 * GLOBAL VARIABLES
 **************************************************************************************************/

extern uint8 Hal_TaskID;

/**************************************************************************************************
 * FUNCTIONS - API
 **************************************************************************************************/

extern void Hal_Init ( uint8 task_id );

/*
 * Process Serial Buffer
 */
extern uint16 Hal_ProcessEvent ( uint8 task_id, uint16 events );

/*
 * Process Polls
 */
extern void Hal_ProcessPoll (void);

/*
 * Initialize HW
 */
extern void HalDriverInit (void);

#ifdef __cplusplus
}
#endif

#endif

/**************************************************************************************************
**************************************************************************************************/
