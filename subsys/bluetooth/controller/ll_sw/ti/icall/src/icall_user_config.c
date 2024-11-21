/******************************************************************************

 @file  icall_user_config.c

 @brief This file contains generic user configurable variables for icall thread.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2016 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*******************************************************************************
 * INCLUDES
 */

#include "hal_types.h"
#include "icall_user_config.h"
#include "icall.h"

#include <ti/drivers/AESCCM.h>
#include <ti/drivers/AESECB.h>

#include <ti/drivers/aesccm/AESCCMLPF3.h>
#include <ti/drivers/aesecb/AESECBLPF3.h>
#include <ti/drivers/ecdh/ECDHLPF3SW.h>
#include <ti/drivers/cryptoutils/sharedresources/CryptoResourceLPF3.h>
#include <ti/drivers/RNG.h>
#include <ti/drivers/cryptoutils/cryptokey/CryptoKeyPlaintext.h>

#ifdef SYSCFG
#include <ti_drivers_config.h>
#endif

#include <ti/drivers/ECDH.h>
#include <ti/drivers/utils/Random.h>

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */
#ifdef EXT_HAL_ASSERT
  #define ASSERT_CBACK                  &halAssertCback
#else // !EXT_HAL_ASSERT
  #define ASSERT_CBACK                  &appAssertCback
#endif // EXT_HAL_ASSERT

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

// this table needs to be field by the application , so it cannot be store in flash.
applicationService_t bleAppServiceInfoTable =
{
  .timerTickPeriod     = 0,               // timerTick_period, This need to be filled at runtime, or the stack will assert an error.
  .timerMaxMillisecond = 0,               // timerMaxMillisecond. This need to be filled at runtime, or the stack will assert an error.
  .assertCback         = ASSERT_CBACK,
};

/*******************************************************************************
 */
