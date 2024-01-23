/******************************************************************************

 @file  ll_error_end_causes.c

 @brief This file contains the Link Layer (LL) handlers for the various error
        end causes that result from a PHY task completion and which are common
        for all tasks.

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

#include "../../ll/inc/ll.h"
#include "../../ll/inc/ll_common.h"
#include "bcomdef.h"
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
 * Functions
 */

/*******************************************************************************
 * @fn          llTaskError
 *
 * @brief       This function is used to flag an unexpected task completion
 *              end cause. The action here is to assert a system error and
 *              stop execution.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llTaskError( void )
{
  switch ( taskEndStatus )
  {
    default:
      // Now this really is unexpected!!
      // fatal error - should not happen
      LL_ASSERT( FALSE );

      break;
  }

  return;
}

/*******************************************************************************
 */
