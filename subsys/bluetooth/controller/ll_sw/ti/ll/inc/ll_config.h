/******************************************************************************

 @file  ll_config.h

 @brief This file contains the BLE link layer configuration table macros,
        constants, typedefs and externs.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2009 $
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


#ifndef LL_CONFIG_H
#define LL_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * INCLUDES
 */

#include "ll_user_config.h"

/*******************************************************************************
 * MACROS
 */

#define CRC_SUFFIX_PRESENT()  (llConfigTable.rxPktSuffixPtr->suffixSel & SUFFIX_CRC_FLAG)
#define RSSI_SUFFIX_PRESENT() (llConfigTable.rxPktSuffixPtr->suffixSel & SUFFIX_RSSI_FLAG)

/*******************************************************************************
 * CONSTANTS
 */

// Receive Suffix Data Sizes
#define SUFFIX_SIZE_NONE                0
#define SUFFIX_CRC_SIZE                 3
#define SUFFIX_RSSI_SIZE                1
#define SUFFIX_STATUS_SIZE              1 // RCL status byte is 1
#define SUFFIX_TIMESTAMP_SIZE           4

#define SUFFIX_MAX_SIZE                 (SUFFIX_CRC_SIZE    +                  \
                                         SUFFIX_RSSI_SIZE   +                  \
                                         SUFFIX_STATUS_SIZE +                  \
                                         SUFFIX_TIMESTAMP_SIZE)

#define SUFFIX_FLAG_NONE                0
#define SUFFIX_CRC_FLAG                 1
#define SUFFIX_RSSI_FLAG                2
#define SUFFIX_STATUS_FLAG              4
#define SUFFIX_TIMESTAMP_FLAG           8

// Receive Suffix Flags
#if defined(BLE_VS_FEATURES) && (BLE_VS_FEATURES & SCAN_REQ_RPT_CFG)
#define ADV_SUFFIX_FLAGS                (SUFFIX_RSSI_FLAG+SUFFIX_STATUS_FLAG)       // Connect Req and Scan Req Packets
#else // !SCAN_REQ_RPT_CFG
#define ADV_SUFFIX_FLAGS                (SUFFIX_FLAG_NONE)                          // Connect Req and Scan Req Packets
#endif // SCAN_REQ_RPT_CFG
#define SCAN_SUFFIX_FLAGS               (SUFFIX_RSSI_FLAG+SUFFIX_STATUS_FLAG)       // Adv and Scan Rsp Packets
#define INIT_SUFFIX_FLAGS               (SUFFIX_STATUS_FLAG)                        // Adv Packets
#define LINK_SUFFIX_FLAGS               (SUFFIX_RSSI_FLAG + SUFFIX_TIMESTAMP_FLAG)  // Data and Control Packets

// Suffix Sizes
#if defined(BLE_VS_FEATURES) && (BLE_VS_FEATURES & SCAN_REQ_RPT_CFG)
#define ADV_SUFFIX_SIZE                 (SUFFIX_RSSI_SIZE+SUFFIX_STATUS_SIZE)
#else // !SCAN_REQ_RPT_CFG
#define ADV_SUFFIX_SIZE                 (SUFFIX_SIZE_NONE)
#endif // SCAN_REQ_RPT_CFG
#define SCAN_SUFFIX_SIZE                (SUFFIX_RSSI_SIZE+SUFFIX_STATUS_SIZE)
#define INIT_SUFFIX_SIZE                (SUFFIX_STATUS_SIZE)
#define LINK_SUFFIX_SIZE                (SUFFIX_RSSI_SIZE + SUFFIX_TIMESTAMP_SIZE)

// Number of Link Connections
#ifndef LL_MAX_NUM_BLE_CONNS
#define LL_MAX_NUM_BLE_CONNS            3
#endif

// Number of TX Data Entries
#define MAX_NUM_TX_ENTRIES              8

// Number Data Entries
#define NUM_RX_DATA_ENTRIES             4

#ifndef MAX_NUM_AL_ENTRIES
#ifdef USE_DFL // (Radio core using dynamic filter list)
#define MAX_NUM_AL_ENTRIES              GAP_BONDINGS_MAX  // at 8 bytes per AL entry
#else // !(Radio core using dynamic filter list)
#define MAX_NUM_AL_ENTRIES              16  // at 8 bytes per AL entry
#endif // (Radio core using dynamic filter list)
#endif // (MAX_NUM_AL_ENTRIES undefined)

#ifndef MAX_NUM_RL_ENTRIES
#ifdef USE_DFL // (Radio core using dynamic filter list)
#define MAX_NUM_RL_ENTRIES              GAP_BONDINGS_MAX  // at 60 bytes per RL entry
#else // !(Radio core using dynamic filter list)
#define MAX_NUM_RL_ENTRIES              10  // at 60 bytes per RL entry
#endif // (Radio core using dynamic filter list)
#endif // (MAX_NUM_RL_ENTRIES undefined)

