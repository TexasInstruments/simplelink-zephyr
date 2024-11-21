/******************************************************************************

 @file  ble_user_config.c

 @brief This file contains user configurable variables for the BLE
        Controller and Host.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2014 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*******************************************************************************
 * INCLUDES
 */

#include "hal_types.h"
#include "ll_common.h"
#include "osal.h"
#include "ll_user_config.h"
#include "ble_user_config.h"

#ifdef CONFIG_ZEPHYR
#include "rcl_settings_ble.h"
#else
#include "ti_radio_config.h"
#endif // CONFIG_ZEPHYR

#ifdef SYSCFG
#include "ti_ble_config.h"
#endif

#if defined( HOST_CONFIG ) && ( HOST_CONFIG & ( CENTRAL_CFG | PERIPHERAL_CFG ) )

#include "ble_dispatch.h"
#include "l2cap.h"

#endif // ( CENTRAL_CFG | PERIPHERAL_CFG )


/*******************************************************************************
 * GLOBAL VARIABLES
 */
uint32_t * icallServiceTblPtr = NULL;

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
 * @fn      setBleUserConfig
 *
 * @brief   Set the user configurable variables for the BLE
 *          Controller and Host.
 *
 *          Note: This function should be called at the start
 *                of stack_main.
 *
 * @param   userCfg - pointer to user configuration
 *
 * @return  none
 */
void setBleUserConfig( icall_userCfg_t *userCfg )
{
  if ( userCfg != NULL )
  {
    stackSpecific_t *stackConfig = (stackSpecific_t*) userCfg->stackConfig;

#if defined( HOST_CONFIG ) && ( HOST_CONFIG & ( CENTRAL_CFG | PERIPHERAL_CFG ) )
    l2capUserCfg_t l2capUserCfg;

    // user reconfiguration of Host variables
    l2capUserCfg.maxNumPSM = stackConfig->maxNumPSM;
    l2capUserCfg.maxNumCoChannels = stackConfig->maxNumCoChannels;

    L2CAP_SetUserConfig( &l2capUserCfg );

#endif // ( CENTRAL_CFG | PERIPHERAL_CFG )
    if ( stackConfig->pfnBMAlloc != NULL )
    {
      *stackConfig->pfnBMAlloc = bleDispatch_BMAlloc;
    }

    if ( stackConfig->pfnBMFree != NULL )
    {
      *stackConfig->pfnBMFree = bleDispatch_BMFree;
    }

    // user reconfiguration of Controller variables
    llUserConfig.maxNumConns   = stackConfig->maxNumConns;
    llUserConfig.numTxEntries  = stackConfig->maxNumPDUs;
    llUserConfig_maxPduSize    = bleUserCfg_maxPduSize;
    llUserConfig.maxAlElems    = stackConfig->maxAcceptListElems;
    llUserConfig.maxRlElems    = stackConfig->maxResolvListElems;
    llUserConfig.advReportIncChannel = stackConfig->advReportIncChannel;
    // Set useDFL to false initially
    llUserConfig.useDFL = stackConfig->useDFL;

    // ECC Driver Parameter
    llUserConfig.eccCurveParams     = stackConfig->eccParams;

    // Fast State Update Callback
    llUserConfig.fastStateUpdateCb = stackConfig->fastStateUpdateCb;

    // BLE Stack Type
    llUserConfig.bleStackType = stackConfig->bleStackType;

    // Extended stack settings
    llUserConfig.extStackSettings = stackConfig->extStackSettings;


    llUserConfig.lrfTxPowerTablePtr = &LRF_txPowerTable;
    llUserConfig.lrfConfigPtr = &LRF_config;
#ifdef CHANNEL_SOUNDING
    llUserConfig.lrfConfigCsPtr = &LRF_configBleCsHp;
#endif
    llUserConfig.defaultTxPowerDbm = defaultTxPowerDbm;
    llUserConfig.defaultTxPowerFraction = 0;
    llUserConfig.rclPhyFeature1MBPS = RCL_PHY_FEATURE_SUB_PHY_1_MBPS;
    llUserConfig.rclPhyFeature2MBPS = RCL_PHY_FEATURE_SUB_PHY_2_MBPS;
    llUserConfig.rclPhyFeatureCoded = RCL_PHY_FEATURE_SUB_PHY_CODED;
    llUserConfig.rclPhyFeatureCodedS8 = RCL_PHY_FEATURE_CODED_TX_RATE_S8;
    llUserConfig.rclPhyFeatureCodedS2 = RCL_PHY_FEATURE_CODED_TX_RATE_S2;

    // save off the application's assert handler
    halAssertInit( **userCfg->appServiceInfo->assertCback, HAL_ASSERT_LEGACY_MODE_ENABLED );

    osal_timer_init( userCfg->appServiceInfo->timerTickPeriod , userCfg->appServiceInfo->timerMaxMillisecond );
  }
  else
  {
      LL_ASSERT( FALSE );
  }

  llUserConfig.useSrcClkLFOSC = SRC_CLK_IS_LFOSC;
  llUserConfig.cfgLFOSCExtraPPM = USER_CFG_LFOSC_EXTRA_PPM;

#ifdef USE_AE
  llUserConfig.useAE = TRUE;
#else
  llUserConfig.useAE = FALSE;
#endif

#ifdef USE_DFL
  llUserConfig.useDFL = TRUE;
#else
  llUserConfig.useDFL = FALSE;
#endif

  return;
}

/*******************************************************************************
 */
