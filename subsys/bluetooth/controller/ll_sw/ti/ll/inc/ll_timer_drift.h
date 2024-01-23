/******************************************************************************

 @file  ll_timer_drift.h

 @brief This file contains the Link Layer (LL) types, constants,
        API's etc. for the Bluetooth Low Energy (ULE) Controller
        Timer Drift routines.

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

#ifndef LL_TIMER_DRIFT_H
#define LL_TIMER_DRIFT_H

#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * INCLUDES
 */
#include "../../ll/inc/ble.h"

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

#define LL_SYNTH_CALIBRATION                (RAT_TICKS_IN_256US)
#define LL_RX_SETTLE_TIME                   (RAT_TICKS_IN_64US)
#define LL_RX_RAMP_OVERHEAD                 (LL_SYNTH_CALIBRATION + LL_RX_SETTLE_TIME)
#define LL_JITTER_CORRECTION                (RAT_TICKS_IN_16US)
#define LL_RX_SYNCH_OVERHEAD                (RAT_TICKS_IN_140US)
#define LL_RX_SYNCH_OVERHEAD_CODED          (RAT_TICKS_IN_900US)

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
 * API
 */

extern uint16 llCalcScaFactor( uint8 centralSCA );

#ifdef __cplusplus
}
#endif

#endif /* LL_TIMER_DRIFT_H */
