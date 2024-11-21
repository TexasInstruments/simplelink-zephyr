/******************************************************************************

 @file  ble_user_config.c

 @brief This file contains user configurable variables for the BLE
        Application.

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
#include "ble_user_config.h"
#include <ti/drivers/AESCCM.h>
#include <ti/drivers/AESECB.h>
#include <ti/drivers/cryptoutils/cryptokey/CryptoKeyPlaintext.h>
#include <ti/drivers/utils/Random.h>

#include <ti/drivers/aesccm/AESCCMLPF3.h>
#include <ti/drivers/aesecb/AESECBLPF3.h>
#include <ti/drivers/RNG.h>

#ifdef SYSCFG
#include "ti_ble_config.h"
#else
#ifndef CONTROLLER_ONLY
#include <gapbondmgr.h>
#endif // CONTROLLER_ONLY
#endif // SYSCFG

#define Swi_restore SwiP_restore
#define Swi_disable SwiP_disable
#include <ti/drivers/dpl/SwiP.h>

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

#ifndef SYSCFG
// Default Tx Power dBm value
#define DEFAULT_TX_POWER               0
#endif // SYSCFG

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*********************************************************************
 * LOCAL FUNCTIONS
 */

void driverTable_fnSpinlock(void);

/*******************************************************************************
 * GLOBAL VARIABLES
 */


// The Tx Power value
int8 defaultTxPowerDbm = DEFAULT_TX_POWER;

// Antenna board configurations (example for a 12-antenna board)
// Maximum number of antennas
#define ANTENNA_TABLE_SIZE 12
// BitMask of all the relevant GPIOs which needed for the antennas
#define ANTENNA_IO_MASK  BV(27)|BV(28)|BV(29)|BV(30)

// Antenna GPIO configuration (should be adapted to the antenna board design)
antennaIOEntry_t antennaTbl[ANTENNA_TABLE_SIZE] = {
                                   0,                        // antenna 0 GPIO configuration (all GPIOs in ANTENNA_IO_MASK are LOW)
                                   BV(28),                   // antenna 1
                                   BV(29),                   // antenna 2
                                   BV(28) | BV(29),          // antenna 3
                                   BV(30),                   // antenna 4
                                   BV(28) | BV(30),          // antenna 5
                                   BV(27),                   // antenna 6
                                   BV(27) | BV(28),          // antenna 7
                                   BV(27) | BV(29),          // antenna 8
                                   BV(27) | BV(28) | BV(29), // antenna 9
                                   BV(27) | BV(30),          // antenna 10
                                   BV(27) | BV(28) | BV(30)  // antenna 11
};


ECCParams_CurveParams eccParams_NISTP256 = {
    .curveType      = ECCParams_CURVE_TYPE_SHORT_WEIERSTRASS_AN3,
    .length         = ECCParams_NISTP256_LENGTH,
    .prime          = ECC_NISTP256_prime.byte,
    .order          = ECC_NISTP256_order.byte,
    .a              = ECC_NISTP256_a.byte,
    .b              = ECC_NISTP256_b.byte,
    .generatorX     = ECC_NISTP256_generatorX.byte,
    .generatorY     = ECC_NISTP256_generatorY.byte,
    .cofactor       = 1
};

#include <icall.h>

// BLE Stack Configuration Structure
const stackSpecific_t bleStackConfig =
{
  .maxNumConns                          = MAX_NUM_BLE_CONNS,
  .maxNumPDUs                           = MAX_NUM_PDU,
  .maxPduSize                           = 0,
  .maxNumPSM                            = L2CAP_NUM_PSM,
  .maxNumCoChannels                     = L2CAP_NUM_CO_CHANNELS,
  .maxAcceptListElems                   = MAX_NUM_AL_ENTRIES,
  .maxResolvListElems                   = CFG_MAX_NUM_RL_ENTRIES,
  .pfnBMAlloc                           = &pfnBMAlloc,
  .pfnBMFree                            = &pfnBMFree,
  .eccParams                            = &eccParams_NISTP256,
  .fastStateUpdateCb                    = NULL,
  .bleStackType                         = 0,
  .extStackSettings                     = EXTENDED_STACK_SETTINGS,
  .advReportIncChannel                  = ADV_RPT_INC_CHANNEL,
#ifdef USE_DFL
  .useDFL                               = TRUE
#else
  .useDFL                               = FALSE
#endif
};

uint16_t bleUserCfg_maxPduSize = MAX_PDU_SIZE;

/*******************************************************************************
 * @fn          RegisterAssertCback
 *
 * @brief       This routine registers the Application's assert handler.
 *
 * input parameters
 *
 * @param       appAssertHandler - Application's assert handler.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void RegisterAssertCback(assertCback_t appAssertHandler)
{
  appAssertCback = appAssertHandler;

#ifdef EXT_HAL_ASSERT
  // also set the Assert callback pointer used by halAssertHandlerExt
  // Note: Normally, this pointer will be intialized by the stack, but in the
  //       event HAL_ASSERT is used by the Application, we initialize it
  //       directly here.
  halAssertCback = appAssertHandler;
#endif // EXT_HAL_ASSERT

  return;
}

/*******************************************************************************
 * @fn          driverTable_fnSpinLock
 *
 * @brief       This routine is used to trap calls to unpopulated indexes of
 *              driver function pointer tables.
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
void driverTable_fnSpinlock(void)
{
  volatile uint8 i = 1;

  while(i);
}

/*******************************************************************************
 * @fn          DefaultAssertCback
 *
 * @brief       This is the Application default assert callback, in the event
 *              none is registered.
 *
 * input parameters
 *
 * @param       assertCause    - Assert cause as defined in hal_assert.h.
 * @param       assertSubCause - Optional assert subcause (see hal_assert.h).
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void DefaultAssertCback(uint8 assertCause, uint8 assertSubCause)
{
#ifdef HAL_ASSERT_SPIN
  driverTable_fnSpinlock();
#endif // HAL_ASSERT_SPIN

  return;
}

// Application Assert Callback Function Pointer
assertCback_t appAssertCback = DefaultAssertCback;

/*******************************************************************************
 */
