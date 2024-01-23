/******************************************************************************

 @file  ll_timer_drift.c

 @brief This file contains the BLE timer drift software.

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

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)

#include "hal_mcu.h"
#include "osal_pwrmgr.h"
#include "ll.h"
#include "ll_common.h"
#include "ll_timer_drift.h"
#include "ll_ae.h"
//
#include "rom_jt.h"

/*******************************************************************************
 * MACROS
 */

#define SCA_INDEX_MASK   0x07U // to use SCA array the index range is 0..7

/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

// Central Sleep Clock Accurracy, in PPM
// Note: Worst case range value is assumed.
const uint16 SCA[] = {500, 250, 150, 100, 75, 50, 30, 20};

/*******************************************************************************
 * GLOBAL VARIABLES
 */

/*******************************************************************************
 * Functions
 */


/*******************************************************************************
 * @fn          llCalcPeriodicScaDriftPerInterval
 *
 * @brief       This function is used when a periodic sync is formed to calculate
 *              the timer drift per periodic interval based on
 *              the combined SCA of the Scanner and the Advertiser
 *
 * input parameters
 *
 * @param       perAdvSCA - An ordinal value from 0..7 that corresponds to a
 *                          SCA range per Vol. 6, Part B, Section 2.3.3.1,
 *                          Table 2.2.
 * @param       periodicInterval - the interval between two periodic events as defined
 *                          by the periodic advertiser
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The timer drift factor in RATS
 */
uint32 llCalcPeriodicScaDriftPerInterval( uint8 perAdvSCA , uint16 periodicInterval)
{
   // Extract the SCA in PPM with the PerAdvSCA index
   uint16 scaFactorPPM = SCA[ perAdvSCA & SCA_INDEX_MASK ];
   uint32 scaDrift;

   // If Source Clock is LFOSC
   if (llUserConfig.useSrcClkLFOSC != 0)
   {
       // Use User cfgLFOSCExtraPPM instead of default SCA
       scaFactorPPM += llUserConfig.cfgLFOSCExtraPPM;
   }
   else
   {
       scaFactorPPM += LL_SCA_CENTRAL_DEFAULT;
   }

   // Multiple the scaFactor in the periodic interval and divide it
   // so ScaDrift value will be in RATS
   scaDrift = ((scaFactorPPM * periodicInterval) / RAT_TICKS_IN_100US ) + 1;

   return( scaDrift );
}

/*******************************************************************************
 * @fn          llCalcScaFactor
 *
 * @brief       This function is used when a connection is formed to calculate
 *              the timer drift divisor (called a timer drift factor) based on
 *              the combined SCA of the Central (as received in the CONNECT_IND
 *              packet) and the Peripheral (based on either the default value of
 *              40ppm (LFXT)/500ppm (LFOSC) or the value set by HCI_EXT_SetSCA, from 0..500).
 *
 * input parameters
 *
 * @param       centralSCA - An ordinal value from 0..7 that corresponds to a
 *                          SCA range per Vol. 6, Part B, Section 2.3.3.1,
 *                          Table 2.2.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The timer drift factor.
 */
uint16 llCalcScaFactor( uint8 centralSCA )
{
  uint16 sca;
  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );

  // make sure centralSCA is not out of bounds (0 - 7),
  // the largest index is 7 which is 0b111 = 0x07.
  // this can lead to garbage SCA value being used or worse
  centralSCA = (uint8)(centralSCA & SCA_INDEX_MASK);

  // include the Peripheral's SCA in timer drift correction
  sca = pAdvSet->scaValue;

  // convert central's SCA to PPM and combine with peripheral
  sca += SCA[ centralSCA ];

  return( sca );
}

#endif // ADV_CONN_CFG

/*******************************************************************************
 */