// Number of CTE Sampling Buffers
#ifndef MAX_NUM_CTE_BUFS
#define MAX_NUM_CTE_BUFS                1  // one CTE samples buffer of ~2.5KB used for RF auto copy
#endif

#ifndef MAX_NUM_MST_PKTS
#define MAX_NUM_MST_PKTS                UNLIMITED_PKTS_PER_EVENT
#endif // !MAX_NUM_MST_PKTS

#ifndef MAX_NUM_SLA_PKTS
#define MAX_NUM_SLA_PKTS                UNLIMITED_PKTS_PER_EVENT
#endif // !MAX_NUM_SLA_PKTS

#ifndef CONN_CUTOFF_IN_PERCENT
#define CONN_CUTOFF_IN_PERCENT          95 // in percent of CI wanted
#endif // CONN_CUTOFF_IN_PERCENT

#ifndef EXTENDED_STACK_SETTINGS
#define EXTENDED_STACK_SETTINGS         0
#endif

#ifndef ADV_RPT_INC_CHANNEL
#define ADV_RPT_INC_CHANNEL             0
#endif

// Max Data Size
// Note: When greater than 27, fragmenation is used.
#define MAX_DATA_SIZE                   27

// Number Scan Entries
#define NUM_RX_SCAN_ENTRIES             4

// Packets Per Event
#define UNLIMITED_PKTS_PER_EVENT        0
#define ONE_PKT_PER_EVENT               1

#define LL_STARTUP_MARGIN               2100

// RF Operation Pointer and RF Front End Settings for Reset and Wake
#define RF_OP_PTR_LOCATION              0x210000EC
//
#define RF_SETUP_CONFIG_ON_RESET        0x0000
#define RF_SETUP_CONFIG_ON_WAKE         0x02D0

// Crypto Driver Mode
#define CRYPTO_DRV_MODE_POLLING         0
#define CRYPTO_DRV_MODE_BLOCKING        1

/*******************************************************************************
 * TYPEDEFS
 */

// This structure is used to specify Peripheral RF timing margins. All values are
// in Radio Timer ticks (i.e. 250ns ticks).
//
// preRfMargin: This is the amount of time to subtract from the RF operation's
//              start time (i.e. how much earlier the RF operation will start),
//              and is used to account for the Frequency Synthesizer Calibration
//              time and Rx settle time (i.e. Rx Ramp time) such that the radio
//              is ready to receive.
// postRfMargin This is the amount of time to add to the duration of Rx while
//              waiting for packet synch. This effectively extends the receive
//              window.
PACKED_TYPEDEF_STRUCT
{
  uint8 preRfMargin;
  uint8 postRfMargin;
} slvRfMargin_t;

PACKED_TYPEDEF_STRUCT
{
  uint8 suffixSel;
  uint8 suffixSize;
} pktSuffix_t;

//PACKED typedef uint8 pktSize_t;
typedef uint8 pktSize_t;

//PACKED typedef uint8 alSize_t;
typedef uint8 alSize_t;

//PACKED typedef uint8 rlSize_t;
typedef uint8 rlSize_t;

PACKED_TYPEDEF_STRUCT
{
  uint8 maxMstPktsPerEvt;
  uint8 maxSlvPktsPerEvt;
} maxPktsPerEvt_t;

typedef void (patchCM0_t)(void);

PACKED_TYPEDEF_STRUCT
{
  uint16 resetRfCfgVal;
  uint16 wakeRfCfgVal;
} rfCfgVal_t;

//PACKED typedef uint32 rfOp_t;
typedef uint32 rfOp_t;

PACKED_TYPEDEF_STRUCT
{
  const uint8           *placeHolder0;
  patchCM0_t            *patchCM0Ptr;
  const uint8           *cryptoMode;
  const uint8           *ecdhMode;
  const uint32          *connEvtCutoff;
  const uint16          *advExtPrimLoopOffsetUs;
  const uint16          *advExtSecLoopOffsetUs;
  const llUserCfg_t     *userCfgPtr;
} llCfgTable_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */


/*******************************************************************************
 * GLOBAL VARIABLES
 */

extern uint8 numTxDataBufs;
extern uint8 maxNumTxDataBufs;
extern uint8 maxNumCteDataBufs;
extern uint8 maxNumConns;
extern uint16 maximumPduSize;
extern uint8 rfFeModeBias;
extern uint8 alSize;
extern uint8 rlSize;
extern uint32 extStackSettings;

extern uint16  llUserConfig_maxPduSize;
extern const llCfgTable_t llConfigTable;

#ifdef __cplusplus
}
#endif

#endif /* LL_CONFIG_H */
