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
#ifndef CONFIG_SOC_CC2340R5
#include <ti/drivers/ECDH.h>
#endif
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

// Tx Power
#define NUM_TX_POWER_VALUES (sizeof( txPowerTable ) / sizeof( txPwrVal_t ))

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

#if defined(CC26X2) || defined(CC13X2) || defined(CC13X4) || defined(CC23X0) && !defined(CC13X2P)

// Default Tx Power Values (Pout, IB, GC, TC) for 7x7 Package
const txPwrVal_t txPowerTable[] =
  { { LL_TX_POWER_0_DBM, GEN_TX_POWER_VAL( 0x21, 1, 0x31 ) } };

#elif defined(CC13X2P)

// Default Tx Power Values (Pout, txPower)
const txPwrVal_t txPowerTable[] =
  { { LL_TX_POWER_0_DBM, RF_TxPowerTable_DefaultPAEntry(25, 1, 0, 26) } };

#endif // CC26X2 || CC13X2 || CC13X4 || CC23X0

// Tx Power Table
txPwrTbl_t txPwrTbl = { txPowerTable,
                        NUM_TX_POWER_VALUES,  // max
                        DEFAULT_TX_POWER };   // default

/*******************************************************************************
 * GLOBAL VARIABLES
 */

// RF Setup Config Value
const rfCfgVal_t rfCfgVal           = { RF_SETUP_CONFIG_ON_RESET,
                                        RF_SETUP_CONFIG_ON_WAKE };
// Rx Packet Suffix Size
// Note: Also included are the flags indicating which suffix fields are
//       included. This may be useful in the Rx ISR if this information is
//       processed some day. Currently, only RSSI is included for Scan, and
//       RSSI and Timestamp are included for link data.
const pktSuffix_t rxPktSuffix       = { LINK_SUFFIX_FLAGS, LINK_SUFFIX_SIZE };
const pktSuffix_t advPktSuffix      = { ADV_SUFFIX_FLAGS,  ADV_SUFFIX_SIZE };
const pktSuffix_t scanPktSuffix     = { SCAN_SUFFIX_FLAGS, SCAN_SUFFIX_SIZE };
const pktSuffix_t initPktSuffix     = { INIT_SUFFIX_FLAGS, INIT_SUFFIX_SIZE };

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

// Max Number of Packets Per Event
const maxPktsPerEvt_t maxPktsPerEvt = { MAX_NUM_MST_PKTS,   // Central
                                        MAX_NUM_SLA_PKTS }; // Peripheral

// RF Operation Pointer
const rfOp_t rfOpLoc                = RF_OP_PTR_LOCATION;

// Crypto Driver Mode
const uint8 cryptoMode              = CRYPTO_DRV_MODE_POLLING;

// ECDH key gen Mode
#ifndef CONFIG_SOC_CC2340R5
const uint8 ecdhMode                = ECDH_RETURN_BEHAVIOR_BLOCKING;
#else
const uint8 ecdhMode                = 2;
#endif
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
  .rfFeModeBias       = DEFAULT_RFE_BIAS,        // RFE Bias
#ifndef CC23X0
  .rfDrvTblPtr        = NULL,                    // RF API Table
  .eccDrvTblPtr       = NULL,                    // ECC API Table
  .cryptoDrvTblPtr    = NULL,                    // Crypto API Table
  .trngDrvTblPtr      = NULL,                    // TRNG API Table
  .rtosApiTblPtr      = NULL,                    // RTOS API Table
#endif
  .maxAlElems         = MAX_NUM_AL_ENTRIES,      // Max number of elements in the accept list
  .maxRlElems         = MAX_NUM_RL_ENTRIES,      // Max number of elements in the resolving list
#ifndef CONFIG_SOC_CC2340R5
  .eccCurveParams     = NULL,                    // ECC curve parameters
#endif
  .fastStateUpdateCb  = NULL,                    // Fast state update callback
  .bleStackType       = 0 ,                      // BLE Stack Type
  .extStackSettings   = EXTENDED_STACK_SETTINGS, // Stack misc settings
/* The define EM_CC1354P10_1_LP is needed since it is High PA device for
   other stacks (not for BLE) and thus needed to be defined */
#if defined(CC13X2P) || defined(EM_CC1354P10_1_LP)
  .txPwrBackoffTblPtr    = NULL,                 // Tx power backoff table
  .rfRegOverrideTxStdPtr = NULL,                 // Default PA overrides
  .rfRegOverrideTx20Ptr  = NULL,                 // High gain overrides
#endif //CC13X2P
#ifndef CC23X0
  .rfMode                = NULL,                 // Specify PRCM Mode and pointers to CPE/MCE/RFE patches
  .rfRegOverrideCtePtr   = NULL,                 // CTE overrides
  .cteAntProp            = NULL,                 // CTE antenna properties
  .privOverrideOffset    = 0,                    // Privacy Override Offset
  .coexUseCaseConfig     = NULL,                 // Coex Configuration
  .maxNumCteBufs         = MAX_NUM_CTE_BUFS,     // Max CTE data Buffers
#endif // CC23X0
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
  .sdaaCfgPtr             = NULL                  // SDAA module user's parameters
};

uint16  llUserConfig_maxPduSize         =  MAX_DATA_SIZE;    // Max Data Size - Used to replace llUserConfig.maxPduSize
////////////////////////////////////////////////////////////////////////////////
// Link Layer Configuration Table
////////////////////////////////////////////////////////////////////////////////
#if defined( USE_FPGA )
  const llCfgTable_t llConfigTable  = {
    .placeHolder0 = NULL,
    .patchCM0Ptr  = NULL,
#elif defined( CC26XX )
  const llCfgTable_t llConfigTable  = {
    .placeHolder0 = NULL,
    .patchCM0Ptr  = NULL,
#elif defined( CC13XX )
  const llCfgTable_t llConfigTable  = {
    .placeHolder0 = NULL,
    .patchCM0Ptr  = NULL,
#elif defined( CC23X0 )
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
