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
#define MAX_NUM_AL_ENTRIES              5  // at 8 bytes per AL entry
#endif // (MAX_NUM_AL_ENTRIES undefined)

#ifndef MAX_NUM_RL_ENTRIES
#define MAX_NUM_RL_ENTRIES              10  // at 60 bytes per RL entry
#endif // (MAX_NUM_RL_ENTRIES undefined)

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

#define LL_STARTUP_MARGIN               2100

// Crypto Driver Mode
#define CRYPTO_DRV_MODE_POLLING         0
#define CRYPTO_DRV_MODE_BLOCKING        1

/*******************************************************************************
 * TYPEDEFS
 */

//PACKED typedef uint8 alSize_t;
typedef uint8 alSize_t;

//PACKED typedef uint8 rlSize_t;
typedef uint8 rlSize_t;

typedef void (patchCM0_t)(void);

PACKED_TYPEDEF_STRUCT
{
  uint16 resetRfCfgVal;
  uint16 wakeRfCfgVal;
} rfCfgVal_t;


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
extern uint8 alSize;
extern uint8 rlSize;
extern uint8 extALSize;
extern uint32 extStackSettings;

extern uint16  llUserConfig_maxPduSize;
extern const llCfgTable_t llConfigTable;

#ifdef __cplusplus
}
#endif

#endif /* LL_CONFIG_H */
