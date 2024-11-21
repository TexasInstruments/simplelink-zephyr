/******************************************************************************

 @file  hal_assert.c

  @brief This file contains the Hardware Abstraction Layer (HAL) Assert APIs
         used to handle asserts in system software. The assert handler, and
         its behavior, depend on the build time define (please see header file).

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2006 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*******************************************************************************
 * INCLUDES
 */

#include "hal_assert.h"
#include "hal_types.h"
#include "hal_defs.h"
#include "onboard.h"

/*******************************************************************************
 * MACROS
 */

// Compile Time Assertions - Integrity check of type sizes.
HAL_ASSERT_SIZE(  int8, 1);
HAL_ASSERT_SIZE( uint8, 1);
HAL_ASSERT_SIZE( int16, 2);
HAL_ASSERT_SIZE(uint16, 2);
HAL_ASSERT_SIZE( int32, 4);
HAL_ASSERT_SIZE(uint32, 4);

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

// HAL Assert callback pointer
assertCback_t halAssertCback = (assertCback_t)halAssertSpinlock;

// optional general purpose subcause - the value dpepends on assert cause.
uint8 assertSubcause = HAL_ASSERT_SUBCAUSE_NONE;

// used to decide how FALSE is handled (see halAssertHandlerExt)
uint8 legacyMode = HAL_ASSERT_LEGACY_MODE_ENABLED;


/*******************************************************************************
 * @fn          halAssertInit API
 *
 * @brief       This function is used by the Stack code to override the build-
 *              time initialization values of assert Callback and Legacy Mode,
 *              thus allowing a project to change the defaults without having
 *              to change the common (shared) header file hal_assert.h. The
 *              default values are: NULL and HAL_ASSERT_LEGACY_MODE_ENABLED.
 *
 * input parameters
 *
 * @param       initAssertCback: Pointer to HAL Assert callback.
 * @param       initLegacyMode:  HAL_ASSERT_LEGACY_MODE_ENABLED |
 *                               HAL_ASSERT_LEGACY_MODE_DISABLED
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void halAssertInit( assertCback_t initAssertCback, uint8 initLegacyMode )
{
  // check if the callback pointer is invalid
  if ( initAssertCback == NULL )
  {
    // set the assert callback pointer to a valid handler
    halAssertCback = (assertCback_t)halAssertSpinlock;
  }
  else // callback pointer is valid
  {
    // set the assert callback pointer
    halAssertCback = initAssertCback;
  }

  // set the legacyMode
  legacyMode = initLegacyMode;

  return;
}


/*******************************************************************************
 * @fn          halAssertHandler API
 *
 * @brief       This function is to trap software execution. The assert action
 *              depends on the defines:
 *
 *              HAL_ASSERT_RESET  - Reset the device.
 *              HAL_ASSERT_SPIN   - Spinlock.
 *              Otherwise:        - Just return.
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
void halAssertHandler( void )
{
#if defined( HAL_ASSERT_RESET )
  SystemReset();
#elif defined( HAL_ASSERT_SPIN )
  halAssertSpinlock();
#endif

  return;
}


/*******************************************************************************
 * @fn          halAssertHandlerExt API
 *
 * @brief       This function is the Extended halAssertHandler, and is backward
 *              compatible with the original halAssertHandler. The assert
 *              causes can be TRUE and FALSE, per usual, or a value from
 *              1..0xFF (see header file for possible assert cause values).
 *              When FALSE, the legacy mode (a global defined as part of the
 *              call to HAL_ASSERT_Init) will determine whether the original
 *              halAssertHandler call is used or the registered callback (if
 *              any) is used.
 *
 * input parameters
 *
 * @param       assertCause: TRUE, FALSE, 1..0xFF.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void halAssertHandlerExt( uint8 assertCause )
{
  // check if there is no cause
  if ( (assertCause == FALSE) &&
       (legacyMode == HAL_ASSERT_LEGACY_MODE_ENABLED) )
  {
    halAssertHandler();

    return;
  }

  // evoke registered callback
  // Note: The pointer halAssertCback will never be NULL due to defaults.
  // Note: The value of assertSubcause, if used, should be set before the call
  //       to HAL_ASSERT. For example:
  // HAL_ASSERT_SET_SUBCAUSE( HAL_ASSERT_SUBCAUSE_FW_INERNAL_ERROR );
  // HAL_ASSERT( HAL_ASSERT_CAUSE_INTERNAL_ERROR );
  halAssertCback( assertCause, assertSubcause );

  // call base HAL Assert handler in case callback handler didn't trap
  halAssertHandler();

  return;
}

/*******************************************************************************
 * @fn          halAssertSpinlock API
 *
 * @brief       This function is to trap software execution.
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
void halAssertSpinlock( void )
{
#ifdef HAL_ASSERT_SPIN
  volatile uint8 i = 1;

  HAL_DISABLE_INTERRUPTS();
  while(i);
  HAL_ENABLE_INTERRUPTS();
#endif // HAL_ASSERT_SPIN

  return;
}

/*******************************************************************************
*/
