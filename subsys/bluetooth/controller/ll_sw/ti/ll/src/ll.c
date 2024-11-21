/******************************************************************************

 @file  ll.c

 @brief This file contains the Link Layer (LL) API for the Bluetooth
        Low Energy (BLE) Controller.

        This API is based on the Bluetooth Core Specification,
        V4.1.0, Vol. 6.

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
#include <string.h>

#include "bcomdef.h"
#include "hal_mcu.h"
#include <ti/drivers/utils/Random.h>
#include <ti/drivers/utils/Math.h>
#include "ll_ecc.h"
#include "onboard.h"
#include "osal_bufmgr.h"
#include "osal_pwrmgr.h"
#include "osal_cbtimer.h"
#include "ll.h"
#include "ll_ae.h"
#include "ll_common.h"
#include "ll_enc.h"
#include "ll_config.h"
#include "ll_scheduler.h"
#include "ll_timer_drift.h"

#include "ll_rat.h"

#include "ll_privacy.h"
#include "ble.h"
#include "hci_event.h"
#ifdef BLE_HEALTH
#include <health_toolkit/inc/debugInfo_errno.h>
#endif //BLE_HEALTH

#include "map_direct.h"

#include <ti/drivers/rcl/RCL.h>
#include <ti/drivers/rcl/commands/ble5.h>
#ifdef USE_FPGA
#include <ble_setup_fpga.h>
#endif // USE_FPGA

#include <ti/drivers/ECDH.h>

#include DeviceFamily_constructPath(inc/hw_fcfg.h)
#ifndef USE_HSM
#include <ti/drivers/rng/RNGLPF3RF.h>
#else
#include <ti/drivers/rng/RNGLPF3HSM.h>
#endif

// Extended Scanner
aeSetScanParamCmd_t aeScanParams;
aeEnableScanCmd_t   aeScanEnable;

// Extended Initiator
aeCreateConnCmd_t   aeCreateConn;


/*******************************************************************************
 * MACROS
 */

#define LL_COPY_DEV_ADDR_LE( dstPtr, srcPtr )                                  \
  (dstPtr)[0] = (srcPtr)[0];                                                   \
  (dstPtr)[1] = (srcPtr)[1];                                                   \
  (dstPtr)[2] = (srcPtr)[2];                                                   \
  (dstPtr)[3] = (srcPtr)[3];                                                   \
  (dstPtr)[4] = (srcPtr)[4];                                                   \
  (dstPtr)[5] = (srcPtr)[5];
// ALT: Could use OSAL copy.
// MAP_osal_memcpy( (dstPtr), (srcPtr), LL_DEVICE_ADDR_LEN );

#define LL_COPY_DEV_ADDR_BE( dstPtr, srcPtr )                                  \
  (dstPtr)[0] = (srcPtr)[5];                                                   \
  (dstPtr)[1] = (srcPtr)[4];                                                   \
  (dstPtr)[2] = (srcPtr)[3];                                                   \
  (dstPtr)[3] = (srcPtr)[2];                                                   \
  (dstPtr)[4] = (srcPtr)[1];                                                   \
  (dstPtr)[5] = (srcPtr)[0];
// ALT: Could use OSAL copy.
// MAP_osal_memcpy( (dstPtr), (srcPtr), LL_DEVICE_ADDR_LEN );

#define BDADDR_VALID( bdAddr )                                                 \
  ( !(                                                                         \
       ((bdAddr)[0] == 0xFF) &&                                                \
       ((bdAddr)[1] == 0xFF) &&                                                \
       ((bdAddr)[2] == 0xFF) &&                                                \
       ((bdAddr)[3] == 0xFF) &&                                                \
       ((bdAddr)[4] == 0xFF) &&                                                \
       ((bdAddr)[5] == 0xFF)                                                   \
     )                                                                         \
  )

// See "Supported States Related" below.
#define LL_SET_SUPPORTED_STATES( state )                                       \
  states[ (state)>>4 ] |= (1<<((state) & 0x0F));

#define LL_READ_PHY_TYPE_CODED 3
// Used in LL_ReadPhy. As mentioned 7.8.47 LE Read PHY command, Read PHY command
// should return 3 for coded PHY. The conversion is needed since LE Set PHY command
// is a bitmap which allows setting multiple PHYs, here the coded PHY value
// is 4 (the third bit)
#define LL_ConvertPhy(phy) ((phy == LL_PHY_CODED)? LL_READ_PHY_TYPE_CODED : phy)

/*******************************************************************************
 * CONSTANTS
 */

// Bluetooth Version Information
#define LL_VERSION_NUM      0x0D    // BT Core Specification V5.4

#define LL_COMPANY_ID       0x000D  // Texas Instruments Inc.

// Major Version (8 bits) . Minor Version (4 bits) . SubMinor Version (4 bits)
#define LL_SUBVERSION_NUM   0x0334  // Controller BLE5 3.3.4

// SDK Version Associated with LL Version
// Major Version (8 bits) . Minor Version (4 bits). SubMinor Version (4 bits)
// The direct conversion is as follows:
// SDK Major Version = LL Minor Version + 5
// SDK SubMinor Version = LL SubMinor Version
#define LL_SDK_VERSION_NUM  0x0840  // SDK Version 8.40.00

#if defined( BUILD_REVISION )
  #if ( BUILD_REVISION == 0xFFFFFFFF )
    #error "The TortoiseSVN command line command svn.exe is missing!"
  #endif // BUILD_REVISION!=0xFFFFFFFF
#else // !BUILD_REVISION
  #define BUILD_REVISION              0
#endif // BUILD_REVISION

// Cold boot or warm boot enumeration
#define LL_COLD_BOOT                  0
#define LL_WARM_BOOT                  1

// Connection Window Information

// Default Adv/Scan/Init Paramters
#define LL_ADV_INTERVAL_DEFAULT       160      // 100ms in 625us ticks
#define LL_SCAN_INTERVAL_DEFAULT      640      // 400ms in 625us ticks
//
#define LL_ADV_CHAN_MAP_DEFAULT       LL_ADV_CHAN_ALL

// Supported States Related
// Note: Each value has the byte offset in the upper nibble and the bit offset
//       in the lower nibble.
// Byte 0
#define LL_NONCONN_ADV_STATE                           0x00
#define LL_SCANNABLE_ADV_STATE                         0x01
#define LL_CONNECTABLE_ADV_STATE                       0x02
#define LL_HDC_DIR_ADV_STATE                           0x03
#define LL_PASSIVE_SCAN_STATE                          0x04
#define LL_ACTIVE_SCAN_STATE                           0x05
#define LL_INIT_CENTRAL_CONN_STATE                     0x06
#define LL_PERIPHERAL_CONN_STATE                       0x07
// Byte 1
#define LL_NONCONN_ADV_PASSIVE_SCAN_STATE              0x10
#define LL_SCANNABLE_ADV_PASSIVE_SCAN_STATE            0x11
#define LL_CONNECTABLE_ADV_PASSIVE_SCAN_STATE          0x12
#define LL_HDC_DIR_ADV_PASSIVE_SCAN_STATE              0x13
#define LL_NONCONN_ADV_ACTIVE_SCAN_STATE               0x14
#define LL_SCANNABLE_ADV_ACTIVE_SCAN_STATE             0x15
#define LL_CONNECTABLE_ADV_ACTIVE_SCAN_STATE           0x16
#define LL_HDC_DIR_ADV_ACTIVE_SCAN_STATE               0x17
// Byte 2
#define LL_NONCONN_ADV_INIT_STATE                      0x20
#define LL_SCANNABLE_ADV_INIT_STATE                    0x21
#define LL_NONCONN_ADV_CENTRAL_CONN_STATE              0x22
#define LL_SCANNABLE_ADV_CENTRAL_CONN_STATE            0x23
#define LL_NONCONN_ADV_PERIPHERAL_CONN_STATE           0x24
#define LL_SCANNABLE_ADV_PERIPHERAL_CONN_STATE         0x25
#define LL_PASSIVE_SCAN_INIT_STATE                     0x26
#define LL_ACTIVE_SCAN_INIT_STATE                      0x27
// Byte 3
#define LL_PASSIVE_SCAN_CENTRAL_CONN_STATE             0x30
#define LL_ACTIVE_SCAN_CENTRAL_CONN_STATE              0x31
#define LL_PASSIVE_SCAN_PERIPHERAL_CONN_STATE          0x32
#define LL_ACTIVE_SCAN_PERIPHERAL_CONN_STATE           0x33
#define LL_INIT_CENTRAL_CONN_MM_ROLE_STATE             0x34
#define LL_LDC_DIR_ADV_STATE                           0x35
#define LL_LDC_DIR_ADV_PASSIVE_SCAN_STATE              0x36
#define LL_LDC_DIR_ADV_ACTIVE_SCAN_STATE               0x37
// Byte 4
#define LL_CONNECTABLE_ADV_INIT_CP_ROLE_STATE          0x40
#define LL_HDC_DIR_ADV_INIT_CP_ROLE_STATE              0x41
#define LL_LDC_DIR_ADV_INIT_CP_ROLE_STATE              0x42
#define LL_CONNECTABLE_ADV_CENTRAL_CONN_CP_ROLE_STATE  0x43
#define LL_HDC_DIR_ADV_CENTRAL_CONN_CP_ROLE_STATE      0x44
#define LL_LDC_DIR_ADV_CENTRAL_CONN_CP_ROLE_STATE      0x45
#define LL_CONNECTABLE_ADV_PERIPHERAL_CONN_PP_ROLE_STATE    0x46
#define LL_HDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE        0x47
// Byte 5
#define LL_LDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE        0x50
#define LL_INIT_PERIPHERAL_CONN_CP_ROLE_STATE               0x51

// RF Callbacks
#define POWER_UP_CALLBACK  (RF_Callback)MAP_rfPUpCallback
#define RF_ERROR_CALLBACK  (RF_Callback)MAP_rfErrorCallback

#define PERIODIC_ADV_INTERVAL_UNIT_IN_US                    1250
#define PERIODIC_ADV_DATA_CMD_MAX_PAYLOAD                   252

#define PERIODIC_SCAN_CREATE_SYNC_OPTIONS_MAX_VAL           3
#define PERIODIC_SCAN_CREATE_SYNC_SID_MAX_VAL               0x0F
#define PERIODIC_SCAN_CREATE_SYNC_SKIP_MAX_VAL              0x01F3
#define PERIODIC_SCAN_CREATE_SYNC_TO_MIN_VAL                0x000A
#define PERIODIC_SCAN_CREATE_SYNC_TO_MAX_VAL                0x4000
#define PERIODIC_SCAN_CREATE_SYNC_CTE_MAX_VAL               0x1F  // bits 0-4
#define PERIODIC_SCAN_CREATE_SYNC_CTE_VALID_BITS_VAL        0x17  // bits 0-4 exclude bit 3
#define PERIODIC_SCAN_ACCEPT_LIST_MAX_ITEMS                 0xFF  // max items according to spec

/*******************************************************************************
 * EXTERNS
 */

#ifdef LL_CONN_SIZE
extern uint32 totalConnSize;
#endif // LL_CONN_SIZE

extern void         LL_rclTestCallback(RCL_Command *cmd, LRF_Events lrfEvents, RCL_Events events);
extern int_fast16_t RCL_AdcNoise_get_samples_blocking(uint32_t *buffer, uint32_t numWords);

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
extern uint8 maxSupportedAdvSets;
extern uint16 maxExtAdvDataLen;
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
// Extended Scan Report State Variables
extern uint8 scanState;
#endif

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*
** Own Device Address
**
** Note that the device may have a permanently assigned BLE address located in
** the Information Page. However, this address could be overridden with either
** the build flag BDADDR_FROM_FLASH, or by the vendor specific command
** LL_EXT_SET_BDADDR.
*/

// Own Device Public Address
uint8 ownPublicAddr[ LL_DEVICE_ADDR_LEN ] ALIGNED; // index 0..5 is LSO..MSB

// Own Device Random Address
 uint8 ownRandomAddr[ LL_DEVICE_ADDR_LEN ] ALIGNED;  // index 0..5 is LSO..MSB

// Saved Own Device Public Address
uint8 ownSavedPublicAddr[ LL_DEVICE_ADDR_LEN ] ALIGNED;  // index 0..5 is LSO..MSB

/*******************************************************************************
 * GLOBAL VARIABLES
 */

RCL_Handle  rfHandle;
RCL_Client  rfClient;

// randomAddressConfigured is set to True when HCI_LE_SetRandomAddressCmd() function called
// and changed ownRandomAddr values to random values.
// Acording to spec, it is expected to configure the random address value
// when adv/scan set params is used with ownAddrType of random address
// and before calling set_adv/scan_enable.
uint8 randomAddressConfigured = FALSE;

// saved copy of max PDU size specified by the user
// Note: Since the user can specify the max sized PDU, a local copy is
//       made and used to avoid possible side effects from changes done by the
//       user during execution.
uint16 maximumPduSize;

// ------------------------------------------------------ //
uint8 maxPduSize;// Deprecated ROM variable - Do not use! //
// ------------------------------------------------------ //

uint8 *activeConns;

extern RNG_Handle rngHandle;

#ifdef LL_TEST_MODE
llTestMode_t llTestMode;
//
volatile uint8 firstTx;
volatile uint8 timSlvBv05Done;
volatile uint8 numSets;
volatile uint8 numTxPkts;
volatile uint8 nomCI;
volatile uint8 numTxEvts;
volatile uint8 setFailed;
volatile uint8 numFailedSets;
volatile uint8 numFailedTx;
#endif // LL_TEST_MODE

//  System Boot Message
uint8 *SysBootMsg;

// V5.0 - 2M and Coded PHY
uint8 defaultPhy;

uint16 connInitialMaxTxOctets;
uint16 connInitialMaxTxTime;
uint16 connInitialMaxTxTimeUncoded;
uint16 connInitialMaxTxTimeCoded;
//
uint16 supportedMaxTxOctets;
uint16 supportedMaxTxTime;
uint16 supportedMaxRxOctets;
uint16 supportedMaxRxTime;

#ifdef LL_TEST_MODE
uint16 invalidRxOctets;
uint16 invalidRxTime;
uint16 invalidTxOctets;
uint16 invalidTxTime;
#endif

// QOS PARAMETERS
//***************
// Qos default priority parameters
uint8  qosDefaultPriorityConnParameter      = LL_QOS_MEDIUM_PRIORITY;
uint8  qosDefaultPriorityAdvParameter       = LL_QOS_LOW_PRIORITY;
uint8  qosDefaultPriorityScnParameter       = LL_QOS_LOW_PRIORITY;
uint8  qosDefaultPriorityInitParameter      = LL_QOS_LOW_PRIORITY;
uint8  qosDefaultPriorityPerAdvParameter    = LL_QOS_LOW_PRIORITY;
uint8  qosDefaultPriorityPerScnParameter    = LL_QOS_LOW_PRIORITY;

// BUILD_REVISION
// The define is initialized to the subversion revision when the network
// library used. It is in a global variable so it can be accessed from outside
// the library.
const uint16 ll_buildRevision = BUILD_REVISION;

// saved copy of number of supported data buffers specified by the user
// Note: Since the user can specify the number of Tx buffers, a local copy is
//       made and used to avoid possible side effects from changes done by the
//       user during execution.
uint8 maxNumTxDataBufs;

// max number of CTE sampling auto copy buffers
uint8 maxNumCteDataBufs;

#ifdef LL_CONN_SIZE
uint32 totalConnSize;
#endif // LL_CONN_SIZE

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
// Disconnect Connection Immediately Global Variable
uint16 disconnectImmedConnId = LL_INACTIVE_CONNECTIONS;
#endif // ADV_CONN_CFG | INIT_CFG

////////////////////////////////////////////////////////////////////////////////

uint8         LL_TaskID;          // OSAL LL task ID
uint8         llState;            // state of LL
uint16        taskEndStatus;      // nR TASKDONE end status
int8          rssiCorrection;     // RSSI correction
featureSet_t  deviceFeatureSet;   // feature set for this device
RFBLEDPL_TX_POWER_TYPE         curTxPowerVal;      // current Tx Power Table Index
RFBLEDPL_TX_POWER_TYPE         maxTxPwrForDTM;     // max power override for DTM
rfPathComp_t *pRfPathComp;        // RF Tx Path Compensation data
verInfo_t     verInfo;            // own version information
buildInfo_t   buildInfo;          // build revision data

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ((ADV_NCONN_CFG | ADV_CONN_CFG) | INIT_CFG))
llConns_t     llConns;            // LL connections table
uint8         onePktPerEvt;       // flag indicates if only one packet allowed per event
uint8         numComplPkts;       // number of completed Tx buffers
uint8         numComplPktsLimit;  // minimum number of completed Tx buffers before event
uint8         numComplPktsFlush;  // flag to indicate send number of completed buffers at end of event
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
uint8         fastTxRespTime;     // flag indicates if fast TX response time feature is enabled/disabled
uint8         slOverride;         // flag for user suspension of SL
#endif // ADV_CONN_CFG

// Direct Test Mode
dtmInfo_t     *dtmInfo;           // Direct Test Mode and Modem Test info

// RX Flow Control
uint8         rxFifoFlowCtrl;

// saved copy of number of supported data buffers specified by the user
// Note: Since the user can specify the number of Tx buffers, a local copy is
//       made and used to avoid possible side effects from changes done by the
//       user during execution.
uint8         numTxDataBufs;

// saved copy of max number of connections specified by the user
// Note: Since the user can specify the number of connections, a local copy is
//       made and used to avoid possible side effects from changes done by the
//       user during execution.
uint8          maxNumConns;

// saved copy of extended stack setting specified by the user
// Note: Settings are set as bitmask.
//       Since the user can specify different settings, a local copy is
//       made and used to avoid possible side effects from changes done by the
//       user during execution.

uint32         extStackSettings;

// Radio Task End Cause Jump Table
void (*taskEndAction)( void );

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
uint8 defaultChannelMap[LL_NUM_BYTES_FOR_CHAN_MAP];
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
uint8 secondaryAdvChannelMap[LL_NUM_BYTES_FOR_CHAN_MAP];
uint8 secondaryAdvChannelMapPopCount;
#endif

/*******************************************************************************
 * Static Functions
 */

// Function prototypes for the refactored event handlers
static uint32 llHandleConnDisconnectedImmed(uint32 events);
static uint32 llHandleExtScanEvents(uint32 events);
static uint32 llHandleCmdStarted(uint32 events);
static uint32 llHandleConnEvents(uint32 events);
static uint32 llHandlePostProcessRF(uint32 events);
static uint32 llHandleAddressResolutionTimeout(uint32 events);
static uint32 llHandleSystemReset(uint32 events);
static uint32 llHandleOutOfMemory(uint32 events);
static uint32 llHandleInitDone(uint32 events);
static uint32 llHandlePeriodicScanEvents(uint32 events);
static uint32 llHandlePeriodicAdvEvents(uint32 events);
static uint32 llHandleAdvEvents(uint32 events);
static uint32 llHandleInitEvents(uint32 events);
static uint32 llHandleReschedule(uint32 events);

/*******************************************************************************
 * LL OSAL API
 */

/*******************************************************************************
 * This is the Link Layer task initialization called by OSAL. It must be called
 * once when the software system is started and before any other function in the
 * LL API is called.
 *
 * Public function defined in ll.h.
 */
void LL_Init( uint8 taskId )
{
  halIntState_t state;

  // save task ID assigned by OSAL
  LL_TaskID = taskId;

  // pointer used to hold OSAL message sent during boot (such as Out of Memory)
  SysBootMsg = NULL;

  // set BLE version information
  verInfo.verNum    = LL_VERSION_NUM;
  verInfo.comId     = LL_COMPANY_ID;
  verInfo.subverNum = LL_SUBVERSION_NUM;

  // fetch BDADDR from FCFG
  LL_COPY_DEV_ADDR_LE( ownPublicAddr,
                      (uint8 *)fcfg->deviceInfo.bleAddr );
  // set saved own public address as the same as currently used own public addr
  // Note: Do not invalidate ownSavedPublicAddr in case the user tries to set
  //       own public address with an invalid address using the HCI command.
  //       An invalid address is used to restore the previous ownPublicAddr
  //       from ownSavedPublicAddr. If we invalidate ownSavedPublicAddr here,
  //       we'll end up restoring an invalid address.
  ownSavedPublicAddr[0] = ownPublicAddr[0];
  ownSavedPublicAddr[1] = ownPublicAddr[1];
  ownSavedPublicAddr[2] = ownPublicAddr[2];
  ownSavedPublicAddr[3] = ownPublicAddr[3];
  ownSavedPublicAddr[4] = ownPublicAddr[4];
  ownSavedPublicAddr[5] = ownPublicAddr[5];

  // set own random address as invalid until one is provided
  ownRandomAddr[0] = 0xFF;
  ownRandomAddr[1] = 0xFF;
  ownRandomAddr[2] = 0xFF;
  ownRandomAddr[3] = 0xFF;
  ownRandomAddr[4] = 0xFF;
  ownRandomAddr[5] = 0xFF;

  // set default user build info
  buildInfo.userRevNum  = 0;

  // save user specified number of Tx buffers to allow
  maxNumTxDataBufs = llConfigTable.userCfgPtr->numTxEntries;

  // save user specified number of connections
  maxNumConns = llConfigTable.userCfgPtr->maxNumConns;

  // save user specified max data size
  maximumPduSize = llUserConfig_maxPduSize;

  // save user specified misc stack settings
  extStackSettings = llConfigTable.userCfgPtr->extStackSettings;

  // save user specified accept list size
  alSize = llConfigTable.userCfgPtr->maxAlElems;

  // save user specified accept list size
  rlSize = llConfigTable.userCfgPtr->maxRlElems;

  // The dynamic filter list not use with extended filter list.
  if ( llUserConfig.useDFL == TRUE )
  {
    extALSize = 0;
  }
  else
  {
    extALSize = (2 * (rlSize)) + 1;
  }

  // Initialize ECC Driver
  MAP_ll_eccInit();

  // reset Tx Power to its default value
  curTxPowerVal = RfBleDpl_getTxPowerDefaultIdx();

  // set Max Tx Power to default value for DTM
  maxTxPwrForDTM = RfBleDpl_getTxPowerMax();

  //////////////////////////////////////////////////////////////////////////////
  // Begin Dynamic Allocation
  // Note: This memory is allocated but never freed.
  //////////////////////////////////////////////////////////////////////////////

  if ( LL_STATUS_SUCCESS != MAP_llDynamicAlloc() )
  {
    MAP_llDynamicFree();

    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_OUT_OF_MEMORY );

    LL_ASSERT( FALSE );

    return;
  }

  //////////////////////////////////////////////////////////////////////////////
  // End Dynamic Allocation
  //////////////////////////////////////////////////////////////////////////////

  //////////////////////////////////////////////////////////////////////////////
  // Radio Initialization
  //////////////////////////////////////////////////////////////////////////////
  HAL_ENTER_CRITICAL_SECTION(state);

  // init and open the PRNG driver
  Random_seedAutomatic();

  // RNG_init should be called only after LL_initRNGNoise is called
  RNG_init();

  RNG_Params params;
  RNG_Params_init(&params);
  params.returnBehavior = RNG_RETURN_BEHAVIOR_BLOCKING;

  // open the RNG driver to get the handle
  rngHandle = RNG_open(0, &params);

  HAL_EXIT_CRITICAL_SECTION(state);

  // init Crypto engine
  MAP_LL_ENC_Init();

  // Initialize connection event reporting to not report
  llConnEvtNotice.cb = NULL;
  llConnEvtNotice.handle = LL_CONNHANDLE_INVALID;
  llConnEvtNotice.eventType = LL_CONN_EVT_INVALID_TYPE;

  // set event to indicate the LL initialization is done
  // Note: LL_Reset is called after LL_EVT_INIT_DONE is processed.
  (void)MAP_osal_set_event( LL_TaskID, LL_EVT_INIT_DONE );

  return;
}

/*******************************************************************************
 * This function returns True if Adv/Scan/Init/periodic_sync is/are active
 * O.W return False.
 */
uint8 LL_IsRLActiveTasksRunning( void )
{
  uint8 activeTasks = FALSE;

#if defined(CTRL_CONFIG) && ((CTRL_CONFIG & ADV_NCONN_CFG) || (CTRL_CONFIG & ADV_CONN_CFG))
      activeTasks |= MAP_LL_CountAdvSets( LE_COUNT_ENABLED_ADV_SETS );
#endif // ADV_NCONN_CFG || ADV_CONN_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
  activeTasks |= (extScanInfo->scanMode == LL_SCAN_START);
#endif // SCAN_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  activeTasks |= (extInitInfo->scanMode == LL_SCAN_START);
#endif // INIT_CFG


#if defined(CTRL_CONFIG) && defined(USE_PERIODIC_SCAN) && (CTRL_CONFIG & SCAN_CFG)
  activeTasks |= (llPeriodicScan.createSync != NULL);
#endif // Periodic ADV SYNC

  return activeTasks;
}

/*******************************************************************************
 * This function returns True if Resolving List is in used
 * O.W return False.
 */
uint8 LL_IsResolvingListInUsed( void )
{
  return (privInfo.addrResolution && LL_IsRLActiveTasksRunning());
}

/*******************************************************************************
 * This is the Link Layer process event handler called by OSAL.
 *
 * @Design: BLE_LOKI-1451
 *
 * Public function defined in ll.h.
 */
uint32 LL_ProcessEvent(uint8 taskId, uint32 events)
{
  // Unused input parameter; PC-Lint error 715.
  (void)taskId;

  taskInfo_t *llTask = MAP_llGetCurrentTask();
  if (llTask != NULL)
  {
    // Clear the initial task start time
    llTask->startTime = 0;
  }

    events = llHandleConnDisconnectedImmed(events);
    events = llHandleExtScanEvents(events);
    events = llHandleCmdStarted(events);
    events = llHandleConnEvents(events);
    events = llHandlePostProcessRF(events);
    events = llHandleAddressResolutionTimeout(events);
    events = llHandleSystemReset(events);
    events = llHandleOutOfMemory(events);
    events = llHandleInitDone(events);
    events = llHandlePeriodicScanEvents(events);
    events = llHandlePeriodicAdvEvents(events);
    events = llHandleAdvEvents(events);
    events = llHandleInitEvents(events);
    events = llHandleReschedule(events);

  // return unhandled events. Those will be handled in the next call to LL_ProcessEvent
  return events;
}

static uint32 llHandleConnDisconnectedImmed(uint32 events)
{
  if (events & LL_EVT_CONN_DISCONNECTED_IMMED)
  {
    llConnState_t *connPtr = NULL;

    // Get connection info.
    if (disconnectImmedConnId != LL_INACTIVE_CONNECTIONS)
    {
      connPtr = MAP_llDataGetConnPtr(disconnectImmedConnId);
    }

    if (connPtr != NULL)
    {
      // Let the application know.
      MAP_LL_DisconnectCback((uint16)connPtr->connId, LL_STATUS_ERROR_HOST_TERM);

      // Cleanup the connection data structures and task
      MAP_llConnCleanup(connPtr);
    }

    // Clear the disconnect immediate connection id.
    disconnectImmedConnId = LL_INACTIVE_CONNECTIONS;

    // Clear the set
    events ^= LL_EVT_CONN_DISCONNECTED_IMMED;
  }

  return events;
}

static uint32 llHandleExtScanEvents(uint32 events)
{
  if (events & LL_EVT_EXT_SCAN_TIMEOUT)
  {
    MAP_llProcessScanTimeout();
    events ^= LL_EVT_EXT_SCAN_TIMEOUT;
  }

  if (events & LL_EVT_SCAN_RX_AVAIL)
  {
    MAP_LL_rclScanRxEntryDone();
    events ^= LL_EVT_SCAN_RX_AVAIL;
  }

  if (events & LL_EVT_SCAN_LAST_CMD_DONE)
  {
    MAP_llExtScan_PostProcess();
    events ^= LL_EVT_SCAN_LAST_CMD_DONE;
  }

  return events;
}

static uint32 llHandleCmdStarted(uint32 events)
{
  if (events & LL_EVT_CMD_STARTED)
  {
    MAP_llCmdStartedEventHandle();
    events ^= LL_EVT_CMD_STARTED;
  }
  return events;
}

static uint32 llHandleConnEvents(uint32 events)
{
  /*****************************************************************************
  ** Process the Connection RX AVAIL
  ******************************************************************************
  ** IMPORTANT: LL_EVT_CONN_RX_AVAIL MUST BE PROCESSED BEFORE THE
  ** LL_EVT_CENTRAL_LAST_CMD_DONE AND LL_EVT_PERIPHERAL_LAST_CMD_DONE EVENTS
  ** DO NOT CHANGE THE PROCCESSING ORDER OF THOSE EVENTS!!!
  *****************************************************************************/
  if (events & LL_EVT_CONN_RX_AVAIL)
  {
    // Check if any post processing is even required
    MAP_llRxEntryDoneEventHandleStateConnection();
    events ^= LL_EVT_CONN_RX_AVAIL;
  }

  /*****************************************************************************
  ** Post-Process the RF Task Completion
  ******************************************************************************
  ** IMPORTANT: LL_EVT_CONN_RX_AVAIL MUST BE PROCESSED BEFORE THE
  ** LL_EVT_CENTRAL_LAST_CMD_DONE AND LL_EVT_PERIPHERAL_LAST_CMD_DONE EVENTS
  ** DO NOT CHANGE THE PROCCESSING ORDER OF THOSE EVENTS!!!
  *****************************************************************************/
  if ( events & LL_EVT_CENTRAL_LAST_CMD_DONE )
  {
    MAP_llCentral_TaskEnd();

    events ^= LL_EVT_CENTRAL_LAST_CMD_DONE;
  }
  /*****************************************************************************
  ** Post-Process the RF Task Completion
  ******************************************************************************
  ** IMPORTANT: LL_EVT_CONN_RX_AVAIL MUST BE PROCESSED BEFORE THE
  ** LL_EVT_CENTRAL_LAST_CMD_DONE AND LL_EVT_PERIPHERAL_LAST_CMD_DONE EVENTS
  ** DO NOT CHANGE THE PROCCESSING ORDER OF THOSE EVENTS!!!
  *****************************************************************************/
  if ( events & LL_EVT_PERIPHERAL_LAST_CMD_DONE )
  {
    MAP_llPeripheral_TaskEnd();

    events ^= LL_EVT_PERIPHERAL_LAST_CMD_DONE;
  }

  if ( events & LL_EVT_CONN_TX_BUFF_FINISHED  )
  {
    MAP_llProcessTxData();

    events ^= LL_EVT_CONN_TX_BUFF_FINISHED;
  }

  if (events & LL_EVT_CENTRAL_CONN_CREATED)
  {
    MAP_llProcessCentralConnectionCreated();
    events ^= LL_EVT_CENTRAL_CONN_CREATED;
  }

  if (events & LL_EVT_CENTRAL_CONN_CANCELLED)
  {
    MAP_llProcessConnectionEstablishFailed(LL_LINK_CONNECT_COMPLETE_CENTRAL,
                                           LL_STATUS_ERROR_UNKNOWN_CONN_HANDLE);
    events ^= LL_EVT_CENTRAL_CONN_CANCELLED;
  }
  return events;
}

static uint32 llHandlePostProcessRF(uint32 events)
{
  if (events & LL_EVT_POST_PROCESS_RF)
  {
    if (taskEndAction != NULL)
    {
      (taskEndAction)();
    }
    events ^= LL_EVT_POST_PROCESS_RF;
  }
  return events;
}

static uint32 llHandleAddressResolutionTimeout(uint32 events)
{
  if (events & LL_EVT_ADDRESS_RESOLUTION_TIMEOUT)
  {
    if (!MAP_LL_PRIV_IsZeroIRK(resolvingList[LOCAL_RL_INDEX].IRK))
    {
      // Re-generate local RPA
      MAP_LL_PRIV_GenerateRPA(resolvingList[LOCAL_RL_INDEX].IRK,
                              resolvingList[LOCAL_RL_INDEX].RPA);
    }
    MAP_llProcessAdvAddrResolutionTimeout();

    // Start timer
    if (MAP_osal_start_timerEx(LL_TaskID,
                               LL_EVT_ADDRESS_RESOLUTION_TIMEOUT,
                               privInfo.rpaTimeout) != SUCCESS)
    {
      // Generate event to the Host
      MAP_HCI_HardwareErrorEvent(HW_FAIL_NO_TIMER_AVAILABLE);
    }

    events ^= LL_EVT_ADDRESS_RESOLUTION_TIMEOUT;
  }
  return events;
}

static uint32 llHandleSystemReset(uint32 events)
{
  if (events & LL_EVT_RESET_SYSTEM_HARD)
  {
    SystemReset();
    events ^= LL_EVT_RESET_SYSTEM_HARD;
  }

  if (events & LL_EVT_RESET_SYSTEM_SOFT)
  {
    // SystemResetSoft();
    events ^= LL_EVT_RESET_SYSTEM_SOFT;
  }
  return events;
}

static uint32 llHandleOutOfMemory(uint32 events)
{
  if (events & LL_EVT_OUT_OF_MEMORY)
  {
    MAP_HCI_HardwareErrorEvent(HW_FAIL_OUT_OF_MEMORY);
    events ^= LL_EVT_OUT_OF_MEMORY;
  }
  return events;
}

static uint32 llHandleInitDone(uint32 events)
{
  if (events & LL_EVT_INIT_DONE)
  {
    MAP_LL_PRIV_GenerateRSA(ownRandomAddr);
    RCL_init();
    rfHandle = MAP_llScheduler_getHandle(LL_TASK_ID_STANDARD_BLE);
    (void)MAP_LL_Reset();
    events ^= LL_EVT_INIT_DONE;
  }
  return events;
}

static uint32 llHandlePeriodicScanEvents(uint32 events)
{
  if (events & LL_EVT_PERIODIC_SCAN_RX_AVAIL)
  {
    MAP_llProcessPeriodicScanRxFIFO();
    events ^= LL_EVT_PERIODIC_SCAN_RX_AVAIL;
  }

  if (events & LL_EVT_PERIODIC_SCAN_LAST_CMD_DONE)
  {
    MAP_llPeriodicScan_PostProcess();
    events ^= LL_EVT_PERIODIC_SCAN_LAST_CMD_DONE;
  }

  if (events & LL_EVT_PERIODIC_SCAN_CANCELLED)
  {
    MAP_llTerminatePeriodicScan();
    events ^= LL_EVT_PERIODIC_SCAN_CANCELLED;
  }
  return events;
}

static uint32 llHandlePeriodicAdvEvents(uint32 events)
{
  if (events & LL_EVT_PERIODIC_ADV_TX_BUFF_FINISHED)
  {
    MAP_llUpdatePeriodicAdvChainPacket();
    events ^= LL_EVT_PERIODIC_ADV_TX_BUFF_FINISHED;
  }

  if (events & LL_EVT_PERIODIC_ADV_LAST_CMD_DONE)
  {
    MAP_llPeriodicAdv_PostProcess();
    events ^= LL_EVT_PERIODIC_ADV_LAST_CMD_DONE;
  }
  return events;
}

static uint32 llHandleAdvEvents(uint32 events)
{
  if (events & LL_EVT_ADV_RX_AVAIL)
  {
    MAP_LL_rclAdvRxEntryDone();
    events ^= LL_EVT_ADV_RX_AVAIL;
  }

  if (events & LL_EVT_ADV_TX_BUFF_FINISHED)
  {
    MAP_LL_rclAdvTxFinished();
    events ^= LL_EVT_ADV_TX_BUFF_FINISHED;
  }

  if (events & LL_EVT_ADV_LAST_CMD_DONE)
  {
    if (MAP_llLastCmdDoneEventHandleConnectRequest() == TRUE)
    {
      MAP_llAdv_TaskConnect();
    }
    else
    {
      MAP_llExtAdv_PostProcess();
    }
    events ^= LL_EVT_ADV_LAST_CMD_DONE;
  }
  return events;
}

static uint32 llHandleInitEvents(uint32 events)
{
  if (events & LL_EVT_INIT_RX_ENTRY_DONE)
  {
    MAP_LL_rclInitRxEntryDone();
    events ^= LL_EVT_INIT_RX_ENTRY_DONE;
  }

  if (events & LL_EVT_INIT_LAST_CMD_DONE_RX_ERR)
  {
    MAP_llExtInit_ResolveConnRsp();
    events ^= LL_EVT_INIT_LAST_CMD_DONE_RX_ERR;
  }

  if (events & LL_EVT_INIT_LAST_CMD_DONE_CONNECT)
  {
    MAP_llInit_TaskConnect();
    events ^= LL_EVT_INIT_LAST_CMD_DONE_CONNECT;
  }

  if (events & LL_EVT_INIT_LAST_CMD_DONE)
  {
    MAP_llExtInit_PostProcess();
    events ^= LL_EVT_INIT_LAST_CMD_DONE;
  }
  return events;
}

static uint32 llHandleReschedule(uint32 events)
{
  if (events & LL_EVT_RESCHEDULE)
  {
    MAP_llScheduler();
    events ^= LL_EVT_RESCHEDULE;
  }
  return events;
}
/*******************************************************************************
 * This function is used by the HCI to reset and initialize the LL Controller.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_Reset( void )
{
  llStatus_t retVal = LL_STATUS_SUCCESS;
  // reset Tx Power to its default value
  // Note: The variable curTxPowerVal must be set here before calling llRfInit!
  curTxPowerVal = RfBleDpl_getTxPowerDefaultIdx();

  // initialize the accept list
  MAP_AL_Init( alTable );
  MAP_AL_Scan_Init( alTableScan );

  // initialize the dynamic filter list
  retVal = LL_DFL_Init( LL_DFL_GetDynamicFilterlist(), LL_DFL_GetRankTable() );

  // clear the accept list table
  MAP_LL_ClearAcceptList();

  // reset Tx/Rx path compensation value
  pRfPathComp->rfTxPathCompParam = 0;
  pRfPathComp->rfRxPathCompParam = 0;
  pRfPathComp->rfTxPathCompVal   = 0;
  pRfPathComp->rfRxPathCompVal   = 0;

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
  // disable scanning, if there's a scanner
  if ( extScanInfo )
  {
    // disable scanning
    extScanInfo->scanMode = LL_SCAN_STOP;

    // if the scan command is active stop it
    if(MAP_llGetCurrentTask() == extScanInfo->llTask)
    {
      // stop the scan command
      MAP_llHaltRadio( (uint32)&extScanCmd );
    }
  }
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  // disable scanning if there is an initiator
  if ( extInitInfo )
  {
    extInitInfo->scanMode = LL_SCAN_STOP;

    // if the initiator command is active stop it
    if(MAP_llGetCurrentTask() == extInitInfo->llTask)
    {
        // stop the command
        MAP_llHaltRadio( (uint32)&extInitCmd );
    }
  }
#endif // INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  // clear all active connections
  for (uint8 i=0; i<maxNumConns; i++)
  {
    if ( llConns.llConnection[i].activeConn == TRUE )
    {
      llConnState_t *connPtr = MAP_llDataGetConnPtr( i );

      // cleanup the connection data structures and task
      MAP_llConnCleanup( connPtr );
    }
  }

  // free all connections
  MAP_llReleaseAllConnId();

  // reset the number of Tx data buffers
  numTxDataBufs = maxNumTxDataBufs;
#endif // ADV_CONN_CFG | INIT_CFG

  // initialize this devices Feature Set
  MAP_llInitFeatureSet();

  // V5.0 - 2M and Coded PHY
  defaultPhy = LL_PHY_SUPPORTED_PHYS;

  // down select connection phy preferences based on device feature bits
  defaultPhy &= ((deviceFeatureSet.featureSet[1] & LL_FEATURE_2M_PHY)    ?  ~0 : ~LL_PHY_2_MBPS);
  defaultPhy &= ((deviceFeatureSet.featureSet[1] & LL_FEATURE_CODED_PHY) ?  ~0 : ~LL_PHY_CODED);

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  onePktPerEvt      = FALSE;
  numComplPkts      = 0;
  numComplPktsLimit = 1;
  numComplPktsFlush = FALSE;
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  fastTxRespTime   = LL_EXT_ENABLE_FAST_TX_RESP_TIME;
  slOverride       = LL_EXT_DISABLE_PL_OVERRIDE;
#endif // ADV_CONN_CFG

  // disable Rx FIFO flow control
  rxFifoFlowCtrl   = LL_RX_FLOW_CONTROL_DISABLED;

  // Note: The Controller shall not change the values of supported variables!
  connInitialMaxTxOctets      = LL_MIN_LINK_DATA_LEN;
  connInitialMaxTxTime        = LL_MIN_LINK_DATA_TIME;
  connInitialMaxTxTimeUncoded = LL_MIN_LINK_DATA_TIME;
  connInitialMaxTxTimeCoded   = LL_MIN_LINK_DATA_TIME_CODED;
  //
  supportedMaxTxOctets        = LL_MAX_LINK_DATA_LEN;
  supportedMaxRxOctets        = LL_MAX_LINK_DATA_LEN;
  supportedMaxTxTime          = LL_MAX_LINK_DATA_TIME;
  supportedMaxRxTime          = LL_MAX_LINK_DATA_TIME;
  // initialize the Privacy Resolving List and disable feature
  MAP_LL_PRIV_Init();

  (void)MAP_LL_AE_Init();
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
#ifdef USE_PERIODIC_ADV
  //clear all periodic adv
  MAP_llClearPeriodicAdvSets();
#endif
#endif // ADV_NCONN_CFG | ADV_CONN_CFG
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
#ifdef USE_PERIODIC_SCAN
  //clear all periodic scan
  MAP_llClearPeriodicScanSets();
#endif
#endif // SCAN_CFG

  // initialize the LL task scheduler
  MAP_llSchedulerInit();

  // set state/role
  llState = LL_STATE_IDLE;

#ifdef LL_TEST_MODE
#ifdef LL_TEST_CASE
  llTestMode.testCase = LL_TEST_CASE;
#else
  llTestMode.testCase = LL_TEST_MODE_INVALID;
#endif
  llTestMode.counter  = 0;
  firstTx        = FALSE;
  timSlvBv05Done = FALSE;
  numSets        = 0;
  numTxPkts      = 0;
  nomCI          = TRUE;
  numTxEvts      = 0;
  setFailed      = FALSE;
  numFailedSets  = 0;
  numFailedTx    = 0;
  invalidRxOctets= 0;
  invalidRxTime  = 0;
  invalidTxOctets= 0;
  invalidTxTime  = 0;
#endif // LL_TEST_MODE

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  defaultChannelMap[0] = 0xFF;
  defaultChannelMap[1] = 0xFF;
  defaultChannelMap[2] = 0xFF;
  defaultChannelMap[3] = 0xFF;
  defaultChannelMap[4] = 0x1F;
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  secondaryAdvChannelMap[0] = 0xFF;
  secondaryAdvChannelMap[1] = 0xFF;
  secondaryAdvChannelMap[2] = 0xFF;
  secondaryAdvChannelMap[3] = 0xFF;
  secondaryAdvChannelMap[4] = 0x1F;
  secondaryAdvChannelMapPopCount = LL_MAX_NUM_DATA_CHAN;
#endif // ADV_CONN_CFG

  //set random Address as not configured
  randomAddressConfigured = FALSE;

  return retVal;
}

#ifndef USE_HSM
/*******************************************************************************
 * This function is used by osal to initialize the RNG driver before the system boots
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_initRNGNoise( void )
{
  int_fast16_t rclStatus;
  int_fast16_t rngStatus = RNG_STATUS_SUCCESS;
  uint8 rngInitRetryCount = 0;
  uint8 status = LL_STATUS_SUCCESS;

  // User's global array for noise input based on size provided in syscfg
  uint32_t *localNoiseInput = MAP_osal_mem_alloc(RNGLPF3RF_noiseInputWordLen * sizeof(uint32));

  if ( localNoiseInput != NULL )
  {
    do {
      // Zeroize localNoiseInput
      MAP_osal_memset(localNoiseInput, 0, RNGLPF3RF_noiseInputWordLen * sizeof(uint32));

      // Fill noise input from RCL
      rclStatus = RCL_AdcNoise_get_samples_blocking(localNoiseInput, RNGLPF3RF_noiseInputWordLen);

      // Should be STATUS_SUCCESS which is also 0, use LL_STATUS_SUCCESS instead
      if ( rclStatus != LL_STATUS_SUCCESS )
      {
        // Failed to sample noise. Assert.
        LL_ASSERT( (bool) FALSE );

        // Return error in case assert is disabled.
        status = LL_STATUS_ERROR_HW_FAILURE;
        break;
      }

      // Initialize the RNG driver noise input pointer with global noise input array from user
      rngStatus = RNGLPF3RF_conditionNoiseToGenerateSeed(localNoiseInput);

      // Bump the counter of the RNG init retries
      rngInitRetryCount++;

      // Continue until we succeed to initialize the RNG with a valid seed
    } while ( (rngStatus != RNG_STATUS_SUCCESS) && (rngInitRetryCount < LL_MAX_INIT_RNG_RETRIES) );

    if (rngStatus != RNG_STATUS_SUCCESS)
    {
      // Return error in case assert is disabled.
      status = LL_STATUS_ERROR_HW_FAILURE;

      // Failed to initialize the RNG after more than the maximum retries allowed. Assert.
      LL_ASSERT( (bool) FALSE );
    }

    // Free before exiting
    MAP_osal_mem_free(localNoiseInput);
  }
  else
  {
    status =  LL_STATUS_ERROR_OUT_OF_HEAP;
  }

  return ( status );
}
#endif
/*******************************************************************************
 * LL API for HCI
 */

/*******************************************************************************
 * This API is used to allocate memory using buffer management.
 *
 * Public function defined in ll.h.
 */
void *LL_TX_bm_alloc( uint16 size )
{
  uint8 *pBuf;
  uint16 allocSize;

  allocSize = size + sizeof(RCL_Buffer_TxBuffer) + RCL_BUFFER_MAX_HEADER_PAD_BYTES + LL_PKT_HDR_LEN + LL_PKT_MIC_LEN;

  // If 'size' is very large and 'allocSize' overflows, the result will be
  // smaller than size. In this case, don't try to allocate.
  if ( allocSize < size )
  {
    return ((void *)NULL);
  }

  // Note: This is the lowest call for buffer management allocation.

  pBuf = MAP_osal_bm_alloc( allocSize );

  if ( pBuf != NULL )
  {
    // return pointer to user payload
    // Note: The adjustment is subtracted from the buffer pointer, so passing
    //       a negative adjustment here advances the buffer pointer by that
    //       number of bytes.
    return( MAP_osal_bm_adjust_header( pBuf, -((uint16)sizeof(RCL_Buffer_TxBuffer) + RCL_BUFFER_MAX_HEADER_PAD_BYTES + LL_PKT_HDR_LEN) ) );
  }

  return( (void *)NULL );
}


/*******************************************************************************
 * This API is used to free memory using buffer management.
 *
 * Public function defined in ll.h.
 */
void LL_TX_bm_free( uint8* pBuf )
{
  MAP_osal_bm_free( pBuf ) ;
}


/*******************************************************************************
 * This API is used to allocate memory using buffer management.
 *
 * Public function defined in ll.h.
 */
void *LL_RX_bm_alloc( uint16 size )
{
  uint8 *pBuf;
  uint16 allocSize;

  allocSize = size + HCI_RX_PKT_HDR_SIZE;

  // If 'size' is very large and 'allocSize' overflows, the result will be
  // smaller than size. In this case, don't try to allocate.
  if ( allocSize < size )
  {
    return ((void *)NULL);
  }

  // Note: This is the lowest call for buffer management allocation.

  pBuf = MAP_osal_bm_alloc( allocSize );

  if ( pBuf != NULL )
  {
#ifdef LL_CONN_SIZE
    totalConnSize += (size + HCI_RX_PKT_HDR_SIZE);
#endif // LL_CONN_SIZE

    // return pointer to user payload
    // Note: The adjustment is subtracted from the buffer pointer, so passing
    //       a negative adjustment here advances the buffer pointer by that
    //       number of bytes.
    return( MAP_osal_bm_adjust_header( pBuf, -HCI_RX_PKT_HDR_SIZE) );
  }

  return( (void *)NULL );
}


/*******************************************************************************
 * This API is called by the HCI to read the controller's own public device
 * address.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadBDADDR( uint8 *bdAddr )
{
  // return own public device address LSO..MSO
  LL_COPY_DEV_ADDR_LE( bdAddr, ownPublicAddr );

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This function is used to save this device's random address. It is provided by
 * the Host for devices that are unable to store an IEEE assigned public address
 * in NV memory.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_SetRandomAddress( uint8 *devAddr )
{
  LL_ASSERT( devAddr != NULL );

  // Check Valid input
  if ( devAddr == NULL )
  {
    return LL_STATUS_ERROR_INVALID_PARAMS;
  }

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
  // case of scanning
  if ((llState == LL_STATE_SCAN) &&
     ((extScanInfo->ownAddrType == LL_DEV_ADDR_TYPE_RANDOM) ||
      (extScanInfo->ownAddrType == LL_DEV_ADDR_TYPE_RANDOM_ID)))
  {
    return ( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  // case of initiating
  if ((llState == LL_STATE_INIT) &&
     ((extInitInfo->ownAddrType == LL_DEV_ADDR_TYPE_RANDOM) ||
      (extInitInfo->ownAddrType == LL_DEV_ADDR_TYPE_RANDOM_ID)))
  {
    return ( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  // case of legacy advertising
  if (llState == LL_STATE_EXT_ADV)
  {
    advSet_t *pAdvSet = advSetList;

    while( pAdvSet )
    {
      if ((pAdvSet->advMode == LL_ADV_MODE_ON) &&
         (TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps)) &&
         ((pAdvSet->ownAddrType == LL_DEV_ADDR_TYPE_RANDOM) ||
          (pAdvSet->ownAddrType == LL_DEV_ADDR_TYPE_RANDOM_ID)))
      {
        return ( LL_STATUS_ERROR_COMMAND_DISALLOWED );
      }

      pAdvSet = pAdvSet->next;
    }
  }
#endif

  // store our random address LSO..MSO
  LL_COPY_DEV_ADDR_LE( ownRandomAddr, devAddr );

  // set random address as configured
  randomAddressConfigured = TRUE;

  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * This function return True if a random address was configured. O.W return False
 */
llStatus_t LL_IsRandomAddressConfigured( void )
{
  return randomAddressConfigured;
}

/*******************************************************************************
 * This API is called by the HCI to clear the Accept List.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ClearAcceptList( void )
{
  llStatus_t status;

  // check that it is okay to use the accept list
  if ( (status = MAP_llCheckAcceptListUsage()) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // clear number of entries, valid flags, address type flags, and entries
  MAP_AL_Clear( alTable );

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This API is called by the HCI to add a device address and it's type to the
 * Accept List.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_AddAcceptListDevice( uint8 *devAddr,
                                  uint8  addrType )
{
  llStatus_t  status;

  // check that it is okay to use the accept list
  if ( (status = MAP_llCheckAcceptListUsage()) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // check the AL device address type
  if ( (addrType != LL_DEV_ADDR_TYPE_PUBLIC) &&
       (addrType != LL_DEV_ADDR_TYPE_RANDOM) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  return( MAP_AL_AddEntry( alTable,
                           devAddr,
                           addrType,
                           BLE_USE_AL_ENTRY ) );
}


/*******************************************************************************
 * This API is called by the HCI to remove a device address and it's type from
 * the Accept List.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_RemoveAcceptListDevice( uint8 *devAddr,
                                     uint8  addrType )
{
  llStatus_t  status;

  // check that it is okay to use the accept list
  if ( (status = MAP_llCheckAcceptListUsage()) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // check the AL device address type
  if ( (addrType != LL_DEV_ADDR_TYPE_PUBLIC) &&
       (addrType != LL_DEV_ADDR_TYPE_RANDOM) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  return( MAP_AL_RemoveEntry( alTable,
                              devAddr,
                              addrType ) );
}


/*******************************************************************************
 * This API is called by the HCI to get the total number of accept list entries
 * that can be stored in the Controller.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadAlSize( uint8 *numEntries )
{
  // indicate the number of free AL entries
  *numEntries = MAP_AL_GetSize( alTable );

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This API is called by the HCI to get the number of Accept List entries that
 * are empty.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_NumEmptyAlEntries( uint8 *numEmptyEntries )
{
  // indicate the number of free AL entries
  *numEmptyEntries = MAP_AL_GetNumFreeEntries( alTable );

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This API is called by the HCI to request the LL to encrypt the data in the
 * command using the key given in the command.
 *
 * Note: The parameters are byte ordered MSO to LSO.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_Encrypt( uint8 *key,
                       uint8 *plaintextData,
                       uint8 *encryptedData )
{
  // check parameters
  if ( (key == NULL )          ||
       (plaintextData == NULL) ||
       (encryptedData == NULL) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // encrypt on behalf of the host
  MAP_LL_ENC_AES128_Encrypt( key, plaintextData, encryptedData );

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This API is called by the HCI to request the LL Controller to provide a data
 * block with random content.
 *
 * Note: The HCI spec indicates that the random number
 *       generation should adhere to one of those specified in FIPS
 *       PUB 140-2. The Core spec refers specifically to the
 *       algorithm specified in FIPS PUB 186-2, Appendix 3.1.
 *       Note that this software only uses the RF hardware to
 *       generate true random numbers. What's more, if the RF is
 *       already in use (i.e. overlapped execution), then the use
 *       of radio to generate true random numbers is prohibited.
 *       In this case, a pseudo-random blocks of numbers will be
 *       returned instead.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_Rand( uint8 *randData,
                    uint8  dataLen )
{
  // return TRN
  return( (llStatus_t)MAP_LL_ENC_GenerateTrueRandNum( randData, dataLen ) );
}


/*******************************************************************************
 * This API is a generic interface to get a block of pseudo-random numbers.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_PseudoRand( uint8 *randData,
                          uint8  dataLen )
{
  uint8 i;

  if ( (randData == NULL) || (dataLen == 0) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // can't be sure the radio isn't in use, so provide pseudo random numbers
  for (i=0; i<dataLen; i++)
  {
    randData[i] = MAP_LL_ENC_GeneratePseudoRandNum();
  }

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This function is used to provide the HCI with the Link Layer supported states
 * and supported state/role combinations.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadSupportedStates( uint8 *states )
{
  // provide supported state/role combinations
  // Note: Upper nibble is byte offset, lower nibble is bit offset.

#if defined(CTRL_CONFIG) && (CTRL_CONFIG == ADV_NCONN_CFG)
  // Non-Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_STATE );

#elif defined(CTRL_CONFIG) && (CTRL_CONFIG == ADV_CONN_CFG)
  // Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );

#elif defined(CTRL_CONFIG) && (CTRL_CONFIG == (ADV_NCONN_CFG | ADV_CONN_CFG))
  // Non-Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_STATE );
  // Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  // Non-Connectable Adv + Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_PERIPHERAL_CONN_STATE );

#elif defined(CTRL_CONFIG) && (CTRL_CONFIG == SCAN_CFG)
  // Scan
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_STATE );

#elif defined(CTRL_CONFIG) && (CTRL_CONFIG == (ADV_NCONN_CFG | SCAN_CFG))
  // Non-Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_STATE );
  // Scan
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_STATE );
  // Non-Connectable Adv + Scan
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_ACTIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_ACTIVE_SCAN_STATE );

#elif defined(CTRL_CONFIG) && (CTRL_CONFIG == (ADV_CONN_CFG | SCAN_CFG))
  // Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  // Scan
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_STATE );
  // Connectable Adv + Scan
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_ACTIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_ACTIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_ACTIVE_SCAN_STATE );

#elif defined(CTRL_CONFIG) && (CTRL_CONFIG == (ADV_NCONN_CFG | ADV_CONN_CFG | SCAN_CFG))
  // Non-Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_STATE );
  // Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  // Non-Connectable Adv + Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_PERIPHERAL_CONN_STATE );
  // Scan
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_STATE );
  // Non-Connectable Adv + Scan
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_ACTIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_ACTIVE_SCAN_STATE );
  // Connectable Adv + Scan
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_ACTIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_ACTIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_ACTIVE_SCAN_STATE );

#elif defined(CTRL_CONFIG) && (CTRL_CONFIG == INIT_CFG)
  // Init
  LL_SET_SUPPORTED_STATES( LL_INIT_CENTRAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_INIT_CENTRAL_CONN_MM_ROLE_STATE );

#elif defined(CTRL_CONFIG) && (CTRL_CONFIG == (ADV_NCONN_CFG | INIT_CFG))
  // Non-Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_STATE );
  // Init
  LL_SET_SUPPORTED_STATES( LL_INIT_CENTRAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_INIT_CENTRAL_CONN_MM_ROLE_STATE );
  // Non-Connectable Adv + Init
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_INIT_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_INIT_STATE );
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_CENTRAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_CENTRAL_CONN_STATE );

#elif defined(CTRL_CONFIG) && (CTRL_CONFIG == (ADV_CONN_CFG | INIT_CFG))
  // Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  // Init
  LL_SET_SUPPORTED_STATES( LL_INIT_CENTRAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_INIT_CENTRAL_CONN_MM_ROLE_STATE );
  // Connectable Adv + Init
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_INIT_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_INIT_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_INIT_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_CENTRAL_CONN_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_CENTRAL_CONN_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_CENTRAL_CONN_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_INIT_PERIPHERAL_CONN_CP_ROLE_STATE );

#elif defined(CTRL_CONFIG) && (CTRL_CONFIG == (ADV_NCONN_CFG | ADV_CONN_CFG | INIT_CFG))
  // Non-Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_STATE );
  // Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  // Non-Connectable Adv + Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_PERIPHERAL_CONN_STATE );
  // Init
  LL_SET_SUPPORTED_STATES( LL_INIT_CENTRAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_INIT_CENTRAL_CONN_MM_ROLE_STATE );
  // Non-Connectable Adv + Init
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_INIT_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_INIT_STATE );
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_CENTRAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_CENTRAL_CONN_STATE );
  // Connectable Adv + Init
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_INIT_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_INIT_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_INIT_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_CENTRAL_CONN_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_CENTRAL_CONN_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_CENTRAL_CONN_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_INIT_PERIPHERAL_CONN_CP_ROLE_STATE );

#elif defined(CTRL_CONFIG) && (CTRL_CONFIG == (SCAN_CFG | INIT_CFG))
  // Scan
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_STATE );
  // Init
  LL_SET_SUPPORTED_STATES( LL_INIT_CENTRAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_INIT_CENTRAL_CONN_MM_ROLE_STATE );
  // Scan + Init
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_INIT_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_INIT_STATE );
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_CENTRAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_CENTRAL_CONN_STATE );

#elif defined(CTRL_CONFIG) && (CTRL_CONFIG == (ADV_NCONN_CFG | SCAN_CFG | INIT_CFG))
  // Non-Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_STATE );
  // Scan
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_STATE );
  // Init
  LL_SET_SUPPORTED_STATES( LL_INIT_CENTRAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_INIT_CENTRAL_CONN_MM_ROLE_STATE );
  // Non-Connectable Adv + Init
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_INIT_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_INIT_STATE );
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_CENTRAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_CENTRAL_CONN_STATE );
  // Non-Connectable Adv + Scan
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_ACTIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_ACTIVE_SCAN_STATE );
  // Scan + Init
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_INIT_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_INIT_STATE );
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_CENTRAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_CENTRAL_CONN_STATE );

#elif defined(CTRL_CONFIG) && (CTRL_CONFIG == (ADV_CONN_CFG | SCAN_CFG | INIT_CFG))
  // Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  // Scan
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_STATE );
  // Init
  LL_SET_SUPPORTED_STATES( LL_INIT_CENTRAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_INIT_CENTRAL_CONN_MM_ROLE_STATE );
  // Connectable Adv + Scan
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_ACTIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_ACTIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_ACTIVE_SCAN_STATE );
  // Connectable Adv + Init
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_INIT_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_INIT_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_INIT_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_CENTRAL_CONN_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_CENTRAL_CONN_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_CENTRAL_CONN_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_INIT_PERIPHERAL_CONN_CP_ROLE_STATE );
  // Scan + Init
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_INIT_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_INIT_STATE );
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_CENTRAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_CENTRAL_CONN_STATE );

#elif defined(CTRL_CONFIG) && (CTRL_CONFIG == (ADV_NCONN_CFG | ADV_CONN_CFG | SCAN_CFG | INIT_CFG))
  // Non-Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_STATE );
  // Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_STATE );
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_PERIPHERAL_CONN_PP_ROLE_STATE );
  // Non-Connectable Adv + Connectable Adv
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_PERIPHERAL_CONN_STATE );
  // Non-Connectable Adv + Init
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_INIT_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_INIT_STATE );
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_CENTRAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_CENTRAL_CONN_STATE );
  // Non-Connectable Adv + Scan
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_NONCONN_ADV_ACTIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_SCANNABLE_ADV_ACTIVE_SCAN_STATE );
  // Connectable Adv + Scan
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_ACTIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_ACTIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_PERIPHERAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_ACTIVE_SCAN_STATE );
  // Connectable Adv + Init
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_INIT_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_INIT_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_INIT_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_CONNECTABLE_ADV_CENTRAL_CONN_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_HDC_DIR_ADV_CENTRAL_CONN_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_LDC_DIR_ADV_CENTRAL_CONN_CP_ROLE_STATE );
  LL_SET_SUPPORTED_STATES( LL_INIT_PERIPHERAL_CONN_CP_ROLE_STATE );
  // Scan
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_STATE );
  // Init
  LL_SET_SUPPORTED_STATES( LL_INIT_CENTRAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_INIT_CENTRAL_CONN_MM_ROLE_STATE );
  // Scan + Init
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_INIT_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_INIT_STATE );
  LL_SET_SUPPORTED_STATES( LL_PASSIVE_SCAN_CENTRAL_CONN_STATE );
  LL_SET_SUPPORTED_STATES( LL_ACTIVE_SCAN_CENTRAL_CONN_STATE );

#else // CTRL_CONFIG==0
#error "***ERROR*** Controller Build Configuration Error!"
#endif // CTRL_CONIFG

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This function is used to provide the HCI with the number of active Link
 * Layer connections.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_GetNumActiveConns( uint8 *numActiveConns )
{
  // make sure the pointer is valid
  if ( numActiveConns != NULL )
  {
    *numActiveConns = llConns.numActiveConns;
  }

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This API is called by the HCI to read the controller's Feature Set. The
 * Controller indicates which features it supports.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadLocalSupportedFeatures( uint8 *featureSet )
{
  uint8 i;

  LL_ASSERT( featureSet != NULL );

  // Check Valid input
  if ( featureSet == NULL )
  {
    return LL_STATUS_ERROR_INVALID_PARAMS;
  }

  // copy Feature Set
  for (i=0; i<LL_MAX_FEATURE_SET_SIZE; i++)
  {
    featureSet[i] = deviceFeatureSet.featureSet[i];
  }

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This API is called by the HCI to read the controller's Version information.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadLocalVersionInfo( uint8  *verNum,
                                    uint16 *comId,
                                    uint16 *subverNum )
{
  LL_ASSERT( verNum != NULL);
  LL_ASSERT( comId != NULL);
  LL_ASSERT( subverNum != NULL);

  // Check Valid input
  if ( verNum == NULL || comId == NULL || subverNum == NULL)
  {
    return LL_STATUS_ERROR_INVALID_PARAMS;
  }

  // get the version of this BLE controller
  *verNum = verInfo.verNum;

  // get the company ID of this BLE controller
  *comId = verInfo.comId;

  // get the subversion of this BLE controller
  *subverNum = verInfo.subverNum;

  return( LL_STATUS_SUCCESS );
}


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This function is used to indicate if the LL enable/disable receive FIFO
 * processing. This function provides support for Controller to Host flow
 * control.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_CtrlToHostFlowControl( uint8 mode )
{
  if ( mode == LL_ENABLE_RX_FLOW_CONTROL )
  {
    // set flag to indicate flow control is enabled
    rxFifoFlowCtrl = LL_RX_FLOW_CONTROL_ENABLED;
  }
  else if ( mode == LL_DISABLE_RX_FLOW_CONTROL )
  {
    // set flag to indicate flow control is disabled
    rxFifoFlowCtrl = LL_RX_FLOW_CONTROL_DISABLED;
  }
  else // error
  {
    return( LL_STATUS_ERROR_UNEXPECTED_PARAMETER );
  }

  return( LL_STATUS_SUCCESS );

}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This API is called by the HCI to read the peer controller's Version
 * Information. If the peer's Version Information has already been received by
 * its request for our Version Information, then this data is already cached and
 * can be directly returned to the Host. If the peer's Version Information is
 * not already cached, then it will be requested from the peer, and when
 * received, returned to the Host via the LL_ReadRemoteVersionInfoCback
 * callback.
 *
 * Note: Only one Version Indication is allowed for a connection.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadRemoteVersionInfo( uint16 connId )
{
  llStatus_t    status;
  llConnState_t *connPtr;

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connId)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // get connection info
  connPtr = MAP_llDataGetConnPtr( connId );

  // first, make sure the connection is still active
  if ( !connPtr->activeConn )
  {
    return( LL_STATUS_ERROR_INACTIVE_CONNECTION );
  }

  // check if the peer's version information has already been obtained
  if ( connPtr->verExchange.peerInfoValid == TRUE )
  {
    // yes it has, so provide it to the host
    MAP_LL_ReadRemoteVersionInfoCback( LL_STATUS_SUCCESS,
                                       connId,
                                       connPtr->verInfo.verNum,
                                       connPtr->verInfo.comId,
                                       connPtr->verInfo.subverNum );
  }
  else // no it hasn't, so...
  {
    // ...check if the host has already requested this information
    // and also enter critical section so the interrupt from the RX would not mess with the verExchange flags
    halIntState_t cs;
    HAL_ENTER_CRITICAL_SECTION(cs);
    if ( connPtr->verExchange.hostRequest == FALSE )
    {
      // no, so request it by queueing the control packet for processing
      MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_VERSION_IND );

      // set the flag to indicate the host has requested this information
      connPtr->verExchange.hostRequest = TRUE;

      // exit the criticl section - the flag was raised
      HAL_EXIT_CRITICAL_SECTION(cs);
    }
    else // previously requested
    {
      // Need to exit the critical section if we did not enter to the if statment
      HAL_EXIT_CRITICAL_SECTION(cs);
      return( LL_STATUS_ERROR_VER_INFO_REQ_ALREADY_PENDING );
    }
  }

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This function is used to read a connection's current transmit power level or
 * power level or the maximum transmit power level, in dBm.
 *
 * Note: As there is no way for the host to modify the TX output power based
 *       on connection, it isn't clear how the settings would be different for
 *       different connections. For now, the connection ID is only used to
 *       check if the connection is active.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadTxPowerLevel( uint8  connId,
                                uint8  type,
                                int8  *txPower )
{
  // check parameters
  if ( txPower == NULL )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // determine which type of TX power level is required
  switch( type )
  {
    case LL_READ_CURRENT_TX_POWER_LEVEL:
      // return the TX power level based on current setting
      *txPower = MAP_llGetTxPower();

      // check if Tx output power is valid
      if ( *txPower == LL_TX_POWER_INVALID )
      {
        return( LL_STATUS_ERROR_PARAM_OUT_OF_RANGE );
      }
      break;

    case LL_READ_MAX_TX_POWER_LEVEL:
      // return max data channel TX power level
      *txPower = RfBleDpl_getTxPowerDbm(RfBleDpl_getTxPowerMax());
      break;

    default:
      return( LL_STATUS_ERROR_BAD_PARAMETER );
  }
  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This API is called by the HCI to read the channel map that the LL controller
 * is using for the LL connection.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadChanMap( uint8  connId,
                           uint8 *chanMap )
{
  llConnState_t *connPtr;

  // make sure connection ID is valid
  if ( MAP_LL_ConnActive(connId) != LL_STATUS_SUCCESS )
  {
    return( LL_STATUS_ERROR_UNKNOWN_CONN_HANDLE );
  }

  // get connection info
  connPtr = MAP_llDataGetConnPtr( connId );

  // copy current channel map
  chanMap[0] = connPtr->curChanMap.chanMap[0];
  chanMap[1] = connPtr->curChanMap.chanMap[1];
  chanMap[2] = connPtr->curChanMap.chanMap[2];
  chanMap[3] = connPtr->curChanMap.chanMap[3];
  chanMap[4] = connPtr->curChanMap.chanMap[4];

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG


/*******************************************************************************
 * This API is called by the HCI to request RSSI. If there is an active
 * connection for the given connection ID, then the RSSI of the last received
 * data packet in the LL will be returned. If a receiver Modem Test is running,
 * then the RF RSSI for the last received data will be returned. If no valid
 * RSSI value is available, then LL_RSSI_NOT_AVAILABLE will be returned.
 *
 * Note: In either case, if the RSSI is valid, it is corrected by the current
 *       receiver gain.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadRssi( uint16  connId,
                        int8   *lastRssi )
{
  uint8 rssi = LL_RF_RSSI_UNDEFINED;

  // check if a receiver modem test is running
  if ( llState == LL_STATE_MODEM_TEST_RX )
  {
    // get last RSSI snapshot
    rssi = RCL_readRssi();
  }
  else if ( llState == LL_STATE_DIRECT_TEST_MODE_RX )
  {
    // get last RSSI snapshot
    rssi = rxTestOut.lastRssi;
  }
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  else // not a receiver modem test
  {
    llStatus_t status;

    // make sure connection ID is valid
    if ( (status=MAP_LL_ConnActive(connId)) != LL_STATUS_SUCCESS )
    {
      return( status );
    }

    // get this connection's last RSSI
    rssi = ((llConnState_t *)MAP_llDataGetConnPtr( connId ))->lastRssi;
  }
#endif // ADV_CONN_CFG | INIT_CFG
  // check RSSI, and if valid, correct
  *lastRssi = LL_CHECK_LAST_RSSI( rssi );

  return( LL_STATUS_SUCCESS );
}


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This API is called by the HCI to terminate a LL connection.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_Disconnect( uint16 connId,
                          uint8  reason )
{
  llStatus_t    status;
  llConnState_t *connPtr;

  // check if the reason code is valid
  if ( (reason != LL_DISCONNECT_AUTH_FAILURE)                &&
       (reason != LL_DISCONNECT_REMOTE_USER_TERM)            &&
       (reason != LL_DISCONNECT_REMOTE_DEV_LOW_RESOURCES)    &&
       (reason != LL_DISCONNECT_REMOTE_DEV_POWER_OFF)        &&
       (reason != LL_DISCONNECT_UNSUPPORTED_REMOTE_FEATURE)  &&
       (reason != LL_DISCONNECT_KEY_PAIRING_NOT_SUPPORTED)   &&
       (reason != LL_DISCONNECT_UNACCEPTABLE_CONN_INTERVAL) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connId)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // get connection info
  connPtr = MAP_llDataGetConnPtr( connId );

  // check if a terminate control procedure is already what's pending
  if ( connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_TERMINATE_IND )
  {
    return( LL_STATUS_ERROR_CTRL_PROC_ALREADY_ACTIVE );
  }
  else // spec says a terminate can happen any time
  {
    // indicate the peer requested this termination
    connPtr->termInfo.reason = reason;

    // de-activate peripheral latency to expedite termination
    connPtr->peripheralLatency = 0;

    // override any control procedure that may be in progress
    MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_TERMINATE_IND,
                          LL_CTRL_UNDEFINED_PKT );
  }

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This API is called by the HCI to transmit a buffer of data on a given LL
 * connection. The length of the packet is compared to the max PDU size to
 * determine whether fragmentation is supported. If the max PDU size is greater
 * than 27 bytes, then fragmentation is supported, otherwise it is not. When
 * fragmentation is supported, the boundary flags are ignored, otherwise they
 * used as is. If this is a Peripheral connection and peripheral latency is in effect and
 * fastTx is enabled, then realignment to the next connection event will be
 * performed.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_TxData( uint16 connId,
                      uint8 *pBuf,
                      uint16 len,
                      uint8  fragFlag )
{
  halIntState_t  cs;
  llStatus_t     status;
  llConnState_t *connPtr;

  HAL_ENTER_CRITICAL_SECTION(cs);

  // Sanity check the buffer pointer
  if ( pBuf == NULL )
  {
    HAL_EXIT_CRITICAL_SECTION(cs);

    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // Sanity check input parameters
  // Note: The value of fragFlag only matters if fragmentation/recombination
  //       is disabled, but okay to check even if fragmentation/recombination
  //       is being used.
  if ( (fragFlag != LL_DATA_FIRST_PKT_HOST_TO_CTRL) &&
       (fragFlag != LL_DATA_CONTINUATION_PKT) )
  {
    HAL_EXIT_CRITICAL_SECTION(cs);

    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // Check PDU size
  if ( len > maximumPduSize )
  {
    // Report failure to Host
    MAP_HCI_HardwareErrorEvent( HW_FAIL_PDU_SIZE_EXCEEDS_MTU );

    HAL_EXIT_CRITICAL_SECTION(cs);

    return( LL_STATUS_ERROR_PARAM_OUT_OF_RANGE );
  }

  // Make sure connection ID is valid
  status = MAP_LL_ConnActive(connId);

  if ( status != LL_STATUS_SUCCESS )
  {
    HAL_EXIT_CRITICAL_SECTION(cs);

    return( status );
  }

  // Get the connection info based on the connection ID
  connPtr = MAP_llDataGetConnPtr( connId );

  // Check if any more Tx buffers are allowed
  if ( numTxDataBufs == 0 )
  {
    // Indicate to the Host that an overflow has occurred
    MAP_HCI_DataBufferOverflowEvent( HCI_LINK_TYPE_ACL_BUFFER_OVERFLOW );

    // Update Debug Info Module with the error
    (void)MAP_DbgInf_addErrorRec(DBGINF_ERROR_LL_OUT_OF_TX_MEM);

    HAL_EXIT_CRITICAL_SECTION(cs);

    return( LL_STATUS_ERROR_OUT_OF_TX_MEM );
  }

  // Decrement the number available Tx Buffers
  // Note: This variable is incremented from the Tx Entry Done ISR.
  --numTxDataBufs;

  // Check if fragmentation is to be used based on length and OTA size
  if ( len > connPtr->lenInfo.connActualMaxTxOctets )
  {
    // Note: PDUs are not queued, so either the PDU will be completely
    //       fragmented, or we'll run out of heap. In the latter case, the
    //       HW error HW_FAIL_OUT_OF_MEMORY will be sent, and fragments will
    //       be sent as it, resulting in a discarded PDU by the peer.

    // Fragment the segment (PDU) into OTA packets
    status = MAP_llFragmentPDU( connPtr, pBuf, len );

    // Check if fragmenting the PDU worked
    // Note: Returning an error indicates the PDU was not freed!
    // Note: An error may still result in one or more OTA fragments being sent!
    if ( status != LL_STATUS_SUCCESS )
    {
      HAL_EXIT_CRITICAL_SECTION(cs);

      return( status );
    }

    // Free segment
    MAP_osal_bm_free( pBuf );
  }
  else // len <= connEffectiveMaxTxOctets
  {
    // Okay to queue data
    // Note: After the packet is transmitted, it will be removed from the Tx
    //       queue, freed, and if the conditions are right, a Number of
    //       Completed Packet event will be sent to the Host.
    MAP_llWriteTxData( connPtr, pBuf, len, fragFlag, DATA_ENTRY_LAST_PACKET );
  }

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  // Check for fast Tx, and realign to next connection event if necessary
  // Note: Only used for Peripheral when Peripheral Latency is in effect.
  if (connPtr->llTask->taskID == MAP_llGetCurrentTask()->taskID)
  {
    MAP_llAlignToNextEvent( connPtr );
  }
#endif // CTRL_CONFIG=ADV_CONN_CFG

  HAL_EXIT_CRITICAL_SECTION(cs);

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG

static void llDirectTestConvertLlPhyToBlePhy(uint8 directTestLlPhy, uint8 *blePhy, uint8 *blePhyOpts)
{
  switch (directTestLlPhy)
  {
    case LL_DTM_TX_1_MBPS: /* LL_DTM_RX_1_MBPS*/
      *blePhy      = BLE5_1M_PHY;
      *blePhyOpts  = BLE5_CODING_NONE;
    break;
    case LL_DTM_TX_2_MBPS: /*LL_DTM_RX_2_MBPS*/
      *blePhy      = BLE5_2M_PHY;
      *blePhyOpts  = BLE5_CODING_NONE;
    break;
    case LL_DTM_TX_C8 /* LL_DTM_RX_CODED */:
      *blePhy      = BLE5_CODED_PHY;
      *blePhyOpts  = (BLE5_CODED_S8_DEFAULT << 2);
    break;
    case LL_DTM_TX_C2:
      *blePhy      = BLE5_CODED_PHY;
      *blePhyOpts  = (BLE5_CODED_S2_DEFAULT << 2);
    break;
    default:
      *blePhy      = BLE5_1M_PHY;
      *blePhyOpts  = BLE5_CODING_NONE;
    break;
  }
}

/*******************************************************************************
 * This function is used to initiate a BLE PHY level Transmit Test in Direct
 * Test Mode where the DUT generates test reference packets at fixed intervals.
 * This test will make use of the nanoRisc Raw Data Transmit and Receive task.
 *
 * Note: The BLE device will transmit at maximum power.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_DirectTestTxTest( uint8 txChan,
                                uint8 payloadLen,
                                uint8 payloadType,
                                uint8 txPhy )
{
  uint32 payloadTime;

  // verify input parameters are valid
  // Note: The txPhy was already verified by LL_EnhancedTxTest.
  if ( (txChan >= LL_TOTAL_NUM_RF_CHAN)                 ||
       ((payloadType != LL_DIRECT_TEST_PAYLOAD_PRBS9)  &&
        (payloadType != LL_DIRECT_TEST_PAYLOAD_0x0F)   &&
        (payloadType != LL_DIRECT_TEST_PAYLOAD_0x55)   &&
        (payloadType != LL_DIRECT_TEST_PAYLOAD_PRBS15) &&
        (payloadType != LL_DIRECT_TEST_PAYLOAD_0xFF)   &&
        (payloadType != LL_DIRECT_TEST_PAYLOAD_0x00)   &&
        (payloadType != LL_DIRECT_TEST_PAYLOAD_0xF0)   &&
        (payloadType != LL_DIRECT_TEST_PAYLOAD_0xAA)) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check that we are idle until combo states are supported
  if ( llState != LL_STATE_IDLE )
  {
    return( LL_STATUS_ERROR_UNEXPECTED_STATE_ROLE );
  }

  // command initialization
  txDtmTestCmd = RCL_CmdBle5DtmTx_DefaultRuntime();
  // set channel
  // Note: The input is the RCL Channel not the BLE Channel.
  txDtmTestCmd.channel = txChan + LL_DTM_RCL_CHANNEL_OFFSET;

  txDtmTestCmd.pduHeader = payloadType;
  txDtmTestCmd.pduLength = payloadLen;
  txDtmTestCmd.cteInfo = 0;
  txDtmTestCmd.txPower = RfBleDpl_getTxPower(maxTxPwrForDTM);

  // init number of of packets to transmit to be unlimited
  txDtmTestCmd.numPackets = dtmInfo->txPktCnt;

  // determine the time in us for the payload length to be transmitted
  uint8 blePhy;
  uint8 blePhyOpts;

  llDirectTestConvertLlPhyToBlePhy(txPhy, &blePhy, &blePhyOpts);

  payloadTime = MAP_llOctets2Time( blePhy,
                                   blePhyOpts,
                                   payloadLen,
                                   MIC_NOT_ENABLED );

  // find the DTM packet period based on BT V5.0, Vol. 6, Part F, section 4.1.6
  // For a LE Test Packet length of L us:
  //   I(L) = ceil((L + 249) / 625) * 625 us
  payloadTime =  ((payloadTime + 249) / 625) + (((payloadTime + 249) % 625) ? 1 : 0);
  payloadTime *= (625 * RAT_TICKS_IN_1US);

  txDtmTestCmd.common.phyFeatures = blePhy | blePhyOpts;

  // Divide by 4 just to avoid ifdefing the payloadTime
  txDtmTestCmd.periodUs = payloadTime/RAT_TICKS_IN_1US;

  // Note: The Tester may also use T(L) upon change of a dirty transmitter
  //       parameter setting and during verification of the EUT's PER report:
  //         T(L) = max(I(L) + 10 ms, 12.5 ms)
  //       To act like a Tester, the following code could be used.
  //txTestParam.period = MAX( txTestParam.period + RAT_TICKS_IN_10MS,
  //                          RAT_TICKS_IN_12_5MS );

  // save parameters
  dtmInfo->rfChan      = txChan;
  dtmInfo->packetLen   = payloadLen;
  dtmInfo->packetType  = payloadType;
  dtmInfo->numPackets  = 0;
  dtmInfo->numRxCrcNOK = 0;
  dtmInfo->lastRssi    = LL_RF_RSSI_UNDEFINED;

  // set state/role
  // Note: This must precede call to llRfInit!
  llState = LL_STATE_DIRECT_TEST_MODE_TX;

  // Set callback function and events
  txDtmTestCmd.common.runtime.callback = LL_rclTestCallback;
  txDtmTestCmd.common.runtime.lrfCallbackMask.value = 0;
  txDtmTestCmd.common.runtime.rclCallbackMask.value =  RCL_EventLastCmdDone.value;
  // post the command
  RCL_Command_submit(MAP_llScheduler_getHandle(LL_TASK_ID_STANDARD_BLE), (RCL_Command_Handle)&txDtmTestCmd);

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This function is used to initiate a BLE PHY level Receive Test in Direct Test
 * Mode where the DUT receives test reference packets at fixed intervals. This
 * test will make use of the nanoRisc Raw Data Transmit and Receive task. The
 * received packets are verified based on the CRC, and metrics are kept.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_DirectTestRxTest( uint8 rxChan,
                                uint8 rxPhy )
{
  // verify input parameters are valid
  if ( rxChan >= LL_TOTAL_NUM_RF_CHAN )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check that we are idle until combo states are supported
  if ( llState != LL_STATE_IDLE )
  {
    return( LL_STATUS_ERROR_UNEXPECTED_STATE_ROLE );
  }

  // command initialization
  rxTestCmd = RCL_CmdBle5GenericRx_DefaultRuntime();
  // set channel
  // Note: The input is the RCL Channel not the BLE Channel.
  rxTestCmd.channel = rxChan + LL_DTM_RCL_CHANNEL_OFFSET;
  // set command parameters and output pointers
  rxTestCmd.ctx = &rxTestParam;
  rxTestCmd.stats = &rxTestOut;
  // set the default params such as Access Address and CRC init
  rxTestParam = RCL_CtxGenericRx_DefaultRuntime();
  rxTestParam.maxPktLen = 0xFF;
  /* Discard the data */
  rxTestParam.config.discardRxPackets = TRUE;
  // set the default statistics
  rxTestOut = RCL_StatsGenericRx_DefaultRuntime();
  rxTestOut.config.accumulate = 1;
  rxTestOut.config.activeUpdate = 1;
  // clear the counters
  rxTestOut.nRxOk      = 0;
  rxTestOut.nRxNok     = 0;
  rxTestOut.nRxFifoFull = 0;
  rxTestOut.lastRssi   = LL_RF_RSSI_UNDEFINED;
  rxTestOut.lastTimestamp  = 0;

  // save parameters
  dtmInfo->rfChan      = rxChan;
  dtmInfo->packetLen   = 0;
  dtmInfo->packetType  = LL_DIRECT_TEST_PAYLOAD_UNDEFINED;
  dtmInfo->numPackets  = 0;
  dtmInfo->numRxCrcNOK = 0;
  dtmInfo->lastRssi    = LL_RF_RSSI_UNDEFINED;

   // set state/role
   // Note: This must precede call to llRfInit!
   llState = LL_STATE_DIRECT_TEST_MODE_RX;

   uint8 blePhy;
   uint8 blePhyOpts;
   llDirectTestConvertLlPhyToBlePhy(rxPhy, &blePhy, &blePhyOpts);

   rxTestCmd.common.phyFeatures = blePhy;

   // Set callback function and events
   rxTestCmd.common.runtime.callback = LL_rclTestCallback;
   rxTestCmd.common.runtime.lrfCallbackMask.value = 0;
   rxTestCmd.common.runtime.rclCallbackMask.value =  RCL_EventLastCmdDone.value;
   // post the command
   RCL_Command_submit(MAP_llScheduler_getHandle(LL_TASK_ID_STANDARD_BLE), (RCL_Command_Handle)&rxTestCmd);

   return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This function is used to end the Direct Test Transmit or Direct Test Receive
 * tests executing in Direct Test mode. When the raw task is ended, the
 * LL_DirectTestEndDoneCback callback is called. If a Direct Test mode operation
 * is not currently active, an error is returned.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_DirectTestEnd( void )
{
  RCL_CommandStatus status;
  RCL_Command_Handle cmd;
  RCL_StopType stopType;
  RCL_CommandStatus stopTypeStatus;

  // first check if we are already in a Direct TX or RX test
  if ( (llState != LL_STATE_DIRECT_TEST_MODE_TX) &&
       (llState != LL_STATE_DIRECT_TEST_MODE_RX) )
  {
    return( LL_STATUS_ERROR_UNEXPECTED_STATE_ROLE );
  }

  // Get the stop commands parameters
  if(llState == LL_STATE_DIRECT_TEST_MODE_TX)
  {
    cmd = (RCL_Command_Handle)&txDtmTestCmd;
    stopType = RCL_StopType_Graceful;
    stopTypeStatus = RCL_CommandStatus_GracefulStopApi;
  }
  else
  {
    cmd = (RCL_Command_Handle)&rxTestCmd;
    stopType = RCL_StopType_Hard;
    stopTypeStatus = RCL_CommandStatus_HardStopApi;
  }

  // Issue STOP command
  status = RCL_Command_stop(cmd, stopType);

  // In case of error in 'RCL_Command_stop'
  if ( (status != RCL_CommandStatus_Active)   &&
       (status != RCL_CommandStatus_Finished) &&
       (status != stopTypeStatus))
  {
      return( LL_STATUS_ERROR_HW_FAILURE );
  }

  // return parameters depend on which test we were running
  if ( llState == LL_STATE_DIRECT_TEST_MODE_TX )
  {
    // generate a callback for the packet report
    // Note: For TX, the number of received packets is always zero.
    MAP_LL_DirectTestEndDoneCback( 0, LL_DIRECT_TEST_MODE_TX );
  }
  else if ( llState == LL_STATE_DIRECT_TEST_MODE_RX )
  {
    dtmInfo->numPackets  = rxTestOut.nRxOk + rxTestOut.nRxNok + rxTestOut.nRxFifoFull;
    dtmInfo->lastRssi    = LL_CHECK_LAST_RSSI( rxTestOut.lastRssi );
    dtmInfo->numRxCrcNOK = rxTestOut.nRxNok;

    // generate a callback for the packet report
    MAP_LL_DirectTestEndDoneCback( rxTestOut.nRxOk, LL_DIRECT_TEST_MODE_RX );
  }
  else
  {
        /* this else clause is required, even if the
           programmer expects this will never be reached
           Fix Misra-C Required: MISRA.IF.NO_ELSE */
  }

  // back to Idle
  llState = LL_STATE_IDLE;

  return( LL_STATUS_SUCCESS );
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This LL API is used to receive the accepted Host connection parameters in
 * response to the LE Remote Connection Parameter Request Event.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_RemoteConnParamReqReply( uint16 connId,
                                       uint16 connIntervalMin,
                                       uint16 connIntervalMax,
                                       uint16 connLatency,
                                       uint16 connTimeout,
                                       uint16 minLen,
                                       uint16 maxLen )
{
  llConnState_t *connPtr;
  llStatus_t     status;

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connId)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // get the connection info based on the connection ID
  connPtr = MAP_llDataGetConnPtr( connId );

#ifdef QUAL_TEST
    // Support non-zero offset value in the Remote Connection Parameter Request Reply command.
    // In case the reference connection event count is lower than the current one make them equal in
    // order to avoid returning the host "BAD PARAMETERS" error
    // Note: test cases LL/CON/MAS/BV-33-C, LL/CON/SLA/BV-26-C, LL/CON/SLA/BV-32-C
    if ( (connPtr->connParams.offset0 != 0) &&
         (connPtr->connParams.refConnEvtCount != 0) &&
         (connPtr->currentEvent > connPtr->connParams.refConnEvtCount) )
    {
      connPtr->connParams.refConnEvtCount = connPtr->currentEvent;
    }
#endif

  // validate parameters.
  // TBD: Extend this feature by comparing/evaluating parameters, and
  //      determine the best connection interval between min/max,
  //      taking periodicity into account.
  // check the basic connection parameters are valid
  // check the combination of the basic connection parameters are valid
  // check the parameters for the connection parameters request are valid
  // check the parameters for APTO are valid
  if ( MAP_llValidateConnParams( connPtr,
                                 connIntervalMin,
                                 connIntervalMax,
                                 connLatency,
                                 connTimeout,
                                 connPtr->currentEvent,
                                 connPtr->connParams.periodicity,
                                 connPtr->connParams.refConnEvtCount,
                                 (uint16 *)&connPtr->connParams.offset0 ) )
  {
    // parameters from Host are invalid

    // so use what was provided by peer, but indicate error to Host
    // ALT: Better to just return here? or proceed with peer's parameters?
    status = LL_STATUS_ERROR_BAD_PARAMETER;
  }
  else // parameters are valid, so replace
  {
    // save parameters
    connPtr->connParams.intervalMin = connIntervalMin;
    connPtr->connParams.intervalMax = connIntervalMax;
    connPtr->connParams.latency     = connLatency;
    connPtr->connParams.timeout     = connTimeout;
    //
    connPtr->paramUpdate.connInterval = connIntervalMax;
    connPtr->paramUpdate.peripheralLatency = connLatency;
    connPtr->paramUpdate.connTimeout  = connTimeout;

    status = LL_STATUS_SUCCESS;
  }

  // check the connection role
  if ( connPtr->llTask->taskID == LL_TASK_ID_CENTRAL )
  {
    // replace place holder with Update Parameter
    MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_CONNECTION_UPDATE_IND,
                          LL_CTRL_DUMMY_PLACE_HOLDER_TRANSMIT );
  }
  else // connPtr->llTask->taskID == LL_TASK_ID_PERIPHERAL
  {
    // replace place holder with Update Parameter
    MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_CONNECTION_PARAM_RSP,
                          LL_CTRL_DUMMY_PLACE_HOLDER_TRANSMIT );
  }

  return( status );
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This LL API is used to reeive the Host's rejection to the LE Remote
 * Connection Parameter Request event from the Controller.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_RemoteConnParamReqNegReply( uint16 connId,
                                          uint8  reason )
{
  llConnState_t *connPtr;
  llStatus_t     status;

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connId)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // get the connection info based on the connection ID
  connPtr = MAP_llDataGetConnPtr( connId );

  // check if the Reject Indication Extended feature is supported
  if ( !(connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_REJECT_EXT_IND) )
  {
    // control packet received for features that are not supported
    connPtr->unknownCtrlType = LL_CTRL_CONNECTION_PARAM_REQ;

    // setup/send an Unknown Response
    MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP,
                          LL_CTRL_DUMMY_PLACE_HOLDER_TRANSMIT );
  }
  else // the Reject Indication Extended is at least supported
  {
    // remove control packet from processing queue and drop through
    MAP_llDequeueCtrlPkt( connPtr );

    // send Reject Indication
    // Note: In V4.1, the only valid rason is
    //       LL_STATUS_ERROR_UNACCEPTABLE_CONN_PARAMETERS.
    MAP_llSendReject( connPtr,
                      LL_CTRL_CONNECTION_PARAM_REQ,
                      reason );
  }

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This API is called by the HCI to update the connection parameters by
 * initiating a connection update control procedure.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ConnUpdate( uint16 connId,
                          uint16 connIntervalMin,
                          uint16 connIntervalMax,
                          uint16 connLatency,
                          uint16 connTimeout,
                          uint16 minLength,
                          uint16 maxLength )
{
  llStatus_t     status;
  llConnState_t *connPtr;

  // unused input parameter; PC-Lint error 715.
  (void)minLength;
  (void)maxLength;

  // sanity checks again to be sure we don't start with bad parameters
  if ( LL_INVALID_CONN_TIME_PARAM( connIntervalMin,
                                   connIntervalMax,
                                   connLatency,
                                   connTimeout ) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check if CI/SL/LSTO is valid (i.e. meets the requirements)
  // Note: LSTO > (1 + Peripheral Latency) * (Connection Interval * 2)
  // Note: The CI * 2 requirement based on ESR05 V1.0, Erratum 3904.
  // Note: LSTO time is normalized to units of 1.25ms (i.e. 10ms = 8 * 1.25ms).
  if ( LL_INVALID_CONN_TIME_PARAM_COMBO(connIntervalMax, connLatency, connTimeout) )
  {
    return( LL_STATUS_ERROR_ILLEGAL_PARAM_COMBINATION );
  }

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connId)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // get connection info
  connPtr = MAP_llDataGetConnPtr( connId );

  // check if an updated parameters control procedure is already what's pending
  // Note: The Central can not send a connection parameter response.
  if ( ((connPtr->ctrlPktInfo.ctrlPktCount > 0) &&
        ((connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_CONNECTION_UPDATE_IND) ||
         (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_CONNECTION_PARAM_REQ)  ||
         (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_CONNECTION_PARAM_RSP))) ||
        (connPtr->pendingParamUpdate == PARAM_UPDATE_PENDING) )
  {
    return( LL_STATUS_ERROR_CTRL_PROC_ALREADY_ACTIVE );
  }

  // check connection role
  if ( connPtr->llTask->taskID == LL_TASK_ID_PERIPHERAL )
  {
    // check if this operation is permitted by a peripheral device
    // Note: The peripheral can only issue a connection parameter request if the
    //       connection parameter request feature is supported!
    // Note: The error status returned depends on whether it is our device that
    //       does not support this feature, or the peer does not support the
    //       feature.
    if ( !(deviceFeatureSet.featureSet[0] & (uint8)LL_FEATURE_CONN_PARAMS_REQ) )
    {
      return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
    }
    else if ( !(connPtr->featureSetInfo.featureSet[0] & (uint8)LL_FEATURE_CONN_PARAMS_REQ) )
    {
      return( LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE );
    }
  }

  // save parameters
  connPtr->connParams.intervalMin = connIntervalMin;
  connPtr->connParams.intervalMax = connIntervalMax;
  connPtr->connParams.latency     = connLatency;
  connPtr->connParams.timeout     = connTimeout;

  //else // connPtr->llTask->taskID == LL_TASK_ID_CENTRAL
  // for now, handle Central and Peripheral the same way.
  {
    // no control procedure currently active, so set this one up
    // set the window size (units of 1.25ms)
    connPtr->paramUpdate.winSize = LL_WINDOW_SIZE;

    // set the window offset (units of 1.25ms)
    connPtr->paramUpdate.winOffset = LL_WINDOW_OFFSET;

    // set the relative offset of the number of events for the update to take place
    // Note: The absolute event number will be determined at the time the packet
    //       is placed in the TX FIFO.
    // Note: The central should allow a minimum of 6 connection events that the
    //       peripheral will be listening for before the instant occurs.
    // ALT:  Check if the number of events based on SL and min listening is more
    //       than the connection's LSTO. That is:
    //       if paramUpdateEvent>expiration: paramUpdateEvent=expiration-(SL+1).
    //       connPtr->paramUpdateEvent = (connPtr->curParam.peripheralLatency+1) * LL_INSTANT_NUMBER_MIN;
    //       Since the spec says the Peripheral is supposed to listen after it gets
    //       an update until its ACK is ACK'ed by the Central, that the instant
    //       should be SL+1+6.
    connPtr->paramUpdateEvent = (connPtr->curParam.peripheralLatency+1) + LL_INSTANT_NUMBER_MIN;

    // determine the connection interval based on min and max values
    // Note: minLength and maxLength are informational.
    // ALT: Use algorithm to determine proper connection interval.
    connPtr->paramUpdate.connInterval = connIntervalMax;

    // save the new connection peripheral latency to be used by the peer
    connPtr->paramUpdate.peripheralLatency = connLatency;

    // save the new connection supervisor timeout
    connPtr->paramUpdate.connTimeout  = connTimeout;

    // indicate the Host initiated this update
    // ALT: Remove this.
    connPtr->connParamReqFlags.hostInitiated = TRUE;
  }
  // a flag that determines whether the device initiated the param update proc.
  connPtr->procInitiator = TRUE;
  // check if the connection parameter control procedure is supported
  // Note: The spec says an Update can be sent if either the central or peripheral
  //       or both do not support the Connection Parameters Request procedure.
  // Note: To know the feature set of the peer, a feature exchange would be
  //       needed. But whether one has occurred or not on this connection, the
  //       connections's feature set can be used as it is initialized with this
  //       device's feature set.
  // Note: There could be a race condition where after the check to see if this
  //       feature is supported, it is removed from the feature set because the
  //       peer doesn't support it. In this case, the connection request will
  //       be rejected, which is fine.
  if ( !(connPtr->featureSetInfo.featureSet[0] & (uint8)LL_FEATURE_CONN_PARAMS_REQ) )
  {
    // feature isn't supported, so issue the update as a central
    // Note: Early on we checked if this device is a peripheral and the connection
    //       parameter request procedure was not supported. So to get here when
    //       this feature is not supported, we must be a central device.
    MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_CONNECTION_UPDATE_IND );
  }
  else // connection parameter request feature supported...
  {
    // ...so whether a central or peripheral, issue the connection parameter request
    // Note: Either we are a peripheral, in which case we can only send a connection
    //       parameter request, or we are a central and this feature is
    //       supported, so we can send the connection parameter request
    MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_CONNECTION_PARAM_REQ );
  }

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ( ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * This function is used to read the transmit power level used for BLE
 * advertising channel packets. Currently, only two settings are possible, a
 * standard setting of 0 dBm, and a maximum setting of 4 dBm.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadAdvChanTxPower( int8 *txPower )
{
  // sanity check
  if ( txPower == NULL )
  {
    return( LL_STATUS_ERROR_INVALID_PARAMS);
  }

  // return Adv TX power level based on settings
  *txPower = MAP_llGetTxPower();

  // check if Tx output power is valid
  if ( *txPower == LL_TX_POWER_INVALID )
  {
    return( LL_STATUS_ERROR_PARAM_OUT_OF_RANGE );
  }

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

/*******************************************************************************
 * This API is called by the HCI to provide the controller with the Long Term
 * Key (LTK) for encryption. This command is actually a reply to the link
 * layer's LL_EncLtkReqCback, which provided the random number and encryption
 * diversifier received from the Central during an encryption setup.
 *
 * Note: The key parameter is byte ordered LSO to MSO.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EncLtkReply( uint16 connId,
                           uint8  *key )
{
  llStatus_t     status;
  llConnState_t *connPtr;

  // check parameters
  if ( key == NULL )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connId)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // get connection info
  connPtr = MAP_llDataGetConnPtr( connId );

  // make sure connection is in Peripheral role
  if ( connPtr->llTask->taskID != LL_TASK_ID_PERIPHERAL )
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }

  // save LTK
  for (uint8 i=0; i<LL_ENC_LTK_LEN; i++)
  {
    // store LTK in MSO..LSO byte order, per FIPS 197 (AES)
    connPtr->encInfo.LTK[(LL_ENC_LTK_LEN-i)-1] = key[i];
  }

  // indicate the host has provided the key
  connPtr->encInfo.LTKValid = TRUE;

  // got the LTK, so schedule the start of encryption
  // Note: Head of queue should be a DUMMY packet.
  MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_START_ENC_REQ,
                        LL_CTRL_UNDEFINED_PKT );

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This API is called by the HCI to indicate to the controller that the Long
 * Term Key (LTK) for encryption can not be provided. This command is actually a
 * reply to the link layer's LL_EncLtkReqCback, which provided the random number
 * and encryption diversifier received from the Central during an encryption
 * setup. How the LL responds to the negative reply depends on whether this is
 * part of a start encryption or a re-start encryption after a pause. For the
 * former, an encryption request rejection is sent to the peer device. For the
 * latter, the connection is terminated.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EncLtkNegReply( uint16 connId )
{
  llStatus_t     status;
  llConnState_t *connPtr;

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connId)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // get connection info
  connPtr = MAP_llDataGetConnPtr( connId );

  // make sure connection is in Peripheral role
  if ( connPtr->llTask->taskID != LL_TASK_ID_PERIPHERAL )
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }

  // check if this is during a start or a re-start encryption procedure
  if ( connPtr->encInfo.encRestart == TRUE )
  {
    // indicate the peer requested this termination
    connPtr->termInfo.reason = LL_ENC_KEY_REQ_REJECTED;

    // queue control packet for processing
    // Note: Head of queue should be a DUMMY packet.
    MAP_llReplaceCtrlPkt( connPtr,
                          LL_CTRL_TERMINATE_IND,
                          LL_CTRL_UNDEFINED_PKT );
  }
  else // during a start encryption
  {
    // check if the Reject Indication Extended feature is supported
    if ( connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_REJECT_EXT_IND )
    {
      connPtr->rejectIndExt.rejectOpcode = LL_CTRL_ENC_REQ;
      connPtr->rejectIndExt.errorCode    = LL_STATUS_ERROR_PIN_OR_KEY_MISSING;

      // and reject the encryption request
      // Note: Head of queue should be a DUMMY packet.
      MAP_llReplaceCtrlPkt( connPtr,
                            LL_CTRL_REJECT_EXT_IND,
                            LL_CTRL_UNDEFINED_PKT );
    }
    else // Reject Extended Indication not supported
    {
      // set the encryption rejection error code
      // Note: Same as LL_ENC_KEY_REQ_REJECTED.
      connPtr->encInfo.encRejectErrCode = LL_STATUS_ERROR_PIN_OR_KEY_MISSING;

       // and reject the encryption request
      // Note: Head of queue should be a DUMMY packet.
      MAP_llReplaceCtrlPkt( connPtr,
                            LL_CTRL_REJECT_IND,
                            LL_CTRL_UNDEFINED_PKT );
    }
  }

  return( LL_STATUS_SUCCESS );
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          LE_SetAdvSetRandAddr
 *
 * @brief       This API is used to set the random device address used in
 *              the Controller for the advertiser's address contained in
 *              the advertising PDUs for the advertising set specified by
 *              the advertising handle.
 *
 *              Note: Parameters per Vol 2, Part E, 7.8.52.
 *
 *
 * input parameters
 *
 * @param       pCmdParams - Pointer to input parameters:
 *                           - advertising handle
 *                           - random address
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 *              LL_STATUS_ERROR_COMMAND_DISALLOWED
 *              LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER
 */
llStatus_t LE_SetAdvSetRandAddr( aeRandAddrCmd_t *pCmdParams )
{
  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( pCmdParams->handle );

  // check if we have an Adv Set
  if ( pAdvSet == NULL )
  {
    // handle is not found
    return( LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER );
  }

  // check if Adv Set is connectable and enabled
  if ( TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps) &&
       (pAdvSet->advMode == LL_ADV_MODE_ON) )
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }

  // set AE random address for this Adv Set
  MAP_osal_memcpy( pAdvSet->ownAddr, pCmdParams->randAddr, B_ADDR_LEN );

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          LE_SetExtAdvParams
 *
 * @brief       This API is used to set the advertising parameters for an
 *              advertisement set.
 *
 *              Note: Parameters per Vol 2, Part E, 7.8.52.
 *
 * input parameters
 *
 * @param       pCmdParams - Pointer to input parameters:
 *                           - advertising handle
 *                           - advertising event properties
 *                           - primary advertising interval min
 *                           - primary advertising interval max
 *                           - primary advertising channel map
 *                           - own address type
 *                           - peer address type
 *                           - peer address
 *                           - advertising filter policy
 *                           - advertising Tx power
 *                           - primary advertising PHY
 *                           - secondary advertising max skip
 *                           - secondary advertising PHY
 *                           - advertising SID
 *                           - scan request notification enable
 *
 * output parameters
 *
 * @param       pRtnParams - Pointer to output parameters:
 *                           - selected Tx power
 *
 * @return      LL_STATUS_SUCCESS
 *              LL_STATUS_ERROR_OUT_OF_HEAP
 *              LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED
 *              LL_STATUS_ERROR_BAD_PARAMETER
 *              LL_STATUS_ERROR_COMMAND_DISALLOWED
 */
llStatus_t LE_SetExtAdvParams( aeSetParamCmd_t *pCmdParams,
                               aeSetParamRtn_t *pRtnParams )
{
  advSet_t *pAdvSet;
  uint32_t primIntMinTemp = BUILD_UINT32(pCmdParams->primIntMin[0],
                                         pCmdParams->primIntMin[1],
                                         pCmdParams->primIntMin[2],
                                         0);
  uint32_t primIntMaxTemp = BUILD_UINT32(pCmdParams->primIntMax[0],
                                         pCmdParams->primIntMax[1],
                                         pCmdParams->primIntMax[2],
                                         0);

#ifdef USE_PERIODIC_ADV
  // Get the periodic adv set according to the handle
  llPeriodicAdvSet_t *pPeriodicAdv = llGetPeriodicAdv(pCmdParams->handle);

  // If periodic advertising is enabled and connectable, scannable,
  // legacy, or anonymous advertising is specified, return an error
  if(((pPeriodicAdv != NULL) &&
     (pPeriodicAdv->state != PERIODIC_ADV_STATE_DISABLE)) &&
     (TST_AE_PROPS_CONN(pCmdParams->eventProps) ||
      TST_AE_PROPS_SCAN(pCmdParams->eventProps) ||
      TST_AE_PROPS_LEGACY(pCmdParams->eventProps) ||
      TST_AE_PROPS_OMIT_ADVA(pCmdParams->eventProps)))
  {
      return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // If periodic advertising is enabled and the secondary PHY
  // does not specify the PHY currently being used for the
  // periodic advertising, return an error
  if(((pPeriodicAdv != NULL) &&
     (pPeriodicAdv->state != PERIODIC_ADV_STATE_DISABLE)) &&
     (pCmdParams->secPhy != pPeriodicAdv->phy))
  {
      return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }
#endif

  // check if primIntMin and primIntMax are within a valid range
  // Note: For HDC, the interval parameters are not used and shall be ignored.
  if ( !TST_AE_PROPS_HDC_DIR(pCmdParams->eventProps)                          &&
       (primIntMinTemp < AE_INTERVAL_MIN  ||
       primIntMinTemp > AE_INTERVAL_MAX  ||
       primIntMaxTemp < AE_INTERVAL_MIN  ||
       primIntMaxTemp > AE_INTERVAL_MAX ))
  {
    return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
  }

  // check the rest of the parameters
  // Note: Spec says primIntMin and primIntMax should not be the same, not shall
  //       not be the same, so allowing here.
  // Note: For HDC, the interval parameters are not used and shall be ignored.
  if ( (!TST_AE_PROPS_HDC_DIR(pCmdParams->eventProps)                    &&
        ((pCmdParams->primIntMax < pCmdParams->primIntMin)               ||
         ((pCmdParams->ownAddrType != LL_DEV_ADDR_TYPE_PUBLIC)       &&
          (pCmdParams->ownAddrType != LL_DEV_ADDR_TYPE_RANDOM)       &&
          (pCmdParams->ownAddrType != LL_DEV_ADDR_TYPE_PUBLIC_ID)    &&
          (pCmdParams->ownAddrType != LL_DEV_ADDR_TYPE_RANDOM_ID))))          ||
       ((pCmdParams->filterPolicy != LL_ADV_AL_POLICY_ANY_REQ)            &&
        (pCmdParams->filterPolicy != LL_ADV_AL_POLICY_AL_SCAN_REQ)        &&
        (pCmdParams->filterPolicy != LL_ADV_AL_POLICY_AL_CONNECT_IND)     &&
        (pCmdParams->filterPolicy != LL_ADV_AL_POLICY_AL_ALL_REQ))            ||
       ((pCmdParams->primPhy != AE_PHY_1_MBPS)   &&
        (pCmdParams->primPhy != AE_PHY_2_MBPS)   &&
        (pCmdParams->primPhy != AE_PHY_CODED_S2) &&
        (pCmdParams->primPhy != AE_PHY_CODED_S8)) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // mask off unused primary advertising channels
  pCmdParams->primChanMap &= LL_ADV_CHAN_ALL;

  // make sure there's at least one advertising channel that can be used
  if ( !pCmdParams->primChanMap )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // get the handle, or allocate it
  pAdvSet = MAP_LL_GetAdvSet( pCmdParams->handle,
                              LE_ALLOCATE_ADV_SET );

  // check if we have an adv set
  if ( pAdvSet == NULL )
  {
    // check if the pointer is NULL because we reached the max allowed Adv Sets
    if ( MAP_LL_CountAdvSets( LE_COUNT_ALL_ADV_SETS ) == MAP_LE_ReadNumSupportedAdvSets() )
    {
      return( LL_STATUS_ERROR_DUE_TO_LIMITED_RESOURCES );
    }
    else // seems we're out of memory
    {
      return( LL_STATUS_ERROR_OUT_OF_HEAP );
    }
  }

  // check if this Adv Set is already
  if ( pAdvSet->advMode == LL_ADV_MODE_ON )
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }


  // If the advertising set already contains advertising data or scan response data, extended
  // advertising is being used, and the length of the data is greater than the maximum that
  // the Controller can transmit within the longest possible auxiliary advertising segment
  // consistent with the parameters, the Controller shall return the error code Packet Too Long (0x45)
  if ((pAdvSet->pAdvData->dataLen > 0 || pAdvSet->pScanRspData->dataLen > 0) &&
      !TST_AE_PROPS_LEGACY(pCmdParams->eventProps)                           &&
      (MAP_llExtAdvTxTime(pAdvSet, pCmdParams->secPhy, pCmdParams->primPhy) >
      (primIntMaxTemp * LL_CONNECTION_SLOT_TIME)))
  {
    return( LL_STATUS_ERROR_PACKET_TOO_LONG );
  }

  // at this point the set is OFF so we can safely update the parameters
  // assume there will be a problem with the parameters
  pAdvSet->paramValid = FALSE;

  // save the pointer for parameters
  pAdvSet->pAdvParam = pCmdParams;

  // count the number of primary advertising channels
  // Note: Need to count number of channels, not for allocation, but
  //       for determining the auxPtr offset if secondary channel used.
  pAdvSet->numPrimChans = 0;
  for (uint8 i=0; i<LL_MAX_NUM_ADV_CHAN; i++)
  {
    if ( pCmdParams->primChanMap & BV(i) )
    {
      // check if the first primary channel
      if ( ++pAdvSet->numPrimChans == 1 )
      {
        pAdvSet->firstPrimChan = LL_ADV_BASE_CHAN + i;
      }
    }
  }

  // check if this will be a legacy advertisement
  if ( TST_AE_PROPS_LEGACY(pCmdParams->eventProps) )
  {
    // check the primary advertising channel PHY
    if ( pCmdParams->primPhy != AE_PHY_1_MBPS )
    {
      return( LL_STATUS_ERROR_BAD_PARAMETER );
    }

    // remap event properties to legacy advertisment event
    // check allowed bit combos for Legacy advertising
    switch( *((uint8 *)(pCmdParams->eventProps)) )
    {
      case AE_EVT_TYPE_CONN_SCAN_ADV_IND:
        pAdvSet->advEvtType = LL_ADV_CONNECTABLE_UNDIRECTED_EVT;
        break;
      case AE_EVT_TYPE_CONN_DIR_LDC_ADV_DIRECT_IND:
        pAdvSet->advEvtType = LL_ADV_CONNECTABLE_LDC_DIRECTED_EVT;
        break;
      case AE_EVT_TYPE_CONN_DIR_HDC_ADV_DIRECT_IND:
        pAdvSet->advEvtType = LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT;
        break;
      case AE_EVT_TYPE_SCAN_UNDIR_ADV_SCAN_IND:
        pAdvSet->advEvtType = LL_ADV_SCANNABLE_UNDIRECTED_EVT;
        break;
      case AE_EVT_TYPE_NONCONN_NONSCAN_ADV_NONCONN_IND:
        pAdvSet->advEvtType = LL_ADV_NONCONNECTABLE_UNDIRECTED_EVT;
        break;
      default:
        return( LL_STATUS_ERROR_BAD_PARAMETER );
        // Note: Unreachable statement generates compiler warning!
        //break;
    }

#if defined(CTRL_CONFIG) && ((CTRL_CONFIG & ADV_NCONN_CFG) && !(CTRL_CONFIG & ADV_CONN_CFG))
    // be sure non-connectable Adv only
    if ( (pAdvSet->advEvtType == LL_ADV_CONNECTABLE_UNDIRECTED_EVT)   ||
         (pAdvSet->advEvtType == LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT) ||
         (pAdvSet->advEvtType == LL_ADV_CONNECTABLE_LDC_DIRECTED_EVT) )
    {
      // connectable advertising is not permitted in this configuration
      return( LL_STATUS_ERROR_UNEXPECTED_PARAMETER );
    }
#elif defined(CTRL_CONFIG) && (!(CTRL_CONFIG & ADV_NCONN_CFG) && (CTRL_CONFIG & ADV_CONN_CFG))
    // be sure connectable Adv only
    if ( (pAdvSet->advEvtType == LL_ADV_NONCONNECTABLE_UNDIRECTED_EVT) ||
         (pAdvSet->advEvtType == LL_ADV_SCANNABLE_UNDIRECTED_EVT) )
    {
      // non-connectable advertising is not permitted in this configuraiton
      return( LL_STATUS_ERROR_UNEXPECTED_PARAMETER );
    }
#endif // ADV_NCONN_CFG & !ADV_CONN_CFG
  }
  else // not a legacy command
#ifndef USE_AE
  {
    return ( LL_STATUS_ERROR_UNEXPECTED_PARAMETER );
  }
#else
  {
    uint8 status = MAP_llSetExtendedAdvParams(pAdvSet,pCmdParams);
    if (status != LL_STATUS_SUCCESS)
    {
      return status;
    }
  }

  // ignore ownAddrType for anonymous Adv (bit 5=1)
#endif

  // check if the Host has no preference
  // Note: The Adv Event Properties bit only indicates whether TxPower
  //       should be included in the advertising PDU, not whether the
  //       Tx Power parameter should be used.
  // TODO: DOES THE MIN NUMBER OF USED CHANNELS API AFFECT THIS? IF SO,
  //       HOW IS THE NUMBER OF CHANNELS DETERMINED WHEN THERE'S NO
  //       CONNECTIONS? ETC.
  if ( pCmdParams->txPower == AE_TX_POWER_NO_PREFERENCE )
  {
    // Host doesn't care, so use the current value
    pAdvSet->txPowerIndex = curTxPowerVal;
  }
  else // use the Host's choice
  {
    // use the Host's choice
    pAdvSet->txPowerIndex = RfBleDpl_getTxPowerByTxPowerDbm(pCmdParams->txPower, 0);
    if (!RfBleDpl_txPowerIsValid( pAdvSet->txPowerIndex))
    {
      /* Check valid Tx Parameter. In case invalid - return with error */
      return LL_STATUS_ERROR_BAD_PARAMETER;
    }
  }

  // if there's a return parameter pointer...
  if ( pRtnParams )
  {
    // set choice in return parameters
    pRtnParams->txPower = RfBleDpl_getTxPowerDbm(pAdvSet->txPowerIndex);
  }
  // set the advertiser address based on the HCI's address type preference
  if ( LL_IS_ADDR_IDENTITY_TYPE(pCmdParams->ownAddrType) )
  {
    // set our address and address type
    pAdvSet->ownAddrType = pCmdParams->ownAddrType;
    pAdvSet->actualOwnAddrType = pCmdParams->ownAddrType;
    // set our address public or random
    MAP_osal_memcpy( pAdvSet->ownAddr,
                     ADDRTYPE_TO_OWNADDR(pCmdParams->ownAddrType),
                     B_ADDR_LEN );
  }
  else // check to use public/random identity type for own address/type
  {
#ifdef QUAL_TEST
    if ( MAP_LL_PRIV_IsNRPA( MASK_ID_ADDRTYPE(pCmdParams->ownAddrType), ADDRTYPE_TO_OWNADDR( pCmdParams->ownAddrType )) )
    {
      // set address type
      pAdvSet->ownAddrType = LL_DEV_ADDR_TYPE_RANDOM;
      pAdvSet->actualOwnAddrType = pCmdParams->ownAddrType;

      // Random Private Non-Resolvable Address, so use our Identity Address
      MAP_osal_memcpy( pAdvSet->ownAddr,
                       ADDRTYPE_TO_OWNADDR(pCmdParams->ownAddrType),
                       B_ADDR_LEN );

    }
    else
#endif
    // check if Local IRK is zero (i.e. there is no Local IRK)
    if ( MAP_LL_PRIV_IsZeroIRK( resolvingList[LOCAL_RL_INDEX].IRK ) )
    {
      // ensure own address is an identity address (public or random static)
      if ( !MAP_LL_PRIV_IsIDA( pCmdParams->ownAddrType,
                               ADDRTYPE_TO_OWNADDR(pCmdParams->ownAddrType) ) )
      {
        return( LL_STATUS_ERROR_BAD_PARAMETER );
      }

      // no Local RPA, so use our Identity Address
      MAP_osal_memcpy( pAdvSet->ownAddr,
                       ADDRTYPE_TO_OWNADDR(pCmdParams->ownAddrType),
                       B_ADDR_LEN );

      // set our address type
      pAdvSet->ownAddrType = MASK_ID_ADDRTYPE(pCmdParams->ownAddrType);
      pAdvSet->actualOwnAddrType = MASK_ID_ADDRTYPE(pCmdParams->ownAddrType);
    }
    else // valid Local IRK, so use RPA
    {
      // set our RPA address
      MAP_osal_memcpy( pAdvSet->ownAddr,
                       resolvingList[LOCAL_RL_INDEX].RPA,
                       B_ADDR_LEN );

      // set our address type
      // Note: Used to set the OTA value via advParam.pDeviceAddr, which is
      //       1 bit, so same as LL_DEV_ADDR_TYPE_RANDOM.
      // Note: In the case where our own address type is public identity, the
      //       value saved should really be LL_DEV_ADDR_TYPE_PUBLIC_ID,
      //       but since advInfo->ownAddrType is used to set the OTA value via
      //       advParam.pDeviceAddr, the wrong OTA address type would result
      //       as only 1 bit is used. Thus, same as LL_DEV_ADDR_TYPE_RANDOM.
      pAdvSet->ownAddrType = LL_DEV_ADDR_TYPE_RANDOM_ID;
      pAdvSet->actualOwnAddrType = pCmdParams->ownAddrType;
    }

    // save local identity address and address type in RL
    resolvingList[LOCAL_RL_INDEX].idAddrType = MASK_ID_ADDRTYPE(pCmdParams->ownAddrType);

    MAP_osal_memcpy( resolvingList[LOCAL_RL_INDEX].idAddr,
                     ADDRTYPE_TO_OWNADDR(pCmdParams->ownAddrType),
                     B_ADDR_LEN );
  }

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
    // check if Directed Advertising
    if ( TST_AE_PROPS_DIR(pCmdParams->eventProps) )
    {
      // Note: Per Vol 6, Part B, Section 6.2.2, if the own address type is
      //		ID, then use the peer's ID if not in the RL, or in the RL with
      //		 an invalid IRK. If in the RL with a valid IRK, use RPA.
      if ( LL_IS_ADDR_TYPE_RPA(pCmdParams->ownAddrType) )
      {
        uint8 rlIndex = MAP_LL_PRIV_FindPeerInRL( resolvingList,
                                                  MASK_ID_ADDRTYPE(pCmdParams->peerAddrType),
                                                  pCmdParams->peerAddr );

        // check if the Peer's ID address was found in the RL
        if ( rlIndex == INVALID_RESOLVE_LIST_INDEX )
        {
          // it was not, so use the Peer's ID address
          pAdvSet->peerAddrType = pCmdParams->peerAddrType;

          MAP_osal_memcpy( pAdvSet->peerAddr,
                           pCmdParams->peerAddr,
                           B_ADDR_LEN );
        }
        else // found the Peer ID address
        {
          // check if the IRK was set to zero by the Host
          if ( MAP_LL_PRIV_IsZeroIRK( resolvingList[rlIndex].IRK ) )
          {
            // it was, so use the Peer's ID address
            pAdvSet->peerAddrType = pCmdParams->peerAddrType;

            MAP_osal_memcpy( pAdvSet->peerAddr,
                             pCmdParams->peerAddr,
                             B_ADDR_LEN );
          }
          else // Peer IRK is valid
          {
            // so generate Peer RPA using Peer's IRK
            MAP_LL_PRIV_GenerateRPA( resolvingList[rlIndex].IRK,
                                     resolvingList[rlIndex].RPA );

            // set the Peer's address and address type
            pAdvSet->peerAddrType = LL_DEV_ADDR_TYPE_RANDOM;

            MAP_osal_memcpy( pAdvSet->peerAddr,
                             resolvingList[rlIndex].RPA,
                             B_ADDR_LEN );
          }
        }
      }
      else // own address type is public or random
      {
        // set the Peer's address and address type
        pAdvSet->peerAddrType = pCmdParams->peerAddrType;
        MAP_osal_memcpy( pAdvSet->peerAddr,
                         pCmdParams->peerAddr,
                         B_ADDR_LEN );
      }
    }
#endif // ADV_CONN_CFG

  // parameters are okay thus far
  pAdvSet->paramValid = TRUE;

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          LE_SetExtAdvData
 *
 * @brief       This API is used to set the Extended Advertising Data.
 *
 *              Note: A common routine is used for advertising data and scan
 *                    response data.
 *
 *              Note: Parameters per Vol 2, Part E, 7.8.54.
 *
 * input parameters
 *
 * @param       pCmdParams - Pointer to input parameters:
 *                           - advertising handle
 *                           - operation
 *                           - fragment preference
 *                           - advertising data length
 *                           - advertising data
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 *              LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER
 *              LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED
 *              LL_STATUS_ERROR_BAD_PARAMETER
 */
llStatus_t LE_SetExtAdvData( aeSetDataCmd_t *pCmdParams )
{
  advSet_t *pAdvSet;

  // get the Adv Set, if there is one
  pAdvSet = MAP_LL_SearchAdvSet( pCmdParams->handle );

  // check if we have an Adv Set and Adv Params
  // in case we don't return an error
  // the adv set and params should have been allocated at hci level beforehand
  // note that for legacy adv where it is allowed to set data and scan rsp before the adv params
  // we set a default adv parameters so it should be exist by now
  if ( pAdvSet == NULL || pAdvSet->pAdvParam == NULL)
  {
    return( LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER );
  }

  // check if this will be a legacy advertisement
  if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
  {
    // check max data length for legacy advertising PDUs
    if ( pCmdParams->dataLen > LL_MAX_ADV_DATA_LEN )
    {
      pCmdParams->dataLen = 0;
      pCmdParams->pData   = NULL;
      return( LL_STATUS_ERROR_BAD_PARAMETER );
    }
  }
#ifdef USE_AE
  else // !legacy
  {
    // Check the length in the payload isn't greater than max allowed
    // payload per the spec, or the max allowed per our device
    // ALT: Consider making max data length ll_config or even user config.
    if ( pCmdParams->dataLen > maxExtAdvDataLen )
    {
      pCmdParams->dataLen = 0;
      pCmdParams->pData   = NULL;
      return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
    }
    // Check that in connectable mode headers size plus adv data length does not exceed
    if ( TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps) )
    {
        // Count the amount of bytes in the header in order to verify the size doesn't exceed
        // Note: ACAD is currently not supported so its header size is not added.
        uint8 extHdrCount = EXTHDR_INFO_SIZE + EXTHDR_FLAGS_SIZE + EXTHDR_FLAG_ADVA_SIZE + EXTHDR_FLAG_ADI_SIZE;
        extHdrCount += (TST_AE_PROPS_DIR(pAdvSet->pAdvParam->eventProps) ? EXTHDR_FLAG_TARGETA_SIZE : 0);
        extHdrCount += (TST_EXTHDR_FLAG(pAdvSet->pAdvParam->eventProps[0], EXTHDR_FLAG_CTEINFO) ? EXTHDR_FLAG_CTEINFO_SIZE : 0);
        extHdrCount += (TST_EXTHDR_FLAG(pAdvSet->pAdvParam->eventProps[0], EXTHDR_FLAG_AUXPTR) ? EXTHDR_FLAG_AUXPTR_SIZE : 0);
        extHdrCount += (TST_EXTHDR_FLAG(pAdvSet->pAdvParam->eventProps[0], EXTHDR_FLAG_SYNCINFO) ? EXTHDR_FLAG_SYNCINFO_SIZE : 0);
        extHdrCount += (TST_AE_PROPS_TX_PWR(pAdvSet->pAdvParam->eventProps) ? EXTHDR_FLAG_TXPWR_SIZE : 0);

        if ( pCmdParams->dataLen > AE_MAX_ADV_PAYLOAD_LEN - extHdrCount )
        {
            pCmdParams->dataLen = 0;
            pCmdParams->pData   = NULL;
            return( LL_STATUS_ERROR_PARAM_OUT_OF_RANGE );
        }
    }
  }
#endif
  return( MAP_LE_AE_SetData( pCmdParams, LE_AE_EXT_DATA_CMD_ADV ) );
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          LE_SetExtScanRspData
 *
 * @brief       This API is used to set the Extended Scan Response Data.
 *
 *              Note: A common routine is used for advertising data and scan
 *                    response data.
 *
 *              Note: Parameters per Vol 2, Part E, 7.8.55.
 *
 * input parameters
 *
 * @param       pCmdParams - Pointer to input parameters:
 *                           - advertising handle
 *                           - operation
 *                           - fragment preference
 *                           - scan response data length
 *                           - scan response data
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 *              LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER
 *              LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED
 *              LL_STATUS_ERROR_BAD_PARAMETER
 */
llStatus_t LE_SetExtScanRspData( aeSetDataCmd_t *pCmdParams )
{
  advSet_t *pAdvSet;

  // get the Adv Set, if there is one
  pAdvSet = MAP_LL_SearchAdvSet( pCmdParams->handle );

  // check if we have an Adv Set and Adv Params
  // in case we don't return an error
  // the adv set and params should have been allocated at hci level beforehand
  // note that for legacy adv where it is allowed to set data and scan rsp before the adv params
  // we set a default adv parameters so it should be exist by now
  if ( pAdvSet == NULL || pAdvSet->pAdvParam == NULL)
  {
    return( LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER );
  }

  // check if this will be a legacy advertisement
  if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
  {
    // check max data length for legacy advertising PDUs
    if ( pCmdParams->dataLen > LL_MAX_ADV_DATA_LEN )
    {
      pCmdParams->dataLen = 0;
      pCmdParams->pData   = NULL;
      return( LL_STATUS_ERROR_BAD_PARAMETER );
    }
  }
#ifdef USE_AE
  else // !legacy
  {
    // check the length in the payload isn't greater than max allowed
    // payload per the spec, or the max allowed per our device
    // ALT: Consider making max data length ll_config or even user config.
     if ( pCmdParams->dataLen > maxExtAdvDataLen )
    {
      pCmdParams->dataLen = 0;
      pCmdParams->pData   = NULL;
      return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
    }
  }
#endif
  return( MAP_LE_AE_SetData( pCmdParams, LE_AE_EXT_DATA_CMD_SCAN_RSP ) );
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          LE_SetExtAdvEnable
 *
 * @brief       This API is used to request the Controller to enable or disable
 *              one or more advertising sets.
 *
 *              Note: Currently, only one advertising set at a time is allowed.
 *
 *              Note: Parameters per Vol 2, Part E, 7.8.56.
 *
 * input parameters
 *
 * @param       pCmdParams - Pointer to input parameters:
 *                           - enable
 *                           - number of sets
 *                           - advertising handle
 *                           - duration
 *                           - max extended advertising events
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 *              LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER
 *              LL_STATUS_ERROR_BAD_PARAMETER
 *              LL_STATUS_ERROR_OUT_OF_HEAP
 */
llStatus_t LE_SetExtAdvEnable( aeEnableCmd_t *pCmdParams )
{
  advSet_t *pAdvSet;
#ifdef USE_PERIODIC_ADV
  llPeriodicAdvSet_t *pPeriodicAdv;
#endif
  // check parameter pointer
  if ( !pCmdParams )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check if a direct test mode or modem test is in progress
  if ( (llState == LL_STATE_DIRECT_TEST_MODE_TX)            ||
       (llState == LL_STATE_DIRECT_TEST_MODE_RX)            ||
       (llState == LL_STATE_MODEM_TEST_TX)                  ||
       (llState == LL_STATE_MODEM_TEST_RX)                  ||
       (llState == LL_STATE_MODEM_TEST_TX_FREQ_HOPPING) )
  {
    return( LL_STATUS_ERROR_UNEXPECTED_STATE_ROLE );
  }

  // get the Adv Set, if there is one
  pAdvSet = MAP_LL_SearchAdvSet( pCmdParams->handle );

  // check if we have an Adv Set
  if ( pAdvSet == NULL )
  {
    return( LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER );
  }

  // save handle for llConnExists
  // Note: Created this "global" so the API to llConnExists doesn't change.
  aeCurAdvEnableHandle = pCmdParams->handle;
  if ( llState == LL_STATE_IDLE )
  {
    // initialize current advertising handle to the proper one
    aeCurHandle = pCmdParams->handle;
  }
  // if extended advertise is scannable the scan paramas must not be null
#ifdef USE_AE
  if ( ( pCmdParams->enable == LL_ADV_MODE_ON ) &&
       (!( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )) &&
       ( TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps) ) &&
       (( pAdvSet->pScanRspData == NULL ) || ( pAdvSet->pScanRspData->pData == NULL )))
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }
#endif
  // sanity checks again to be sure we don't start with bad parameters, and
  // also check duration parameter (nothing to check for max events)
  if ( (pAdvSet->paramValid == FALSE)                                         ||
       ((pAdvSet->advEvtType == LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT) &&
        (pCmdParams->duration != 0) && (pCmdParams->duration > 128))          ||
       (pCmdParams->numSets > AE_MAX_ADV_SETS_TO_ENABLE_DISABLE) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // allocate the radio structure and hook it into the adv set
  // Note: Only done if this structure doesn't already exist.
  MAP_llAllocRfMem( pAdvSet );

  // check if we got it
  if ( pAdvSet->pRfCmds == NULL )
  {
    return( LL_STATUS_ERROR_OUT_OF_HEAP );
  }

  // save the pointer to advertising data parameters
  pAdvSet->pEnable = pCmdParams;

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  // check for combo state restrictions for connectable Adv and number of connections
  if ( (pCmdParams->enable == LL_ADV_MODE_ON)  &&
       ((pAdvSet->advEvtType == LL_ADV_CONNECTABLE_UNDIRECTED_EVT)   ||
       (pAdvSet->advEvtType == LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT)  ||
       (pAdvSet->advEvtType == LL_ADV_CONNECTABLE_LDC_DIRECTED_EVT) ) )
  {
    // check if at least one connection is allowed
    if ( maxNumConns == 0 )
    {
      // connectable advertising is not permitted if configured for zero connections
      return( LL_STATUS_ERROR_UNEXPECTED_PARAMETER );
    }

    // check if we're already at the max number of connections
    if ( llConns.numActiveConns == maxNumConns )
    {
      return( LL_STATUS_ERROR_CONNECTION_LIMIT_EXCEEDED );
    }

    // check if we're already in a connection as a Peripheral with this device as a Central
    // Note: This test is done ONLY if the advertisement is a directed advertisement
    if (  TST_AE_PROPS_DIR(pAdvSet->pAdvParam->eventProps) &&
          MAP_llConnExists(pAdvSet->peerAddr,
                           pAdvSet->peerAddrType ) )
    {
      return( HCI_ERROR_CODE_ACL_CONN_ALREADY_EXISTS );
    }
  }
#endif // ADV_CONN_CFG

  // check if the number of sets is not zero
  // TODO: IF NUMBER OF SETS == 0, DISABLE ALL ADV SETS!
  if ( pCmdParams->numSets == 0 )
  {
    return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
  }

  // check the enable operation
  if ( pCmdParams->enable == LL_ADV_MODE_ON )
  {
    llStatus_t status;

    // clear event count
    // Note: This counter is cleared whether we're starting an advertisement
    //       for the first time, or simply resetting max events.
    pAdvSet->maxAdvEvts = 0;
    // clear duration time
    pAdvSet->durationExpireTime = 0;

    // check if this adv set is already enabled
    if ( pAdvSet->advMode == LL_ADV_MODE_ON )
    {
      // update max Adv events
      pAdvSet->pEnable->maxEvents = pCmdParams->maxEvents;

      // update random address (not supported)

      return( LL_STATUS_SUCCESS );
    }

    // init health check
    MAP_llHealthUpdate(LL_STATE_EXT_ADV);
    // not, so enable
    pAdvSet->advMode = LL_ADV_MODE_ON;

    // clear flag indicating first advertisement event
    pAdvSet->firstAdvEvt = 0;

    // used to track if radio was halted , initialize with 0.
    pAdvSet->advHalted = 0;

    // get a task block for this BLE state/role
    // Note: There will always be a valid pointer, so no NULL check required.
    // Note: All Adv Sets share the same llTask block.
    pAdvSet->llTask = MAP_llAllocTask( LL_TASK_ID_ADVERTISER );
    if (pAdvSet->llTask == NULL)
    {
      return (LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED);
    }

    halIntState_t cs;

    HAL_ENTER_CRITICAL_SECTION(cs);

    advSet_t *pAdvSetEntry = advSetList;

    while( pAdvSetEntry )
    {
      // check if the specific adv set llTask was assigned.
      // if one of the sets is enabled all adv set llTasks should be assigned.

      pAdvSetEntry->llTask = pAdvSet->llTask;

      pAdvSetEntry = pAdvSetEntry->next;
    }
    HAL_EXIT_CRITICAL_SECTION(cs);


    // set startup callback
    pAdvSet->llTask->setup = MAP_llExtAdvSchedSetup;

    // check if this will be a legacy advertisement
    if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
    {
      // setup the radio command
      if ( (status = MAP_llSetupExtAdvLegacy( pAdvSet )) != LL_STATUS_SUCCESS )
      {
        return( status );
      }
    }
#ifdef USE_AE
    else // !legacy
    {
      // Avoid initialize the advertising channel counter
      // in order to keep the secondary channel random.
      //pAdvSet->auxChanCounter = 0;

      // find secondary channel index
      // Note: Must set before calling llSetupExtAdv!
      pAdvSet->auxChanIndex = MAP_llNextChanIndex( pAdvSet->auxChanCounter );
      //check for periodic adv
#ifdef USE_PERIODIC_ADV
      pPeriodicAdv = MAP_llGetPeriodicAdv(pAdvSet->pAdvParam->handle);
      if ((pPeriodicAdv != NULL) && (pPeriodicAdv->state >= PERIODIC_ADV_STATE_PENDING_ENABLE))
      {
        SET_EXTHDR_FLAG( pAdvSet->auxHdrFlags,EXTHDR_FLAG_SYNCINFO );
        // set auxPtr and ADI flags in extHdrFlags
        SET_EXTHDR_FLAG( pAdvSet->extHdrFlags,EXTHDR_FLAG_AUXPTR | EXTHDR_FLAG_ADI );
      }
#endif
      // setup the radio command
      if ( (status = MAP_llSetupExtAdv( pAdvSet )) != LL_STATUS_SUCCESS )
      {
        if( status == LL_STATUS_ERROR_UNEXPECTED_PARAMETER )
        {
          HAL_ENTER_CRITICAL_SECTION(cs);

          // There was an error building the advertising TX packets. Remove the node from the sorted list
          MAP_llRemoveAdvSortedEntry(pAdvSet);
          // free task and teardown privacy if need be
          MAP_llEndExtAdvTask( pAdvSet );

          HAL_EXIT_CRITICAL_SECTION(cs);
        }
        return( status );
      }

#ifdef USE_PERIODIC_ADV
      if ((pPeriodicAdv != NULL) && (pPeriodicAdv->state == PERIODIC_ADV_STATE_PENDING_ENABLE))
      {
        MAP_llSetupPeriodicAdv(pAdvSet);
      }
#endif
    }
#endif

    // TODO: check if duration is used, and it is long enough for the OTA transfer

    // check if connectable data has been truncated
    if ( TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps) &&
         pAdvSet->dataLen && pAdvSet->pData                &&
         (pAdvSet->dataLen > pAdvSet->maxAvailData) )
    {
      // check if there's a registered callback
      if ( MAP_llCheckCBack(LL_CBACK_EXT_ADV_DATA_TRUNCATED) )
      {
        // Note: Local used to ensure callee can't modify the value.
        aeAdvTrucData_t truncData;

        // initialize read-only parameters
        truncData.handle          = pAdvSet->pAdvParam->handle;
        truncData.advDataLen      = pAdvSet->dataLen;
        truncData.availAdvDataLen = pAdvSet->maxAvailData;

        // data is going to be truncated, so let the Host know
        MAP_llExtAdvCBack( LL_CBACK_EXT_ADV_DATA_TRUNCATED, &truncData );
      }
    }

    // check if there are currently no tasks that are active
    if ( llState == LL_STATE_IDLE )
    {
      // only if address resolution is enabled
      if ( privInfo.addrResolution )
      {
        // check type of advertisement
        // Note: No Scan/Init response when advertising non-connectable.
        if ( TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps) ||
             TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps) )
        {
          // (Radio core using dynamic filter list)
          if ( llUserConfig.useDFL == TRUE )
          {
            if ( LL_DFL_Init( LL_DFL_GetDynamicFilterlist(), LL_DFL_GetRankTable()) != LL_STATUS_SUCCESS )
            {
              return (LL_STATUS_ERROR_INVALID_PARAMS);
            }
          }
          else // !(Radio core using dynamic filter list)
          {
            // enables and clears the extended accept list
            MAP_LL_PRIV_SetupPrivacy( alTable );
          }
        }
      }
      // set LL state based on Adv event type
      llState = LL_STATE_EXT_ADV;

      // set control state
      pAdvSet->advMode = LL_ADV_MODE_ON;

      // schedule this task
      MAP_llScheduler();
    }
    else
    {
      sortedAdv_t *advSortedEntry = llGetAdvSortedEntry(pAdvSet);

      if ((advSortedEntry != NULL) && (pAdvSet->pRfCmds != NULL))
      {
        /*
         * Disable Task preemption for LOKI
         * Preemption allows a task to preempt scheduled command in case there is
         * enough time for a new command to be executed. Then, previously de-scheduled
         * command is returned to the queue and executed on it's time.
         * This feature is not supported in LOKI, so currently is disabled
         */
        uint32 endTime = (pAdvSet->advEvtType != LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT)?
                         (pAdvSet->advStartTime + US_TO_RAT_TICKS(advSortedEntry->timeConsume)):
                         (pAdvSet->advStartTime + ((aeRf_t *)pAdvSet->pRfCmds)->advCmd.common.timing.relHardStopTime);
        // check RF command preemption
        MAP_llCheckRfCmdPreemption(endTime,pAdvSet->priority);
      }
    }
  }
  else // enable == LL_ADV_MODE_OFF
  {
    halIntState_t cs;

    // check if already disabled
    // Note: Disabling an already disabled advertising set has no effect.
    if ( pAdvSet->advMode == LL_ADV_MODE_OFF )
    {
      return( LL_STATUS_SUCCESS );
    }

    HAL_ENTER_CRITICAL_SECTION(cs);

    // Disabling the advertising set identified by the Advertising_Handle[i]
    // parameter does not disable any periodic advertising associated with
    // that set.
    MAP_llRemoveAdvSortedEntry(pAdvSet);

    // check if this task is to be halted
    // Note: We can halt the radio if this is the only task and if this is
    //       the advertise task.
    if ( (aeCurAdvEnableHandle == aeCurHandle) &&
         (MAP_llGetCurrentTask() == pAdvSet->llTask) )
    {
      // stop the command
      MAP_llHaltRadio( pAdvSet->llTask->command );

      HAL_EXIT_CRITICAL_SECTION(cs);
    }
    else // not the only task AND not the current task
    {
      // free task and teardown privacy if need be
      MAP_llEndExtAdvTask( pAdvSet );

      HAL_EXIT_CRITICAL_SECTION(cs);

      // send the Adv Set End callback, if enabled
      MAP_llSendAdvSetEndEvent( pAdvSet );
    }
  }

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          LE_ReadMaxAdvDataLen
 *
 * @brief       This API is used to read the maximum length of data supported
 *              by the Controller for use as advertisement data or scan response
 *              data in an advertising event or as periodic advertisement data.
 *              The maximum amount may be fragmented across multiple PDUs.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The maximum advertising data length allowed by Controller.
 */
uint16 LE_ReadMaxAdvDataLen( void )
{
  return( maxExtAdvDataLen );
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          LE_ReadNumSupportedAdvSets
 *
 * @brief       This API is used to read the maximum number of advertising sets
 *              supported by the advertising Controller at the same time. The
 *              number of advertising sets that can be supported is not fixed,
 *              and the Controller can change it at any time, based on available
 *              memory.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The maximum number of advertising sets supported by Controller.
 */
uint8 LE_ReadNumSupportedAdvSets( void )
{
  return( maxSupportedAdvSets );
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          LE_RemoveAdvSet
 *
 * @brief       This API is used to remove an advertising set from the
 *              Controller.
 *
 * input parameters
 *
 * @param       handle - Advertising set handle.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS,
 *              LL_STATUS_ERROR_COMMAND_DISALLOWED,
 *              LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER
 */
llStatus_t LE_RemoveAdvSet( uint8 handle )
{
  halIntState_t cs;

  HAL_ENTER_CRITICAL_SECTION(cs);
  {
    advSet_t *pPre = advSetList;
    advSet_t *pCur = advSetList;

    while( pCur )
    {
      if ( pCur->pAdvParam->handle == handle )
      {
        // check if this advertising set is enabled
        if ( pCur->advMode == LL_ADV_MODE_ON )
        {
          HAL_EXIT_CRITICAL_SECTION(cs);
          return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
        }

        // chain over the handle to be removed
        pPre->next = pCur->next;

        if ( pCur == advSetList )
        {
          advSetList = pCur->next;
        }

        // check if an RF command was allocated
        if ( pCur->pRfCmds )
        {
          // remove advertising set
          MAP_osal_mem_free( pCur->pRfCmds );
        }

        // remove advertising set
        MAP_osal_mem_free( pCur );

        // ALT: Check if advSetList is left correctly set?

        HAL_EXIT_CRITICAL_SECTION(cs);
        return( LL_STATUS_SUCCESS );
      }

      // not found yet, so on to next entry in list
      pPre = pCur;
      pCur = pCur->next;
    }
  }

  HAL_EXIT_CRITICAL_SECTION(cs);
  return( LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER );
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          LE_ClearAdvSets
 *
 * @brief       This API is used to remove all existing advertising sets from
 *              the Controller.
 *
 *              Note: All advertising sets are cleared on HCI reset.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS,
 */
llStatus_t LE_ClearAdvSets( void )
{
  // first check if any Adv Sets are enabled
  if ( MAP_LL_CountAdvSets( LE_COUNT_ENABLED_ADV_SETS ) != 0 )
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }
  else // no Adv Sets or none that are enabled
  {
    halIntState_t cs;

    HAL_ENTER_CRITICAL_SECTION(cs);
    advSet_t *pCurr = advSetList;

    while( pCurr )
    {
      // move head to next, if any
      advSetList = pCurr->next;

      // check if an RF command was allocated
      if ( pCurr->pRfCmds )
      {
        // remove advertising set
        MAP_osal_mem_free( pCurr->pRfCmds );
      }

      // Getting current ll task
      if ( MAP_llGetCurrentTask() == pCurr->llTask )
      {
        // Halt the radio
        MAP_llHaltRadio( pCurr->llTask->command );
      }

      // free the associated task block
      // Note: If the last task, llState will be set to Idle.
      MAP_llFreeTask( &pCurr->llTask );

      // free what was at head
      MAP_osal_mem_free( pCurr );

      // point to next head, if any
      pCurr = advSetList;
    }

    // Sanity Check:
    // Note: The list head pointer should be NULL.
    LL_ASSERT( advSetList == NULL );
    HAL_EXIT_CRITICAL_SECTION(cs);

    return( LL_STATUS_SUCCESS );
  }
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 * @fn          LE_SetExtScanParams
 *
 * @brief       This API is used to set the extended scan parameters to be used
 *              on the advertising channels.
 *
 *              Note: Currently, only one Scan Type, Scan Interval, and Scan
 *                    Window is supported for a PHY at a time.
 *
 * input parameters
 *
 * @param       pCmdParams - Pointer to input parameters:
 *                           - own address type
 *                           - scanning filter policy
 *                           - scanning PHYs
 *                           - scan type
 *                           - scan interval
 *                           - scan window
 *                           - type/interval/window for next PHY...
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 *              LL_STATUS_ERROR_OUT_OF_HEAP
 *              LL_STATUS_ERROR_BAD_PARAMETER
 *              LL_STATUS_ERROR_COMMAND_DISALLOWED
 */
llStatus_t LE_SetExtScanParams( aeSetScanParamCmd_t *pCmdParams )
{
  // check if we already have extScanInfo
  // Note: This structure is pre-allocated upon startup, and should always be
  //       valid by the time this function is called.
  if ( !extScanInfo )
  {
    return( LL_STATUS_ERROR_OUT_OF_HEAP );
  }

  // check the parameters
  if ( (!pCmdParams)                                                               ||
       (
        (pCmdParams->scanFilterPolicy != LL_SCAN_AL_POLICY_ANY_ADV_PKTS_EXT)    &&
        (pCmdParams->scanFilterPolicy != LL_SCAN_AL_POLICY_USE_ACCEPT_LIST_EXT)  &&
        (pCmdParams->scanFilterPolicy != LL_SCAN_AL_POLICY_ANY_ADV_PKTS)        &&
        (pCmdParams->scanFilterPolicy != LL_SCAN_AL_POLICY_USE_ACCEPT_LIST))        ||
       (
         (pCmdParams->ownAddrType != LL_DEV_ADDR_TYPE_PUBLIC_ID)                &&
         (pCmdParams->ownAddrType != LL_DEV_ADDR_TYPE_RANDOM_ID)                &&
         (pCmdParams->ownAddrType != LL_DEV_ADDR_TYPE_PUBLIC)                   &&
         (pCmdParams->ownAddrType != LL_DEV_ADDR_TYPE_RANDOM))                     ||
       // only 1M and Code PHY allowed
       (!(pCmdParams->scanPhys & LL_PHY_1_MBPS)                                 &&
        !(pCmdParams->scanPhys & LL_PHY_CODED))                                    ||
       // when only one PHY is specified, parameters are always at index 0
       (((pCmdParams->scanPhys == LL_PHY_1_MBPS)                                ||
         (pCmdParams->scanPhys == LL_PHY_CODED))                                &&
        ((pCmdParams->extScanParam[0].scanWindow >
          pCmdParams->extScanParam[0].scanInterval)                             ||
         (pCmdParams->extScanParam[0].scanInterval < AE_EXT_SCAN_MIN_TIME)      ||
         (pCmdParams->extScanParam[0].scanWindow   < AE_EXT_SCAN_MIN_TIME)      ||
         ((pCmdParams->extScanParam[0].scanType != LL_SCAN_PASSIVE)             &&
          (pCmdParams->extScanParam[0].scanType != LL_SCAN_ACTIVE))))              ||
       ((pCmdParams->scanPhys == (LL_PHY_1_MBPS + LL_PHY_CODED))                &&
        ((pCmdParams->extScanParam[0].scanWindow >
          pCmdParams->extScanParam[0].scanInterval)                             ||
         (pCmdParams->extScanParam[0].scanInterval < AE_EXT_SCAN_MIN_TIME)      ||
         (pCmdParams->extScanParam[0].scanWindow   < AE_EXT_SCAN_MIN_TIME)      ||
         ((pCmdParams->extScanParam[0].scanType != LL_SCAN_PASSIVE)             &&
          (pCmdParams->extScanParam[0].scanType != LL_SCAN_ACTIVE))             ||
         (pCmdParams->extScanParam[1].scanWindow >
          pCmdParams->extScanParam[1].scanInterval)                             ||
         (pCmdParams->extScanParam[1].scanInterval < AE_EXT_SCAN_MIN_TIME)      ||
         (pCmdParams->extScanParam[1].scanWindow   < AE_EXT_SCAN_MIN_TIME)      ||
         ((pCmdParams->extScanParam[1].scanType != LL_SCAN_PASSIVE)             &&
          (pCmdParams->extScanParam[1].scanType != LL_SCAN_ACTIVE)))) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check if Scan is currently enabled
  if ( extScanInfo->scanMode == LL_SCAN_START )
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }

  // at this point the set is OFF so we can safely update the parameters
  // assume there will be a problem with the parameters
  extScanInfo->paramValid = FALSE;

  // always set scan index to zero
  // Note: If both PHYs are present, we'll start with 1M.
  // Note: This should not be done in enable because Scan is disabled when
  //       duration is used with period, and re-enabled on next period.
  extScanIndex = 0;

  // set the scanner's address based on the HCI's address type preference
  if ( LL_IS_ADDR_IDENTITY_TYPE(pCmdParams->ownAddrType) )
  {
    // set our address and address type
    extScanInfo->ownAddrType = pCmdParams->ownAddrType;

    // set our address public or random
    MAP_osal_memcpy( extScanInfo->ownAddr,
                     ADDRTYPE_TO_OWNADDR(pCmdParams->ownAddrType),
                     B_ADDR_LEN );

    // store local address and address type to the resolving list
    resolvingList[LOCAL_RL_INDEX].idAddrType = MASK_ID_ADDRTYPE(pCmdParams->ownAddrType);

    MAP_osal_memcpy( resolvingList[LOCAL_RL_INDEX].idAddr,
                     ADDRTYPE_TO_OWNADDR(pCmdParams->ownAddrType),
                     B_ADDR_LEN );
  }
  else // public/random identity type
  {
#ifdef QUAL_TEST
    if ( MAP_LL_PRIV_IsNRPA( MASK_ID_ADDRTYPE(pCmdParams->ownAddrType), ADDRTYPE_TO_OWNADDR( pCmdParams->ownAddrType )) )
    {
      // set address type
      extScanInfo->ownAddrType = LL_DEV_ADDR_TYPE_RANDOM;

      // Random Private Non-Resolvable Address, so use our Identity Address
      MAP_osal_memcpy( extScanInfo->ownAddr,
                       ADDRTYPE_TO_OWNADDR(pCmdParams->ownAddrType),
                       B_ADDR_LEN );

    }
    else
#endif
    // check if Local IRK is zero (i.e. there is no Local IRK)
    if ( MAP_LL_PRIV_IsZeroIRK( resolvingList[LOCAL_RL_INDEX].IRK ) )
    {
      // ensure own address is an identity address (public)
      if ( !MAP_LL_PRIV_IsIDA( pCmdParams->ownAddrType,
                               ADDRTYPE_TO_OWNADDR(pCmdParams->ownAddrType) ) )
      {
        return( LL_STATUS_ERROR_BAD_PARAMETER );
      }

      // no Local RPA, so use our Identity Address
      MAP_osal_memcpy( extScanInfo->ownAddr,
                       ADDRTYPE_TO_OWNADDR(pCmdParams->ownAddrType),
                       B_ADDR_LEN );

      // set OTA address type
      extScanInfo->ownAddrType = MASK_ID_ADDRTYPE(pCmdParams->ownAddrType);
    }
    else // valid Local IRK, so use RPA
    {
      // use our RPA address
      MAP_osal_memcpy( extScanInfo->ownAddr,
                       resolvingList[LOCAL_RL_INDEX].RPA,
                       B_ADDR_LEN );

      // set our address type
      // Note: Used to set the OTA value via extScanParam.pDeviceAddr, which is
      //       1 bit, so same as LL_DEV_ADDR_TYPE_RANDOM.
      // Note: In the case where our own address type is public identity, the
      //       value saved should really be LL_DEV_ADDR_TYPE_PUBLIC_ID,
      //       but since advInfo->ownAddrType is used to set the OTA value via
      //       advParam.pDeviceAddr, the wrong OTA address type would result
      //       as only 1 bit is used. Thus, same as LL_DEV_ADDR_TYPE_RANDOM.
      extScanInfo->ownAddrType = LL_DEV_ADDR_TYPE_RANDOM_ID;
    }

    // store local identity address and address type
    resolvingList[LOCAL_RL_INDEX].idAddrType = MASK_ID_ADDRTYPE(pCmdParams->ownAddrType);

    MAP_osal_memcpy( resolvingList[LOCAL_RL_INDEX].idAddr,
                     ADDRTYPE_TO_OWNADDR(pCmdParams->ownAddrType),
                     B_ADDR_LEN );
  }

  // by default - reset ownAddrRclCtx value so it will not be used
  MAP_osal_memset( extScanInfo->ownAddrRclCtx,
                   0x0,
                   B_ADDR_LEN );

  // save the pointer
  extScanInfo->pScanParam = pCmdParams;

  // parameters are okay thus far
  extScanInfo->paramValid = TRUE;

  return( LL_STATUS_SUCCESS );
}
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 * @fn          LE_SetExtScanEnable
 *
 * @brief       This API is used to set the extended scan parameters to be used
 *              on the advertising channels.
 *
 *              Note: Currently, only one Scan Type, Scan Interval, and Scan
 *                    Window is supported.
 *
 * input parameters
 *
 * @param       pCmdParams - Pointer to input parameters:
 *                           - enable
 *                           - filter duplicates
 *                           - scanning PHYs
 *                           - duration
 *                           - period
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 *              LL_STATUS_ERROR_BAD_PARAMETER
 *              LL_STATUS_ERROR_OUT_OF_HEAP
 */
llStatus_t LE_SetExtScanEnable( aeEnableScanCmd_t *pCmdParams )
{
  llStatus_t status;

  // check parameter pointer
  if ((!extScanInfo) || (!pCmdParams) || (!extScanInfo->pScanParam) ||
     ((!extScanInfo->paramValid) && (pCmdParams->enable == LL_SCAN_START)))
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check if a direct test mode or modem test is in progress
  if ( (llState == LL_STATE_DIRECT_TEST_MODE_TX)            ||
       (llState == LL_STATE_DIRECT_TEST_MODE_RX)            ||
       (llState == LL_STATE_MODEM_TEST_TX)                  ||
       (llState == LL_STATE_MODEM_TEST_RX)                  ||
       (llState == LL_STATE_MODEM_TEST_TX_FREQ_HOPPING) )
  {
    return( LL_STATUS_ERROR_UNEXPECTED_STATE_ROLE );
  }

  // check if scan parameters are valid
  // Note: Assuming here that scan interval needs to be less than duration.
  if ( (!pCmdParams)                                                                   ||
       ((pCmdParams->enable != LL_SCAN_STOP)                                    &&
        (pCmdParams->enable != LL_SCAN_START))                                         ||
       ((pCmdParams->dupFiltering != LL_FILTER_REPORTS_DISABLE)                 &&
        (pCmdParams->dupFiltering != LL_FILTER_REPORTS_ENABLE)                  &&
        (pCmdParams->dupFiltering != LL_FILTER_REPORTS_RESET_EACH_SCAN_PERIOD))        ||
       ((pCmdParams->duration != AE_EXT_SCAN_DURATION_DISABLED)                 &&
        (pCmdParams->period   != AE_EXT_SCAN_PERIOD_DISABLED)                   &&
        (((uint32)pCmdParams->duration * 10) >= ((uint32)pCmdParams->period * 1280)))  ||
       // when only one PHY is specified, parameters are always at index 0
       ((pCmdParams->duration != AE_EXT_SCAN_DURATION_DISABLED)                 &&
        ((extScanInfo->pScanParam->scanPhys == LL_PHY_1_MBPS)                   ||
         (extScanInfo->pScanParam->scanPhys == LL_PHY_CODED))                   &&
        ((extScanInfo->pScanParam->extScanParam[0].scanInterval >=
         ((uint32)pCmdParams->duration * 16))                                   ||
         (extScanInfo->pScanParam->extScanParam[0].scanWindow   >=
         ((uint32)pCmdParams->duration * 16))))                                        ||
       ((pCmdParams->duration != AE_EXT_SCAN_DURATION_DISABLED)                 &&
        (extScanInfo->pScanParam->scanPhys == (LL_PHY_1_MBPS+LL_PHY_CODED))     &&
        ((extScanInfo->pScanParam->extScanParam[0].scanInterval >=
         ((uint32)pCmdParams->duration * 16))                                   ||
         (extScanInfo->pScanParam->extScanParam[0].scanWindow   >=
         ((uint32)pCmdParams->duration * 16))                                   ||
         (extScanInfo->pScanParam->extScanParam[1].scanInterval >=
         ((uint32)pCmdParams->duration * 16))                                   ||
         (extScanInfo->pScanParam->extScanParam[1].scanWindow   >=
         ((uint32)pCmdParams->duration * 16)))) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // Sanity Check
  // Note: The 2M PHY is not allowed for primary channels.
  LL_ASSERT( extScanInfo->pScanParam->scanPhys != LL_PHY_2_MBPS );

  // save the pointer to advertising data parameters
  extScanInfo->pEnable = pCmdParams;

  if ( pCmdParams->enable == LL_SCAN_START )
  {
    // ignore command if already scanning
    if ( extScanInfo->scanMode == LL_SCAN_START )
    {
      // TODO: UPDATE DURATION AND PERIOD, APPLY DUPLICATE FILTERING AND RAND ADDR.

      return( LL_STATUS_SUCCESS );
    }

    // get a task block for this BLE state/role
    // Note: There will always be a valid pointer, so no NULL check required.
    extScanInfo->llTask = MAP_llAllocTask( LL_TASK_ID_SCANNER );
    if (extScanInfo->llTask == NULL)
    {
      return (LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED);
    }
    // pointer to first radio operation command
    extScanInfo->llTask->command = (uint32)&extScanCmd;

    // callback function for scheduler
    extScanInfo->llTask->setup = MAP_llExtScanSchedSetup;

    // used to track the scan start state
    extScanInfo->scanStartState = AE_SCAN_START_STATE_FIRST;

    // init variable used to track last legacy ADV PDU
    extScanInfo->lastLegacyAdv = LL_PKT_TYPE_RESERVED;

    // set flag to indicate the scan timeout is based on duration (vs period)
    extScanInfo->timingFlag = AE_SCAN_DURATION_TIMEOUT;

    // calculate the remainder of time after duration expires, to next period
    // Note: Time converted to ms.
    extScanInfo->scanPeriodLeft = ((uint32)extScanInfo->pEnable->period * 1280) -
                                  ((uint32)extScanInfo->pEnable->duration * 10);

    // by default - reset ownAddrRclCtx value so it will not be used
    MAP_osal_memset( extScanInfo->ownAddrRclCtx,
                     0x0,
                     B_ADDR_LEN );

    // Update the default priority extended scan variable.
    extScanInfo->priority = qosDefaultPriorityScnParameter;

    // Setup the radio command
    if ( (status = MAP_llSetupExtScan()) != LL_STATUS_SUCCESS )
    {
      return( status );
    }

    // Update health check
    MAP_llHealthUpdate(LL_STATE_SCAN);
    // Indicate we are actively scanning
    extScanInfo->scanMode = LL_SCAN_START;

    // Only if address resolution is enabled
    if ( privInfo.addrResolution )
    {
      // Report packets where advertiser address is an unknown addres
      extScanParam.addrModePeer = TRUE;
      extScanParam.acceptAllRpaConnectRsp = TRUE;

      if ( extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST_EXT ||
           extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_ANY_ADV_PKTS_EXT )
      {
        extScanParam.scanExtFilterPolicy = TRUE;
      }

      // Check if no other tasks are currently active
      if ( llState == LL_STATE_IDLE )
      {
        // Check the Scan accept list policy
        if ( (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST) ||
             ((privInfo.addrResolution) &&
             (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST_EXT)) )
        {
          // Setup AL table and extended AL table
          MAP_LL_PRIV_SetupPrivacy( GET_AL_TABLE_POINTER(extScanParam.filterList) );
        }
        else if ( extScanInfo->pEnable->dupFiltering != LL_FILTER_REPORTS_DISABLE )
        {
          // Clear alternate accept list used for duplicate filtering
          MAP_AL_Scan_Init( alTableScan );
        }
        else // Duplicate filtering disabled
        {
          /* This else clause is required, even if the
             programmer expects this will never be reached
             Fix Misra-C Required: MISRA.IF.NO_ELSE */
        }
      }
    }

    // Check if no other tasks are currently active
    if ( llState == LL_STATE_IDLE )
    {
#ifdef USE_AE //Scanner
      // Reset scanner report state machine
      llManageExtScanStateList(EXT_SCAN_STATE_LIST_CLEAR_ALL, 0, 0, 0);
      scanState = WAIT_FOR_ADV_EXT_IND;
      MAP_llSetSIDFilterScanRsp();
#endif
      // Set LL state
      llState = LL_STATE_SCAN;

      // Schedule this task
      MAP_llScheduler();
    }
    else
    {
      /*
       * Disable Task preemption for LOKI
       * Preemption allows a task to preempt scheduled command in case there is
       * enough time for a new command to be executed. Then, previously de-scheduled
       * command is returned to the queue and executed on it's time.
       * This feature is not supported in LOKI, so currently is disabled
       */
      uint32 endTime = extScanInfo->scanStartTime +
                      (extScanInfo->pScanParam->extScanParam[extScanIndex].scanWindow * RAT_TICKS_IN_625US) +
                       EXT_SCAN_MARGIN_TIME_RAT_TICKS;

      // Check RF command preemption
      MAP_llCheckRfCmdPreemption(endTime,extScanInfo->priority);
    }
  }
  else // disable
  {
    halIntState_t cs;

    // ignore command if already scanning
    if ( extScanInfo->scanMode == LL_SCAN_STOP )
    {
      if ( extScanInfo->timingFlag == AE_SCAN_PERIOD_TIMEOUT )
      {
        // stop the timer
        (void)MAP_osal_stop_timerEx( LL_TaskID, LL_EVT_EXT_SCAN_TIMEOUT );

        // invoke callback, if there is one
        MAP_llExtAdvCBack( LL_CBACK_EXT_SCAN_END, NULL );
      }

      return( LL_STATUS_SUCCESS );
    }

    HAL_ENTER_CRITICAL_SECTION(cs);

    // stop the timer
    (void)MAP_osal_stop_timerEx( LL_TaskID, LL_EVT_EXT_SCAN_TIMEOUT );

    // indicate we are no longer actively scanning
    extScanInfo->scanMode = LL_SCAN_STOP;

    // check if this task is to be halted
    // Note: We can halt the radio if this is the only task and if this is
    //       the scan task.
    if (MAP_llGetCurrentTask() == extScanInfo->llTask)
    {
      // stop the command
      status = MAP_llHaltRadio( (uint32)&extScanCmd );

      if ((status == (uint8)RCL_CommandStatus_DescheduledApi) ||
          (status == (uint8)RCL_CommandStatus_Scheduled))
      {
        // force a post-processing event, which will call Scheduler if needed
        // Note: Either the CM0 was halted, and a callback set's up this
        //       post-processing event, or a callback is never generated
        //       (because the CM0 wasn't running at the time), in which
        //       case, we setup the post-processing event here. Either
        //       way, there's no harm setting it here.
        (void)MAP_osal_set_event( LL_TaskID, LL_EVT_SCAN_LAST_CMD_DONE );
      }

      HAL_EXIT_CRITICAL_SECTION(cs);
    }
    else // not the only task AND not the current task
    {
      // free task and teardown privacy if need be
      MAP_llEndExtScanTask();

      HAL_EXIT_CRITICAL_SECTION(cs);

      // invoke callback, if there is one, unless period is enabled
      if (extScanInfo->pEnable->period == 0)
      {
        MAP_llExtAdvCBack( LL_CBACK_EXT_SCAN_END, NULL );

        // The original scan command (pointed by extScanInfo->pEnable) is going to be freed in the callback above,
        // so internally it can't be used anymore
        // Note: all LE_SetExtScanEnable callers should check the pEnable after the call
        extScanInfo->pEnable = NULL;
      }
    }
  }

  return( LL_STATUS_SUCCESS );
}
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * @fn          LE_ExtCreateConn
 *
 * @brief       This API is used to create a connection to a connectable
 *              advertiser.
 *
 *              Note: Currently, only one set of connection parameters are
 *                    supported.
 *
 * input parameters
 *
 * @param       pCmdParams - Pointer to input parameters:
 *                           - initiator filter policy
 *                           - own address type
 *                           - peer address type
 *                           - peer address
 *                           - initiating PHYS
 *                           - scan interval
 *                           - scan window
 *                           - connection interval min
 *                           - connection interval max
 *                           - connection latency
 *                           - supervision timeout
 *                           - minimum CE length
 *                           - maximum CE length
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 *              LL_STATUS_ERROR_OUT_OF_HEAP
 *              LL_STATUS_ERROR_BAD_PARAMETER
 *              LL_STATUS_ERROR_CONNECTION_LIMIT_EXCEEDED
 *              LL_STATUS_ERROR_ILLEGAL_PARAM_COMBINATION
 */
llStatus_t LE_ExtCreateConn( aeCreateConnCmd_t *pCmdParams )
{
  llConnState_t *connPtr;
#ifdef QUAL_TEST
  uint8 *localIRK;
#endif

  // Check if we already have extInitInfo
  // Note: This structure is pre-allocated upon startup, and should always be
  //       valid by the time this function is called.
  if ( !extInitInfo )
  {
    return( LL_STATUS_ERROR_OUT_OF_HEAP );
  }

  // first check if a create connection is already in progress
  if ( (extInitInfo->scanMode == LL_SCAN_START)            ||
       (MAP_llGetTaskState(LL_TASK_ID_INITIATOR) == LL_TASK_STATE_ACTIVE) )
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }

  // Check if a direct test mode or modem test is in progress
  if ( (llState == LL_STATE_DIRECT_TEST_MODE_TX)           ||
       (llState == LL_STATE_DIRECT_TEST_MODE_RX)           ||
       (llState == LL_STATE_MODEM_TEST_TX)                 ||
       (llState == LL_STATE_MODEM_TEST_RX)                 ||
       (llState == LL_STATE_MODEM_TEST_TX_FREQ_HOPPING) )
  {
    return( LL_STATUS_ERROR_UNEXPECTED_STATE_ROLE );
  }

  // Check if scan parameters are valid
  if ( (!pCmdParams)                                                            ||
       ( (pCmdParams->initFilterPolicy != LL_INIT_AL_POLICY_USE_PEER_ADDR)  &&
         (pCmdParams->initFilterPolicy != LL_INIT_AL_POLICY_USE_ACCEPT_LIST) )   ||
       ( (pCmdParams->initFilterPolicy == LL_INIT_AL_POLICY_USE_PEER_ADDR)  &&
         (MAP_osal_isbufset(pCmdParams->peerAddr, 0x00, B_ADDR_LEN) ) )         ||
       ( (pCmdParams->peerAddrType != LL_DEV_ADDR_TYPE_PUBLIC) &&
         (pCmdParams->peerAddrType != LL_DEV_ADDR_TYPE_RANDOM) &&
         (pCmdParams->peerAddrType != LL_DEV_ADDR_TYPE_PUBLIC_ID)  &&
         (pCmdParams->peerAddrType != LL_DEV_ADDR_TYPE_RANDOM_ID) )             ||
       ( (pCmdParams->ownAddrType  != LL_DEV_ADDR_TYPE_PUBLIC) &&
         (pCmdParams->ownAddrType  != LL_DEV_ADDR_TYPE_RANDOM) &&
         (pCmdParams->ownAddrType  != LL_DEV_ADDR_TYPE_PUBLIC_ID)  &&
         (pCmdParams->ownAddrType  != LL_DEV_ADDR_TYPE_RANDOM_ID) )             ||
       // Only 1M and Code PHY allowed
       (!(pCmdParams->initPhys & LL_PHY_1_MBPS) &&
        !(pCmdParams->initPhys & LL_PHY_CODED)) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // Check parameters associated with PHY bits
  for ( uint8_t i=0,j=0; i<LL_PHY_NUMBER_OF_PHYS; i++ )
  {
    if ( pCmdParams->initPhys & BV(i) )
    {
      // Check parameter ranges
      if ( (pCmdParams->extInitParam[j].scanInterval < LL_SCAN_INTERVAL_MIN)                     ||
           (pCmdParams->extInitParam[j].scanInterval > LL_SCAN_INTERVAL_MAX)                     ||
           (pCmdParams->extInitParam[j].scanWindow   < LL_SCAN_INTERVAL_MIN)                     ||
           (pCmdParams->extInitParam[j].scanWindow   > LL_SCAN_INTERVAL_MAX)                     ||
           (pCmdParams->extInitParam[j].scanWindow   > pCmdParams->extInitParam[j].scanInterval) ||
           (pCmdParams->extInitParam[j].maxLength    < pCmdParams->extInitParam[j].minLength) )
      {
        return( LL_STATUS_ERROR_BAD_PARAMETER );
      }

      // Sanity checks again to be sure we don't start with bad parameters
      if ( LL_INVALID_CONN_TIME_PARAM( pCmdParams->extInitParam[j].connIntMin,
                                       pCmdParams->extInitParam[j].connIntMax,
                                       pCmdParams->extInitParam[j].connLatency,
                                       pCmdParams->extInitParam[j].connTimeout ) )
      {
        return( LL_STATUS_ERROR_BAD_PARAMETER );
      }

      // Check if CI/SL/LSTO is valid (i.e. meets the requirements)
      // Note: LSTO > (1 + Peripheral Latency) * (Connection Interval * 2)
      // Note: The CI * 2 requirement based on ESR05 V1.0, Erratum 3904.
      // Note: LSTO time is normalized to units of 1.25ms (i.e. 10ms = 8 * 1.25ms).
      if ( LL_INVALID_CONN_TIME_PARAM_COMBO( pCmdParams->extInitParam[j].connIntMin,
                                             pCmdParams->extInitParam[j].connLatency,
                                             pCmdParams->extInitParam[j].connTimeout) )
      {
        return( LL_STATUS_ERROR_ILLEGAL_PARAM_COMBINATION );
      }

      // Increment array index
      j++;
    }
  }

  // Save the pointer to advertising data parameters
  extInitInfo->pCreateConn = pCmdParams;

  // Always set scan index to zero, except when 2M+Code is specified
  // Note: If both PHYs are present, we'll start with 1M.
  extInitIndex = (pCmdParams->initPhys == (LL_PHY_2_MBPS+LL_PHY_CODED)) ? 1 : 0;

  // Allocate a connection and assure it is valid
  if ( (connPtr = MAP_llAllocConnId()) == NULL )
  {
    // Exceeded the number of available connection structures
    return( LL_STATUS_ERROR_CONNECTION_LIMIT_EXCEEDED );
  }

  // Save the connection ID with Init
  extInitInfo->connId = connPtr->connId;

  // Check if we're already in a connection with this device as a Peripheral
#ifdef LL_TEST_MODE
  if (llTestMode.testCase != LL_TEST_MODE_TP_CON_ADV_BI_02)
#endif
  { // DO NOT REMOVE!!!! - opened a block section to enable the test mode combination
    if ( MAP_llConnExists( pCmdParams->peerAddr,
                           pCmdParams->peerAddrType ) )
    {
      // Connection already exists, release the connection id
      MAP_llReleaseConnId( connPtr );

      return( HCI_ERROR_CODE_ACL_CONN_ALREADY_EXISTS );
    }
  }

  // Process connection channel map into the data channel table
  MAP_llProcessChanMap( connPtr, connPtr->curChanMap.chanMap );

  // Core spec V4.0 requires that a minimum of two data channels be used
  if ( connPtr->numUsedChans < 2 )
  {
    // It isn't, so release the resource
    MAP_llReleaseConnId( connPtr );

    return( LL_STATUS_ERROR_ILLEGAL_PARAM_COMBINATION );
  }

  // Set our address type
  // Note: In the case where the type is LL_DEV_ADDR_TYPE_PUBLIC_ID and the
  //       Local IRK is valid, the PHY will be set to the INCORRECT address
  //       since the address will be an RPA, and the type should be Random.
  //       This will be corrected after calling llSetupInit below.
  // Note: When LL_DEV_ADDR_TYPE_PUBLIC_ID or LL_DEV_ADDR_TYPE_RANDOM_ID is
  //       used to set the PHY, then only Public or Random will be sent OTA
  //       as only one bit is used.
  extInitInfo->ownAddrType = pCmdParams->ownAddrType;

  // Set the advertiser address based on the HCI's address type preference
  if ( LL_IS_ADDR_IDENTITY_TYPE(pCmdParams->ownAddrType) )
  {
    // Set our address public or random
    MAP_osal_memcpy( extInitInfo->ownAddr,
                     ADDRTYPE_TO_OWNADDR(pCmdParams->ownAddrType),
                     B_ADDR_LEN );
  }
  else // LL_DEV_ADDR_TYPE_PUBLIC_ID | LL_DEV_ADDR_TYPE_RANDOM_ID
  {
#ifdef QUAL_TEST
    if ( MAP_LL_PRIV_IsNRPA( MASK_ID_ADDRTYPE(pCmdParams->ownAddrType), ADDRTYPE_TO_OWNADDR( pCmdParams->ownAddrType )) )
    {
      // Set address type
      extInitInfo->ownAddrType = LL_DEV_ADDR_TYPE_RANDOM;

      // Random Private Non-Resolvable Address, so use our identity address
      MAP_osal_memcpy( extInitInfo->ownAddr,
                       ADDRTYPE_TO_OWNADDR( pCmdParams->ownAddrType ),
                       B_ADDR_LEN );
    }
    else
#endif
    // Check if Local IRK is zero (i.e. there is no Local IRK)
    if (( MAP_LL_PRIV_IsZeroIRK( resolvingList[LOCAL_RL_INDEX].IRK ) ) ||
        (( privInfo.addrResolution == FALSE ) && (pCmdParams->initFilterPolicy == LL_INIT_AL_POLICY_USE_PEER_ADDR)))
    {
      // Ensure own address is an identity address (public or random static)
      if ( !MAP_LL_PRIV_IsIDA( pCmdParams->ownAddrType,
                               ADDRTYPE_TO_OWNADDR( pCmdParams->ownAddrType )) )
      {
        return( LL_STATUS_ERROR_BAD_PARAMETER );
      }

      // No Local RPA, so use our identity address
      MAP_osal_memcpy( extInitInfo->ownAddr,
                       ADDRTYPE_TO_OWNADDR( pCmdParams->ownAddrType ),
                       B_ADDR_LEN );

      // Set our ownAddrType to public
      extInitInfo->ownAddrType = LL_DEV_ADDR_TYPE_PUBLIC;
    }
    else // Valid Local IRK, so use RPA
    {

#ifdef QUAL_TEST
      // Get the proper Local IRK per peer
      localIRK = LL_PRIV_GetLocalIrk(pCmdParams->peerAddr, MASK_ID_ADDRTYPE(pCmdParams->peerAddrType));

      // Update the proper local IRK in case the current IRK is different
      if ((localIRK != NULL) &&
         !( MAP_osal_memcmp( resolvingList[LOCAL_RL_INDEX].IRK, localIRK, KEYLEN ) ))
      {
        MAP_osal_memcpy( resolvingList[LOCAL_RL_INDEX].IRK, localIRK, KEYLEN );
      }

      // This line is removed because generation of new RPA happening in the middle
      // of pairing if we have 2 connections, which are established too fast.
      // When ever we create new connection we won't generate new RPA , only
      // after the RPA timeout happened.
      // so generate Local RPA using Local IRK
      MAP_LL_PRIV_GenerateRPA( resolvingList[LOCAL_RL_INDEX].IRK,
                               resolvingList[LOCAL_RL_INDEX].RPA );
#endif
      // set our RPA address
      MAP_osal_memcpy( extInitInfo->ownAddr,
                       resolvingList[LOCAL_RL_INDEX].RPA,
                       B_ADDR_LEN );
    }

    // Save local identity address and address type in RL
    resolvingList[LOCAL_RL_INDEX].idAddrType = MASK_ID_ADDRTYPE(pCmdParams->ownAddrType);

    MAP_osal_memcpy( resolvingList[LOCAL_RL_INDEX].idAddr,
                     ADDRTYPE_TO_OWNADDR( pCmdParams->ownAddrType ),
                     B_ADDR_LEN );
  }

  // Check if the connection will be based on the peer device address and type
  if ( pCmdParams->initFilterPolicy == LL_INIT_AL_POLICY_USE_PEER_ADDR )
  {
    // Save peer address type as provided
    connPtr->peerInfo.peerAddrType = pCmdParams->peerAddrType;

    // Save peer address as provided
    MAP_osal_memcpy( connPtr->peerInfo.peerAddr,
                     pCmdParams->peerAddr,
                     B_ADDR_LEN );

    // Search the RL for this peer address and address type
    uint8 rlIndex = MAP_LL_PRIV_FindPeerInRL( resolvingList,
                                              MASK_ID_ADDRTYPE(pCmdParams->peerAddrType),
                                              pCmdParams->peerAddr );

    // Check if the Peer's ID address was found in the RL
    if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
    {
      // Check peer ID in RL has a valid IRK and set to Network Privacy Mode
      if ( (resolvingList[rlIndex].privMode == LL_NETWORK_PRIVACY_MODE) &&
           !MAP_LL_PRIV_IsZeroIRK( resolvingList[rlIndex].IRK ) )
      {
        // The peer address type is an ID (public ID or random ID)
        connPtr->peerInfo.peerAddrType = pCmdParams->peerAddrType | LL_DEV_ADDR_TYPE_ID_MASK;

        // Use the RPA found in the resolving list.
        // note: in case the RPA is not up to date,
        //       it will be updated in rxIgnore event
        MAP_osal_memcpy(connPtr->peerInfo.peerAddr,
                        resolvingList[rlIndex].RPA,
                        B_ADDR_LEN);
      }
    }
  }

  // Ramdomly generate a valid 24 bit CRC value
  connPtr->crcInit = MAP_llGenerateCRC();

  // Randomly generate a valid, previously unused, 32-bit access address
  connPtr->accessAddr = MAP_llGenerateValidAccessAddr();

  // Determine the connection interval based on min and max values
  // Note: Range not used, so assume max value.
  // Note: minLength and maxLength are informational.
  connPtr->curParam.connInterval = pCmdParams->extInitParam[extInitIndex].connIntMax;

  // Set the connection timeout
  // Note: The spec says this begins at the end of the CONNECT_IND, but the
  //       LSTO will be converted into events.
  connPtr->curParam.connTimeout = pCmdParams->extInitParam[extInitIndex].connTimeout;

  // Set the peripheral latency
  connPtr->curParam.peripheralLatency = pCmdParams->extInitParam[extInitIndex].connLatency;

  // Set the central's SCA
  connPtr->sleepClkAccuracy = extInitInfo->scaValue;

  // Set the window size (units of 1.25ms)
  // Note: Must be the lesser of 10ms and the connection interval - 1.25ms.
  connPtr->curParam.winSize = LL_WINDOW_SIZE;

  // The minimum connection interval is needed due to the fact that
  // the Stack doesn't know which connection interval will be choosed.
  // To avoid using dynamic window offset that is bigger than the correct ConnInterval,
  // choose the minimum one.
  uint16 minConnInterval = llReturnMinConnInterval(pCmdParams);

  // Set the window offset (units of 1.25ms), will be used only if dynamicWinOffset is set to 0.
  connPtr->curParam.winOffset = (MAP_LL_ENC_GeneratePseudoRandNum() % minConnInterval);
  // Set the channel map hop length (5..16)
  // Note: 0..255 % 12 = 0..11 + 5 = 5..16.
  connPtr->hopLength = (uint8)( (MAP_LL_ENC_GeneratePseudoRandNum() % 12) + 5);

  // TODO: REDO HOW llTask WORKS.
  // Get a task block for this BLE state/role
  // Note: There will always be a valid pointer, so no NULL check required.
  extInitInfo->llTask = MAP_llAllocTask( LL_TASK_ID_INITIATOR );
  if (extInitInfo->llTask == NULL)
  {
    return (LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED);
  }
  // Pointer to first radio operation command
  extInitInfo->llTask->command = (uint32)&extInitCmd;

  // Callback function for scheduler
  extInitInfo->llTask->setup = MAP_llExtInitSchedSetup;

  // Update health check
  MAP_llHealthUpdate(LL_STATE_INIT);

  // Enable Init scan
  extInitInfo->scanMode = LL_SCAN_START;

  // Set the default extended initiator priority.
  extInitInfo->priority = qosDefaultPriorityInitParameter;

  // Setup Init command, parameters and outptut registers
  MAP_llSetupExtInit( connPtr->connId );

  // Check if this is the only task
  // Note: If there are one or more central connections already running, then
  //       the Init task will be scheduled when the next connection ends.
  if ( llState == LL_STATE_IDLE )
  {
    // Only if address resolution is enabled
    if ( privInfo.addrResolution )
    {
      // Enables and clears the extended accept list
      MAP_LL_PRIV_SetupPrivacy( alTable );
    }

    // Determine the correct connection start time
    MAP_llSetupConn( extInitInfo->connId );

    // Set LL state
    llState = LL_STATE_INIT;

    // Schedule this task
    MAP_llScheduler();
  }
  else
  {
    /*
     * Disable Task preemption for LOKI
     * Preemption allows a task to preempt scheduled command in case there is
     * enough time for a new command to be executed. Then, previously de-scheduled
     * command is returned to the queue and executed on it's time.
     * This feature is not supported in LOKI, so currently is disabled
     */
    uint32 endTime = extInitInfo->initStartTime +
                    (extInitInfo->pCreateConn->extInitParam[extInitIndex].scanWindow * RAT_TICKS_IN_625US) +
                     EXT_INIT_MARGIN_TIME_RAT_TICKS;

    // check RF command preemption
    MAP_llCheckRfCmdPreemption(endTime,extInitInfo->priority);
  }

  return( LL_STATUS_SUCCESS );
}
#endif // INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * This API is called by the HCI to cancel a previously given LL connection
 * creation command that is still pending. This command should only be used
 * after the LL_CreateConn command as been issued, but before the
 * LL_ConnComplete callback.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_CreateConnCancel( void )
{
  halIntState_t cs;

  // ensure Init is active
  if ( (extInitInfo->scanMode == LL_SCAN_STOP) ||
       (!MAP_llActiveTask(LL_TASK_ID_INITIATOR)) )
  {
    // no create connection in progress
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }

  HAL_ENTER_CRITICAL_SECTION(cs);

  // indicate we are no longer actively scanning
  extInitInfo->scanMode = LL_SCAN_STOP;

  // check if this task is to be halted
  // Note: We can halt the radio if this is the only task, or if this is
  //       the current task.
  if ( (MAP_llGetNumTasks() == 1) ||
       (MAP_llGetCurrentTask() == extInitInfo->llTask) )
  {
    // stop the command
    uint8 status = MAP_llHaltRadio( (uint32)&extInitCmd );

    if ((status == (uint8)RCL_CommandStatus_DescheduledApi) ||
        (status == (uint8)RCL_CommandStatus_Scheduled))
    {
      taskEndAction = MAP_llExtInit_PostProcess;

      // force a post-processing event, which will call Scheduler if needed
      // Note: Either the CM0 was halted, and a callback set's up this
      //       post-processing event, or a callback is never generated
      //       (because the CM0 wasn't running at the time), in which
      //       case, we setup the post-processing event here. Either
      //       way, there's no harm setting it here.
      (void)MAP_osal_set_event( LL_TaskID, LL_EVT_POST_PROCESS_RF );
    }
  }
  else // not the only task AND not the current task
  {
    // free task and teardown privacy if need be
    MAP_llEndExtInitTask();
  }

  //  release the associated allocated connection
  MAP_llReleaseConnId( MAP_llDataGetConnPtr( extInitInfo->connId ) );

  // follow the command complete by a connection complete event, per the spec
  (void)MAP_osal_set_event( LL_TaskID, LL_EVT_CENTRAL_CONN_CANCELLED );

  HAL_EXIT_CRITICAL_SECTION(cs);

  return( LL_STATUS_SUCCESS );

}
#endif // INIT_CFG

#ifdef USE_PERIODIC_ADV
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*********************************************************************
 * @fn      LE_SetPeriodicAdvParams
 *
 * @brief   Used by the Host to set the advertiser parameters for periodic advertising
 *
 * @Design: BLE_LOKI-1795
 * @Design: BLE_LOKI-2034
 * @Design  /ref did_286039104
 *
 * input parameters
 *
 * @param   advHandle               Used to identify a periodic advertising train
 *                                   Created by LE Set Extended Advertising Parameters command
 * @param   periodicAdvIntervalMin  Minimum advertising interval for periodic advertising
 *                                   Range: 0x0006 to 0xFFFF Time = N * 1.25 ms Time Range: 7.5ms to 81.91875 s
 * @param   periodicAdvIntervalMax  Maximum advertising interval for periodic advertising
 *                                   Range: 0x0006 to 0xFFFF Time = N * 1.25 ms Time Range: 7.5ms to 81.91875 s
 * @param   periodicAdvProp         Periodic advertising properties - set bit 6 for include TxPower in the advertising PDU
 *
 * output parameters
 *
 * @param       None.
 *
 * @return  llStatus_t
 */
llStatus_t LE_SetPeriodicAdvParams( uint8 advHandle,
                                    uint16 periodicAdvIntervalMin,
                                    uint16 periodicAdvIntervalMax,
                                    uint16 periodicAdvProp )
{
  llPeriodicAdvSet_t *pPeriodicAdv;
  advSet_t *pAdvSet;

  // Get the Adv Set, if there is one
  pAdvSet = MAP_LL_SearchAdvSet( advHandle );

  // Check if we have an Adv Set
  if ( pAdvSet == NULL )
  {
    return (LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER);
  }
  // Check if the adv set was configure to anonymous advertising
  // or connectable or scannable or legacy
  else if ((TST_AE_PROPS_OMIT_ADVA(pAdvSet->pAdvParam->eventProps)) ||
           (TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps))      ||
           (TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps))      ||
           (TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps)))
  {
    return (LL_STATUS_ERROR_BAD_PARAMETER);
  }

  // check the interval parameters
  if ((periodicAdvIntervalMin > periodicAdvIntervalMax) ||
      (periodicAdvIntervalMin < LL_CONN_INTERVAL_MIN))
  {
    return (LL_STATUS_ERROR_BAD_PARAMETER);
  }

  // Get the periodic adv set
  pPeriodicAdv = llGetPeriodicAdv(advHandle);

  // Check that the periodic adv was already enabled
  if ((pPeriodicAdv != NULL) &&
      (pPeriodicAdv->state >= PERIODIC_ADV_STATE_PENDING_ENABLE))
  {
    return (LL_STATUS_ERROR_COMMAND_DISALLOWED);
  }

  // If periodic adv data exists for the set and the length of data
  // is greater than the maximum that the Controller can transmit within
  // a periodic advertising interval of periodicAdvIntervalMax, return error
  if ((pPeriodicAdv != NULL) && (pPeriodicAdv->pData != NULL))
  {
    uint32 otaTime = llEstimatePeriodicAdvOtaTime(pPeriodicAdv->dataLen,
                                                  pPeriodicAdv->maxAvailData,
                                                  pPeriodicAdv->phy, 0, 0);
    if((otaTime + RAT_TICKS_TO_US(PERIODIC_ADV_MARGIN_TIME_RAT_TICKS)) >
       (periodicAdvIntervalMax * PERIODIC_ADV_INTERVAL_UNIT_IN_US))
    {
      return (LL_STATUS_ERROR_PACKET_TOO_LONG);
    }
  }

  // Add current set to the periodic list
  if (pPeriodicAdv == NULL)
  {
    pPeriodicAdv = MAP_osal_mem_alloc(sizeof(llPeriodicAdvSet_t));
    if (pPeriodicAdv == NULL)
    {
      return (LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED);
    }
    MAP_osal_memset( pPeriodicAdv, 0, sizeof(llPeriodicAdvSet_t) );
    // Case the list is empty
    if (llPeriodicAdv.advList == NULL)
    {
      // Update the channel map (common to all extended advertisers)
      llSetPeriodicChanMap(&llPeriodicAdv.chanMap.current, secondaryAdvChannelMap);
      // Set the list head
      llPeriodicAdv.advList = pPeriodicAdv;
    }
    else
    {
      llPeriodicAdvSet_t *pTmp = llPeriodicAdv.advList;

      while (pTmp->next != NULL) pTmp = pTmp->next;
      pTmp->next = pPeriodicAdv;
    }
    // Init the set
    pPeriodicAdv->handle = advHandle;
    pPeriodicAdv->next = NULL;
    pPeriodicAdv->state = PERIODIC_ADV_STATE_DISABLE;
    pPeriodicAdv->dataCmd.pData = NULL;
    pPeriodicAdv->dataCmd.operation = AE_DATA_OP_NO_DATA;
    // Determine the amount of space available in packet for data
    pPeriodicAdv->maxAvailData = (AE_MAX_ADV_PAYLOAD_LEN - (EXTHDR_FLAG_CTEINFO_SIZE + EXTHDR_FLAG_AUXPTR_SIZE +
                                  EXTHDR_FLAG_TXPWR_SIZE + EXTHDR_INFO_SIZE + EXTHDR_FLAGS_SIZE + 1));
  }

  // Set the parameters
  pPeriodicAdv->paramsCmd.intervalMin = periodicAdvIntervalMin;
  pPeriodicAdv->paramsCmd.intervalMax = periodicAdvIntervalMax;
  pPeriodicAdv->paramsCmd.props[0] = LO_UINT16(periodicAdvProp);
  pPeriodicAdv->paramsCmd.props[1] = HI_UINT16(periodicAdvProp);
  pPeriodicAdv->phy = pAdvSet->pAdvParam->secPhy;

  return( LL_STATUS_SUCCESS );
}

/*********************************************************************
 * @fn      LE_SetPeriodicAdvData
 *
 * @brief   Used to set the advertiser data used in periodic advertising PDUs.
 *          This command may be issued at any time after the advertising set identified by
 *          the Advertising_Handle parameter has been configured for periodic advertising
 *          using the HCI_LE_Set_Periodic_Advertising_Parameters command
 *
 * @Design: BLE_LOKI-1795
 * @Design: BLE_LOKI-2034
 * @Design  /ref did_286039104
 *
 * input parameters
 *
 * @param   advHandle   Used to identify a periodic advertising train
 * @param   operation   0x00 - Intermediate fragment of fragmented periodic advertising data
 *                       0x01 - First fragment of fragmented periodic advertising data
 *                       0x02 - Last fragment of fragmented periodic advertising data
 *                       0x03 - Complete periodic advertising data
 * @param   dataLength  The number of bytes in the Advertising Data parameter
 * @param   data        Periodic advertising data
 *
 * output parameters
 *
 * @param       None.
 *
 * @return  llStatus_t
 */
llStatus_t LE_SetPeriodicAdvData( uint8 advHandle,
                                  uint8 operation,
                                  uint8 dataLength,
                                  uint8 *data )
{
  llPeriodicAdvSet_t *pPeriodicAdv = llGetPeriodicAdv(advHandle);
  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( advHandle );

  // Check if we have an Adv Set
  if ( pAdvSet == NULL )
  {
    return (LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER);
  }

  // Check that the periodic was already configured
  if (pPeriodicAdv == NULL)
  {
    return (LL_STATUS_ERROR_COMMAND_DISALLOWED);
  }

  // In case periodic is enable and operation is not 0x03
  if ((pPeriodicAdv->state >= PERIODIC_ADV_STATE_PENDING_ENABLE) && (operation != AE_DATA_OP_COMPLETE))
  {
    return (LL_STATUS_ERROR_COMMAND_DISALLOWED);
  }

  // Check the data length
  if (dataLength > PERIODIC_ADV_DATA_CMD_MAX_PAYLOAD)
  {
    return (LL_STATUS_ERROR_BAD_PARAMETER);
  }

  pPeriodicAdv->dataUpdated = FALSE;

  if ((operation == AE_DATA_OP_COMPLETE) || (operation == AE_DATA_OP_FIRST_FRAG))
  {
    // When performing the complete operation to clear the data
    // free the pData that is used for advertising only if the
    // set is currently disabled
    if((operation == AE_DATA_OP_COMPLETE) &&
       (pPeriodicAdv->state == PERIODIC_ADV_STATE_DISABLE) &&
       (pPeriodicAdv->pData != NULL))
    {
        MAP_osal_mem_free(pPeriodicAdv->pData);
        pPeriodicAdv->pData = NULL;
        pPeriodicAdv->dataLen = 0;
    }
    if (pPeriodicAdv->dataCmd.pData != NULL)
    {
      MAP_osal_mem_free(pPeriodicAdv->dataCmd.pData);
      pPeriodicAdv->dataCmd.pData = NULL;
      pPeriodicAdv->dataCmd.dataLen = 0;
      pPeriodicAdv->dataCmd.operation = AE_DATA_OP_NO_DATA;
    }
    if (dataLength > 0)
    {
      pPeriodicAdv->dataCmd.pData = MAP_osal_mem_alloc(dataLength);
      if (pPeriodicAdv->dataCmd.pData == NULL)
      {
        return (LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED);
      }
      MAP_osal_memcpy(pPeriodicAdv->dataCmd.pData, data, dataLength);
    }
    pPeriodicAdv->dataCmd.dataLen = dataLength;
    pPeriodicAdv->dataCmd.operation = operation;
    if (operation == AE_DATA_OP_COMPLETE)
    {
      pPeriodicAdv->dataUpdated = TRUE;
    }
  }
  else if (((operation == AE_DATA_OP_NEXT_FRAG) || (operation == AE_DATA_OP_LAST_FRAG)) &&
          ((pPeriodicAdv->dataCmd.operation == AE_DATA_OP_FIRST_FRAG) ||
           (pPeriodicAdv->dataCmd.operation == AE_DATA_OP_NEXT_FRAG)))
  {
    uint8 *pTotalData;
    uint16 totalDataLen = pPeriodicAdv->dataCmd.dataLen + dataLength;
    uint32 otaTime = llEstimatePeriodicAdvOtaTime(totalDataLen, pPeriodicAdv->maxAvailData,
                                                  pPeriodicAdv->phy, 0, 0);

    if (dataLength > 0)
    {
      // Check max data allowed
      if ((totalDataLen > maxExtAdvDataLen) ||
         ((otaTime + RAT_TICKS_TO_US(PERIODIC_ADV_MARGIN_TIME_RAT_TICKS)) >
          (pPeriodicAdv->paramsCmd.intervalMax * PERIODIC_ADV_INTERVAL_UNIT_IN_US)))
      {
        MAP_osal_mem_free(pPeriodicAdv->dataCmd.pData);
        pPeriodicAdv->dataCmd.pData = NULL;
        pPeriodicAdv->dataCmd.operation = AE_DATA_OP_NO_DATA;
        pPeriodicAdv->dataCmd.dataLen = 0;
        if (totalDataLen > maxExtAdvDataLen)
        {
          return (LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED);
        }
        else
        {
          return (LL_STATUS_ERROR_PACKET_TOO_LONG);
        }
      }

      // Allocate complete data
      pTotalData = MAP_osal_mem_alloc(totalDataLen);
      if (pTotalData == NULL)
      {
        return (LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED);
      }
      // Update the data
      if (pPeriodicAdv->dataCmd.pData)
      {
        MAP_osal_memcpy(pTotalData, pPeriodicAdv->dataCmd.pData, pPeriodicAdv->dataCmd.dataLen);
        MAP_osal_mem_free(pPeriodicAdv->dataCmd.pData);
      }
      MAP_osal_memcpy(pTotalData + pPeriodicAdv->dataCmd.dataLen, data, dataLength);
      pPeriodicAdv->dataCmd.pData = pTotalData;
    }
    pPeriodicAdv->dataCmd.dataLen = totalDataLen;
    pPeriodicAdv->dataCmd.operation = operation;

    if (operation == AE_DATA_OP_LAST_FRAG)
    {
      pPeriodicAdv->dataUpdated = TRUE;
    }
  }
  else
  {
    return (LL_STATUS_ERROR_BAD_PARAMETER);
  }

  return( LL_STATUS_SUCCESS );
}

/*********************************************************************
 * @fn      LE_SetPeriodicAdvEnable
 *
 * @brief   Used to request the advertiser to enable or disable
 *          the periodic advertising for the advertising set
 *
 * @Design:  BLE_LOKI-1795
 * @Design:  BLE_LOKI-2034
 * @Design   /ref did_286039104
 *
 * input parameters
 *
 * @param   enable     0x00 - Periodic advertising is disabled (default)
 *                      0x01 - Periodic advertising is enabled
 * @param   advHandle  Used to identify a periodic advertising train
 *
 * output parameters
 *
 * @param       None.
 *
 * @return  llStatus_t
 */
llStatus_t LE_SetPeriodicAdvEnable( uint8 enable,
                                    uint8 advHandle )
{
  llPeriodicAdvSet_t *pPeriodicAdv = llGetPeriodicAdv(advHandle);
  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( advHandle );

  // Check if we have an Adv Set
  if ( pAdvSet == NULL )
  {
    return (LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER);
  }

  // Check that the periodic was already configured
  if (pPeriodicAdv == NULL)
  {
    return (LL_STATUS_ERROR_COMMAND_DISALLOWED);
  }

  // Check that the periodic data was complete and the eventProps are valid
  if ((enable == 1) &&
     ((pPeriodicAdv->dataCmd.operation == AE_DATA_OP_FIRST_FRAG) ||
      (pPeriodicAdv->dataCmd.operation == AE_DATA_OP_NEXT_FRAG) ||
      TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps) ||
      TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps) ||
      TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) ||
      TST_AE_PROPS_OMIT_ADVA(pAdvSet->pAdvParam->eventProps)))
  {
    return (LL_STATUS_ERROR_COMMAND_DISALLOWED);
  }
  if (((pPeriodicAdv->state >= PERIODIC_ADV_STATE_PENDING_ENABLE) && (enable == 1)) ||
      (((pPeriodicAdv->state == PERIODIC_ADV_STATE_DISABLE) || (pPeriodicAdv->pendingDisable)) && (enable == 0)))
  {
    return( LL_STATUS_SUCCESS );
  }
  if (enable == 1)
  {
    // Check if we don't yet have an RF command structure, and if not, malloc one
    if (pPeriodicAdv->pRfCmds == NULL)
    {
      // We don't have this memory yet, so try to allocate
      pPeriodicAdv->pRfCmds = MAP_osal_mem_allocLimited(sizeof(periodicRf_t));
      if (pPeriodicAdv->pRfCmds == NULL)
      {
          return( LL_STATUS_ERROR_OUT_OF_HEAP );
      }
      // Clear entire structure before initializing
      MAP_osal_memset( pPeriodicAdv->pRfCmds, 0, sizeof(periodicRf_t) );
    }

    pPeriodicAdv->eventCounter = 0;
    pPeriodicAdv->syncInfo.accessAddr = MAP_llGenerateValidAccessAddr();
    for (uint8 i = 0; i < LL_PKT_CRC_LEN; i++)
    {
      pPeriodicAdv->syncInfo.crcInit[i] = MAP_LL_ENC_GeneratePseudoRandNum();
    }
    pPeriodicAdv->syncInfo.eventCounter = pPeriodicAdv->eventCounter;
    pPeriodicAdv->syncInfo.sca = LL_SCA_CENTRAL_DEFAULT;
    // Set the periodic data
    llSetPeriodicAdvData(pPeriodicAdv);

    // Determine the interval
    pPeriodicAdv->interval = pPeriodicAdv->paramsCmd.intervalMax;
    // Use the current TX power value
    pPeriodicAdv->txPowerIndex = curTxPowerVal;

    // Initialize the channel
    pPeriodicAdv->pChanMap = &llPeriodicAdv.chanMap.current;
    pPeriodicAdv->currentChan = llSetNextPeriodicAdvChan( pPeriodicAdv->pChanMap, pPeriodicAdv->syncInfo.accessAddr,pPeriodicAdv->eventCounter);

    pPeriodicAdv->state = PERIODIC_ADV_STATE_PENDING_ENABLE;
  }
  else if (enable == 0)
  {
    halIntState_t cs;

    if (pPeriodicAdv->state == PERIODIC_ADV_STATE_PENDING_ENABLE)
    {
      pPeriodicAdv->state = PERIODIC_ADV_STATE_DISABLE;
    }
    else
    {
      HAL_ENTER_CRITICAL_SECTION(cs);
      pPeriodicAdv->pendingDisable = TRUE;

      // Check if this task is to be halted
      // Note: We can halt the radio if this is the only task and if this is
      //       the current task.
      if ( (MAP_llGetCurrentTask() == llPeriodicAdv.llTask) &&
           (llPeriodicAdv.currentAdv == pPeriodicAdv))
      {
        // Halt the radio
        MAP_llHaltRadio( llPeriodicAdv.llTask->command );
      }
      else // Not the only task AND not the current task
      {
        // Free task
        MAP_llEndPeriodicAdvTask( pPeriodicAdv );
      }
      HAL_EXIT_CRITICAL_SECTION(cs);
    }
  }
  else
  {
    return (LL_STATUS_ERROR_BAD_PARAMETER);
  }

  return( LL_STATUS_SUCCESS );
}

#endif // ADV_NCONN_CFG | ADV_CONN_CFG
#endif // USE_PERIODIC_ADV

#ifdef USE_PERIODIC_SCAN
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*********************************************************************
 * @fn      LE_PeriodicAdvCreateSync
 *
 * @brief   Used a scanner to synchronize with a periodic advertising train from
 *          an advertiser and begin receiving periodic advertising packets.
 *
 * @design  /ref did_286039104
 *
 * @param   options      Clear Bit 0 - Use the advSID, advAddrType, and advAddress
 *                                      parameters to determine which advertiser to listen to.
 *                        Set Bit 0   - Use the Periodic Advertiser List to determine which
 *                                      advertiser to listen to.
 *                        Clear Bit 1 - Reporting initially enabled.
 *                        Set Bit 1   - Reporting initially disabled.
 * @param   advSID       Advertising SID subfield in the ADI field used to identify
 *                        the Periodic Advertising (Range: 0x00 to 0x0F)
 * @param   advAddrType  Advertiser address type - 0x00 - public ; 0x01 - random
 * @param   advAddress   Advertiser address
 * @param   skip         The maximum number of periodic advertising events that can be
 *                        skipped after a successful receive (Range: 0x0000 to 0x01F3)
 * @param   syncTimeout  Synchronization timeout for the periodic advertising train
 *                           Range: 0x000A to 0x4000 Time = N*10 ms Time Range: 100 ms to 163.84 s
 * @param   syncCteType  Set Bit 0 - Do not sync to packets with an AoA CTE
 *                        Set Bit 1 - Do not sync to packets with an AoD CTE with 1 us slots
 *                        Set Bit 2 - Do not sync to packets with an AoD CTE with 2 us slots
 *                        Set Bit 4 - Do not sync to packets without a CTE
 *
 * @return  HCI_Success
 */
llStatus_t LE_PeriodicAdvCreateSync( uint8  options,
                                     uint8  advSID,
                                     uint8  advAddrType,
                                     uint8  *advAddress,
                                     uint16 skip,
                                     uint16 syncTimeout,
                                     uint8  syncCteType )
{
  llPeriodicScanSet_t *pPeriodicScan;

  // check if a direct test mode or modem test is in progress
  if ( (llState == LL_STATE_DIRECT_TEST_MODE_TX)           ||
       (llState == LL_STATE_DIRECT_TEST_MODE_RX)           ||
       (llState == LL_STATE_MODEM_TEST_TX)                 ||
       (llState == LL_STATE_MODEM_TEST_RX)                 ||
       (llState == LL_STATE_MODEM_TEST_TX_FREQ_HOPPING) )
  {
    return( LL_STATUS_ERROR_UNEXPECTED_STATE_ROLE );
  }
  //check that another Create Sync command is pending
  if (llPeriodicScan.createSync != NULL)
  {
    return (LL_STATUS_ERROR_COMMAND_DISALLOWED);
  }
  // validate the parameters
  if ((options     > PERIODIC_SCAN_CREATE_SYNC_OPTIONS_MAX_VAL) ||
      (skip        > PERIODIC_SCAN_CREATE_SYNC_SKIP_MAX_VAL) ||
      (syncTimeout < PERIODIC_SCAN_CREATE_SYNC_TO_MIN_VAL) ||
      (syncTimeout > PERIODIC_SCAN_CREATE_SYNC_TO_MAX_VAL) ||
      (syncCteType > PERIODIC_SCAN_CREATE_SYNC_CTE_MAX_VAL))
  {
    return (LL_STATUS_ERROR_BAD_PARAMETER);
  }
  // validate the sync CTE type
  if ((GET_PERIODIC_CTE_TYPE_SYNC_NO_TYPE_3(syncCteType)) ||
      (syncCteType == PERIODIC_SCAN_CREATE_SYNC_CTE_VALID_BITS_VAL))
  {
    return (LL_STATUS_ERROR_COMMAND_DISALLOWED);
  }
  // in case of not using the advertiser list
  if (!GET_PERIODIC_SCAN_OPTIONS_LIST_USE(options))
  {
    // validate the advertiser params
    if ((advSID > PERIODIC_SCAN_CREATE_SYNC_SID_MAX_VAL) ||
        (advAddrType > LL_DEV_ADDR_TYPE_RANDOM) ||
        (advAddress == NULL))
    {
      return (LL_STATUS_ERROR_BAD_PARAMETER);
    }
    // check that the controller do not already synchronized to the requested advertiser
    pPeriodicScan = llGetPeriodicScanByAdvertiser( advSID, advAddrType, advAddress );
    if (pPeriodicScan != NULL)
    {
      return (LL_STATUS_ERROR_CONNECTION_ALREADY_EXISTS);
    }
  }

  //allocate the new periodic scanner
  llPeriodicScan.createSync = MAP_osal_mem_alloc( sizeof(llPeriodicScanSet_t));
  if (llPeriodicScan.createSync == NULL)
  {
    return (LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED);
  }
  MAP_osal_memset( llPeriodicScan.createSync, 0, sizeof(llPeriodicScanSet_t) );
  llPeriodicScan.createSync->syncCmd.options = options;
  llPeriodicScan.createSync->syncCmd.skip = skip;
  llPeriodicScan.createSync->syncCmd.timeout = syncTimeout;
  llPeriodicScan.createSync->syncCmd.cteType = syncCteType;
  llPeriodicScan.createSync->reportEnable = !(GET_PERIODIC_SCAN_OPTIONS_REPORT_DISABLE(options));
  // in case of not using the accept list
  if (!GET_PERIODIC_SCAN_OPTIONS_LIST_USE(options))
  {
    llPeriodicScan.createSync->syncCmd.sid = advSID;
    llPeriodicScan.createSync->syncCmd.addrType = advAddrType;
    MAP_osal_memcpy( llPeriodicScan.createSync->syncCmd.addr, advAddress, LL_DEVICE_ADDR_LEN );
  }
  // setup the periodic scan command
  llSetupPeriodicScan(llPeriodicScan.createSync);
  llPeriodicScan.createSync->state = PERIODIC_SCAN_STATE_SYNCING_PENDING;

  return( LL_STATUS_SUCCESS );
}

/*********************************************************************
 * @fn      LE_PeriodicAdvCreateSyncCancel
 *
 * @brief   Used a scanner to cancel the HCI_LE_Periodic_Advertising_Create_Sync
 *          command while it is pending.
 *
 * @Design: BLE_LOKI-2022
 * @Design: BLE_LOKI-2034
 * @Design  /ref did_286039104
 *
 * @param   None
 *
 * @return  llStatus_t
 */
llStatus_t LE_PeriodicAdvCreateSyncCancel( void )
{
  halIntState_t cs;

  // in case periodic scanner is not in create sync process
  if (llPeriodicScan.createSync == NULL)
  {
    return (LL_STATUS_ERROR_COMMAND_DISALLOWED);
  }
  HAL_ENTER_CRITICAL_SECTION(cs);
  // set the terminate flag
  llPeriodicScan.createSync->terminate = LL_STATUS_ERROR_OP_CANCELLED_BY_HOST;
  // check if this task is to be halted
  // Note: We can halt the radio if this is the only task and if this is
  //       the current task.
  if ( (MAP_llGetCurrentTask() == llPeriodicScan.llTask) &&
       (llPeriodicScan.currentScan == llPeriodicScan.createSync))
  {
    // halt the radio
    MAP_llHaltRadio( llPeriodicScan.llTask->command );

    // force a post-processing event, which will call Scheduler if needed
    // Note: Either the CM0 was halted, and a callback set's up this
    //       post-processing event, or a callback is never generated
    //       (because the CM0 wasn't running at the time), in which
    //       case, we setup the post-processing event here. Either
    //       way, there's no harm setting it here.
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_PERIODIC_SCAN_LAST_CMD_DONE );
  }
  else // not the only task AND not the current task
  {
    // set the first index which reserved for the create sync handle
    // the value is not important only to be different from 0xFF
    llPeriodicScan.terminateList[PERIODIC_SCAN_TERMINATE_LIST_CREATE_SYNC_INDEX] = 1;
    // notify the Host
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_PERIODIC_SCAN_CANCELLED );
  }
  HAL_EXIT_CRITICAL_SECTION(cs);

  return( LL_STATUS_SUCCESS );
}

/*********************************************************************
 * @fn      LE_PeriodicAdvTerminateSync
 *
 * @brief   Used a scanner to stop reception of the periodic advertising
 *          train identified by the syncHandle parameter.
 *
 * @Design: BLE_LOKI-2022
 * @Design: BLE_LOKI-2034
 * @Design  /ref did_286039104
 *
 *
 * @param   syncHandle - Handle identifying the periodic advertising train
 *                       (Range: 0x0000 to 0x0EFF)
 *                       The handle was assigned by the Controller while generating
 *                       the LE Periodic Advertising Sync Established event
 *
 * @return  llStatus_t
 */
llStatus_t LE_PeriodicAdvTerminateSync( uint16 syncHandle )
{
  llPeriodicScanSet_t *pPeriodicScan = MAP_llGetPeriodicScan( syncHandle );
  halIntState_t cs;

  if (pPeriodicScan == NULL)
  {
    return (LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER);
  }

  if (pPeriodicScan->state == PERIODIC_SCAN_STATE_SYNCED)
  {
    HAL_ENTER_CRITICAL_SECTION(cs);
    //terminate the periodic scanner
    pPeriodicScan->terminate = LL_STATUS_ERROR_OP_CANCELLED_BY_HOST;

    // check if this task is to be halted
    if ((MAP_llGetCurrentTask() == llPeriodicScan.llTask) &&
        (llPeriodicScan.currentScan == pPeriodicScan))
    {
      // halt the radio
      (void)MAP_llHaltRadio( llPeriodicScan.llTask->command );
    }
    else // not the only task AND not the current task
    {
      // free task
      MAP_llEndPeriodicScanTask(pPeriodicScan);
    }

    HAL_EXIT_CRITICAL_SECTION(cs);
  }

  return( LL_STATUS_SUCCESS );
}

/*********************************************************************
 * @fn      LE_AddDeviceToPeriodicAdvertiserList
 *
 * @brief   Used a scanner to add an entry, consisting of a single device address
 *          and SID, to the Periodic Advertiser list stored in the Controller.
 *
 * @design  /ref did_286039104
 *
 * @param   advAddrType  Advertiser address type - 0x00 - Public or Public Identity Address
 *                                                  0x01 - Random or Random (static) Identity Address
 * @param   advAddress   Advertiser address
 * @param   advSID       Advertising SID subfield in the ADI field used to identify
 *                        the Periodic Advertising (Range: 0x00 to 0x0F)
 *
 * @return  llStatus_t
 */
llStatus_t LE_AddDeviceToPeriodicAdvList( uint8 advAddrType,
                                          uint8 *advAddress,
                                          uint8 advSID )
{
  llPeriodicAcceptListItem_t *pItem = NULL;
  llPeriodicAcceptListItem_t *pPrevItem = NULL;

  // Check the validity of the parameters
  if ((advSID > PERIODIC_SCAN_CREATE_SYNC_SID_MAX_VAL) ||
      (advAddrType > LL_DEV_ADDR_TYPE_RANDOM) ||
      (advAddress == NULL))
  {
    return (LL_STATUS_ERROR_BAD_PARAMETER);
  }
  // check if the list full
  if (llPeriodicScan.acceptList.numItems == PERIODIC_SCAN_ACCEPT_LIST_MAX_ITEMS)
  {
    return (LL_STATUS_ERROR_AL_TABLE_FULL);
  }
  // check that the create sync is pending
  if (llPeriodicScan.createSync != NULL)
  {
    return (LL_STATUS_ERROR_COMMAND_DISALLOWED);
  }

  // check if the entry is already on the list
  pItem = llGetPeriodicAcceptListItem(advSID,advAddrType,advAddress,&pPrevItem);
  if (pItem != NULL)
  {
    // item already exist in the list
    return (LL_STATUS_ERROR_UNEXPECTED_PARAMETER);
  }
  // add the new entry
  pItem = MAP_osal_mem_alloc( sizeof(llPeriodicAcceptListItem_t));
  if (pItem == NULL)
  {
    return (LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED);
  }
  MAP_osal_memcpy( pItem->addr, advAddress, LL_DEVICE_ADDR_LEN );
  pItem->addrType = advAddrType;
  pItem->sid = advSID;
  pItem->next = NULL;

  // check if the list is empty
  if (llPeriodicScan.acceptList.itemList == NULL)
  {
    llPeriodicScan.acceptList.itemList = pItem;
  }
  else
  {
    pPrevItem->next = pItem;
  }

  // increase the num items
  llPeriodicScan.acceptList.numItems++;

  return( LL_STATUS_SUCCESS );
}

/*********************************************************************
 * @fn      LE_RemoveDeviceFromPeriodicAdvList
 *
 * @brief   Used a scanner to remove one entry from the list of Periodic Advertisers
 *          stored in the Controller.
 *
 * @design  /ref did_286039104
 *
 * @param   advAddrType  Advertiser address type -
 *                        0x00 - Public or Public Identity Address
 *                        0x01 - Random or Random (static) Identity Address
 * @param   advAddress   Advertiser address
 * @param   advSID       Advertising SID subfield in the ADI field used to identify
 *                        the Periodic Advertising (Range: 0x00 to 0x0F)
 *
 * @return  llStatus_t
 */
llStatus_t LE_RemoveDeviceFromPeriodicAdvList( uint8 advAddrType,
                                               uint8 *advAddress,
                                               uint8 advSID )
{
  llPeriodicAcceptListItem_t *pItem = NULL;
  llPeriodicAcceptListItem_t *pPrevItem = NULL;

  // Check the validity of the parameters
  if ((advSID > PERIODIC_SCAN_CREATE_SYNC_SID_MAX_VAL) ||
      (advAddrType > LL_DEV_ADDR_TYPE_RANDOM) ||
      (advAddress == NULL))
  {
    return (LL_STATUS_ERROR_BAD_PARAMETER);
  }

  // check that the create sync is pending
  if (llPeriodicScan.createSync != NULL)
  {
    return (LL_STATUS_ERROR_COMMAND_DISALLOWED);
  }

  // check if the entry is already on the list
  pItem = llGetPeriodicAcceptListItem(advSID,advAddrType,advAddress,&pPrevItem);
  if (pItem == NULL)
  {
    // item not exist in the list
    return (LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER);
  }

  // check the head list
  if (llPeriodicScan.acceptList.itemList == pItem)
  {
    llPeriodicScan.acceptList.itemList = pItem->next;
  }
  else
  {
    pPrevItem->next = pItem->next;
  }
  // delete the item
  MAP_osal_mem_free(pItem);
  // decrease the num items
  llPeriodicScan.acceptList.numItems--;

  return( LL_STATUS_SUCCESS );
}

/*********************************************************************
 * @fn      LE_ClearPeriodicAdvList
 *
 * @brief   Used a scanner to remove all entries from the list of Periodic
 *          Advertisers in the Controller.
 *
 * @design  /ref did_286039104
 *
 * @return  llStatus_t
 */
llStatus_t LE_ClearPeriodicAdvList( void )
{
  llPeriodicAcceptListItem_t *pItem = NULL;

  // check that the create sync is pending
  if (llPeriodicScan.createSync != NULL)
  {
    return (LL_STATUS_ERROR_COMMAND_DISALLOWED);
  }

  while( llPeriodicScan.acceptList.itemList != NULL )
  {
    pItem = llPeriodicScan.acceptList.itemList->next;
    // delete the item
    MAP_osal_mem_free(llPeriodicScan.acceptList.itemList);
    // advance to next entry
    llPeriodicScan.acceptList.itemList = pItem;
  }
  llPeriodicScan.acceptList.numItems = 0;
  llPeriodicScan.acceptList.itemList = NULL;
  return( LL_STATUS_SUCCESS );
}

/*********************************************************************
 * @fn      LE_ReadPeriodicAdvListSize
 *
 * @brief   Used a scanner to read the total number of Periodic Advertiser
 *          list entries that can be stored in the Controller.
 *
 * @design  /ref did_286039104
 *
 * @return  llStatus_t
 *          Periodic Advertiser List Size (Range: 0x01 to 0xFF)
 */
llStatus_t LE_ReadPeriodicAdvListSize( uint8 *listSize )
{
  *listSize = PERIODIC_SCAN_ACCEPT_LIST_MAX_ITEMS;
  return( LL_STATUS_SUCCESS );
}

/*********************************************************************
 * @fn      LE_SetPeriodicAdvReceiveEnable
 *
 * @brief   Used a scanner to enable or disable reports for the periodic
 *          advertising train identified by the syncHandle parameter.
 *
 * @design  /ref did_286039104
 *
 * @param   syncHandle - Handle identifying the periodic advertising train
 *                       (Range: 0x0000 to 0x0EFF)
 *                       The handle was assigned by the Controller while generating
 *                       the LE Periodic Advertising Sync Established event
 * @param   enable     - 0x00 - Reporting disable
 *                       0x01 - Reporting enable
 *
 * @return  llStatus_t
 */
llStatus_t LE_SetPeriodicAdvReceiveEnable( uint16 syncHandle,
                                           uint8  enable )
{
  llPeriodicScanSet_t *pPeriodicScan;

  if (enable > 1)
  {
    return (LL_STATUS_ERROR_BAD_PARAMETER);
  }

  pPeriodicScan = MAP_llGetPeriodicScan( syncHandle );
  if (pPeriodicScan == NULL)
  {
    return (LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER);
  }
  pPeriodicScan->reportEnable = enable;

  return( LL_STATUS_SUCCESS );
}

#endif
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This API is called by the LL or HCI to check if a connection given by the
 * connection handle is active.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ConnActive( uint16 connId )
{
  // check if the connection handle is valid
  if ( (connId >= LL_RESERVED_CONNECTION_ID) ||
       (connId >= maxNumConns) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check if the connection is active
  if ( llConns.llConnection[ connId ].activeConn == FALSE )
  {
    return( LL_STATUS_ERROR_INACTIVE_CONNECTION );
  }

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * This API is called by the HCI to update the Host data channels initiating an
 * Update Data Channel control procedure.
 *
 * Note: If connID equals to maxNumConns, the host will update channel
 *       map on all active Central connections.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ChanMapUpdate( uint8 *chanMap , uint16 connID )
{
  uint8 i = 0;
  uint8 j = 0;

  // Parameter check
  if ( chanMap == NULL || connID > maxNumConns )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // Ensure non-data channels 37..39 are not set and that the Core spec V4.0
  // requirement of a minimum of two data channels be used is met
  if ( (chanMap[LL_NUM_BYTES_FOR_CHAN_MAP-1] & ~0x1F) ||
       (MAP_llAtLeastTwoChans( chanMap ) != TRUE) )
  {
    return( LL_STATUS_ERROR_ILLEGAL_PARAM_COMBINATION );
  }

  // If a specific connID
  if( connID != maxNumConns )
  {
    i = connID;
    if ( llConns.llConnection[i].activeConn == FALSE )
    {
      return LL_STATUS_ERROR_BAD_PARAMETER;
    }
  }

  do {
    // Need to issue an update on relevant active connection(s), if any.
    // Note: Can only send if the active connection is Central.
    if ( llConns.llConnection[i].activeConn == TRUE )
    {
      llConnState_t *connPtr = MAP_llDataGetConnPtr( i );

      // Check if an update channel map control procedure is already pending
      if ( ((connPtr->ctrlPktInfo.ctrlPktCount > 0) &&
            (connPtr->ctrlPktInfo.ctrlPkts[0] == LL_CTRL_CHANNEL_MAP_IND)) ||
            (connPtr->pendingChanUpdate == TRUE) )
      {
        // If a specific connID
        if( connID != maxNumConns )
        {
          return LL_STATUS_ERROR_CTRL_PROC_ALREADY_ACTIVE;
        }
        continue;
      }
      // Can only send if the active connection is Central.
      if(connPtr->llTask->taskID != LL_TASK_ID_CENTRAL)
      {
        // If a specific connID
        if( connID != maxNumConns )
        {
          return LL_STATUS_ERROR_BAD_PARAMETER;
        }
        continue;
      }

      // Save the new channel map
      for (j=0; j<LL_NUM_BYTES_FOR_CHAN_MAP; j++)
      {
        connPtr->curChanMap.chanMap[j] = chanMap[j];
      }

      // Determine the channel update event by setting the relative offset of
      // the number of events for the update to take place
      // Note: The absolute event number will be determined at the time the
      //       packet is placed in the TX FIFO.
      // Note: The central should allow a minimum of 6 connection events that the
      //       peripheral will be listening for before the instant occurs.
      // Note: Since the spec says the Peripheral is supposed to listen after it gets
      //       an update until its ACK is ACK'ed by the Central, then the instant
      //       should be SL+1+6.
      connPtr->chanMapUpdateEvent = (connPtr->curParam.peripheralLatency+1) +
                                    LL_INSTANT_NUMBER_MIN;

      // Queue control packet for processing
      MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_CHANNEL_MAP_IND );
    }
    i++;

    // Continue for next connection only if needs to run on all of the connections
  } while(i < maxNumConns && connID == maxNumConns);

  return( LL_STATUS_SUCCESS );
}
#endif // INIT_CFG


//#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * This API is called by the Central HCI to setup encryption and to update
 * encryption keys in the LL connection. If the connection is already in
 * encryption mode, then this command will first pause the encryption before
 * subsequently running the encryption setup.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_StartEncrypt( uint16  connId,
                            uint8  *rand,
                            uint8  *eDiv,
                            uint8  *ltk )
{
  uint8          i;
  llStatus_t     status;
  llConnState_t *connPtr;
  halIntState_t  cs;
  uint8          tempSKD[LL_ENC_SKD_LINK_LEN] ALIGNED = {0};
  uint8          tempIV[LL_ENC_IV_LINK_LEN] ALIGNED = {0};

  // check parameters
  if ( (rand == NULL) || (eDiv == NULL) || (ltk == NULL) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connId)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // get connection info
  connPtr = MAP_llDataGetConnPtr( connId );

  // make sure connection is in Central role
  if ( connPtr->llTask->taskID != LL_TASK_ID_CENTRAL )
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }

  // check if encryption is a supported feature set item
  if ( !(connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_ENCRYPTION) )
  {
    return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
  }

  // check if an encryption procedure is already in progress
  if ( connPtr->encInfo.encInProgress == TRUE )
  {
    return( LL_STATUS_ERROR_CTRL_PROC_ALREADY_ACTIVE );
  }

  // generate SKDm
  // Note: The SKDm LSO is the LSO of the SKD.
  // Note: Placement of result forms concatenation of SKDm and SKDs.
  // Note: The order of the bytes will be maintained as MSO..LSO
  //       per FIPS 197 (AES).
  // Note: This will be set in a temporary variable since this API cannot be called
  //       from within a critical section
  MAP_LL_ENC_GenDeviceSKD( tempSKD );

  // generate IVm
  // Note: The IVm LSO is the LSO of the IV.
  // Note: Placement of result forms concatenation of IVm and IVs.
  // Note: The order of the bytes will be maintained as MSO..LSO
  //       per FIPS 197 (AES).
  // Note: This will be set in a temporary variable since this API cannot be called
  //       from within a critical section
  MAP_LL_ENC_GenDeviceIV( tempIV );

  HAL_ENTER_CRITICAL_SECTION(cs);

  // set a flag to indicate the encryption procedure is in progress
  connPtr->encInfo.encInProgress = TRUE;

  // cache the central's random vector
  // Note: The RAND will be left in LSO..MSO order as this is assumed to be the
  //       order of the bytes that will be returned to the Host.
  for (i=0; i<LL_ENC_RAND_LEN; i++)
  {
    connPtr->encInfo.RAND[i] = rand[i];
  }

  // cache the central's encryption diversifier
  // Note: The EDIV will be left in LSO..MSO order as this is assumed to be the
  //       order of the bytes that will be returned to the Host.
  connPtr->encInfo.EDIV[0] = eDiv[0];
  connPtr->encInfo.EDIV[1] = eDiv[1];

  // cache the central's long term key
  // Note: The order of the bytes will be maintained as MSO..LSO
  //       per FIPS 197 (AES).
  for (i=0; i<LL_ENC_LTK_LEN; i++)
  {
    connPtr->encInfo.LTK[(LL_ENC_LTK_LEN-i)-1] = ltk[i];
  }

  // copy temp SKD
  MAP_osal_memcpy(&connPtr->encInfo.SKD[ LL_ENC_SKD_M_OFFSET ], tempSKD, LL_ENC_SKD_LINK_LEN);

  // copy temp IV
  MAP_osal_memcpy(&connPtr->encInfo.IV[ LL_ENC_IV_M_OFFSET ], tempIV, LL_ENC_IV_LINK_LEN);

  // set flag to stop all outgoing transmissions
  connPtr->txDataEnabled = FALSE;

  // invalidate the existing session key, if any
  connPtr->encInfo.SKValid = FALSE;

  // indicate the LTK is not valid
  connPtr->encInfo.LTKValid = FALSE;

  // check if we are already in encryption mode
  if ( connPtr->encEnabled == TRUE )
  {
    // set a flag to indicate this is a restart (i.e. pause-then-start)
    connPtr->encInfo.encRestart = TRUE;

    // setup a pause encryption control procedure
    MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_PAUSE_ENC_REQ );
  }
  else // no, so...
  {
    // clear flag to indicate this is an encryption setup
    connPtr->encInfo.encRestart = FALSE;

    // setup an encryption control procedure
    MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_ENC_REQ );
  }

  HAL_EXIT_CRITICAL_SECTION(cs);

  return( LL_STATUS_SUCCESS );
}
//#endif // INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This API is called by the Central or Peripheral HCI to initiate a Feature Exchange
 * control procedure.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadRemoteUsedFeatures( uint16 connId )
{
  llStatus_t     status;
  llConnState_t *connPtr;

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connId)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // get connection info
  connPtr = MAP_llDataGetConnPtr( connId );

  // check if a feature exchange procedure has already been completed
  if ( connPtr->featureSetInfo.featureRspRcved == LL_FEATURE_RSP_DONE )
  {
    // notify the Host
    MAP_LL_ReadRemoteUsedFeaturesCompleteCback( LL_STATUS_SUCCESS,
                                                connPtr->connId,
                                                connPtr->featureSetInfo.featureSetMask );
  }
  else // never done on this connection before
  {
    // check which connection role we're in
    // Note: Cannot use llState here due to combo roles!
    if ( connPtr->llTask->taskID == LL_TASK_ID_CENTRAL )
    {
      // initiate a Feature Set control procedure
      MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_FEATURE_REQ );
    }
    else if ( connPtr->llTask->taskID == LL_TASK_ID_PERIPHERAL )
    {
      // check if the Peripheral Feature Request is a supported feature set item
      if ( !(connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_SLV_FEATURES_EXCHANGE) )
      {
        return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
      }
      else // a supported feature for this connection
      {
        // so initiate a Peripheral Feature Set control procedure
        MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_PERIPHERAL_FEATURE_REQ );
      }
    }
  }

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG

// V4.1 - Ping

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This LE API is called by HCI to get the Auhenticated Payload Timeout (APTO)
 * value for this connection.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadAuthPayloadTimeout( uint16  connId,
                                      uint16 *apto )
{
  llStatus_t     status;
  llConnState_t *connPtr = MAP_llDataGetConnPtr( connId );

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connId)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // return connection's APTO, in units of 10ms
  *apto = connPtr->aptoValue / 10;

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This LE API is called by HCI to set the Auhenticated Payload Timeout (APTO)
 * value for this connection.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_WriteAuthPayloadTimeout( uint16 connId,
                                       uint16 apto )
{
  llStatus_t     status;
  llConnState_t *connPtr;

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connId)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // get connection info
  connPtr = MAP_llDataGetConnPtr( connId );

  // check the APTO
  // Note: The CI and APTO are normalized to units of 1.25ms.
  if ( (apto==0) || LL_INVALID_APTO_COMBO( connPtr->curParam.connInterval*2,
                                           connPtr->curParam.peripheralLatency,
                                           apto*8 ) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // save the connection's APTO, in ms
  // Note: Parameter value is in units of 10ms.
  connPtr->aptoValue = apto*10;

  // and restart the timer if already running
  if ( connPtr->encEnabled )
  {
    halIntState_t cs;

    HAL_ENTER_CRITICAL_SECTION(cs);

    // reset the expiration toggle flag
    // Note: Rx ISR sets numAptoExp to zero as well, so no race condition.
    connPtr->numAptoExp = 0;

    // restart the APTO timer
    MAP_osal_CbTimerUpdate( connPtr->aptoTimerId, (connPtr->aptoValue / 2) );

    HAL_EXIT_CRITICAL_SECTION(cs);
  }

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG


// V4.2 - Extended Data Length

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This function is used to set the maximum transmission packet size and the
 * maximum packet transmission time for the connection.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_SetDataLen( uint16 connHandle,
                          uint16 txOctets,
                          uint16 txTime)
{
  llConnState_t *connPtr;
  llStatus_t     status;

  // range check parameters
  if ( (txOctets < LL_MIN_LINK_DATA_LEN) || (txOctets > LL_MAX_LINK_DATA_LEN) ||
       (txTime  < LL_MIN_LINK_DATA_TIME) || (txTime > LL_MAX_LINK_DATA_TIME) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check parameters against supported max sizes
  if ( (txOctets > supportedMaxTxOctets) ||
       (txTime   > supportedMaxTxTime) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connHandle)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // get connection pointer
  connPtr = MAP_llDataGetConnPtr( connHandle );

  // check if this feature is supported
  if ( !(connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_DATA_PACKET_LENGTH_EXTENSION) )
  {
    return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
  }

  // check if this control procedure has been disabled
  // Note: If a Len Request was answered with an Unknown Response, then the
  //       LL is not to send this request again.
  if ( TST_FEATURE_FLAG( connPtr->lenInfo.lenFlags, DISABLE_LEN_REQUEST ) )
  {
    return( LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE );
  }

  // check if head of control queue is LL_CTRL_LENGTH_REQ

  // check if already received a LL_LENGTH_REQ
  if ( connPtr->pendingLenUpdate == TRUE )
  {
    // per the spec, allow our LL_LENGTH_RSP to complete the control procedure
    return( LL_STATUS_ERROR_CTRL_PROC_ALREADY_ACTIVE );
  }

  // check if a feature exchange procedure has already been completed
  // and if the peer does not support Coded
  if ( ( connPtr->featureSetInfo.featureRspRcved == LL_FEATURE_RSP_DONE ) &&
       ( !(connPtr->featureSetInfo.featureSet[1] & LL_FEATURE_CODED_PHY) ) )
  {
    // update the connMaxTxOctets variables
    connPtr->lenInfo.connMaxTxOctets = txOctets;

    // peer doesn't support coded phy, so limit max Tx/Rx time to no greater than 2120us
    // Note: Per Vol 6, Part B, Section 5.1.9.
    connPtr->lenInfo.connMaxTxTime = Math_MIN(txTime, LL_MAX_LINK_DATA_TIME_UNCODED);
    connPtr->lenInfo.connMaxRxTime = LL_MAX_LINK_DATA_TIME_UNCODED;
  }
  else // feature exchange procedure was not completed or peer supports coded
  {
    // update the connMaxTx variables as requested
    connPtr->lenInfo.connMaxTxOctets = txOctets;
    connPtr->lenInfo.connMaxTxTime   = txTime;
  }

  // set flag to indicate that a Length control procedure is in progress
  connPtr->pendingLenUpdate = TRUE;

  // schedule a data length update control procedure
  MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_LENGTH_REQ );

  return( LL_STATUS_SUCCESS );
}
#endif // (ADV_CONN_CFG | INIT_CFG)


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This function is used to read the default maximum trasmit packet size and
 * the default maximum packet transmit time to be used for new connections.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadDefaultDataLen( uint16 *txOctets,
                                  uint16 *txTime )
{
  // clear parameters in case of an error
  *txOctets = 0;
  *txTime   = 0;

  // check if this feature is supported
  if ( !(deviceFeatureSet.featureSet[0] & LL_FEATURE_DATA_PACKET_LENGTH_EXTENSION) )
  {
    return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
  }

  // return initial max Tx bytes and max Tx time
  *txOctets = connInitialMaxTxOctets;
  *txTime   = connInitialMaxTxTime;

  return( LL_STATUS_SUCCESS );
}
#endif // (ADV_CONN_CFG | INIT_CFG)


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This function is used to set the default maximum transmission packet size
 * and the default maximum packet transmission time for the connection.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_WriteDefaultDataLen( uint16 txOctets,
                                   uint16 txTime)
{
  // range check parameters
  if ( (txOctets < LL_MIN_LINK_DATA_LEN) || (txOctets > LL_MAX_LINK_DATA_LEN) ||
       (txTime  < LL_MIN_LINK_DATA_TIME) || (txTime > LL_MAX_LINK_DATA_TIME) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check parameters against supported max sizes
  if ( (txOctets > supportedMaxTxOctets) ||
       (txTime   > supportedMaxTxTime) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check if this feature is supported
  if ( !(deviceFeatureSet.featureSet[0] & LL_FEATURE_DATA_PACKET_LENGTH_EXTENSION) )
  {
    return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
  }

  // set the initial max Tx bytes and max Tx time
  connInitialMaxTxOctets = txOctets;
  connInitialMaxTxTime   = txTime;
  connInitialMaxTxTimeUncoded = txTime;
  connInitialMaxTxTimeCoded = txTime;

  return( LL_STATUS_SUCCESS );
}
#endif // (ADV_CONN_CFG | INIT_CFG)


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This function is used to read the maximum supported transmit and receive
 * payload octets and packet duration times.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadMaxDataLen( uint16 *maxTxOctets,
                              uint16 *maxTxTime,
                              uint16 *maxRxOctets,
                              uint16 *maxRxTime )
{
  // clear parameters in case of an error
  *maxTxOctets = 0;
  *maxTxTime   = 0;
  *maxRxOctets = 0;
  *maxRxTime   = 0;

  // check if this feature is supported
  if ( !(deviceFeatureSet.featureSet[0] & LL_FEATURE_DATA_PACKET_LENGTH_EXTENSION) )
  {
    return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
  }

  // return initial max Tx bytes and max Tx time
  *maxTxOctets = supportedMaxTxOctets;
  *maxTxTime   = supportedMaxTxTime;
  *maxRxOctets = supportedMaxRxOctets;
  *maxRxTime   = supportedMaxRxTime;

  return( LL_STATUS_SUCCESS );
}
#endif // (ADV_CONN_CFG | INIT_CFG)


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This function is used to set the maximum supported Rx Octets and time.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_SetMaxDataLen( uint16 txOctets,
                                 uint16 txTime,
                                 uint16 rxOctets,
                                 uint16 rxTime )
{
#ifdef LL_TEST_MODE
  if ( llTestMode.testCase == LL_TEST_MODE_TP_CON_MAS_BI_07 )
  {
    invalidTxOctets = txOctets;
    invalidTxTime = txTime;
    invalidRxOctets = rxOctets;
    invalidRxTime = rxTime;
    return( LL_STATUS_SUCCESS );
  }
#endif

  // check if there's any active connection
  if ( llConns.numActiveConns != 0 )
  {
    // there is, so disallow a change to the device's feature set
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }

  // check if this feature is supported
  if ( !(deviceFeatureSet.featureSet[0] & LL_FEATURE_DATA_PACKET_LENGTH_EXTENSION) )
  {
    return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
  }

  // only update when parameter is within a valid range of values
  if ( (txOctets >= LL_MIN_LINK_DATA_LEN) && (txOctets <= LL_MAX_LINK_DATA_LEN) )
  {
    supportedMaxTxOctets = txOctets;
  }

  // only update when parameter is within a valid range of values
  if ( (txTime >= LL_MIN_LINK_DATA_TIME) && (txTime <= LL_MAX_LINK_DATA_TIME) )
  {
    supportedMaxTxTime = txTime;
  }

  // only update when parameter is within a valid range of values
  if ( (rxOctets >= LL_MIN_LINK_DATA_LEN) && (rxOctets <= LL_MAX_LINK_DATA_LEN) )
  {
    supportedMaxRxOctets = rxOctets;
  }

  // only update when parameter is within a valid range of values
  if ( (rxTime >= LL_MIN_LINK_DATA_TIME) && (rxTime <= LL_MAX_LINK_DATA_TIME) )
  {
    supportedMaxRxTime = rxTime;
  }

  return( LL_STATUS_SUCCESS );
}
#endif // (ADV_CONN_CFG | INIT_CFG)

// V4.2 - Privacy 1.2
/*******************************************************************************
 * This API is called by the HCI to add one device to the list of address
 * translations used to resolve Resolvable Private Addresses in the Controller.
 *
 * Note: The key parameters are byte ordered LSO to MSO.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_AddDeviceToResolvingList( uint8  peerIdAddrType,
                                        uint8 *peerIdAddr,
                                        uint8 *peerIRK,
                                        uint8 *localIRK )
{

  // check parameters
  if ( ((peerIdAddrType != LL_DEV_ADDR_TYPE_PUBLIC) &&
        (peerIdAddrType != LL_DEV_ADDR_TYPE_RANDOM))                          ||
       ((peerIRK != NULL) && (peerIdAddr == NULL))                            ||
       ((peerIRK == NULL) && (peerIdAddr != NULL))                            ||
       ((peerIRK == NULL) && (localIRK == NULL)) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }


//The purpose of this flag is to distinguish between a CONTROLLER-ONLY device
//that needs to be qualified as a controller only device to other devices.
//We would not like to eliminate the option of changing data in the resolving list for the other devices.
//According to spec:
//In the sections for Vol 4, Part E 7.8.38, 7.8.39, 7.8.40, 7.8.44, 7.8.77 it has the requirement.
//This command shall not be used when:
//1.Advertising (other than periodic advertising) is enabled.
//2.Scanning is enabled.
//3.An HCI_LE_Create_Connection, HCI_LE_Extended_Create_Connection.
//4.HCI_LE_Periodic_Advertising_Create_Sync command is pending.
#ifdef CONTROLLER_ONLY
  //check if Resolving List in use
  if (LL_IsResolvingListInUsed() == TRUE)
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }
#endif

  // save Local IRK
  // Note: The spec does not require that there be a unique Local IRK per peer.
  // Note: The Local IRK may be zero.
  // Note: If Local IRK is valid, an RPA will be generated when AR is enabled.
  if ( localIRK != NULL )
  {
    MAP_osal_memcpy( resolvingList[LOCAL_RL_INDEX].IRK, localIRK, KEYLEN );

    // keys are received LSO..MSO, make MSO..LSO per FIPS 197 (AES)
    MAP_LL_ENC_ReverseBytes( resolvingList[LOCAL_RL_INDEX].IRK, KEYLEN );

    // generate RPA for peer identity address
    MAP_LL_PRIV_GenerateRPA( resolvingList[LOCAL_RL_INDEX].IRK,
                             resolvingList[LOCAL_RL_INDEX].RPA );
#ifdef QUAL_TEST
    // Add the local IRK per peer address - for test case LL/CON/INI/BV-23-C
    LL_PRIV_UpdateLocalIrkList( peerIdAddr, peerIdAddrType, resolvingList[LOCAL_RL_INDEX].IRK );
#endif
  }

  // find an empty entry for Peer Adddress and Address Type
  // Note: If Peer IRK is valid, the the Peer ID Address pointer is not NULL.
  if ( peerIRK != NULL )
  {
    // check the Peer ID Address
    // Note: Public can be anything, but Random must be Static.
    // Note: check that peer IRK is not all-zero to pass test case LL/DDI/SCN/BV-15-C
    if ( (peerIdAddrType == LL_DEV_ADDR_TYPE_RANDOM) &&
         (!MAP_LL_PRIV_IsZeroIRK( peerIRK )) &&
         ((peerIdAddr[B_ADDR_LEN-1] & RANDOM_ADDR_MASK) != STATIC_RANDOM_ADDR_MASK) )
    {
      return( LL_STATUS_ERROR_BAD_PARAMETER );
    }

    for (uint8 i=1; i<=BLE_RESOLVING_LIST_SIZE; i++)
    {
      // addr type is used to indicate if free
      // Note: Entry zero is reserved for Own device.
      if ( resolvingList[i].idAddrType == EMPTY_RESOLVE_LIST_ENTRY )
      {
        resolvingList[i].idAddrType = peerIdAddrType;
        MAP_osal_memcpy( resolvingList[i].idAddr, peerIdAddr, B_ADDR_LEN );

        // save the IRK
        // Note: The Peer IRK may be zero.
        MAP_osal_memcpy( resolvingList[i].IRK, peerIRK, KEYLEN );

        // check if the Peer IRK is valid (i.e. not equal to zero)
        if ( !MAP_LL_PRIV_IsZeroIRK( resolvingList[i].IRK ) )
        {
          // keys are received LSO..MSO, make MSO..LSO per FIPS 197 (AES)
          MAP_LL_ENC_ReverseBytes( resolvingList[i].IRK, KEYLEN );

          // generate RPA for peer identity address
          MAP_LL_PRIV_GenerateRPA( resolvingList[i].IRK, resolvingList[i].RPA );
        }

        // set default privacy mode
        resolvingList[i].privMode = LL_NETWORK_PRIVACY_MODE;

        // check if this is being done while Adv/Scan/Init is/are active
        if ( LL_IsRLActiveTasksRunning() == TRUE )
        {
          // (Radio core using dynamic filter list)
          if ( llUserConfig.useDFL == TRUE )
          {
            // Check if peer address complies with the privacy, and if
            // not, remove address from the dynamic filter list.
            if ( LL_PRIV_RemoveInvalidPeerId( &resolvingList[i],
                                             LL_DFL_GetDynamicFilterlist(),
                                             LL_DFL_GetRankTable() ) != USUCCESS )
            {
              return (LL_STATUS_ERROR_INVALID_PARAMS);
            }
          }
          else // !(Radio core using dynamic filter list)
          {
            alTable_t *pAlTable;
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
            if ( extScanInfo->scanMode == LL_SCAN_START )
            {
              pAlTable = GET_AL_TABLE_POINTER(extScanParam.filterList);
            }
            else
#endif // SCAN_CFG
            {
              pAlTable = alTable;
            }

            // tasks are running, so check if this one entry has a valid IRK and
            // uses Network Privacy Mode, and if so, update either the AL or the
            // Extended AL to mark that ID address as "ignore"
            MAP_LL_PRIV_CheckRLPeerIdEntry( &resolvingList[i],
                                            pAlTable );
            }
        }

        // While adding the device to the resolving list, traverse all active connections
        // and check if any of them match the RL record being added.
        // If one of the devices matches, update the connection structure with the IDA of this device.
        // For example, this can be later used to efficiently check if the connection already exists.
        for ( uint8 connIndx = 0; connIndx < maxNumConns; connIndx++ )
        {
          llConnState_t *connPtr = MAP_llDataGetConnPtr( connIndx );
          if ( connPtr->activeConn != UFALSE )
          {
             if( (connPtr->peerInfo.peerAddrType == LL_DEV_ADDR_TYPE_RANDOM) &&
                 (MAP_LL_PRIV_ResolveRPA( connPtr->peerInfo.peerAddr, resolvingList[i].IRK ) != UFALSE) )
             {
               connPtr->peerInfo.peerAddrType = MASK_ID_ADDRTYPE(resolvingList[i].idAddrType);
               (void) MAP_osal_memcpy( connPtr->peerInfo.peerAddr, resolvingList[i].idAddr , B_ADDR_LEN );
               break;
             }
          }
        }

        return( LL_STATUS_SUCCESS );
      }
    }
  }
  else
  {
    return( LL_STATUS_SUCCESS );
  }

  return( LL_STATUS_ERROR_OUT_OF_RESOLVING_LIST );
}

/*******************************************************************************
 * This API is called by the HCI to remove one device fromthe list of address
 * translations used to resolve Resolvable Private Addresses in the Controller.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_RemoveDeviceFromResolvingList( uint8  peerIdAddrType,
                                             uint8 *peerIdAddr )
{
  // check parameters
  if ( ((peerIdAddrType != LL_DEV_ADDR_TYPE_PUBLIC) &&
        (peerIdAddrType != LL_DEV_ADDR_TYPE_RANDOM)) ||
       (peerIdAddr == NULL) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }


//The purpose of this flag is to distinguish between a CONTROLLER-ONLY device
//that needs to be qualified as a controller only device to other devices.
//We would not like to eliminate the option of changing data in the resolving list for the other devices.
//According to spec:
//In the sections for Vol 4, Part E 7.8.38, 7.8.39, 7.8.40, 7.8.44, 7.8.77 it has the requirement.
//This command shall not be used when:
//1.Advertising (other than periodic advertising) is enabled.
//2.Scanning is enabled.
//3.An HCI_LE_Create_Connection, HCI_LE_Extended_Create_Connection.
//4.HCI_LE_Periodic_Advertising_Create_Sync command is pending.
#ifdef CONTROLLER_ONLY
  //check if Resolving List in use
  if (LL_IsResolvingListInUsed() == TRUE)
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }
#endif

  // find the matching entry (if present)
  for (uint8 i=1; i<=BLE_RESOLVING_LIST_SIZE; i++)
  {
    if ( (resolvingList[i].idAddrType == peerIdAddrType) &&
         MAP_osal_memcmp(resolvingList[i].idAddr, peerIdAddr, B_ADDR_LEN) )
    {
      // found, so remove
      resolvingList[i].idAddrType = EMPTY_RESOLVE_LIST_ENTRY;

      // check if this is being done while Adv/Scan/Init is/are active
      if ( LL_IsRLActiveTasksRunning() == TRUE )
      {
        // (Radio core using dynamic filter list)
        if ( llUserConfig.useDFL == TRUE )
        {
          // Check if peer address complies with the privacy, and if
          // not, remove address from the dynamic filter list.
          if ( LL_PRIV_RemoveInvalidPeerId( &resolvingList[i],
                                           LL_DFL_GetDynamicFilterlist(),
                                           LL_DFL_GetRankTable() ) != USUCCESS )
          {
            return (LL_STATUS_ERROR_INVALID_PARAMS);
          }
        }
        else // !(Radio core using dynamic filter list)
        {
          uint8      alIndex;
          alTable_t *pAlTable;

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
          if ( extScanInfo->scanMode == LL_SCAN_START )
          {
            pAlTable = GET_AL_TABLE_POINTER(extScanParam.filterList);
          }
          else
#endif // SCAN_CFG
          {
            pAlTable = alTable;
          }

          // tasks are running, so clear the "ignore" bit of this entry in the AL
          if ( (alIndex = MAP_AL_FindEntry( pAlTable,
                                            peerIdAddr,
                                            peerIdAddrType )) != pAlTable->numAlEntries )
          {
            // found Peer ID in the AL, so clear "ignore" flag
            CLR_AL_ENTRY_PRIV_IGNORE( pAlTable->pAlEntries[alIndex].alFlags );
          }
          // check the Extended AL
          else if ( (alIndex = MAP_LL_PRIV_FindExtALEntry( pAlTable,
                                                           peerIdAddr,
                                                           peerIdAddrType )) != INVALID_EXT_ACCEPT_LIST_INDEX )
          {
            // clear the entire entry
            // Note: The extended AL entry was created because the peer address
            //       and address type were in the RL, but not in the AL.
            MAP_AL_ClearEntry( &pAlTable->pAlEntries[alIndex] );
          }
          else
          {
              /* this else clause is required, even if the
                programmer expects this will never be reached
                Fix Misra-C Required: MISRA.IF.NO_ELSE */
          }

        }
      }

      return( LL_STATUS_SUCCESS );
    }
  }

  return( LL_STATUS_ERROR_UNKNOWN_CONN_HANDLE );
}


/*******************************************************************************
 * This API is called by the HCI to remove all devices from the list of address
 * translations used to resolve Resolvable Private addresses in the Controller.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ClearResolvingList( void )
{

//The purpose of this flag is to distinguish between a CONTROLLER-ONLY device
//that needs to be qualified as a controller only device to other devices.
//We would not like to eliminate the option of changing data in the resolving list for the other devices.
//According to spec:
//In the sections for Vol 4, Part E 7.8.38, 7.8.39, 7.8.40, 7.8.44, 7.8.77 it has the requirement.
//This command shall not be used when:
//1.Advertising (other than periodic advertising) is enabled.
//2.Scanning is enabled.
//3.An HCI_LE_Create_Connection, HCI_LE_Extended_Create_Connection.
//4.HCI_LE_Periodic_Advertising_Create_Sync command is pending.
#ifdef CONTROLLER_ONLY
  //check if Resolving List in use
  if (LL_IsResolvingListInUsed() == TRUE)
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }
#endif

#if defined(QUAL_TEST)
    // clear the RPA and IRK only during QUAL test
    MAP_osal_memset( resolvingList[LOCAL_RL_INDEX].RPA, 0, B_ADDR_LEN );
    MAP_osal_memset( resolvingList[LOCAL_RL_INDEX].IRK, 0, KEYLEN );

    // set default privacy mode
    resolvingList[LOCAL_RL_INDEX].privMode = LL_NETWORK_PRIVACY_MODE;
#endif

  // clear all entries
  for (uint8 i=1; i<=BLE_RESOLVING_LIST_SIZE; i++)
  {
    // found, so remove
    resolvingList[i].idAddrType = EMPTY_RESOLVE_LIST_ENTRY;

    // clear the RPA and IRK
    MAP_osal_memset( resolvingList[i].RPA, 0, B_ADDR_LEN );
    MAP_osal_memset( resolvingList[i].IRK, 0, KEYLEN );

    // set default privacy mode
    resolvingList[i].privMode = LL_NETWORK_PRIVACY_MODE;
  }

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This API is called by the HCI to read the total number of address translation
 * entries in the resolving list that can be stored in the Controller.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadResolvingListSize( uint8 *resolvingListSize )
{
  // set out parameter to size of resolving list
  *resolvingListSize = BLE_RESOLVING_LIST_SIZE;

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This API is called by the HCI to get the current peer Resolvable Private
 * Address being used for the corresponding peer Public or Random (Static)
 * Identity Address.
 *
 * Note: The peer's Resolvable Private Address being used may change after
 *       this command is called.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadPeerResolvableAddress( uint8  peerIdAddrType,
                                         uint8 *peerIdAddr,
                                         uint8 *peerRPA )
{
  uint8 rlIndex;

  // check parameters
  if ( ((peerIdAddrType != LL_DEV_ADDR_TYPE_PUBLIC) &&
        (peerIdAddrType != LL_DEV_ADDR_TYPE_RANDOM))                          ||
       (peerIdAddr == NULL)                                                   ||
       (peerRPA == NULL) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  rlIndex = MAP_LL_PRIV_FindPeerInRL( resolvingList,
                                      peerIdAddrType,
                                      peerIdAddr );

  // check if entry is found
  if ( rlIndex != INVALID_RESOLVE_LIST_INDEX )
  {
    // found, so fetch peer RPA
    MAP_osal_memcpy( peerRPA, resolvingList[rlIndex].RPA, B_ADDR_LEN );

    return( LL_STATUS_SUCCESS );
  }

  // address/type not found so clear out peerRPA
  MAP_osal_memset( peerRPA, 0, B_ADDR_LEN);

  return( LL_STATUS_ERROR_UNKNOWN_CONN_HANDLE );
}


/*******************************************************************************
 * This API is called by the HCI to get the current local Resolvable Private
 * Address being used for the corresponding local Public or Random (Static)
 * Identity Address.
 *
 * Note: The local Resolvable Private Address being used may change after
 *       this command is called.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadLocalResolvableAddress( uint8  localIdAddrType,
                                          uint8 *localIdAddr,
                                          uint8 *localRPA )
{
  // check parameters
  if ( ((localIdAddrType != LL_DEV_ADDR_TYPE_PUBLIC) &&
        (localIdAddrType != LL_DEV_ADDR_TYPE_RANDOM))                         ||
       (localIdAddr == NULL)                                                  ||
       (localRPA == NULL) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // return the local RPA
  // Note: The spec does not require that there be a unique local IRK per peer.
  MAP_osal_memcpy( localRPA, resolvingList[LOCAL_RL_INDEX].RPA, B_ADDR_LEN );

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This API is called by the HCI to enable resolution of Resolvable Private
 * Addresses in the Controller. This causes the Controller to use the resolving
 * list whenever the Controller receives a local or peer Resolvable Private
 * Address.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_SetAddressResolutionEnable( uint8 addrResolutionEnable )
{

//The purpose of this flag is to distinguish between a CONTROLLER-ONLY device
//that needs to be qualified as a controller only device to other devices.
//We would not like to eliminate the option of changing data in the resolving list for the other devices.
//According to spec:
//In the sections for Vol 4, Part E 7.8.38, 7.8.39, 7.8.40, 7.8.44, 7.8.77 it has the requirement.
//This command shall not be used when:
//1.Advertising (other than periodic advertising) is enabled.
//2.Scanning is enabled.
//3.An HCI_LE_Create_Connection, HCI_LE_Extended_Create_Connection.
//4.HCI_LE_Periodic_Advertising_Create_Sync command is pending.
#ifdef CONTROLLER_ONLY
  //check if Resolving List in use
  if (LL_IsResolvingListInUsed() == TRUE)
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }
#endif

  // check mode
  switch( addrResolutionEnable )
  {
    case LL_ENABLE_ADDR_RESOLUTION:
      // check that we aren't already enabled
      if ( privInfo.addrResolution ) break;

      // start/reload the timer
      // Note: While osal_start_reload_timer might be easier, the rpaTimeout
      //       may change, so the timer needs to be restarted with possibly a
      //       new timeout value. For that reason, start/stop will be used.
      // ALT: Using osal_start_reload_timer might actually work as well since
      //      the timer is first searched for, and if found, used. So there
      //      could be a bit more overhead this way, but more straight forward.
      if ( MAP_osal_start_timerEx( LL_TaskID,
                                   LL_EVT_ADDRESS_RESOLUTION_TIMEOUT,
                                   privInfo.rpaTimeout ) != SUCCESS )
      {
        return( LL_STATUS_ERROR_HW_FAILURE );
      }

      // set global
      privInfo.addrResolution = TRUE;

      break;

    case LL_DISABLE_ADDR_RESOLUTION:
      // check that we aren't already disabled
      if ( !privInfo.addrResolution ) break;

      // stop timer
      MAP_osal_stop_timerEx( LL_TaskID, LL_EVT_ADDRESS_RESOLUTION_TIMEOUT );

      // clear the event (in case set)
      MAP_osal_clear_event( LL_TaskID, LL_EVT_ADDRESS_RESOLUTION_TIMEOUT );

      // set global
      privInfo.addrResolution = FALSE;

      break;

    default:
      // we have an invalid value for advertisement mode
      return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This API is called by the HCI to set the length of time the Controller uses
 * a Resolvable Private Address before a new Resolvable Private Address is
 * generated and starts being used. Note that this timeout applies to all
 * addresses generated by the Controller.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_SetResolvablePrivateAddressTimeout( uint16 rpaTimeout )
{
  llStatus_t status = LL_STATUS_SUCCESS;

  // check parameters
  // Note: Range is from 1s to 41400s, inclusive.
  if ( (rpaTimeout == 0) || (rpaTimeout > MAX_RPA_TIMEOUT) )
  {
    return( LL_STATUS_ERROR_PARAM_OUT_OF_RANGE );
  }

  // set the global
  privInfo.rpaTimeout = rpaTimeout * 1000; // osal timer in ms

  // check if Address Resolution is enabled and Local IRK is valid (not zero)
  if ( privInfo.addrResolution == LL_ENABLE_ADDR_RESOLUTION )
  {
    // stop timer
    MAP_osal_stop_timerEx( LL_TaskID, LL_EVT_ADDRESS_RESOLUTION_TIMEOUT );

    // set the event
    MAP_osal_set_event( LL_TaskID, LL_EVT_ADDRESS_RESOLUTION_TIMEOUT );
  }

  return( status );
}


/*******************************************************************************
 * This API is called by the HCI to set the Privacy Mode to either Network
 * Privacy Mode or Device Privacy Mode. The Privacy Mode can be set for any
 * peer in the Resolving List.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_SetPrivacyMode( uint8  peerIdAddrType,
                              uint8 *peerIdAddr,
                              uint8  privacyMode )
{
  // check parameters
  if ( ((privacyMode    != LL_NETWORK_PRIVACY_MODE) &&
        (privacyMode    != LL_DEVICE_PRIVACY_MODE))                           ||
       ((peerIdAddrType != LL_DEV_ADDR_TYPE_PUBLIC) &&
        (peerIdAddrType != LL_DEV_ADDR_TYPE_RANDOM))                          ||
       (peerIdAddr      == NULL) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

//The purpose of this flag is to distinguish between a CONTROLLER-ONLY device
//that needs to be qualified as a controller only device to other devices.
//We would not like to eliminate the option of changing data in the resolving list for the other devices.
//According to spec
//In the sections for Vol 4, Part E 7.8.38, 7.8.39, 7.8.40, 7.8.44, 7.8.77 it has the requirement.
//This command shall not be used when:
//1.Advertising (other than periodic advertising) is enabled.
//2.Scanning is enabled.
//3.An HCI_LE_Create_Connection, HCI_LE_Extended_Create_Connection.
//4.HCI_LE_Periodic_Advertising_Create_Sync command is pending

#ifdef CONTROLLER_ONLY
  //check if Resolving List in use
  if (LL_IsResolvingListInUsed() == TRUE)
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }
#endif

  {
    // locate peer in the resolving list
    uint8 index = MAP_LL_PRIV_FindPeerInRL( resolvingList,
                                            peerIdAddrType,
                                            peerIdAddr );

    if ( index != INVALID_RESOLVE_LIST_INDEX )
    {
      // valid entry found, so update the privacy mode
      resolvingList[index].privMode = privacyMode;

      return( LL_STATUS_SUCCESS );
    }
  }

  return( LL_STATUS_ERROR_UNKNOWN_CONN_HANDLE );
}

// V4.2 - Secure Connections

/*******************************************************************************
 * This function is used to read the local P-256 public key from the Controller.
 * The Controller shall generate a new P-256 public/private key pair upon
 * receipt of this command.
 *
 * Note: Generates LE Read Local P256 Public Key Complete event.
 *
 * WARNING: THIS ROUTINE WILL TIE UP THE LL FOR ABOUT 160ms!
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadLocalP256PublicKeyCmd( void )
{
  int8 status;

  uint8_t p256Key[ LL_SC_P256_KEY_LEN_OCTET_STRING_FORMAT ] = {0};
  status = MAP_ll_ReadLocalP256PublicKey(p256Key);

  // MAP_ll_ReadLocalP256PublicKey() is calling ECDH_generatePublicKey() which reverse
  // p256Key from little-endian to big-endian. The stack is using little-endian,
  // therefore, reversing p256Key back to little-endian.
  MAP_LL_ENC_ReverseBytes(p256Key + 1, LL_SC_DHKEY_LEN);
  MAP_LL_ENC_ReverseBytes(p256Key + (LL_SC_DHKEY_LEN + 1), LL_SC_DHKEY_LEN);

  // check the ECC software status; reuse status for LL status
  status = (status == ECDH_STATUS_SUCCESS) ?
            HCI_SUCCESS       :
            HCI_ERROR_CODE_UNSPECIFIED_ERROR;

  // generate the Complete event
  // Note: The p256KeyX and p256KeyY parts are preceded by four bytes that
  //       contain the length in words. The offsets used here get rid of the
  //       these lengths so this aspect of the ECC driver stays hidden from
  //       the callback.
  MAP_LL_ReadLocalP256PublicKeyCompleteEventCback( (uint8) status,
                                                   p256Key + 1,
                                                   &p256Key[(LL_SC_P256_KEY_LEN/2) + 1] );

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This LE API is used to initiate the generation of a Diffie-hellman key in
 * the Controller for use over the LE transport. This command takes the remote
 * P-256 public key as input. The Diffie-Hellman key generation uses the
 * private key generated by LE_Read_Local_P256_Public_Key command.
 *
 * Note: Generates LE DHKey Generation Complete event.
 *
 * WARNING: THIS ROUTINE WILL TIE UP THE LL FOR ABOUT 160ms!
 *
 * Public function defined in hci.h.
 */
llStatus_t LL_GenerateDHKeyCmd( uint8 *publicKey )
{
  int8 status;
  uint8 dhKey[ (2*LL_SC_DHKEY_LEN) + 1 ];
  // Create a new public key array with an octet string format size
  uint8 publicKeyOS[ (2*LL_SC_DHKEY_LEN) + 1 ];

  // Copy the publicKey into publicKeyOS.
  // publicKeyOS[1] - publicKeyOS[(2*LL_SC_DHKEY_LEN) + 1] should contain the key
  MAP_osal_memcpy(publicKeyOS + 1, publicKey, 2*LL_SC_DHKEY_LEN);

  // The ECDH driver is expecting the public key to be in octet string format,
  // the formatting byte is (0x04) and the public key array contains [0x04, X, Y]
  publicKeyOS[0] = 0x04;

  // MAP_ll_GenerateDHKey() is calling ECDH_computeSharedSecret() which expects
  // to get publicKeyOS in big-endian format. The stack is using little-endian,
  // therefore, reversing publicKeyOS to big-endian.
  MAP_LL_ENC_ReverseBytes(publicKeyOS + 1, LL_SC_DHKEY_LEN);
  MAP_LL_ENC_ReverseBytes(publicKeyOS + (LL_SC_DHKEY_LEN + 1), LL_SC_DHKEY_LEN);

  status = MAP_ll_GenerateDHKey(publicKeyOS, dhKey);

  // check the ECC software status; reuse status for LL status
  status = (status == ECDH_STATUS_SUCCESS) ?
           HCI_SUCCESS                     :
           HCI_ERROR_CODE_UNSPECIFIED_ERROR;

  // Reversing dhKey to little-endian format.
  MAP_LL_ENC_ReverseBytes((dhKey + 1), LL_SC_DHKEY_LEN);

  // generate the Complete event
  // Note: The DH key is return in two parts, X and Y. Only X is needed by Host.
  MAP_LL_GenerateDHKeyCompleteEventCback( (uint8)status, dhKey + 1);

  return( LL_STATUS_SUCCESS );
}

// V5.0 - 2M and Coded PHY

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This function is used to read the current transmitter and receiver PHY.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_ReadPhy( uint16  connHandle,
                       uint8  *txPhy,
                       uint8  *rxPhy )
{
  llConnState_t *connPtr;
  llStatus_t     status;

  // check if the parameters are valid
  if ( (txPhy == NULL) || (rxPhy == NULL) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // clear in case of an error
  *txPhy = LL_PHY_NONE;
  *rxPhy = LL_PHY_NONE;

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connHandle)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // get connection pointer
  connPtr = MAP_llDataGetConnPtr( connHandle );

  // return the current Tx and Rx PHY
  // Note: Only symmetric PHY is supported.
  *txPhy = LL_ConvertPhy(connPtr->phyInfo.curPhy);
  *rxPhy = LL_ConvertPhy(connPtr->phyInfo.curPhy);

  return( LL_STATUS_SUCCESS );
}
#endif // (ADV_CONN_CFG | INIT_CFG)


/*******************************************************************************
 * This function allows the Host to specify its preferred values for the
 * transmitter and receiver PHY to be used for all subsequent connections.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_SetDefaultPhy( uint8 allPhys,
                             uint8 txPhy,
                             uint8 rxPhy )
{
  uint8 tx, rx;
  uint8 supportedPhys = LL_PHY_SUPPORTED_PHYS;

  // down select supported phys based on feature bits
  supportedPhys &= ((deviceFeatureSet.featureSet[1] & LL_FEATURE_2M_PHY)    ?  ~0 : ~LL_PHY_2_MBPS);
  supportedPhys &= ((deviceFeatureSet.featureSet[1] & LL_FEATURE_CODED_PHY) ?  ~0 : ~LL_PHY_CODED);

  // check if the Host wants to use the txPhy parameter
  if ( allPhys & LL_PHY_USE_ANY_PHY )
  {
    tx = supportedPhys;
  }
  else // LL_PHY_USE_PHY_PARAM
  {
    // filter out phys based on build configuration; check if anythings left
    if ( !(tx = txPhy & supportedPhys) )
    {
      return( LL_STATUS_ERROR_UNSUPPORTED_PARAM_VAL );
    }
  }

  // check if the Host wants to use the rxPhy parameter
  if ( (allPhys >> 1) & LL_PHY_USE_ANY_PHY )
  {
    rx = supportedPhys;
  }
  else // LL_PHY_USE_PHY_PARAM
  {
    // filter out phys based on build configuration; check if anythings left
    if ( !(rx = rxPhy & supportedPhys) )
    {
      return( LL_STATUS_ERROR_UNSUPPORTED_PARAM_VAL );
    }
  }

  // check if there are any common phys (for symmetric connections)
  if ( !(tx & rx) )
  {
    return( LL_STATUS_ERROR_INVALID_PARAMS );
  }

  // save common phys as new default
  defaultPhy = tx & rx;

  return( LL_STATUS_SUCCESS );
}


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This function is used to request a change to the transmitter and receiver PHY
 * for a connection.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_SetPhy( uint16 connHandle,
                      uint8  allPhys,
                      uint8  txPhy,
                      uint8  rxPhy,
                      uint16 phyOpts )
{
  llConnState_t *connPtr;
  uint8          tx, rx, phy, supportedPhys;
  llStatus_t     status;

  // check parameter
  if ( ((phyOpts != LL_PHY_OPT_NONE)    &&
        (phyOpts != LL_PHY_OPT_S2)      &&
        (phyOpts != LL_PHY_OPT_S8)) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connHandle)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // get connection pointer
  connPtr = MAP_llDataGetConnPtr( connHandle );

  // initialize local phy preferences
  supportedPhys = LL_PHY_SUPPORTED_PHYS;

  // down select connection phy preferences based on device feature bits
  supportedPhys &= ((deviceFeatureSet.featureSet[1] & LL_FEATURE_2M_PHY)    ?  ~0 : ~LL_PHY_2_MBPS);
  supportedPhys &= ((deviceFeatureSet.featureSet[1] & LL_FEATURE_CODED_PHY) ?  ~0 : ~LL_PHY_CODED);

  // check if this control procedure has been disabled
  // Note: If a Phy Request was answered with an Unknown Response, then the
  //       LL is not to send this request again.
  if ( TST_FEATURE_FLAG( connPtr->phyInfo.phyFlags, DISABLE_PHY_REQUEST ) )
  {
    return( LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE );
  }

  // check for symmetric phy in case the Host wants to use the txPhy and rxPhy parameters
  // In case we have to use the tx and rx parameters because allPhys == LL_PHY_USE_PHY_PARAM
  if ( allPhys == LL_PHY_USE_PHY_PARAM )
  {
    // According to test spec LL/CON/MAS/BV-42 when ALL_PHYS is 0x00 and TX_PHYS does not equal
    // RX_PHYS, return the error code Unsupported Feature or Parameter Value (0x11)
    if (txPhy != rxPhy)
    {
      return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
    }
    // There is an overlap bitwise between the txPhy and rxPhy
    else
    {
      rx = tx = (txPhy & rxPhy) & supportedPhys;
    }
  } // End if  if ( allPhys == LL_PHY_USE_PHY_PARAM )

  // In case we can use any Tx phy and have to use Rx param
  else if ( allPhys == LL_PHY_USE_ANY_TX_PHY_RX_PARAM )
  {
    // filter out phys based on connection preferences; check if anything's left
    if ((rxPhy > supportedPhys) || ( !(rx = rxPhy & supportedPhys) ))
    {
      return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
    }
    else
    {
      //configure the rx and tx to as the rx
      rx = tx = rxPhy & supportedPhys;
    }
  }

  // In case we can use any Rx phy and have to use Tx param
  else if ( allPhys == LL_PHY_USE_ANY_RX_PHY_TX_PARAM )
  {
    // filter out phys based on connection preferences; check if anything's left
    if ((txPhy > supportedPhys) || ( !(tx = txPhy & supportedPhys) ))
    {
      return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
    }
    else
    {
      //configure the rx and tx to as the tx
      rx = tx = txPhy & supportedPhys;
    }
  }

  // In case we can use any Rx and any Tx - choose all supported phys
  else
  {
    rx = tx = supportedPhys;
  }

  // Set the phy to be the bitwise and between the tx and rx;
  phy = (tx & rx) & supportedPhys;

  // update preferred PHYs for this connection
  connPtr->phyInfo.phyPreference = phy;

  // save coded phy options, if any
  // Note: If the Host has no preference, then default to fastest coded scheme,
  //       otherwise, use the coded scheme specified.
  connPtr->phyInfo.phyOpts = phyOpts;

  // check if more than one phy is left
  // Note: This device only supports symmetric connections. This means the Tx
  //       and Rx phy must be the same, and only one phy can be specified.
  if ( phy & (phy-1) )
  {
    // there is more than one phy specified, so check if Coded is among them
    if ( (connPtr->phyInfo.phyPreference & LL_PHY_CODED) &&
         (connPtr->phyInfo.phyOpts != LL_PHY_OPT_NONE) )
    {
      // choose coded
      phy = LL_PHY_CODED;
    }
    else // find the fastest phy
    {
      // so need to down select to one phy
      // Note: The coded phy will only be selected when it is the only phy
      //       specified. Otherwise, the fastest phy is always chosen.
      for (uint8 i=LL_PHY_FASTEST_PHY; i>0; i>>=1)
      {
        // for now, choose the fastest PHY
        if ( phy & i )
        {
          // update to this phy
          phy = i;

          break;
        }
      }
    }
  }

  // save the PHYs to use
  connPtr->phyInfo.updatePhy = phy;

  // for the Peripheral, limit Tx time based on TX_PHYS
  // Note: This is based on BLE5, Volume 6, Part B, Section 5.1.10.1.
  if ( connPtr->llTask->taskID == LL_TASK_ID_PERIPHERAL )
  {
    // determine the slowest PHY based on Peripheral's TX_PHYS
    connPtr->lenInfo.connSlowestPhy =
      MAP_llGetSlowestPhy( connPtr->phyInfo.updatePhy );

    connPtr->lenInfo.connActualMaxTxOctets =
      Math_MIN( connPtr->lenInfo.connEffectiveMaxTxOctets,
                MAP_llTime2Octets( connPtr->lenInfo.connSlowestPhy,
                                   connPtr->phyInfo.phyOpts,
                                   connPtr->lenInfo.connEffectiveMaxTxTime,
                                   MIC_ENABLED ) );
  }

  // setup an PHY control procedure
  MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_PHY_REQ );

  return( LL_STATUS_SUCCESS );
}
#endif // (ADV_CONN_CFG | INIT_CFG)

/*******************************************************************************
 * This function is used to start a test where the DUT generates test reference
 * packets at a fixed interval. The Controller shall transmit at maximum power.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EnhancedTxTest( uint8 txChan,
                              uint8 payloadLen,
                              uint8 payloadType,
                              uint8 txPhy )
{
  // check if this feature is supported
  if ( !(deviceFeatureSet.featureSet[1] & LL_FEATURE_2M_PHY) &&
       !(deviceFeatureSet.featureSet[1] & LL_FEATURE_CODED_PHY) )
  {
    return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
  }

  // check if txPhy parameter contains a single valid PHY
  if ((txPhy != LL_DTM_TX_1_MBPS) &&
      (txPhy != LL_DTM_TX_2_MBPS) &&
      (txPhy != LL_DTM_TX_C2) &&
      (txPhy != LL_DTM_TX_C8))
  {
    return( LL_STATUS_ERROR_UNSUPPORTED_PARAM_VAL );
  }

  // start the test
  return ( MAP_LL_DirectTestTxTest( txChan,
                                    payloadLen,
                                    payloadType,
                                    txPhy ) );
}


/*******************************************************************************
 * This function is used to start a test where the DUT receives reference packets
 * at a fixed interval. The tester generates the test reference packets.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EnhancedRxTest( uint8 rxChan,
                              uint8 rxPhy,
                              uint8 modIndex )
{
  // check if this feature is supported
  if ( !(deviceFeatureSet.featureSet[1] & LL_FEATURE_2M_PHY) &&
       !(deviceFeatureSet.featureSet[1] & LL_FEATURE_CODED_PHY) )
  {
    return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
  }

  // check if rxPhy parameter contains a single valid PHY
  if ((rxPhy != LL_DTM_RX_1_MBPS) &&
      (rxPhy != LL_DTM_RX_2_MBPS) &&
      (rxPhy != LL_DTM_RX_CODED))
  {
    return( LL_STATUS_ERROR_UNSUPPORTED_PARAM_VAL );
  }

  // check the modulation index parameter
  if ( (modIndex != LL_DTM_STANDARD_MODULATION_INDEX) &&
       (modIndex != LL_DTM_STABLE_MODULATION_INDEX) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // start the test
  return ( MAP_LL_DirectTestRxTest( rxChan,
                                    rxPhy ) );
}

/*******************************************************************************
 * This function is used to is used to read the minimum and maximum transmit
 * powers (in dBm) supported by the Controller.
 *
 * Public function defined in ll.h.
 */
llStatus_t LE_ReadTxPowerCmd( int8 *minTxPwr,
                              int8 *maxTxPwr )
{
  // return min/max transmit power
  *minTxPwr = RfBleDpl_getTxPowerDbm(RfBleDpl_getTxPowerMin());
  *maxTxPwr = RfBleDpl_getTxPowerDbm(RfBleDpl_getTxPowerMax());

  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * This function is used to read the RF Path Compensation Values (in 0.1 dBm)
 * parameter used in the Tx Power Level and RSSI calculation.
 *
 * Public function defined in ll.h.
 */
llStatus_t LE_ReadRfPathCompCmd( int16 *txPathParam,
                                 int16 *rxPathParam )
{
  // return the current
  *txPathParam = pRfPathComp->rfTxPathCompParam;
  *rxPathParam = pRfPathComp->rfRxPathCompParam;

  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * This function is used to indicate the RF path gain or loss (in 0.1 dBm)
 * between the RF transceiver and the antenna contributed by intermediate
 * components. A positive value means a net RF path gain and a negative value
 * means a net RF path loss.
 *
 * Public function defined in ll.h.
 */
llStatus_t LE_WriteRfPathCompCmd( int16 txPathParam,
                                  int16 rxPathParam )
{
  // check if the parameters are valid
  if ( (((uint16)txPathParam < 0xFB00) && ((uint16)txPathParam > 0x500)) ||
       (((uint16)rxPathParam < 0xFB00) && ((uint16)rxPathParam > 0x500)) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // save the Tx/Rx path compensation values
  pRfPathComp->rfTxPathCompParam = txPathParam;
  pRfPathComp->rfRxPathCompParam = rxPathParam;

  // find the rounded Tx value
  pRfPathComp->rfTxPathCompVal  = (int8)(pRfPathComp->rfTxPathCompParam) / 10;

  // find the rounded Rx value
  pRfPathComp->rfRxPathCompVal  = (int8)(pRfPathComp->rfRxPathCompParam) / 10;

  return( LL_STATUS_SUCCESS );
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * This API is called by the HCI to update the default channel map initiating an
 * Update Default Channel Map procedure.
 *
 *
 * Public function defined in ll.h.
 */

llStatus_t LL_SetDefChanMap( uint8 *chanMap )
{
  uint8 i;

  // parameter check
  if ( chanMap == NULL )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }
  // ensure non-data channels 37..39 are not set and that the Core spec V4.0
  // requirement of a minimum of two data channels be used is met
  if ( (chanMap[LL_NUM_BYTES_FOR_CHAN_MAP-1] & ~0x1F) ||
       (MAP_llAtLeastTwoChans( chanMap ) != TRUE) )
  {
    return( LL_STATUS_ERROR_ILLEGAL_PARAM_COMBINATION );
  }
  // save the new channel map
  for (i=0; i<LL_NUM_BYTES_FOR_CHAN_MAP; i++)
  {
    defaultChannelMap[i] = chanMap[i];
  }
  return( LL_STATUS_SUCCESS );
}
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * This API is called by the HCI to update the Host data channels initiating an
 * Update secondary advertising Data Channel procedure.
 *
 * Note: While it isn't specified, it is assumed that the Host expects an
 *       update channel map on all advertising sets.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_SetSecAdvChanMap( uint8 *chanMap )
{
  uint8 i,j;

  // parameter check
  if ( chanMap == NULL )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  // ensure non-data channels 37..39 are not set and that the Core spec V4.0
  // requirement of a minimum of two data channels be used is met
  if ( (chanMap[LL_NUM_BYTES_FOR_CHAN_MAP-1] & ~0x1F) ||
       (MAP_llAtLeastTwoChans( chanMap ) != TRUE) )
  {
    return( LL_STATUS_ERROR_ILLEGAL_PARAM_COMBINATION );
  }
#endif
  // save the new channel map
  secondaryAdvChannelMapPopCount = 0;
  for (i=0; i<LL_NUM_BYTES_FOR_CHAN_MAP; i++)
  {
    secondaryAdvChannelMap[i] = chanMap[i];

    for (j=0;j < BITS_PER_BYTE;j++)
    {
      secondaryAdvChannelMapPopCount += ((secondaryAdvChannelMap[i] >> j) & 1 );
    }
  }
#ifdef USE_PERIODIC_ADV
  MAP_llSetPeriodicAdvChmapUpdate( TRUE );
#endif
  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG

#ifdef LL_TEST_MODE
/*******************************************************************************
 * This API is used to to issue a soft or hard system reset.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_LLTestMode( uint8 testCase )
{
  // set the test case
  llTestMode.testCase = testCase;

  return( LL_STATUS_SUCCESS );
}
#endif // LL_TEST_MODE


/*******************************************************************************
* Vendor Specific Commands
*******************************************************************************/

/*******************************************************************************
 * This API is used to set the RF RX gain. If there is enough time to perform
 * this before the next radio event, then it is done immediately. Otherwise, it
 * will be scheduled for after the next radio event.
 *
 * Note: In the latter case, more overhead will be added to the radio
 *       post-processing.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_SetRxGain( uint8 rxGain,
                             uint8 *cmdComplete )
{
  if ( cmdComplete == NULL )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  *cmdComplete = TRUE;

  return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
}


/*******************************************************************************
 * This API is used to set the RF TX power and save the current Tx power index.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_SetTxPower( uint8  txPowerIdx,
                              uint8 *cmdComplete )
{
  if (cmdComplete == NULL)
  {
    // bad parameter
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }
  // indicate to HCI the command has completed
  *cmdComplete = TRUE;
  return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
}



/*******************************************************************************
 * This API is used to set the RF TX power and save the current Tx power index.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_SetTxPowerDbm( int8   txPowerDbm,
                                 uint8  fraction,
                                 uint8 *cmdComplete )
{
  // check the parameters are valid
  if (cmdComplete == NULL)
  {
    // bad parameter
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }
  RFBLEDPL_TX_POWER_HW_TYPE txPower = RfBleDpl_getTxPowerByTxPowerDbm(txPowerDbm, fraction);

  if (!RfBleDpl_txPowerIsValid(txPower))
  {
    // bad parameter
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // and save the value
  MAP_llSetTxPower( txPower );

  // indicate to HCI the command has completed
  *cmdComplete = TRUE;
  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * This function is used to enable or disable dividing down the system clock
 * while halted.
 *
 * Note: This command is disallowed if haltDuringRf is not defined.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_ClkDivOnHalt( uint8 control )
{
  return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
}


/*******************************************************************************
 * This HCI Extension API is used to indicate to the Controller whether or not
 * the Host will be using the NV memory during BLE operations.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_DeclareNvUsage( uint8 mode )
{
  return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
}


/*******************************************************************************
 * This API is called by the HCI to request the LL to decrypt the data in the
 * command using the key given in the command.
 *
 * Note: The parameters are byte ordered MSO to LSO.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_Decrypt( uint8 *key,
                           uint8 *encryptedData,
                           uint8 *plaintextData )
{
  // check parameters
  if ( (key == NULL) || (encryptedData == NULL) || (plaintextData == NULL) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // decrypt on behalf of the host
  MAP_LL_ENC_AES128_Decrypt( key, encryptedData, plaintextData );

  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * This API is called by the HCI to indicate to the Controller which features
 * can or can not be used.
 *
 * Note: The parameters are byte ordered LSO to MSO.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_SetLocalSupportedFeatures( uint8 *featureSet )
{
  uint8 i;
  // check parameters
  if ( featureSet == NULL )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  // check if there's any active connection
  if ( llConns.numActiveConns != 0 )
  {
    // there is, so disallow a change to the device's feature set
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }
#endif // ADV_CONN_CFG | INIT_CFG

  // set/clear any allowable features per the user
  for ( i = 0; i < LL_MAX_FEATURE_SET_SIZE; i++ )
  {
    deviceFeatureSet.featureSet[i] = featureSet[i];
  }

  // V5.0 - 2M and Coded PHY
  defaultPhy = LL_PHY_SUPPORTED_PHYS;

  // down select connection phy preferences based on device feature bits
  defaultPhy &= ((deviceFeatureSet.featureSet[1] & LL_FEATURE_2M_PHY)    ?  ~0 : ~LL_PHY_2_MBPS);
  defaultPhy &= ((deviceFeatureSet.featureSet[1] & LL_FEATURE_CODED_PHY) ?  ~0 : ~LL_PHY_CODED);

  return( LL_STATUS_SUCCESS );
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
/*******************************************************************************
 * This API is used to enable or disable the fast TX response time feature.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_SetFastTxResponseTime( uint8 control )
{
  // set global based on parameter
  switch( control )
  {
    case LL_EXT_ENABLE_FAST_TX_RESP_TIME:
      fastTxRespTime = LL_EXT_ENABLE_FAST_TX_RESP_TIME;
      break;

    case LL_EXT_DISABLE_FAST_TX_RESP_TIME:
      fastTxRespTime = LL_EXT_DISABLE_FAST_TX_RESP_TIME;
      break;

    default:
      // bad parameter
      return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
/*******************************************************************************
 * This API is used to enable or disable the suspension of peripheral latency.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_SetPeripheralLatencyOverride( uint8 control )
{
  // make sure we're in a connection as a Peripheral
  if ( llState != LL_STATE_CONN_PERIPHERAL )
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }

  // set global based on parameter
  switch( control )
  {
    case LL_EXT_ENABLE_PL_OVERRIDE:
      slOverride = LL_EXT_ENABLE_PL_OVERRIDE;
      break;

    case LL_EXT_DISABLE_PL_OVERRIDE:
      slOverride = LL_EXT_DISABLE_PL_OVERRIDE;
      break;

    default:
      // bad parameter
      return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG


/*******************************************************************************
 * This API is used start a continuous transmitter modem test, using either
 * a modulated or unmodulated carrier wave tone, at the frequency that
 * corresponds to the specified RF channel. Use LL_EXT_EndModemTest to end the
 * test.
 *
 * Note: A LL reset will be issued by LL_EXT_EndModemTest!
 * Note: The BLE device will transmit at the current TX power setting.
 * Note: This API can be used to verify this device meets Japan's TELEC
 *       regulations.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_ModemTestTx( uint8 cwMode,
                               uint8 rfChan )
{
  // verify input parameters are valid
  if ( (rfChan >= LL_TOTAL_NUM_RF_CHAN)  ||
       ((cwMode != LL_EXT_TX_MODULATED_CARRIER) &&
        (cwMode != LL_EXT_TX_UNMODULATED_CARRIER)) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check that we are idle until combo states are supported
  if ( llState != LL_STATE_IDLE )
  {
    return( LL_STATUS_ERROR_UNEXPECTED_STATE_ROLE );
  }

  RCL_init();

  /* Open client and provide settings */
  rfHandle = RCL_open(&rfClient, llUserConfig.lrfConfigPtr);

  txTestCmd = RCL_CmdBle5TxTest_DefaultRuntime();

  txTestCmd.config.sendCw = cwMode; /*!< 0: Send modulated signal. 1: Send CW */
  // modulated or unmodulated?
  txTestCmd.config.whitenMode = ( cwMode == LL_EXT_TX_UNMODULATED_CARRIER ) ? RCL_CMD_BLE5_WH_MODE_DEFAULT : RCL_CMD_BLE5_WH_MODE_PRBS15;

  txTestCmd.channel = rfChan + LL_DTM_RCL_CHANNEL_OFFSET;
  txTestCmd.txWord = 0;
  txTestCmd.txPower = curTxPowerVal;
  txTestCmd.common.phyFeatures = LL_EXT_RF_SETUP_1M_PHY;
  txTestCmd.common.runtime.callback = LL_rclTestCallback;
  txTestCmd.common.runtime.lrfCallbackMask.value = 0;
  txTestCmd.common.runtime.rclCallbackMask.value =  RCL_EventLastCmdDone.value;

// set state/role
  llState = LL_STATE_MODEM_TEST_TX;

  RCL_Command_submit(rfHandle, (RCL_Command_Handle)&txTestCmd);

  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * This API is used to start a continuous receiver modem test using a modulated
 * carrier wave tone, at the frequency that corresponds to the specific RF
 * channel. Any received data is discarded. Receiver gain may be adjusted using
 * the LL_EXT_SetRxGain command. RSSI may be read during this test by using the
 * LL_ReadRssi command. Use LL_EXT_EndModemTest command to end the test.
 *
 * Note: A LL reset will be issued by LL_EXT_EndModemTest!
 * Note: The BLE device will transmit at maximum power.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_ModemTestRx( uint8 rfChan )
{
  // verify input parameters are valid
  if ( rfChan >= LL_TOTAL_NUM_RF_CHAN )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check that we are idle until combo states are supported
  if ( llState != LL_STATE_IDLE )
  {
    return( LL_STATUS_ERROR_UNEXPECTED_STATE_ROLE );
  }

  RCL_init();

  /* Open client and provide settings */
  rfHandle = RCL_open(&rfClient, llUserConfig.lrfConfigPtr);

  rxTestCmd = RCL_CmdBle5GenericRx_DefaultRuntime();
  // set command parameters and output pointers
  rxTestCmd.ctx = &rxTestParam;
  rxTestCmd.stats = &rxTestOut;
  // set the default params such as Access Address and CRC init
  rxTestParam = RCL_CtxGenericRx_DefaultRuntime();
  rxTestParam.maxPktLen = 0xFF;
  /* Discard the data */
  rxTestParam.config.discardRxPackets = TRUE;
  rxTestParam.accessAddress = LL_DIRECT_TEST_SYNCH_WORD;
  // set the default statistics
  rxTestOut = RCL_StatsGenericRx_DefaultRuntime();
  rxTestOut.config.accumulate = 1;
  rxTestOut.config.activeUpdate = 1;
  // clear the counters
  rxTestOut.nRxOk      = 0;
  rxTestOut.nRxNok     = 0;
  rxTestOut.nRxFifoFull = 0;
  rxTestOut.lastRssi   = LL_RF_RSSI_UNDEFINED;
  rxTestOut.lastTimestamp  = 0;

  rxTestCmd.channel = rfChan + LL_DTM_RCL_CHANNEL_OFFSET;

  rxTestCmd.common.runtime.callback = LL_rclTestCallback;
  rxTestCmd.common.runtime.lrfCallbackMask.value = 0;
  rxTestCmd.common.runtime.rclCallbackMask.value =  RCL_EventLastCmdDone.value;

// set state/role
  llState = LL_STATE_MODEM_TEST_RX;

  RCL_Command_submit(rfHandle, (RCL_Command_Handle)&rxTestCmd);

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This API is used start the enhanced BLE5 continuous transmitter modem test,
 * using either a modulated or unmodulated carrier wave tone, at the frequency
 * that corresponds to the specified RF channel, for a given PHY (1M, 2M,
 * Coded S2, or Coded S8). Use LL_EXT_EndModemTest to end the test.
 *
 * Note: A LL reset will be issued by LL_EXT_EndModemTest!
 * Note: The BLE device will transmit at the current TX power setting.
 * Note: This API can be used to verify this device meets Japan's TELEC
 *       regulations.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_EnhancedModemTestTx( uint8 cwMode,
                                       uint8 rfPhy,
                                       uint8 rfChan )
{
  // verify input parameters are valid
  if ( (rfChan >= LL_TOTAL_NUM_RF_CHAN)                ||
       ((cwMode != LL_EXT_TX_MODULATED_CARRIER) &&
        (cwMode != LL_EXT_TX_UNMODULATED_CARRIER))     ||
       ((rfPhy != LL_EXT_RF_SETUP_1M_PHY)       &&
        (rfPhy != LL_EXT_RF_SETUP_2M_PHY)       &&
        (rfPhy != LL_EXT_RF_SETUP_CODED_S8_PHY) &&
        (rfPhy != LL_EXT_RF_SETUP_CODED_S2_PHY)) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check if this feature is supported
  if ( ((rfPhy == LL_EXT_RF_SETUP_2M_PHY) &&
        !(deviceFeatureSet.featureSet[1] & LL_FEATURE_2M_PHY)) )
  {
    return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
  }

  // check if this feature is supported
  if ( (((rfPhy == LL_EXT_RF_SETUP_CODED_S8_PHY) ||
         (rfPhy == LL_EXT_RF_SETUP_CODED_S2_PHY)) &&
        !(deviceFeatureSet.featureSet[1] & LL_FEATURE_CODED_PHY)) )
  {
    return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
  }

  // check that we are idle until combo states are supported
  if ( llState != LL_STATE_IDLE )
  {
    return( LL_STATUS_ERROR_UNEXPECTED_STATE_ROLE );
  }

  RCL_init();

  /* Open client and provide settings */
  rfHandle = RCL_open(&rfClient, llUserConfig.lrfConfigPtr);

  txTestCmd = RCL_CmdBle5TxTest_DefaultRuntime();

  txTestCmd.config.sendCw = cwMode; /*!< 0: Send modulated signal. 1: Send CW */
  // modulated or unmodulated?
  txTestCmd.config.whitenMode = ( cwMode == LL_EXT_TX_UNMODULATED_CARRIER ) ? RCL_CMD_BLE5_WH_MODE_DEFAULT : RCL_CMD_BLE5_WH_MODE_PRBS15;

  txTestCmd.channel = rfChan + LL_DTM_RCL_CHANNEL_OFFSET;
  txTestCmd.txWord = 0;
  txTestCmd.txPower = curTxPowerVal;

  txTestCmd.common.phyFeatures = rfPhy + LL_DTM_RCL_CHANNEL_OFFSET;

  txTestCmd.common.runtime.callback = LL_rclTestCallback;
  txTestCmd.common.runtime.lrfCallbackMask.value = 0;
  txTestCmd.common.runtime.rclCallbackMask.value =  RCL_EventLastCmdDone.value;

// set state/role
  llState = LL_STATE_MODEM_TEST_TX;

  RCL_Command_submit(rfHandle, (RCL_Command_Handle)&txTestCmd);

  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * This API is used to start the enhanced continuous transmitter direct test
 * mode test using a modulated carrier wave.
 *
 * Test steps:
 * A test reference data packet is transmitted on a different frequency
 * (linearly stepping through all RF channels 0..39), for a given PHY (1M, 2M, Coded S2, or Coded S8),
 * every period (depending on the payload length, as given Vol. 6, Part F, section 4.1.6).
 * Use LL_EXT_EndModemTest to end the test.
 *
 * Note: A LL reset will be issued by LL_EXT_EndModemTest!
 * Note: The BLE device will transmit at the current TX power setting.
 * Note: This API can be used to verify this device meets Japan's TELEC
 *       regulations.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_EnhancedModemHopTestTx( uint8 payloadLen,
                                          uint8 payloadType,
                                          uint8 rfPhy )
{
  uint32 payloadTime;
  uint8 blePhy;
  uint8 blePhyOpts;

  // verify input parameters are valid
  if ( ((payloadType != LL_DIRECT_TEST_PAYLOAD_PRBS9)  &&
        (payloadType != LL_DIRECT_TEST_PAYLOAD_0x0F)   &&
        (payloadType != LL_DIRECT_TEST_PAYLOAD_0x55)   &&
        (payloadType != LL_DIRECT_TEST_PAYLOAD_PRBS15) &&
        (payloadType != LL_DIRECT_TEST_PAYLOAD_0xFF)   &&
        (payloadType != LL_DIRECT_TEST_PAYLOAD_0x00)   &&
        (payloadType != LL_DIRECT_TEST_PAYLOAD_0xF0)   &&
        (payloadType != LL_DIRECT_TEST_PAYLOAD_0xAA))    ||
       ((rfPhy != LL_EXT_RF_SETUP_1M_PHY)       &&
        (rfPhy != LL_EXT_RF_SETUP_2M_PHY)       &&
        (rfPhy != LL_EXT_RF_SETUP_CODED_S8_PHY) &&
        (rfPhy != LL_EXT_RF_SETUP_CODED_S2_PHY)) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check if this feature is supported
  if ( ((rfPhy == LL_EXT_RF_SETUP_2M_PHY) &&
        !(deviceFeatureSet.featureSet[1] & LL_FEATURE_2M_PHY)) )
  {
    return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
  }

  // check if this feature is supported
  if ( (((rfPhy == LL_EXT_RF_SETUP_CODED_S8_PHY) ||
         (rfPhy == LL_EXT_RF_SETUP_CODED_S2_PHY)) &&
        !(deviceFeatureSet.featureSet[1] & LL_FEATURE_CODED_PHY)) )
  {
    return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
  }

  // check that we are idle until combo states are supported
  if ( llState != LL_STATE_IDLE )
  {
    return( LL_STATUS_ERROR_UNEXPECTED_STATE_ROLE );
  }

  // command initialization
  txDtmTestCmd = RCL_CmdBle5DtmTx_DefaultRuntime();

  // Intialize the channel to 64 (physical channel) corresponds to 0 (BLE channel) with 2402 MHz freq.
  // Note: 'txDtmTestCmd.channel' is holding the Physical channel number
  txDtmTestCmd.channel = LL_FIRST_RF_CHAN + LL_DTM_RCL_CHANNEL_OFFSET;

  // init packet length
  txDtmTestCmd.pduLength = payloadLen;

  // init packet type
  txDtmTestCmd.pduHeader = payloadType;

  txDtmTestCmd.common.phyFeatures = rfPhy;

  txDtmTestCmd.cteInfo = 0;

  txDtmTestCmd.txPower = RfBleDpl_getTxPower(maxTxPwrForDTM);

  // init number of of packets to transmit
  // Note: One per command, where command repeats continuously until stopped.
  // Note: This is set to one due to the requirement of changing channels.
  //       In this way we receive lastCmdDone, change the channel and resubmit the
  //       command in the callback.
  txDtmTestCmd.numPackets = 1;

  // determine the time in us for the payload length to be transmitted

  llDirectTestConvertLlPhyToBlePhy(rfPhy, &blePhy, &blePhyOpts);

  // find the payload time in us based on the data length and PHY
  payloadTime = MAP_llOctets2Time( blePhy,
                                   blePhyOpts,
                                   payloadLen,
                                   MIC_NOT_ENABLED );

  // find the DTM packet period based on BT V5.0, Vol. 6, Part F, section 4.1.6
  // For a LE Test Packet length of L us:
  //   I(L) = ceil((L + 249) / 625) * 625 us
  payloadTime =  ((payloadTime + 249) / 625) + (((payloadTime + 249) % 625) ? 1 : 0);
  payloadTime *= 625;

  txDtmTestCmd.periodUs = payloadTime;

  // set state/role
  llState = LL_STATE_MODEM_TEST_TX_FREQ_HOPPING;

  // Set callback function and events
  txDtmTestCmd.common.runtime.callback = LL_rclTestCallback;
  txDtmTestCmd.common.runtime.lrfCallbackMask.value = 0;
  txDtmTestCmd.common.runtime.rclCallbackMask.value =  RCL_EventLastCmdDone.value | RCL_EventTxBufferFinished.value;

  // post the command
  RCL_Command_submit(MAP_llScheduler_getHandle(LL_TASK_ID_STANDARD_BLE), (RCL_Command_Handle)&txDtmTestCmd);

  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * This API is used to start the enhanced BLE5 continuous receiver modem test
 * using a modulated carrier wave tone, at the frequency that corresponds to the
 * specific RF channel, for a given PHY (1M, 2M, Coded S2, or Coded S8). Any
 * received data is discarded. RSSI may be read during this test by using the
 * LL_ReadRssi command. Use LL_EXT_EndModemTest command to end the test.
 *
 * Note: A LL reset will be issued by LL_EXT_EndModemTest!
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_EnhancedModemTestRx( uint8 rfPhy,
                                       uint8 rfChan )
{
  // verify input parameters are valid
  if ( rfChan >= LL_TOTAL_NUM_RF_CHAN )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }
  // verify input parameters are valid
  if ( (rfChan >= LL_TOTAL_NUM_RF_CHAN)    ||
       ((rfPhy != LL_EXT_RF_SETUP_1M_PHY)       &&
        (rfPhy != LL_EXT_RF_SETUP_2M_PHY)       &&
        (rfPhy != LL_EXT_RF_SETUP_CODED_S8_PHY) &&
        (rfPhy != LL_EXT_RF_SETUP_CODED_S2_PHY)) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check if this feature is supported
  if ( ((rfPhy == LL_EXT_RF_SETUP_2M_PHY) &&
        !(deviceFeatureSet.featureSet[1] & LL_FEATURE_2M_PHY)) )
  {
    return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
  }

  // check if this feature is supported
  if ( (((rfPhy == LL_EXT_RF_SETUP_CODED_S8_PHY) ||
         (rfPhy == LL_EXT_RF_SETUP_CODED_S2_PHY)) &&
        !(deviceFeatureSet.featureSet[1] & LL_FEATURE_CODED_PHY)) )
  {
    return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
  }

  // check that we are idle until combo states are supported
  if ( llState != LL_STATE_IDLE )
  {
    return( LL_STATUS_ERROR_UNEXPECTED_STATE_ROLE );
  }

  RCL_init();

  /* Open client and provide settings */
  rfHandle = RCL_open(&rfClient, llUserConfig.lrfConfigPtr);

  rxTestCmd = RCL_CmdBle5GenericRx_DefaultRuntime();

  // set command parameters and output pointers
  rxTestCmd.ctx = &rxTestParam;
  rxTestCmd.stats = &rxTestOut;
  // set the default params such as Access Address and CRC init
  rxTestParam = RCL_CtxGenericRx_DefaultRuntime();
  rxTestParam.maxPktLen = 0xFF;
  /* Discard the data */
  rxTestParam.config.discardRxPackets = TRUE;
  // set the default statistics
  rxTestOut = RCL_StatsGenericRx_DefaultRuntime();
  rxTestOut.config.accumulate = 1;
  rxTestOut.config.activeUpdate = 1;
  // clear the counters
  rxTestOut.nRxOk       = 0;
  rxTestOut.nRxNok      = 0;
  rxTestOut.nRxFifoFull = 0;
  rxTestOut.lastTimestamp  = 0;

  rxTestCmd.channel = rfChan + LL_DTM_RCL_CHANNEL_OFFSET;

  rxTestCmd.common.phyFeatures = rfPhy;

  rxTestCmd.common.runtime.callback = LL_rclTestCallback;
  rxTestCmd.common.runtime.lrfCallbackMask.value = 0;
  rxTestCmd.common.runtime.rclCallbackMask.value =  RCL_EventLastCmdDone.value;

// set state/role
  llState = LL_STATE_MODEM_TEST_RX;

  RCL_Command_submit(rfHandle, (RCL_Command_Handle)&rxTestCmd);

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This API is used to shutdown a modem test. A complete link layer reset will
 * take place.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_EndModemTest( void )
{
  // check we are in a modem test state
  if ( (llState != LL_STATE_MODEM_TEST_TX) &&
       (llState != LL_STATE_MODEM_TEST_RX) &&
       (llState != LL_STATE_MODEM_TEST_TX_FREQ_HOPPING) )
  {
    return( LL_STATUS_ERROR_UNEXPECTED_STATE_ROLE );
  }

  RCL_Command *cmd;
  if (llState == LL_STATE_MODEM_TEST_TX)
    cmd = (RCL_Command *)&txTestCmd;
  else if (llState == LL_STATE_MODEM_TEST_RX)
    cmd = (RCL_Command *)&rxTestCmd;
  else if (llState == LL_STATE_MODEM_TEST_TX_FREQ_HOPPING)
    cmd = (RCL_Command *)&txDtmTestCmd;
  else
    return LL_STATUS_ERROR_UNEXPECTED_STATE_ROLE;

  MAP_llHaltRadio( (uint32)cmd );

  // reset link layer
  (void)MAP_LL_Reset();

  // ALT: Use CMD_WRITE_FWPAR, address 0x34 (mdmTxIntFreq), with address type
  //      of 2, value of 0x00000000. Restore with value of 0x00010000.
  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * This API is used to set this device's BLE address (BDADDR).
 *
 * Note: This command is only allowed when the device's state is Standby.
 * Note: An invalid address (i.e. all FF's) will restore this device's address
 *       to the address set at initialization.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_SetBDADDR( uint8 *bdAddr )
{
  // check parameters
  if ( bdAddr == NULL )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // make sure this device is idle to avoid anything odd from happening
  if ( llState != LL_STATE_IDLE )
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }

  // check if the specified address is valid
  if ( BDADDR_VALID( bdAddr ) == TRUE )
  {
    // yes, so save the previously valid BDADDR (either from Flash or CCFG)
    ownSavedPublicAddr[0] = ownPublicAddr[0];
    ownSavedPublicAddr[1] = ownPublicAddr[1];
    ownSavedPublicAddr[2] = ownPublicAddr[2];
    ownSavedPublicAddr[3] = ownPublicAddr[3];
    ownSavedPublicAddr[4] = ownPublicAddr[4];
    ownSavedPublicAddr[5] = ownPublicAddr[5];

    // and override the system BDADDR with the one specified by this command
    ownPublicAddr[0] = bdAddr[0];
    ownPublicAddr[1] = bdAddr[1];
    ownPublicAddr[2] = bdAddr[2];
    ownPublicAddr[3] = bdAddr[3];
    ownPublicAddr[4] = bdAddr[4];
    ownPublicAddr[5] = bdAddr[5];
  }
  else // set BDADDR is invalid
  {
    // restore own public address
    // Note: Do not invalidate ownSavedPublicAddr in case the user tries to set
    //       own public address with an invalid address using the HCI command.
    //       An invalid address is used to restore the previous ownPublicAddr
    //       from ownSavedPublicAddr. If we invalidate ownSavedPublicAddr here,
    //       we'll end up restoring an invalid address.
    ownPublicAddr[0] = ownSavedPublicAddr[0];
    ownPublicAddr[1] = ownSavedPublicAddr[1];
    ownPublicAddr[2] = ownSavedPublicAddr[2];
    ownPublicAddr[3] = ownSavedPublicAddr[3];
    ownPublicAddr[4] = ownSavedPublicAddr[4];
    ownPublicAddr[5] = ownSavedPublicAddr[5];
  }

  return( LL_STATUS_SUCCESS );
}


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This API is used to set this device's Sleep Clock Accuracy value.
 *
 * Note: For a peripheral device, this value is directly used, but only
 *       if power management is enabled. For a central device, this
 *       value is converted into one of eight ordinal values
 *       representing a SCA range, as specified in Table 2.2,
 *       Vol. 6, Part B, Section 2.3.3.1 of the Core specification.
 *
 * Note: This command is only allowed when the device is not in a connection.
 *
 * Note: The device's SCA value remains unaffected by a HCI_Reset.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_SetSCA( uint16 scaInPPM )
{
  // check if out of max value range
  if ( scaInPPM > 500 )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }
  else // input parameter okay
  {
    // check we are not in a connection
    if ( (llState == LL_STATE_CONN_CENTRAL) || (llState == LL_STATE_CONN_PERIPHERAL) )
    {
      return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
    }
    else // not in a connection, so okay to change setting
    {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
       extInitInfo_t *pInitInfo = extInitInfo;

      // find the ordinal value corresponding to SCA range
      if ( scaInPPM <= 20 )
      {
        pInitInfo->scaValue = 7;
      }
      else if ( (scaInPPM >= 21) && (scaInPPM <= 30) )
      {
        pInitInfo->scaValue = 6;
      }
      else if ( (scaInPPM >= 31) && (scaInPPM <= 50) )
      {
        pInitInfo->scaValue = 5;
      }
      else if ( (scaInPPM >= 51) && (scaInPPM <= 75) )
      {
        pInitInfo->scaValue = 4;
      }
      else if ( (scaInPPM >= 76) && (scaInPPM <= 100) )
      {
        pInitInfo->scaValue = 3;
      }
      else if ( (scaInPPM >= 101) && (scaInPPM <= 150) )
      {
        pInitInfo->scaValue = 2;
      }
      else if ( (scaInPPM >= 151) && (scaInPPM <= 250) )
      {
        pInitInfo->scaValue = 1;
      }
      else // if ( (scaValInPPM >= 251) && (scaValInPPM <= 500) )
      {
        pInitInfo->scaValue = 0;
      }
#endif // INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
    advSet_t *pAdvSet = advSetList;

    // update all Adv Sets
    while( pAdvSet )
    {
      pAdvSet->scaValue = scaInPPM;

      pAdvSet = pAdvSet->next;
    }

    // save the peripheral's SCA in PPM
    aePeripheralSCA = scaInPPM;
#endif // ADV_NCONN_CFG | ADV_CONN_CFG
    }
  }

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG


/*******************************************************************************
 * This API is used to set this device's frequency tuning up or down.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_SetFreqTune( uint8 step )
{
  return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
}


/*******************************************************************************
 * This API is used to save the current frequency tuning to flash.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_SaveFreqTune( void )
{
  return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
}

/*******************************************************************************
 * This API is used to set the max RF TX power to be used by Direct Test Mode.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_SetMaxDtmTxPower( uint8 txPowerIdx )
{
  return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
}

/*******************************************************************************
 * This API is used to set the max RF TX power (in dBm) to be used by Direct Test Mode.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_SetMaxDtmTxPowerDbm( int8 txPowerDbm, uint8 fraction )
{
  RFBLEDPL_TX_POWER_HW_TYPE txPower = RfBleDpl_getTxPowerByTxPowerDbm(txPowerDbm, fraction);

  if (!RfBleDpl_txPowerIsValid(txPower))
  {
    // bad parameter
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // save the max TX Power to use for DTM
  maxTxPwrForDTM = txPower;

  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * This API is used to configure and map a CC254x I/O Port as a General
 * Purpose I/O (GPIO) output signal that reflects the Power Management (PM)
 * state of the CC254x device. The GPIO output will be High on Wake, and Low
 * upon entering Sleep. This feature can be disabled by specifying
 * LL_EXT_PM_IO_PORT_NONE for the ioPort (ioPin is then ignored). The system
 * default value upon hardware reset is disabled.
 *
 * Note: Only Pins 0, 3 and 4 are valid for Port 2 since Pins 1 and 2 are mapped
 *       to debugger signals DD and DC.
 *
 * Note: Port/Pin signal change will only occur when Power Savings is enabled.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_MapPmIoPort( uint8 ioPort, uint8 ioPin )
{
  return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This API is called by the HCI to terminate a LL connection immediately.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_DisconnectImmed( uint16 connId )
{
  llStatus_t     status;
  llConnState_t  *connPtr;
  halIntState_t  cs;
  taskInfo_t     *llTask = MAP_llGetCurrentTask();

  // Make sure connection ID is valid.
  if ( (status = MAP_LL_ConnActive(connId)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // Get connection info.
  connPtr = MAP_llDataGetConnPtr( connId );

  // Sanity check.
  if ( (connPtr == NULL) || (llTask == NULL) )
  {
    return LL_STATUS_ERROR_INACTIVE_CONNECTION;
  }

  // Enter Critical Section.
  HAL_ENTER_CRITICAL_SECTION(cs);

  // Check if the current task is a Central or Central.
  // Check that the current connection id is the requested halted connection id.
  if ( ( llTask->taskID == LL_TASK_ID_CENTRAL ||
         llTask->taskID == LL_TASK_ID_PERIPHERAL) &&
        (connId == llConns.currentConn) )
  {
    // Turn on the terminate flag.
    // In the central/peripheral task end the terminate function would be called.
    connPtr->termInfo.termIndRcvd = TRUE;

    // Update the reason for termination.
    connPtr->termInfo.reason = LL_STATUS_ERROR_HOST_TERM;

    // Halt the radio.
    // Aborting will stop RF activities right away, while stopping
    // will let the RF command finish gently.
    // stop the command
    status = MAP_llHaltRadio( llTask->command );
    if ((status == (uint8)RCL_CommandStatus_DescheduledApi) ||
        (status == (uint8)RCL_CommandStatus_Scheduled))

    {
      // Call the Peripheral/Central task end in order to terminate the connection.
      if (llTask->taskID == LL_TASK_ID_PERIPHERAL)
      {
  #if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
        // Set taskendAction and activate it at the LL Post Process Case.
        (void)MAP_osal_set_event( LL_TaskID, LL_EVT_PERIPHERAL_LAST_CMD_DONE );
  #endif
      }
      else
      {
  #if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
        (void)MAP_osal_set_event( LL_TaskID, LL_EVT_CENTRAL_LAST_CMD_DONE );
  #endif
      }
    }
  }
  else
  {
    // Save the connection id to be disconnected immediately.
    disconnectImmedConnId = connId;

    // Handle the connection's disconnection.
    (void)MAP_osal_set_event( LL_TaskID, LL_EVT_CONN_DISCONNECTED_IMMED );

    // NOTE: Unable to use the function llConnTerminate() function
    // because this function calls the llScheduler(). This would cause a
    // race condition between the current scheduling and a new scheduling.
    // The llScheduler() needs to be called at the end of a scheduled task only.
  }

  // Exit Critical Section
  HAL_EXIT_CRITICAL_SECTION(cs);

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This API is called by the HCI to Reset or Read the Packet Error Rate counters
 * for a connection.
 *
 * Note: The counters are only 16 bits. At the shortest connection
 *       interval, this provides a bit over 8 minutes of data.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_PacketErrorRate( uint16 connId, uint8 command )
{
  llStatus_t     status;
  llConnState_t *connPtr;

#ifdef LL_TEST_MODE
  if ( llTestMode.testCase == LL_TEST_MODE_TP_TIM_SLA_BV_05 )
  {
    uint8 numTx      = numSets*10;
    uint8 failedSets = numFailedSets;
    uint8 sets       = numSets;
    MAP_LL_EXT_PacketErrorRateCback( numTx,
                                     0,
                                     sets,
                                     failedSets );

    return( LL_STATUS_SUCCESS );
  }
#endif // LL_TEST_MODE

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connId)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // get connection info
  connPtr = MAP_llDataGetConnPtr( connId );

  // check the operation
  switch( command )
  {
   case LL_EXT_PER_RESET:
     // clear the PER Test counters
      connPtr->perInfo.numPkts       = 0;
      connPtr->perInfo.numCrcErr     = 0;
      connPtr->perInfo.numEvents     = 0;
      connPtr->perInfo.numMissedEvts = 0;
      break;

    case LL_EXT_PER_READ:
      // generate a callback to HCI
      MAP_LL_EXT_PacketErrorRateCback( connPtr->perInfo.numPkts,
                                       connPtr->perInfo.numCrcErr,
                                       connPtr->perInfo.numEvents,
                                       connPtr->perInfo.numMissedEvts );
      break;

    default:
      return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This API is called by the HCI to start or end Packet Error Rate by Channel
 * counter accumulation for a connection.
 *
 * Note: The counters are only 16 bits. At the shortest connection
 *       interval, this provides a bit over 8 minutes of data.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_PERbyChan( uint16 connId, perByChan_t *perByChan )
{
  llStatus_t     status;
  llConnState_t *connPtr;

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connId)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // get connection info
  connPtr = MAP_llDataGetConnPtr( connId );

  // set pointer
  connPtr->perInfoByChan = perByChan;

  return( LL_STATUS_SUCCESS );

}
#endif // ADV_CONN_CFG | INIT_CFG

// ALT: Need to add BUILD_REVISION to ll_config table?
/*******************************************************************************
 * This API is used to to set a user revision number or read the build revision
 * number.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_BuildRevision( uint8 mode, uint16 userRevNum, uint8 *buildRev )
{
  switch( mode )
  {
    case LL_EXT_SET_USER_REVISION:
      buildInfo.userRevNum = userRevNum;
      break;

    case LL_EXT_READ_BUILD_REVISION:
      buildRev[0] = LO_UINT16( buildInfo.userRevNum );
      buildRev[1] = HI_UINT16( buildInfo.userRevNum );
      buildRev[2] = LO_UINT16( (uint16)BUILD_REVISION );
      buildRev[3] = HI_UINT16( (uint16)BUILD_REVISION );
      break;

    default:
      return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  return( LL_STATUS_SUCCESS );
}


/*******************************************************************************
 * This API is used to to issue a soft or hard system reset.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_ResetSystem( uint8 mode )
{
  switch( mode )
  {
    case LL_EXT_RESET_SYSTEM_HARD:
      MAP_osal_start_timerEx( LL_TaskID, LL_EVT_RESET_SYSTEM_HARD, LL_EXT_RESET_SYSTEM_DELAY );
      break;

    case LL_EXT_RESET_SYSTEM_SOFT:
      MAP_osal_start_timerEx( LL_TaskID, LL_EVT_RESET_SYSTEM_SOFT, LL_EXT_RESET_SYSTEM_DELAY );
      break;

    default:
      return( LL_STATUS_ERROR_BAD_PARAMETER );
      break;
  }

  return( LL_STATUS_SUCCESS );
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This API is used to set the minimum number of completed packets
 * which must be met before a Number of Completed Packets event is returned. If
 * the limit is not reach by the end of the connection event, then a Number of
 * Completed Packets event will be returned (if non-zero) based on the
 * flushOnEvt flag.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_NumComplPktsLimit( uint8 limit,
                                     uint8 flushOnEvt )
{
  if ( (limit == 0) || (limit > maxNumTxDataBufs) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  switch( flushOnEvt )
  {
    case LL_EXT_ENABLE_NUM_COMPL_PKTS_ON_EVENT:
      numComplPktsFlush = TRUE;
      break;

    case LL_EXT_DISABLE_NUM_COMPL_PKTS_ON_EVENT:
      numComplPktsFlush = FALSE;
      break;

    default:
      return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // save the limit
  numComplPktsLimit = limit;

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This API is used to get connection related information, which includes the
 * number of allocated connections, the number of active connections, and for
 * each active connection, the connection ID, the connection role (Central or
 * Peripheral), the peer address and peer address type. The number of allocated
 * connections is based on a default build value that can be changed using
 * MAX_NUM_BLE_CONNS. The number of active connections refers to active BLE
 * connections.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_GetConnInfo( uint8 *numAllocConns,
                               uint8 *numActiveConns,
                               uint8 *activeConnInfo )
{
  if ( numAllocConns != NULL )
  {
    // set the number of allocated connections
    *numAllocConns = maxNumConns;
  }

  if ( numActiveConns != NULL )
  {
    // set the number of active connections
    *numActiveConns = llConns.numActiveConns;
  }

  if ( activeConnInfo != NULL )
  {
    llConnState_t *connPtr;
    uint8          connId;
    uint8          j = 0;

    for (connId=0; connId<maxNumConns; connId++)
    {
      if ( MAP_LL_ConnActive(connId) == LL_STATUS_SUCCESS )
      {
        connPtr = MAP_llDataGetConnPtr( connId );

        // this device's connection handle and connection role
        activeConnInfo[j++] = connId;
        activeConnInfo[j++] = (connPtr->llTask->taskID == LL_TASK_ID_CENTRAL) ?
                              (HCI_EVT_CENTRAL_ROLE)                          :
                              (HCI_EVT_PERIPHERAL_ROLE);

        // peer device's address and address type
        activeConnInfo[j++] = connPtr->peerInfo.peerAddr[0];
        activeConnInfo[j++] = connPtr->peerInfo.peerAddr[1];
        activeConnInfo[j++] = connPtr->peerInfo.peerAddr[2];
        activeConnInfo[j++] = connPtr->peerInfo.peerAddr[3];
        activeConnInfo[j++] = connPtr->peerInfo.peerAddr[4];
        activeConnInfo[j++] = connPtr->peerInfo.peerAddr[5];
        activeConnInfo[j++] = connPtr->peerInfo.peerAddrType;
      }
    }
  }

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
/*******************************************************************************
 * This API is used to get connection related information required to follow the
 * target connection with a BLE connecton monitor.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_GetActiveConnInfo( uint8 connId, uint8 *pData  )
{
  hciActiveConnInfo_t *activeConnInfo = (hciActiveConnInfo_t *)pData;
  if ( connId < maxNumConns )
  {
    llConnState_t *connPtr = MAP_llDataGetConnPtr( connId );

    if ( connPtr->activeConn )
    {
      activeConnInfo->accessAddr   = connPtr->accessAddr;
      activeConnInfo->connInterval = connPtr->curParam.connInterval ;
      activeConnInfo->hopValue     = connPtr->hopLength;
      activeConnInfo->mSCA         = connPtr->mstSCA;
      activeConnInfo->nextChan     = connPtr->nextChan;
      activeConnInfo->ownAddrType  = connPtr->ownAddrType;

      //copy over channel map
      osal_memcpy( &activeConnInfo->chanMap[0],
                   &connPtr->curChanMap.chanMap[0],
                   LL_NUM_BYTES_FOR_CHAN_MAP );

      //copy over crcinit
      osal_memcpy((uint8 *)&activeConnInfo->crcInit,
                  &connPtr->crcInit,
                  BLE_CRC_LEN);
    }
    else
    {
      return( LL_STATUS_ERROR_INACTIVE_CONNECTION );
    }
  }
  else
  {
    return( LL_STATUS_ERROR_INVALID_PARAMS );
  }

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_CONN_CFG | INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 * This API is used to set the scan channels mapping
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_SetExtScanChannels( uint8 extScanChanMapVal )
{
  // Mask off unused scanning channels
  extScanChanMapVal &= LL_SCN_ADV_MAP_CHAN_ALL;

  // Make sure there's at least one scanning channel that can be used
  if ( extScanChanMapVal == 0 )
  {
    return( LL_STATUS_ERROR_INVALID_PARAMS );
  }

  // Update the scan channel mapping according to entered value.
  extScanChanMap = extScanChanMapVal;

  return( LL_STATUS_SUCCESS );
}
#endif // SCAN_CFG

/*******************************************************************************
 * This API is used to set the QOS Parameters
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_SetQOSParameters( uint8  taskType,
                                    uint8  paramType,
                                    uint32 paramVal,
                                    uint16 taskHandle)
{
  // Get the connection's info.
  llConnState_t    *connPtr = NULL;

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  // Get periodic adv info.
#ifdef USE_PERIODIC_ADV
  llPeriodicAdvSet_t *pPeriodicAdv = NULL;
#endif
  // Get advertise set info.
  advSet_t *pAdvSet = NULL;
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if (defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG))
  // Get periodic scan info.
#ifdef USE_PERIODIC_SCAN
  llPeriodicScanSet_t *pPeriodicScan = NULL;
#endif
#endif // SCAN_CFG

  // Check valid input of task type
  if (taskType >= LL_QOS_MAX_NUM_TASK_TYPE)
  {
    return LL_STATUS_ERROR_INVALID_PARAMS;
  }

  /***** Get Task's Info *****/
  /***************************/

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  /***** Connection Type *****/
  if (taskType == LL_QOS_CONN_TASK_TYPE)
  {
    // Get the connection's info.
    connPtr = MAP_llDataGetConnPtr( taskHandle );

    // Check this is a valid handle and param type.
    if (paramType <= LL_QOS_TYPE_CONN_MAX_LENGTH)
    {
      // Check Valid Input.
      if ((connPtr == NULL) || (!( connPtr->activeConn )))
      {
        return LL_STATUS_ERROR_INVALID_PARAMS;
      }
    }
  }// End if (taskType == LL_QOS_CONN_TASK_TYPE)
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  /***** Advertise Type *****/
  if (taskType == LL_QOS_ADV_TASK_TYPE)
  {
    // Get adv set info.
    pAdvSet = MAP_LL_SearchAdvSet( taskHandle );
    // Check valid input.
    if (pAdvSet == NULL)
    {
      return LL_STATUS_ERROR_INVALID_PARAMS;
    }
  }// End if (taskType == LL_QOS_ADV_TASK_TYPE)

  /***** Periodic Advertise Type *****/
#ifdef USE_PERIODIC_ADV
  else if (taskType == LL_QOS_PERIODIC_ADV_TASK_TYPE)
  {
    pPeriodicAdv = MAP_llGetPeriodicAdv( taskHandle );
    // Check Valid Input.
    if (pPeriodicAdv == NULL)
    {
      return LL_STATUS_ERROR_INVALID_PARAMS;
    }
  }// End if (taskType == LL_QOS_PERIODIC_ADV_TASK_TYPE)
#endif
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if (defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG))
  /***** Scanner Type *****/
  if (taskType == LL_QOS_SCN_TASK_TYPE)
  {
    // Check Valid Input.
    // There is only one scan set in the system using extScanInfo.
    // Therefore the taskHandle is not relevent for the scanner.
    if ((extScanInfo == NULL) || (extScanInfo->paramValid == FALSE))
    {
      return LL_STATUS_ERROR_INVALID_PARAMS;
    }
  }// End if (taskType == LL_QOS_SCN_TASK_TYPE)

 /***** Periodic Scan Type *****/
#ifdef USE_PERIODIC_SCAN
  else if (taskType == LL_QOS_PERIODIC_SCN_TASK_TYPE)
  {
    pPeriodicScan = MAP_llGetPeriodicScan( taskHandle );
    // Check Valid Input.
    if (pPeriodicScan == NULL)
    {
      return LL_STATUS_ERROR_INVALID_PARAMS;
    }
  }// End if (taskType == LL_QOS_PERIODIC_SCN_TASK_TYPE)
#endif
#endif // SCAN_CFG

#if (defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG))
  /***** Initiator Type *****/
  if (taskType == LL_QOS_INIT_TASK_TYPE)
  {
    // Check Valid Input.
    // There is only one init set in the system using extInitInfo.
    // Therefore the taskHandle is not relevent for the initiator.
    if (extInitInfo == NULL)
    {
      return LL_STATUS_ERROR_INVALID_PARAMS;
    }
  }// End if (taskType == LL_QOS_INIT_TASK_TYPE)
#endif // INIT_CFG

  /***** Perform Param Update *****/
  /********************************/
  // Go over the parameter type and handle it.
  switch (paramType)
  {
    // Case of QOS Type of Priority.
    case LL_QOS_TYPE_PRIORITY:
    {
      // Check Valid input.
      if (paramVal <= LL_QOS_HIGH_PRIORITY)
      {
        /***** Connection Type *****/
        // In case this is a connection task type.
        if (taskType == LL_QOS_CONN_TASK_TYPE && connPtr != NULL)
        {
          // Update the priority of the connection.
          connPtr->connPriority = paramVal;
        }

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
        /***** Advertise Type *****/
        // In case this is an advertise task type.
        if (taskType == LL_QOS_ADV_TASK_TYPE)
        {
          pAdvSet->priority = paramVal;
        }
        /***** Periodic Advertise Type *****/
        // In case this is a periodic advertise task type.
#ifdef USE_PERIODIC_ADV
        else if (taskType == LL_QOS_PERIODIC_ADV_TASK_TYPE)
        {
          pPeriodicAdv->priority = paramVal;
        }
#endif
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if (defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG))
        /***** Scanner Type *****/
        // In case this is a scanner task type.
        if (taskType == LL_QOS_SCN_TASK_TYPE)
        {
          extScanInfo->priority = paramVal;
        }
       /***** Periodic Scan Type *****/
        // In case this is a periodic scanner task type.
#ifdef USE_PERIODIC_SCAN
        else if (taskType == LL_QOS_PERIODIC_SCN_TASK_TYPE)
        {
          pPeriodicScan->priority = paramVal;
        }
#endif
#endif // SCAN_CFG

#if (defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG))
        /***** Initiator Type *****/
        // In case this is an initiator task type.
        if (taskType == LL_QOS_INIT_TASK_TYPE)
        {
          extInitInfo->priority = paramVal;
        }
#endif // INIT_CFG

      }// End of if (paramVal <= LL_QOS_HIGH_PRIORITY).
      // Invalid parameters
      else
      {
        return LL_STATUS_ERROR_INVALID_PARAMS;
      }
    }// End of LL_QOS_TYPE_PRIORITY.
    break;

    // Case of QOS Type of Connection's Min Length.
    case LL_QOS_TYPE_CONN_MIN_LENGTH:
    {
      // Make sure this is a connection task type
      if (taskType == LL_QOS_CONN_TASK_TYPE && connPtr != NULL)
      {
        // In case this is a coded phy -  the limit for the connection's time.
        if (connPtr->phyInfo.curPhy == LL_PHY_CODED)
        {
          // For coded the max connection's time is LL_MAX_LINK_DATA_TIME_CODED (17040) in us.
          // For coded the min connection's time is LL_MIN_LINK_DATA_TIME_CODED (2704) in us.
          if ((paramVal > LL_MAX_LINK_DATA_TIME_CODED) || (paramVal < LL_MIN_LINK_DATA_TIME_CODED))
          {
            return LL_STATUS_ERROR_INVALID_PARAMS;
          }
        }
        // In case this is not a coded phy - the limit for the connection's time.
        else
        {
          // For uncoded the max connection's time is LL_MAX_LINK_DATA_TIME_UNCODED (2120) in us.
          // For uncoded the min connection's time is LL_MIN_LINK_DATA_TIME (328) in us.
          if ((paramVal > LL_MAX_LINK_DATA_TIME_UNCODED) || (paramVal < LL_MIN_LINK_DATA_TIME))
          {
            return LL_STATUS_ERROR_INVALID_PARAMS;
          }
        }

        // Mark that this parameter was set externally, so it would not be overrun later on.
        connPtr->connMinTimeExternalUpdateInd = TRUE;
        // Update the Connection's Min Length (only 31 bits for connMinTimeLength)
        // Add LL_TOTAL_MARGIN_TIME_FOR_MIN_CONN_RAT_TICKS margin to the minimum time
        connPtr->connMinTimeLength = ( (US_TO_RAT_TICKS(paramVal) + LL_TOTAL_MARGIN_TIME_FOR_MIN_CONN_RAT_TICKS) & LL_MIN_MAX_CONN_TIME_LENGTH_MASK );
      }// End of if (taskType == LL_QOS_CONN_TASK_TYPE).
      else
      {
        return LL_STATUS_ERROR_INVALID_PARAMS;
      }
    }
    break;

    // Case of QOS Type of Connection's Max Length.
    case LL_QOS_TYPE_CONN_MAX_LENGTH:
    {
      // Make sure this is a connection task type
      if (taskType == LL_QOS_CONN_TASK_TYPE && connPtr != NULL)
      {
        // In case this is a coded phy -  the min value for the max connection's time.
        if (connPtr->phyInfo.curPhy == LL_PHY_CODED)
        {
          // For coded the min connection's time is LL_MIN_LINK_DATA_TIME_CODED (2704) in us.
          // So the max value should be higher than that.
          if (paramVal < LL_MIN_LINK_DATA_TIME_CODED)
          {
            return LL_STATUS_ERROR_INVALID_PARAMS;
          }
        }
        // In case this is not a coded phy - the min value for the max connection's time.
        else
        {
          // For uncoded the min connection's time is LL_MIN_LINK_DATA_TIME (328) in us.
          // So the max value should be higher than that.
          if (paramVal < LL_MIN_LINK_DATA_TIME)
          {
            return LL_STATUS_ERROR_INVALID_PARAMS;
          }
        }

        // Check valid input before update.
        // The input max time should not be larger than the connection's interval.
        // The input max time has to be larger than the minimum time
        if ( ((connPtr->curParam.connInterval*RAT_TICKS_IN_625US) >= US_TO_RAT_TICKS(paramVal)) &&
             (US_TO_RAT_TICKS(paramVal) >= connPtr->connMinTimeLength) )
        {
          // Mark that this parameter was set externally, so it would not be overrun later on.
          connPtr->connMaxTimeExternalUpdateInd = TRUE;
          // Update the Connection's Max Length in RAT TICKS.
          connPtr->connMaxTimeLength = US_TO_RAT_TICKS(paramVal);
        }
        // Invalid parameters
        else
        {
          return LL_STATUS_ERROR_INVALID_PARAMS;
        }
      }// End of if (taskType == LL_QOS_CONN_TASK_TYPE).
      else
      {
        return LL_STATUS_ERROR_INVALID_PARAMS;
      }
    }
    break;

    // Default case
    default:
    {
        // Invalid input.
        return LL_STATUS_ERROR_INVALID_PARAMS;
    }

    break;

  }// End of Switch.

  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * This API is used to set the QOS Default Parameters Values.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_SetQOSDefaultParameters(uint32 paramDefaultVal,
                                          uint8  paramType,
                                          uint8  taskType)
{
  /***** LL_QOS_TYPE_PRIORITY *****/
  /********************************/
  if (paramType == LL_QOS_TYPE_PRIORITY)
  {
    // Check Valid input.
    if ( paramDefaultVal > LL_QOS_HIGH_PRIORITY ) // Priority must be lower then LL_QOS_HIGH_PRIORITY
    {
      return (LL_STATUS_ERROR_INVALID_PARAMS);
    }

    /***** Connection Type *****/
    if (taskType == LL_QOS_CONN_TASK_TYPE)
    {
      // Update the default priority of the connection.
      qosDefaultPriorityConnParameter      = paramDefaultVal;
    }
    /***** Advertise Type *****/
    else if (taskType == LL_QOS_ADV_TASK_TYPE)
    {
      // Update the default priority of advertise.
      qosDefaultPriorityAdvParameter      = paramDefaultVal;
    }
    /***** Scanner Type *****/
    else if (taskType == LL_QOS_SCN_TASK_TYPE)
    {
      // Update the default priority of scan.
      qosDefaultPriorityScnParameter      = paramDefaultVal;
    }
    /***** Initiator Type *****/
    else if (taskType == LL_QOS_INIT_TASK_TYPE)
    {
      // Update the default priority of Initiator.
      qosDefaultPriorityInitParameter      = paramDefaultVal;
    }
    /***** Periodic Advertise Type *****/
    else if (taskType == LL_QOS_PERIODIC_ADV_TASK_TYPE)
    {
      // Update the default priority of periodic advertise.
      qosDefaultPriorityPerAdvParameter      = paramDefaultVal;
    }
    /***** Periodic Scan Type *****/
    else if (taskType == LL_QOS_PERIODIC_SCN_TASK_TYPE)
    {
      // Update the default priority of periodic scan.
      qosDefaultPriorityPerScnParameter      = paramDefaultVal;
    }
    /***** Invalid Input *****/
    else
    {
      return (LL_STATUS_ERROR_INVALID_PARAMS);
    }
  }// End of if (paramType == LL_QOS_TYPE_PRIORITY).
  /***** Invalid Input *****/
  else
  {
    return (LL_STATUS_ERROR_INVALID_PARAMS);
  }

  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * This API is used to set the DTM TX packet count.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_SetDtmTxPktCnt( uint16 txPktCnt )
{
  // save the desired number of DTM packets to transmit
  dtmInfo->txPktCnt = txPktCnt;

  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * This API is used to read the the controller's own random device address.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_ReadRandomAddress( uint8 *bdAddr )
{
  // return own public device address LSO..MSO
  LL_COPY_DEV_ADDR_LE( bdAddr, ownRandomAddr );

  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * This API is used to set the advertiser's virtual public address.
 *
 * Note: This command is only allowed when the advertise set is not active,
 *       and its PDU type is Legacy Non-Connectable and Non-Scanable.
 *
 * Public function defined in ll.h.
 */

llStatus_t LL_EXT_SetVirtualAdvAddr( uint8 advHandle , uint8 *bdAddr )
{
  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( advHandle );

  if ( bdAddr == NULL )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check if we have an Adv Set
  if ( pAdvSet == NULL )
  {
    // handle is not found
    return( LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER );
  }

  // check if Adv Set is connectable and enabled
  if ( !(pAdvSet->advEvtType == LL_ADV_NONCONNECTABLE_UNDIRECTED_EVT) ||
       (pAdvSet->advMode == LL_ADV_MODE_ON))
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }
  // set own address type to be public
  pAdvSet->ownAddrType = LL_DEV_ADDR_TYPE_PUBLIC;

  // copy the virtual address in to the relevant adv set
  LL_COPY_DEV_ADDR_LE( pAdvSet->ownAddr, bdAddr );

  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * This API is called by the HCI to Reset or Read the RX Statistics counters
 * for a connection.
 *
 * Note: The counters are only 16 bits. At the shortest connection
 *       interval, this provides a bit over 8 minutes of data.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_GetRxStats( uint16 connId, uint8 command )
{
  llStatus_t     status;
  llConnState_t *connPtr;

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connId)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // get connection info
  connPtr = MAP_llDataGetConnPtr( connId );

  // check the operation
  switch( command )
  {
    case LL_EXT_STATS_RESET:
    {
      // clear the statistics counters
      connPtr->rxStats.numRxOk         = 0;
      connPtr->rxStats.numRxCtrl       = 0;
      connPtr->rxStats.numRxCtrlAck    = 0;
      connPtr->rxStats.numRxCrcErr     = 0;
      connPtr->rxStats.numRxIgnored    = 0;
      connPtr->rxStats.numRxEmpty      = 0;
      connPtr->rxStats.numRxBufFull    = 0;
      break;
    }

    case LL_EXT_STATS_READ:
    {
      // generate a callback to HCI
      MAP_LL_EXT_GetRxStatsCback( connPtr->rxStats.numRxOk,
                                  connPtr->rxStats.numRxCtrl,
                                  connPtr->rxStats.numRxCtrlAck,
                                  connPtr->rxStats.numRxCrcErr,
                                  connPtr->rxStats.numRxIgnored,
                                  connPtr->rxStats.numRxEmpty,
                                  connPtr->rxStats.numRxBufFull );
      break;
    }

    default:
      return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * This API is called by the HCI to Reset or Read the TX Statistics counters
 * for a connection.
 *
 * Note: The counters are only 16 bits. At the shortest connection
 *       interval, this provides a bit over 8 minutes of data.
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_EXT_GetTxStats( uint16 connId, uint8 command )
{
  llStatus_t     status;
  llConnState_t *connPtr;

  // make sure connection ID is valid
  if ( (status=MAP_LL_ConnActive(connId)) != LL_STATUS_SUCCESS )
  {
    return( status );
  }

  // get connection info
  connPtr = MAP_llDataGetConnPtr( connId );

  // check the operation
  switch( command )
  {
    case LL_EXT_STATS_RESET:
    {
      // clear the statistics counters
      connPtr->txStats.numTx           = 0;
      connPtr->txStats.numTxAck        = 0;
      connPtr->txStats.numTxCtrl       = 0;
      connPtr->txStats.numTxCtrlAck    = 0;
      connPtr->txStats.numTxCtrlAckAck = 0;
      connPtr->txStats.numTxRetrans    = 0;
      connPtr->txStats.numTxEntryDone  = 0;
      break;
    }

    case LL_EXT_STATS_READ:
    {
      // generate a callback to HCI
      MAP_LL_EXT_GetTxStatsCback( connPtr->txStats.numTx,
                                  connPtr->txStats.numTxAck,
                                  connPtr->txStats.numTxCtrl,
                                  connPtr->txStats.numTxCtrlAck,
                                  connPtr->txStats.numTxCtrlAckAck,
                                  connPtr->txStats.numTxRetrans,
                                  connPtr->txStats.numTxEntryDone );
      break;
    }

    default:
      return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * This API is used to enable/disable a feature in the Host feature set
 *
 * Public function defined in ll.h.
 */
llStatus_t LL_SetHostFeature( uint8 bitNumber,
                              uint8 bitValue )
{
  uint8 controllerBitNunmber;
  uint8 *pFeatureSet;
  uint8 bitMask;

  if( bitValue > 1 || bitNumber > 0x3F  )
  {
      return( LL_STATUS_ERROR_BAD_PARAMETER ); // not in the spec
  }

  // If bitNumber specifies a feature bit that is not controlled by the Host -> LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED
  // LL_FEATURE_CONNECTION_SUBRAING_HOST_SUPPORT 32 not supported
  // LL_FEATURE_CONNECTED_ISOCHROOUS_STREAM_HOST 38 not supported
  // LL_FEATURE_CS_HOST in bit position 47 - still temporary in the TS spec
  if( bitNumber != 47 )
  {
    return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
  }

  // If bitValue is set to 0x01 and bitNumber specifies a feature bit that requires support of a feature
  // that the Controller does not support ->LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED
  if(bitValue == 1)
  {
    controllerBitNunmber = bitNumber -1;
    pFeatureSet = &deviceFeatureSet.featureSet[controllerBitNunmber / 8] ;
    bitMask = 1 << (controllerBitNunmber % 8);

    if((*pFeatureSet & bitMask) == 0 )
    {
      return( LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED );
    }
  }

  // If there is an active connections -> LL_STATUS_ERROR_COMMAND_DISALLOWED
  if ( !llConns.numActiveConns )
  {
    pFeatureSet = &deviceFeatureSet.featureSet[bitNumber / 8] ;
    // Create mask for the relevant bit
    bitMask = 1 << (bitNumber % 8);
    // Add the bitValue to the feature position in the feature set
    // (*pFeatureSet & ~bitMask) take the value of the feature set with zero in the relevant position
    *pFeatureSet = (bitValue << (bitNumber % 8) | (*pFeatureSet & ~bitMask));
  }
  else
  {
    return( LL_STATUS_ERROR_COMMAND_DISALLOWED );
  }

  return( LL_STATUS_SUCCESS );
}
/*******************************************************************************
 */
