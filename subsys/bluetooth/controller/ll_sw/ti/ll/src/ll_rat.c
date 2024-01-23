/******************************************************************************

 @file  ll_rat.c

 @brief This file contains the Bluetooth Low Energy (BLE) Link
        Layer (LL) software timer management software for RAT.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2009 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*******************************************************************************
 * INCLUDES
 */

#include "bcomdef.h"
#ifdef USE_RCL
#include <ti/drivers/rcl/RCL.h>
#include <ti/drivers/rcl/RCL_Scheduler.h>
#else
#include <ti/drivers/rf/RF.h>
#include "rf_api.h"
#include "rf_hal.h"
#endif
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(inc/hw_memmap.h)
#include "hal_mcu.h"
#include "../../ll/inc/ll_rat.h"
//
#include "rom_jt.h"

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

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
 * PROTOTYPES
 */
uint32 llGetCurrentTime( void );

/*******************************************************************************
 * Functions
 */

/*******************************************************************************
 * @fn          llGetCurrentTime
 *
 * @brief       This routine is used to retrieve the current value of the
 *              radio timer.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      32-bit RAT counter
 */
uint32 llGetCurrentTime( void )
{
#ifdef USE_RCL
  return( RCL_Scheduler_getCurrentTime() );
#else
  return( RF_getCurrentTime() );
#endif
}


/*******************************************************************************
 * @fn          llTimeCompare
 *
 * @brief       This function determines if the first time parameter is greater
 *              than the second time parameter, taking timer counter wrap into
 *              account. If so, TRUE is returned.
 *
 * input parameters
 *
 * @param       time1 - First time.
 * @param       time2 - Second time.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE:  When first parameter is greater than second.
 *              FALSE: When first parmaeter is less than or equal to
 *                     the second.
 */
uint8 llTimeCompare( uint32 time1,
                     uint32 time2 )
{
  if ( time1 > time2 )
  {
    // check if time1 is greater than time2 due to wrap; that is, time2 is
    // actually greater than time1
    // Note: LL_MAX_OVERLAP_TIME_LIMIT value is 8 minutes which is ~half of the
    // 17 minutes overlap of the RF.
    return ( (uint8)((time1-time2) <= LL_MAX_OVERLAP_TIME_LIMIT) );
  }
  else // time2 >= time1
  {
    // check if time2 is greater than time1 due to wrap; that is, time1 is
    // actually greater than time2
    // Note: LL_MAX_OVERLAP_TIME_LIMIT value is 8 minutes which is ~half of the
    // 17 minutes overlap of the RF.
    return( (uint8)((time2-time1) > LL_MAX_OVERLAP_TIME_LIMIT) );
  }
}


/*******************************************************************************
 * @fn          llTimeDelta
 *
 * @brief       This function determines the difference between two time
 *              parameters, taking timer counter wrap into account.
 *
 * input parameters
 *
 * @param       time1 - First time.
 * @param       time2 - Second time.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Difference between time1 and time2.
 */
uint32 llTimeDelta( uint32 time1,
                    uint32 time2 )
{
  if ( time1 >= time2 )
  {
    return( time1 - time2 );
  }
  else // time2 > time1
  {
    return( (LL_MAX_32BIT_TIME - (time2 - time1)) + 1 );
  }
}

/*******************************************************************************
 * @fn          llTimeAbs
 *
 * @brief       This function determines the absolute difference between two time
 *              parameters, taking timer counter wrap into account.
 *
 * input parameters
 *
 * @param       time1 - First time.
 * @param       time2 - Second time.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Absolute difference between time1 and time2.
 */
uint32 llTimeAbs( uint32 time1,
                  uint32 time2 )
{
  if (MAP_llTimeCompare(time1 , time2))
  {
    return (MAP_llTimeDelta(time1,time2));
  }
  else
  {
    return (MAP_llTimeDelta(time2,time1));
  }
}

/*******************************************************************************
 */
