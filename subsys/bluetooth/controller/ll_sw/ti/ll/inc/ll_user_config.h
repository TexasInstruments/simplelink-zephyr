/******************************************************************************

 @file  ll_user_config.h

 @brief This file contains user configurable variables for the BLE
        Controller.

        To change the default value of configurable variable:
          - Include "ll_userConfig.h" in your OSAL_ICallBle.c file.
          - Set the variables at the start of stack_main. Actually,
            it is okay to set the variables anywhere in stack_main
            as long as it is BEFORE osal_init_system, but best to
            set at the very start of stack_main.

        Note: User configurable variables are only used during the
              initialization of the Controller. Changing the values
              of these variables after this will have no effect.

        For example:
          int stack_main()
          {
            // user reconfiguration of Controller variables
            llUserConfig.maxNumConns  = 1;
            llUserConfig.numTxEntries = 10;
                       :

        Default values:
          maxNumConns  : 3
          numTxEntries : 6

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2014 $
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

#ifndef LL_USER_CONFIG_H
#define LL_USER_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * INCLUDES
 */
#include "ble_user_config.h"

#ifdef USE_RCL
#include <ti/drivers/rcl/LRF.h>
#endif

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * TYPEDEFS
 */

typedef struct
{
  uint8                 maxNumConns;           // Max number of BLE connections
  uint8                 numTxEntries;          // Max number of BLE connection Tx buffers
  uint8                 maxPduSize;            // Max PDU data size
  uint8                 rfFeModeBias;          // RF Front End Mode and Bias
#ifndef USE_RCL
  regOverride_t         *rfRegPtr;             // RF Common Override Registers
  regOverride_t         *rfReg1MPtr;           // RF 1M Override Register Table
  regOverride_t         *rfReg2MPtr;            // RF 2M Override Register Table
  regOverride_t         *rfRegCodedPtr;         // RF Coded Override Register Table
  txPwrTbl_t            *txPwrTblPtr;           // Tx Power Table
#endif // !(USE_RCL)
#ifndef CC23X0
  rfDrvTblPtr_t         *rfDrvTblPtr;           // Table of Rf Driver API
  eccDrvTblPtr_t        *eccDrvTblPtr;          // Table of ECC Driver API
  cryptoDrvTblPtr_t     *cryptoDrvTblPtr;       // Table of Crypto Driver API
  trngDrvTblPtr_t       *trngDrvTblPtr;         // Table of TRNG Driver API
  rtosApiTblPtr_t       *rtosApiTblPtr;         // Table of RTOS API
#endif
#ifndef USE_RCL
  uint32                startupMarginUsecs;     // Startup Margin in us
  uint32                inactivityTimeout;      // Inactivity timeout in us
  uint32                powerUpDuration;        // Powerup time in us
  RF_Callback           *pErrCb;                // RF Driver Error Callback
#endif
  uint8                 maxAlElems;             // Max elements in the accept list
  uint8                 maxRlElems;             // Max elements in the resolving list
#ifndef CONFIG_SOC_CC2340R5
  ECCParams_CurveParams *eccCurveParams;        // ECC curve parameters
#endif
  pfnFastStateUpdate_t  fastStateUpdateCb;      // Fast state update callback
  uint32                bleStackType;           // BLE Stack Type
  uint32                extStackSettings;       // BLE misc stack settings
  /* The define EM_CC1354P10_1_LP is needed since it is High PA device for
     other stacks (not for BLE) and thus needed to be defined */
#if defined(CC13X2P) || defined(EM_CC1354P10_1_LP)
  txPwrBackoffTbl_t     *txPwrBackoffTblPtr;    // Tx Power Table
  regOverride_t         *rfRegOverrideTx20Ptr;  // High gain overrides
  regOverride_t         *rfRegOverrideTxStdPtr; // Default PA overrides
#endif //CC13X2P
#ifndef CC23X0
  RF_Mode               *rfMode;                // Specify PRCM Mode and pointers to CPE/MCE/RFE patches
  regOverride_t         *rfRegOverrideCtePtr;   // CTE overrides
  cteAntProp_t          *cteAntProp;            // CTE antenna properties
  uint8                 privOverrideOffset;    // Privacy Override Offset
  coexUseCaseConfig_t   *coexUseCaseConfig;     // CoEx priority and RX request configuration
  uint8                 maxNumCteBufs;         // num of CTE samples buffers (each ~2.5KB) used for RF auto copy
#endif
  uint8                 advReportIncChannel;   // include channel index in advertising report
#ifdef USE_RCL
  const LRF_TxPowerTable  *lrfTxPowerTablePtr;
  const LRF_Config        *lrfConfigPtr;
  int8                    defaultTxPowerDbm;      // The default Tx Power value in dBm
  uint8                   defaultTxPowerFraction; // The fraction field allows 0.5 dB steps in the power table
                                                  // 0 - use the integer Tx power dBm value
                                                  // 1 - raise the Tx power value by 0.5 dBm
  uint16                  rclPhyFeature1MBPS;     // RCL_PHY_FEATURE_SUB_PHY_1_MBPS
  uint16                  rclPhyFeature2MBPS;     // RCL_PHY_FEATURE_SUB_PHY_2_MBPS
  uint16                  rclPhyFeatureCoded;     // RCL_PHY_FEATURE_SUB_PHY_CODED
  uint16                  rclPhyFeatureCodedS8;   // RCL_PHY_FEATURE_CODED_TX_RATE_S8
  uint16                  rclPhyFeatureCodedS2;   //RCL_PHY_FEATURE_CODED_TX_RATE_S2
#endif
} llUserCfg_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */


/*******************************************************************************
 * GLOBAL VARIABLES
 */

extern llUserCfg_t llUserConfig;
extern uint16      llUserConfig_maxPduSize;

#ifdef __cplusplus
}
#endif

#endif /* LL_USER_CONFIG_H */
