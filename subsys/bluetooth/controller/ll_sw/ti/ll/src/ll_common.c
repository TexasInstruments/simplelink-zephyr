/******************************************************************************

 @file  ll_common.c

 @brief This file contains common functions used by the Link Layer
        Bluetooth Low Energy (BLE) Controller.

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

#include "hal_mcu.h"
#include "map_direct.h"
#include <string.h>
#include "stdint.h"
#include "stdbool.h"
#include <ti/devices/DeviceFamily.h>
#include <ti/drivers/utils/Math.h>
#include DeviceFamily_constructPath(inc/hw_memmap.h)
#include "bcomdef.h"
#include <ti/drivers/rcl/LRF.h>
#include "osal_tasks.h"
#include "osal_bufmgr.h"
#include "osal_cbtimer.h"
#include "ll.h"
#include "ll_common.h"
#include "ll_config.h"
#include "ll_scheduler.h"
#include "ll_enc.h"
#include "hci_event.h"
#include "ble.h"
#include "ll_al.h"
#include "ll_rat.h"
#include "ll_timer_drift.h"
#include "ll_privacy.h"
#include "ll_handover_sn.h"
#ifdef BLE_HEALTH
#include <health_toolkit/inc/debugInfo_internal.h>
#endif // BLE_HEALTH
#include "ll_ae.h"

#include "cs/ll_cs_db.h"

#ifdef __IAR_SYSTEMS_ICC__
// Note: Not required for TI Compiler (CCS)
#include "intrinsics.h"
#endif

// SW Tracer
/*******************************************************************************
 * MACROS
 */

#define LL_GET_ABS_VAL(val1, val2) ((val1)>=(val2))?((val1)-(val2)):((val2)-(val1));

#define SWAP( conn, a, b )                                                     \
  (conn)[(a)] ^= (conn)[(b)];                                                  \
  (conn)[(b)] ^= (conn)[(a)];                                                  \
  (conn)[(a)] ^= (conn)[(b)];                                                  \

// Access Address31-16 XOR Access Address15-0
#define FORM_CHAN_ID(aa) ((uint16)(((aa)>>16) ^ ((aa) & 0xFFFF)))

// (17 * a + b) mod 2^16
#define MAM(a, b) ((uint16)((((uint32)(a) * 17) + (uint32)(b)) & 0xFFFF))

// TO Check Why it doesn't build on some configuraiton for CM4 core
//#ifdef __IAR_SYSTEMS_ICC__
//  #define REVBITS( b ) (__RBIT((uint32)(b)) >> 24)
//#elif defined(__TI_COMPILER_VERSION__)
//  #define REVBITS( b ) (__rbit((uint32)(b)) >> 24)
//#else
  #define REVBITS( b ) MAP_llReverseBits( (b) )
//#endif

// bit reverse the lower 8 bits and the upper 8 bits
#define PERM(a) ((REVBITS(((a) >> 8)) << 8) | REVBITS((a) & 0xFF))
// DATA_ENTRY_LAST_PACKET is uint8 0x10 so we need to shift it 3 times left to mask it into pad0
#define SET_LAST_PKT(a,b) a |= (b << 3)
// return 1 if MSB is 1 else return 0
#define CHECK_LAST_PKT(x)   ((x & 0x80) == 0x80)

/*******************************************************************************
 * CONSTANTS
 */

// Fast TX Limits
#define LL_FAST_TX_TICKS_TO_EVT_PAD   2

// Connection Parameter Request
#define MAX_NUM_OFFSETS               6
#define INVALID_OFFSET                0xFFFF

// LL Topology Pad
// This is the time it takes to realign and sort up to eight connections.
#define LL_TOPO_PAD                   (RAT_TICKS_IN_625US) // one slot

// Basic L2CAP header: Length (2 bytes) + Channel ID (2 bytes)
#define L2CAP_HDR_SIZE                4

#define DMM_POLICY_MAX_ACTIVITIES     4
#define DMM_POLICY_MAX_PRIORITIES     3

#define DMM_POLICY_ACTIVITY_INDEX_INITIATING     0
#define DMM_POLICY_ACTIVITY_INDEX_CONNECTION     1
#define DMM_POLICY_ACTIVITY_INDEX_BROADCASTING   2
#define DMM_POLICY_ACTIVITY_INDEX_OBSERVING      3

#define DMM_POLICY_ACTIVITY_INITIATING     (((DMM_POLICY_ACTIVITY_INDEX_INITIATING   + 1) * 1000) << 16)     //0x3E80000
#define DMM_POLICY_ACTIVITY_CONNECTION     (((DMM_POLICY_ACTIVITY_INDEX_CONNECTION   + 1) * 1000) << 16)     //0x7D00000
#define DMM_POLICY_ACTIVITY_BROADCASTING   (((DMM_POLICY_ACTIVITY_INDEX_BROADCASTING + 1) * 1000) << 16)     //0xBB80000
#define DMM_POLICY_ACTIVITY_OBSERVING      (((DMM_POLICY_ACTIVITY_INDEX_OBSERVING    + 1) * 1000) << 16)     //0xFA00000

#define DMM_POLICY_PRIORITY_NORMAL         0
#define DMM_POLICY_PRIORITY_HIGH           1
#define DMM_POLICY_PRIORITY_URGENT         2

#define DMM_POLICY_TIME_LIMIT_500MS        2000000   // Time in ticks
#define DMM_POLICY_TIME_LIMIT_1S           4000000   // Time in ticks
#define DMM_POLICY_TIME_LIMIT_2S           8000000   // Time in ticks

#define DMM_POLICY_PERCENT_LIMIT_HIGH      50        // 50% of consecutive commands aborted out of total events
#define DMM_POLICY_PERCENT_LIMIT_URGENT    80        // 80% of consecutive commands aborted out of total events
#define DMM_POLICY_REPEAT_PRIO_MAX_COUNT   3         // Number of consecutive commands kept in same priority
#define DMM_POLICY_REPEAT_PRIO_IDX_HIGH    0         // Repeat High priority index
#define DMM_POLICY_REPEAT_PRIO_IDX_URGENT  1         // Repeat Urgent priority index

/*******************************************************************************
 * TYPEDEFS
 */
typedef struct
{
  uint32 time;
  uint32 threshold;
}llHealthCheckActivity_t;

typedef struct
{
  llHealthCheckActivity_t conn;
  llHealthCheckActivity_t scan;
  llHealthCheckActivity_t init;
  llHealthCheckActivity_t adv;
  uint8                   preRelease;
}llHealthCheck_t;

llHealthCheck_t llHealth;

/*******************************************************************************
 * LOCAL FUNCTIONS
 */

// Main functions to process control packets setup
static inline void  llBuildCtrlPkt(llConnState_t *connPtr, uint8_t *pData, uint8_t ctrlPkt);
static inline void  llPostSetupCtrlPkt( llConnState_t *connPtr, uint8_t ctrlPkt);

// Functions to handle specific control packet
static inline void  llSetupUpdateParamReq( llConnState_t *connPtr, uint8_t *pData );                    // C
static inline void  llSetupUpdateChanReq( llConnState_t *connPtr, uint8_t *pData );                     // C
static inline void  llSetupTermInd( llConnState_t *connPtr, uint8_t *pData );
static inline void  llSetupEncReq( llConnState_t *connPtr, uint8_t *pData );                            // C
static inline void  llSetupEncRsp( llConnState_t *connPtr, uint8_t *pData );                            // P
static inline void  llSetupUnknownRsp( llConnState_t *connPtr, uint8_t *pData );
static inline void  llSetupFeatureSetReq( llConnState_t *connPtr, uint8_t *pData );                     // C, P
static inline void  llSetupLenCtrlPkt( llConnState_t *connPtr, uint8_t *pData );                        // C, P
static inline void  llSetupConnParam( llConnState_t *connPtr,uint8_t *pData, uint8_t ctrlPkt);          // C, P
static inline void  llSetupRejectIndExt(llConnState_t *connPtr, uint8_t *pData);
static inline void  llSetupFeatureSetRsp( llConnState_t *connPtr,uint8_t *pData );                      // C, P
static inline void  llSetupVersionIndReq( llConnState_t *connPtr, uint8_t *pData );
static inline void  llSetupRejectInd( llConnState_t *connPtr, uint8_t *pData );
static inline void  llSetupPhyCtrlPkt( llConnState_t *connPtr, uint8_t *pData );                        // C, P

// Service function to setup control packet
static inline uint8_t llEncryptControlPkt(llConnState_t *connPtr, uint8_t ctrlPkt);

// Extension to test mode to setup control packets
#ifdef LL_TEST_MODE
static inline void llSetupConnParamReq_testmode( llConnState_t *connPtr, uint8_t *pData  );
static inline void llSetupConnParamRsp_testmode( llConnState_t *connPtr, uint8_t *pData  );
#endif // LL_TEST_MODE

/*******************************************************************************
 * GLOBAL VARIABLES
 */
// Host Connection Event Notice Callback
llConnEvtNotice_t llConnEvtNotice;

extern void LL_rclRescheduleCommand(RCL_Command *cmd);

// DMM Policy Table
uint32 dmmPolicyTable[DMM_POLICY_MAX_ACTIVITIES][DMM_POLICY_MAX_PRIORITIES] =
{
  {
   DMM_POLICY_ACTIVITY_INITIATING + DMM_POLICY_PRIORITY_NORMAL,
   DMM_POLICY_ACTIVITY_INITIATING + DMM_POLICY_PRIORITY_HIGH,
   DMM_POLICY_ACTIVITY_INITIATING + DMM_POLICY_PRIORITY_URGENT
  },
  {
   DMM_POLICY_ACTIVITY_CONNECTION + DMM_POLICY_PRIORITY_NORMAL,
   DMM_POLICY_ACTIVITY_CONNECTION + DMM_POLICY_PRIORITY_HIGH,
   DMM_POLICY_ACTIVITY_CONNECTION + DMM_POLICY_PRIORITY_URGENT
  },
  {
  DMM_POLICY_ACTIVITY_BROADCASTING + DMM_POLICY_PRIORITY_NORMAL,
  DMM_POLICY_ACTIVITY_BROADCASTING + DMM_POLICY_PRIORITY_HIGH,
  DMM_POLICY_ACTIVITY_BROADCASTING + DMM_POLICY_PRIORITY_URGENT
  },
  {
  DMM_POLICY_ACTIVITY_OBSERVING + DMM_POLICY_PRIORITY_NORMAL,
  DMM_POLICY_ACTIVITY_OBSERVING + DMM_POLICY_PRIORITY_HIGH,
  DMM_POLICY_ACTIVITY_OBSERVING + DMM_POLICY_PRIORITY_URGENT
  }
};

// LL control messages strings, used only when BLE_LOG is defined
char *llCtrl_BleLogStrings[] = {
  "LL_CTRL_CONNECTION_UPDATE_IND",
  "LL_CTRL_CHANNEL_MAP_IND      ",
  "LL_CTRL_TERMINATE_IND        ",
  "LL_CTRL_ENC_REQ              ",
  "LL_CTRL_ENC_RSP              ",
  "LL_CTRL_START_ENC_REQ        ",
  "LL_CTRL_START_ENC_RSP        ",
  "LL_CTRL_UNKNOWN_RSP          ",
  "LL_CTRL_FEATURE_REQ          ",
  "LL_CTRL_FEATURE_RSP          ",
  "LL_CTRL_PAUSE_ENC_REQ        ",
  "LL_CTRL_PAUSE_ENC_RSP        ",
  "LL_CTRL_VERSION_IND          ",
  "LL_CTRL_REJECT_IND           ",
  "LL_CTRL_PERIPHERAL_FEATURE_REQ    ",
  "LL_CTRL_CONNECTION_PARAM_REQ ",
  "LL_CTRL_CONNECTION_PARAM_RSP ",
  "LL_CTRL_REJECT_EXT_IND       ",
  "LL_CTRL_PING_REQ             ",
  "LL_CTRL_PING_RSP             ",
  "LL_CTRL_LENGTH_REQ           ",
  "LL_CTRL_LENGTH_RSP           ",
  "LL_CTRL_PHY_REQ              ",
  "LL_CTRL_PHY_RSP              ",
  "LL_CTRL_PHY_UPDATE_REQ       ",
  "LL_CTRL_MIN_USED_CHANNELS_IND",
  "LL_CTRL_CTE_REQ              ",
  "LL_CTRL_CTE_RSP              ",
};

// Lookup table for convert control packet to packet data len align with control packet opcode
const uint8 ctrlPktLenTable[NUM_OF_CTRL_PKT] =
{
   LL_CONN_UPDATE_IND_PAYLOAD_LEN,         //index 0  - LL_CTRL_CONNECTION_UPDATE_IND
   LL_CHAN_MAP_IND_PAYLOAD_LEN,            //index 1  - LL_CTRL_CHANNEL_MAP_IND
   LL_TERM_IND_PAYLOAD_LEN,                //index 2  - LL_CTRL_TERMINATE_IND
   LL_ENC_REQ_PAYLOAD_LEN,                 //index 3  - LL_CTRL_ENC_REQ
   LL_ENC_RSP_PAYLOAD_LEN,                 //index 4  - LL_CTRL_ENC_RSP
   LL_START_ENC_REQ_PAYLOAD_LEN,           //index 5  - LL_CTRL_START_ENC_REQ
   LL_START_ENC_RSP_PAYLOAD_LEN,           //index 6  - LL_CTRL_START_ENC_RSP
   LL_UNKNOWN_RSP_PAYLOAD_LEN,             //index 7  - LL_CTRL_UNKNOWN_RSP
   LL_FEATURE_REQ_PAYLOAD_LEN,             //index 8  - LL_CTRL_FEATURE_REQ
   LL_FEATURE_RSP_PAYLOAD_LEN,             //index 9  - LL_CTRL_FEATURE_RSP
   LL_PAUSE_ENC_REQ_PAYLOAD_LEN,           //index 10 - LL_CTRL_PAUSE_ENC_REQ
   LL_PAUSE_ENC_RSP_PAYLOAD_LEN,           //index 11 - LL_CTRL_PAUSE_ENC_RSP
   LL_VERSION_IND_PAYLOAD_LEN,             //index 12 - LL_CTRL_VERSION_IND
   LL_REJECT_IND_PAYLOAD_LEN,              //index 13 - LL_CTRL_REJECT_IND
   LL_PERIPHERAL_FEATURE_REQ_PAYLOAD_LEN,  //index 14 - LL_CTRL_PERIPHERAL_FEATURE_REQ
   LL_CONN_PARAM_REQ_PAYLOAD_LEN,          //index 15 - LL_CTRL_CONNECTION_PARAM_REQ
   LL_CONN_PARAM_RSP_PAYLOAD_LEN,          //index 16 - LL_CTRL_CONNECTION_PARAM_RSP
   LL_REJECT_EXT_IND_PAYLOAD_LEN,          //index 17 - LL_CTRL_REJECT_EXT_IND
   LL_PING_REQ_PAYLOAD_LEN,                //index 18 - LL_CTRL_PING_REQ
   LL_PING_RSP_PAYLOAD_LEN,                //index 19 - LL_CTRL_PING_RSP
   LL_LENGTH_REQ_PAYLOAD_LEN,              //index 20 - LL_CTRL_LENGTH_REQ
   LL_LENGTH_RSP_PAYLOAD_LEN,              //index 21 - LL_CTRL_LENGTH_RSP
   LL_PHY_REQ_PAYLOAD_LEN,                 //index 22 - LL_CTRL_PHY_REQ
   LL_PHY_RSP_PAYLOAD_LEN,                 //index 23 - LL_CTRL_PHY_RSP
   LL_PHY_UPDATE_REQ_PAYLOAD_LEN,          //index 24 - LL_CTRL_PHY_UPDATE_REQ
   LL_MIN_USED_CHANNELS_IND_LEN,           //index 25 - LL_CTRL_MIN_USED_CHANNELS_IND
   LL_CTE_REQ_PAYLOAD_LEN,                 //index 26 - LL_CTRL_CTE_REQ
   LL_CTE_RSP_PAYLOAD_LEN,                 //index 27 - LL_CTRL_CTE_RSP
};

// Lookup table for convert control packet to packet data len align with control packet opcode
const uint8 ctrlCsPktLenTable[NUM_OF_CS_CTRL_PKT] =
{
   LL_CS_SEC_RSP_PL_LEN,                   //index 0 - LL_CTRL_CS_SEC_RSP
   LL_CS_CAPABILITIES_REQ_PAYLOAD_LEN,     //index 1 - LL_CTRL_CS_CAPABILITIES_REQ
   LL_CS_CAPABILITIES_RSP_PAYLOAD_LEN,     //index 2 - LL_CTRL_CS_CAPABILITIES_RSP
   LL_CS_CONFIG_REQ_PL_LEN,                //index 3 - LL_CTRL_CS_CONFIG_REQ
   LL_CS_CONFIG_RSP_PL_LEN,                //index 4 - LL_CTRL_CS_CONFIG_RSP
   LL_CS_REQ_PL_LEN,                       //index 5 - LL_CTRL_CS_REQ
   LL_CS_RSP_PL_LEN,                       //index 6 - LL_CTRL_CS_RSP
   LL_CS_IND_PL_LEN,                       //index 7 - LL_CTRL_CS_IND
   LL_CS_TERMINATE_REQ_PL_LEN,             //index 8 - LL_CTRL_CS_TERMINATE_REQ
   LL_CS_FAE_REQ_PL_LEN,                   //index 9 - LL_CTRL_CS_FAE_REQ
   LL_CS_FAE_RSP_PL_LEN,                   //index 10 - LL_CTRL_CS_FAE_RSP
   LL_CS_CHANNEL_MAP_IND_PL_LEN,           //index 11 - LL_CTRL_CS_CHANNEL_MAP_IND
   LL_CS_SEC_REQ_PL_LEN,                   //index 12 - LL_CTRL_CS_SEC_REQ
   LL_CS_TERMINATE_RSP_PL_LEN,             //index 13 - LL_CTRL_CS_TERMINATE_RSP
};

void llPostRealignConn(llConnState_t *connPtr, uint32 timeToNextEvt);

/*******************************************************************************
 * Functions
 */

/*******************************************************************************
 * @fn          llHaltRadio
 *
 * @brief       This call is used to stop or abort the radio. It will wait until
 *              the interrupt occurs, then clear it.
 *
 *              Note: Done in case the radio is running; no effect if not.
 *
 *              Note: Use with caution. Safe if called if there's enough time
 *                    before the next radio start. Not safe if called just
 *                    before the radio ends and generates an Done or Last Done
 *                    interrupt, as that interrupt could then be missed!
 *
 * input parameters
 *
 * @param       cmd - CMD_STOP or CMD_ABORT.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
uint8 llHaltRadio( uint32 cmd )
{
  return ((uint8)(RCL_Command_stop((RCL_Command_Handle)cmd, RCL_StopType_Hard)));
}

/*******************************************************************************
 * @fn          llCheckAcceptListUsage
 *
 * @brief       This routine is used to check if it is okay to use the accept
 *              list for routines LL_ClearAcceptList, LL_AddAcceptListDevice,
 *              and LL_RemoveAcceptListDevice.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS, LL_STATUS_ERROR_COMMAND_DISALLOWED
 */
