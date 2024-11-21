/******************************************************************************

 @file  ll_config.c

 @brief This file contains the BLE link layer configuration table.

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
#include "hal_mcu.h"
#include <ti/drivers/ECDH.h>
#include "ll_config.h"
#include "ll.h"
#include "ll_common.h"
#include "ble.h"
#include "ll_al.h"

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */
// Default Tx Power Index
#define DEFAULT_TX_POWER               0

// Default RFE Bias
#define DEFAULT_RFE_BIAS               0

// Default offset for the primary channel counter
#define ADV_EXT_PRIMARY_LOOP_OFFSET    40

// Default offset for the secondary channel counter (40us)
#define ADV_EXT_SECONDARY_LOOP_OFFSET  40

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

// Accept List Size
// Note: This value represents the amount of memory that will be preallocated
//       from the heap. The actual amount of memory used will be two times the
//       accept list size. This is to accomodate the accept list used during
//       scanning when the duplicate filtering feature is enabled.
//       The accept list size is defined at start up by the application, and
//       alSize is set in LL_init().
alSize_t alSize;

// Resolving List Size
//       The Resolving list size is defined at start up by the application, and
//       rlSize is set in LL_init().
rlSize_t rlSize;

// Extended Accept List Size
//       The Extended Accept list size is defined at start up by the application, and
//       the accept list size determined according to extended accept list.
uint8 extALSize;

// Crypto Driver Mode
const uint8 cryptoMode              = CRYPTO_DRV_MODE_POLLING;

// ECDH key gen Mode
const uint8 ecdhMode                = ECDH_RETURN_BEHAVIOR_BLOCKING;

// Advertising Extension parameter:
// Offset to be added to the start time of the count command used
// in the primary channel.
const uint16 advExtPrimLoopOffsetUs  = ADV_EXT_PRIMARY_LOOP_OFFSET;

// Advertising Extension parameter:
// Offset to be added to the start time of the count command used
// in the secondary channel.
const uint16 advExtSecLoopOffsetUs   = ADV_EXT_SECONDARY_LOOP_OFFSET;


// Connection Event Cutoff, in percent of Connection Interval wanted
// WARNING: This value should never be less than 10% or the stack connection
//          timing could cause issues.
const uint32 connEvtCutoff          = CONN_CUTOFF_IN_PERCENT;

// User Defined Configuration Table - RAM Based!
llUserCfg_t llUserConfig            =
{
  .maxNumConns        = LL_MAX_NUM_BLE_CONNS,    // Max BLE Conns
  .numTxEntries       = MAX_NUM_TX_ENTRIES,      // Max Tx Entries
  .maxPduSize         = 0,                       // Max Data Size - Unused, replaced by llUserConfig_maxPduSize
  .maxAlElems         = MAX_NUM_AL_ENTRIES,      // Max number of elements in the accept list
  .maxRlElems         = MAX_NUM_RL_ENTRIES,      // Max number of elements in the resolving list
  .eccCurveParams     = NULL,                    // ECC curve parameters
  .fastStateUpdateCb  = NULL,                    // Fast state update callback
  .bleStackType       = 0 ,                      // BLE Stack Type
  .extStackSettings   = EXTENDED_STACK_SETTINGS, // Stack misc settings
/* The define EM_CC1354P10_1_LP is needed since it is High PA device for
   other stacks (not for BLE) and thus needed to be defined */
  .advReportIncChannel   = ADV_RPT_INC_CHANNEL,  // include channel index in advertising report
  .lrfTxPowerTablePtr     = NULL,
  .lrfConfigPtr           = NULL,
  .lrfConfigCsPtr         = NULL,
  .defaultTxPowerDbm      = 0,
  .defaultTxPowerFraction = 0,
  .rclPhyFeature1MBPS     = 0,
  .rclPhyFeature2MBPS     = 0,
  .rclPhyFeatureCoded     = 0,
  .rclPhyFeatureCodedS8   = 0,
  .rclPhyFeatureCodedS2   = 0,
};

uint16  llUserConfig_maxPduSize         =  MAX_DATA_SIZE;    // Max Data Size - Used to replace llUserConfig.maxPduSize
////////////////////////////////////////////////////////////////////////////////
// Link Layer Configuration Table
////////////////////////////////////////////////////////////////////////////////
#if defined( CC23X0 )
  const llCfgTable_t llConfigTable  = {
    .placeHolder0 = NULL,
    .patchCM0Ptr  = NULL,
#else // unknown device
    #error "ERROR: Unknown device!"
#endif // <device>
    .cryptoMode             = &cryptoMode,
    .ecdhMode               = &ecdhMode,
    .connEvtCutoff          = &connEvtCutoff,
    .advExtPrimLoopOffsetUs = &advExtPrimLoopOffsetUs,
    .advExtSecLoopOffsetUs  = &advExtSecLoopOffsetUs,
    .userCfgPtr             = &llUserConfig
};


/*******************************************************************************
 */
