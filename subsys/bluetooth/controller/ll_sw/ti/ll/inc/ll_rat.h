/******************************************************************************

 @file  ll_rat.h

 @brief This file contains the Link Layer (LL) types, constants,
        API's etc. for the Bluetooth Low Energy (BLE) Controller
        Radio Access Timer (RAT).

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2009 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*********************************************************************
 *
 * WARNING!!!
 *
 * THE API'S FOUND IN THIS FILE ARE FOR INTERNAL STACK USE ONLY!
 * FUNCTIONS SHOULD NOT BE CALLED DIRECTLY FROM APPLICATIONS, AND ANY
 * CALLS TO THESE FUNCTIONS FROM OUTSIDE OF THE STACK MAY RESULT IN
 * UNEXPECTED BEHAVIOR.
 *
 */

#ifndef LL_RAT_H
#define LL_RAT_H

#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * INCLUDES
 */

#include "bcomdef.h"

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

#define LL_MAX_32BIT_TIME_IN_625US     0x07A12000  // 32s in 625us ticks (LSTO limit)
#define LL_MAX_OVERLAP_TIME_LIMIT      0x7270E000  // 8 minutes in 625us ticks. chosen to be half of 17 minutes RF overlap.
#define LL_MAX_32BIT_TIME              0xFFFFFFFF

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

/*******************************************************************************
 * Functions
 */

extern uint32 llGetCurrentTime( void );

extern uint8 llTimeCompare( uint32 time1, uint32 time2 );

extern uint32 llTimeDelta( uint32 time1, uint32 time2 );

extern uint32 llTimeAbs( uint32 time1, uint32 time2 );

#ifdef __cplusplus
}
#endif

#endif /* LL_RAT_H */