llStatus_t llCheckAcceptListUsage( void )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );
#endif // ADV_NCONN_CFG | ADV_CONN_CFG
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
  // check if any accept list
  if ( (extScanInfo->scanMode == LL_SCAN_START) &&
       (((extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST) ||
         (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST_EXT))        ||
        (((extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_ANY_ADV_PKTS)   ||
         (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_ANY_ADV_PKTS_EXT)) &&
        (extScanInfo->pEnable->dupFiltering == LL_FILTER_REPORTS_ENABLE))) )
  {
    // yes, so accept list is in use and can't be touched
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  // check if any accept list
  if ( (extInitInfo->scanMode == LL_SCAN_START) &&
       (extInitInfo->pCreateConn->initFilterPolicy == LL_INIT_AL_POLICY_USE_ACCEPT_LIST) )
  {
    // yes, so accept list is in use and can't be touched
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }
#endif // INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  // check if any accept list
  if ((pAdvSet) && (pAdvSet->pAdvParam) &&
      (pAdvSet->advMode == LL_ADV_MODE_ON) &&
      (pAdvSet->pAdvParam->filterPolicy != LL_ADV_AL_POLICY_ANY_REQ) )
  {
    // yes, so accept list is in use and can't be touched
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

  return( LL_STATUS_SUCCESS );;
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * @fn          llSetupConn
 *
 * @brief       This function is used to determine the proper start time for a
 *              new central connection based on this and the start time of the
 *              Initiator. This allows for the precise alignment of a Central
 *              connection relative to other Central connections.
 *
 * input parameters
 *
 * @param       connId - The ID of the connection to be created.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetupConn( uint8 connId )
{
  // get the current time in RAT ticks, and pad some for overhead
  uint32		 curTime = MAP_llGetCurrentTime() + RAT_TICKS_IN_1MS;
  llConnState_t *connPtr = MAP_llDataGetConnPtr( connId );

  // Set dynamicWinOffset to be dynamic( e.g defined by the RCL).
  extInitCmd.dynamicWinOffset = 1;

  BLE_LOG_INT_INT(0, BLE_LOG_MODULE_CTRL, "CTRL: llSetupConn connId=%d, status=%d\n", connId, 0);

  //Guard time between anchor times will be used if user defined CENTRAL_GUARD_TIME_ENABLE
  if (!(extStackSettings & CENTRAL_GUARD_TIME_ENABLE))
  {
    extInitCmd.connectTime = curTime + (connPtr->curParam.connInterval * RAT_TICKS_IN_1_25MS);
  }
  else
  {
    llConnState_t *connPtrRef = NULL;
    for (uint8_t i = 0 ; i < maxNumConns ; i++)
    {
      if (i == connId) continue;
      connPtrRef = MAP_llDataGetConnPtr(i);
      if ((connPtrRef->allocConn == TRUE) &&
          (connPtrRef->activeConn == TRUE) &&
          (connPtrRef->llTask->taskID == LL_TASK_ID_CENTRAL))
      {
        break;
      }
      else
      {
        connPtrRef = NULL;
      }
    }
    if (connPtrRef)
    {
      extInitCmd.connectTime = connPtrRef->llTask->anchorPoint +
                                (connPtr->curParam.connInterval * RAT_TICKS_IN_1_25MS) +
                                (((connId - connPtrRef->connId)  * NUM_SLOTS_PER_CENTRAL) * RAT_TICKS_IN_625US);
    }
    else
    {
      extInitCmd.connectTime = curTime + (connPtr->curParam.connInterval * RAT_TICKS_IN_1_25MS);
    }
  }

  return;
}
#endif // INIT_CFG

/*******************************************************************************
 * @fn          llSetupDataEntry
 *
 * @brief       This function is used to setup control command RCL data entry.
 *
 * input parameters
 *
 * @param       dataEntry - Pointer to the RCL TX data buffer.
 * @param       cmdLen - LE command length.
 * @param       encEnabled - is encryption enable.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetupDataEntry( RCL_Buffer_TxBuffer *dataEntry, uint8 cmdLen, uint8 encEnabled )
{
  if( dataEntry != NULL )
  {
    dataEntry->state = RCL_BufferStatePending;
    dataEntry->numPad = RCL_BUFFER_MAX_PAD_BYTES;
    if (encEnabled == TRUE)
    {
      cmdLen += LL_PKT_MIC_LEN;
    }
    dataEntry->length = cmdLen + dataEntry->numPad + LL_PKT_HDR_LEN + 1;
    dataEntry->pad0   = 0;
    dataEntry->data[0] = 0; //pad
    dataEntry->data[1] = 0; //pad
    // write the header
    dataEntry->data[2] = LL_DATA_PDU_HDR_LLID_CONTROL_PKT;
    dataEntry->data[3] = cmdLen;
  }
}


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llEnqueueCtrlPkt
 *
 * @brief       This function is used to add a control packet for subsequent
 *              processing. How the control packet is handled depends on
 *              whether it is part of a control procedure, and whether there's
 *              a control packet timeout associated with it.
 *
 *              Note: Duplicate entries are filtered.
 *
 *              Note: Can be called via the HCI interface or from the LL.
 *
 * input parameters
 *
 * @param       connPtr  - Pointer to the current connection.
 * @param       ctrlType - Control packet type.
 *
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llEnqueueCtrlPkt( llConnState_t *connPtr,
                       uint8 ctrlType )
{
  halIntState_t intState;

// Sanity Check
  if ( connPtr == NULL )
  {
  LL_ASSERT( connPtr != NULL );
      return;
  }
  if ( connPtr->ctrlPktInfo.ctrlPktCount >= LL_MAX_NUM_CTRL_PROC_PKTS )
  {
    LL_ASSERT( connPtr->ctrlPktInfo.ctrlPktCount < LL_MAX_NUM_CTRL_PROC_PKTS );
    return;
  }

  if (ctrlType <= LL_CTRL_BLE_LOG_STRINGS_MAX)
  {
    BLE_LOG_INT_STR(0, BLE_LOG_MODULE_CTRL, "CTRL: llEnqueueCtrlPkt connId=%d, ctrlType=%s\n", connPtr->connId, llCtrl_BleLogStrings[ctrlType]);
  }
  else
  {
    BLE_LOG_INT_INT(0, BLE_LOG_MODULE_CTRL, "CTRL: llEnqueueCtrlPkt connId=%d, ctrlType=%d\n", connPtr->connId, ctrlType);
  }
  HAL_ENTER_CRITICAL_SECTION(intState);

  // filter duplicates
  for (uint8 i=0; i<connPtr->ctrlPktInfo.ctrlPktCount; i++)
  {
    if ( connPtr->ctrlPktInfo.ctrlPkts[ i ] == ctrlType )
    {
      // already present, so we're done here

      HAL_EXIT_CRITICAL_SECTION(intState);

      return;
    }
  }

  // add to queue
  connPtr->ctrlPktInfo.ctrlPkts[ connPtr->ctrlPktInfo.ctrlPktCount ] = ctrlType;

  // bump the count
  connPtr->ctrlPktInfo.ctrlPktCount++;

  LL_ASSERT( connPtr->ctrlPktInfo.ctrlPktCount < LL_MAX_NUM_CTRL_PROC_PKTS );

  HAL_EXIT_CRITICAL_SECTION(intState);

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llDequeueCtrlPkt
 *
 * @brief       This function is used to remove a control packet that has
 *              completed processing. The control packet at the head of the
 *              queue is "removed" by moving remaining packets up.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection.
 *
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llDequeueCtrlPkt( llConnState_t *connPtr )
{
  halIntState_t intState;
  uint8 status = USUCCESS;

  // Sanity Check
  if ( connPtr == NULL )
  {
    LL_ASSERT( connPtr != NULL );
    status = UFAILURE;
  }
  if (( status == USUCCESS ) && ( connPtr->ctrlPktInfo.ctrlPktCount == 0 ))
  {
    LL_ASSERT( connPtr->ctrlPktInfo.ctrlPktCount > 0 );
    status = UFAILURE;
  }

  if ( status == USUCCESS )
  {
    HAL_ENTER_CRITICAL_SECTION(intState);

    // decrement the number of control packets left
    if ( --connPtr->ctrlPktInfo.ctrlPktCount == 0 )
    {
      connPtr->ctrlPktInfo.ctrlPkts[0] = LL_CTRL_UNDEFINED_PKT;
    }
    else // more control packets queued up
    {
      // shift remaining packets, if any, up one spot
      for (uint8 i=0; i<LL_MAX_NUM_CTRL_PROC_PKTS-1; i++)
      {
        connPtr->ctrlPktInfo.ctrlPkts[i] = connPtr->ctrlPktInfo.ctrlPkts[i+1];
      }

      // stuff an undefined packet at the end of the queue
      connPtr->ctrlPktInfo.ctrlPkts[LL_MAX_NUM_CTRL_PROC_PKTS-1] = LL_CTRL_UNDEFINED_PKT;
    }

    // set the control processing to inactive for next packet
    connPtr->ctrlPktInfo.ctrlPktActive = FALSE;

    HAL_EXIT_CRITICAL_SECTION(intState);
  }

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llReplaceCtrlPkt
 *
 * @brief       This function is used to replace an existing control packet
 *              in the queue with the given next control packet.
 *              If the control packet to replace is LL_CTRL_UNDEFINED_PKT,
 *              the head of the queue will be replaced with the next control
 *              packet for the same control procedure. This is used to ensure
 *              that a new control procedure enqueued doesn't interleave with
 *              a control procedure that is already in progress. If there is
 *              no control packet to replace, the next control packet is just
 *              added to the queue and the counter will be incremented to ensure
 *              this control packet is processed (i.e. will act like an enqueue).
 *
 * input parameters
 *
 * @param       connPtr  - Pointer to the current connection.
 * @param       ctrlTypeToReplaceWith - Control packet type to replace with.
 * @param       ctrlTypeToReplace - Control packet type to replace
 *
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llReplaceCtrlPkt( llConnState_t *connPtr,
                       uint8          ctrlTypeToReplaceWith,
                       uint8          ctrlTypeToReplace )
{
  halIntState_t intState;
  uint8 replaceIdx;

  // Sanity Check
  LL_ASSERT( connPtr != NULL );

  if (connPtr != NULL)
  {
    HAL_ENTER_CRITICAL_SECTION(intState);

    if ( ctrlTypeToReplace == LL_CTRL_UNDEFINED_PKT )
    {
      // don't care which control packet to replace.
      // just pick one at the head of the queue and replace
      replaceIdx = 0;
      connPtr->ctrlPktInfo.ctrlPkts[replaceIdx] = ctrlTypeToReplaceWith;

      // check if there was nothing on the queue
      if ( connPtr->ctrlPktInfo.ctrlPktCount == 0 )
      {
        // there isn't, so bump the counter
        connPtr->ctrlPktInfo.ctrlPktCount++;
      }
    }
    else
    {
      // find the control packet to replace in the queue
      for ( replaceIdx = 0; replaceIdx < LL_MAX_NUM_CTRL_PROC_PKTS; replaceIdx++ )
      {
        if ( connPtr->ctrlPktInfo.ctrlPkts[replaceIdx] == ctrlTypeToReplace)
        {
          // found the control packet to replace.
          connPtr->ctrlPktInfo.ctrlPkts[replaceIdx] = ctrlTypeToReplaceWith;
          break;
        }
      }

      // check if there was nothing to replace in the queue
      if ( replaceIdx == LL_MAX_NUM_CTRL_PROC_PKTS )
      {
        // there isn't, so add
        replaceIdx = connPtr->ctrlPktInfo.ctrlPktCount++;
        connPtr->ctrlPktInfo.ctrlPkts[replaceIdx] = ctrlTypeToReplaceWith;
      }
    }

    if ( replaceIdx == 0 )
    {
      // set the control processing to inactive
      connPtr->ctrlPktInfo.ctrlPktActive = FALSE;
    }

    LL_ASSERT( connPtr->ctrlPktInfo.ctrlPktCount < LL_MAX_NUM_CTRL_PROC_PKTS );

    HAL_EXIT_CRITICAL_SECTION(intState);
  }

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llProcessChanMap
 *
 * @brief       This function is used to convert a channel map in bit map
 *              format of used data channels to a table of consecutively
 *              ordered entries of used data channels. This is used by the
 *              getNextDataChan algorithm, and should be done whenever the
 *              data channel map is updated.
 *
 * input parameters
 *
 * @param       connPtr - A pointer to the current LL connection data.
 * @param       chanMap - A five byte array containing one bit per data channel
 *                        where a 1 means the channel is "used".
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llProcessChanMap( llConnState_t *connPtr,
                       uint8         *chanMap )
{
  uint8 i, j;

  // channels 37..39 are not data channels and these bits should not be set
  chanMap[LL_NUM_BYTES_FOR_CHAN_MAP-1] &= 0x1F;

  // clear the count of the number of used channels
  connPtr->numUsedChans = 0;

  // the used channel map uses 1 bit per data channel, or 5 bytes for 37 chans
  for (i=0; i<LL_NUM_BYTES_FOR_CHAN_MAP; i++)
  {
    // save each valid channel for every channel map bit that's set
    // Note: When i is on the last byte, only 5 bits need to be checked, but
    //       it is easier here to check all 8 with the assumption that the rest
    //       of the reserved bits are zero.
    for (j=0; j<BITS_PER_BYTE; j++)
    {
      // check if the channel is used; only interested in used channels
      if ( (chanMap[i] >> j) & 1 )
      {
        // sequence used channels in ascending order
        connPtr->chanMapTable[ connPtr->numUsedChans ] = (i*8U)+j;

        // count it
        connPtr->numUsedChans++;
      }
    }
  }
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llSetNextDataChan
 *
 * @brief       This function finds and sets the data channel for the next
 *              active connection event. This routine also checks if a data
 *              channel update procedure is pending, and if so, whether it
 *              will take place between the current event (exclusive) and the
 *              next active event (inclusive). If so, the next data is adjusted
 *              for the next active event keeping peripheral latency in effect.
 *
 * input parameters
 *
 * @param       *connPtr - Pointer to the current connection.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetNextDataChan( llConnState_t *connPtr )
{
  /*
  ** Check for a Data Channel Update and Set the Next Data Channel
  **
  ** Note: The Data Channel Update must come after the Parameters Update in
  **       case the latter updates the next event count.
  */

  // before finding the next channel, save it as the current channel
  // Note: nextChan is now called unmappedChannel in the spec.
  // Note: currentChan is now called lastUnmappedChannel in the spec.
  connPtr->currentChan = connPtr->nextChan;

  // check if there's a data channel udpate for the connection, and if so,
  // check if the update event is the event after this one
  if ( (connPtr->pendingChanUpdate == TRUE) &&
       (connPtr->chanMapUpdateEvent <= (connPtr->currentEvent+1)) )
  {
    // update data channel table based on pending data channel update
    MAP_llProcessChanMap( connPtr, connPtr->curChanMap.chanMap );

    // get the next data channel
    connPtr->currentMappedChan = connPtr->pChSelAlgo( connPtr );

    // clear the pending flag
    connPtr->pendingChanUpdate = CHANNEL_MAP_UPDATE_APPLIED;
  }
  else
  {
    // get the next data channel
    connPtr->currentMappedChan = connPtr->pChSelAlgo( connPtr );
  }

  // set the channel number
  // Note: The channel field is prior to ble5OpCmd_t field changes, so okay.
  ((RCL_CmdBle5Connection *)connPtr->llTask->command)->channel = connPtr->currentMappedChan;

  return;
}


/*******************************************************************************
 * @fn          llGetNextDataChanAlgo1
 *
 * @brief       This function returns the next data channel for a LL connection
 *              based on the previous data channel, the hop length, and the
 *              number of connection intervals to the next active event. If the
 *              derived channel is "used", then it is returned. If the derived
 *              channel is "unused", then a remapped data channel is returned.
 *
 *              Note: nextChan is updated, and must remain that way, even if the
 *                    remapped channel is returned.
 *
 * input parameters
 *
 * @param       connPtr - A pointer to the current LL connection data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The next data channel to use for Algorithm 1.
 */
uint8 llGetNextDataChanAlgo1( llConnState_t *connPtr )
{
  uint8 i;
  uint16 numEvents;

  // find the number of events to the next event
  numEvents = MAP_llEventDelta( connPtr->nextEvent, connPtr->currentEvent );

  // check the starting case  where nextEvent and currentEvent are both zero
  numEvents = (numEvents == 0) ? 1 : numEvents;

  // calculate the data channel based on hop and number of events to next event
  // Note: nextChan is now called UnmappedChannel in the spec.
  // Note: currentChan is now called lastUnmappedChannel in the spec.
  connPtr->nextChan = (connPtr->currentChan + (connPtr->hopLength * numEvents)) %
                      LL_MAX_NUM_DATA_CHAN;
  //BLE_LOG_INT_INT(0, BLE_LOG_MODULE_APP, "N-e=%d, n=%d\n", numEvents, connPtr->nextChan);

  // check if next channel is a used data channel
  for (i=0; i<connPtr->numUsedChans; i++)
  {
    // check if next channel is in the channel map table
    if ( connPtr->nextChan == connPtr->chanMapTable[i] )
    {
      // it is, so return the used channel
      return( connPtr->nextChan );
    }
  }

  //BLE_LOG_INT_INT(0, BLE_LOG_MODULE_APP, "M-r=%d, n=%d\n", connPtr->chanMapTable[connPtr->nextChan % connPtr->numUsedChans], connPtr->nextChan);
  // next channel is unused, so return the remapped channel
  return( connPtr->chanMapTable[connPtr->nextChan % connPtr->numUsedChans] );
}



/*******************************************************************************
 * @fn          llReverseBits
 *
 * @brief       This function is used to reverse eight bits.
 *
 * input parameters
 *
 * @param       dataByte - A byte of data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The input data byte with its bits in reverse order.
 */
uint8 llReverseBits( uint8 dataByte )
{
   dataByte = (dataByte & 0xF0) >> 4 | (dataByte & 0x0F) << 4;
   dataByte = (dataByte & 0xCC) >> 2 | (dataByte & 0x33) << 2;
   dataByte = (dataByte & 0xAA) >> 1 | (dataByte & 0x55) << 1;

   return( dataByte );
}


/*******************************************************************************
 * @fn          llGenPrnE
 *
 * @brief       This function is used to generate a pseudo-random number based
 *              on the channel Id and the connection event.
 *
 * input parameters
 *
 * @param       chanId    - Channel Id formed from connection access address.
 * @param       connEvent - Connection event count.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      prn_e, per BLE5 Vol 6 Part B Section 4.5.8.
 */
uint16 llGenPrnE( uint16 chanId, uint16 connEvent )
{
  uint8 i;
  uint16 prn_e = chanId ^ connEvent;

  for (i=0; i<3; i++)
  {
    prn_e = MAM( PERM(prn_e), chanId );
  }

  return(prn_e ^ chanId);
}


/*******************************************************************************
 * @fn          llGetNextDataChanAlgo2
 *
 * @brief       This function returns the next data channel for a LL connection
 *              based on the connection event.
 *
 * input parameters
 *
 * @param       connPtr - A pointer to the current LL connection data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The next data channel to use for Algorithm 2.
 */
uint8 llGetNextDataChanAlgo2( llConnState_t *connPtr )
{
  uint8 i;
  uint16 prn_e;

  // find prn_e and unmappedChan
  prn_e             = llGenPrnE( FORM_CHAN_ID( connPtr->accessAddr ), connPtr->nextEvent );
  connPtr->nextChan = prn_e % LL_MAX_NUM_DATA_CHAN;

  // check if the unmappedChan is a used channel
  for (i=0; i<connPtr->numUsedChans; i++)
  {
    if ( connPtr->nextChan == connPtr->chanMapTable[i] )
    {
      return( connPtr->nextChan );
    }
  }

  connPtr->nextChan = connPtr->chanMapTable[(connPtr->numUsedChans * prn_e) >> 16];

  return( connPtr->nextChan );
}


/*******************************************************************************
 * @fn          llGetNextDataChan
 *
 * @brief       This function returns the next data channel for a LL connection
 *              based on the previous data channel, the hop length, and the
 *              number of connection intervals to the next active event. If the
 *              derived channel is "used", then it is returned. If the derived
 *              channel is "unused", then a remapped data channel is returned.
 *
 *              Note: nextChan is updated, and must remain that way, even if the
 *                    remapped channel is returned.
 *
 * input parameters
 *
 * @param       connPtr   - A pointer to the current LL connection data.
 * @param       numEvents - The number of skipped events to base the next
 *                          data channel calculation on.
 * output parameters
 *
 * @param       None.
 *
 * @return      The next data channel to use.
 */
uint8 llGetNextDataChan( llConnState_t *connPtr,
                                   uint16         numEvents )
{
  uint8 i;

  // fatal error - there needs to be at least one event
  LL_ASSERT( numEvents != 0 );

  // calculate the data channel based on hop and number of events to next event
  // Note: nextChan is now called UnmappedChannel in the spec.
  // Note: currentChan is now called lastUnmappedChannel in the spec.
  connPtr->nextChan = (connPtr->currentChan + (connPtr->hopLength * numEvents)) %
                      LL_MAX_NUM_DATA_CHAN;

  // check if next channel is a used data channel
  for (i=0; i<connPtr->numUsedChans; i++)
  {
    // check if next channel is in the channel map table
    if ( connPtr->nextChan == connPtr->chanMapTable[i] )
    {
      // it is, so return the used channel
      return( connPtr->nextChan );
    }
  }

  // next channel is unused, so return the remapped channel
  return( connPtr->chanMapTable[connPtr->nextChan % connPtr->numUsedChans] );
}

#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llAtLeastTwoChans
 *
 * @brief       This function determines if at least two data channels are
 *              being used, as required by a change in requirements per BT
 *              Core V4.0.0, Volume 6. The channel map is provided as an array
 *              of five bytes, which bits 0 to 36 represent the corresponding
 *              data channel. When set, the data channel is used. This routine
 *              first checks if more than one byte is not zero. If only one
 *              byte is zero, it checks if more than one bit in that byte is
 *              non-zero.
 *
 * input parameters
 *
 * @param       chanMap  - A five byte array containing one bit per data channel
 *                         where a 1 means the channel is "used".
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Indicates whether the channel map is valid:
 *              TRUE:  Two or more data channels are being used.
 *              FALSE: Less than two data channels are being used.
 */
uint8 llAtLeastTwoChans( uint8 *chanMap )
{
  uint8 x            = 0; // for lint
  uint8 nonZeroBytes = 0;

  // channels 37..39 are not data channels and these bits should not be set
  chanMap[LL_NUM_BYTES_FOR_CHAN_MAP-1] &= 0x1F;

  // check each byte to see if more than one has a bit set
  for (uint8 i=0; i<LL_NUM_BYTES_FOR_CHAN_MAP; i++)
  {
    // check/count if a byte is non-zero; we can leave once greater than one
    if ( (nonZeroBytes+=((chanMap[i]!=0)?1:0)) > 1 )
    {
      return( TRUE );
    }

    // save if any channels are set in case only one byte is non-zero
    if ( chanMap[i] )
    {
      x = chanMap[i];
    }
  }

  // check if no channels are used
  // Note: Value is either zero or one as we would have exited if greater
  //       than one.
  if ( nonZeroBytes != 0 )
  {
    // exactly one byte is non-zero; check if not a power of two
    return( (x & (x-1)) != 0);
  }

  // no channel bits are set
  return( FALSE );
}
#endif // INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))

/*******************************************************************************
 * @fn          llEstimateConnMinTimeLength
 *
 * @brief       This function is called to estimate the connection's minimum time length
 *              according to other connection's parameters and sets it.
 *
 * @design      /ref did_361975877
 *
 * input parameters
 *
 * @param       connId - The connection's id.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS.
 *              LL_INACTIVE_CONNECTIONS - In case this connection is inactive.
 */
uint8 llEstimateConnMinTimeLength( uint16 connId )
{
  uint16 calcPktTimeLength = 0;
  llConnState_t    *connPtr = MAP_llDataGetConnPtr( connId );

  // Update Phy value to be suitable to the function MAP_llOctets2Time.
  // Uses curPhy - BLE5_1M_PHY, BLE5_2M_PHY, BLE5_CODED_PHY
  uint8 currentPhy;
  llConvertLlPhyToBlePhy(connPtr->phyInfo.curPhy, &currentPhy);

  // Update the Phy Scheme value to be suitable to the function MAP_llOctets2Time.
  // Uses coding - BLE5_CODED_S8_DEFAULT, BLE5_CODED_S2_DEFAULT
  uint8 currentPhyScheme = 0;
  if (connPtr->phyInfo.curPhy == LL_PHY_CODED)
  {
    llConvertLlPhyOptToBlePhyOpt(connPtr->phyInfo.phyOpts, &currentPhyScheme);
  }

  /********************************************/
  /************ Check Valid Input *************/
  /********************************************/
  // Check if this connection is valid and active?
  if (!(connPtr->activeConn))
  {
    return LL_INACTIVE_CONNECTIONS;
  }

  // In case the minimum time length was externally updated - abort.
  if (connPtr->connMinTimeExternalUpdateInd)
  {
    return LL_STATUS_SUCCESS;
  }

  /******************************************************/
  /************ Actual Maximum Tx / Rx Time *************/
  /******************************************************/
  // Calculate the packet time length according to:
  // - Phy Type.
  // - Packet Length = 0.
  // - Encrypted / Not Encrypted.
  calcPktTimeLength = MAP_llOctets2Time( currentPhy,
                                         currentPhyScheme,        // Scheme.
                                         0,                       // Use 0 bytes.
                                         connPtr->encEnabled);    // Encrypted or not?

  // Add margin time LL_TOTAL_MARGIN_TIME_FOR_MIN_CONN_RAT_TICKS that was emprically tested.
  // (only 31 bits for connMinTimeLength).
  connPtr->connMinTimeLength = (LL_TOTAL_MARGIN_TIME_FOR_MIN_CONN_RAT_TICKS + RAT_TICKS_IN_150US + US_TO_RAT_TICKS( calcPktTimeLength * 2 /* Rx and Tx */)) & LL_MIN_MAX_CONN_TIME_LENGTH_MASK;

  // return Success
  return LL_STATUS_SUCCESS;
}

/*******************************************************************************
 * @fn          llAllocConnId
 *
 * @brief       This function is called to get the next free LL connection. If
 *              all available connections are in use, NULL is returned.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      A valid connection pointer, or NULL.
 */
llConnState_t *llAllocConnId( void )
{
  uint8 i;

  // check if there's a resource available
  if ( llConns.numLLConns == maxNumConns )
  {
    // unable to add another connection
    return( NULL );
  }

  // find first free connection
  for (i=0; i<maxNumConns; i++)
  {
    // check for inactive connection
    if ( llConns.llConnection[i].allocConn == FALSE )
    {
      llConnState_t *connPtr = MAP_llDataGetConnPtr( i );

      // found a free connection, so initialize it
      connPtr->allocConn                             = TRUE;
      connPtr->activeConn                            = FALSE;
      connPtr->connId                                = i;
      connPtr->accessAddr                            = 0;
      connPtr->txDataEnabled                         = TRUE;
      connPtr->rxDataEnabled                         = TRUE;
      connPtr->pTxDataEntryQ                         = NULL;
      connPtr->pRxDataEntryQ                         = NULL;
      connPtr->currentEvent                          = 0;
      connPtr->nextEvent                             = 0;
      connPtr->firstPacket                           = TRUE;
      connPtr->currentChan                           = 0;
      connPtr->lastTimeToNextEvt                     = 0;
      connPtr->peripheralLatencyAllowed              = FALSE;
      connPtr->peripheralLatency                     = 0;
      connPtr->pendingChanUpdate                     = FALSE;
      connPtr->pendingParamUpdate                    = PARAM_UPDATE_NOT_PENDING;
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
      connPtr->updateSLPending                       = UPDATE_PL_OKAY;
#endif // ADV_CONN_CFG
      connPtr->lastRssi                              = LL_RF_RSSI_UNDEFINED;
      connPtr->ctrlPktInfo.ctrlPktActive             = FALSE;
      connPtr->ctrlPktInfo.ctrlPktCount              = 0;
      connPtr->ctrlPktInfo.ctrlPktPending            = LL_CTRL_UNDEFINED_PKT;
      connPtr->verExchange.peerInfoValid             = FALSE;
      connPtr->verExchange.hostRequest               = FALSE;
      connPtr->verExchange.verInfoSent               = FALSE;
      connPtr->verInfo.verNum                        = 0;
      connPtr->verInfo.comId                         = 0;
      connPtr->verInfo.subverNum                     = 0;
      connPtr->termInfo.termIndRcvd                  = FALSE;
      connPtr->encEnabled                            = FALSE;
      connPtr->encInfo.SKValid                       = FALSE;
      connPtr->encInfo.LTKValid                      = FALSE;
      connPtr->encInfo.txPktCount                    = 0;
      connPtr->encInfo.rxPktCount                    = 0;
      connPtr->encInfo.startEncRspRcved              = FALSE;
      connPtr->encInfo.encReqRcved                   = FALSE;
      connPtr->encInfo.encRestart                    = FALSE;
      connPtr->encInfo.encInProgress                 = FALSE;
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
      connPtr->encInfo.startEncReqRcved              = FALSE;
      connPtr->encInfo.rejectIndRcved                = FALSE;
#endif // INIT_CFG
      connPtr->perInfo.numPkts                       = 0;
      connPtr->perInfo.numCrcErr                     = 0;
      connPtr->perInfo.numEvents                     = 0;
      connPtr->perInfo.numMissedEvts                 = 0;
      connPtr->perInfoByChan                         = NULL;
      connPtr->taskID                                = TASK_NO_TASK;
      connPtr->taskEvent                             = LL_EVT_NONE;
      connPtr->aptoValue                             = LL_APTO_DEFAULT_VALUE;
      connPtr->pingReqPending                        = FALSE;
      connPtr->numAptoExp                            = 0;
      connPtr->aptoTimerId                           = INVALID_TIMER_ID;
      connPtr->phyInfo.curPhy                        = LL_PHY_BASE_PHY;
      connPtr->phyInfo.updatePhy                     = LL_PHY_FASTEST_PHY;
      connPtr->phyInfo.phyOpts                       = LL_PHY_OPT_NONE;
      connPtr->phyInfo.phyFlags                      = CLEAR_ALL_FLAGS;
      connPtr->phyInfo.phyPreference                 = defaultPhy;
      connPtr->pendingPhyUpdate                      = FALSE;
      // init peer address and address type
      connPtr->peerInfo.peerAddrType = LL_DEV_ADDR_TYPE_PUBLIC;
      connPtr->peerInfo.peerAddr[0]                  = 0x00;
      connPtr->peerInfo.peerAddr[1]                  = 0x00;
      connPtr->peerInfo.peerAddr[2]                  = 0x00;
      connPtr->peerInfo.peerAddr[3]                  = 0x00;
      connPtr->peerInfo.peerAddr[4]                  = 0x00;
      connPtr->peerInfo.peerAddr[5]                  = 0x00;
      // init own address type
      connPtr->ownAddrType = LL_DEV_ADDR_TYPE_PUBLIC;
      // init connection data length info
      connPtr->pendingLenUpdate                      = FALSE;
      connPtr->lenInfo.lenFlags                      = CLEAR_ALL_FLAGS;
      // set local default values
      connPtr->lenInfo.connMaxTxTime                 = connInitialMaxTxTime;
      connPtr->lenInfo.connMaxRxTime                 = supportedMaxRxTime;
      connPtr->lenInfo.connMaxTxOctets               = connInitialMaxTxOctets;
      connPtr->lenInfo.connMaxRxOctets               = supportedMaxRxOctets;
      // set peer default values (defaults to uncoded)
      connPtr->lenInfo.connRemoteMaxTxTime           = LL_MIN_LINK_DATA_TIME;
      connPtr->lenInfo.connRemoteMaxRxTime           = LL_MIN_LINK_DATA_TIME;
      connPtr->lenInfo.connRemoteMaxTxOctets         = LL_MIN_LINK_DATA_LEN;
      connPtr->lenInfo.connRemoteMaxRxOctets         = LL_MIN_LINK_DATA_LEN;
      // set effective default values (defaults to uncoded)
      connPtr->lenInfo.connEffectiveMaxTxTime        = LL_MIN_LINK_DATA_TIME;
      connPtr->lenInfo.connEffectiveMaxRxTime        = LL_MIN_LINK_DATA_TIME;
      connPtr->lenInfo.connEffectiveMaxTxOctets      = LL_MIN_LINK_DATA_LEN;
      connPtr->lenInfo.connEffectiveMaxRxOctets      = LL_MIN_LINK_DATA_LEN;
      // BLE V5.0
      connPtr->lenInfo.connIntervalPortionAvail      = LL_MIN_LINK_DATA_TIME;
      connPtr->lenInfo.connEffectiveMaxTxTimeAvail   = LL_MIN_LINK_DATA_TIME;
      connPtr->lenInfo.connEffectiveMaxTxTimeCoded   = LL_MIN_LINK_DATA_TIME;
      connPtr->lenInfo.connEffectiveMaxTxTimeUncoded = LL_MIN_LINK_DATA_TIME;
      connPtr->lenInfo.connEffectiveMaxRxTimeCoded   = LL_MIN_LINK_DATA_TIME;
      connPtr->lenInfo.connEffectiveMaxRxTimeUncoded = LL_MIN_LINK_DATA_TIME;
      // internal values
      connPtr->lenInfo.connActualMaxTxOctets         = LL_MIN_LINK_DATA_LEN;
      connPtr->lenInfo.connSlowestPhy                = LL_PHY_SLOWEST_PHY;

      // set packet queue to undefined values
      // Note: This is used for debugging purposes.
      for (i=0; i<LL_MAX_NUM_CTRL_PROC_PKTS; i++)
      {
        connPtr->ctrlPktInfo.ctrlPkts[i] = LL_CTRL_UNDEFINED_PKT;
      }

      // initialize the channel map
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
      connPtr->curChanMap.chanMap[0] = defaultChannelMap[0];
      connPtr->curChanMap.chanMap[1] = defaultChannelMap[1];
      connPtr->curChanMap.chanMap[2] = defaultChannelMap[2];
      connPtr->curChanMap.chanMap[3] = defaultChannelMap[3];
      connPtr->curChanMap.chanMap[4] = defaultChannelMap[4];
#else
      connPtr->curChanMap.chanMap[0] = 0xFF;
      connPtr->curChanMap.chanMap[1] = 0xFF;
      connPtr->curChanMap.chanMap[2] = 0xFF;
      connPtr->curChanMap.chanMap[3] = 0xFF;
      connPtr->curChanMap.chanMap[4] = 0x1F;
#endif

      // set the connection Feature Set based on this device's default
      for (i=0; i<LL_MAX_FEATURE_SET_SIZE; i++)
      {
        connPtr->featureSetInfo.featureSet[i] = deviceFeatureSet.featureSet[i];
      }

      // Clear the extended indication feature.
      // According to 5.3, section 2.4.2.18, this PDU shall be issued ONLY when the
      // remote Link Layer supports the Extended Reject Indication Link Layer feature.
      // Otherwise, the LL_REJECT_IND PDU shall be issued instead.
      // The feature will be properly updated when remote feature exchange procedure will be executed.
      // Until then, we will behave as if it is not enabled.
      connPtr->featureSetInfo.featureSet[0] &= ~(uint8)LL_FEATURE_REJECT_EXT_IND;

      // default to Channel Selection Algorithm #1
      // Note: When the connection forms, the data channel algorithm to use
      //       will depend on both our device and the peer device's selection.
      //       To use algorithm #2, both devices must support this feature.
      connPtr->pChSelAlgo = MAP_llGetNextDataChanAlgo1;

      // and clear the Feature Set Exchange response flag
      connPtr->featureSetInfo.featureRspRcved = LL_FEATURE_RSP_INIT;

      // Define default Conn Min Time Length (in RAT TICKS).
      llEstimateConnMinTimeLength( connPtr->connId );
      // Define default Conn Priority
      connPtr->connPriority = qosDefaultPriorityConnParameter;
      // Define default Miss Count Value
      connPtr->connMissCount = 0;
      // Define the user min time update indication
      connPtr->connMinTimeExternalUpdateInd = FALSE;
      // Define the user max time update indication
      connPtr->connMaxTimeExternalUpdateInd = FALSE;
      // Define default Conn Max Time Length (in RAT TICKS).
      connPtr->connMaxTimeLength =  US_TO_RAT_TICKS(LL_MAX_LINK_DATA_TIME);
      // Define default number of retries in case of LSTO state.
      connPtr->numLSTORetries = 0;
      // Define default for the receive data buffers
      connPtr->rxData.pduSize = 0;
      connPtr->rxData.pduCid = 0;
      connPtr->rxData.pEntry = NULL;
      connPtr->rxData.size = 0;
      connPtr->rxData.state = LL_DATA_FIRST_PKT_CTRL_TO_HOST;
      connPtr->extFeatureMask = 0;

      // count the number of active connections
      if ( ++llConns.numLLConns == 1 )
      {
        // this is the only connection, so make it the current connection
        llConns.currentConn = connPtr->connId;
      }

      // clear RX Statistics
      connPtr->rxStats.numRxOk         = 0;
      connPtr->rxStats.numRxCtrl       = 0;
      connPtr->rxStats.numRxCtrlAck    = 0;
      connPtr->rxStats.numRxCrcErr     = 0;
      connPtr->rxStats.numRxIgnored    = 0;
      connPtr->rxStats.numRxEmpty      = 0;
      connPtr->rxStats.numRxBufFull    = 0;

      // clear TX Statistics
      connPtr->txStats.numTx           = 0;
      connPtr->txStats.numTxAck        = 0;
      connPtr->txStats.numTxCtrl       = 0;
      connPtr->txStats.numTxCtrlAck    = 0;
      connPtr->txStats.numTxCtrlAckAck = 0;
      connPtr->txStats.numTxRetrans    = 0;
      connPtr->txStats.numTxEntryDone  = 0;

      // Reset Handover flags
      connPtr->estWithHandover         = 0;
      connPtr->handoverInProg          = 0;

      return( connPtr );
    }
  }

  // Sanity Check
  // There should have been at last one resource available!
  LL_ASSERT( FALSE );

  return( NULL );
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llReleaseConnId
 *
 * @brief       This function is called to mark a connection resource as free.
 *
 *              Note: If there are other active connections, the scheduler will
 *                    find the next one to schedule, and will set currentConn.
 *
 * input parameters
 *
 * @param       connPtr - A pointer to the connection resource to free.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llReleaseConnId( llConnState_t *connPtr )
{
  // clear all CS data set
  MAP_llCsClearConnProcedures(connPtr->connId);

  // check that this connection is active
  // Note: If a cancel is issued, this connection may be released even though
  //       it isn't yet active.
  if ( connPtr->activeConn )
  {
    // mark connection as inactive
    connPtr->activeConn = FALSE;

    LL_ASSERT( llConns.numActiveConns != 0 );

    // decrement the number of active connections
    llConns.numActiveConns--;
  }

  // count the number of active connections
  // Note: Only side effect the current connection if there are no more
  --llConns.numLLConns;
  if ( ( llConns.llConnection[connPtr->connId].connId == llConns.currentConn ) ||
       ( llConns.numLLConns == 0 ))
  {
    // mark the current connection as invalid until a new one is assigned
    llConns.currentConn = LL_INVALID_CONNECTION_ID;
  }

  // mark the connection as deallocated
  connPtr->allocConn = FALSE;

  memset (&llConns.llConnection[connPtr->connId], 0, sizeof( llConnState_t ));

  llConns.llConnection[connPtr->connId].connId = LL_INVALID_CONNECTION_ID;

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llReleaseAllConnId
 *
 * @brief       This function is called to free all connection resources.
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
void llReleaseAllConnId( void )
{
  uint8 i;

  // mark all connections as inactive
  for (i=0; i<maxNumConns; i++)
  {
    // de-activate all connections
    llConns.llConnection[i].activeConn = FALSE;

    // deallocated all connection structures
    llConns.llConnection[i].allocConn = FALSE;
  }

  // zero the number of allocated connections
  llConns.numLLConns = 0;

  // zero the number of active connections
  llConns.numActiveConns = 0;

  // mark the current connection
  llConns.currentConn = LL_INVALID_CONNECTION_ID;

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*********************************************************************
* @fn     llCalcConnMissCount
*
* @brief  This is the function that calculates the connection's miss count.
*
* @design      /ref did_361975877
*
* input parameters
*
* @param connId            - The connection id.
* @param connlastStartTime - last start time of the connection.
*
* output parameters
*
* @param       None
*
* @return      LL_STATUS_SUCCESS.
*              LL_INACTIVE_CONNECTIONS - In case this connection is inactive.
*/
uint8 llCalcConnMissCount(uint16 connId, uint32 connlastStartTime)
{
  llConnState_t    *connPtr = MAP_llDataGetConnPtr( connId );
  // Connection's Start Time
  uint32 connCurrentStartTime = ((RCL_Command *)connPtr->llTask->command)->timing.absStartTime;

  /********************************************/
  /************ Check Valid Input *************/
  /********************************************/
  // Check if this connection is valid and active?
  if (!(connPtr->activeConn))
  {
    return LL_INACTIVE_CONNECTIONS;
  }

  // Check if the connection start time is in the past
  // FALSE: Second parameter is greater than the first parameter; ST is in past.
  // Count the number of times we need to add an interval in order
  // for it to be in the future - this is the number of miss counts added.
  while ( MAP_llTimeCompare(connCurrentStartTime, connlastStartTime) )
  {
     // Add the connection's interval to the connlastStartTime
     // until reaching connCurrentStartTime.
     connlastStartTime+= (connPtr->curParam.connInterval * RAT_TICKS_IN_625US);

     // Increment the miss count of the connection untill
     // reaching connCurrentStartTime.
     connPtr->connMissCount++;
  }

  return LL_STATUS_SUCCESS;
}
/*********************************************************************
* @fn     llSelectConn
*
* @brief  This is the function that chooses the correct connection
*         out of 2 input connections.
*         The chosen connection would depend on the following parameters:
*         - LSTO of the connections.
*         - Connection establishement event scenario.
*         - Instant connection event scenario.
*         - The priority of the connections (configured by the user).
*         - The miss count of the connections.
*
* @design      /ref did_361975877
*
* input parameters
*
* @param connId1 - The First connection id for comperison.
* @param connId2 - The Second connection id for comperison.
*
* output parameters
*
* @param       None
*
* @return Connection ID of the next connection.
*         LL_INACTIVE_CONNECTIONS - In case both connections are inactive.
*/
uint16 llSelectConn(uint16 connId1, uint16 connId2)
{
  llConnState_t *connPtr1 = MAP_llDataGetConnPtr( connId1 );
  llConnState_t *connPtr2 = MAP_llDataGetConnPtr( connId2 );

  uint32 startTime1 = ((RCL_Command *)connPtr1->llTask->command)->timing.absStartTime;
  uint32 startTime2 = ((RCL_Command *)connPtr2->llTask->command)->timing.absStartTime;
  /********************************************/
  /************ Check Valid Input *************/
  /********************************************/
  // Check if these connections are valid and active?
  if (connPtr1->activeConn)
  {
    if (!(connPtr2->activeConn))
    {
      // Connection 1 is active, connection 2 is inactive.
      return (connId1);
    }
  }
  else if (connPtr2->activeConn)
  {
    // Connection 2 is active, connection 1 is inactive.
    return (connId2);
  }
  else
  {
    // Connection 1 & 2 are not-active.
    return (LL_INACTIVE_CONNECTIONS);
  }

  /* NOTE: Increment Miss Count of the un-selected connection would be done in the RealignConn() function. */

  /* TODO - As a first step return the connection that has CS running.
   * When multiple connections will be supported instanses and timeouts
   * need to be taken into consideration
   */
  /********************************************/
  /************ Check CS Priority *************/
  /********************************************/
  if ( connPtr1->connPriority == LL_QOS_CS_PRIORITY )
  {
    return (connId1);
  }
  else if ( connPtr2->connPriority == LL_QOS_CS_PRIORITY )
  {
    return (connId2);
  }

  /*  In this Stage - Both connections are active */

  /********************************************/
  /************ Check Conn LSTO ***************/
  /********************************************/
  // Check if either the number of remaining events to LSTO are less than
  // half the LSTO.
  // Is Conn 1 in half of its LSTO?
  if (connPtr1->numEventsLeft <= (connPtr1->expirationValue / 2))
  {
    // Is Conn 2 NOT in half of its LSTO?
    if (!(connPtr2->numEventsLeft <= (connPtr2->expirationValue / 2)))
    {
      // return Conn id that is in half of its LSTO.
      return (connId1);
    }
  }
  // Is Conn 2 in half of its LSTO?
  else if (connPtr2->numEventsLeft <= (connPtr2->expirationValue / 2))
  {
    // return Conn id that is in half of its LSTO.
    return (connId2);
  }

  /*  In this Stage - Both connections are not in half of their LSTO */

  /***************************************************/
  /************ Check Conn In Starvation ************/
  /**************************************************/
  // Check if one of the connections is in starvation mode.
  // Is Conn 1 in Starvation Mode?
  if (connPtr1->StarvationMode)
  {
    // Is Conn 2 NOT in Starvation Mode?
    if (!(connPtr2->StarvationMode))
    {
      // return Conn id that is in Starvation Mode.
      return (connId1);
    }
  }
  // Is Conn 2 in Starvation Mode?
  else if (connPtr2->StarvationMode)
  {
    // return Conn id that is in Starvation Mode.
    return (connId2);
  }

  /*  In this Stage - Both connections are not in Starvation Mode */

  /********************************************/
  /************ Check Conn Instant ************/
  /********************************************/

  // CONNECTION PARAMETER UPDATE //
  // Is Conn 1 in parameter update?
  if ((connPtr1->pendingParamUpdate != PARAM_UPDATE_NOT_PENDING) &&
      // Is instant in the next connection event or the one after that?
      (((connPtr1->nextEvent + 1) & 0xFFFF) >= connPtr1->paramUpdateEvent))
  {
    // Is Conn 2 NOT in parameter update?
    if (!((connPtr2->pendingParamUpdate != PARAM_UPDATE_NOT_PENDING) &&
        // Is instant in the next connection event or the one after that?
        (((connPtr2->nextEvent + 1) & 0xFFFF) >= connPtr2->paramUpdateEvent)))
    {
      // return Conn 1 that is in parameter update.
      return (connId1);
    }
  }
  // Is Conn 2 in parameter update?
  else if ((connPtr2->pendingParamUpdate != PARAM_UPDATE_NOT_PENDING) &&
      // Is instant in the next connection event or the one after that?
      (((connPtr2->nextEvent + 1) & 0xFFFF) >= connPtr2->paramUpdateEvent))
  {
    // return Conn 2 that is in parameter update.
    return (connId2);
  }

  // CHANNEL MAP UPDATE UPDATE //
  // Is Conn 1 in channel map update?
  if ((connPtr1->pendingChanUpdate == TRUE) &&
      // Is instant in the next connection event or the one after that?
      (((connPtr1->nextEvent + 1) & 0xFFFF) >= connPtr1->chanMapUpdateEvent))
  {
    // Is Conn 2 NOT in channel map update?
    if (!((connPtr2->pendingChanUpdate == TRUE) &&
         // Is instant in the next connection event or the one after that?
         (((connPtr2->nextEvent + 1) & 0xFFFF) >= connPtr2->chanMapUpdateEvent)))
    {
      // return Conn 1 that is in channel map update.
      return (connId1);
    }
  }
   // Is Conn 2 in channel map update?
  else if ((connPtr2->pendingChanUpdate == TRUE) &&
           // Is instant in the next connection event or the one after that?
           (((connPtr2->nextEvent + 1) & 0xFFFF) >= connPtr2->chanMapUpdateEvent))
  {
    // return Conn 2 that is in channel map update.
    return (connId2);
  }

  // PHY UPDATE //
  // Is Conn 1 in phy update?
  if ((connPtr1->pendingPhyUpdate == TRUE) &&
      // Is instant in the next connection event or the one after that?
      (((connPtr1->nextEvent + 1) & 0xFFFF) >= connPtr1->phyUpdateEvent))
  {
    // Is Conn 2 NOT in phy update?
    if (!((connPtr2->pendingPhyUpdate == TRUE) &&
         // Is instant in the next connection event or the one after that?
         (((connPtr2->nextEvent + 1) & 0xFFFF) >= connPtr2->phyUpdateEvent)))
    {
      // return Conn 1 that is in phy update?
      return (connId1);
    }
  }
   // Is Conn 2 in phy update?
  else if ((connPtr2->pendingPhyUpdate == TRUE) &&
           // Is instant in the next connection event or the one after that?
           (((connPtr2->nextEvent + 1) & 0xFFFF) >= connPtr2->phyUpdateEvent))
  {
    // return Conn 2 that is in phy update.
    return (connId2);
  }

  /*  In this Stage - Both connections are not in an instant connection event */

  /********************************************/
  /************ Check Conn Priority ***********/
  /********************************************/
  // Is Conn 1 in with higher priority then Conn2?
  if (connPtr1->connPriority > connPtr2->connPriority)
  {
    // return the higher priority connection.
    return (connId1);
  }
  // Is Conn 2 in with higher priority then Conn2.
  else if (connPtr1->connPriority < connPtr2->connPriority)
  {
    // return the higher priority connection.
    return (connId2);
  }

  /*  In this Stage - Both connections have the same priority */

  /********************************************/
  /**** Check Conn Interval and Miss Count ****/
  /********************************************/
  // Is Conn 1 interval larger then Conn 2 interval?
  if (connPtr1->curParam.connInterval > connPtr2->curParam.connInterval)
  {
    // Is (Conn 2 interval * Conn 2 miss count) larger then Conn 1 interval?
    if (connPtr1->curParam.connInterval <= connPtr2->curParam.connInterval * connPtr2->connMissCount)
    {
      // The interval of Conn 2 is smaller and missed more air time then the interval of Conn 1.
      return (connId2);
    }
    else
    {
      return (connId1);
    }
  }
  // Is Conn 2 interval larger then Conn 1 interval?
  if (connPtr2->curParam.connInterval > connPtr1->curParam.connInterval)
  {
    // Is (Conn 1 interval * Conn 1 miss count) larger then Conn 1 interval?
    if (connPtr2->curParam.connInterval <= connPtr1->curParam.connInterval * connPtr1->connMissCount)
    {
     // The interval of Conn 1 is smaller and missed more air time then the interval of Conn 2.
      return (connId1);
    }
    else
    {
      return (connId2);
    }
  }

  /*  In this Stage - Both connections have the same interval */

  /********************************************/
  /*********** Check Conn Miss Count **********/
  /********************************************/
  // Is Conn 1 miss count larger then Conn 2 miss count?
  if (connPtr1->connMissCount > connPtr2->connMissCount)
  {
    // Conn 1 was missed more times then Conn 2.
    return (connId1);
  }
  // Is Conn 2 miss count larger then Conn 1 miss count?
  else if (connPtr2->connMissCount > connPtr1->connMissCount)
  {
    // Conn 2 was missed more time then Conn 1.
    return (connId2);
  }

  /*  In this Stage - Both connections have the same missed count */

  /********************************************/
  /*********** Check Conn Start Time **********/
  /********************************************/
  // Choose the connection that starts first.
  if (MAP_llTimeCompare(startTime1 ,startTime2))
  {
    return (connId2);
  }
  else
  {
    return (connId1);
  }
}// End of llSelectConn

/*********************************************************************
* @fn     llFindNextActiveConnId
*
* @brief  This function return the next active connection from the
*         startConnId
*
* @design      /ref did_361975877
*
* input parameters
*
* @param startConnId - The starting connection id to look for the next
*                      active connection id.
*
* output parameters
*
* @param       None
*
* @return Connection ID of the next connection.
*         LL_INACTIVE_CONNECTIONS - No active connections found after startConnId.
*/
uint16 llFindNextActiveConnId(uint16 startConnId)
{
  /********************************************/
  /********* Check Single Connection **********/
  /********************************************/
  // Check if only one connection is active
  if ( llConns.numActiveConns == 1 )
  {
    // Sanity Check
    if (startConnId == activeConns[0])
    {
      // There are no more active connections
      return (LL_INACTIVE_CONNECTIONS);
    }
  }

  /********************************************/
  /********** Find Next Active Conn ID ********/
  /********************************************/
  // Find Next active connection id.
  // Go over the active connections array to find the next active connection.
  for (uint8 i=0; i < (llConns.numActiveConns - 1); i++)
  {
    // Find the startConnId in the active connections array
    if (startConnId == activeConns[i])
    {
      // In case this is the last active connection
      if ( (i+1) == llConns.numActiveConns)
      {
        return (LL_INACTIVE_CONNECTIONS);
      }
      // return the connection id of the next active connection
      else
      {
        return (activeConns[i+1]);
      }
    }
  }// End of   for (uint8 i=0; i<llConns.numActiveConns-1; i++)

  /********************************************/
  /*************** Invalid Input **************/
  /********************************************/
  return (LL_INACTIVE_CONNECTIONS);

}// End of llFindNextActiveConnId

/*********************************************************************
* @fn     llIsThereACollisionBetweenConn
*
* @brief  This function check if there is a collision or not
*         between 2 connection tasks?
*
* @design      /ref did_361975877
*
* Note:   The collision check would use the connMinTimeLength for comparison.
*
* input parameters
*
* @param connId1 - The first  connection id.
* @param connId2 - The second connection id.
*
* output parameters
*
* @param       None
*
* @return TRUE  - There is a collision.
*         FALSE - There is no collision.
*         LL_INACTIVE_CONNECTIONS - Invalid input.
*/
uint16 llIsThereACollisionBetweenConn(uint16 connId1, uint16 connId2)
{
  llConnState_t *connPtr1 = MAP_llDataGetConnPtr( connId1 );
  llConnState_t *connPtr2 = MAP_llDataGetConnPtr( connId2 );

  uint32 startTime1 = ((RCL_Command *)connPtr1->llTask->command)->timing.absStartTime;
  uint32 startTime2 = ((RCL_Command *)connPtr2->llTask->command)->timing.absStartTime;
  /********************************************/
  /************ Check Valid Input *************/
  /********************************************/
  // Check if these connections are valid and active?
  if (connPtr1->activeConn)
  {
    if (!(connPtr2->activeConn))
    {
      // Connection 1 is active, connection 2 is inactive.
      // No collision.
      return (FALSE);
    }
  }
  else if (connPtr2->activeConn)
  {
    // Connection 2 is active, connection 1 is inactive.
    // No collision.
    return (FALSE);
  }
  else
  {
    // Connection 1 & 2 are not-active.
    return (LL_INACTIVE_CONNECTIONS);
  }

  /********************************************/
  /************** Collision Check *************/
  /********************************************/
  // Check if the second start before the first?
  if (MAP_llTimeCompare(startTime1 , startTime2))
  {
    // Check for collision between the two (in RAT TICKS).
    if (MAP_llTimeCompare(startTime1 , startTime2 + connPtr2->connMinTimeLength))
    {
      // no collision.
      return FALSE;
    }
    else // There is a collision.
    {
      return TRUE;
    }
  }
  else // First connection starts before the second
  {
    // Check for collision between the two (in RAT TICKS).
    if (MAP_llTimeCompare(startTime2 , startTime1 + connPtr1->connMinTimeLength))
    {
      // no collision.
      return FALSE;
    }
    else // There is a collision.
    {
      return TRUE;
    }
  }

}// End of llIsThereACollisionBetweenConn

/*********************************************************************
* @fn     llCalcConnMaxTimeLength
*
* @brief  This function calculates and updates the maximum time length of a
*         connection. This time would be the difference in time between
*         the entered connection's start time and the next higher priority
*         connection's start time.
*
* @design      /ref did_361975877
*
* input parameters
*
* @param startConnId - The first connection id to calculate from.
*
* @param bestSelectedConnIdAfterStart - In case the startConnId is the earliestConn that
*                                       was chosen, this param is the the winning connection id
*                                       in case of multiple collisions under min time length.
*
* output parameters
*
* @param       None
*
* @return      LL_STATUS_SUCCESS for success.
*              LL_INACTIVE_CONNECTIONS for Invalid input.
*/
uint32 llCalcConnMaxTimeLength(uint16 startConnId, uint16 bestSelectedConnIdAfterStart)
{
  uint16 selectedConnId = startConnId;
  uint16 nextConnId = selectedConnId;
  llConnState_t *connPtr1 = MAP_llDataGetConnPtr( startConnId );
  llConnState_t *connPtr2 = NULL;

  uint32 startTime1 = ((RCL_Command *)connPtr1->llTask->command)->timing.absStartTime;
  uint32 startTime2;

  /* NOTE: The maximum connection time length would be adjusted
  once used in the timeOutTime / endTime variables */

  /********************************************/
  /************ Check Valid Input *************/
  /********************************************/
  // Check if startConnId connection is active?
  if (!(connPtr1->activeConn))
  {
    // Connection inactive.
    return (LL_INACTIVE_CONNECTIONS);
  }

  // In case the maximum time length was externally update - abort and return SUCCESS.
  if (connPtr1->connMaxTimeExternalUpdateInd)
  {
    return LL_STATUS_SUCCESS;
  }

  /********************************************/
  /********* Check Single Connection **********/
  /********************************************/
  // Check if only one connection is active
  if ( llConns.numActiveConns == 1 )
  {
    // Sanity Check
    LL_ASSERT( selectedConnId == activeConns[0] );

    // Update the connection's interval as the max time length of
    // the connection because there is only 1 connection.
    connPtr1->connMaxTimeLength = connPtr1->curParam.connInterval * RAT_TICKS_IN_625US;

    // return SUCCESS
    return LL_STATUS_SUCCESS;
  }

  /*******************************************************************/
  /******** Find Delta from earliestConn to bestSelectedConn *********/
  /*******************************************************************/
  // In case bestSelectedConnIdAfterStart is not LL_INACTIVE_CONNECTIONS
  // This means the startConnId is the earliest connection that was chosen to be
  // sent out despite the fact it was not in the highest priority while a collision.
  // The max connection time length of the startConnId (earliest) would be
  // the differance between its start time till the start time of the bestSelectedConnIdAfterStart connection.
  if (bestSelectedConnIdAfterStart != LL_INACTIVE_CONNECTIONS)
  {
    // Get the bestSelectedConnIdAfterStart connection's info.
    connPtr2 = MAP_llDataGetConnPtr( bestSelectedConnIdAfterStart );
    startTime2 = ((RCL_Command *)connPtr2->llTask->command)->timing.absStartTime;

    // Check if the start time of the bestSelectedConnIdAfterStart connection is higher than the startConnId's interval.
    // if so - return the startConnId's interval.
    if (MAP_llTimeCompare(startTime2, startTime1 + (connPtr1->curParam.connInterval * RAT_TICKS_IN_625US)))
    {
      // Update the connMaxTimeLength value.
      connPtr1->connMaxTimeLength = connPtr1->curParam.connInterval * RAT_TICKS_IN_625US;

      // Return SUCCESS.
      return LL_STATUS_SUCCESS;
    }
	// The differance between the start time of the connections is lower than first connection's interval.
    // Return the difference in start time between connections in RAT TICKS
    // Only if the start time of the higher priority is lower then the
    // startConnId interval.
    else
    {
      // Update the connMaxTimeLength value.
      connPtr1->connMaxTimeLength = MAP_llTimeDelta(startTime2, startTime1);

      // Make sure the Max Conn Time Length is Larger than the Min Conn Time Length
      if (connPtr1->connMaxTimeLength < connPtr1->connMinTimeLength)
      {
        // Update the Max Conn Time Length to be the Min Conn Time Length.
        connPtr1->connMaxTimeLength = connPtr1->connMinTimeLength;
      }

      // Return SUCCESS.
      return LL_STATUS_SUCCESS;
    }
  }

  // More than 1 connection.
  // Get next active connection after the current start connection id
  nextConnId = llFindNextActiveConnId(startConnId);

  /********************************************/
  /******** Find higher priority conn *********/
  /********************************************/
  // Go over the active connections until found a higher priority one.
  while (nextConnId != LL_INACTIVE_CONNECTIONS)
  {
    // Get connection's info.
    connPtr2 = MAP_llDataGetConnPtr( nextConnId );

    startTime2 = ((RCL_Command *)connPtr2->llTask->command)->timing.absStartTime;

    // Check if the start time of the next connection is higher than the startConnId's interval.
    // if so - return the startConnId's interval.
    if (MAP_llTimeCompare(startTime2, startTime1 + (connPtr1->curParam.connInterval * RAT_TICKS_IN_625US)))
    {
      // Update the connMaxTimeLength value.
      connPtr1->connMaxTimeLength = connPtr1->curParam.connInterval * RAT_TICKS_IN_625US;
      // Return SUCCESS.
      return LL_STATUS_SUCCESS;
    }

    // Check if the new found connection is in higher priority
    // then the startConnId.
    selectedConnId = llSelectConn(nextConnId, startConnId);

    // In case we have found a higher priority connection
    // after startConnId.
    if (selectedConnId == nextConnId)
    {
      // Double check.
      // Return the difference in start time between connections in RAT TICKS
      // Only if the start time of the higher priority is lower then the
      // startConnId interval.
      if (MAP_llTimeCompare(startTime1 + (connPtr1->curParam.connInterval * RAT_TICKS_IN_625US) ,startTime2))
      {
        // Update the connMaxTimeLength value.
        connPtr1->connMaxTimeLength = MAP_llTimeDelta(startTime2, startTime1);

        // Make sure the Max Conn Time Length is Larger than the Min Conn Time Length
        if (connPtr1->connMaxTimeLength < connPtr1->connMinTimeLength)
        {
          // Update the Max Conn Time Length to be the Min Conn Time Length.
          connPtr1->connMaxTimeLength = connPtr1->connMinTimeLength;
        }

        // Return SUCCESS.
        return LL_STATUS_SUCCESS;
      }
      // We have reached a higher priority connection but it is located
      // after the startConnId's connection interval
      else
      {
        // Update the connection's interval as the max time length of the connection in RAT TICKS.
        connPtr1->connMaxTimeLength = connPtr1->curParam.connInterval * RAT_TICKS_IN_625US;
        // Return SUCCESS.
        return LL_STATUS_SUCCESS;
      }
    }// End of if (selectedConnId == nextConnId)

    // Get next active connection after last one.
    nextConnId =  llFindNextActiveConnId(nextConnId);

  }//  End of (nextConnId != LL_INACTIVE_CONNECTIONS)

  // In case there is no higher priority connection after startConnId.
  // The max time length of the connection would be it's interval in RAT TICKS.
  connPtr1->connMaxTimeLength = connPtr1->curParam.connInterval * RAT_TICKS_IN_625US;

  // Return SUCCESS.
  return LL_STATUS_SUCCESS;

}// End of llCalcConnMaxTimeLength

/*********************************************************************
* @fn     llFindNextConn
*
* @brief  This is the function that selects the next connection event.
*         In case of a collision it would choose the most suitable
*         connection event among the colliding connections.
*
* @design      /ref did_361975877
*
* @param  void.
*
* @return Connection ID of the next connection.
*         LL_INACTIVE_CONNECTIONS - In case there are no active connections.
*/
uint16 llFindNextConn(void)
{
  //uint32 currentTime = MAP_llGetCurrentTime();
  uint8 activeConnIndex1 = 0, activeConnIndex2 = 1;
  uint16 selectedConnId = LL_INACTIVE_CONNECTIONS, earliestConnId = LL_INACTIVE_CONNECTIONS;
  uint8  numOfCollisionComprison = 0;

  /********************************************/
  /********* Find First Active Conn ID ********/
  /********************************************/

  // In case we have only one connection - return it.
  if ( llConns.numActiveConns == 1 )
  {
    // Return the only connection found.
    return (activeConns[0]);
  }

  // Set the first active connection.
  selectedConnId = activeConns[activeConnIndex1];

  // Set the earliest connection id
  earliestConnId = selectedConnId;

  /********************************************/
  /********* Find the next Connection *********/
  /********************************************/
  // Look for the most appropriate connection task among all active connection tasks
  // Increment the to the nextActiveConnId and also increment the number of comprisons.
  // Go over the active connections.
  while ( ((activeConnIndex1 < llConns.numActiveConns) && (activeConnIndex2 < llConns.numActiveConns)) && ( numOfCollisionComprison < LL_MAX_COLLISION_COMPRISON) )
  {
    /***********************************/
    /********* Collision Check *********/
    /***********************************/
    // Check if the first connection event overlaps the second connection event?
    if (llIsThereACollisionBetweenConn(activeConns[activeConnIndex1],activeConns[activeConnIndex2]) == TRUE)
    {
      // There is a collision - choose the highest priority connection.
      // Valid Output: No need to check that selectedConnId - because we are going over active
      // connections only.
      selectedConnId = llSelectConn(activeConns[activeConnIndex1], activeConns[activeConnIndex2]);
    }
    else
    {
      // Exit - We have found the selected connection between overlapping connections.
      break;
    }

    // Next iteration.
    // In case the selectedConnId is origin from activeConnIndex2
    if (selectedConnId == activeConns[activeConnIndex2] )
    {
      // increment the second active connection index.
      activeConnIndex1 = activeConnIndex2;
    }

    // increment the second active connection index.
    activeConnIndex2++;

    // Increment the number of comprisons index.
    // It is limited to be up to LL_MAX_COLLISION_COMPRISON.
    numOfCollisionComprison++;

    if ( activeConns[activeConnIndex2] == LL_INACTIVE_CONNECTIONS )
    {
      // This can happen if we removed the handover connection.
      // Change the connIndex to invalid
      activeConnIndex2 = LL_INACTIVE_CONNECTIONS;
    }
  }// End of while (currentActiveConnId != LL_INACTIVE_CONNECTIONS)

  // if the earliestConnId is the same as the selectedConnId or there is
  // collision between them.
  if ((earliestConnId == selectedConnId) || (llIsThereACollisionBetweenConn(earliestConnId,selectedConnId) == TRUE))
  {
    // Set the Maximum Connection Length for the selectedConnId.
    // This would be the differnce in time between the selectedConnId and the connection
    // with the higher priority arrives after it.
    llCalcConnMaxTimeLength(selectedConnId ,LL_INACTIVE_CONNECTIONS);

    // Return the selectedConnId because it has higher priority than earliestConnId and they are colliding.
    return (selectedConnId);
  }
  // In case the earliestConnId is before selectedConnId and there is no collision
  else
  {
    // Set the Maximum Connection Length for the earliestConnId.
    // This would be the differnce in time between the earliestConnId and the connection
    // selectedConnId.
    llCalcConnMaxTimeLength(earliestConnId , selectedConnId);

    // Return the earliestConnId
    return (earliestConnId);
  }

}// End of llFindNextConn

/*********************************************************************
* @fn     llGetLstoNumOfEventsLeftMargin
*
* @brief  This is the function that calculates the num of events left margin
*         that would activate the algorithm that would make sure the packet
*         sent would be heard and the starvation would stop.
*
* @design      /ref did_408769671
*
* input parameters
*
* @param connId - The connection id to calculates it's margin.
*
* output parameters
*
* @param       None
*
* @return Num Of Events Left Margin.
*
*/
uint16 llGetLstoNumOfEventsLeftMargin(uint16 connId)
{
  llConnState_t *connPtr = MAP_llDataGetConnPtr( connId );
  uint16 LstoNumOfEventsLeftMargin = 0;

  // Check valid input
  if (connPtr == NULL)
  {
    return (LstoNumOfEventsLeftMargin);
  }

  // Get the starvation event number.
  uint16 getStarvationNumEvent = (connPtr->expirationValue / 2);

  // Need to make sure the starvation event number is over LL_MIN_NUM_EVENTS_LEFT_LSTO_MARGIN.
  if (getStarvationNumEvent <= LL_MIN_NUM_EVENTS_LEFT_LSTO_MARGIN)
  {
    return (LstoNumOfEventsLeftMargin);
  }

  // The LstoNumOfEventsLeftMargin would be about 10% from half of it's LSTO.
  LstoNumOfEventsLeftMargin = (getStarvationNumEvent / 10) +
                              (((getStarvationNumEvent % 10) > 0)? 1 : 0 );

  // Make sure that Number of Events Left is larger than calculated LstoNumOfEventsLeftMargin.
  // In not - reset LstoNumOfEventsLeftMargin.
  if (connPtr->numEventsLeft <= LstoNumOfEventsLeftMargin)
  {
    LstoNumOfEventsLeftMargin = 0;
  }

  // return value of LstoNumOfEventsLeftMargin.
  return (LstoNumOfEventsLeftMargin);
}

/*********************************************************************
* @fn     llSetStarvationMode
*
* @brief  This is the function that sets the starvation mode
*         to be ON/OFF and performs all actions related.
*
* @design      /ref did_408769671
*
* input parameters
*
* @param connId        - The connection id for the starvatiom mode settings.
* @param setOnOffValue - LL_SET_STARAVATION_MODE_OFF / LL_SET_STARAVATION_MODE_ON
*
* Note: Activate the Starvation Mode only in case the connection reached
*       half of its LSTO (with a certain margin) and it is not in connection establishment.
*
* output parameters
*
* @param       None
*
* @return      LL_STATUS_SUCCESS for success.
*              LL_INACTIVE_CONNECTIONS for Invalid input.
*/
uint8 llSetStarvationMode(uint16 connId, uint8 setOnOffValue)
{
   llConnState_t    *connPtr = MAP_llDataGetConnPtr( connId );

   // Check valid input.
   if ((connPtr == NULL) || (!(connPtr->activeConn)))
   {
     return LL_INACTIVE_CONNECTIONS;
   }

  /****************************************************/
  /************* Peripheral Starvation Mode ON *************/
  /****************************************************/
  if (setOnOffValue == LL_SET_STARVATION_MODE_ON)
  {
     // In case we have chosen connId because it reached half of it's LSTO:
     // Starvation mode is ON - This means this connection was starved until reaching this connection event.
     // Need to make sure the central actually hears the peripheral (or visa versa) - This is done by
     // increasing the probability that SN BIT from the central is changed in LL_MAX_PERIPHERAL_NUM_LSTO_RETRIES continues transfers.
     // When the SN BIT is changed -> the central received the peripheral's packet,
     // We are doing this by retrying to send the connection event for LL_MAX_PERIPHERAL_NUM_LSTO_RETRIES continues transfers.
     // **** NOTE: Make SURE the connection reached half of it's LSTO before activating the starvation mode ****
     // **** NOTE: We are not checking the SN bit received to simplify the starvation mode ****

     // Update the starvation mode bit to be ON.
     connPtr->StarvationMode = TRUE;

  }// End of if (setOnOffValue == LL_SET_STARVATION_MODE_ON).
  /*****************************************************/
  /************* Peripheral Starvation Mode OFF *************/
  /*****************************************************/
  else if (setOnOffValue == LL_SET_STARVATION_MODE_OFF)
  {
      // Reset the Lsto number of retries and the starvation mode bit.
      connPtr->StarvationMode = FALSE;
      connPtr->numLSTORetries = 0;

  }// End of LL_SET_STARVATION_MODE_OFF.
  // Invalid input for setOnOffValue.
  else
  {
      return LL_INACTIVE_CONNECTIONS;
  }

  return LL_STATUS_SUCCESS;
}

/*******************************************************************************
 * @fn          llGetNextConn
 *
 * @brief       This function is used to find the next connection
 *
 *              Note: Assumes there's at least one active connection.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      Connection ID of the next connection.
 */
uint8 llGetNextConn( void )
{
  uint8            i;
  uint32           curTime, connLastStartTimeBeforeRealigment;
  uint8            numActiveConns;
  uint8            nextConnId;
  llConnState_t    *connPtr = NULL;

  // Sanity Check
  // There should be at least one active connection.
  LL_ASSERT( llConns.numActiveConns != 0 );

  // clear the number of active conns counter
  numActiveConns = 0;

  // Initialize activeConnsArray
  osal_memset(activeConns, LL_INACTIVE_CONNECTIONS, maxNumConns);

  // take a snapshot of the current time with pad
  curTime = MAP_llGetCurrentTime() + LL_TOPO_PAD;

  /********************************************/
  /************ Realign Connections ***********/
  /********************************************/
  // check/align if necessary each active connection to the future
  for (i=0; i<maxNumConns; i++)
  {
    // Get the info pointer of the connection.
    connPtr = MAP_llDataGetConnPtr( i );

    // check if this connection is active
    if ( connPtr->activeConn )
    {
      // Add this connection to the list of active connections
      activeConns[numActiveConns++] = connPtr->connId;

      // Save start time before realigment
      // For miss count calculations.
      connLastStartTimeBeforeRealigment = ((RCL_Command *)connPtr->llTask->command)->timing.absStartTime;

      // Realign to the future.
      MAP_llRealignConn( connPtr, curTime );

      // Calculate the connection's miss count
      // according to number of realignments from last start time
      // to the new start time.
      llCalcConnMissCount(i, connLastStartTimeBeforeRealigment);

      // Update the connection's minimum time length
      // Before choosing the scheduled one.
      llEstimateConnMinTimeLength(i);
    }
  }

  /********************************************/
  /************ Verify Connections ************/
  /********************************************/
  // verify the number of active connection counters match
  if ( numActiveConns != llConns.numActiveConns )
  {
    // Sanity Check:
    // Should never hit this!
    LL_ASSERT( FALSE );

    // report failure to Host
    MAP_llHardwareError( HW_FAIL_FW_INTERNAL_ERROR );
  }

  // NOTE: We should not get here if we have 1 connection and handover
  // is in progress as the serving node
  // This is currently protected by the ll_scheduler (the only location
  // this function is called from.
  // Need to add another protection to prevent the scheduler from scheduling
  // this connection
  /********************************************/
  /********* Check Single Connection **********/
  /********************************************/
  // check if only one connection is active
  if ( llConns.numActiveConns == 1 )
  {
    // find and return it
    for (i=0; i<maxNumConns; i++)
    {
      connPtr = MAP_llDataGetConnPtr( i );

      // check if this connection is active
      if ( connPtr->activeConn )
      {
        // Sanity Check
        LL_ASSERT( connPtr->connId == activeConns[0] );

        /* Please Note that the timeoutTime / endTime of this connection was already set under
           the MAP_llRealignConn(). There are no other connections so connMaxTimeLength
           has no meaning and its value would be the connection's interval */

        // Update the max time length just in case.
        llCalcConnMaxTimeLength(connPtr->connId ,LL_INACTIVE_CONNECTIONS);

        // Enable Starvation Mode in case the connection reached half of it's LSTO.
        // Make sure we are not in the first packet of the connection.
        if ( (connPtr->numEventsLeft <= (connPtr->expirationValue / 2)) && (connPtr->firstPacket == FALSE) )
        {
          MAP_llSetStarvationMode(connPtr->connId, LL_SET_STARVATION_MODE_ON);
        }

        return( connPtr->connId );
      }
    }

    // Sanity Check:
    // Should never hit this!
    LL_ASSERT( FALSE );

    // report failure to Host
    MAP_llHardwareError( HW_FAIL_FW_INTERNAL_ERROR );
  }

  // If there are 2 connections check if one is in the middle of an handover process
  if ( llConns.numActiveConns == 2 )
  {
    uint16 connHandle = MAP_llReturnNonHandoverConn();

    // If there is a connection in the middle of handover process
    // return the other connection handle, otherwise continue
    if ( connHandle != LL_CONNHANDLE_INVALID )
    {
      return connHandle;
    }
  }

  /*************************************************/
  /*********** Check Multiple Connection **********/
  /************************************************/

  /********************************************/
  /************* Sort Connections *************/
  /********************************************/
  // sort by start time
  MAP_llShellSortActiveConns(activeConns , llConns.numActiveConns);

  // If there is a connection in the middle of an handover process, remove it
  MAP_llRemoveHandoverConn(activeConns , llConns.numActiveConns);

  /********************************************/
  /************ Find Next Connection **********/
  /********************************************/
  // Find next connection according to it's priority.
  nextConnId = llFindNextConn();

  // Sanity Check
  if (nextConnId == LL_INACTIVE_CONNECTIONS)
  {
    // Should never hit this!
    LL_ASSERT( FALSE );

    // report failure to Host
    MAP_llHardwareError( HW_FAIL_FW_INTERNAL_ERROR );
  }

  // Get nextConnId info pointer.
  connPtr = MAP_llDataGetConnPtr( nextConnId );

  // Enable Starvation Mode in case the connection reached half of it's LSTO.
  // Make sure we are not in the first packet of the connection.
  if ( (connPtr->numEventsLeft <= (connPtr->expirationValue / 2)) && (connPtr->firstPacket == FALSE) )
  {
    MAP_llSetStarvationMode(nextConnId, LL_SET_STARVATION_MODE_ON);
  }
  /* Note: The nextConnId max time length is calculated in llFindNextConn() routine */
  /* Note: Start Timer for the max connection length in llLinkSchedSetup() routine  */

  // return the next connection id.
  return( nextConnId );
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llGetMinCI
 *
 * @brief       This function is used to find the connection with the minimum
 *              connection interval.
 *
 *              Note: This routine assumes there's at least one active
 *                    connection.
 *
 *              Note: If a connection's pendingChanUpdate flag is TRUE, then
 *                    the update parameter connection interval will be used
 *                    instead of the current connection interval. If this flag
 *                    is FALSE, but there's a Connection Update control
 *                    procedure pending, then Create Connection will indicate
 *                    the Controller Busy status.
 *
 * input parameters
 *
 * @param       connInterval - The CI of the connection being created.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      The minimal connection interval, in 625us ticks.
 */
uint16 llGetMinCI( uint16 connInterval )
{
  uint16 minCI = 0xFFFF;

  // Sanity Check
  // There should be at least one active connection.
  LL_ASSERT( llConns.numActiveConns != 0 );

  for (uint8 i=0; i<maxNumConns; i++)
  {
    llConnState_t *connPtr = MAP_llDataGetConnPtr( i );

    // check if this connection is active
    if ( connPtr->activeConn )
    {
      // check if we have a minimum connection yet
      if ( minCI == 0xFFFF )
      {
        // no, so make this active connection the minimum so far, but do so
        // based on either the current or an active update connection interval
        if ( connPtr->pendingParamUpdate )
        {
          minCI = connPtr->paramUpdate.connInterval;
        }
        else
        {
          minCI = connPtr->curParam.connInterval;
        }
      }
      else // we already have a minimum connection
      {
        // check if an Update Parameter control procedure packet has been sent
        if ( connPtr->pendingParamUpdate )
        {
          // it has, so compare to the update parameter connection interval
          if ( connPtr->paramUpdate.connInterval < minCI )
          {
            // yes, so make it the new minimum connection interval
            minCI = connPtr->paramUpdate.connInterval;
          }
        }
        else // no, so use the current parameter connection interval
        {
          // so compare it to this active connection's start time
          if ( connPtr->curParam.connInterval < minCI )
          {
            // yes, so make it the new minimum connection interval
            minCI = connPtr->curParam.connInterval;
          }
        }
      }
    }
  }

  // see if the new connection is less than minCI of active connections
  if ( connInterval < minCI )
  {
    return( connInterval );
  }
  else // it isn't
  {
    return( minCI );
  }
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llConnExists
 *
 * @brief       This function determines whether we are already in a connection
 *              with the peer device. The peer address and address type may be
 *              checked against every active connection.
 *
 * input parameters
 *
 * @param       peerAddr     - Peer device address.
 * @param       peerAddrType - Peer device address type.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE:  Already in a connection with this peer device.
 *              FALSE: Not in a connection with this peer device.
 */
uint8 llConnExists( uint8 *peerAddr,
                    uint8  peerAddrType )
{
  uint8 i;
  uint8 rlIndex = INVALID_RESOLVE_LIST_INDEX;

  // first, check if there's at least one connection
  if ( llConns.numActiveConns == 0 ) return( FALSE );

  if ( MAP_LL_PRIV_IsRPA( peerAddrType, peerAddr ) )
  {
    // Try to resolve RPA to find ID address
    rlIndex = MAP_LL_PRIV_IsResolvable( peerAddr, resolvingList );
  }

  // Check each connection's peer device address and address type
  for ( i = 0; i < maxNumConns; i++ )
  {
    llConnState_t *connPtr = MAP_llDataGetConnPtr( i );

    if ( connPtr->activeConn )
    {
      {
        // Check if the peer address is in the resolvingList
        if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
        {
          // Check the resolvable peer address and address type against the connection
          if ( (connPtr->peerInfo.peerAddrType == resolvingList[rlIndex].idAddrType) &&
                LL_CMP_BDADDR( connPtr->peerInfo.peerAddr, resolvingList[rlIndex].idAddr ) )
          {
            return( TRUE );
          }
        }
        else
        {
          // Check the peer address and address type against the connection
          if ( (connPtr->peerInfo.peerAddrType == peerAddrType) &&
                LL_CMP_BDADDR( connPtr->peerInfo.peerAddr, peerAddr ) )
          {
            return( TRUE );
          }
        }
      }
    }
  }

  return( FALSE );
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llConnCleanup
 *
 * @brief       This function is used to clean up the LL connection data
 *              structures, queued Tx data packet, and the connection task.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llConnCleanup( llConnState_t *connPtr )
{
  RCL_Buffer_TxBuffer *pEntry;
  halIntState_t  cs;

  // Sanity check
  LL_ASSERT( connPtr != NULL );

  if (connPtr != NULL)
  {
    HAL_ENTER_CRITICAL_SECTION(cs);

    // stop the APTO timer, if running
    MAP_osal_CbTimerStop( connPtr->aptoTimerId );

  // free all Tx entries, finished or not
  // Note: The memory for the Tx Data queue is currently static in the sense
  //       that it is malloc'ed once and never freed. However, the connection's
  //       pointer to the queue is cleared whenever the connection is alloc'ed.
  if ( connPtr->pTxDataEntryQ != NULL )
      {
      // remove the buffer from the LL list
      while( (pEntry=RCL_TxBuffer_get(&((txDataQ_t *)(connPtr->pTxDataEntryQ))->llDataBuffers)) != NULL )
      {
        // check if its last packet
        if ( (*((uint8 *)(pEntry->data + (pEntry->numPad -1))) != LL_DATA_PDU_HDR_LLID_CONTROL_PKT) &&
             (CHECK_LAST_PKT(pEntry->pad0)))
        {
          // bump the number of buffers freed
          numComplPkts++;
        }
        // free the TX data buffer
        MAP_osal_bm_free( (void *)pEntry );
      }
      // check temp Tx queue for any remaining entries and remove
      while( (pEntry = RCL_TxBuffer_get(&((txDataQ_t *)(connPtr->pTxDataEntryQ))->tmpDataBuffers)) != NULL )
      {
        //check if its last packet
        if ( (*((uint8 *)(pEntry->data + (pEntry->numPad -1))) != LL_DATA_PDU_HDR_LLID_CONTROL_PKT) &&
             (CHECK_LAST_PKT(pEntry->pad0)))
        {
          // bump the number of buffers freed
          numComplPkts++;
        }
        // free the TX data buffer
        MAP_osal_bm_free( (void *)pEntry );
      }
      // Clear the pTxDataEntryQ
      llClearTxDataQueue(connPtr->pTxDataEntryQ);
    }

    // reset the number of Tx data buffers
    numTxDataBufs = maxNumTxDataBufs;

    // check that the shared RX Queue could be free.
    // set the RX Queue pointer to NULL in case this is NOT the last connection,
    linkParam[connPtr->connId].rxBuffers.head = NULL;
    linkParam[connPtr->connId].rxBuffers.tail = NULL;
    linkParam[connPtr->connId].txBuffers.head = NULL;
    linkParam[connPtr->connId].txBuffers.tail = NULL;

    if ((llConns.numLLConns == 1) && ( connPtr->pRxDataEntryQ != NULL ))
    {
      List_clearList((List_List *)connPtr->pRxDataEntryQ);
      List_clearList(&rxDataQ.finishedBuffers);
      List_clearList(&rxDataQ.multiBuffers);
      // check the entire Rx buffers
      for (uint8 i=0; i<NUM_RX_DATA_ENTRIES; i++)
      {
        // free the buffer
        MAP_osal_bm_free( rxDataQ.dataBuffers[i] );
        rxDataQ.dataBuffers[i] = NULL;
      }
      rxDataQ.length = 0;
    }

    // check if we completed any packets
    if ( numComplPkts > 0 )
    {
      uint16 connId = connPtr->connId;
      uint16 numCompletedPackets = numComplPkts;

      // and send credits to the Host
      MAP_HCI_NumOfCompletedPacketsEvent( 1,
                                          &connId,
                                          &numCompletedPackets );

      // clear count
      numComplPkts = 0;
    }
    // free the associated task block
    // Note: If the last task, llState will be set to Idle.
    MAP_llFreeTask( &connPtr->llTask );

    // release the connection
    MAP_llReleaseConnId( connPtr );


    HAL_EXIT_CRITICAL_SECTION(cs);
  } // connPtr != NULL

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llConnTerminate
 *
 * @brief       This function is used to handle the commmon termination
 *              operations for a LL connection.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection.
 * @param       reason  - The reason code for the termination, to send to Host.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llConnTerminate( llConnState_t *connPtr,
                      uint8          reason )
{
  LL_ASSERT( connPtr != NULL );

  if ( connPtr != NULL )
  {
    // let the application know
    MAP_LL_DisconnectCback( (uint16)connPtr->connId, reason );

    // cleanup the connection data structures and task
    MAP_llConnCleanup( connPtr );

    // Only call the scheduler if the connection isn't terminated due to a successful handover
    if ( reason != LL_CONN_TERMINATE_SUCCESSFUL_HANDOVER )
    {
      // determine next task (if any) and schedule it
      MAP_llScheduler();
    }
  }

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llPendingUpdateParam
 *
 * @brief       This function is used to check if any active connections have
 *              an Update Parameter control procedure pending.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      TRUE:  There is an update control procedure for at least one
 *                     active connection.
 *              FALSE: There is no update control procedure on any active
 *                     connection.
 */
uint8 llPendingUpdateParam( void )
{
  // check if an update parameter control procedure is active on any connections
  for (uint8 i=0; i<maxNumConns; i++)
  {
    llConnState_t *connPtr = MAP_llDataGetConnPtr( i );

    // check if this connection is active
    if ( connPtr->activeConn )
    {
      // check if an Update Parameter control procedure is pending
      if ( (connPtr->ctrlPktInfo.ctrlPktCount > 0) &&
           (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_CONNECTION_UPDATE_IND) )
       {
         return( TRUE );
       }
    }
  }

  return( FALSE );
}
#endif // ADV_CONN_CFG | INIT_CFG

/*******************************************************************************
 * @fn          llRemoveFromFeatureSet
 *
 * @brief       Removes a feature from the feature set.
 *              Only in use on init device, otherwise, use
 *              LL_EXT_SetLocalSupportedFeatures
 *
 * input parameters
 *
 * @param       byte - the place in the feature set
 * @param       feature - Feature to remove from the device feature set..
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llRemoveFromFeatureSet( uint8 byte, uint8 feature )
{
  // validate byte is in the featureSet array range
  // validate feature is not 0 and a power of 2 (only one bit is set)
  if ( (byte < LL_MAX_FEATURE_SET_SIZE) &&
       (feature != LL_FEATURE_NONE) && ((feature & (feature-1)) == 0) )
  {
    deviceFeatureSet.featureSet[byte] &= ~feature;
  }
}

/*******************************************************************************
 * @fn          llInitFeatureSet
 *
 * @brief       This function initializes this device's default Feature Set.
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
void llInitFeatureSet( void )
{
  // clear Feature Set data for this device
  for (uint8 i=0; i<LL_MAX_FEATURE_SET_SIZE; i++)
  {
    // set all feature set bits to "unused"
    deviceFeatureSet.featureSet[i] = LL_FEATURE_NONE;
  }

  // set Feature Set ignored bits mask
  // Note: Please see BLE V5.0, Vol.6, Part B, Section 4.6, Table 4.4
  deviceFeatureSet.featureSetMask[0] = LL_FEATURE_MASK_BYTE0;
  deviceFeatureSet.featureSetMask[1] = LL_FEATURE_MASK_BYTE1;
  deviceFeatureSet.featureSetMask[2] = LL_FEATURE_MASK_BYTE2;
  deviceFeatureSet.featureSetMask[3] = LL_FEATURE_MASK_BYTE3;
  deviceFeatureSet.featureSetMask[4] = LL_FEATURE_MASK_BYTE4;
  deviceFeatureSet.featureSetMask[5] = LL_FEATURE_MASK_BYTE5;
  deviceFeatureSet.featureSetMask[6] = LL_FEATURE_MASK_BYTE6;
  deviceFeatureSet.featureSetMask[7] = LL_FEATURE_MASK_BYTE7;

  // set those features supported by this controller
  deviceFeatureSet.featureSet[0] |= (uint8)LL_FEATURE_ENCRYPTION;

  // ALT: Reject Indication Extended IS OPTIONAL FOR ENCRYPTION.

  // V4.x, Vol. 6, Part B, Section 4.6 states that Reject Indication Extended
  // shall be supported if Connection Parameters Request Procedure is supported.
  deviceFeatureSet.featureSet[0] |= (uint8)LL_FEATURE_CONN_PARAMS_REQ;
  deviceFeatureSet.featureSet[0] |= (uint8)LL_FEATURE_REJECT_EXT_IND;
  deviceFeatureSet.featureSet[0] |= (uint8)LL_FEATURE_SLV_FEATURES_EXCHANGE;
  deviceFeatureSet.featureSet[0] |= (uint8)LL_FEATURE_PING;
  deviceFeatureSet.featureSet[0] |= (uint8)LL_FEATURE_DATA_PACKET_LENGTH_EXTENSION;
  deviceFeatureSet.featureSet[0] |= (uint8)LL_FEATURE_PRIVACY;
  deviceFeatureSet.featureSet[0] |= (uint8)LL_FEATURE_EXTENDED_SCANNER_FILTER_POLICIES;

  // V4.x, Vol. 6, Part B, Section 4.6 states that Reject Indication Extended
  // shall be supported if the PHY Change Procedure is supported.
  deviceFeatureSet.featureSet[1] |= (uint8)LL_FEATURE_2M_PHY;
  // Note: Stable Modulation Index not supported!
  //deviceFeatureSet.featureSet[1] |= (uint8)LL_FEATURE_STABLE_MODULATION_INDEX;

  deviceFeatureSet.featureSet[1] |= (uint8)LL_FEATURE_CODED_PHY;

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  deviceFeatureSet.featureSet[1] |= (uint8)LL_FEATURE_CHAN_ALGO_2;
#endif // (ADV_CONN_CFG | INIT_CFG)

  // add AE to supported feature set. if USE_AE is not defined, this feature support will be turned off later in rom_init.c
#if defined(CTRL_V50_CONFIG) && (CTRL_V50_CONFIG & AE_CFG)
  deviceFeatureSet.featureSet[1] |= (uint8)LL_FEATURE_EXTENDED_ADVERTISING;
#endif // AE_CFG

  // set here and omit those features later in rom_init.c depends on defines
  deviceFeatureSet.featureSet[1] |= (uint8)LL_FEATURE_PERIODIC_ADVERTISING;
  deviceFeatureSet.featureSet[3] |= (uint8)LL_FEATURE_REMOTE_PUBLIC_KEY_VALIDATION;

  // Set the CS Feature bit
  MAP_llCsSetFeatureBit();

  return;
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * @fn          llGenerateCRC
 *
 * @brief       This function is used to generate an initial 24-bit CRC value.

 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      A 24-bit CRC value.
 */
uint32 llGenerateCRC( void )
{
  uint32 crcInit = 0;

  // generate 24 bit CRC init value
  ((uint8 *)&crcInit)[0] = MAP_LL_ENC_GeneratePseudoRandNum();
  ((uint8 *)&crcInit)[1] = MAP_LL_ENC_GeneratePseudoRandNum();
  ((uint8 *)&crcInit)[2] = MAP_LL_ENC_GeneratePseudoRandNum();

  return( crcInit );
}
#endif // INIT_CFG


/*******************************************************************************
 * @fn          llEventInRange
 *
 * @brief       This function determines if a connection update event count is
 *              between the current event count (exclusive) and the next event
 *              count (inclusive), given 16 bit event counters are used. It is
 *              used when the next active event count is beyond the current
 *              event count plus one (possible due to peripheral latency) but an
 *              update event is pending before that and must be handled.
 *
 * input parameters
 *
 * @param       curEvent    - The current event that just completed.
 * @param       nextEvent   - The next active event based on peripheral latency.
 * @param       updateEvent - The event when an update needs to take place.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Indicates whether is between current event and next event:
 *              TRUE:  current event < update event <= next event
 *              FALSE: update event > next event
 */
uint8 llEventInRange( uint16 curEvent,
                      uint16 nextEvent,
                      uint16 updateEvent )
{
  // first check if we don't need to worry about wrapping
  if ( nextEvent > curEvent )
  {
    // we don't, so check if the instant is between curEvent and nextEvent
    // Note: Need to check that the instant is greater than the curEvent in
    //       case the instant has wrapped!
    return( (uint8) ((updateEvent <= nextEvent) && (updateEvent > curEvent)) );
  }
  else // we have to handle wrap
  {
    // so compare either the upper range exclusive or the lower range inclusive
    return( (uint8)((updateEvent > curEvent) || (updateEvent <= nextEvent)) );
  }
}


/*******************************************************************************
 * @fn          llEventDelta
 *
 * @brief       This function returns the difference between two 16 bit event
 *              counters. It is used to find the number of events between some
 *              future event (event A) and the current event (event B).
 *
 * input parameters
 *
 * @param       eventA - The first event count.
 * @param       eventB - The second event count.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The absolute number of events: | eventA - eventB |
 */
uint16 llEventDelta( uint16 eventA,
                     uint16 eventB )
{
  // first check if we don't need to worry about wrapping
  if ( eventA >= eventB )
  {
    // we don't, so take a straight difference
    return( eventA - eventB );
  }
  else // we have to handle wrap
  {
    // so combine the two ranges
    return( (uint16)((0x10000 - eventB) + eventA) );
  }
}


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llConvertLstoToEvent
 *
 * @brief       This function is used to convert the LSTO, which is given
 *              in integer multiples of 10ms from 100ms to 32s, into an
 *              expiration event count.
 *
 *              Note: It is assumed here that connInterval and connTimeout have
 *                    already been converted into units of 625us.
 *
 *              Side Effects:
 *              expirationValue - The connection expiration value is updated.
 *
 * input parameters
 *
 * @param       connPtr    - Pointer to the current connection.
 * @param       connParams - Pointer to the connection parameters.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llConvertLstoToEvent( llConnState_t *connPtr,
                           connParam_t   *connParams )
{
  // find the number of connection intervals in the LSTO
  connPtr->expirationValue = connParams->connTimeout /
                             connParams->connInterval;

  // take the ceiling
  if ( connParams->connTimeout % connParams->connInterval )
  {
    // there's overage, so bump the count
    connPtr->expirationValue++;
  }

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llConvertCtrlProcTimeoutToEvent
 *
 * @brief       This function is used to convert the control procedure timeout,
 *              which is a constant 40s, into an expiration event count. This
 *              routine is called when a new connection is formed, or when the
 *              connection update parameters have been changed.
 *
 *              Note: It is assumed here that connInterval has already been
 *                    converted into units of 625us.
 *
 *              Note: Based on Core V4.0, the termination control procedure
 *                    timeout is based on the Connection Supervision Timeout
 *                    value.
 *
 *              Side Effects:
 *              ctrlPktInfo.ctrlTimeoutVal - Updated with number of events to
 *                                           expiration.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llConvertCtrlProcTimeoutToEvent( llConnState_t *connPtr )
{
  // Find the number of connection intervals in the control procedure timeout
  // Note: 1600 coarse ticks per second
  connPtr->ctrlPktInfo.ctrlTimeoutVal = (uint16)LL_MAX_CTRL_PROC_TIMEOUT /
                                        connPtr->curParam.connInterval;

  // Take the ceiling
  if ( (uint16)LL_MAX_CTRL_PROC_TIMEOUT % connPtr->curParam.connInterval )
  {
    // There's overage, so bump the count
    connPtr->ctrlPktInfo.ctrlTimeoutVal++;
  }

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llProcessTxData
 *
 * @brief       This function is used to complete sent Tx data packets based
 *              on how the user specified the Number of Completed Packets
 *              Limit using LL_EXT_NumComplPktsLimit.
 *
 * input parameters
 *

 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llProcessTxData( void )
{
  // get connection information
  llConnState_t *connPtr = MAP_llDataGetConnPtr( llConns.currentConn );

  if (NULL != connPtr)
  {
  RCL_Buffer_TxBuffer *pDataEntry = NULL;

  // free all Finished entries
  while ( (pDataEntry=RCL_TxBuffer_head(&((txDataQ_t *)(connPtr->pTxDataEntryQ))->llDataBuffers)) != NULL )
  {
    // check if the data entry is not finished
    if ( pDataEntry->state != RCL_BufferStateFinished )
    {
      // we're done since the data entries at this point forward are
      // still pending
      break;
    }
    // only count if this is the last data packet
    if ( (*((uint8 *)(pDataEntry->data + (pDataEntry->numPad -1))) != LL_DATA_PDU_HDR_LLID_CONTROL_PKT) &&
         (CHECK_LAST_PKT(pDataEntry->pad0)))
    {
      // bump the number of buffers freed
      numComplPkts++;

      // bump the number of available Tx buffers
      // Note: Critical section not needed here as this routine is only called
      //       either from an ISR or during post-processing.
      numTxDataBufs++;
    }

    // TX entry at head of internal connection queue is done, so free it
    // remove the buffer from the LL list
    pDataEntry=RCL_TxBuffer_get(&((txDataQ_t *)(connPtr->pTxDataEntryQ))->llDataBuffers);
    // free the TX data buffer
    MAP_osal_bm_free( (void *)pDataEntry );
  }

  // check if we completed any packets
  // The Number of Completed Packets event is sent when the number of completed
  // packets is equal to or greater than the user specified limit, which can
  // range from 1 to the LL_MAX_NUM_DATA_BUFFERS (default is one). If the
  // number of completed packets is less than the limit, then this event is only
  // sent if the user indicated that this event to be sent at the end of the
  // connection event.
  // Note: Spec indicates that while the Controller has HCI data packets in its
  //       buffer, it must keep sending the Number Of Completed Packets event
  //       to the Host at least periodically, until it finally reports that all
  //       the pending ACL Data Packets have been transmitted or flushed.
  //       However, this can potentially waste a lot of time sending events
  //       with a number of completed packets set to zero, so for now, this
  //       will not be supported.
  if ( (numComplPkts > 0)                   &&
       ((numComplPkts >= numComplPktsLimit) || numComplPktsFlush) )
  {
    uint16 connId = connPtr->connId;
    uint16 numCompletedPackets = numComplPkts;

      // and send credits to the Host
      MAP_HCI_NumOfCompletedPacketsEvent( 1,
                                          &connId,
                                          &numCompletedPackets );

      // clear count
      numComplPkts = 0;
  }
  }
  return;
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llWriteTxData
 *
 * @brief       This function is used to place Host/HCI data in the TX FIFO for
 *              transmission. If there is no reason to stall transmission (due
 *              to encryption control procedures, physical flow control, etc.),
 *              then the data is placed in the TX FIFO for transmission. If
 *              encryption is enabled, the data packet is encrypted in place.
 *
 * input parameters
 *
 * @param       *connPtr   - A pointer to the current connection.
 * @param       *pBuf      - A pointer to the data buffer to transmit.
 * @param       payloadLen - The number of bytes to transmit on this connection.
 * @param       fragFlag   - LL_DATA_FIRST_PKT_HOST_TO_CTRL indicates buffer is
 *                           the start of a Host-to-Controller packet.
 *                           LL_DATA_CONTINUATION_PKT: Indicates buffer is a
 *                             continuation of a Host-to-Controller packet.
 * @param       lastPkt    - Flag to indicate if this is the last fragmented
 *                           packet.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS, LL_STATUS_ERROR_OUT_OF_TX_MEM,
 *              LL_STATUS_WARNING_TX_DISABLED
 */
uint8 llWriteTxData ( llConnState_t *connPtr,
                      uint8         *pBuf,
                      uint8          dataLen,
                      uint8          fragFlag,
                      uint8          lastPkt )
{
  uint8        pktHdr;
  RCL_Buffer_TxBuffer *dataEntry;
  uint8       *pData;

  // point to start of data entry
  uint8       *pDataLen;
  dataEntry = MAP_osal_bm_adjust_header( pBuf, ((sizeof(RCL_Buffer_TxBuffer)) + RCL_BUFFER_MAX_HEADER_PAD_BYTES + LL_PKT_HDR_LEN));

  // make sure the header bytes are clear before we transmit the packet
  MAP_osal_memset(dataEntry, 0x00, (sizeof(RCL_Buffer_TxBuffer)) + RCL_BUFFER_MAX_HEADER_PAD_BYTES + LL_PKT_HDR_LEN);

  // point to start of packet, including header
  pData = dataEntry->data + RCL_BUFFER_MAX_HEADER_PAD_BYTES;

  // set LLID fragmentation flag
  // Note: NESN=SN=MD=0 and is handled by RF.
  pktHdr = (fragFlag==LL_DATA_FIRST_PKT_HOST_TO_CTRL) ?
            LL_DATA_PDU_HDR_LLID_DATA_PKT_FIRST       : // first pkt
            LL_DATA_PDU_HDR_LLID_DATA_PKT_NEXT;         // continuation pkt

  // put LLID in the packet
  // Note: Skip data length - derived by the FW from the data entry length.
  *pData++ = pktHdr;

  // yes, so initialize the data entry
  // Note: Exclude header and MIC length until we're sure we can send this data
  //       entry, and that encryption is eabled.
  // save a pointer to the header payload length location
  // this is needed in case encryption will be done in this function
  // in case of encryption, the LL_PKT_MIC_LEN will be added
  pDataLen = pData;
  // update the payload length value in the header
  *pData++ = dataLen;
  dataEntry->state = RCL_BufferStatePending;
  dataEntry->numPad = RCL_BUFFER_MAX_PAD_BYTES;
  // it will set the packet as last packet only if lastPkt set to 0x10
  SET_LAST_PKT(dataEntry->pad0, lastPkt);
  dataEntry->length = dataLen + dataEntry->numPad + LL_PKT_HDR_LEN + 1;

  // check if data output is allowed
  // Note: Data output can be disabled so that the TX FIFO can empty. This is
  //       needed for encryption pause, for example.
  if ( connPtr->txDataEnabled == TRUE )
  {
    // adjust length for header
    // check if packet needs to be encrypted
    if ( connPtr->encEnabled )
    {
      // add the LL_PKT_MIC_LEN to the header payload length
      *pDataLen += LL_PKT_MIC_LEN;

      // adjust length for encryption
      dataEntry->length += LL_PKT_MIC_LEN;

      // and encrypt
      MAP_LL_ENC_Encrypt( connPtr,
                          pktHdr,
                          dataLen,
                          pData );
    }

    // queue it on connection TX list and queue for RF
    MAP_llAddTxDataEntry( connPtr->pTxDataEntryQ,
                          dataEntry );
  }
  else // TX is disabled
  {
    // have to wait on this data, so save it on another internal
    // linked list until Tx data is enabled again
    // we call directly to the List function because the RCL_TxBuffer_put will also add it to the RF fifo.
    List_put(&((txDataQ_t *)(connPtr->pTxDataEntryQ))->tmpDataBuffers,(void *)dataEntry);
  }

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llSendReject
 *
 * @brief       This function is called to send a Reject Indication or a
                Reject Indication Extended.
 *
 *              Note: Even if the Reject Indication Extended feature is
 *                    supported by this device, it is possible the connection
 *                    doesn't support the feature.
 *
 * input parameters
 *
 * @param       rejectOpcode - The control opcode that is being rejected.
 * @param       errorCode    - The error status code for the rejection.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSendReject( llConnState_t *connPtr,
                   uint8  rejectOpcode,
                   uint8              errorCode )
{
  // check if the Reject Indication Extended feature is supported
  // Note: While this device necessarily supports this feature (as the
  //       code is conditionally included), it is possible the connection
  //       does not support this feature after a feature exchange. Although
  //       the peer shouldn't send a control packet for a feature they
  //       indicated they did not support, it is possible Tester software
  //       such as Codenomics might. In this case, Reject Indication can be
  //       used instead of Reject Indication Extended.
  if ( connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_REJECT_EXT_IND )
  {
    // save opcode and error code
    connPtr->rejectIndExt.rejectOpcode = rejectOpcode;
    connPtr->rejectIndExt.errorCode    = errorCode;

    // setup/send a Reject Indication Extended
    // Note: This control packet is not queued, but merely sent.
    if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_REJECT_EXT_IND) == FALSE )
    {
      // unable to malloc a packet!
      (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
    }
  }
  else // the Reject Indication Extended is not supported
  {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
    // so use the Reject Indication instead
    // ROM WORKAROUND - The reject routine should no longer be specific to enc,
    //                  but trying to avoid patching llSetupRejectInd just so
    //                  it uses a common variable instead of the one from enc.
    connPtr->encInfo.encRejectErrCode = errorCode;

    // setup/send a Reject Indication
    // Note: This control packet is not queued, but merely sent.
    if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_REJECT_IND) == FALSE )
    {
      // unable to malloc a packet!
      (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );
    }
#else // !ADV_CONN_CFG
    // this configuration is bad, so disconnect
    MAP_llConnTerminate( connPtr, LL_DISCONNECT_UNSUPPORTED_REMOTE_FEATURE );
#endif // ADV_CONN_CFG
  }

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llCBTimer_AptoExpiredCback
 *
 * @brief       This CBTimer callback function is used to process an APTO timer
 *              expiration. The timer is setup to expire at 1/2 APTO, and either
 *              a PING control procedure is started or an APTO Expired event
 *              is returned to the Host.
 *
 *              To make use of only one timer to handle both the time to send
 *              a Ping Request, and an APTO expiration (to send the Host an
 *              expiration event), and flag to track the timer expirations. If
 *              the flag is zero, then this is the first expiration and a Ping
 *              Request can be sent, and the flag is toggled to one. If the flag
 *              is one, then this is the second expiration and an event is sent
 *              to the Host. Any time a valid MIC packet is received, the flag
 *              is forced back to zero. Since a Ping Request can still be
 *              pending (i.e. a valid MIC packet was received, but not the Ping
 *              Response), a flag is checked to prevent the sending of multiple
 *              Ping Request control packets.
 *
 * input parameters
 *
 * @param       uint8 *pData
 *
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llCBTimer_AptoExpiredCback( uint8 *pData )
{
  halIntState_t  cs;
  llConnState_t *connPtr = (llConnState_t *)pData;

  if ( MAP_LL_ConnActive( connPtr->connId ) != LL_STATUS_SUCCESS )
  {
    return;
  }

  HAL_ENTER_CRITICAL_SECTION(cs);

  // check if this is the second expiration without a valid packet
  if ( connPtr->numAptoExp == 1 )
  {
    // it is, so notify the Host
    MAP_LL_AuthPayloadTimeoutExpiredCback( connPtr->connId );

    // Sanity Check:
    // If we've already expired once, then a PING was already sent. Either
    // we should have gotten a ping response, in which case this shouldn't be
    // the second expiration, or the ping request must still be pending.
    LL_ASSERT( connPtr->pingReqPending == TRUE );
  }
  else // this is the first expiration without a valid packet
  {
#ifdef LL_TEST_MODE
      switch( llTestMode.testCase )
      {
        case LL_TEST_MODE_TP_SEC_MAS_BV_08:
          HAL_EXIT_CRITICAL_SECTION(cs);
          return;

          // Note: Unreachable statement generates compiler warning!
          //break;

        case LL_TEST_MODE_TP_SEC_SLA_BV_08:
          HAL_EXIT_CRITICAL_SECTION(cs);
          return;

          // Note: Unreachable statement generates compiler warning!
          //break;

        default:
          break;
      }
#endif // LL_TEST_MODE

    // check if we need to send a ping request
    // Note: The timer may be restarted by a valid MIC that wasn't the ping
    //       response, so it is possible that ping can still be pending.
    if ( connPtr->pingReqPending == FALSE )
    {
      // set flag to indicate the ping is pending
      connPtr->pingReqPending = TRUE;

      // setup an encryption control procedure
      MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_PING_REQ );
    }
  }

  // either way, toggle the number of expirations and restart the timer
  connPtr->numAptoExp ^= 1;

  // re-start the APTO timer
  MAP_osal_CbTimerStart( MAP_llCBTimer_AptoExpiredCback,
                         (uint8 *)connPtr,
                         (connPtr->aptoValue / 2),
                         &connPtr->aptoTimerId );

  HAL_EXIT_CRITICAL_SECTION(cs);

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llFragmentPDU
 *
 * @brief       This function is used to fragment a PDU (i.e. SDU Segment) into
 *              OTA sized fragments.
 *
 *              Note: Fragmentation is only enabled when the MTU is greater
 *                    than 27 bytes.
 *
 *              Note: Queuing of segments is currently not supported. Either
 *                    PDU successfully fragments, or it is tossed.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the connection.
 * @param       pBuf    - Pointer to the PDU payload buffer.
 * @param       len     - Length of the PDU.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      llStatus_t.
 */
llStatus_t llFragmentPDU( llConnState_t *connPtr,
                          uint8         *pBuf,
                          uint16         len )
{
  uint8 *pData;
  uint8  size;
  uint8  cnt = 0;

  while (len > 0)
  {
    size = (len > connPtr->lenInfo.connActualMaxTxOctets) ?
            connPtr->lenInfo.connActualMaxTxOctets        :
            len;

    if ( (pData = MAP_HCI_bm_alloc(size)) != NULL )
    {
      MAP_osal_memcpy( pData, pBuf, size );

      // adjust offset to PDU
      pBuf += size;

      // adjust remaining length
      len -= size;

      // queue the packet
      MAP_llWriteTxData( connPtr,
                         pData,
                         size,
                         ((cnt++==0)                     ?
                          LL_DATA_FIRST_PKT_HOST_TO_CTRL :
                          LL_DATA_CONTINUATION_PKT),
                         ((len>0)                        ?
                          DATA_ENTRY_NOT_LAST_PACKET     :
                          DATA_ENTRY_LAST_PACKET) );
    }
    else // malloc failed
    {
      // report failure to Host
      MAP_HCI_HardwareErrorEvent( HW_FAIL_OUT_OF_MEMORY );

      return( LL_STATUS_ERROR_OUT_OF_TX_MEM );
    }
  }

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llAllocatePDU
 *
 * @brief       This function is used to allocate complete PDU
 *
 * input parameters
 *
 * @param       dataPtr - Pointer to the ext conn.
 * @param       len    - Length of the last fragment.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      status.
 */
uint8 llAllocatePDU( llConnState_t *connPtr, uint16  len)
{
  uint8 status = FALSE;

  // check the size does not exceed this device's MTU
  if ( connPtr->rxData.pduSize > maximumPduSize )
  {
    // report failure to Host
    LL_DataLenExceedEventCback( HW_FAIL_PDU_SIZE_EXCEEDS_MTU, (uint16)llConns.currentConn, connPtr->rxData.pduCid );
  }
  else if ( len > connPtr->rxData.pduSize )
  {
    // report failure to Host
    LL_DataLenExceedEventCback( HW_FAIL_PKT_LEN_EXCEEDS_PDU_SIZE, (uint16)llConns.currentConn, connPtr->rxData.pduCid );
  }
  else
  {
    // allocate the PDU, based on specified size
    connPtr->rxData.pEntry = MAP_LL_RX_bm_alloc( connPtr->rxData.pduSize );
    if ( connPtr->rxData.pEntry != NULL )
    {
      status = TRUE;
    }
    else
    {
      // report failure to Host
      MAP_HCI_HardwareErrorEvent( HW_FAIL_OUT_OF_MEMORY );
    }
  }
  return status;
}

/*******************************************************************************
 * @fn          llCombinePDU
 *
 * @brief       This function is used to combine received OTA fragments into a
 *              final PDU (i.e. SDU Segment).
 *
 *              Note: Fragmentation is only enabled when the MTU is greater
 *                    than 27 bytes.
 *
 *              Note: The size of the PDU is determined by the first two bytes
 *                    of the Start packet. This is the L2CAP header, which is
 *                    normally not accessed by this layer.
 *
 * input parameters
 *
 * @param       connId - The connection handle ID.
 * @param       pBuf   - Pointer to the fragment payload buffer.
 * @param       len    - Length of the fragment.
 * @param       llid   - Boundary flag used for fragmentation.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llCombinePDU( uint16  connId,
                   uint8  *pBuf,
                   uint16  len,
                   uint8   llid )
{
  llConnState_t *connPtr;
  uint8 status = TRUE;

  // get the connection info based on the connection ID
  connPtr = MAP_llDataGetConnPtr( connId );

  // check the expected fragment type
  switch( connPtr->rxData.state )
  {
    case LL_DATA_CONTINUATION_PKT:
      if ( llid == LL_DATA_FIRST_PKT_CTRL_TO_HOST )
      {
        // unexpected packet received, so restart the PDU
        connPtr->rxData.pduSize = 0;
        connPtr->rxData.pduCid  = 0;
        connPtr->rxData.size    = 0;
        connPtr->rxData.state   = LL_DATA_FIRST_PKT_CTRL_TO_HOST;

        // previous PDU being built must be freed
        MAP_osal_bm_free( connPtr->rxData.pEntry );

        // clear the pointer
        connPtr->rxData.pEntry = NULL;

        /* DROP THROUGH */
      }
      else // LLID = Continue
      {
        // check if this fragment exceeds allocated PDU size
        if ((connPtr->rxData.size > 1) && ((connPtr->rxData.size + len) > connPtr->rxData.pduSize ))
        {
          // received an expected continuation packet, but length exceeds the
          // L2CAP header size specified in the first packet, so discard PDU
          connPtr->rxData.pduSize = 0;
          connPtr->rxData.size    = 0;
          connPtr->rxData.state   = LL_DATA_FIRST_PKT_CTRL_TO_HOST;

          // previous PDU being built must be freed
          MAP_osal_bm_free( connPtr->rxData.pEntry );

          // clear the pointer
          connPtr->rxData.pEntry = NULL;

          // report failure to Host
          LL_DataLenExceedEventCback( HW_FAIL_PKT_LEN_EXCEEDS_PDU_SIZE,
                                      (uint16)llConns.currentConn, connPtr->rxData.pduCid );
          connPtr->rxData.pduCid  = 0;
        }
        else // len is okay or previously got only 1 byte
        {
          if ((connPtr->rxData.size == 1) && (len > 0))
          {
            connPtr->rxData.pduSize = (((pBuf[0] << 8) | (connPtr->rxData.pduSize)) + L2CAP_HDR_SIZE);
            if (len >= L2CAP_HDR_SIZE)
            {
              //extruct the CID
              connPtr->rxData.pduCid = BUILD_UINT16(pBuf[1], pBuf[2]);
            }
            status = llAllocatePDU(connPtr,len);
          }
          if ((connPtr->rxData.pEntry != NULL) && (status == TRUE))
          {
            // accumulate the data
            MAP_osal_memcpy( connPtr->rxData.pEntry + connPtr->rxData.size, pBuf, len );

            // track the accumulated amount
            connPtr->rxData.size += len;

            // check if we're done
            if ( connPtr->rxData.size == connPtr->rxData.pduSize )
            {
              // we are, so send the packet to the Host
              // Note: Check RSSI, and if valid, correct.
              MAP_LL_RxDataCompleteCback( (uint16)llConns.currentConn,
                                         connPtr->rxData.pEntry,
                                         connPtr->rxData.pduSize,
                                         llid,
                                         LL_CHECK_LAST_RSSI(connPtr->lastRssi) );

              // reset PDU size
              connPtr->rxData.pduSize = 0;
              connPtr->rxData.pduCid  = 0;
              connPtr->rxData.size    = 0;
              connPtr->rxData.state   = LL_DATA_FIRST_PKT_CTRL_TO_HOST;
            }
          }
        }

        break;
      }

    case LL_DATA_FIRST_PKT_CTRL_TO_HOST:
      if ( llid == LL_DATA_FIRST_PKT_CTRL_TO_HOST )
      {
        // check packet length
        if ( len == 0)
        {
          // report failure to Host
          MAP_HCI_HardwareErrorEvent( HW_FAIL_INADEQUATE_PKT_LEN );
        }
        else if (len == 1)
        {
          // track the accumulated amount
          connPtr->rxData.size = 1;
          // peek the first byte out of two bytes that represent the total size
          connPtr->rxData.pduSize = pBuf[0];
          // wait for continue packet
          connPtr->rxData.state = LL_DATA_CONTINUATION_PKT;
        }
        else // minimum packet length met
        {
          // peek the L2CAP header to get total size of PDU (i.e. segment)
          // Note: L2CAP Length excludes the two bytes of length and two of CID.
          // Note: Strictly speaking, this should not be done by the HCI!
          connPtr->rxData.pduSize = (((pBuf[1] << 8) | (pBuf[0])) + L2CAP_HDR_SIZE);
          if (len >= L2CAP_HDR_SIZE)
          {
            //extruct the CID
            connPtr->rxData.pduCid = BUILD_UINT16(pBuf[2], pBuf[3]);
          }
          // allocate the PDU, based on specified size
          status = llAllocatePDU(connPtr,len);

          // check if we have the memory
          if ((connPtr->rxData.pEntry != NULL) && (status == TRUE))
          {
            // accumulate the data
            MAP_osal_memcpy( connPtr->rxData.pEntry, pBuf, len );

            // check if we're done
            if ( len == connPtr->rxData.pduSize )
            {
              // we are, so send the packet to the Host
              // Note: Check RSSI, and if valid, correct.
              MAP_LL_RxDataCompleteCback( (uint16)llConns.currentConn,
                                          connPtr->rxData.pEntry,
                                          connPtr->rxData.pduSize,
                                          llid,
                                          LL_CHECK_LAST_RSSI(connPtr->lastRssi) );
              connPtr->rxData.pduCid = 0;
            }
            else // not done yet
            {
              // track the accumulated amount
              connPtr->rxData.size += len;

              // wait for continue packet
              connPtr->rxData.state = LL_DATA_CONTINUATION_PKT;
            }
          }
        }
      }
      break;

    default:
      break;
  }

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
/*******************************************************************************
 * @fn          llAlignToNextEvent
 *
 * @brief       This API is used to realign to the next connection event when
 *              peripheral latency is in effect. This is mainly in support of fast
 *              Tx.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the connection.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llAlignToNextEvent( llConnState_t *connPtr )
{
  // first check that this connection is a Peripheral using SL with fastTx
  if ( (connPtr->llTask->taskID == LL_TASK_ID_PERIPHERAL)          &&
       (connPtr->peripheralLatency != 0)                           &&
       (fastTxRespTime == LL_EXT_ENABLE_FAST_TX_RESP_TIME) )
  {
    halIntState_t cs;
    uint32        curTime;
    RCL_Command *connCmd = (RCL_Command *)connPtr->llTask->command;
    HAL_ENTER_CRITICAL_SECTION(cs);

    // update current time, and adjust for one event plus some pad
    // Note: If we are just before the next event, then there's no
    //       point for an event re-alignment.
    curTime = MAP_llGetCurrentTime()           +
              ((connPtr->curParam.connInterval +
                LL_FAST_TX_TICKS_TO_EVT_PAD) * RAT_TICKS_IN_625US);

    // check if this connection is the current connection
    if ( connPtr->connId == llConns.currentConn )
    {
      // check if there's enough time before the next event for re-alignment
      // Note: Returns TRUE when first parameter is greater than second.
      if ( MAP_llTimeCompare(connCmd->timing.absStartTime, curTime) )
      {
        uint32      time;
        uint16      numEventsPast;
        taskInfo_t *curTask = MAP_llGetCurrentTask();

        // there is, so stop the pending Peripheral radio operation
        // Note: The Stop will generate an interrupt. To prevent possible
        //       erroneous behavior in the ISR, the interrupt will pend until
        //       this routine completes.
        // Note: If a combination state is running, then we won't get here as
        //       the LL state won't be Central. Thus, combo roles and fast Tx
        //       don't work together.
        RCL_Command_stop(connCmd, RCL_StopType_DescheduleOnly);
        // get the time delta between current time (CT) and last start time
        // Note: The assumption here is that CT is always ahead of the
        //       lastStartTime because startTime is always ahead of
        //       lastStartTime and CT is at this point behind startTime.
        //       If the call to this function happens during a radio event
        //       or between the end of a radio event and the start of LL
        //       processing, then CT will still be greater than startTime
        //       (because startTime has not yet been updated). If the call
        //       is made after LL processing, then startTime will be updated
        //       to the next radio event (i.e. ahead of CT), and lastStartTime
        //       will be the previous radio event (i.e. behind CT). So if CT
        //       is behind startTime, then it is always ahead of
        //       lastsSartTime.
        // Note: The only time CT could be equal to lastStartTime is
        //       if radio event ended in less than one software tick
        //       and this routine is called before the rollover. This
        //       should never happen given the current radio time and
        //       post-processing time, but even if it does, this code
        //       should still be fine as the delta would be zero.
        time = MAP_llTimeDelta( curTime, curTask->lastStartTime );

        // determine the number of events past the last startTime
        numEventsPast = time / (connPtr->curParam.connInterval * RAT_TICKS_IN_625US);

        // update next event counter
        connPtr->nextEvent = connPtr->currentEvent + numEventsPast;

        // update time to next event based on last adjusted AP
        connCmd->timing.absStartTime =
          connPtr->llTask->lastStartTime +
          (((uint32)numEventsPast * (uint32)connPtr->curParam.connInterval) * RAT_TICKS_IN_625US);

        // adjust data channel
        connPtr->nextChan = connPtr->currentChan;
        MAP_llSetNextDataChan( connPtr );

        // enable RF event
        LL_rclRescheduleCommand(NULL);
      }
      // else amount of time to next event is less than CI+pad, so do nothing
    }
    else // it is not the current connection, so no halt is possible, only align
    {
      // update time to next event based on last adjusted AP
      // Note: Since we really cannot stop the current BLE task, and need to
      //       wait for it to complete, then just force this task's start time
      //       to be in the past. The scheduler will realign this connection to
      //       its next event. This does not mean this connection will
      //       necessarily be the next connection, but it will at least be
      //       realigned to its next most recent event. Larger fastTx latencies,
      //       for now, can result when there are many active connections.
      connCmd->timing.absStartTime =
      connPtr->llTask->lastStartTime;

      // Note: Do not try to reschedule as some BLE task is currently running.
    }

    HAL_EXIT_CRITICAL_SECTION(cs);
  }

  return;
}
#endif // ADV_CONN_CFG


/*******************************************************************************
 * @fn          llVerifyConnParamReqParams
 *
 * @brief       This function is used to check that the Connection Parameter
 *              Request parameters (Offset0..Offset5, Preferred Periodicity,
 *              and Reference Connection Event Count) are valid.
 *
 *              Note: It is assume that the connection intervals, peripheral latency,
 *                    and connection timeout, as well as their combination, have
 *                    already been validated.
 *
 * input parameters
 *
 * @param       minInterval     - Lower allowed connection interval.
 * @param       maxInterval     - Upper allowed connection interval.
 * @param       eventCount      - The current event count.
 * @param       periodicity     - The Preferred Periodicity.
 * @param       refConnEvtCount - The Reference Connection Event Count.
 * @param       pOffsets        - A pointer to Offset0..Offset5.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE:  Connection Parameter Request parameters are invalid.
 *              FALSE: Connection Parameter Request parameters are valid.
 */
uint8 llVerifyConnParamReqParams( uint16  minInterval,
                                  uint16  maxInterval,
                                  uint16  eventCount,
                                  uint8   periodicity,
                                  uint16  refConnEvtCount,
                                  uint16 *pOffsets )
{
  uint8 i;
  uint8 numValidOffsets = 0;

  // count the valid offsets
  for (i=0; i<MAX_NUM_OFFSETS; i++)
  {
    if ( pOffsets[i] != INVALID_OFFSET ) numValidOffsets++;
  }

  // verify offsets:
  // - no valid offset follows an invalid offset
  // - no valid offset is greater than the maxInterval
  // - no two valid offsets are the same
  for (i=0; i<numValidOffsets; i++)
  {
    // check if a good offset is followed by a bad offset
    // Note: First numValidOffsets must be valid!
    if ( pOffsets[i] == INVALID_OFFSET ) break;
    // check > max interval
    else if ( pOffsets[i] > maxInterval ) break;
    // check offsets are unique
    else
    {
      uint8 j;

      for (j=(i+1); j<numValidOffsets; j++)
      {
        // check if two offsets are the same
        if ( pOffsets[i] == pOffsets[j] ) break;
      }
    }
  }

  // check if any offsets are invalid
  if ( i != numValidOffsets ) return( TRUE );

  // check periodicity is valid
  // Note: Section 5.1.7.1 plus ESR08 Erratum #5706 imply the periodicity is
  //       only checked if an offset is valid, and that the periodity is invalid
  //       if zero when the min/max interval are not the same.
  if ( ((numValidOffsets) && (minInterval != maxInterval) &&
        ((periodicity == 0) || (periodicity > maxInterval))) ) return( TRUE );

  // check if refConnEvtCount is valid
  // Note: Section 5.1.7.1 imply the reference event count is only checked if
  //       an offset is valid.
  if ( (numValidOffsets) &&
       (MAP_llEventDelta( refConnEvtCount,
                          eventCount) >= LL_MAX_UPDATE_COUNT_RANGE) ) return( TRUE );

  return( FALSE );
}


/*******************************************************************************
 * @fn          llValidateConnParams
 *
 * @brief       This function is used to valid the connection parameter
 *              request/reply parameters. Is is used for the control packets
 *              as well as the Host's Request Reply Event.
 *
 *              Note: Assumed that all connection values are normalized
 *                    to 1.25ms.
 *
 * input parameters
 *
 * @param       connPtr         - Pointer to the connection.
 * @param       minInterval     - Lower allowed connection interval.
 * @param       maxInterval     - Upper allowed connection interval.
 * @param       connLatency     - The connection's Peripheral Latency.
 * @param       connTimeout     - The connection's Supervision Timeout.
 * @param       eventCount      - The current event count.
 * @param       periodicity     - The Preferred Periodicity.
 * @param       refConnEvtCount - The Reference Connection Event Count.
 * @param       pOffsets        - A pointer to Offset0..Offset5.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE:  Connection parameters are invalid.
 *              FALSE: Connection parameters are valid.
 */
uint8 llValidateConnParams( llConnState_t *connPtr,
                            uint16         minInterval,
                            uint16         maxInterval,
                            uint16         connLatency,
                            uint16         connTimeout,
                            uint16         eventCount,
                            uint8          periodicity,
                            uint16         refConnEvtCount,
                            uint16        *pOffsets )
{
  return ( (LL_INVALID_CONN_TIME_PARAM( minInterval,
                                        maxInterval,
                                        connLatency,
                                        connTimeout ) == TRUE)            ||
           (LL_INVALID_CONN_TIME_PARAM_COMBO( maxInterval,
                                              connLatency,
                                              connTimeout ) == TRUE)      ||
           ((connPtr->encEnabled) &&
            (LL_INVALID_APTO_COMBO( maxInterval,
                                    connLatency,
                                    (connPtr->aptoValue/10)*8 )) == TRUE) ||
           (MAP_llVerifyCodedConnInterval(connPtr, maxInterval ) == TRUE) ||
           (MAP_llVerifyConnParamReqParams( minInterval,
                                            maxInterval,
                                            eventCount,
                                            periodicity,
                                            refConnEvtCount,
                                            pOffsets ) == TRUE) );
}


/*******************************************************************************
 * @fn          llTime2Octets
 *
 * @brief       This function is used to convert Time in microseconds to number
 *              of Octets in bytes based on the specified PHY data rate. If PHY
 *              is LL_PHY_NONE, then LL_PHY_1_MBPS will be assumed. For
 *              LL_PHY_CODED, if coding scheme LL_PHY_OPT_NONE is used, then
 *              the default is LL_PHY_OPT_S2.
 *
 * input parameters
 *
 * @param       curPhy     - LL_PHY_1_MBPS, LL_PHY_2_MBPS, LL_PHY_CODED
 * @param       coding     - LL_PHY_OPT_S2, LL_PHY_OPT_S8
 * @param       time       - In microseconds.
 * @param       encEnabled - MIC_ENABLED (TRUE) or MIC_NOT_ENABLED (FALSE).
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The floor of time converted to octets.
 */
uint16 llTime2Octets( uint8  curPhy,
                      uint8  coding,
                      uint16 time,
                      uint8  encEnabled )
{
  uint8  micSize = (encEnabled)?4:0;

  switch( curPhy )
  {
    case LL_PHY_2_MBPS:
      return( ((time * 2) / 8) - (11 + micSize) );
      //break;

    case LL_PHY_CODED:
    {
      // Note: If LL_PHY_OPT_NONE is used, defaults to BLE5_CODED_S2_SCHEME.
      uint16 S = ((coding == LL_PHY_OPT_S8) ? BLE5_CODED_S8_SCHEME :
                                              BLE5_CODED_S2_SCHEME);

      // 80us (preamble) + 256us (synch) + 16us (CI) + 24us (TERM1) + N*S*8us (PDU) + S*24us (CRC) + S*3us (TERM2)
      return( ((time - (376 + (S*24) + (S*3))) / (S*8)) - (LL_PKT_HDR_LEN + micSize) );
      //break;
    }

    case LL_PHY_1_MBPS:
    default:
      return( ((time * 1) / 8) - (10 + micSize) );
      //break;
  }
}


/*******************************************************************************
 * @fn          llOctets2Time
 *
 * @brief       This function is used to convert Octets in bytes to Time in
 *              microseconds based on the specified PHY. If PHY is LL_PHY_NONE,
 *              then BLE5_1M_PHY will be assumed. For BLE5_CODED_PHY, if
 *              coding scheme BLE5_CODING_NONE is used, then the default is
 *              BLE5_CODED_S2_SCHEME.
 *
 * input parameters
 *
 * @param       curPhy     - BLE5_1M_PHY, BLE5_2M_PHY, BLE5_CODED_PHY
 * @param       coding     - BLE5_CODED_S8_DEFAULT, BLE5_CODED_S2_DEFAULT
 * @param       octets     - In bytes.
 * @param       encEnabled - MIC_ENABLED (TRUE) or MIC_NOT_ENABLED (FALSE).
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The number of octets converted to time in us.
 */
uint32 llOctets2Time( uint8  curPhy,
                      uint8  coding,
                      uint16 octets,
                      uint8  encEnabled )
{
  uint8 micSize = (encEnabled)?4:0;

  switch( curPhy )
  {
    case BLE5_2M_PHY:
      return( ((((octets + 11) + micSize) * 8) / 2) );
      //break;

    case BLE5_CODED_PHY:
    {
      // set coding scheme
      // Note: If BLE5_CODING_NONE is used, defaults to BLE5_CODED_S2_SCHEME.
      uint8 S = (coding == BLE5_CODED_S8_DEFAULT) ? BLE5_CODED_S8_SCHEME
                                                  : BLE5_CODED_S2_SCHEME;

      // find the payload time basded on the Coding scheme
      // 80us (preamble) + 256us (synch) + 16us (CI) + 24us (TERM1) + N*S*8us (PDU) + S*24us (CRC) + S*3us (TERM2)
      return( 376 + (octets + LL_PKT_HDR_LEN + micSize) * (S*8) + (S*24) + (S*3) );
      //break;
    }

    case BLE5_1M_PHY:
    default:
      return( ((((octets + 10) + micSize) * 8) / 1) );
      //break;
  }
}


/*******************************************************************************
 * @fn          llSetCodedMaxTxTime
 *
 * @brief       This function finds the value of connEffectiveMaxTxTime based
 *              on: connEffectiveMaxTxTimeUncoded, connIntervalPortionAvail,
 *              and connEffectiveMaxTxTimeAvail.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the connection.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The value of connEffectiveMaxTxTime.
 */
uint16 llSetCodedMaxTxTime( llConnState_t *connPtr )
{
   connPtr->lenInfo.connEffectiveMaxTxTimeUncoded =
     Math_MIN( connPtr->lenInfo.connMaxTxTime,
               connPtr->lenInfo.connRemoteMaxRxTime );

   connPtr->lenInfo.connIntervalPortionAvail =
     (connPtr->curParam.connInterval * LL_CONNECTION_SLOT_TIME) -
     ((2*LL_CONNECTION_T_IFS) +
         Math_MIN(connPtr->lenInfo.connEffectiveMaxRxTime,
                  ((connPtr->lenInfo.connEffectiveMaxRxOctets * 64) + 976)));

   connPtr->lenInfo.connEffectiveMaxTxTimeAvail =
       Math_MIN( connPtr->lenInfo.connEffectiveMaxTxTimeUncoded,
                 connPtr->lenInfo.connIntervalPortionAvail );

   return( Math_MAX( 2704, connPtr->lenInfo.connEffectiveMaxTxTimeAvail ) );

}


/*******************************************************************************
 * @fn          llVerifyCodedConnInterval
 *
 * @brief       This function checks if an update procedure connection interval
 *              is valid when Coded is being used with Extended Data Length.
 *
 *              Note: The values 976 and 2704 microseconds are derived from the
 *                    durations of packets with a zero octet payload and
 *                    with a 27 octet payload when sent on the LE Coded PHY
 *                    using S=8 coding.
 *
 * input parameters
 *
 * @param       connPtr      - Pointer to the connection.
 * @param       connInterval - New connection interval (in 1.25ms).
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE:  Coded Connection Interval is invalid.
 *              FALSE: Coded Connection Interval is valid based on:
 *                     Vol 6, Part B, Section 5.1.1.
 */
uint8 llVerifyCodedConnInterval( llConnState_t *connPtr,
                                 uint16         connInterval )
{
  if ( (deviceFeatureSet.featureSet[1] & LL_FEATURE_CODED_PHY)                    &&
       (connPtr->phyInfo.curPhy == LL_PHY_CODED)                                  &&
       (deviceFeatureSet.featureSet[0] & LL_FEATURE_DATA_PACKET_LENGTH_EXTENSION) )
  {
    return( (connInterval * (2*LL_CONNECTION_SLOT_TIME)) <
            ((2*LL_CONNECTION_T_IFS) +
                Math_MIN( connPtr->lenInfo.connEffectiveMaxRxTime,
                          ((connPtr->lenInfo.connEffectiveMaxRxOctets * 64) + 976)) + 2704) );
  }

  return( FALSE );
}


/*******************************************************************************
 * @fn          llGetSlowestPhy
 *
 * @brief       This function is used to find the PHY with the slowest data rate
 *              based on the preferred phys.
 *
 * input parameters
 *
 * @param       phys = Set of one or more PHYs.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The PHY with the slowest data rate, or LL_PHY_NONE.
 */
uint8 llGetSlowestPhy( uint8 phys )
{
  // find the slowest PHY be checking in order of slowest to fastest
  if ( phys == LL_PHY_NONE )
  {
    return( LL_PHY_NONE );
  }
  else if ( phys & LL_PHY_CODED )
  {
    return( LL_PHY_CODED );
  }
  else if ( phys & LL_PHY_1_MBPS )
  {
    return( LL_PHY_1_MBPS );
  }
  else if ( phys & LL_PHY_2_MBPS )
  {
    return( LL_PHY_2_MBPS );
  }
  else // phys is set to something that isn't defined
  {
    return( LL_PHY_NONE );
  }
}


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llShellSortActiveConns
 *
 * @brief       This function sorts the active connections by their start time.
 *
 *              Note: Based on Shell Sort with O(Nlogn) at best
 *
 * input parameters
 *
 * @param       activeConnsArray - List of active connection IDs.
 * @param       numActiveConns   - Number of valid entries in activeConns.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llShellSortActiveConns(uint8 *activeConnsArray, uint8 numActiveConns)
{
  uint8 gap,i,j;
  uint32 tempST,tempST1;
  uint8 tempIndex;
  for (gap = numActiveConns/2; gap > 0; gap /= 2)
  {
    for (i = gap; i < numActiveConns; i++)
    {
      RCL_Command *connCmd2 = (RCL_Command *)((llConnState_t *)(MAP_llDataGetConnPtr(activeConnsArray[i])))->llTask->command;
      tempST = connCmd2->timing.absStartTime;
      tempIndex = activeConnsArray[i];
      j = i;
      while (j >= gap)
      {
        RCL_Command *connCmdx  = (RCL_Command *)((llConnState_t *)(MAP_llDataGetConnPtr(activeConnsArray[j - gap])))->llTask->command;
        tempST1 = connCmdx->timing.absStartTime;
        if (tempST1 > tempST)
        {
          activeConnsArray[j]  = activeConnsArray [j - gap];
          j -= gap;
        }
        else
        {
          break;
        }
      }
      activeConnsArray[j] = tempIndex;
    }
  }
}

/*******************************************************************************
 * @fn          llSortActiveConns
 *
 * @brief       This function sorts the active connections by their start time.
 *
 *              Note: Based on Bubble Sort with O(n^2).
 *
 * input parameters
 *
 * @param       activeConns    - List of active connection Ids.
 * @param       numActiveConns - Number of valid entries in activeConns.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSortActiveConns( uint8 *activeConnsArray, uint8 numActiveConns )
{
  uint8 i, n, nextIndex;
  llConnState_t *connPtr1;
  llConnState_t *connPtr2;
  uint32 startTime1;
  uint32 startTime2;

  // sort the active connections by start time
  n = numActiveConns;

  do
  {
    nextIndex = 0;

    for (i=1; i<n; i++)
    {
      connPtr1 = MAP_llDataGetConnPtr( activeConnsArray[i-1] );
      connPtr2 = MAP_llDataGetConnPtr( activeConnsArray[i] );
      startTime1 = ((RCL_Command *)connPtr1->llTask->command)->timing.absStartTime;
      startTime2 = ((RCL_Command *)connPtr2->llTask->command)->timing.absStartTime;

      // TRUE: First Param GT Second Param
      if ( MAP_llTimeCompare( startTime1, startTime2 ) == TRUE )
      {
        SWAP( activeConnsArray, i-1, i );

        nextIndex = i;
      }
    }

    n = nextIndex;

  } while( n!=0 );

  return;
}

/*******************************************************************************
 * @fn          llRemoveHandoverConn
 *
 * @brief       This function will remove the connection that is currently in the
 *              middle of an handover process from the active connections array
 *
 * input parameters
 *
 * @param       activeConnsArray - List of active connection IDs.
 * @param       numActiveConns   - Number of valid entries in activeConns.
 *
 * output parameters
 *
 * @param       Connection IDs list without the handover connection
 *
 * @return      None
 */
void llRemoveHandoverConn(uint8 *activeConnsArray, uint8 numActiveConns)
{
  uint8_t i;
  uint8_t j;
  llConnState_t *connPtr;

  for (i = 0; i < maxNumConns; i++)
  {
    connPtr = MAP_llDataGetConnPtr(activeConnsArray[i]);

    if ( (connPtr != NULL) &&
         (connPtr->activeConn == TRUE) &&
         (llIsHandoverInProgress(connPtr) == TRUE) )
    {
      for (j = i; j < maxNumConns-1; j++)
      {
        activeConnsArray[j] = activeConnsArray[j+1];
      }
      // Set the last index as inactive as well
      activeConnsArray[maxNumConns-1] = LL_INACTIVE_CONNECTIONS;
      break;
    }
  }
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llRealignConn
 *
 * @brief       This function is used to update a connection that missed a
 *              start time in the past, and update it to a start time in the
 *              future.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to connection to be realigned.
 * @param       curTime - The current time (padded).
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llRealignConn( llConnState_t *connPtr, uint32 curTime )
{
  uint32 time;
  uint32 timeToNextEvt;
  uint16 numEventsPast = 0;
  RCL_Command *connCmd  = ((RCL_Command *)connPtr->llTask->command);
  // check if the connection start time is in the past
  // FALSE: Second parameter is GE to first parameter; ST is in past.
  if ( MAP_llTimeCompare(connCmd->timing.absStartTime, curTime) == FALSE )
  {
    // get the time delta between current time (CT) and last start time
    time = MAP_llTimeDelta( curTime, connPtr->llTask->lastStartTime );

    // check if an update parameter control procedure will or has occurred
    if ( connPtr->pendingParamUpdate )
    {
      uint32 numPreInstantEvts;
      uint32 numPostInstantEvts;
      uint32 preInstantTime;
      uint32 postInstantTime;

      // determine if CT > UE

      // count the number of events from CE to UE
      numPreInstantEvts = MAP_llEventDelta( connPtr->paramUpdateEvent-1,
                                            connPtr->currentEvent );

      // convert number of preInstant events to time
      preInstantTime = (uint32)numPreInstantEvts *
                       ((connPtr->pendingParamUpdate == PARAM_UPDATE_APPLIED) ?
                        (uint32)connPtr->prevConnInterval                     :
                        (uint32)connPtr->curParam.connInterval);

      // compare preInstantTime to CT
      // TRUE: When first parameter is greater than second.
      if ( MAP_llTimeCompare( curTime,
                              connPtr->llTask->lastStartTime + (preInstantTime * RAT_TICKS_IN_625US) ) )
      {
        uint32 remTime;

        // CT > UE-1

        numPreInstantEvts++;

        // convert number of preInstant events to time
        preInstantTime += ((connPtr->pendingParamUpdate == PARAM_UPDATE_APPLIED) ?
                           (uint32)connPtr->prevConnInterval                     :
                           (uint32)connPtr->curParam.connInterval);

        numPostInstantEvts = 0;
        postInstantTime    = 0;

        if ( MAP_llTimeCompare( curTime,
                                connPtr->llTask->lastStartTime + (preInstantTime * RAT_TICKS_IN_625US) ) )
        {
          // CT > UE

          // find the remaining time CT-((UE-CE)*CIold)
          remTime = time - (preInstantTime * RAT_TICKS_IN_625US);

          // convert remaining time into events based on CInew
          numPostInstantEvts  =  remTime / (connPtr->paramUpdate.connInterval * RAT_TICKS_IN_625US);
          numPostInstantEvts += (remTime % (connPtr->paramUpdate.connInterval * RAT_TICKS_IN_625US))?1:0;

          // convert number of preInstant events to time
          postInstantTime = (uint32)numPostInstantEvts *
                            (uint32)connPtr->paramUpdate.connInterval;
        }

        // accumulate the total number of events past
        numEventsPast = numPreInstantEvts + numPostInstantEvts;

        // update next event counter
        connPtr->nextEvent = connPtr->currentEvent + numEventsPast;

        // find the time to next event
        timeToNextEvt = preInstantTime                         +
                        (uint32)connPtr->paramUpdate.winOffset +
                        postInstantTime;

        // check if we need to apply the update parameters
        // Note: If firstPacket is false, then pendingUpdateParam is true.
        if ( connPtr->pendingParamUpdate == PARAM_UPDATE_PENDING )
        {
          llApplyParamUpdate(connPtr);
        }
      }
      else // CT <= UE
      {
        // determine the number of events past the lastStartTime
        numEventsPast  =  time / (connPtr->curParam.connInterval * RAT_TICKS_IN_625US);
        numEventsPast += (time % (connPtr->curParam.connInterval * RAT_TICKS_IN_625US))?1:0;

        // update next event counter
        connPtr->nextEvent = connPtr->currentEvent + numEventsPast;

        // find the time to next event
        timeToNextEvt = (uint32)numEventsPast * (uint32)connPtr->curParam.connInterval;

        // Apply the update connection, In case the event before the instant missed and
        // we are towards the instant and the update has not been applied
        // The bug was found as peripheral role but maybe it should apply also to central role
        if ((connPtr->llTask->taskID == LL_TASK_ID_PERIPHERAL) &&
            (connPtr->pendingParamUpdate == PARAM_UPDATE_PENDING) &&
            (connPtr->paramUpdateEvent == connPtr->nextEvent))
        {
          // Add the updated win offset to timeToNextEvt.
          timeToNextEvt += connPtr->paramUpdate.winOffset;

          // apply the connection param update
          llApplyParamUpdate(connPtr);
        }
      }
    }
    else // no update
    {
      // determine the number of events past the lastStartTime
      numEventsPast  =  time / (connPtr->curParam.connInterval * RAT_TICKS_IN_625US);
      numEventsPast += (time % (connPtr->curParam.connInterval * RAT_TICKS_IN_625US))?1:0;

      if ( connPtr->estWithHandover == UTRUE )
      {
        connPtr->nextEvent += numEventsPast;
      }
      else
      {
        // update next event counter
        connPtr->nextEvent = connPtr->currentEvent + numEventsPast;
      }
      // find the time to next event
      timeToNextEvt = (uint32)numEventsPast * (uint32)connPtr->curParam.connInterval;
    }

    // calcluate the new start time
    connCmd->timing.absStartTime =
      connPtr->llTask->lastStartTime + (timeToNextEvt * RAT_TICKS_IN_625US);

    // set anchor point to the new start time - Central Only
    if ( connPtr->llTask->taskID == LL_TASK_ID_CENTRAL )
    {
      // save off the anchor point
      connPtr->llTask->anchorPoint = connCmd->timing.absStartTime;
      // setup the connection event End Time
      // Note: Per the spec, this an be as late as 150us (i.e. T_IFS) before the
      //       next connection event. However, we need to end before that to allow
      //       time to post-process. Also, Extended Data could potentially require
      //       us to end the connection event at least 4.54ms before. In any case,
      //       to allow some build time flexibility, the amount of back-off can
      //       be set at build time using llConfig.connEvtCutoff.
      // Note: The Central connection parameter structure differs from the Peripheral
      //       connection parameter structure in that there is no timeout trigger
      //       or timeout time. In all other ways these two structures are
      //       identical. So when using the Peripheral connection parameter structure
      //       as the common link parameter structure, the end time offset in
      //       the structure corresponds to the timeoutTime field.
      //linkParam[connPtr->connId].endTime =
      ((RCL_CmdBle5Connection *)connCmd)->relRxTimeoutTime =
        ((((uint32)connPtr->curParam.connInterval * *llConfigTable.connEvtCutoff) / 100) * RAT_TICKS_IN_625US) -
        (2 * RAT_TICKS_IN_150US);
    }

    // adjust timer drift to account for the missed events - Peripheral Only
    if ( connPtr->llTask->taskID == LL_TASK_ID_PERIPHERAL )
    {
      // save time to next event for next connection event
      connPtr->lastTimeToNextEvt = timeToNextEvt;

      // calculate timer drift based on time since last event
      connPtr->timerDrift = ((timeToNextEvt * (RCOSC_LF_SCA + connPtr->mstSCA)) /
                             RAT_TICKS_IN_100US) + 1;

      // correct for elapsed timer drift
      connCmd->timing.absStartTime -= connPtr->timerDrift;
      if ( connPtr->estWithHandover == UFALSE )
      {
          // and adjust the timeout relative to the start time based on last timeout
          ((RCL_CmdBle5Connection *)connCmd)->relRxTimeoutTime =
                  connPtr->lastTimeoutTime + (2 * connPtr->timerDrift);
      }

      // add window size to timeout time
      if ( connPtr->pendingParamUpdate == PARAM_UPDATE_APPLIED )
      {
        ((RCL_CmdBle5Connection *)connCmd)->relRxTimeoutTime +=
          ( (uint32)connPtr->curParam.winSize * RAT_TICKS_IN_625US );
      }

      // setup the connection event End Time relative to the timestamp
      // Note: Per the spec, this an be as late as 150us (i.e. T_IFS) before the
      //       next connection event. However, we need to end before that to allow
      //       time to post-process. Also, Extended Data could potentially require
      //       us to end the connection event at least 4.54ms before. In any case,
      //       to allow some build time flexibility, the amount of back-off can
      //       be set at build time using llConfig.connEvtCutoff.
      connCmd->timing.relHardStopTime =
        ((((uint32)connPtr->curParam.connInterval * *llConfigTable.connEvtCutoff) / 100) * RAT_TICKS_IN_625US) -
        (2 * RAT_TICKS_IN_150US);
    }

    // save off the start time in the BLE task
    connPtr->llTask->startTime =
    connCmd->timing.absStartTime;
    // adjust data channel
    connPtr->nextChan = connPtr->currentChan;
    MAP_llSetNextDataChan( connPtr );
  }

  // so only update number of expiration events left to a LSTO timeout
  connPtr->numEventsLeft = MAP_llEventDelta( connPtr->expirationEvent,
                                             connPtr->nextEvent );

  // check if the number of events overflowed
  if ( connPtr->numEventsLeft > connPtr->expirationValue )
  {
    // appears we rolled over, and are about to disconnect due to a LSTO
    connPtr->numEventsLeft = 1;
  }

  return;
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llRemoveFeaturesForSendToPeer
 *
 * @brief       If a bit is shown as Host Controlled,
 *              the value may be set by the Host and shall default to zero.
 *              this function shout down bits by the table 4.7 in:
 *              BLUETOOTH CORE SPECIFICATION Version 5.4 | Vol 6, Part B page 2845.
 *              this function shout down bits that should be zero
 *              at feature exchange with peer device.
 *
 * input parameters
 *
 * @param       featureSetToPeer - array that hold FeatureSet bit mask to send to peer device.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llRemoveFeaturesForSendToPeer ( uint8* featureSetToPeer )
{
    featureSetToPeer[3] &=  ~LL_FEATURE_REMOTE_PUBLIC_KEY_VALIDATION;
}

/*******************************************************************************
 * @fn          llApplyParamUpdate
 *
 * @brief       This function is used to apply a connection update params
 *
 * input parameters
 *
 * @param       connPtr - Pointer to connection to be updated.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llApplyParamUpdate(llConnState_t *connPtr)
{
    // convert the new LSTO from time to an expiration connection event count
    MAP_llConvertLstoToEvent( connPtr, &connPtr->paramUpdate );

    // make the update peripheral latency the next latency to use
    connPtr->peripheralLatencyValue = connPtr->paramUpdate.peripheralLatency;

    // peripheral latency is not allowed until the first data packet is received
    connPtr->peripheralLatencyAllowed = FALSE;

    // for update parameters, deactivate peripheral latency until first packet arrives
    // Note: The spec isn't clear if peripheral latency is to be used immediately or
    //       not, but it seems awkward to allow it before we've even received
    //       the first packet.
    // Note: Assuming we wait for the first packet to re-enable peripheral latency
    //       at the possibly new value, the spec places no restrictions on
    //       requiring an ACK from the Central. However, to keep the TaskDone
    //       processing generic, the same rules will be applied. This does not
    //       violate the spec as the Peripheral is not required to use peripheral latency.
    connPtr->peripheralLatency = 0;

    // update the LSTO expiration count
    // Note: This is needed in case the new connection fails to receive a
    //       packet and a RXTIMEOUT results. The expiration count must
    //       already be initialized based on the new event values.
    connPtr->expirationEvent = connPtr->nextEvent + connPtr->expirationValue;

    // notify the Host if connInterval, connTimeout, or peripheralLatency
    // has been changed by the central
    // If no parameters are updated following a connection params request this event shall be issued
    // only if the device is the requester and not as a response to a peer device.
    if ( (connPtr->paramUpdate.connInterval != connPtr->curParam.connInterval)  ||
         (connPtr->paramUpdate.connTimeout  != connPtr->curParam.connTimeout )  ||
         (connPtr->paramUpdate.peripheralLatency != connPtr->curParam.peripheralLatency) ||
         (connPtr->procInitiator == TRUE) )
    {
      // notify the Host after the Instant has been transmitted
      connPtr->paramUpdateNotifyHost = TRUE;
    }
    // save off previous CI in case realignment needs it
    connPtr->prevConnInterval = connPtr->curParam.connInterval;
    // update the current parameters from the new parameters
    // Note: The new parameter connTimeout is not needed since once it is
    //       converted to connection events, its value is maintained by
    //       expirationValue. However, in order to compare to the last received
    //       updates, this value must be copied as well. The new parameter
    //       winSize is only needed in case there's a RX timeout.
    connPtr->curParam.winSize       = connPtr->paramUpdate.winSize;
    connPtr->curParam.connInterval  = connPtr->paramUpdate.connInterval;
    connPtr->curParam.peripheralLatency  = connPtr->paramUpdate.peripheralLatency;
    connPtr->curParam.connTimeout   = connPtr->paramUpdate.connTimeout;

    // convert the Control Procedure timeout into connection event count
    MAP_llConvertCtrlProcTimeoutToEvent( connPtr );

    // update parameters have been applied, but not yet ratified
    connPtr->pendingParamUpdate = PARAM_UPDATE_APPLIED;
}
#endif // ADV_CONN_CFG

/*******************************************************************************
 * This function is by the Controller to indicate to the Host that a hardware
 * error has occurred.
 *
 * Public function defined in ll.h.
 */
void llHardwareError( uint8 reasonCode )
{
  // generate event to the Host
  MAP_HCI_HardwareErrorEvent( reasonCode );

  return;
}


/*******************************************************************************
 * @fn          llMemCopySrc
 *
 * @brief      Generic memory copy. This function copies the source location to
 *             the destination location in bytes, and returns the next source
 *             address.
 *
 * input parameters
 *
 * @param       pDst - Destination address.
 * @param       pSrc - Source address.
 * @param       len  - Number of bytes to copy.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to next source address.
 */
uint8 *llMemCopySrc( uint8 *pDst, uint8 *pSrc, uint8 len )
{
  while ( len-- )
  {
    *pDst++ = *pSrc++;
  }

  return( pSrc );
}


/*******************************************************************************
 * @fn          llMemCopyDst
 *
 * @brief      Generic memory copy. This function copies the destination
 *             location to the source location in bytes, and returns the
 *             next destination address.
 *
 * input parameters
 *
 * @param       pDst - Destination address.
 * @param       pSrc - Source address.
 * @param       len  - Number of bytes to copy.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to next destination address.
 */
uint8 *llMemCopyDst( uint8 *pDst, uint8 *pSrc, uint8 len )
{
  while ( len-- )
  {
    *pDst++ = *pSrc++;
  }

  return( pDst );
}

/*******************************************************************************
 * @fn          llRegisterConnEvtCallback
 *
 * @brief       Register host's connection event notice callback
 *
 * @param       connPtr - Pointer to the connection.
 */
void llRegisterConnEvtCallback( llConnEvtCB_t cb, uint8_t eventType, uint16_t connHandle )
{
  // Set connection event reporting parameters
  llConnEvtNotice.cb = cb;
  llConnEvtNotice.handle = connHandle;
  llConnEvtNotice.eventType = eventType;
}

/*******************************************************************************
 * @fn          llSendConnEvtCallback
 *
 * @brief       Prepare and send connection event notica callback
 *
 * @param       connEvtStatus - connection event status
 * @param       numPkts       - num packets
 * @param       connPrt       - pointer to the connection information
 */
void llSendConnEvtCallback(uint8 connEvtStatus, uint16 numPkts, llConnState_t *connPtr)
{
  uint8 sendReport = FALSE;
  uint8_t curEventType;

  // Call connection event notification callback If host callback has been
  // registered...
  if ((llConnEvtNotice.cb != NULL) &&
      // And the connection event handle matches the desired handle...
      ((llConnEvtNotice.handle == connPtr->connId) ||
      // Or all connection event handles are to be sent...
      (llConnEvtNotice.handle == LL_CONNHANDLE_ALL)))
  {
    // if this is the first succesful packet
    // and registered to Connection_Established event
    if ( (connPtr->firstPacket) &&
        ((llConnEvtNotice.eventType & LL_CONN_EVT_CONN_ESTABLISHED) == LL_CONN_EVT_CONN_ESTABLISHED) )
    {
      sendReport   = TRUE;
      curEventType = LL_CONN_EVT_CONN_ESTABLISHED;
    }
    // Else if there is a PHY update indication
    else if ( (connPtr->phyUpdateSentOrReceivedInd == TRUE) &&
               // and registered to PHY update event
              ((llConnEvtNotice.eventType & LL_CONN_EVT_PHY_UPDATE) == LL_CONN_EVT_PHY_UPDATE) )
    {
      sendReport   = TRUE;
      curEventType = LL_CONN_EVT_PHY_UPDATE;
    }
    //else, registered tp all events
    else if ( llConnEvtNotice.eventType == LL_CONN_EVT_ALL )
    {
      sendReport   = TRUE;
      curEventType = LL_CONN_EVT_ALL;
    }
    else
    {
        /* this else clause is required, even if the
           programmer expects this will never be reached
           Fix Misra-C Required: MISRA.IF.NO_ELSE */
    }
  }

  if ( sendReport == TRUE )
  {
    // Allocate memory for report
    connEvtRpt_t *pReport = MAP_osal_mem_allocLimited( sizeof(connEvtRpt_t) );

    if(pReport != NULL)
    {
      pReport->status = connEvtStatus;
      pReport->handle = connPtr->connId;
      pReport->channel = connPtr->currentMappedChan;
      pReport->phy = connPtr->phyInfo.curPhy;
      pReport->lastRssi = connPtr->lastRssi;
      pReport->packets = numPkts;
      pReport->errors = connPtr->perInfo.numCrcErr;
      pReport->eventCounter = connPtr->currentEvent;
      pReport->timeStamp = RAT_TICKS_TO_US( connPtr->llTask->anchorPoint );
      pReport->eventType = curEventType;

      // Find time to next RF operation
      taskInfo_t *pCurrTask = MAP_llGetCurrentTask();
      if (pCurrTask == NULL)
      {
        // There is no next scheduled operation.
        pReport->nextTaskType = LL_TASK_ID_NONE;
        pReport->nextTaskTime = 0xFFFFFFFF;
      }
      else
      {
        pReport->nextTaskType = pCurrTask->taskID;
        // Find delta and convert to us
        pReport->nextTaskTime = RAT_TICKS_TO_US( MAP_llTimeDelta( pCurrTask->startTime,
                                                                  MAP_llGetCurrentTime() ) );
      }

      // Call callback
      ((llConnEvtNotice.cb))(pReport);
    }
  }
  if (connPtr->phyUpdateSentOrReceivedInd == TRUE )
  {
    connPtr->phyUpdateSentOrReceivedInd = FALSE;
  }
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 * @fn          llProcessScanTimeout
 *
 * @brief       This function is used to perform scan timeout operation after
 *              receives Extended Scan Timeout Event (LL_EVT_EXT_SCAN_TIMEOUT)
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
void llProcessScanTimeout( void )
{
  if ( extScanInfo->timingFlag == AE_SCAN_DURATION_TIMEOUT )
  {
    // invoke callback, if there is one
    MAP_llExtAdvCBack( LL_CBACK_EXT_SCAN_DURATION_END, NULL );

    // stop the Scanner
    // Check that the pointer is valid before use
    if (extScanInfo->pEnable != NULL )
    {
      extScanInfo->pEnable->enable = LL_SCAN_STOP;
      MAP_LE_SetExtScanEnable( extScanInfo->pEnable );
    }

    // check if period is enabled
    if ( extScanInfo->pEnable && extScanInfo->pEnable->period )
    {
      // re-enable to indicate to the back end that we're not really stopped
      extScanInfo->pEnable->enable = LL_SCAN_START;

      // setup timer for start of next period
      // Note: If the timer is not available, an event will be sent.
      (void)MAP_llStartDurationTimer( LL_EVT_EXT_SCAN_TIMEOUT,
                                      extScanInfo->scanPeriodLeft );

      // toggle flag
      extScanInfo->timingFlag = AE_SCAN_PERIOD_TIMEOUT;
    }
    else // period == zero
    {
      // invoke callback, if there is one
      // Note: Since the only data is the subcode HCI_BLE_SCAN_TIMEOUT_EVENT,
      //       which can be inferred by the event, no allocation will be done.
      MAP_llExtAdvCBack( LL_CBACK_EXT_SCAN_TIMEOUT, NULL );
    }
  }
  else // AE_SCAN_PERIOD_TIMEOUT
  {
    // this corresponds to the start of a period, and the re-start of Scan

    // set start state to indicate start is due to new period
    extScanInfo->scanStartState = AE_SCAN_START_STATE_NEXT;

    // invoke callback, if there is one
    MAP_llExtAdvCBack( LL_CBACK_EXT_SCAN_PERIOD_END, NULL );

    // toggle flag
    extScanInfo->timingFlag = AE_SCAN_DURATION_TIMEOUT;

    // check if filtering is enabled, and to be reset each period
    if ( extScanInfo->pEnable->dupFiltering == LL_FILTER_REPORTS_RESET_EACH_SCAN_PERIOD )
    {
      // check the extended scan accept list policy
      if ( (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_ANY_ADV_PKTS) ||
           (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_ANY_ADV_PKTS_EXT) )
      {
        // clear alternate accept list used for duplicate filtering
        MAP_AL_Scan_Init( alTableScan );
      }
      else // accept list used
      {
        // clear any ignore bits that might be set
        MAP_AL_ClearIgnoreList( alTable );
      }
    }

    // check if the user hasn't tried to halt the Scan
    // Note: Above, we're using the user's flag to know if the Scan stopped
    //       because of the duration. But between duration and the next
    //       period, the user could stop the scan. In that case, the user's
    //       enable flag will be STOP.
    if ( extScanInfo->pEnable->enable == LL_SCAN_START )
    {
      // start the Scanner
      // Note: The Timer is restarted by the enable.
      //extScanInfo->pEnable->enable = LL_SCAN_START;
      MAP_LE_SetExtScanEnable( extScanInfo->pEnable );
    }
  }
}
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * @fn          llProcessCentralConnectionCreated
 *
 * @brief       This function is used to perform central connection created procedure
 *              after receives LL_EVT_CENTRAL_CONN_CREATED Event
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
void llProcessCentralConnectionCreated( void )
{
  llConnState_t    *connPtr = MAP_llDataGetConnPtr( extInitInfo->connId );

  uint8  peerType = connPtr->peerInfo.peerAddrType;
  uint8 *peerAddr = connPtr->peerInfo.peerAddr;
  uint8 *peerRPA  = NULL;
  uint8 *localRPA = NULL;

  LL_ASSERT( extInitInfo->connId != LL_INVALID_CONNECTION_ID );

  if (( privInfo.addrResolution ) ||
     (( privInfo.addrResolution == FALSE) &&
      ( extInitInfo->pCreateConn->initFilterPolicy == LL_INIT_AL_POLICY_USE_ACCEPT_LIST )))
  {
      // get index to peer address/type in RL (if present)
    uint8 rlIndex = MAP_LL_PRIV_FindPeerInRL( resolvingList,
                                          MASK_ID_ADDRTYPE(peerType),
                                          peerAddr );

    // check if in the RL
    if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
    {
      // check if peer address/type is an RPA
      if ( MAP_LL_PRIV_IsRPA( MASK_ID_ADDRTYPE(peerType), peerAddr ) )
      {
        // peer RPA in RL, so get Identity address/type
        peerType = resolvingList[rlIndex].idAddrType | LL_DEV_ADDR_TYPE_ID_MASK;
        peerAddr = resolvingList[rlIndex].idAddr;
        peerRPA  = resolvingList[rlIndex].RPA;

        // update peer address/type in connection
        // Note: This is done to ensure the command HCI_EXT_GetConnInfo
        //       displays the proper peer address and address type.
        connPtr->peerInfo.peerAddrType = MASK_ID_ADDRTYPE(peerType);
        MAP_osal_memcpy( connPtr->peerInfo.peerAddr, peerAddr, B_ADDR_LEN );
      }
#ifndef QUAL_TEST
      else if ( MAP_LL_PRIV_IsIDA( MASK_ID_ADDRTYPE(peerType), peerAddr ) )
      {
        // check if the ID address is in the RL and associated with an IRK=0 or
        // an IRK!=0 and Device Privacy Mode
        if ( (resolvingList[rlIndex].privMode == LL_DEVICE_PRIVACY_MODE) ||
             MAP_LL_PRIV_IsZeroIRK(resolvingList[rlIndex].IRK) )
        {
          // indicate an ID address was received
          peerType |= LL_DEV_ADDR_TYPE_ID_MASK;
        }
      }
#endif // QUAL_TEST
    }

    // check if own address type is Identity
    if ( LL_IS_ADDR_TYPE_RPA(extInitInfo->ownAddrType) )
    {
      // check if Identity address was specified and local IRK is valid
      if ( !MAP_LL_PRIV_IsZeroIRK( resolvingList[LOCAL_RL_INDEX].IRK )
#ifdef QUAL_TEST
          // case Non-Resolvable Address
          && ( !MAP_LL_PRIV_IsNRPA( MASK_ID_ADDRTYPE(extInitInfo->ownAddrType),
                                    ADDRTYPE_TO_OWNADDR( extInitInfo->ownAddrType )) )
#endif // QUAL_TEST
         )
      {
        localRPA = resolvingList[LOCAL_RL_INDEX].RPA;
      }
    }
  }

  MAP_LL_EnhancedConnectionCompleteCback( LL_STATUS_SUCCESS,                   // reasonCode
                                          (uint16)connPtr->connId,             // connection handle
                                          LL_LINK_CONNECT_COMPLETE_CENTRAL,     // role
                                          peerType,                            // peer's address type
                                          peerAddr,                            // peer's address
                                          localRPA,                            // local RPA
                                          peerRPA,                             // peer RPA
                                          connPtr->curParam.connInterval >> 1, // connection interval, back to 1.25ms units
                                          connPtr->curParam.peripheralLatency,      // peripheral latency
                                          connPtr->curParam.connTimeout >> 4,  // connection timeout, back to 10ms units
                                          0 );                                 // sleep clock accurracy not valid for central

  MAP_LL_ChannelSelectionAlgorithmCback( (uint16)connPtr->connId,              // connection handle
                                         (connPtr->pChSelAlgo == MAP_llGetNextDataChanAlgo1) ?
                                         LL_CHANNEL_SELECT_ALGO_1                            :
                                         LL_CHANNEL_SELECT_ALGO_2 );
}
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
/*******************************************************************************
 * @fn          llProcessPeripheralConnectionCreated
 *
 * @brief       This function is used to perform peripheral connection created procedure
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
void llProcessPeripheralConnectionCreated( void )
{
  llConnState_t *connPtr;
  uint8          peerType;
  uint8         *peerAddr;
  uint8         *peerRPA  = NULL;
  uint8         *localRPA = NULL;
  advSet_t      *pAdvSet = MAP_LL_SearchAdvSet( aeCurConnHandle );
  uint8          ownAddrType = 0xFF;
  uint8          connIndex = 0;

  LL_ASSERT( llConns.currentConn != LL_INVALID_CONNECTION_ID );

  if ( llConns.numActiveConns == 0 )
  {
      // No Active connections
      return;
  }

  do
  {
    // Get the info pointer of the connection.
    connPtr = MAP_llDataGetConnPtr( connIndex );

	// Check if the connection is active and if it was formed using connection handover
    if( connPtr->activeConn  == UTRUE )
    {
      if ( connPtr->estWithHandover == UTRUE )
      {
        // Found the connection formed with handover
        break;
      }
    }
    connIndex++;
  } while( connIndex < maxNumConns );

  // If it isn't handover connection
  if ( connIndex == maxNumConns )
  {
    if( pAdvSet != NULL )
    {
      // The connection was formed using the Advertising set. Fetch the connection pointer
      connPtr = MAP_llDataGetConnPtr( pAdvSet->connId );
      ownAddrType = pAdvSet->pAdvParam->ownAddrType;
    }
  }
  else
  {
    // We have the handover connection pointer
    if ( MAP_LL_PRIV_IsZeroIRK(resolvingList[LOCAL_RL_INDEX].IRK) == FALSE )
    {
      ownAddrType = resolvingList[LOCAL_RL_INDEX].idAddrType | LL_DEV_ADDR_TYPE_ID_MASK;
    }
  }

  // get the peer address type and address from Connect Request packet
  peerType = connPtr->peerInfo.peerAddrType;
  peerAddr = connPtr->peerInfo.peerAddr;

  if ( privInfo.addrResolution )
  {
    // check if own address type is Identity
    if ( LL_IS_ADDR_TYPE_RPA(ownAddrType) )
    {
      // check if Identity address was specified and local IRK is valid
      if ( !MAP_LL_PRIV_IsZeroIRK( resolvingList[LOCAL_RL_INDEX].IRK )
#ifdef QUAL_TEST
          // note: fix for test case LL/SEC/ADV/BV-06-C
          && ( !MAP_LL_PRIV_IsNRPA( MASK_ID_ADDRTYPE(ownAddrType),
                                    ADDRTYPE_TO_OWNADDR( ownAddrType )) )
#endif // QUAL_TEST
        )
      {
        localRPA = resolvingList[LOCAL_RL_INDEX].RPA;
      }
    }

    // Check if the peer address is an RPA
    if ( MAP_LL_PRIV_IsRPA( peerType, peerAddr ) )
    {
      uint8 rlIndex = INVALID_RESOLVE_LIST_INDEX;

      // If the connection wasn't formed using connection handover process, check
      // the advertiser's accept list policy
      if ( (connPtr->estWithHandover == UFALSE) && (pAdvSet != NULL) )
      {
        // check the filter policy
        // Note: If the filter policy does not use the AL, then we connected
        //       even though we never resolved the address! Please see CSWG
        //       Erratum #6984 for more detail.
        if ( (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_ANY_REQ)  ||
             (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_SCAN_REQ) )
        {
          rlIndex = MAP_LL_PRIV_IsResolvable( peerAddr, resolvingList );
        }
        else // the AL was used, so we had to have resolved the RPA
        {
          rlIndex = MAP_LL_PRIV_FindPeerInRL( resolvingList,
                                              peerType,
                                              peerAddr );
        }
      }
      else
      {
        // Check if this is a resolvable address
        if ( MAP_LL_PRIV_IsRPA( peerType, peerAddr ) )
        {
          // Check if this address can to be resolved
          rlIndex = MAP_LL_PRIV_IsResolvable( peerAddr, resolvingList );
        }
      }

      // Ensure we have a valid index
      if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
      {
        // Put the Peer's RPA in the RL
        // Note: When the AL is enabled, this isn't needed, but does no harm
        //       either and allows this code to be used in either case.
        MAP_osal_memcpy( resolvingList[rlIndex].RPA, peerAddr, B_ADDR_LEN );

        peerType = resolvingList[rlIndex].idAddrType | LL_DEV_ADDR_TYPE_ID_MASK;
        peerAddr = resolvingList[rlIndex].idAddr;
        peerRPA  = resolvingList[rlIndex].RPA;

        if ( pAdvSet != NULL )
        {
          // Update the ownAddrType the device connected with
          connPtr->ownAddrType = pAdvSet->actualOwnAddrType;
        }
        else
        {
          // Update based on the controller information
          if ( LL_IsRandomAddressConfigured() == TRUE )
          {
            connPtr->ownAddrType = LL_DEV_ADDR_TYPE_RANDOM;
          }
          else
          {
            connPtr->ownAddrType = LL_DEV_ADDR_TYPE_PUBLIC;
          }
        }

        // Update peer address type and address in packet
        // Note: This is done to ensure the command HCI_EXT_GetConnInfo
        //       displays the proper peer address and address type.
        connPtr->peerInfo.peerAddrType = MASK_ID_ADDRTYPE(peerType);
        MAP_osal_memcpy( connPtr->peerInfo.peerAddr, peerAddr , B_ADDR_LEN );
      }
    }
    else if ( MAP_LL_PRIV_IsIDA( peerType, peerAddr ) )
    {
      uint8 rlIndex = MAP_LL_PRIV_FindPeerInRL( resolvingList,
                                                peerType,
                                                peerAddr );

  #ifdef QUAL_TEST
      // Note: test cases LL/SEC/ADV/BV-07-C and LL/SEC/ADV/BV-12-C
      if ( (rlIndex != INVALID_RESOLVE_LIST_INDEX) &&
           (!MAP_LL_PRIV_IsZeroIRK(resolvingList[rlIndex].IRK)) &&
           (resolvingList[rlIndex].privMode == LL_DEVICE_PRIVACY_MODE) )
  #else
      // Check if the ID address is in the RL and associated with an IRK=0 or
      // an IRK!=0 and Device Privacy Mode
      if ( (rlIndex != INVALID_RESOLVE_LIST_INDEX) &&
           (MAP_LL_PRIV_IsZeroIRK(resolvingList[rlIndex].IRK) ||
            (!MAP_LL_PRIV_IsZeroIRK(resolvingList[rlIndex].IRK) &&
             resolvingList[rlIndex].privMode == LL_DEVICE_PRIVACY_MODE)) )
  #endif // QUAL_TEST
      {
        // Indicate an ID address was received
        peerType |= LL_DEV_ADDR_TYPE_ID_MASK;
      }
    }
  }

  // The events must be ordered according to the test spec LL/CONN/ADV/BV-5-16
  MAP_LL_EnhancedConnectionCompleteCback( LL_STATUS_SUCCESS,                   // reasonCode
                                          (uint16)connPtr->connId,             // connection handle
                                          LL_LINK_CONNECT_COMPLETE_PERIPHERAL,      // role
                                          peerType,                            // peer's address type
                                          peerAddr,                            // peer's address
                                          localRPA,                            // local RPA
                                          peerRPA,                             // peer RPA
                                          connPtr->curParam.connInterval >> 1, // connection interval, back to 1.25ms units
                                          connPtr->peripheralLatencyValue,          // peripheral latency
                                          connPtr->curParam.connTimeout >> 4,  // connection timeout, back to 10ms units
                                          connPtr->sleepClkAccuracy );         // sleep clock accurracy

  MAP_LL_ChannelSelectionAlgorithmCback( (uint16)connPtr->connId,              // connection handle
                                         (connPtr->pChSelAlgo == MAP_llGetNextDataChanAlgo1) ?
                                         LL_CHANNEL_SELECT_ALGO_1                            :
                                         LL_CHANNEL_SELECT_ALGO_2 );

  if ( pAdvSet != NULL )
  {
    // Send the Adv Set End callback, if enabled
    MAP_llSendAdvSetEndEvent( pAdvSet );

    // Send the LE Advertisement Set Terminated Event
    MAP_llSendAdvSetTermEvent( pAdvSet,
                               LL_STATUS_SUCCESS,
                               connPtr->connId );
  }
}
#endif // ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * @fn          llProcessConnectionEstablishFailed
 *
 * @brief       This function is used to perform connection failed to establish
 *              procedure after receives failure event
 *
 * input parameters
 *
 * @param       role - LL_LINK_CONNECT_COMPLETE_CENTRAL or LL_LINK_CONNECT_COMPLETE_PERIPHERAL.
 * @param       reason - failure reason code.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llProcessConnectionEstablishFailed( uint8 role, uint8 reason )
{
    // Notify Host
    MAP_LL_EnhancedConnectionCompleteCback( reason,                              // reasonCode
                                            (uint16)0,                           // connection handle
                                            role,                                // role
                                            0,                                   // peer's address type
                                            0,                                   // peer's address
                                            0,                                   // local RPA
                                            0,                                   // peer RPA
                                            0,                                   // connection interval, back to 1.25ms units
                                            0,                                   // peripheral latency
                                            0,                                   // connection timeout, back to 10ms units
                                            0 );                                 // sleep clock accurracy
}
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llProcessAdvAddrResolutionTimeout
 *
 * @brief       This function is used to perform advertising address resolution
 *              timeout procedure after receives LL_EVT_ADDRESS_RESOLUTION_TIMEOUT Event
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
void llProcessAdvAddrResolutionTimeout( void )
{
  advSet_t *pAdvSet = advSetList;

  // For each Adv Set that is enabled, is Directed, and uses an Identity
  // address, update the peer InitA address' RPA, if in the RL
  while( pAdvSet )
  {
    // Get pointer to RF command
    aeLegacyRf_t *pRf = (aeLegacyRf_t *)pAdvSet->pRfCmds;

    if ( TST_AE_PROPS_DIR(pAdvSet->pAdvParam->eventProps)      &&
         (LL_IS_ADDR_TYPE_RPA(pAdvSet->ownAddrType)            ||
         (pAdvSet->peerAddrType == LL_DEV_ADDR_TYPE_RANDOM)) )
    {
      // we are, so update the RPA of InitA; first, we have to find it in RL
      uint8 rlIndex = MAP_LL_PRIV_FindPeerInRL( resolvingList,
                                                pAdvSet->peerAddrType,
                                                pAdvSet->peerAddr );

      // check if the Peer's ID address was found in the RL
      if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
      {
        // check if the Peer's IRK is valid
        if ( !MAP_LL_PRIV_IsZeroIRK( resolvingList[rlIndex].IRK ) )
        {
          // it is, so re-generate peer RPA
          MAP_LL_PRIV_GenerateRPA( resolvingList[rlIndex].IRK,
                                   resolvingList[rlIndex].RPA );

          // set the Peer's address and address type
          MAP_osal_memcpy( pAdvSet->peerAddr,
                           resolvingList[rlIndex].RPA,
                           B_ADDR_LEN );
          if (pRf)
          {
            // copy the peer address to the adv params
            MAP_osal_memcpy(pRf->advParam.peerA,pAdvSet->peerAddr,LL_DEVICE_ADDR_LEN );
            // copy the peer address to the adv data
            MAP_osal_memcpy(pRf->advPacket.targetA,pAdvSet->peerAddr,LL_DEVICE_ADDR_LEN );
            // copy the peer address to the scan rsp data
            MAP_osal_memcpy(pRf->scanRspPacket.targetA,pAdvSet->peerAddr,LL_DEVICE_ADDR_LEN );
          }
        }
      }
    }
    if ( (LL_IS_ADDR_TYPE_RPA(pAdvSet->ownAddrType)) &&
         !MAP_LL_PRIV_IsZeroIRK( resolvingList[LOCAL_RL_INDEX].IRK ) )
    {
      // update the local RPA
      MAP_osal_memcpy( pAdvSet->ownAddr,
                       resolvingList[LOCAL_RL_INDEX].RPA,
                       B_ADDR_LEN );
      if (pRf)
      {
        // copy the advertising address to the adv params
        MAP_osal_memcpy(pRf->advParam.advA,pAdvSet->ownAddr,LL_DEVICE_ADDR_LEN );
        // copy the advertising address to the adv data
        MAP_osal_memcpy(pRf->advPacket.advA,pAdvSet->ownAddr,LL_DEVICE_ADDR_LEN );
        // copy the advertising address to the scan rsp data
        MAP_osal_memcpy(pRf->scanRspPacket.advA,pAdvSet->ownAddr,LL_DEVICE_ADDR_LEN );
      }
    }

    pAdvSet = pAdvSet->next;
  }
}

#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
/*******************************************************************************
 * @fn          llCheckPeripheralTerminate
 *
 * @brief       This function is used to check if the disconnect flag was set and
 *              perform connection termination
 *
 * input parameters
 *
 * @param       connId - Connection ID.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      True in case terminate was executed.
 */
uint8 llCheckPeripheralTerminate( uint8 connId )
{
  // get connection pointer
  llConnState_t * connPtr = MAP_llDataGetConnPtr( connId );

  // check if disconnection flag was marked
  if ((connPtr != NULL) && (connPtr->extFeatureMask & EXT_FEATURE_DISCONNECT_ENABLE))
  {
    llState = LL_STATE_CONN_PERIPHERAL;
    // terminate the connection
    MAP_llConnTerminate(connPtr ,LL_DISCONNECT_AUTH_FAILURE);
    // unmark the disconnection flag
    connPtr->extFeatureMask &= EXT_FEATURE_DISCONNECT_DISABLE;
    return TRUE;
  }
  return FALSE;
}
#endif

#ifdef BLE_HEALTH
/*******************************************************************************
 * @fn          llDbgInf_addSchedRec
 *
 * @brief       This function adds a scheduler record to the debug info module
 *              Note: this function is implemented here due to use of ll types
 *
 * input parameters
 *
 * @param       llTask - Pointer to task information
 *
 * @return  @ref SUCCESS
 * @return  @ref INVALIDPARAMETER : the input is not valid
 * @return  @ref FAILURE : the scheduler domain is not initialized
 *
 */
uint8_t llDbgInf_addSchedRec(taskInfo_t * const llTask)
{
  uint8_t status = INVALIDPARAMETER;
  DbgInf_schedNewRec_t newRec = {0};

  // Validate input
  if ( llTask != NULL )
  {
    // Add new scheduler record info
    newRec.activeTasks = llTaskList.activeTasks;
    newRec.taskID = llTask->taskID;
    newRec.llState = llState;
    newRec.cmdStartTime = (uint32_t)llTask->startTime;
    newRec.timeStamp = (uint32_t)MAP_llGetCurrentTime();

    status = MAP_DbgInf_addSchedRec( &newRec );
  }

  // Return status value
  return (status);
}

/*******************************************************************************
 * @fn          llDbgInf_addConnTerm
 *
 * @brief       This function adds a connection terminated record to the debug info module
 *              Note: this function is implemented here due to use of ll types
 *
 * input parameters
 *
 * @param       connHandle    - Connection handle
 * @param       reasonCode - Status of connection complete
 *
 * @return  @ref SUCCESS
 * @return  @ref INVALIDPARAMETER : the input is not valid
 * @return  @ref FAILURE : the scheduler domain is not initialized
 *
 */
uint8_t llDbgInf_addConnTerm(uint16_t connHandle, uint8_t reasonCode)
{
  uint8_t status = INVALIDPARAMETER;
  llConnState_t *connPtr = NULL;
  DbgInf_connTermRec_t newRec = {0};

  // Get the struct info of the terminated connection
  connPtr = (llConnState_t *)MAP_llDataGetConnPtr((uint8)connHandle );
  if ( connPtr != NULL )
  {
    (void)MAP_osal_memcpy(&newRec.peerAddr[0], &connPtr->peerInfo.peerAddr[0], 2);
    newRec.connId = (uint8_t)connHandle;
    newRec.termReason = reasonCode;
    newRec.connInterval = connPtr->curParam.connInterval;
    newRec.connEvent = connPtr->currentEvent;

    status = MAP_DbgInf_addConnTerm( &newRec );
  }

  // Return status value
  return ( status );
}
#endif //BLE_HEALTH

/*******************************************************************************
 * @fn          llHealthCheck
 *
 * @brief       This function do an health check to the controller and returns
 *              status in order to give the host the ability to monitor the stack.
 *
 * input parameters
 *
 * @param       None
 *
 * @return      In case of success - return LL_HEALTH_MONITOR_SUCCESS
 *              In case of error in connection - return LL_HEALTH_MONITOR_CONN_FAILURE
 *              In case of error in scan - return LL_HEALTH_MONITOR_SCAN_FAILURE
 *              In case of error in init connection - return LL_HEALTH_MONITOR_INIT_FAILURE
 *              In case of error in advertise - return LL_HEALTH_MONITOR_ADV_FAILURE
 *
 */
int8 llHealthCheck(void)
{
  uint32 currentTime;
  halIntState_t cs;
  volatile uint32 connTime;
  volatile uint32 scanTime;
  volatile uint32 initTime;
  volatile uint32 advTime;

  HAL_ENTER_CRITICAL_SECTION(cs);
  connTime = llHealth.conn.time;
  scanTime = llHealth.scan.time;
  initTime = llHealth.init.time;
  advTime  = llHealth.adv.time;
  currentTime = MAP_llGetCurrentTime();
  HAL_EXIT_CRITICAL_SECTION(cs);

 if (llHealth.preRelease == TRUE)
  {
    // Lowering the flag back to FALSE state
    llHealth.preRelease = FALSE;
    return LL_HEALTH_CHECK_PRE_RELEASE;
  }
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  // set the threshold time as default in case it was not set.
  if (llHealth.conn.threshold == 0)
  {
    llHealth.conn.threshold = LL_HEALTH_CHECK_CONN_DEFAULT_THRESHOLD;
  }
  // in case there is at least 1 connection, check the connection health
  if ((llConns.numActiveConns > 0) && (connTime > 0) &&
      (llTimeAbs(currentTime, connTime) > llHealth.conn.threshold))
  {
    return LL_HEALTH_CHECK_CONN_FAILURE;
  }
#endif
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
  // set the threshold time as default in case it was not set.
  if (llHealth.scan.threshold == 0)
  {
    llHealth.scan.threshold = LL_HEALTH_CHECK_SCAN_DEFAULT_THRESHOLD;
  }
  // in case the scan is enable, check the scan health
  if ((extScanInfo->scanMode == LL_SCAN_START) && (scanTime > 0) &&
      (llTimeAbs(currentTime, scanTime) > llHealth.scan.threshold))
  {
    return LL_HEALTH_CHECK_SCAN_FAILURE;
  }
#endif
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  // set the threshold time as default in case it was not set.
  if (llHealth.init.threshold == 0)
  {
    llHealth.init.threshold = LL_HEALTH_CHECK_INIT_DEFAULT_THRESHOLD;
  }
  // in case the host request for create connection, check the init health
  if ((extInitInfo->scanMode == LL_SCAN_START) && (initTime > 0) &&
      (llTimeAbs(currentTime, initTime) > llHealth.init.threshold))
  {
    return LL_HEALTH_CHECK_INIT_FAILURE;
  }
#endif
  // set the threshold time as default in case it was not set.
  if (llHealth.adv.threshold == 0)
  {
    llHealth.adv.threshold = LL_HEALTH_CHECK_ADV_DEFAULT_THRESHOLD;
  }
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  // in case there is at least one enabled advertising set, check the adv health
  if (( MAP_LL_CountAdvSets(LE_COUNT_ENABLED_ADV_SETS )) && (advTime > 0) &&
      (llTimeAbs(currentTime, advTime) > llHealth.adv.threshold))
  {
    return LL_HEALTH_CHECK_ADV_FAILURE;
  }
#endif
  // everything is OK
  return LL_HEALTH_CHECK_SUCCESS;
}

/*******************************************************************************
 * @fn          llHealthUpdate
 *
 * @brief       This function update the health reference time according
 *              to a requested activity
 *
 * input parameters
 *
 * @param       state - requested activity
 *
 * @return      None
 *
 */
void llHealthUpdate(uint8 state)
{
  uint32 currentTime = MAP_llGetCurrentTime();
  halIntState_t cs;

  HAL_ENTER_CRITICAL_SECTION(cs);
  switch (state)
  {
    case LL_STATE_CONN_CENTRAL:
    case LL_STATE_CONN_PERIPHERAL:
      // update the connection time
      llHealth.conn.time = currentTime;
      break;
    case LL_STATE_SCAN:
      // update the scan time
      llHealth.scan.time = currentTime;
      break;
    case LL_STATE_INIT:
      // update the init time
      llHealth.init.time = currentTime;
      break;
    case LL_STATE_EXT_ADV:
      // update the adv time
      llHealth.adv.time = currentTime;
      break;
    case LL_PRE_REALSE_INDICATION:
      // update preRelease
      llHealth.preRelease = TRUE;
      break;
  }
  HAL_EXIT_CRITICAL_SECTION(cs);
}

/*******************************************************************************
 * @fn          llHealthSetThreshold
 *
 * @brief       This function set the health threshold time in milliseconds
 *              for each activity.
 *
 * input parameters
 *
 * @param       connTime - connection threshold time in milliseconds
 * @param       scanTime - scan threshold time in milliseconds
 * @param       initTime - init connection threshold time in milliseconds
 * @param       advTime  - advertising threshold time in milliseconds
 *
 * @return      None
 *
 */
void llHealthSetThreshold(uint32 connTime, uint32 scanTime, uint32 initTime, uint32 advTime )
{
  // convert the time from msec to rf ticks
  llHealth.conn.threshold = connTime * RAT_TICKS_IN_1MS;
  llHealth.scan.threshold = scanTime * RAT_TICKS_IN_1MS;
  llHealth.init.threshold = initTime * RAT_TICKS_IN_1MS;
  llHealth.adv.threshold = advTime * RAT_TICKS_IN_1MS;
}

/*******************************************************************************
 * @fn          llHealthUpdateWrapperForOsal
 *
 * @brief       This function is a wrapper for llHealthCheckUpdate for premature release indication
 *              that is being managed in the osal_bm_free
 *
 * input parameters
 *
 * @param       None
 *
 * @return      None
 *
 */
void llHealthUpdateWrapperForOsal(void)
{
  llHealthUpdate(LL_PRE_REALSE_INDICATION);
}
/*******************************************************************************
 * @fn          llQueryTxQueue
 *
 * @brief search inside the entries inside th TX queue if an adress exist
 *
 * input parameters
 *
 * @param addr address to serach in the TX queue
 *
 * @return TRUE or FALSE depends if address was found or not
 */
uint8 llQueryTxQueue(uint32 addr)
{
  uint8 addressFound = FALSE;
  uint8 connectionIter = 0;
  uint16 dataEntrySize;

  // going through all connection, on all data entries until address found or finish all connections
  while ( (!addressFound) && (connectionIter < maxNumConns))
  {
    // Validate the connection is active
    if ( llConns.llConnection[connectionIter].activeConn == TRUE )
    {
      // get connection information
      llConnState_t *connPtr = MAP_llDataGetConnPtr( connectionIter );

      RCL_Buffer_TxBuffer *pNext;
      dataEntrySize = sizeof(RCL_Buffer_TxBuffer);

      // Get pointer to first data entry in the TX Queue
      pNext = RCL_TxBuffer_head(&((txDataQ_t *)(connPtr->pTxDataEntryQ))->llDataBuffers);

      // count finished entries
      while (pNext != NULL)
      {
        // Check if the address input is inside the entry
        // The addresses inside the entry are continuous
        // you can see more about it in the rf_data_entry.h
        if ( (addr >= (uint32)pNext) &&
            (addr <= (uint32)(((uint8 *)(pNext)) + dataEntrySize + pNext->length + 1) ))
        {
          addressFound = TRUE;
          break;
        }
        pNext = RCL_TxBuffer_next(pNext);
      }
    }
    connectionIter++;
  }
  return addressFound;
}

/*******************************************************************************
 * @fn          llCreateCommonFeatureSet
 *
 * @brief       This routine is used to create a common active set for the device and
 *              it's peer. The common set is stored in the
 *              featureSetInfo.featureSet per connection.
 *              Also, this routine will store the peer's feature set.
 *              This data will be used for the remote_feature_request pdu.
 *              To store this data we will use the previously unused featureSetInfo.featureSetMask
 *              array in the connPtr to store the peer info.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to BLE LL Connection.
 * @param       pBuf    - Pointer to Control packet payload.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
/********************************************************************************/
void llCreateCommonFeatureSet( llConnState_t *connPtr, uint8 *pBuf )
{
  uint8 i;

  // Please see BLE V5.3, Vol.6, Part B, Section 4.6, Table 4.7
  // Please see BLE V5.3, Vol.6, Part B, Section 5.1.4 - Feature Exchange procedure

  for ( i = 0; i < LL_MAX_FEATURE_SET_SIZE; i++ )
  {
    // Store the peer's feature set.
    // Note: This will be used for the future remote_feature_response PDU.
    // Note: We will reuse the previously unused featureSetInfo.featureSetMask array to store the peer set.
    // Note: The remote_feature_response PDU is constructed in the following way:
    //  - FeatureSet_USED parameter is one octet long and is the logical AND of
    //    the least significant octets of FeatureSet_C and FeatureSet_P.
    //  - octet 0 of the FeatureSet response field shall be set to FeatureSet_USED and the remaining
    //    octets shall be set to the corresponding octets of the peer device.
    connPtr->featureSetInfo.featureSetMask[i] = pBuf[i] & ((i == 0) ? deviceFeatureSet.featureSet[i] : 0xff);

    // Masked features shall not be used to determine whether a peer device supports any associated procedure.
    // Thus, treat the peer's feature set as if those features are enabled.
    // according to Section 4.6
    pBuf[i] |= deviceFeatureSet.featureSetMask[i];

    // combine device's and the peer's feature sets into a common active set.
    connPtr->featureSetInfo.featureSet[i] = deviceFeatureSet.featureSet[i] & pBuf[i];

    // set flag to indicate the response has been received
    // exclude a case where device is a peripheral and a peer (central) does not support the "peripheral feature exchange"
    // this case is excluded because of possible race condition: a peer central device (that does not support the
    // "peripheral feature exchange") sent a feature exchange at the same time of the peripheral sent his own.
    // the procedure might be marked as completed from the peripheral side by mistake.
    if (( llState == LL_STATE_CONN_CENTRAL ) || (connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_SLV_FEATURES_EXCHANGE))
    {
      connPtr->featureSetInfo.featureRspRcved = LL_FEATURE_RSP_DONE;
    }
  }
}

/*******************************************************************************
 * @fn          llConvertBlePhyToLlPhy
 *
 * @brief       This routine is used to convert the BLE PHY values to LL PHY
 *              values.
 *
 * input parameters
 *
 * @param       blePhy  - BLE Phy value. This value represents:
 *                        1. Phy value in the Set_Phy Procedure PDU
 *                        2. Phy value in the adv packet
 *
 * output parameters
 *
 * @param       llPhy         - Pointer to LL PHY value. This value represents:
 *                              1. TxPhy/RxPhy value in the HCI_LE_SetPhyCmd from the host
 *                              2. The way we store the PHY value in LL (connPtr).
 * @param       llPhyCodedOpt - Pointer to LL PHY Options value.
 *                              1. For PHY 1Mbps and PHY 2Mbps, the option should be on LL_PHY_NONE.
 *                              2. Using Coded PHY, there are 2 options: LL_PHY_OPT_S2 and LL_PHY_OPT_S8.
 *
 * @return      TRUE  - in case the convert was successful.
 *              FALSE - in case the convert was not successful.
 */
/********************************************************************************/
uint8 llConvertBlePhyToLlPhy(uint8 blePhy, uint8 *llPhy, uint8 *llPhyCodedOpt)
{
  switch (blePhy)
  {
    case BLE5_1M_PHY:
      *llPhy = LL_PHY_1_MBPS;
      *llPhyCodedOpt = LL_PHY_OPT_NONE;
    break;
    case BLE5_2M_PHY:
      *llPhy = LL_PHY_2_MBPS;
      *llPhyCodedOpt = LL_PHY_OPT_NONE;
    break;
    case BLE5_S8_PHY:
      *llPhy = LL_PHY_CODED;
      *llPhyCodedOpt = LL_PHY_OPT_S8;
    break;
    case BLE5_S2_PHY:
        *llPhy = LL_PHY_CODED;
        *llPhyCodedOpt = LL_PHY_OPT_S2;
    break;
    default:
      /* Shouldn't be here! */
      return FALSE;
    break;
  }
  return TRUE;
}

/*******************************************************************************
 * @fn          llConvertAePhyToBlePhy
 *
 * @brief       This routine is used to convert the AE PHY values to BLE PHY
 *              values.
 *
 * input parameters
 *
 * @param       aePhy   - AE PHY value.
 *
 * output parameters
 *
 * @param       blePhy  - Pointer to BLE Phy value. This value represents:
 *                        1. Phy value in the Set_Phy Procedure PDU
 *                        2. Phy value in the adv packet
 *
 */
/********************************************************************************/
void llConvertAePhyToBlePhy(uint8 aePhy, uint8 *blePhy)
{
  switch (aePhy)
  {
    case AE_PHY_1_MBPS:
      *blePhy = BLE5_1M_PHY;
    break;

    case AE_PHY_2_MBPS:
      *blePhy = BLE5_2M_PHY;
    break;

    case AE_PHY_CODED:
    case AE_PHY_CODED_S2:
      *blePhy = BLE5_CODED_PHY;
    break;

    // Won't get here, for safety enter Coded Phy and if it's
    // a wrong Phy the connection will drop.
    default:
      *blePhy = BLE5_CODED_PHY;
    break;
  }
}

/*******************************************************************************
 * @fn          llConvertLlPhyToBlePhy
 *
 * @brief       This routine is used to convert the LL PHY values to BLE PHY
 *              values.
 *
 * input parameters
 *
 * @param       llPhy   - LL PHY value. This value represents:
 *                        1. TxPhy/RxPhy value in the HCI_LE_SetPhyCmd from the host
 *                        2. The way we store the PHY value in LL (connPtr).
 *
 * output parameters
 *
 * @param       blePhy  - Pointer to BLE Phy value. This value represents:
 *                        1. Phy value in the Set_Phy Procedure PDU
 *                        2. Phy value in the adv packet
 *
 * @return      TRUE  - in case the convert was successful.
 *              FALSE - in case the convert was not successful.
 */
/********************************************************************************/
uint8 llConvertLlPhyToBlePhy(uint8 llPhy, uint8 *blePhy)
{
  switch (llPhy)
  {
    case LL_PHY_1_MBPS:
      *blePhy = BLE5_1M_PHY;
    break;
    case LL_PHY_2_MBPS:
      *blePhy = BLE5_2M_PHY;
    break;
    case LL_PHY_CODED:
      *blePhy = BLE5_CODED_PHY;
    break;
    default:
      /* Shouldn't be here! */
      return FALSE;
    break;
  }
  return TRUE;
}

/*******************************************************************************
 * @fn          llConvertLlPhyOptToBlePhyOpt
 *
 * @brief       This routine is used to convert the LL PHY Opt values to BLE PHY
 *              Opt values.
 *
 * input parameters
 *
 * @param       llPhyOpt - LL PHY Options value.
 *
 * output parameters
 *
 * @param       blePhy  - Pointer to BLE Phy Options value.
 *
 * @return      TRUE  - in case the convert was successful.
 *              FALSE - in case the convert was not successful.
 */
/********************************************************************************/
uint8 llConvertLlPhyOptToBlePhyOpt(uint8 llPhyOpt, uint8 *blePhyOpt)
{
  switch (llPhyOpt)
  {
    case LL_PHY_OPT_NONE:
      *blePhyOpt = BLE5_CODING_NONE;
    break;
    case LL_PHY_OPT_S2:
      *blePhyOpt = BLE5_CODED_S2_DEFAULT;
    break;
    case LL_PHY_OPT_S8:
      *blePhyOpt = BLE5_CODED_S8_DEFAULT;
    break;
    default:
      /* Shouldn't be here! */
      return FALSE;
    break;
  }
  return TRUE;
}

/*******************************************************************************
 * @fn          llSetPhy
 *
 * @brief       This routine is used to handle the PHY. It:
 *              1. Converts the BLE PHY input value into LL PHY value,
 *                 and updates the connPtr->phyInfo.curPhy.
 *              2. Updates the MaxTxTime according to PHY coding.
 *              3. Sets the PHY value into a relevant RF command.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to BLE LL Connection.
 * @param       blePhy  - BLE Phy value. This value represents:
 *                        1. Phy value in the Set_Phy Procedure PDU
 *                        2. Phy value in the adv packet
 *
 * output parameters
 *
 * @return      TRUE  - in case the set to RF command was successful.
 *              FALSE - in case the set to RF command was not successful.
 */
/********************************************************************************/
uint8 llSetPhy(llConnState_t *connPtr, uint8 blePhy)
{
  llConvertBlePhyToLlPhy(blePhy, &connPtr->phyInfo.curPhy, &connPtr->phyInfo.phyOpts);

  // set max Tx Time and max Remote Tx Time to max coded/uncoded values
  connPtr->lenInfo.connMaxTxTime       = (connPtr->phyInfo.curPhy == LL_PHY_CODED) ? connInitialMaxTxTimeCoded : connInitialMaxTxTimeUncoded;
  connPtr->lenInfo.connRemoteMaxTxTime = (connPtr->phyInfo.curPhy == LL_PHY_CODED) ? connInitialMaxTxTimeCoded : connInitialMaxTxTimeUncoded;

  return RfBleDpl_setConnPhy(connPtr->connId, connPtr->phyInfo.curPhy, connPtr->phyInfo.phyOpts);
}

/*******************************************************************************
 * @fn          llSetRangeDelay
 *
 * @brief       This routine calculates and sets the rangeDelay value into the RF
 *              command. The calculation is based on PHY coding.
                Note: RfBleDpl_setRangeDelay is a dummy placement for RCL
 *
 * input parameters
 *
 * @param       connPtr - Pointer to BLE LL Connection.
 *
 * output parameters
 *
 * @return      None
 */
/********************************************************************************/
void llSetRangeDelay(llConnState_t *connPtr)
{
  uint8 rangeDelay = (connPtr->phyInfo.curPhy == LL_PHY_CODED) ? LL_CODED_RANGE_DELAY_RAT_TICKS : LL_UNCODED_RANGE_DELAY_RAT_TICKS;
  RfBleDpl_setRangeDelay(connPtr->connId, rangeDelay );
}

/*******************************************************************************
 * @fn          llSetPower
 *
 * @brief       This routine sets the TxPower value into the RF
 *              command, according to device type.
 *
 * input parameters
 *
 * @param       pRfCmd - Pointer to BLE RF command to be updated.
 * @param       txPowerIdx - Index to the TxPower table.
 * @param       txPower - txPower value. evaluates to RCL_Command_TxPower for
 *                        CC23XX device family and to uint8 for all other devices
 *
 * output parameters
 *
 * @return      None
 */
/********************************************************************************/
void llSetPower(uint32 *pRfCmd, RFBLEDPL_TX_POWER_TYPE txPowerIdx, RFBLEDPL_TX_POWER_HW_TYPE txPower)
{
  // set the Tx power based on initiator setting
  RfBleDpl_setTxPower(pRfCmd, txPower);
}

/*******************************************************************************
 * @fn          llGetTxPower
 *
 * @brief       This function is used to get the current Tx power in dBm.
 *
 * input parameters
 * @param       None.
 *
  * output parameters
 * @param       txPower               - Tx Power Value in dBm
 */
int8 llGetTxPower( void )
{
  return RfBleDpl_getTxPowerDbm(curTxPowerVal);
}

/*******************************************************************************
 * @fn          llValidateConnectIndPkt
 *
 * @brief       This function is used to check if the CONNECT_IND packet has valid parameters.
 *
 * input parameters
 * @param       pData - the CONNECT_IND packet data.
 *
 * output parameters
 *
 * @return      TRUE  - in case the CONNECT_IND parameters are valid.
 *              FALSE - in case one of the CONNECT_IND parameters are not valid.
 */
uint8 llValidateConnectIndPkt( uint8 *pData )
{
  uint8 *chanMap = ((uint8 *)(pData + LL_CONN_IND_CHANNEL_MAP_OFFSET));
  uint32 connAccessAddr = *(uint32 *)( pData + LL_CONN_IND_ACCESS_ADDRESS_OFFSET );
  uint16 connInterval = *(uint16 *)( pData + LL_CONN_IND_INTERVAL_OFFSET );
  uint16 transmitWinOffset = *(uint16 *)( pData + LL_CONN_IND_TRANSMIT_WINDOW_OFFSET );
  uint16 connTimeout = *(uint16 *)( pData + LL_CONN_IND_TIMEOUT_OFFSET );
  uint16 connSlaveLatency = *(uint16 *)( pData + LL_CONN_IND_LATENCY_OFFSET );

  // verify the connect indication
  if (( connAccessAddr == 0 )                                                  ||
      ( connInterval == 0)                                                     ||
      ((chanMap[LL_NUM_BYTES_FOR_CHAN_MAP-1] & ~0x1F) != 0)                    ||
      (MAP_llAtLeastTwoChans( chanMap ) != TRUE)                               ||
      ((connTimeout << 4) <= (( 1 + connSlaveLatency) * ( connInterval << 2))) ||
      ( transmitWinOffset > connInterval )                                     ||
      (connTimeout > LL_MAX_SUPERVISION_TIMEOUT )                              ||
      (connTimeout < LL_MIN_SUPERVISION_TIMEOUT ))
  {
    return FALSE;
  }

  return TRUE;
}

/*******************************************************************************
 * @fn          llUpdateRxBuffersForActiveConnections
 *
 * @brief       This function is used to update the Rx Buffer pointers in the link parameters
 *              for each active connection.
 *              This function should be called after every update of the RX Buffers, caused by any connection,
 *              Such as after RCL_MultiBuffer_RxEntry_get() and llClearRxDataEntry().
 *              The update may occur after re-allocation of the buffers (like in llReplaceRxBuffers()).
 *              IMPORTANT NOTE: This function should be called when NO command is running. Meaning,
 *              current command has already completetd, while the next command is NOT scheduled yet.
 *              Meaning, no command should be in active/scheduled status.
 *
 * input parameters
 * @param       rxBuffers - Pointer to the head of the MultiBuffer list.
 * @param       tail - Pointer to the tail of the MultiBuffer list.
 *
  * output parameters
 */
void llUpdateRxBuffersForActiveConnections(List_List *rxBuffers)
{
  uint8 curConnId = 0;
  llConnState_t *connPtr;

  // Update the shared rx buffers for all active connections
  do
  {
    connPtr = MAP_llDataGetConnPtr( curConnId );

    if (connPtr->activeConn)
    {
      linkParam[curConnId].rxBuffers.head = rxBuffers->head;
      linkParam[curConnId].rxBuffers.tail = rxBuffers->tail;
	}

    curConnId++;
  } while (curConnId < maxNumConns);
}

/*******************************************************************************
 * @fn          llGetCsConnTaskID
 *
 * @brief       This function is used to search the device connection role of
 *              the connection running with an active CS procedure
 *
 * input parameters
 * @param       None
 *
 * output parameters
 * @param		None.
 *
 * @return 	    Connection Handle
 */
uint16 llGetCsConnTaskID( void )
{
  int i;

  for (i=0; i<maxNumConns; i++)
  {
    if ( llConns.llConnection[i].allocConn == TRUE )
    {
      if ( llConns.llConnection[i].connPriority == LL_QOS_CS_PRIORITY )
      {
        return llConns.llConnection[i].llTask->taskID;
      }
    }
  }
  return LL_TASK_ID_NONE;
}


/*******************************************************************************
 * @fn          llSetTxPower
 *
 * @brief       This routine is used to save the TX Power value provided by user,
 *              after it was already transformed into Power value
 *              according to the Power table. It will later be set per each command separately.
 *
 * input parameters
 * @param       txPower     - Tx Power .
 * output parameters
 * @param       None.
 *
 * @return      None.
 */
void llSetTxPower( RFBLEDPL_TX_POWER_TYPE txPower )
{
  // save the Tx Power setting
  curTxPowerVal = txPower;

  return;
}

/*******************************************************************************
 * @fn          RfBleDpl_setConnPhy
 *
 * @brief       This routine is used to set PHY value into RCL RF command.
                LL phy and phyOpts values are converted into RF coded PHY values
                and then set into the RF command.
 *
 * input parameters
 *
 * @param       connId  - The ID of the connection for RF Command update.
 * @param       phy     - LL Phy value.
 * @param       phyOpts - LL Phy Options value.
 *
 * output parameters
 *
 * @return      TRUE  - in case the set to RF command was successful.
 *              FALSE - in case the set to RF command was not successful.
 */
/********************************************************************************/
uint8 RfBleDpl_setConnPhy(uint8 connId, uint8 phy, uint8 phyOpts)
{
  uint16_t phyFeatures;
  switch (phy)
  {
    case LL_PHY_1_MBPS:
      // For 1 MBPS, use .phyFeatures = RCL_BLE_PHY_FEATURE_PHY_1MBPS,
      phyFeatures = llUserConfig.rclPhyFeature1MBPS;
    break;

    case LL_PHY_2_MBPS:
      // For 2 Mbps, use .phyFeatures = RCL_BLE_PHY_FEATURE_PHY_2MBPS,
      phyFeatures = llUserConfig.rclPhyFeature2MBPS;
    break;

    case LL_PHY_CODED:
    {
      switch (phyOpts)
      {
        case LL_PHY_OPT_S2:
          // For coded with S=2 (500 kbps) in TX , use .phyFeatures = RCL_BLE_PHY_FEATURE_PHY_CODED | RCL_BLE_PHY_FEATURE_CODING_S2,
          phyFeatures = llUserConfig.rclPhyFeatureCoded  | llUserConfig.rclPhyFeatureCodedS2;
        break;
        case LL_PHY_OPT_S8:
          // For coded with S=8 (125 kbps) in TX , use .phyFeatures = RCL_BLE_PHY_FEATURE_PHY_CODED | RCL_BLE_PHY_FEATURE_CODING_S8,
          phyFeatures = llUserConfig.rclPhyFeatureCoded  | llUserConfig.rclPhyFeatureCodedS8;
        break;
        default:
          // the default value of aux_Conn_req is S8 coded
          phyFeatures = llUserConfig.rclPhyFeatureCoded  | llUserConfig.rclPhyFeatureCodedS8;
      }
    }
    break;
    default:
      /* Shouldn't be here */
      return FALSE;
    break;
  }

  linkCmd[connId].common.phyFeatures = phyFeatures;

  return TRUE;
}

/*******************************************************************************
 * @fn          llPhyToPhyFeatures
 *
 * @brief       Get the PHY features based on primary and secondary PHY.
 *
 * input parameters
 *
 * @param       primPhy - Primary PHY used for advertising.
 * @param       secPhy - Secondary PHY used for advertising.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      The PHY features based on the primary and secondary PHY.
 */
uint16 llPhyToPhyFeatures(uint8 primPhy, uint8 secPhy)
{
  uint16_t phyFeatures = 0;

  switch (primPhy)
  {
    case AE_PHY_1_MBPS:
      phyFeatures = llUserConfig.rclPhyFeature1MBPS;
      break;

    case AE_PHY_2_MBPS:
      phyFeatures = llUserConfig.rclPhyFeature2MBPS;
      break;

    case AE_PHY_CODED:
      phyFeatures = llUserConfig.rclPhyFeatureCoded | llUserConfig.rclPhyFeatureCodedS8;
      break;

    case AE_PHY_CODED_S2:
      phyFeatures = llUserConfig.rclPhyFeatureCoded | llUserConfig.rclPhyFeatureCodedS2;
      break;

    default:
      return FALSE;
  }

  if (secPhy == AE_PHY_CODED)
  {
    phyFeatures |= (uint16)llUserConfig.rclPhyFeatureCodedS8;
  }
  else if (secPhy == AE_PHY_CODED_S2)
  {
    phyFeatures |= (uint16)llUserConfig.rclPhyFeatureCodedS2;
  }

  return phyFeatures;
}

/*******************************************************************************
 * @fn          llAuxPhyFeatures
 *
 * @brief       Get the auxiliary PHY features based on the secondary PHY.
 *
 * input parameters
 *
 * @param       secPhy - Secondary PHY used for advertising.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      The auxiliary PHY features based on the secondary PHY.
 */
uint16 llAuxPhyFeatures(uint8 secPhy)
{
  uint16 auxPhyFeature;
  if ( (secPhy == AE_PHY_1_MBPS) ||
       (secPhy == AE_PHY_2_MBPS) )
  {
    auxPhyFeature = secPhy - 1;
  }
  else if(secPhy == AE_PHY_CODED_S2) // Coded
  {
    auxPhyFeature = BLE5_CODED_PHY | llUserConfig.rclPhyFeatureCodedS2;
  }
  else
  {
    auxPhyFeature = BLE5_CODED_PHY;
  }
  return auxPhyFeature;
}

/*******************************************************************************
 * @fn          RfBleDpl_setAdvPhy
 *
 * @brief       This routine is used to set PHY value into RCL Adv RF command.
                The RCL use phyFeatures to get the primary channel phy.
                The RCL takes the secondary channel phy from the AuxPhy inside
                the AuxPtr inside the ADV_EXT_IND pkt, but it doesn't include
                the type of Coded Phy, so in case the Secondary phy is coded,
                it will take the Coded type from PhyFeatures.

                Note: RCL doesn't support switching between Coded S2 and Coded S8
                in the extended advertising process.
 *
 * input parameters
 *
 * @param       pRfCmd  - Pointer to the RF command.
 * @param       primPhy - Primary channel Phy
 * @param       secPhy  - secondary channel Phy
 *
 * output parameters
 *
 * @return      TRUE  - in case the set to RF command was successful.
 *              FALSE - in case the set to RF command was not successful.
 */
/********************************************************************************/
uint8 RfBleDpl_setAdvPhy(void *pRfCmd, uint8 primPhy, uint8 secPhy)
{
  uint16_t phyFeatures = llPhyToPhyFeatures(primPhy, secPhy);

  ((RCL_Command *)pRfCmd)->phyFeatures = phyFeatures;

  return TRUE;
}

/*******************************************************************************
 * @fn          RfBleDpl_setRangeDelay
 *
 * @brief       This is a dummy placement routine in RCL RF command.
 *              rangeDelay is irrelevant in RCL
 *
 * input parameters
 *
 * @param       connId     - The ID of the connection for RF Command update.
 * @param       rangeDelay - rangeDelay value to set into the Command.
 *
 * output parameters
 *
 * @return      none
 */
/********************************************************************************/
void RfBleDpl_setRangeDelay(uint8 connId, uint8 rangeDelay)
{
  /* LEFT EMPTY INTENTIONALLY */
}

#define SW_TX_POWER_TABLE (llUserConfig.lrfTxPowerTablePtr)

void RfBleDpl_setTxPower(uint32 *pRfCmd, RFBLEDPL_TX_POWER_HW_TYPE txPower)
{
  uint16 cmdId = ((RCL_Command *)pRfCmd)->cmdId;

  switch( cmdId )
  {

    case RCL_CMDID_BLE5_ADVERTISER:
    {
        ((RCL_CmdBle5Advertiser *)pRfCmd)->txPower = txPower;
        break;
    }

    case RCL_CMDID_BLE5_INITIATOR:
    {
        ((RCL_CmdBle5Initiator *)pRfCmd)->txPower = txPower;
        break;
    }

    case RCL_CMDID_BLE5_SCANNER:
    {
        ((RCL_CmdBle5Scanner *)pRfCmd)->txPower = txPower;
        break;
    }

    case RCL_CMDID_BLE5_CONNECTION:
    {
        ((RCL_CmdBle5Connection *)pRfCmd)->txPower = txPower;
        break;
    }

    case RCL_CMDID_BLE5_AUX_ADV:
    {
        ((RCL_CmdBle5AuxAdvertiser *)pRfCmd)->txPower = txPower;
        break;
    }

    case RCL_CMDID_BLE5_PERIODIC_ADV:
    {
      ((RCL_CmdBle5PeriodicAdvertiser *)pRfCmd)->txPower = txPower;
      break;
    }

    case RCL_CMDID_BLE5_PERIODIC_SCAN:
    {
      ((RCL_CmdBle5PeriodicScanner *)pRfCmd)->txPower = txPower;
      break;
    }

    default:
      break;
  }
}

RFBLEDPL_TX_POWER_TYPE RfBleDpl_getTxPowerByTxPowerDbm(int8 txPowerDbm, uint8 fraction)
{
  RCL_Command_TxPower inPower = { .dBm = txPowerDbm, .fraction = fraction };

  return LRF_TxPowerTable_findValue(SW_TX_POWER_TABLE, inPower).power;
}

RFBLEDPL_TX_POWER_TYPE RfBleDpl_getTxPowerMax()
{
  return LRF_TxPowerTable_findValue(SW_TX_POWER_TABLE, LRF_TxPower_Use_Max).power;
}

RFBLEDPL_TX_POWER_TYPE RfBleDpl_getTxPowerMin()
{
  return LRF_TxPowerTable_findValue(SW_TX_POWER_TABLE, LRF_TxPower_Use_Min).power;
}

uint8 RfBleDpl_getNumTxPwrVals()
{
  return SW_TX_POWER_TABLE->numEntries;
}

RFBLEDPL_TX_POWER_HW_TYPE RfBleDpl_getTxPowerDefaultIdx()
{
  return RfBleDpl_getTxPowerByTxPowerDbm(llUserConfig.defaultTxPowerDbm, llUserConfig.defaultTxPowerFraction);
}

RFBLEDPL_TX_POWER_HW_TYPE RfBleDpl_getTxPower(RFBLEDPL_TX_POWER_TYPE txPower)
{
  return txPower;
}

int8 RfBleDpl_getTxPowerDbm(RFBLEDPL_TX_POWER_TYPE txPower)
{
  return txPower.dBm;
}

bool RfBleDpl_txPowerIsValid(RFBLEDPL_TX_POWER_TYPE txPower)
{
  if (txPower.rawValue == LRF_TxPower_None.rawValue)
  {
    return FALSE; /* Failed to find tx power value that fits the requested txPower */
  }
  else
  {
    return TRUE; /* Success */
  }
}

/*********************************************************************
 * @fn      llBleToRfChannel
 *
 * @brief   This function used to convert BLE channel to RF channel.
 *          The conversion will be convert according to this table:
 *          BLE channel         ->          RF channel
 *          0, 1, .. 10         ->          2404, 2406, ... 2424
 *          11, 12, ..36        ->          2428, 2430, ... 2479
 *
 * input parameters
 *
 * @param   bleChannel          -   channel in range 0 - 36 to be convert.
 *
 * output parameters
 *
 * @param   None
 *
 * @return  rf Channel in MHZ units.
 */
uint16 llBleToRfChannel(uint8 bleChannel)
{
    // number of adv channel before the current channel
    uint8 numOfAdvChan = 0;

    // set the rf Channel to th base rf channel
    uint16 rfChannel = LL_FIRST_RF_CHAN_FREQ; // 2402 MHz

    // verify that the channal is valid
    if (bleChannel < LL_TOTAL_NUM_RF_CHAN)
    {
        // there is 1 adv channel before the 0-10 BLE channels
        if (rfChannel < LL_BLE_CHANNEL_11)
        {
            numOfAdvChan = LL_ONE_ADV_CHANNEL;
        }

        // there is 2 adv channel before the 0-10 BLE channels
        else
        {
            numOfAdvChan = LL_TWO_ADV_CHANNEL;
        }

        // add the difference between the first and current RF channels to
        // to the base rf channel
        rfChannel += (bleChannel + numOfAdvChan) * LL_RF_FREQ_HOP;
    }

    return rfChannel;
}

/*******************************************************************************
 * @fn          llSetupCtrlPkt
 *
 * @brief       This function setup to generic part of the control packet and
 *              call to more specific function for each case
 *
 * input parameters
 *
 * @param       connPtr   - Pointer to the current connection
 * @param       ctrlPkt   - Control packet opcode
 *
 * output parameters
 *
 * @param   TRUE/FALSE if setup success
 */
uint8_t llSetupCtrlPkt(llConnState_t *connPtr, uint8_t ctrlPkt)
{
  uint8_t status = TRUE; // TODO: use error code

  RCL_Buffer_TxBuffer *pLocalTxBufferHead = RCL_TxBuffer_head(
      ((txDataQ_t*) connPtr->pTxDataEntryQ)->rfDataBuffers );

  if ( ctrlPkt > NUM_OF_CTRL_PKT )
  {
    status = FALSE;
  }

  // Before setup ENC/PAUSE_ENC control packet the TX should be empty
  else if ( ((ctrlPkt == LL_CTRL_ENC_RSP)        ||
             (ctrlPkt == LL_CTRL_ENC_REQ)        ||
             (ctrlPkt == LL_CTRL_PAUSE_ENC_REQ)  ||
             (ctrlPkt == LL_CTRL_PAUSE_ENC_RSP)) &&
             (pLocalTxBufferHead != NULL)         )
  {
    status = FALSE;
  }

  else
  {
    uint8_t ctrlpktLen = ctrlPktLenTable[ctrlPkt];

    uint8_t encPkt = llEncryptControlPkt( connPtr, ctrlPkt );

    uint8_t dataEntryOffset = sizeof(RCL_Buffer_TxBuffer)     +
                              RCL_BUFFER_MAX_HEADER_PAD_BYTES +
                              LL_PKT_HDR_LEN;

    // Allocate a data entry and payload to send control packet
    uint8_t *pData = MAP_LL_TX_bm_alloc( ctrlpktLen );

    // Check if we have a data entry
    if ( pData != NULL )
    {
      RCL_Buffer_TxBuffer *dataEntry = (RCL_Buffer_TxBuffer*) (pData
          - dataEntryOffset);

      llSetupDataEntry( dataEntry, ctrlpktLen, encPkt );

      // Point to data
      pData = &dataEntry->data[LL_PKT_DATAENTRY_DATA_OFFSET];

      // Populate data entry depend on opcode
      llBuildCtrlPkt( connPtr, pData, ctrlPkt );

      // Encrypt TX packet in place in the TX FIFO
      if ( encPkt )
      {
        // Encrypt PDU with authentication check
        MAP_LL_ENC_Encrypt( connPtr,
                            LL_DATA_PDU_HDR_LLID_CONTROL_PKT,
                            ctrlpktLen,
                            pData );
      }

      // Queue it on connection TX list and queue for RF
      MAP_llAddTxDataEntry( connPtr->pTxDataEntryQ, dataEntry );

      // Handle action and database update post setup
      llPostSetupCtrlPkt( connPtr, ctrlPkt );
    }

    // Data allocation failed
    else
    {
      status = FALSE;
    }

  }

  return (status);
}

/*******************************************************************************
 * @fn          llBuildCtrlPkt
 *
 * @brief       This function setup control packet
 *
 * input parameters
 *
 * @param       connPtr   - Pointer to the current connection
 * @param       pData     - Pointer to array to be fill
 * @param       ctrlPkt   - Control packet opcode
 *
 * @return      None
 *
 * @Output      pData - Packet's data payload
 */
static inline void llBuildCtrlPkt(llConnState_t *connPtr, uint8_t *pData,
                                  uint8_t ctrlPkt)
{
  // Write control opcode
  *pData++ = ctrlPkt;

  switch ( ctrlPkt )
  {
    case LL_CTRL_TERMINATE_IND:
    {
      llSetupTermInd( connPtr, pData );
      break;
    }
    case LL_CTRL_UNKNOWN_RSP:
    {
      llSetupUnknownRsp( connPtr, pData );
      break;
    }
    case LL_CTRL_FEATURE_REQ:
    case LL_CTRL_PERIPHERAL_FEATURE_REQ:
    {
      llSetupFeatureSetReq( connPtr, pData );
      break;
    }
    case LL_CTRL_FEATURE_RSP:
    {
      llSetupFeatureSetRsp( connPtr, pData );
      break;
    }
    case LL_CTRL_VERSION_IND:
    {
      llSetupVersionIndReq( connPtr, pData );
      break;
    }
    case LL_CTRL_CONNECTION_PARAM_REQ:
    case LL_CTRL_CONNECTION_PARAM_RSP:
    {
      llSetupConnParam( connPtr, pData, ctrlPkt);
      break;
    }
    case LL_CTRL_REJECT_EXT_IND:
    {
      llSetupRejectIndExt( connPtr, pData );
      break;
    }
    case LL_CTRL_LENGTH_REQ:
    case LL_CTRL_LENGTH_RSP:
    {
      llSetupLenCtrlPkt( connPtr, pData );
      break;
    }
    case LL_CTRL_PHY_REQ:
    case LL_CTRL_PHY_RSP:
    case LL_CTRL_PHY_UPDATE_REQ:
    {
      llSetupPhyCtrlPkt( connPtr, pData );
      break;
    }
    default:
    {
      // Two functions handle relevant cases for specific roles.
      // if a role is not enabled, the functions link to an empty function.
      MAP_llBuildCtrlPktPeri( connPtr, pData, ctrlPkt );
      MAP_llBuildCtrlPktCent( connPtr, pData, ctrlPkt );
      break;
    }
  }
}

/*******************************************************************************
 * @fn          llBuildCtrlPktPeri
 *
 * @brief       This function setup control packet that relevant to
 *              peripheral role only.
 *
 * input parameters
 *
 * @param       connPtr   - Pointer to the current connection
 * @param       pData     - Pointer to array to be fill
 * @param       ctrlPkt   - Control packet opcode
 *
 * @Output      pData - Packet's data payload
 */
void llBuildCtrlPktPeri(llConnState_t *connPtr, uint8_t *pData, uint8_t ctrlPkt)
{
  switch ( ctrlPkt )
  {
    case LL_CTRL_ENC_RSP:
    {
      llSetupEncRsp( connPtr, pData );
      break;
    }
    case LL_CTRL_REJECT_IND:
    {
      llSetupRejectInd( connPtr, pData );
      break;
    }
    default:
    {
      break;
    }
  }
}

/*******************************************************************************
 * @fn          llBuildCtrlPktCent
 *
 * @brief       This function setup control packet that relevant to
 *              central role only.
 *
 * input parameters
 *
 * @param       connPtr   - Pointer to the current connection
 * @param       pData     - Pointer to array to be fill
 * @param       ctrlPkt   - Control packet opcode
 *
 * @Output      pData - Packet's data payload
 */
void llBuildCtrlPktCent(llConnState_t *connPtr, uint8_t *pData, uint8_t ctrlPkt)
{
  switch ( ctrlPkt )
  {
    case LL_CTRL_CONNECTION_UPDATE_IND:
    {
      llSetupUpdateParamReq( connPtr, pData );
      break;
    }
    case LL_CTRL_CHANNEL_MAP_IND:
    {
      llSetupUpdateChanReq( connPtr, pData );
      break;
    }
    case LL_CTRL_ENC_REQ:
    {
      llSetupEncReq( connPtr, pData );
      break;
    }
    default:
    {
      break;
    }
  }
}

/*******************************************************************************
 * @fn          llPostSetupCtrlPkt
 *
 * @brief       This function handles tasks after adding packets to the TX FIFO
 *
 * input parameters
 *
 * @param       connPtr   - Pointer to the current connection
 * @param       ctrlPkt   - Control packet opcode
 *
 */
static inline void llPostSetupCtrlPkt(llConnState_t *connPtr, uint8_t ctrlPkt)
{

  if ( ctrlPkt == LL_CTRL_TERMINATE_IND )
  {
    // deactivate peripheral latency, if it was enabled
    // Note: Not used by Central.
    connPtr->peripheralLatency = 0;
    // Per Core V4.0 spec change, the termination timeout is the LSTO
    connPtr->ctrlPktInfo.ctrlTimeout = connPtr->expirationValue;
  }

  // Our device is initiating the Length control procedure
  else if ( ctrlPkt == LL_CTRL_LENGTH_REQ )
  {
    // it is, so set the Max Tx Octets to the mininum
    // Note: This control procedure could result in a reduced OTA Tx packet
    //       size, regardless of the value of connMaxTxOctets, since the
    //       peer's connRemoteMaxRxOctets value can cap the max allowed
    //       data size. It is unknown at this point what the resulting max
    //       Tx packet size will be. So it is possible that this device
    //       could end up queuing Tx packets with lengths that exceed the
    //       peer's Rx buffer. To prevent this from happening, this device
    //       must begin fragmenting to ensure no packet larger then the
    //       minimum OTA size is sent. When the control procedure completes,
    //       connActualMaxTxOctets will be updated accordingly.
    //
    //       For example:
    //       A 200ms connection is currently using an OTA Tx size of 251.
    //       The Peripheral sends a Length Request with MD=0 to change it to 27.
    //       The connection event ends, and the Host queues up 20 packets
    //       of size 251. No fragmentation occurs because the current OTA
    //       size is 251. The next connection event begins and the peer's
    //       Rx buffer size is too small, so it NACKs our device. Deadlock.
    connPtr->lenInfo.connActualMaxTxOctets = LL_MIN_LINK_DATA_LEN;
  }

  else
  {
    //Two functions handle relevant cases for specific roles.
    //If a role is not enabled, the functions link to an empty function.
    MAP_llPostSetupCtrlPktPeri( connPtr, ctrlPkt );
    MAP_llPostSetupCtrlPktCent( connPtr, ctrlPkt );
  }

  // This section specifies procedure timeout rules that shall be applied to all the
  // Link Layer control procedures, except for the
  // Connection Update and Channel Map Update procedures for which there are
  // no timeout rules.
  if ( ctrlPkt != LL_CTRL_CONNECTION_UPDATE_IND
      && ctrlPkt != LL_CTRL_CHANNEL_MAP_IND )
  {
    // set the control packet timeout for 40s relative to our present time
    // Note: This is done in terms of connection events there is convert function called
    // llConvertCtrlProcTimeoutToEvent that convert 40s to connection events.
    connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;
  }

}

/*******************************************************************************
 * @fn          llPostSetupCtrlPktPeri
 *
 * @brief       This function handles tasks after adding packets to the TX FIFO
 *              Note: For cases that relevant to peripheral role only
 *
 * input parameters
 *
 * @param       connPtr   - Pointer to the current connection
 * @param       ctrlPkt   - Control packet opcode
 *
 */
void llPostSetupCtrlPktPeri(llConnState_t *connPtr, uint8_t ctrlPkt)
{

  if ( (ctrlPkt == LL_CTRL_REJECT_EXT_IND)       ||
       (ctrlPkt == LL_CTRL_PING_REQ)             ||
       (ctrlPkt == LL_CTRL_PING_RSP)             ||
       (ctrlPkt == LL_CTRL_CONNECTION_PARAM_RSP) ||
       (ctrlPkt == LL_CTRL_CONNECTION_PARAM_REQ) ||
       (ctrlPkt == LL_CTRL_FEATURE_RSP)           )
  {
    // deactivate peripheral latency, if it was enabled
    connPtr->peripheralLatency = 0; // TODO: align with spec, set zero to peripheralLatency shall be only for pdu with instant field
  }

  else if ( ctrlPkt == LL_CTRL_ENC_RSP )
  {
    // bytes are generated LSO..MSO, but need to be maintained as
    // MSO..LSO, per FIPS 197 (AES), so reverse the bytes
    MAP_LL_ENC_ReverseBytes(
        (uint8_t*) &connPtr->encInfo.SKD[LL_ENC_SKD_S_OFFSET],
        LL_ENC_SKD_S_LEN );

    // bytes are generated LSO..MSO, but need to be maintained as
    // MSO..LSO, per FIPS 197 (AES), so reverse the bytes
    // ALT: Maintain the IV in LSO..MSO order as the Nonce is formed that way.
    MAP_LL_ENC_ReverseBytes(
        (uint8_t*) &connPtr->encInfo.IV[LL_ENC_IV_S_OFFSET],
        LL_ENC_IV_S_LEN );

    // place the IV into the Nonce to be used for this connection
    // Note: If a Pause Encryption control procedure is started, the
    //       old Nonce value will be used until encryption is disabled.
    // Note: The IV is sequenced LSO..MSO within the Nonce.
    // ALT: Maintain the IV in LSO..MSO order as the Nonce is formed that way.
    for ( uint8_t i = 0; i < LL_ENC_IV_LEN; i++ )
    {
      connPtr->encInfo.nonce[ LL_ENC_NONCE_IV_OFFSET + i] =
          connPtr->encInfo.IV[(LL_ENC_IV_LEN - i) - 1];
    }
  }

}

/*******************************************************************************
 * @fn          llPostSetupCtrlPktCent
 *
 * @brief       This function handles tasks after adding packets to the TX FIFO
 *              Note: For cases that relevant to central role only
 *
 * input parameters
 *
 * @param       connPtr   - Pointer to the current connection
 * @param       ctrlPkt   - Control packet opcode
 *
 */
void llPostSetupCtrlPktCent(llConnState_t *connPtr, uint8_t ctrlPkt)
{

  if ( ctrlPkt == LL_CTRL_ENC_REQ )
  {
    // bytes are generated LSO..MSO, but need to be maintained as
    // MSO..LSO, per FIPS 197 (AES), so reverse the bytes
    MAP_LL_ENC_ReverseBytes(
        (uint8_t*) &connPtr->encInfo.SKD[LL_ENC_SKD_M_OFFSET],
        LL_ENC_SKD_M_LEN );

    // bytes are generated LSO..MSO, but need to be maintained as
    // MSO..LSO, per FIPS 197 (AES), so reverse the bytes
    // ALT: Maintain the IV in LSO..MSO order as the Nonce is formed that way.
    MAP_LL_ENC_ReverseBytes(
        (uint8_t*) &connPtr->encInfo.IV[LL_ENC_IV_M_OFFSET],
        LL_ENC_IV_M_LEN );
  }

}

/*******************************************************************************
 * @fn          llSetupFeatureSetRsp
 *
 * @brief       This function is used to setup the Feature Set Response packet.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 * @param       pData   - Pointer to array to be fill
 *
 * output parameters
 *
 * @Output      pData - Packet's data payload
 */
static inline void llSetupFeatureSetRsp(llConnState_t *connPtr, uint8_t *pData)
{
  uint8_t *pCurData = pData;
  // use this connection's feature set as payload
  // Note: Normally, this device's feature set would be used, but since there
  //       is a HCI Extension command that allows the user to change this
  //       device's feature set, and given that the Peripheral can now request a
  //       feature set procedure (V4.1 specification update), the used feature
  //       set could get changed.
  // Note: Per Vol 6, Part B, Section 5.1.4, only byte 0 of the peer's
  //       feature set is logically AND'ed with this device's feature set's
  //       byte 0. All remaining bytes are set based on this devices feature
  //       set.
  *pCurData++ = connPtr->featureSetInfo.featureSet[0];

  memcpy( pCurData,
          &deviceFeatureSet.featureSet[1],
          (LL_MAX_FEATURE_SET_SIZE - 1) );

  // If a bit is shown as Host Controlled,
  // the value may be set by the Host and shall default to zero.
  // this function shout down bits by the table 4.7 in:
  // BLUETOOTH CORE SPECIFICATION Version 5.4 | Vol 6, Part B page 2845.
  llRemoveFeaturesForSendToPeer( pData );
}

/*******************************************************************************
 * @fn          llSetupVersionIndReq
 *
 * @brief       This function is used to setup the version indication packet.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 * @param       pData   - Pointer to array to be fill
 *
 * output parameters
 *
 * @Output      pData - Packet's data payload
 */
static inline void llSetupVersionIndReq(llConnState_t *connPtr, uint8_t *pData)
{
  uint8_t *pCurData = pData;

  // Fill verNum field
  *pCurData++ = verInfo.verNum;

  size_t nextCmdSize = ( sizeof(verInfo.comId) +
                         sizeof(verInfo.subverNum) );

  uint8_t *pSrcAddress = (uint8_t*)&(verInfo.comId);

  // Fill comId field
  // Fill subverNum field
  memcpy( pCurData, pSrcAddress, nextCmdSize );
}

/*******************************************************************************
 * @fn          llSetupRejectInd
 *
 * @brief       This function is used to setup the encrypt reject
 *              indication packet.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 * @param       pData   - Pointer to array to be fill
 *
 * output parameters
 *
 * @Output      pData - Packet's data payload
 */
static inline void llSetupRejectInd(llConnState_t *connPtr, uint8_t *pData)
{
  // Fill encRejectErrCode field
  *pData = connPtr->encInfo.encRejectErrCode;
}

/*******************************************************************************
 * @fn          llSetupConnParam
 *
 * @brief       This function is used to setup the Connection Parameter
 *              Response/Request control procedure.
 *
 *              ALT: Determine if this was initiated by the Host or by the
 *                   Controller. If the Host, then could use values as follows,
 *                   but would need to determine if offsets and periodicity
 *                   need to be adjusted. If the Controller,
 *                   all values should be provided.
 *
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 * @param       pData   - Pointer to array to be fill
 *
 * output parameters
 *
 * @Output      pData - Packet's data payload
 */
static inline void llSetupConnParam(llConnState_t *connPtr, uint8_t *pData, uint8_t ctrlPkt)
{
  uint8_t *pCurData = pData;

  uint8_t nextCmdSize = ( sizeof(connPtr->connParams.intervalMin) +
                          sizeof(connPtr->connParams.intervalMax) +
                          sizeof(connPtr->connParams.latency)     +
                          sizeof(connPtr->connParams.timeout)     );

  uint8_t *pSrcAddress = (uint8_t*)&(connPtr->connParams);

  // Fill intervalMin field
  // Fill intervalMax field
  // Fill latency field
  // Fill timeout field
  memcpy( pCurData, pSrcAddress, nextCmdSize );

  pCurData += nextCmdSize;

  nextCmdSize = ( sizeof(connPtr->connParams.periodicity) +
                  sizeof(connPtr->connParams.refConnEvtCount) );

  // Fill zeroes to periodicity field
  // Fill zeroes to refConnEvtCount field
  memset( pCurData, 0, nextCmdSize );

  pCurData += nextCmdSize;

  // Size of 6 offsets fields (offset0-offset5)
  nextCmdSize = (6 * sizeof(connPtr->connParams.offset0));

  // Fill invalid to offset0-offset5
  memset( pCurData, 0xFF, nextCmdSize );

#ifdef LL_TEST_MODE
  if(ctrlPkt == LL_CTRL_CONNECTION_PARAM_RSP)
  {
    llSetupConnParamRsp_testmode( connPtr, pData );
  }
  if(ctrlPkt == LL_CTRL_CONNECTION_PARAM_REQ)
  {
    llSetupConnParamReq_testmode( connPtr, pData );
  }
#endif //LL_TEST_MODE
}

/*******************************************************************************
 * @fn          llSetupRejectIndExt
 *
 * @brief       This function is used to setup the extended reject indication
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 * @param       pData   - Pointer to array to be fill
 *
 * output parameters
 *
 * @Output      pData - Packet's data payload
 */
static inline void llSetupRejectIndExt(llConnState_t *connPtr, uint8_t *pData)
{
  uint8_t *pCurData = pData;

  // Fill the reject opcode
  *pCurData++ = connPtr->rejectIndExt.rejectOpcode;
  // Fill the reason code
  *pCurData = connPtr->rejectIndExt.errorCode;
}

/*******************************************************************************
 * @fn          llSetupPhyCtrlPkt
 *
 * @brief       This function is used to setup the phy request, response, or
 *              update control packet.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 * @param       pData   - Pointer to array to be fill
 *
 * output parameters
 *
 * @Output      pData - Packet's data payload
 */
static inline void llSetupPhyCtrlPkt(llConnState_t *connPtr, uint8_t *pData)
{
  uint8_t *pCurData = pData;

  // Fill the TX PHY
  // Note: This device only supports symmetric connections!
  *pCurData++ = connPtr->phyInfo.updatePhy;
  // Fill the RX PHY
  *pCurData++ = connPtr->phyInfo.updatePhy;

  if ( connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_PHY_UPDATE_REQ )
  {
    // Check whether an instant is required
    // Note: When no phy change is to occur, the instant shall be zero.
    if ( connPtr->phyUpdateEvent != 0 )
    {
      // convert relative instant number to an absolute event number
      connPtr->phyUpdateEvent += connPtr->currentEvent;
    }

#if defined( LL_TEST_MODE )
    if ( llTestMode.testCase == LL_TEST_MODE_TP_CON_SLA_BI_09 )
    {
      // override the update event to cause a Instant in Past failure
      connPtr->phyUpdateEvent = connPtr->currentEvent-1;
    }
#endif // LL_TEST_MODE

    // Fill the update event count
    *pCurData++ = LO_UINT16( connPtr->phyUpdateEvent );
    *pCurData = HI_UINT16( connPtr->phyUpdateEvent );
  }

}

/*******************************************************************************
 * @fn          llSetupLenCtrlPkt
 *
 * @brief       This function is used to setup the length request, response
 *              control packet.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 * @param       pData   - Pointer to array to be fill
 *
 * output parameters
 *
 * @Output      pData - Packet's data payload
 */
static inline void llSetupLenCtrlPkt(llConnState_t *connPtr, uint8_t *pData)
{
  // FILL the RX/TX lengths in octets, and time in us
  memcpy( pData,
          &connPtr->lenInfo,
          (LL_LENGTH_REQ_PAYLOAD_LEN - 1) );

#ifdef LL_TEST_MODE
  if ( llTestMode.testCase == LL_TEST_MODE_TP_CON_MAS_BI_07 )
  {
    // write invalid rx/tx lengths in octets, and time in us
    // note: invalid values were configure using LL_EXT_SetMaxDataLen under LL_TEST_MODE
    pData[0] = LO_UINT16(invalidRxOctets);
    pData[1] = HI_UINT16(invalidRxOctets);
    pData[2] = LO_UINT16(invalidRxTime);
    pData[3] = HI_UINT16(invalidRxTime);
    pData[4] = LO_UINT16(invalidTxOctets);
    pData[5] = HI_UINT16(invalidTxOctets);
    pData[6] = LO_UINT16(invalidTxTime);
    pData[7] = HI_UINT16(invalidTxTime);
  }
#endif
}

/*******************************************************************************
 * @fn          llSetupFeatureSetReq
 *
 * @brief       This function is used to setup the feature set request.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 * @param       pData   - Pointer to array to be fill
 *
 * output parameters
 *
 * @Output      pData - Packet's data payload
 */
static inline void llSetupFeatureSetReq(llConnState_t *connPtr, uint8_t *pData)
{
#ifdef LL_TEST_MODE
  switch( llTestMode.testCase )
  {
    case LL_TEST_MODE_TP_PAC_MAS_BV01:
    case LL_TEST_MODE_TP_PAC_SLA_BV01:
      // override opcode: send an invalid control packet opcode
      *(pData - 1) = LL_CTRL_INVALID_OPCODE; //Change opcode
      break;

    default:
      break;
  }
#endif // LL_TEST_MODE

  // use this connection's feature set as payload
  memcpy( pData,
          &(deviceFeatureSet.featureSet),
          LL_MAX_FEATURE_SET_SIZE );

  // If a bit is shown as Host Controlled,
  // the value may be set by the Host and shall default to zero.
  // this function shout down bits by the table 4.7 in:
  // BLUETOOTH CORE SPECIFICATION Version 5.4 | Vol 6, Part B page 2845.
  llRemoveFeaturesForSendToPeer( pData );
}

/*******************************************************************************
 * @fn          llSetupUpdateParamReq
 *
 * @brief       This function is used to setup the update parameter request.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 * @param       pData   - Pointer to array to be fill
 *
 * output parameters
 *
 * @Output      pData - Packet's data payload
 */
static inline void llSetupUpdateParamReq(llConnState_t *connPtr, uint8_t *pData)
{
  uint8_t *pCurData = pData;

  // Fill window size
  *pCurData++ = connPtr->paramUpdate.winSize;

  uint8_t nextCmdSize = ( sizeof(connPtr->paramUpdate.winOffset)         +
                          sizeof(connPtr->paramUpdate.connInterval)      +
                          sizeof(connPtr->paramUpdate.peripheralLatency) +
                          sizeof(connPtr->paramUpdate.connTimeout)       );

  uint8_t *pSrcAddress = (uint8_t*) &(connPtr->paramUpdate.winOffset);

  // Fill winOffset field
  // Fill connInterval field
  // Fill peripheralLatency field
  // Fill connTimeout field
  memcpy( pCurData, pSrcAddress, nextCmdSize );

  pCurData += nextCmdSize;

  // convert relative instant number to an absolute event number
  connPtr->paramUpdateEvent += connPtr->currentEvent +
                               (llConns.numActiveConns * LL_INSTANT_NUMBER_FACTOR);

#ifdef LL_TEST_MODE
  switch( llTestMode.testCase )
  {
    case LL_TEST_MODE_TP_CON_SLA_BI_04:
      // override paramUpdateEvent to cause a Passed Instant failure
      connPtr->paramUpdateEvent = connPtr->currentEvent-1;

      break;

    default:
      break;
  }
#endif // LL_TEST_MODE

  // Fill the update event count
  *pCurData++ = LO_UINT16( connPtr->paramUpdateEvent );
  *pCurData = HI_UINT16( connPtr->paramUpdateEvent );
}

/*******************************************************************************
 * @fn          llSetupUpdateChanReq
 *
 * @brief       This function is used to setup the update channel request.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 * @param       pData   - Pointer to array to be fill
 *
 * output parameters
 *
 * @Output      pData - Packet's data payload
 */
static inline void llSetupUpdateChanReq(llConnState_t *connPtr, uint8_t *pData)
{
  uint8_t *pCurData = pData;

  // Fill with the new channel map
  memcpy( pCurData,
          &(connPtr->curChanMap.chanMap),
          LL_NUM_BYTES_FOR_CHAN_MAP );

  pCurData += LL_NUM_BYTES_FOR_CHAN_MAP;

  // So convert relative instant number to an absolute event number
  connPtr->chanMapUpdateEvent += connPtr->currentEvent;

#ifdef LL_TEST_MODE
  switch( llTestMode.testCase )
  {
    case LL_TEST_MODE_TP_CON_SLA_BI_04:
      // override chanMapUpdateEvent to cause a Passed Instant failure
      connPtr->chanMapUpdateEvent = connPtr->currentEvent-1;
      break;

    case LL_TEST_MODE_JIRA_3646:
      // so convert relative instant number to an absolute event number
      connPtr->chanMapUpdateEvent++;
      break;

    default:
      break;
  }
#endif // LL_TEST_MODE

  // Fill the update event count field
  *pCurData++ = LO_UINT16( connPtr->chanMapUpdateEvent );
  *pCurData = HI_UINT16( connPtr->chanMapUpdateEvent );
}

/*******************************************************************************
 * @fn          llSetupTermInd
 *
 * @brief       This function is used to setup the termination indication.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 * @param       pData   - Pointer to array to be fill
 *
 * output parameters
 *
 * @Output      pData - Packet's data payload
 */
static inline void llSetupTermInd(llConnState_t *connPtr, uint8_t *pData)
{
  // Fill reason code
  *pData = connPtr->termInfo.reason;
}

/*******************************************************************************
 * @fn          llSetupEncReq
 *
 * @brief       This function is used to setup the start encryption request.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 * @param       pData   - Pointer to array to be fill
 *
 * output parameters
 *
 * @Output      pData - Packet's data payload
 */
static inline void llSetupEncReq(llConnState_t *connPtr, uint8_t *pData)
{
  uint8_t *pCurData = pData;

  // Fill the random vector field
  memcpy( pCurData,
          &(connPtr->encInfo.RAND),
          LL_ENC_RAND_LEN );

  pCurData += LL_ENC_RAND_LEN;

  // Fill the encryption diversifier field
  memcpy( pCurData,
          &(connPtr->encInfo.EDIV),
          LL_ENC_EDIV_LEN );

  pCurData += LL_ENC_EDIV_LEN;

  // Fill the central's session key diversifier field
  // Note: The SKDm LSO is the LSO of the SKD.
  memcpy( pCurData,
          &(connPtr->encInfo.SKD[LL_ENC_SKD_M_OFFSET]),
          LL_ENC_SKD_M_LEN );

  pCurData += LL_ENC_SKD_M_LEN;

  // Fill the central's initialization vector field
  // Note: The IVm LSO is the LSO of the IV.
  memcpy( pCurData,
          &(connPtr->encInfo.IV[LL_ENC_IV_M_OFFSET]),
          LL_ENC_IV_M_LEN );
}

/*******************************************************************************
 * @fn          llSetupEncRsp
 *
 * @brief       This function is used to setup the start encryption respond.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 * @param       pData   - Pointer to array to be fill
 *
 * output parameters
 *
 * @Output      pData - Packet's data payload
 */
static inline void llSetupEncRsp(llConnState_t *connPtr, uint8_t *pData)
{
  uint8_t *pCurData = pData;

  // Fill the SKDs payload field
  memcpy( pCurData,
          &(connPtr->encInfo.SKD[LL_ENC_SKD_S_OFFSET]),
          LL_ENC_SKD_S_LEN );

  pCurData += LL_ENC_SKD_S_LEN;

  // Fill the IVs payload field
  memcpy( pCurData,
          &(connPtr->encInfo.IV[LL_ENC_IV_S_OFFSET]),
          LL_ENC_SKD_S_LEN );
}
/*******************************************************************************
 * @fn          llSetupUnknownRsp
 *
 * @brief       This function is used to setup unknown packet respond.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 * @param       pData   - Pointer to array to be fill
 *
 * output parameters
 *
 * @Output      pData - Packet's data payload
 */
static inline void llSetupUnknownRsp(llConnState_t *connPtr, uint8_t *pData)
{
  // Fill unknown control type as payload field
  *pData = connPtr->unknownCtrlType;
}
/*******************************************************************************
 * @fn          llEncryptControlPkt
 *
 * @brief       Set if control packet should encrypt or not, base on
 *              connPtr->encEnabled value or override for control packet that
 *              shall sent in mode specific
 *
 * input parameters
 *
 * @param       connPtr   - Pointer to the current connection
 * @param       ctrlPkt   - Control packet opcode
 *
 */
static inline uint8_t llEncryptControlPkt(llConnState_t *connPtr,
                                          uint8_t ctrlPkt)
{
  uint8_t encPkt = FALSE;

  if ( ctrlPkt == LL_CTRL_PAUSE_ENC_RSP )
  {
    // Only the Peripheral encrypts the Pause Encryption Response
    if ( llState == LL_STATE_CONN_PERIPHERAL )
    {
      encPkt = TRUE;
    } //else FASLE
  }
  else if ( (ctrlPkt == LL_CTRL_START_ENC_RSP) ||
            (ctrlPkt == LL_CTRL_PAUSE_ENC_REQ) )
  {
    encPkt = TRUE;
  }
  else if ( (ctrlPkt == LL_CTRL_ENC_REQ)       ||
            (ctrlPkt == LL_CTRL_ENC_RSP)       ||
            (ctrlPkt == LL_CTRL_START_ENC_REQ) ||
            (ctrlPkt == LL_CTRL_REJECT_IND)     )
  {
    encPkt = FALSE;
  }
  else
  {
    encPkt = connPtr->encEnabled;
  }
  return (encPkt);
}

/*******************************************************************************
 * @fn          llCheckConnInstant
 *
 * @brief       This function check if this connection has a control procedure
 *              in progress
 *
 * input parameters
 *
 * @param       connPtr - Connection pointer
 *
 * output parameters
 *
 * @return FALSE if there isn't a control procedure active else TRUE.
 */
uint8 llCheckConnInstant(llConnState_t *connPtr)
{
  uint8_t isInstant = 0;
  isInstant = connPtr->pendingParamUpdate |
              connPtr->pendingChanUpdate  |
              connPtr->pendingPhyUpdate   |
              connPtr->pendingLenUpdate;
  isInstant |= connPtr->connParamReqFlags.connUpdateActive;
  isInstant |= connPtr->ctrlPktInfo.ctrlPktCount;

  return isInstant;
}

#ifdef LL_TEST_MODE
/*******************************************************************************
 * @fn          llSetupConnParamReq_testmode
 *
 * @brief       This function is used to setup the Connection Parameter
 *              Request control procedure for test mode.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 * @param       pData   - Pointer to array to be fill
 *
 * output parameters
 *
 * @Output      pData - Packet's data payload
 */
static inline void llSetupConnParamReq_testmode( llConnState_t *connPtr,
                                                 uint8_t *pData  )
{
  switch( llTestMode.testCase )
  {
    case LL_TEST_MODE_TP_CON_MAS_BV_28:
      // write the min connection interval
      pData[0] = LO_UINT16( 0x0006 );
      pData[1] = HI_UINT16( 0x0006 );

      // write the max connection interval
      pData[2] = LO_UINT16( 0x0006 );
      pData[3] = HI_UINT16( 0x0006 );

      // write the connection latency
      pData[4] = LO_UINT16( 0x0000 );
      pData[5] = HI_UINT16( 0x0000 );

      // write the connection timeout
      pData[6] = LO_UINT16( 0x012C );
      pData[7] = HI_UINT16( 0x012C );

      // write the preferred periodicity - invalid
      pData[8] = LO_UINT16( 0 );

      // write the reference connection event count - invalid
      pData[9] = LO_UINT16( connPtr->currentEvent );
      pData[10] = HI_UINT16( connPtr->currentEvent );

      break;

    case LL_TEST_MODE_TP_CON_MAS_BV_31_1:
    case LL_TEST_MODE_TP_CON_SLA_BV_30_1:
      // override Offset0 to 1.25ms
      pData[11] = LO_UINT16( 0x0001 );
      pData[12] = HI_UINT16( 0x0001 );

      // write the reference connection event count - use current event count
      pData[9] = LO_UINT16( connPtr->currentEvent );
      pData[10] = HI_UINT16( connPtr->currentEvent );

      break;

    case LL_TEST_MODE_TP_CON_MAS_BV_31_2:
    case LL_TEST_MODE_TP_CON_SLA_BV_30_2:
      // override Offset0 to CI-1.25ms
      pData[11] = LO_UINT16( (connPtr->curParam.connInterval>>1)-1 );
      pData[12] = HI_UINT16( (connPtr->curParam.connInterval>>1)-1 );

      // write the reference connection event count - use current event count
      pData[9] = LO_UINT16( connPtr->currentEvent );
      pData[10] = HI_UINT16( connPtr->currentEvent );

      break;

    case LL_TEST_MODE_TP_CON_MAS_BV_31_3:
    case LL_TEST_MODE_TP_CON_SLA_BV_30_3:
      // override Offset0 to 1.25ms
      pData[11] = LO_UINT16( 0x0001 );
      pData[12] = HI_UINT16( 0x0001 );

      // override Offset1 to 1.25ms
      pData[13] = LO_UINT16( 0x0002 );
      pData[14] = HI_UINT16( 0x0002 );

      // write the reference connection event count - use current event count
      pData[9] = LO_UINT16( connPtr->currentEvent );
      pData[10] = HI_UINT16( connPtr->currentEvent );

      break;

    case LL_TEST_MODE_TP_CON_MAS_BV_32:
    case LL_TEST_MODE_TP_CON_SLA_BV_31:
      // write the min connection interval
      pData[0] = LO_UINT16( connPtr->connParams.intervalMin );
      pData[1] = HI_UINT16( connPtr->connParams.intervalMin );

      // write the max connection interval
      pData[2] = LO_UINT16( connPtr->connParams.intervalMax );
      pData[3] = HI_UINT16( connPtr->connParams.intervalMax );

      // override preferred periodicity - within CI range
      pData[8] = LO_UINT16( 10 );

      // write the reference connection event count - use current event count
      pData[9] = LO_UINT16( connPtr->currentEvent );
      pData[10] = HI_UINT16( connPtr->currentEvent );

      break;

    case LL_TEST_MODE_TP_CON_MAS_BV_33:
    case LL_TEST_MODE_TP_CON_SLA_BV_32:
      // write the min connection interval
      pData[0] = LO_UINT16( connPtr->connParams.intervalMin );
      pData[1] = HI_UINT16( connPtr->connParams.intervalMin );

      // write the max connection interval
      pData[2] = LO_UINT16( connPtr->connParams.intervalMax );
      pData[3] = HI_UINT16( connPtr->connParams.intervalMax );

      // override preferred periodicity - within CI range
      pData[8] = LO_UINT16( 10 );

      // write the reference connection event count - use current event count
      pData[9] = LO_UINT16( connPtr->currentEvent );
      pData[10] = HI_UINT16( connPtr->currentEvent );

      // override Offset0 to 1.25ms
      pData[11] = LO_UINT16( 0x0001 );
      pData[12] = HI_UINT16( 0x0001 );

      break;

    case LL_TEST_MODE_TP_CON_MAS_BI_06:
    case LL_TEST_MODE_TP_CON_SLA_BI_08:
      // write an invalid min connection interval
      pData[0] = LO_UINT16( 0x0004 );
      pData[1] = HI_UINT16( 0x0004 );

      // write an invalid max connection interval
      pData[2] = LO_UINT16( 0x0004 );
      pData[3] = HI_UINT16( 0x0004 );

      break;

    // otherwise
    default:
      break;
  }
}
/*******************************************************************************
 * @fn          llSetupConnParamRsp_testmode
 *
 * @brief       This function is used to setup the Connection Parameter
 *              Response control procedure for test mode.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection
 * @param       pData   - Pointer to array to be fill
 *
 * output parameters
 *
 * @Output      pData - Packet's data payload
 */
static inline void llSetupConnParamRsp_testmode( llConnState_t *connPtr,
                                                 uint8_t *pData  )
{
  switch( llTestMode.testCase )
  {
    case LL_TEST_MODE_TP_CON_SLA_BV_30_1:
      // override Offset0 to 1.25ms
      pData[11] = LO_UINT16( connPtr->connParams.offset0 );
      pData[12] = HI_UINT16( connPtr->connParams.offset0 );

      // write the reference connection event count - use current event count
      pData[9] = LO_UINT16( connPtr->currentEvent );
      pData[10] = HI_UINT16( connPtr->currentEvent );

      break;

    case LL_TEST_MODE_TP_CON_SLA_BV_30_2:
      // override Offset0 to CI-1.25ms
      pData[11] = LO_UINT16( connPtr->connParams.offset0 );
      pData[12] = HI_UINT16( connPtr->connParams.offset0 );

      // write the reference connection event count - use current event count
      pData[9] = LO_UINT16( connPtr->currentEvent );
      pData[10] = HI_UINT16( connPtr->currentEvent );

      break;

    case LL_TEST_MODE_TP_CON_SLA_BV_30_3:
      // override Offset0 to 1.25ms
      pData[11] = LO_UINT16( connPtr->connParams.offset0 );
      pData[12] = HI_UINT16( connPtr->connParams.offset0 );

      // override Offset1 to 1.25ms
      pData[13] = LO_UINT16( connPtr->connParams.offset1 );
      pData[14] = HI_UINT16( connPtr->connParams.offset1 );

      // write the reference connection event count - use current event count
      pData[9] = LO_UINT16( connPtr->currentEvent );
      pData[10] = HI_UINT16( connPtr->currentEvent );

      break;

    case LL_TEST_MODE_TP_CON_SLA_BV_31:
      // write the min connection interval
      pData[0] = LO_UINT16( connPtr->connParams.intervalMin );
      pData[1] = HI_UINT16( connPtr->connParams.intervalMin );

      // write the max connection interval
      pData[2] = LO_UINT16( connPtr->connParams.intervalMax );
      pData[3] = HI_UINT16( connPtr->connParams.intervalMax );

      // override preferred periodicity - within CI range
      pData[8] = LO_UINT16( 10 );

      // write the reference connection event count - use current event count
      pData[9] = LO_UINT16( connPtr->currentEvent );
      pData[10] = HI_UINT16( connPtr->currentEvent );

      break;

    case LL_TEST_MODE_TP_CON_SLA_BV_32:
      // write the min connection interval
      pData[0] = LO_UINT16( connPtr->connParams.intervalMin );
      pData[1] = HI_UINT16( connPtr->connParams.intervalMin );

      // write the max connection interval
      pData[2] = LO_UINT16( connPtr->connParams.intervalMax );
      pData[3] = HI_UINT16( connPtr->connParams.intervalMax );

      // override preferred periodicity - within CI range
      pData[8] = LO_UINT16( 10 );

      // write the reference connection event count - use current event count
      pData[9] = LO_UINT16( connPtr->currentEvent );
      pData[10] = HI_UINT16( connPtr->currentEvent );

      // override Offset0 to 1.25ms
      pData[11] = LO_UINT16( connPtr->connParams.offset0 );
      pData[12] = HI_UINT16( connPtr->connParams.offset0 );

      break;

    // otherwise
    default:
      break;
  }
}
#endif // LL_TEST_MODE
