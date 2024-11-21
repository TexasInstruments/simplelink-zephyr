/******************************************************************************

 @file  osal_pwrmgr.c

 @brief This file contains the OSAL Power Management API.

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2004 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*********************************************************************
 * INCLUDES
 */

#include "comdef.h"
#include "onboard.h"
#include "osal.h"
#include "osal_tasks.h"
#include "osal_timers.h"
#include "osal_pwrmgr.h"

#include <icall.h>

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

/* This global variable stores the power management attributes.
 */
pwrmgr_attribute_t pwrmgr_attribute;
uint8 pwrmgr_initialized = FALSE;

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

/*********************************************************************
 * LOCAL FUNCTION PROTOTYPES
 */

/*********************************************************************
 * FUNCTIONS
 *********************************************************************/

/*********************************************************************
 * @fn      osal_pwrmgr_init
 *
 * @brief   Initialize the power management system.
 *
 * @param   none.
 *
 * @return  none.
 */
void osal_pwrmgr_init( void )
{
  pwrmgr_attribute.pwrmgr_task_state = 0;            // Cleared.  All set to conserve
  pwrmgr_initialized = TRUE;
}

/*********************************************************************
 * @fn      osal_pwrmgr_task_state
 *
 * @brief   This function is called by each task to state whether or
 *          not this task wants to conserve power.
 *
 * @param   task_id - calling task ID.
 *          state - whether the calling task wants to
 *          conserve power or not.
 *
 * @return  TRUE if power is required; FALSE is power is not required
 */
uint8 osal_pwrmgr_task_state( uint8 task_id, uint8 state )
{
  halIntState_t intState;
  bool pwrRequired = TRUE;

  if ( task_id >= tasksCnt )
    return ( pwrRequired );

  if ( !pwrmgr_initialized )
  {
    /* If voting is made before this module is initialized,
     * pwrmgr_task_state will reset later when the module is
     * initialized, and cause incorrect activity count.
     */
    return ( pwrRequired );
  }

  HAL_ENTER_CRITICAL_SECTION( intState );

  if ( state == PWRMGR_CONSERVE )
  {
    uint16 cache = pwrmgr_attribute.pwrmgr_task_state;
    // Clear the task state flag
    pwrmgr_attribute.pwrmgr_task_state &= ~(1 << task_id );
    if (cache != 0 && pwrmgr_attribute.pwrmgr_task_state == 0)
    {
      /* Decrement activity counter */
      pwrRequired = ICall_pwrUpdActivityCounter(FALSE);
    }
  }
  else
  {
    if (pwrmgr_attribute.pwrmgr_task_state == 0)
    {
      /* Increment activity counter */
      (void)ICall_pwrUpdActivityCounter(TRUE);

    }
    // Set the task state flag
    pwrmgr_attribute.pwrmgr_task_state |= (1 << task_id);
  }

  HAL_EXIT_CRITICAL_SECTION( intState );

  return ( pwrRequired );
}

/*********************************************************************
*********************************************************************/
