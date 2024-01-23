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
#ifndef USE_RCL
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(inc/hw_rfc_rat.h)
#endif //!USE_RCL

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */
#ifndef USE_RCL
#define RAT_BASE                       RFC_RAT_BASE

// Registers
#define RAT_CONFIG_REG                 HWREG(RAT_BASE + RFC_RAT_O_RATCFG)
#define RAT_COUNT_REG                  HWREG(RAT_BASE + RFC_RAT_O_RATCNT)
#define RAT_ADJUST_REG                 HWREG(RAT_BASE + RFC_RAT_O_RATADJ)
#define RAT_ARM_CAPTURE_REG            HWREG(RAT_BASE + RFC_RAT_O_RATARMSET)
#define RAT_ARM_CLEAR_REG              HWREG(RAT_BASE + RFC_RAT_O_RATARMCLR)
#define RAT_OUT_EVT_REG                HWREG(RAT_BASE + RFC_RAT_O_RATEV)
#define RAT_OUT_MODE_CONFIG_REG        HWREG(RAT_BASE + RFC_RAT_O_RATOUT)
#define RAT_CHAN_0_CONFIG_REG          HWREG(RAT_BASE + RFC_RAT_O_RATCH0CFG)
#define RAT_CHAN_1_CONFIG_REG          HWREG(RAT_BASE + RFC_RAT_O_RATCH1CFG)
#define RAT_CHAN_2_CONFIG_REG          HWREG(RAT_BASE + RFC_RAT_O_RATCH2CFG)
#define RAT_CHAN_3_CONFIG_REG          HWREG(RAT_BASE + RFC_RAT_O_RATCH3CFG)
#define RAT_CHAN_4_CONFIG_REG          HWREG(RAT_BASE + RFC_RAT_O_RATCH4CFG)
#define RAT_CHAN_5_CONFIG_REG          HWREG(RAT_BASE + RFC_RAT_O_RATCH5CFG)
#define RAT_CHAN_6_CONFIG_REG          HWREG(RAT_BASE + RFC_RAT_O_RATCH6CFG)
#define RAT_CHAN_7_CONFIG_REG          HWREG(RAT_BASE + RFC_RAT_O_RATCH7CFG)
#define RAT_CHAN_0_CAPT_COMP_REG       HWREG(RAT_BASE + RFC_RAT_O_RATCH0VAL)
#define RAT_CHAN_1_CAPT_COMP_REG       HWREG(RAT_BASE + RFC_RAT_O_RATCH1VAL)
#define RAT_CHAN_2_CAPT_COMP_REG       HWREG(RAT_BASE + RFC_RAT_O_RATCH2VAL)
#define RAT_CHAN_3_CAPT_COMP_REG       HWREG(RAT_BASE + RFC_RAT_O_RATCH3VAL)
#define RAT_CHAN_4_CAPT_COMP_REG       HWREG(RAT_BASE + RFC_RAT_O_RATCH4VAL)
#define RAT_CHAN_5_CAPT_COMP_REG       HWREG(RAT_BASE + RFC_RAT_O_RATCH5VAL)
#define RAT_CHAN_6_CAPT_COMP_REG       HWREG(RAT_BASE + RFC_RAT_O_RATCH6VAL)
#define RAT_CHAN_7_CAPT_COMP_REG       HWREG(RAT_BASE + RFC_RAT_O_RATCH7VAL)
#endif
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
