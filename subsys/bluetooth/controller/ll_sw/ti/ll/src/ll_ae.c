/******************************************************************************

 @file  ll_ae.c

 @brief This file contains the Link Layer (LL) Advertising Extension for the
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
#include "osal_memory.h"
#include "osal_timers.h"
#include "bcomdef.h"
#ifdef USE_RCL
#include <ti/drivers/rcl/RCL.h>
#include <ti/drivers/rcl/commands/ble5.h>
#else
#include <ti/drivers/rf/RF.h>
#include "rf_api.h"
#endif
#include "hal_mcu.h"
#include "hci_event.h"
#include "../../ll/inc/ll_common.h"
#include "../../ll/inc/ll_enc.h"
#include "../../ll/inc/ll_privacy.h"
#include "../../ll/inc/ll_rat.h"
#include "../../ll/inc/ll_timer_drift.h"
#include "../../ll/inc/ll_ae.h"
#include "hal_gpio_wrapper.h"
//
#include "rom_jt.h"

/*******************************************************************************
 * EXTERNS
 */
#ifdef USE_RCL
extern void LL_rclAdvCallback(RCL_Command *cmd, LRF_Events lrfEvents, RCL_Events events);
extern void LL_rclScanCallback(RCL_Command *cmd, LRF_Events lrfEvents, RCL_Events events);
extern void LL_rclInitCallback(RCL_Command *cmd, LRF_Events lrfEvents, RCL_Events events);
extern void LL_rclRescheduleCommand(RCL_Command *cmd);
#endif

/*******************************************************************************
 * CONSTANTS
 */

// set Entry is Valid bit - Ignore Packets with same DID for a given SID
#define EXT_SCAN_ADI_INIT                                   0x0000
#define EXT_SCAN_FILTER_ADI                                 0x1000

// no ADI in packet
#define EXT_SCAN_NO_ADI                                     0xFF
#define EXT_SCAN_NO_DID                                     0xFFFF

// Max adv sets to store their scan report state in parallel
#define EXT_SCAN_STATE_LIST_MAX_ENTRIES                     10

// Add extra time over the time expected for AUX packet (5 msec)
#define EXT_SCAN_STATE_RCV_AUX_GUARD_TIME                   5000

// keep scanning max every 1 second when exist periodic scan
#define EXT_SCAN_KEEP_SCHEDULE_MAX_TIME                     1000
// calculate the max number of scans which possible to missed
// every EXT_SCAN_KEEP_SCHEDULE_MAX_TIME (1 sec)
#define EXT_SCAN_GET_MAX_NUMBER_POSSIBLE_MISSED(interval)  ((EXT_SCAN_KEEP_SCHEDULE_MAX_TIME * 1000) / (interval * 625))

#define PERIODIC_SCAN_REPORT_DATA_COMPLETE                  0
#define PERIODIC_SCAN_REPORT_DATA_INCOMPLETE_MORE           1
#define PERIODIC_SCAN_REPORT_DATA_INCOMPLETE_TRUNCATED      2

#define PERIODIC_SCAN_HIGH_PRIO_PERCENT                     80    // 80% of events number before timeout
#define PERIODIC_SCAN_MISSED_LIMIT(timeout,interval)       ((timeout * 10000) / (interval * 1250) * PERIODIC_SCAN_HIGH_PRIO_PERCENT / 100)
#define PERIODIC_SCAN_TIMESTAMP_DRIFT_MIN_GLICH            (RAT_TICKS_IN_100US)
#define PERIODIC_SCAN_DRIFT_LEARNING_THRESHOLD             (RAT_TICKS_IN_10US)
#define PERIODIC_SCAN_DRIFT_DIRECTION_POSITIVE              (1)
#define PERIODIC_SCAN_DRIFT_DIRECTION_NEGATIVE              (-1)
#define PERIODIC_SCAN_DRIFT_1SEC_MAX_TICKS                  (100) // estimate the max ticks drift in 1 second
#define PERIODIC_SCAN_DRIFT_1SEC_RATIO                      (1000000/PERIODIC_SCAN_DRIFT_1SEC_MAX_TICKS)
#define PERIODIC_SCAN_DRIFT_GET_MAX_GLICH(skip,interval)    ((skip + 1)*(interval * 1250)/(PERIODIC_SCAN_DRIFT_1SEC_RATIO))

//convert adv high duty cycle duration in msec to number of events by divide duration by 3.75msec
#define ADV_CONVERT_HDC_DURATION_TO_NUM_EVENTS(duration)    ((duration * 4 / 15) + 1)

/*******************************************************************************
 * TYPEDEFS
 */

// RF Command Preemption
typedef struct
{
  uint32 startTime;
  uint8 enable;
} llRfCmdPreemption_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */

// ALT: Add these to ll_config/user config.
uint16 maxExtAdvDataLen    = AE_DEFAULT_ADV_DATA_LEN; // 31..1650
uint8  maxSupportedAdvSets = AE_DEFAULT_NUM_ADV_SETS; // 1..240
#ifdef QUAL_TEST
// indication for data was updated during advertising
// represent the adv handle which the data was updated by Host
// default value - EXT_DATA_NO_UPDATE_DURING_ADV (0xFF)
uint8  aeDataUpdatedDuringAdv = EXT_DATA_NO_UPDATE_DURING_ADV;
#endif

#ifdef USE_AE //scanner
// Scan report state
extScanReportState_t extScanReportState[EXT_SCAN_STATE_LIST_MAX_ENTRIES];
// Scan Report last received SID
uint8  lastScanAdvSid;
#endif
// RF Command Preemption
llRfCmdPreemption_t llRfCmdPreemption;

/*******************************************************************************
 * GLOBAL VARIABLES
 */

// Callback Table
aeCBackTbl_t aeCBackTbl;

//
// Extended Advertiser
//
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
// Advertising Set List
advSet_t *advSetList = NULL;

// Current Adv Set Handle
uint8 aeCurHandle;

// Current Adv Set Enable Handle
uint8 aeCurAdvEnableHandle;

// Current Adv Set Handle When a Connection is Formed
uint8 aeCurConnHandle;

// Pointer which always points to the next scheduled Adv set
sortedAdv_t *pNextAdvSet;

// Number of enabled adv sets
uint8 numActiveAdvSets;

// Adv Set Peripheral SCA
uint8 aePeripheralSCA = LL_SCA_PERIPHERAL_DEFAULT;
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

//
// Extended Scanner
//
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
extScanInfo_t  *extScanInfo = NULL;
uint8           extScanIndex;
uint16          extScanNumMissed = 0;
uint8           extScanPriority = 0;
//
#ifdef USE_RCL
RCL_CmdBle5Scanner   extScanCmd;
RCL_CtxScanInit      extScanParam;
RCL_StatsAdvScanInit extScanOutput;
extern List_List     scanDataQueue;
#else
ble5OpCmd_t     extScanCmd;
adiList_t       adiList[AE_MAX_NUM_SID];
extScanParam_t  extScanParam;
extScanOut_t    extScanOutput;
#endif
// Extended Scan Report State Variables
uint8 scanState   = WAIT_FOR_ADV_EXT_IND;
//
// Define the scan channel mapping
// default mapping would be all channels (37,38,39).
uint8 extScanChanMap = LL_SCN_ADV_MAP_CHAN_ALL;
#endif // SCAN_CFG

//
// Extended Initiator
//
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
extInitInfo_t  *extInitInfo = NULL;
uint8           extInitIndex;
//
#ifdef USE_RCL
RCL_CmdBle5Initiator  extInitCmd;
RCL_CtxScanInit       extInitParam;
RCL_StatsAdvScanInit  extInitOutput;
#ifdef RCL_329
RCL_FilterList        peerAddrInitCmd ALIGNED;       // RCL Accept List table - used when connecting to RPA with LL_INIT_AL_POLICY_USE_PEER_ADDR as filter policy
#endif // RCL_329
#else
ble5OpCmd_t     extInitCmd;
extInitParam_t  extInitParam;
extScanOut_t    extInitOutput;
#endif
#endif // INIT_CFG

#ifndef QUAL_TEST
uint8 lastPrimPhy = AE_AUX_1M_PHY;
uint8 lastScanRsp = FALSE;
#endif


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
extern uint8 secondaryAdvChannelMap[LL_NUM_BYTES_FOR_CHAN_MAP];
extern uint8 secondaryAdvChannelMapPopCount;
#endif

#ifdef USE_AE // scanner
extScanReportState_t *llManageExtScanStateList(uint8 action, uint8 sid, uint32 timeOffset);
#endif

#ifdef USE_PERIODIC_ADV
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
// instance for periodic advertiser and periodic advertising scanner
llPeriodicAdv_t llPeriodicAdv;
#endif
#endif // USE_PERIODIC_ADV

#ifdef USE_PERIODIC_SCAN
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
llPeriodicScan_t llPeriodicScan;
#endif
#endif // USE_PERIODIC_SCAN

/*******************************************************************************
 * API
 */
#ifdef USE_PERIODIC_ADV
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llGetPeriodicAdv
 *
 * @brief       This function is used to get the periodic adv by handle
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       handle - periodic advertiser handle.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      pPeriodicAdv - A pointer to periodic advertiser.
 */
llPeriodicAdvSet_t *llGetPeriodicAdv( uint8 handle )
{
  llPeriodicAdvSet_t *pPeriodicAdv = llPeriodicAdv.advList;

  // search for handle
  while( pPeriodicAdv != NULL )
  {
    if ( handle == pPeriodicAdv->handle )
    {
      return( pPeriodicAdv );
    }

    // advance to next entry
    pPeriodicAdv = pPeriodicAdv->next;
  }
  return NULL;
}

/*******************************************************************************
 * @fn          llGetCurrentPeriodicAdv
 *
 * @brief       This function is used to get the current periodic Adv
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      pPeriodicScan - A pointer to syncing periodic advertiser.
 */
llPeriodicAdvSet_t *llGetCurrentPeriodicAdv( void )
{
  return (llPeriodicAdv.currentAdv);
}

/*******************************************************************************
 * @fn          llSetPeriodicAdvChmapUpdate
 *
 * @brief       This function is used to set the periodic advertiser
 *              channel map update procedure
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       set - TRUE - Host was updated the channel map,
 *                    FALSE - all periodics moved to new channel map
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetPeriodicAdvChmapUpdate( uint8 set )
{
  // check channel map update in progress
  if ((set == TRUE) && (llPeriodicAdv.chanMap.updated == TRUE))
  {
    return;
  }
  if (set == TRUE)
  {
    // update the new channel map
    llSetPeriodicChanMap(&llPeriodicAdv.chanMap.next, secondaryAdvChannelMap);
  }
  llPeriodicAdv.chanMap.updated = set;
}

/*******************************************************************************
 * @fn          llCalcPeriodicAdvOtaTime
 *
 * @brief       This function is used to estimate the periodic adv OTA time
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       dataLen - periodic advertiser data length.
 * @param       maxAvailData - periodic advertiser max data length in one chunk.
 * @param       phy - periodic advertiser phy.
 * @param       cteLen - CTE length (2 - 20).
 * @param       cteCount - CTE Count (1 - 16).
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      OTA time in usec.
 */
uint32 llEstimatePeriodicAdvOtaTime(uint16 dataLen, uint8 maxAvailData, uint8 phy, uint8 cteLen, uint8 cteCount)
{
  uint32 otaTime = 0;
  // calculate the number of fragments needed
  uint8 numFrags = (dataLen > 0)?((dataLen / maxAvailData)  + ((dataLen % maxAvailData) ? 1 : 0)):0;
  // find the fragment length include the t_mafs and max header available
  uint8 fragLen = (numFrags > 0)?(((numFrags == 1) ? dataLen : maxAvailData)):0;
  // find the last fragment length with max header available
  uint8 lastFragLen = dataLen - ((numFrags-1)*fragLen);
  uint8 numChains = MAX(numFrags,(cteLen > 0)? cteCount:0);

  // check periodic adv data
  if ((numFrags > 0) && (fragLen > 0))
  {
    // set max ota of the first chain packet
    otaTime = MAP_llOctets2Time(phy - 1, ((phy - 1)>>2), fragLen +
                               (EXTHDR_FLAG_CTEINFO_SIZE + EXTHDR_FLAG_TXPWR_SIZE + EXTHDR_FLAG_AUXPTR_SIZE   +
                                EXTHDR_INFO_SIZE + EXTHDR_FLAGS_SIZE), MIC_NOT_ENABLED);
  }
  // check CTE count and CTE length
  if ((cteCount > 0) && (cteLen > 0))
  {
    // add all CTE length in usec
    otaTime += (cteCount * cteLen * 8);
  }
  if (numFrags > 1)
  {
    // set max ota of the rest chain packets
    otaTime += ((numFrags - 2) *
               (MAP_llOctets2Time(phy - 1, ((phy - 1)>>2), fragLen +
                                (EXTHDR_FLAG_CTEINFO_SIZE + EXTHDR_FLAG_AUXPTR_SIZE +
                                 EXTHDR_INFO_SIZE + EXTHDR_FLAGS_SIZE), MIC_NOT_ENABLED))) +
               MAP_llOctets2Time(phy - 1, ((phy - 1)>>2), lastFragLen +
                                (EXTHDR_FLAG_CTEINFO_SIZE +
                                 EXTHDR_INFO_SIZE + EXTHDR_FLAGS_SIZE), MIC_NOT_ENABLED);
  }
  if (numChains > 1)
  {
    if (numChains > numFrags)
    {
      // add the header size in case chain includes only CTE
      otaTime += ((numChains - numFrags) * (MAP_llOctets2Time(phy - 1, ((phy - 1)>>2),
                  (EXTHDR_FLAG_CTEINFO_SIZE + EXTHDR_FLAG_AUXPTR_SIZE +
                   EXTHDR_INFO_SIZE + EXTHDR_FLAGS_SIZE), MIC_NOT_ENABLED)));
    }
    // add all gap time for the chain packets
    otaTime += ((numChains - 1) * AE_MIN_T_MAFS_IN_US);
  }
  return (otaTime);
}
/*******************************************************************************
 * @fn          llSetPeriodicAdvData
 *
 * @brief       This function is used to update the periodic adv data
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       pPeriodicAdv - A pointer to periodic advertiser.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetPeriodicAdvData( llPeriodicAdvSet_t *pPeriodicAdv )
{
  if (pPeriodicAdv->dataUpdated)
  {
    pPeriodicAdv->dataUpdated = FALSE;
    // free previously allocated data
    if (pPeriodicAdv->pData != NULL)
    {
      MAP_osal_mem_free(pPeriodicAdv->pData);
      pPeriodicAdv->pData = NULL;
      pPeriodicAdv->dataLen = 0;
    }
    if (pPeriodicAdv->dataCmd.dataLen > 0)
    {
      pPeriodicAdv->pData = pPeriodicAdv->dataCmd.pData;
      pPeriodicAdv->dataLen = pPeriodicAdv->dataCmd.dataLen;
      pPeriodicAdv->dataCmd.pData = NULL;
      pPeriodicAdv->dataCmd.dataLen = 0;
      pPeriodicAdv->dataCmd.operation = AE_DATA_OP_NO_DATA;
    }
    else
    {
      pPeriodicAdv->dataLen = 0;
    }
  }
  // calculate the number of fragments needed
  pPeriodicAdv->numFrags = (pPeriodicAdv->dataLen == 0)?1:(pPeriodicAdv->dataLen / pPeriodicAdv->maxAvailData)  +
                          ((pPeriodicAdv->dataLen % pPeriodicAdv->maxAvailData) ? 1 : 0);
  // find the fragment length
  pPeriodicAdv->fragLen = (pPeriodicAdv->numFrags == 1) ?
                           pPeriodicAdv->dataLen        :
                           pPeriodicAdv->maxAvailData;

  // find the last fragment length
  pPeriodicAdv->lastFragLen = (pPeriodicAdv->numFrags == 1) ?
                               pPeriodicAdv->fragLen        :
                              (pPeriodicAdv->dataLen -
                             ((pPeriodicAdv->numFrags-1)*pPeriodicAdv->fragLen));
}

/*******************************************************************************
 * @fn          llClearPeriodicAdvSets
 *
 * @brief       This function is used to clear all periodic adv sets
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       void.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llClearPeriodicAdvSets(void)
{
  llPeriodicAdvSet_t *pPeriodicAdv = NULL;

  if (llPeriodicAdv.advList == NULL)
  {
    return;
  }
  if (MAP_llGetCurrentTask() == llPeriodicAdv.llTask)
  {
    // halt the radio
    MAP_llHaltRadio( CMD_ABORT );
  }
  // release ll task
  if (llPeriodicAdv.llTask != NULL)
  {
    MAP_llFreeTask( &llPeriodicAdv.llTask );
  }
  // release all sets
  while( llPeriodicAdv.advList != NULL )
  {
    pPeriodicAdv = llPeriodicAdv.advList->next;
    // release periodic data
    if (llPeriodicAdv.advList->dataCmd.pData != NULL)
    {
      MAP_osal_mem_free(llPeriodicAdv.advList->dataCmd.pData);
    }
    if (llPeriodicAdv.advList->pData != NULL)
    {
      MAP_osal_mem_free(llPeriodicAdv.advList->pData);
    }
    // delete the set
    MAP_osal_mem_free(llPeriodicAdv.advList);
    // advance to next set
    llPeriodicAdv.advList = pPeriodicAdv;
  }
  llPeriodicAdv.advNumActive = 0;
  llPeriodicAdv.currentAdv = NULL;
  llPeriodicAdv.advList = NULL;
}

#endif
#endif

#if defined (USE_PERIODIC_ADV) || (USE_PERIODIC_SCAN)
/*******************************************************************************
 * @fn          llSetPeriodicChanMap
 *
 * @brief       This function is used to convert a channel map in bit map
 *              format of used data channels to a table of consecutively
 *              ordered entries of used data channels. This should be done
 *              whenever the data channel map is updated.
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetPeriodicChanMap( llPeriodicChanMap_t *pChanMap, uint8 *pSrcMap )
{
  uint8 i, j;

  pChanMap->numUsedChans = 0;
  // copy the channel map
  MAP_osal_memcpy(pChanMap->bitmap,pSrcMap,LL_NUM_BYTES_FOR_CHAN_MAP);
  // clear channels 37,38,39
  pChanMap->bitmap[LL_NUM_BYTES_FOR_CHAN_MAP-1] &= 0x1F;

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
      if ( (pChanMap->bitmap[i] >> j) & 1 )
      {
        // count it
        pChanMap->numUsedChans++;
      }
    }
  }
}
#endif

/*******************************************************************************
 * @fn          llGetSecondaryTaskEndTime
 *
 * @brief       This function is used to calculate a certain secondary task
 *              end time, depending on the primary's task status:
 *              - LSTO.
 *              - Length of secondary task compared to the missed event of
 *                of the connection.
 *
 *
 * input parameters
 *
 * @param       secTask         - Pointer to the secondary task information structure.
 * @param       secTaskLength   - Secondary Task Length.
 * @param       connPtr         - Pointer to the connection info.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      Secondary Task's End Time [RAT TICKS]
 */
uint32 llGetSecondaryTaskEndTime( taskInfo_t    *secTask,
                                  uint32        secTaskLength,
                                  llConnState_t *connPtr)
{
  uint32 secTaskEndTimeCalc = 0;
  taskInfo_t    *connTask = NULL;

  // Check Valid input.
  if ( (secTask == NULL) || (connPtr == NULL) )
  {
    return (secTaskEndTimeCalc);
  }

  connTask = connPtr->llTask;
#ifdef USE_RCL
  uint32 connStartTime = ((RCL_Command *)connTask->command)->timing.absStartTime;
#else
  uint32 connStartTime = ((ble5OpCmd_t *)connTask->command)->rfOpCmd.startTime;
#endif

  /*********** Calculate the Secondary Task End Time ***********/

  // In case the secondary task has lower priority than the primary task.
  // In case secondary task has higher priority than the primary task, but the length of the
  // secondary task runs over the connection entirely --> Set the end time to be the next's connection event.
  if (   (llCompareSecondaryPrimaryTasksQoSParam(LL_QOS_TYPE_PRIORITY, secTask, connPtr) == FALSE) ||
         ( (llCompareSecondaryPrimaryTasksQoSParam(LL_QOS_TYPE_PRIORITY, secTask, connPtr) == TRUE) &&
           ( (connPtr->numEventsLeft <= connPtr->expirationValue / 2) ||
             (connPtr->numEventsLeft <= ((secTaskLength) / connPtr->curParam.connInterval)) ) ))
  {
    secTaskEndTimeCalc = connStartTime - LL_SCHED_POST_CUTOFF;
  }

  // In case secondary task has higher priority than the primary task, but the length of the
  // secondary task will cause the connection to reach it's LSTO timeout --> Set the end time to be the LSTO timeout.
  else if ( (llCompareSecondaryPrimaryTasksQoSParam(LL_QOS_TYPE_PRIORITY, secTask, connPtr) == TRUE) &&
            ( (connPtr->numEventsLeft > ((secTaskLength) / connPtr->curParam.connInterval)) &&
              (connPtr->numEventsLeft - ((secTaskLength) / connPtr->curParam.connInterval)) < (connPtr->expirationValue / 2) ) )
  {
    secTaskEndTimeCalc = (connPtr->numEventsLeft - (connPtr->expirationValue / 2)) * (connPtr->curParam.connInterval * RAT_TICKS_IN_625US) + connStartTime - LL_SCHED_POST_CUTOFF;
  }

  // Return the end time
  // Note: The secTaskEndTimeCalc might be zero in case the conditions above are not met.
  // When secTaskEndTimeCalc is zero the entire secTaskLength is executed.
  return (secTaskEndTimeCalc);
}

/*******************************************************************************
 * @fn          llCompareSecondaryPrimaryTasksQoSParam
 *
 * @brief       This function is used to compare a QoS parameter between
 *              primary and secondary task.
 *
 * @design      /ref did_408769671
 *
 * input parameters
 *
 * @param       qosParamType - Type of Qos Param, for example: LL_QOS_TYPE_PRIORITY.
 * @param       secTask      - Pointer to the secondary task information structure.
 * @param       primConnPtr  - Pointer to the primary information structure.
 *
 * NOTE:        Notice that the primConnPtr & secTask are not the same type of variable.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      TRUE -  If Secondary task's QoS Param is higher than
 *                      Primary task's Qos Param.
 *              FALSE - If Secondary task's QoS Param is lower than
 *                      Primary task's Qos Param.
 */
uint8 llCompareSecondaryPrimaryTasksQoSParam( uint8         qosParamType,
                                              taskInfo_t    *secTask,
                                              llConnState_t *primConnPtr  )
{
  // Check Valid Input
  if (( secTask == NULL ) || ( primConnPtr == NULL ) ||
      (!(primConnPtr->activeConn)))
  {
    // Return FALSE.
    return( FALSE );
  }

  // Compare the QoS Parameter between Secondary and
  // Primary Task.
  switch (qosParamType)
  {
    // Case of QOS Type of Connection's Priority.
    case LL_QOS_TYPE_PRIORITY:
    {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
      /*****************************************/
      /********* SECONDARY SCANNER TYPE ********/
      /*****************************************/
      // Check for Scanner
      if (secTask->taskID == LL_TASK_ID_SCANNER)
      {
        // Compare the priority of the tasks and make sure the priority is
        // not garbage.
        if ((extScanInfo->priority > primConnPtr->connPriority) &&
            (extScanInfo->priority <= LL_QOS_HIGH_PRIORITY))
        {
          return TRUE;
        }
      }
#ifdef USE_PERIODIC_SCAN
      // Check For Periodic Scan
      else if (secTask->taskID == LL_TASK_ID_PERIODIC_SCANNER)
      {
        llPeriodicScanSet_t *pPeriodicScan = MAP_llGetCurrentPeriodicScan(PERIODIC_SCAN_STATE_SYNCED);
        // Compare the priority of the tasks and make sure the priority is
        // not garbage.
        if ( (pPeriodicScan != NULL) && (pPeriodicScan->priority > primConnPtr->connPriority) &&
            (pPeriodicScan->priority <= LL_QOS_HIGH_PRIORITY))
        {
          return TRUE;
        }
      }
#endif
#endif  // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
      /*******************************************/
      /********* SECONDARY INITIATOR TYPE ********/
      /*******************************************/
      if (secTask->taskID == LL_TASK_ID_INITIATOR)
      {
        // Compare the priority of the tasks and make sure the priority is
        // not garbage.
        if ((extInitInfo->priority > primConnPtr->connPriority) &&
            (extInitInfo->priority <= LL_QOS_HIGH_PRIORITY))
        {
          return TRUE;
        }
      }
#endif  // INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
      /********************************************/
      /********* SECONDARY ADVERTISER TYPE ********/
      /********************************************/
      // Check for Advertising
      if (secTask->taskID == LL_TASK_ID_ADVERTISER)
      {
        // Compare the priority of the tasks and make sure the priority is
        // not garbage.
        if ((pNextAdvSet->AdvEntry->priority > primConnPtr->connPriority) &&
            (pNextAdvSet->AdvEntry->priority <= LL_QOS_HIGH_PRIORITY))
        {
          return TRUE;
        }
      }
      // Check for Periodic Advertising
#ifdef USE_PERIODIC_ADV
      else if (secTask->taskID == LL_TASK_ID_PERIODIC_ADVERTISER)
      {
        llPeriodicAdvSet_t *pPeriodicAdv = MAP_llGetCurrentPeriodicAdv();
        // Compare the priority of the tasks and make sure the priority is
        // not garbage.
        if ((pPeriodicAdv != NULL) && (pPeriodicAdv->priority > primConnPtr->connPriority) &&
            (pPeriodicAdv->priority <= LL_QOS_HIGH_PRIORITY))
        {
          return TRUE;
        }
      }
#endif
#endif // ADV_NCONN_CFG || ADV_CONN_CFG

    }// End of LL_QOS_TYPE_PRIORITY.
    break;

    default:
    {
      // Unknown QoS parameter.
      return (FALSE);
    }
    break;

  }// End of switch (qosParamType)

  // Return False.
  return (FALSE);

}

#if defined (USE_PERIODIC_ADV) || (USE_PERIODIC_SCAN)
/*******************************************************************************
 * @fn          llCreatePermutation
 *
 * @brief       This function set the permutation operation consists of separately
 *              bit-reversing the lower 8 input bits and upper 8 input bits.
 *
 * @design  /ref did_286039104
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
uint8 llCreatePermutation( uint8 dataByte )
{
   dataByte = (dataByte & 0xF0) >> 4 | (dataByte & 0x0F) << 4;
   dataByte = (dataByte & 0xCC) >> 2 | (dataByte & 0x33) << 2;
   dataByte = (dataByte & 0xAA) >> 1 | (dataByte & 0x55) << 1;

   return( dataByte );
}

/*******************************************************************************
 * @fn          llSetNextPeriodicAdvChan
 *
 * @brief       This function returns the next data channel for a periodic
 *              advertiser based on the periodic event counter.
 *
 * @design      /ref did_286039104
 *
 * input parameters
 *
 * @param       pPeriodicAdv - A pointer to periodic advertiser.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The next data channel to use for Algorithm 2.
 */
uint8 llSetNextPeriodicAdvChan( llPeriodicChanMap_t *pChanMap, uint32 accessAddr, uint16 counter )
{
  uint8  nextChan;
  uint8  i,j;
  uint16 prn_e;
  uint16 chanId;
  uint16 perm;
  uint8 count;
  uint8 remapChIndex;

  // find prn_e and unmappedChan
  //channel Identifier
  chanId = ((uint16)(((accessAddr)>>16) ^ ((accessAddr) & 0xFFFF)));
  prn_e = chanId ^ counter;
  for (i=0; i<3; i++)
  {
    // bit reverse the lower 8 bits and the upper 8 bits
    perm = ((llCreatePermutation(((prn_e) >> 8)) << 8) |
            (llCreatePermutation((prn_e) & 0xFF)));
    // Multiply, Add, and Modulo (MAM)
    prn_e =((uint16)((((uint32)(perm) * 17) + (uint32)(chanId)) & 0xFFFF));
  }
  prn_e = prn_e ^ chanId;
  nextChan = prn_e % LL_MAX_NUM_DATA_CHAN;

  // check if the unmappedChan is a used channel
  if ( (pChanMap->bitmap[nextChan / 8] >> (nextChan % 8)) & 1 )
  {
    return( nextChan );
  }

  // The unmappedChan is a unused channel
  // calculate the remapping index according to spec - 4.5.8.3.4
  remapChIndex = (pChanMap->numUsedChans * prn_e) >> 16;
  // find the unmappedChan according to the remapping index
  count = 0;
  for (i=0; i<LL_NUM_BYTES_FOR_CHAN_MAP; i++)
  {
    for (j=0; j<BITS_PER_BYTE; j++)
    {
      // check if the channel is used; only interested in used channels
      if ( (pChanMap->bitmap[i] >> j) & 1 )
      {
        if (remapChIndex == count)
        {
          return ((i * 8) + j);
        }
        count++;
      }
    }
  }
  // we should not get to this point
  return( nextChan );
}
#endif

#ifdef USE_PERIODIC_SCAN
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 * @fn          llGetPeriodicScan
 *
 * @brief       This function is used to get the periodic scan by handle
 *
 * @design      /ref did_286039104
 *
 * input parameters
 *
 * @param       handle - periodic scanner handle.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      pPeriodicScan - A pointer to periodic scanner.
 */
llPeriodicScanSet_t *llGetPeriodicScan( uint16 handle )
{
  llPeriodicScanSet_t *pPeriodicScan = llPeriodicScan.scanList;

  // search for handle
  while( pPeriodicScan != NULL )
  {
    if ( handle == pPeriodicScan->handle )
    {
      return( pPeriodicScan );
    }

    // advance to next entry
    pPeriodicScan = pPeriodicScan->next;
  }
  return NULL;
}

/*******************************************************************************
 * @fn          llGetPeriodicScanByAdvertiser
 *
 * @brief       This function is used to find a periodic scan by address and sid
 *
 * @design      /ref did_286039104
 *
 * input parameters
 *
 * @param       sid - periodic advertiser SID (received in ADI).
 * @param       addrType - periodic advertiser address type.
 * @param       addr - periodic advertiser address.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      pPeriodicScan - A pointer to periodic advertising scanner.
 */
llPeriodicScanSet_t *llGetPeriodicScanByAdvertiser( uint8 sid, uint8 addrType, uint8 *addr )
{
  llPeriodicScanSet_t *pPeriodicScan = llPeriodicScan.scanList;

  // search for handle
  while( pPeriodicScan != NULL )
  {
    if ((addr[5] == pPeriodicScan->syncCmd.addr[5]) &&
        (addr[4] == pPeriodicScan->syncCmd.addr[4]) &&
        (addr[3] == pPeriodicScan->syncCmd.addr[3]) &&
        (addr[2] == pPeriodicScan->syncCmd.addr[2]) &&
        (addr[1] == pPeriodicScan->syncCmd.addr[1]) &&
        (addr[0] == pPeriodicScan->syncCmd.addr[0]) &&
        (addrType == pPeriodicScan->syncCmd.addrType) &&
        (sid == pPeriodicScan->syncCmd.sid))
    {
      return( pPeriodicScan );
    }

    // advance to next entry
    pPeriodicScan = pPeriodicScan->next;
  }
  return NULL;
}

/*******************************************************************************
 * @fn          llGetPeriodicAcceptListItem
 *
 * @brief       This function is used to find a periodic accept list item
 *              by given address, address type and sid
 *
 * @design      /ref did_286039104
 *
 * input parameters
 *
 * @param       sid - periodic advertiser SID (received in ADI).
 * @param       addrType - periodic advertiser address type.
 * @param       addr - periodic advertiser address.
 *
 * output parameters
 *
 * @param       pPrevItem - pointer to periodic accept list previous item.
 *
 * @return      pItem - A pointer to periodic advertising accept list item.
 */
llPeriodicAcceptListItem_t *llGetPeriodicAcceptListItem( uint8 sid, uint8 addrType, uint8 *addr, llPeriodicAcceptListItem_t **pPrevItem )
{
  llPeriodicAcceptListItem_t *pItem = llPeriodicScan.AcceptList.itemList;

  while( pItem != NULL )
  {
    if ((addr[5] == pItem->addr[5]) && (addr[4] == pItem->addr[4]) &&
        (addr[3] == pItem->addr[3]) && (addr[2] == pItem->addr[2]) &&
        (addr[1] == pItem->addr[1]) && (addr[0] == pItem->addr[0]) &&
        (addrType == pItem->addrType) && (sid == pItem->sid))
    {
      return pItem;
    }
    if (pPrevItem != NULL)
    {
      *pPrevItem = pItem;
    }
    // advance to next entry
    pItem = pItem->next;
  }
  return NULL;
}

/*******************************************************************************
 * @fn          llGetCurrentPeriodicScan
 *
 * @brief       This function is used to get the current periodic scan according
 *              to the periodic scan state
 *
 * @design      /ref did_286039104
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      pPeriodicScan - A pointer to syncing periodic scanner.
 */
llPeriodicScanSet_t *llGetCurrentPeriodicScan( uint8 state )
{
  if (state == PERIODIC_SCAN_STATE_SYNCED)
  {
    return (llPeriodicScan.currentScan);
  }
  return (llPeriodicScan.createSync);
}

#ifdef RTLS_CTE
/*******************************************************************************
 * @fn          llGetPeriodicScanCteTasks
 *
 * @brief       This function is used to get the number of periodic scan
 *              tasks which have active CTE sampling process
 *
 * @design      /ref did_286039104
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      tasksCounter - number of CTE tasks.
 */
uint8 llGetPeriodicScanCteTasks( void )
{
  llPeriodicScanSet_t *pPeriodicScan = llPeriodicScan.scanList;
  uint8 tasksCounter = 0;

  // search on periodic scanner list
  while( pPeriodicScan != NULL )
  {
    // check if Connectionless CTE sampling is or was enabled
    if (pPeriodicScan->cteInfo.enable != LL_CTE_SAMPLING_NOT_INIT)
    {
      tasksCounter++;
    }
    // advance to next entry
    pPeriodicScan = pPeriodicScan->next;
  }
  return tasksCounter;
}
#endif

/*******************************************************************************
 * @fn          llSetPeriodicScanChmapUpdate
 *
 * @brief       This function is used to set the periodic scanner
 *              channel map update procedure
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       pPeriodicScan - Pointer to the periodic scan.
 * @param       progress - TRUE - advertiser was updated the channel map
 *                         FALSE - new channel map was adapted
 * @param       pChanMap - Pointer to the new channel map
 * @param       instant - Periodic adv event counter for change to new channel map
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetPeriodicScanChmapUpdate( llPeriodicScanSet_t *pPeriodicScan, uint8 progress, uint8 *pChanMap, uint16 instant )
{
  if ((progress == TRUE) && (pPeriodicScan->chanMap.updated == TRUE))
  {
    return;
  }
  if (progress == TRUE)
  {
    // update the new channel map
    llSetPeriodicChanMap(&pPeriodicScan->chanMap.next, pChanMap );
    //set the channel map instant
    pPeriodicScan->chanMapUpdateEvent = instant;
  }
  else
  {
    // copy the new channel map to the current
    MAP_osal_memcpy(&pPeriodicScan->chanMap.current,&pPeriodicScan->chanMap.next,sizeof(llPeriodicChanMap_t));
  }
  pPeriodicScan->chanMap.updated = progress;
}

/*******************************************************************************
 * @fn          llProcessPeriodicScanSyncInfo
 *
 * @brief       This function process the sync info that was received in
 *              current AUX_ADV_IND.
 *
 * @design      /ref did_286039104
 *
 * input parameters
 *
 * @param       pPkt - A pointer to received sync info.
 * @param       advEvent - A pointer to generated advertising report.
 * @param       timeStamp - time stamp of received AUX_ADV_IND.
 * @param       phy - PHY type of received AUX_ADV_IND.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llProcessPeriodicScanSyncInfo( uint8 *pPkt, aeExtAdvRptEvt_t *advEvent, uint32 timeStamp, uint8 phy )
{
  uint8 found = FALSE;
  llPeriodicScanSet_t *pPeriodicScan;

  // check for existing sync with current advertiser
  if (llPeriodicScan.scanList != NULL)
  {
    pPeriodicScan = llGetPeriodicScanByAdvertiser( advEvent->advSid, advEvent->addrType, advEvent->addr );
    if (pPeriodicScan != NULL)
    {
      found = TRUE;
      //check if the channel map was updated
      if (((*(pPkt + 4)) != pPeriodicScan->chanMap.current.bitmap[0]) ||
          ((*(pPkt + 5)) != pPeriodicScan->chanMap.current.bitmap[1]) ||
          ((*(pPkt + 6)) != pPeriodicScan->chanMap.current.bitmap[2]) ||
          ((*(pPkt + 7)) != pPeriodicScan->chanMap.current.bitmap[3]) ||
          (((*(pPkt + 8)) & 0x1F) != (pPeriodicScan->chanMap.current.bitmap[4] & 0x1F)))
      {
        // start the channel map update procedure
        llSetPeriodicScanChmapUpdate(pPeriodicScan,TRUE,pPkt + 4,*(uint16 *)(pPkt + 16));
      }
    }
  }

  // check the create sync status
  if ((found == FALSE) && (llPeriodicScan.createSync != NULL) &&
      (llPeriodicScan.createSync->terminate == 0) &&
      (llPeriodicScan.createSync->state == PERIODIC_SCAN_STATE_SYNCING_PENDING))
  {
    // case of not using the periodic accept list
    if (!(GET_PERIODIC_SCAN_OPTIONS_LIST_USE(llPeriodicScan.createSync->syncCmd.options)))
    {
      //check that the peer addr, type and sid were received from the requested advertiser
      if ((advEvent->addr[5] == llPeriodicScan.createSync->syncCmd.addr[5]) &&
          (advEvent->addr[4] == llPeriodicScan.createSync->syncCmd.addr[4]) &&
          (advEvent->addr[3] == llPeriodicScan.createSync->syncCmd.addr[3]) &&
          (advEvent->addr[2] == llPeriodicScan.createSync->syncCmd.addr[2]) &&
          (advEvent->addr[1] == llPeriodicScan.createSync->syncCmd.addr[1]) &&
          (advEvent->addr[0] == llPeriodicScan.createSync->syncCmd.addr[0]) &&
          (advEvent->addrType == llPeriodicScan.createSync->syncCmd.addrType) &&
          (advEvent->advSid == llPeriodicScan.createSync->syncCmd.sid))
      {
        found = TRUE;
      }
    }
    else
    {
      // look for the the advertiser in the periodic accept list
      llPeriodicAcceptListItem_t *pItem = llGetPeriodicAcceptListItem( advEvent->advSid, advEvent->addrType, advEvent->addr,NULL );
      if (pItem != NULL)
      {
        MAP_osal_memcpy( llPeriodicScan.createSync->syncCmd.addr , pItem->addr, LL_DEVICE_ADDR_LEN );
        llPeriodicScan.createSync->syncCmd.addrType = pItem->addrType;
        llPeriodicScan.createSync->syncCmd.sid = pItem->sid;
        found = TRUE;
      }
    }
    if (found)
    {
      uint16 packetOffsetField = *(uint16 *)pPkt;
      uint16 offsetUnitVal;

      if (packetOffsetField > 0)
      {
        // parse the packet offset
        llPeriodicScan.createSync->syncInfo.offsetUnit = ((packetOffsetField >> AE_AUX_OFFSET_SIZE) & 0x01);
        offsetUnitVal = (llPeriodicScan.createSync->syncInfo.offsetUnit == AE_AUX_OFFSET_UNITS_30_US)?
                      AE_AUX_OFFSET_30_US_UNIT_VALUE: AE_AUX_OFFSET_300_US_UNIT_VALUE;
        llPeriodicScan.createSync->syncInfo.packetOffset = (packetOffsetField & AE_AUX_OFFSET_MASK) * offsetUnitVal;
      }
      else
      {
        // the packet offset is too far
        return;
      }
      pPkt += 2;
      // parse the interval
      llPeriodicScan.createSync->interval = *(uint16 *)pPkt;
      pPkt += 2;
      //parse the channel map
      llSetPeriodicChanMap( &llPeriodicScan.createSync->chanMap.current, pPkt );
      //parse the SCA
      llPeriodicScan.createSync->syncInfo.sca = ((*(pPkt + 4) >> 5) & 0x07);
      pPkt += LL_NUM_BYTES_FOR_CHAN_MAP;
      //parse the access address
      llPeriodicScan.createSync->syncInfo.accessAddr = *(uint32 *)pPkt;
      pPkt += LL_PKT_SYNCH_LEN;
      //parse the access crc init
      llPeriodicScan.createSync->syncInfo.crcInit[0] = *(pPkt + 0);
      llPeriodicScan.createSync->syncInfo.crcInit[1] = *(pPkt + 1);
      llPeriodicScan.createSync->syncInfo.crcInit[2] = *(pPkt + 2);
      pPkt += LL_PKT_CRC_LEN;
      //parse the event counter
      llPeriodicScan.createSync->syncInfo.eventCounter = *(uint16 *)pPkt;
      //update periodic scanning start time
      llPeriodicScan.createSync->startTime = timeStamp + US_TO_RAT_TICKS(llPeriodicScan.createSync->syncInfo.packetOffset);
      // update the periodic phy
      llPeriodicScan.createSync->phy = phy;
      llPeriodicScan.createSync->ownAddrType = extScanInfo->ownAddrType;
      //update the skip according to the timeout and interval parameters
      llPeriodicScan.createSync->syncCmd.skip = MIN(llPeriodicScan.createSync->syncCmd.skip,
                                                PERIODIC_SCAN_MISSED_LIMIT(llPeriodicScan.createSync->syncCmd.timeout,llPeriodicScan.createSync->interval));
      // trigger the periodic scan
      llTrigPeriodicScan(llPeriodicScan.createSync);
    }
  }
}

/*******************************************************************************
 * @fn          llClearPeriodicScanSets
 *
 * @brief       This function is used to clear all periodic scan sets
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       void.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llClearPeriodicScanSets(void)
{
  llPeriodicScanSet_t *pPeriodicScan = NULL;
  taskInfo_t          *pCurTask;
  uint8 i;

  // clear the periodic terminate list
  for (i=0; i < PERIODIC_SCAN_TERMINATE_LIST_MAX_HANDLES;i++)
  {
    llPeriodicScan.terminateList[i] = PERIODIC_SCAN_TERMINATE_LIST_INVALID_HANDLE;
  }

  if ((llPeriodicScan.scanList == NULL) && (llPeriodicScan.createSync == NULL) &&
      (llPeriodicScan.acceptList.itemList == NULL))
  {
    return;
  }
  pCurTask = MAP_llGetCurrentTask();
  //check if current task is periodic scan
  if ((pCurTask != NULL) &&
      (pCurTask->taskID == LL_TASK_ID_PERIODIC_SCANNER))
  {
    // halt the radio
    MAP_llHaltRadio( CMD_ABORT );
  }
  // release the create sync
  if (llPeriodicScan.createSync != NULL)
  {
    if ((llPeriodicScan.scanNumActive == 0) && (llPeriodicScan.llTask != NULL))
    {
      // free the associated task block
      MAP_llFreeTask( &llPeriodicScan.llTask );
    }
    // clear the set
    MAP_osal_mem_free(llPeriodicScan.createSync);
    llPeriodicScan.createSync = NULL;
  }
  //release all periodic scan sets
  if (llPeriodicScan.scanList != NULL)
  {
    // free the associated task block
    MAP_llFreeTask( &llPeriodicScan.llTask );

    while( llPeriodicScan.scanList != NULL )
    {
      pPeriodicScan = llPeriodicScan.scanList->next;
#ifdef RTLS_CTE
      // delete the CTE antenna
      if (llPeriodicScan.scanList->cteInfo.pAntenna != NULL)
      {
        MAP_osal_mem_free(llPeriodicScan.scanList->cteInfo.pAntenna);
      }
#endif // RTLS_CTE
      // delete the set
      MAP_osal_mem_free(llPeriodicScan.scanList);
      // advance to next set
      llPeriodicScan.scanList = pPeriodicScan;
    }
    llPeriodicScan.scanList = NULL;
    llPeriodicScan.currentScan = NULL;
    llPeriodicScan.scanNumActive = 0;
  }
  //release the periodic accept list
  LE_ClearPeriodicAdvList();
}

/*******************************************************************************
 * @fn          llTerminatePeriodicScan
 *
 * @brief       This function is used to end all periodic scan sets in terminate list
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       void.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llTerminatePeriodicScan(void)
{
  llPeriodicScanSet_t *pPeriodicScan;
  uint8 i;

  for (i=0; i < PERIODIC_SCAN_TERMINATE_LIST_MAX_HANDLES;i++)
  {
    if (llPeriodicScan.terminateList[i] != PERIODIC_SCAN_TERMINATE_LIST_INVALID_HANDLE)
    {
      // in case of the first index which reserved for the create sync
      if (i == PERIODIC_SCAN_TERMINATE_LIST_CREATE_SYNC_INDEX)
      {
        pPeriodicScan = MAP_llGetCurrentPeriodicScan(PERIODIC_SCAN_STATE_SYNCING_ACTIVE);
      }
      else
      {
        pPeriodicScan = llGetPeriodicScan(llPeriodicScan.terminateList[i]);
      }
      llPeriodicScan.terminateList[i] = PERIODIC_SCAN_TERMINATE_LIST_INVALID_HANDLE;
      if (pPeriodicScan != NULL)
      {
        MAP_llEndPeriodicScanTask(pPeriodicScan);
      }
    }
  }
}

/*******************************************************************************
 * @fn          llUpdateExtScanAcceptSyncInfo
 *
 * @brief       This function is used to set the accept sync info bit in
 *              the extended scan filter RF param byte
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       void.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llUpdateExtScanAcceptSyncInfo(void)
{
  if ((llPeriodicScan.scanNumActive > 0) || (llPeriodicScan.createSync != NULL))
  {
    SET_EXT_SCAN_FILTER_CFG_ACCEPT_SYNCINFO( extScanParam.extFltrCfg );
  }
  else
  {
    CLR_EXT_SCAN_FILTER_CFG_ACCEPT_SYNCINFO( extScanParam.extFltrCfg );
  }
}

#endif
#endif //USE_PERIODIC_SCAN

/*********************************************************************
 * @fn          llCheckRfCmdPreemption
 *
 * @brief     This routine is used to check the start time of the
 *            current RF command and if it meets the conditions,
 *            it aborts it and reschedules the next RF command.
 *
 * @design  /ref did_411832444
 *
 * input parameters
 *
 * @param     endTime - time in RF ticks represents the new RF command
 *                      start time plus OTA time and processing time.
 *
 * @return    True - previous RF command was aborted
 *            False - otherwise
 */
uint8 llCheckRfCmdPreemption( uint32 endTime ,uint8 priority)
{
  halIntState_t cs;
  llConnState_t *connPtr = NULL;

  // Current priority set as defult to high for primary tasks
  uint8 llStatePriority = LL_QOS_HIGH_PRIORITY;

  if (llConns.currentConn != LL_INVALID_CONNECTION_ID)
  {
    // Get the struct info for the connection that is about to be scheduled.
    connPtr = MAP_llDataGetConnPtr( llConns.currentConn );

    // check if the current connection is in a connection establishment (first 6 connection events).
    // in case of a connection establishment, don't allow the preemption to enable the connection connect properly.
    // and if not activate the preemption mechanism
    if (connPtr->currentEvent <= LL_LINK_SETUP_TIMEOUT)
    {
      return FALSE;
    }
  }

  // Get the current priority in case a secondary task is active
  switch (llState)
  {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
    case LL_STATE_SCAN:
      llStatePriority = extScanInfo->priority;
    break;
#ifdef USE_PERIODIC_SCAN
    case LL_STATE_PERIODIC_SCAN:
    {
      llPeriodicScanSet_t *pPeriodicScan = MAP_llGetCurrentPeriodicScan(PERIODIC_SCAN_STATE_SYNCED);
      if (pPeriodicScan != NULL)
      {
        llStatePriority = pPeriodicScan->priority;
      }
    }break;
#endif
#endif
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
    case LL_STATE_INIT:
      llStatePriority = extInitInfo->priority;
    break;
#endif
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
    case LL_STATE_EXT_ADV:
    {
      advSet_t *pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );
      if (pAdvSet != NULL)
      {
        llStatePriority = pAdvSet->priority;
      }
    }break;
#ifdef USE_PERIODIC_ADV
    case LL_STATE_PERIODIC_ADV:
    {
      llPeriodicAdvSet_t *pPeriodicAdv = MAP_llGetCurrentPeriodicAdv();
      if (pPeriodicAdv != NULL)
      {
        llStatePriority = pPeriodicAdv->priority;
      }
    }break;
#endif
#endif
  }
  // Check if the current RF command has not yet started and
  // that there is enough time to run the next RF command
  // also check the priority in case of secondary task
  if ((MAP_llTimeCompare(llRfCmdPreemption.startTime , endTime)) || (priority > llStatePriority))
  {
    HAL_ENTER_CRITICAL_SECTION(cs);
    // Enable the preemption in order to call the appropriate post process function in abort interrupt
    llRfCmdPreemption.enable = TRUE;

#ifdef USE_RCL
    // Abort the current RF command
    RCL_Command_stop((RCL_Command_Handle)llTaskList.curTask->command, RCL_StopType_DescheduleOnly);
    // call the scheduler
    // we call it with NULL because we do not want to set the status to idle
    // while it's in the middle of the stop process
    LL_rclRescheduleCommand(NULL);
#else
    MAP_llHaltRadio( CMD_ABORT );
#endif
    HAL_EXIT_CRITICAL_SECTION(cs);

    return TRUE;
  }
  return FALSE;
}

/*********************************************************************
 * @fn          llSetRfCmdPreemptionParams
 *
 * @brief     This routine is used to set the preemption mechanism
 *            parameters such as the start time of the current RF command.
 *
 * @design  /ref did_411832444
 *
 * input parameters
 *
 * @param     startTime - RF command start time.
 *
 * @return    None.
 */
void llSetRfCmdPreemptionParams( uint32 startTime)
{
  llRfCmdPreemption.startTime = startTime;
  llRfCmdPreemption.enable = FALSE;
}

/*********************************************************************
 * @fn          llGetRfCmdPreemptionEnable
 *
 * @brief     This routine is used to Get the preemption enable parameter.
 *
 * @design  /ref did_411832444
 *
 * input parameters
 *
 * @param     None.
 *
 * @return    True or False.
 */
uint8 llGetRfCmdPreemptionEnable(void)
{
  return llRfCmdPreemption.enable;
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llDetachNode
 *
 * @brief       This routine detaches a node from the adv sorted list in O(1).
 *
 * @design      /ref did_262316463
 *
 * input parameters
 *
 * @param       aeNode - Pointer to AE node to be detached.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      pointer to the detached node.
 */
sortedAdv_t *llDetachNode(sortedAdv_t *aeNode)
{
  sortedAdv_t *temp = aeNode->next;
  advSet_t *tempSet = aeNode->AdvEntry;
  uint16  tmpTimeConsume;
  uint32  tmpTimeScheduled;

  // avoid losing pNextAdvSet due to this algo which
  // switches "values" with the next
  if (aeNode->next == pNextAdvSet)
  {
    pNextAdvSet = aeNode;
  }

  aeNode->AdvEntry=temp->AdvEntry;
  tmpTimeConsume=aeNode->timeConsume;
  tmpTimeScheduled=aeNode->timeScheduled;
  aeNode->timeConsume=temp->timeConsume;
  temp->timeConsume=tmpTimeConsume;
  aeNode->timeScheduled=temp->timeScheduled;
  temp->timeScheduled=tmpTimeScheduled;
  temp->AdvEntry = tempSet;
  aeNode->next = temp->next;
  temp->next = NULL;

  return (temp);
}

#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llSetAETimeConsume
 *
 * @brief       This routine calculates the time to be consumed
 *              over the air while executing the relevant AE set.
 *              The time is saved in the aeNode.
 *
 * @design      /ref did_262316463
 *
 * input parameters
 *
 * @param       aeNode - Pointer to AE node which will be calculated
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetAETimeConsume(sortedAdv_t *aeNode)
{
  uint16 totTime = 0;

  // if AE set is legacy, set its consume time to a const value.
  if (TST_AE_PROPS_LEGACY(aeNode->AdvEntry->pAdvParam->eventProps))
  {
    totTime = LEGACY_ADV_MAX_TIME_CONSUME;
  }
#ifdef USE_AE
  else //NOT LEGACY
  {
    aeRf_t *pRf;
    // get pointer to RF command.
    pRf = (aeRf_t *)aeNode->AdvEntry->pRfCmds;

    // time consumed on primary channels.
    totTime = AE_CONSUME_OVERHEAD;
    if (!aeNode->AdvEntry->otaTimeExtAdv)
    {
      totTime += MAP_llOctets2Time(pRf->extRfCmd[0].phyMode & 0x03, // first two bits only
                (pRf->extRfCmd[0].phyMode>>2) & 0x01, // scheme
                 AE_NUM_BYTES_OVERHAED_27_BYTES * 5,
                 MIC_NOT_ENABLED );
    }
    else
    {
      totTime += aeNode->AdvEntry->otaTimeExtAdv;
    }

    // time consumed on secondary channels.
    // NOTE : we distinguish between scannable and non scannable due to the fact
    // that otaTimeAuxAdv in the scannable case is not calculated with the "worse case scenario" timing.
    // "worse case scenario" is when the advertiser receives a scan req and then it will send a scan rsp.

    // check if not scanable.
    if (!TST_AE_PROPS_SCAN(aeNode->AdvEntry->pAdvParam->eventProps ))
    {
      if (!aeNode->AdvEntry->otaTimeAuxAdv)
      {
        totTime += MAP_llOctets2Time(pRf->auxRfCmd.phyMode & 0x03,  // first two bits only
                  (pRf->auxRfCmd.phyMode>>2) & 0x01, // scheme
                   aeNode->AdvEntry->dataLen ? aeNode->AdvEntry->dataLen : AE_NUM_BYTES_OVERHAED_27_BYTES * 2,
                   MIC_NOT_ENABLED );
      }
      else
      {
        // more then 1 fragment OTA
        if (aeNode->AdvEntry->numFrags > 1)
        {
          totTime += ((AE_MIN_T_MAFS_IN_US + aeNode->AdvEntry->otaTimeAuxAdv) * (aeNode->AdvEntry->numFrags - 1))
                    + (MAP_llOctets2Time( pRf->auxRfCmd.phyMode & 0x03,    // first two bits only
                                        (pRf->auxRfCmd.phyMode>>2) & 0x01, // scheme
                                         aeNode->AdvEntry->lastFragLen,
                                         MIC_NOT_ENABLED ));
        }
        else
        // one fragment will be sent OTA
        {
          totTime += aeNode->AdvEntry->otaTimeAuxAdv;
        }
      }
    }
    else // scannable
    {
      uint32 firstFragConsume;
      // calculation of first fragment as it should have been.
      firstFragConsume = MAP_llOctets2Time(pRf->auxRfCmd.phyMode & 0x03,     // first two bits only
                                          (pRf->auxRfCmd.phyMode>>2) & 0x01, // scheme
                                           aeNode->AdvEntry->fragLen,
                                           MIC_NOT_ENABLED );
      // more then 1 fragment OTA
      if (aeNode->AdvEntry->numFrags > 1)
      {
        totTime += ((AE_MIN_T_MAFS_IN_US + firstFragConsume) * (aeNode->AdvEntry->numFrags - 1))
                  + (MAP_llOctets2Time( pRf->auxRfCmd.phyMode & 0x03,    // first two bits only
                                      (pRf->auxRfCmd.phyMode>>2) & 0x01, // scheme
                                      aeNode->AdvEntry->lastFragLen,
                                      MIC_NOT_ENABLED ));

      }
      else
      // one fragment will be sent OTA
      {
        totTime += firstFragConsume;
      }
    }
  }
#endif
  // set the total time to be consumed over the air.
  aeNode->timeConsume = totTime;
}

#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llGetAdvSortedEntry
 *
 * @brief       This routine is used to find an adv sorted list node,
 *              according to given adv set.
 *
 * input parameters
 *
 * @param       pAdvSet - Pointer to advertising set.
 *
 * output parameters
 *
 * @param       none.
 *
 * @return      Pointer to the AE scheduler node or NULL.
 */

sortedAdv_t * llGetAdvSortedEntry( advSet_t *pAdvSet )
{
  sortedAdv_t *advSortedEntry = pNextAdvSet;
  uint8 i;

  for (i=0; i < numActiveAdvSets; i++)
  {
    if (advSortedEntry != NULL)
    {
      if (advSortedEntry->AdvEntry == pAdvSet)
      {
        return advSortedEntry;
      }
      advSortedEntry = advSortedEntry->next;
    }
  }
  return NULL;
}

/*******************************************************************************
 * @fn          llAddAdvSortedEntry
 *
 * @brief       This routine is used to allocate the memory needed for adv sorted list
 *              node. Then sets the correct start time for the node and insert the node
 *              in the correct sorted place in the AE List.
 *
 * @design      /ref did_262316463
 *
 *
 * input parameters
 *
 * @param       pAdvSet - Pointer to advertising set.
 *
 * output parameters
 *
 * @param       newNode - Pointer to the AE scheduler node.
 *
 * @return      start time of the AE scheduler node.
 */

uint32 llAddAdvSortedEntry( advSet_t *pAdvSet, sortedAdv_t** newNode )
{
  sortedAdv_t *newAdvEntry;
  uint32 newStartTime;

  // check that we alreay add the handle
  newAdvEntry = llGetAdvSortedEntry(pAdvSet);
  if (newAdvEntry != NULL)
  {
    *newNode = NULL;
    return 1;
  }
  // allocate new node
  newAdvEntry = MAP_osal_mem_alloc(sizeof(sortedAdv_t));

  if (newAdvEntry)
  {
    newAdvEntry->AdvEntry = pAdvSet;
    // creation of first node
    if ((pNextAdvSet == NULL) && (numActiveAdvSets == 0))
    {
      pNextAdvSet = newAdvEntry;
      // make the list cyclic
      newAdvEntry->next = pNextAdvSet;
      // set  the startTime
      newStartTime = MAP_llGetCurrentTime() + (3*RAT_TICKS_IN_1MS);
    }
    else if (numActiveAdvSets == 1)
    // 2nd node
    {
      // set the start time to be list's head + 2ms for processing  +  random (0-15 ms)
      newStartTime = pNextAdvSet->AdvEntry->advStartTime + (2*RAT_TICKS_IN_1MS);
      newStartTime += (uint32)(MAP_LL_ENC_GeneratePseudoRandNum() & 0xF) * RAT_TICKS_IN_1MS;
      // make the list cyclic
      newAdvEntry->next = pNextAdvSet;
      pNextAdvSet->next = newAdvEntry;
    }
    else
    {
      // set start time and insert node in the correct place
      newStartTime = pNextAdvSet->AdvEntry->advStartTime + (2 *  RAT_TICKS_IN_1MS);
      newStartTime += (uint32)(MAP_LL_ENC_GeneratePseudoRandNum() & 0xF) * RAT_TICKS_IN_1MS;
      // temporary node to traverse the list
      sortedAdv_t *tmpNode = pNextAdvSet;
      // search for the correct place to insert the new node and maintain the sort
      while( (!MAP_llTimeCompare(tmpNode->next->AdvEntry->advStartTime , newStartTime)) && (tmpNode->next != pNextAdvSet))
      {
        tmpNode=tmpNode->next;
      }
      // Insert node at correct place
      newAdvEntry->next = tmpNode->next;
      tmpNode->next = newAdvEntry;
    }
  }
  else
  {
    // can't allocate memory
    MAP_llExtAdvCBack( LL_CBACK_OUT_OF_MEMORY, NULL );
    return( AE_INVALID_START_TIME );
  }
  // increment active advertise sets by 1
  numActiveAdvSets ++;
  // output the pointer to the new AE scheduler node
  *newNode = newAdvEntry;
  return( newStartTime );
}
#endif

#ifdef USE_AE
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llSetRestPrimaryChannels
 *
 * @brief       This routine is used to set randomly the rest of the primary
 *              channels number. In case the channel map has 3 channels, it will set
 *              second and third channels randomly according to the first channel.
 *              In case it includes only 2 channels, it will set only the second.
 *
 * input parameters
 *
 * @param       pAdvSet - Pointer to advertising set.
 *
 * output parameters
 *
 * @param       none.
 *
 * @return      none.
 */

void llSetRestPrimaryChannels( advSet_t *pAdvSet )
{
  aeRf_t *pRf = (aeRf_t *)pAdvSet->pRfCmds;
  uint8 i;

  // initialise the first index channel as reference
  pRf->extRfCmd[1].chan = pAdvSet->firstPrimChan;
  // update the channels
  for (i=1; i<(pAdvSet->numPrimChans); i++)
  {
    // update the channel
    if ((i == 1) && (pAdvSet->numPrimChans == LL_MAX_NUM_ADV_CHAN))
    {
      // set randomly one of the other two channels
      pRf->extRfCmd[i].chan = pAdvSet->firstPrimChan + ((MAP_llGetCurrentTime() % 2) + 1);
      // in case the first channel was 39
      if (pRf->extRfCmd[i].chan > LL_LAST_RF_CHAN)
      {
        pRf->extRfCmd[i].chan -= LL_MAX_NUM_ADV_CHAN;
      }
    }
    else // set the last channel
    {
      pRf->extRfCmd[i].chan = ((pAdvSet->pAdvParam->primChanMap & ((~(BV(pRf->extRfCmd[1].chan - LL_ADV_BASE_CHAN) | BV(pAdvSet->firstPrimChan - LL_ADV_BASE_CHAN)))&LL_ADV_CHAN_ALL))| 36);
      // in case the last channel is 39
      if (pRf->extRfCmd[i].chan == (LL_MAX_NUM_DATA_CHAN - 1))
      {
        pRf->extRfCmd[i].chan = LL_LAST_RF_CHAN;
      }
    }
  }
}
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llSetExtendedAdvParams
 *
 * @brief       This routine is used to set the extended advertising parameters.
 *
 * input parameters
 *
 * @param       pAdvSet - Pointer to advertising set.
 * @param       pCmdParams - Pointer to advertising parameters.
 *
 * output parameters
 *
 * @param       none.
 *
 * @return      status.
 */

llStatus_t llSetExtendedAdvParams( advSet_t *pAdvSet, aeSetParamCmd_t *pCmdParams )
{
  // set advertisement event to invalid
  pAdvSet->advEvtType = LL_ADV_INVALID_EVT;

  // check the primary advertising channel PHY
  if ( pCmdParams->primPhy == AE_PHY_2_MBPS )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check if connectable and scannable, and HDC Directed is requested
  if ( (TST_AE_PROPS_CONN(pCmdParams->eventProps) &&
        TST_AE_PROPS_SCAN(pCmdParams->eventProps))                          ||
        TST_AE_PROPS_HDC_DIR(pCmdParams->eventProps) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check if connectable or scannable, and AdvA requested to be omitted
  if ( (TST_AE_PROPS_CONN(pCmdParams->eventProps) ||
        TST_AE_PROPS_SCAN(pCmdParams->eventProps))                          &&
       TST_AE_PROPS_OMIT_ADVA(pCmdParams->eventProps) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check peerAddrType/peerAddr are valid when using Directed Advertising
  if ( TST_AE_PROPS_DIR(pCmdParams->eventProps)                             &&
       ( ((pCmdParams->peerAddrType != LL_DEV_ADDR_TYPE_PUBLIC) &&
         (pCmdParams->peerAddrType != LL_DEV_ADDR_TYPE_RANDOM))) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // ?? check if event type does not support data, but we already have data

  // clear flags, etc.
  pAdvSet->extHdrFlags = 0;
  pAdvSet->auxHdrFlags = 0;

  // check the advertising event properties for the advMode
  if ( !TST_AE_PROPS_CONN(pCmdParams->eventProps) &&
       !TST_AE_PROPS_SCAN(pCmdParams->eventProps) )
  {
    SET_ADV_MODE( pAdvSet->extHdrInfo,
                  AE_ADV_MODE_NONCONN_NONSCAN );

    // check if an auxPtr is going to be mandatory:
    // - Host requested AdvA be omitted
    // - Adv Data or ACAD is present
    // Note: ACAD is currently not supported.
    if ( TST_AE_PROPS_OMIT_ADVA(pCmdParams->eventProps) ||
         (pAdvSet->pAdvData     != NULL)                ||
         (pAdvSet->pScanRspData != NULL) )
    {
      // set auxPtr and ADI flags in extHdrFlags
      SET_EXTHDR_FLAG( pAdvSet->extHdrFlags,
                       EXTHDR_FLAG_AUXPTR | EXTHDR_FLAG_ADI );
    }

    // check if an auxPtr is going to be mandatory:
    // - Host requested AdvA be included, but primary PHY is Coded
    // Note: AdvA can only be present in Ext or Aux, not both.
    if ( !TST_AE_PROPS_OMIT_ADVA(pCmdParams->eventProps) )
    {
      // check the primary advertising channel PHY
      // Note: Per the spec, when the primary PHY is Coded, the ADV_EXT_IND
      //       shall not contain an AdvA field!
      if ( (pCmdParams->primPhy == AE_PHY_CODED_S2) ||
           (pCmdParams->primPhy == AE_PHY_CODED_S8) )
      {
        // set auxPtr and ADI flags in extHdrFlags
        SET_EXTHDR_FLAG( pAdvSet->extHdrFlags,
                         EXTHDR_FLAG_AUXPTR | EXTHDR_FLAG_ADI );

        // set auxPtr, ADI, and AdvA flags in auxHdrFlags
        SET_EXTHDR_FLAG( pAdvSet->auxHdrFlags,
                         EXTHDR_FLAG_ADVA );
      }
      else // primary PHY is 1M
      {
        // we don't know yet whether auxPtr will be set based on data or ACAD,
        // but we do know we either have an ADV_EXT_IND-only without data, or
        // an ADV_EXT_IND with an auxPtr, so in either case, set the AdvA flag
        // in the extended header until we know for sure
        SET_EXTHDR_FLAG( pAdvSet->extHdrFlags,
                         EXTHDR_FLAG_ADVA );

      }
    }

    // check if directed
    if ( TST_AE_PROPS_DIR(pCmdParams->eventProps) )
    {
      // it is, so include TargetA in extHdrFlags
      SET_EXTHDR_FLAG( pAdvSet->auxHdrFlags,
                       EXTHDR_FLAG_TARGETA );
    }
  }
  else // either connectable or scannable
  {
    SET_ADV_MODE( pAdvSet->extHdrInfo,
                  (TST_AE_PROPS_CONN(pCmdParams->eventProps) ?
                   AE_ADV_MODE_CONNECTABLE                   :
                   AE_ADV_MODE_SCANNABLE) );

    // set auxPtr and ADI flags in extHdrFlags (never AdvA)
    SET_EXTHDR_FLAG( pAdvSet->extHdrFlags,
                     EXTHDR_FLAG_AUXPTR | EXTHDR_FLAG_ADI );

    // set ADI and AdvA flags in auxHdrFlags (never auxPtr)
    SET_EXTHDR_FLAG( pAdvSet->auxHdrFlags,
                     EXTHDR_FLAG_ADI | EXTHDR_FLAG_ADVA );

    // check if directed
    if ( TST_AE_PROPS_DIR(pCmdParams->eventProps) )
    {
      // it is, so include TargetA, but only in the secondary channel
      SET_EXTHDR_FLAG( pAdvSet->auxHdrFlags,
                       EXTHDR_FLAG_TARGETA );
    }
  }

  // copy Extended Header Adv Mode to Aux
  pAdvSet->auxHdrInfo = pAdvSet->extHdrInfo & ADV_MODE_MASK;

  return( LL_STATUS_SUCCESS );
}
#endif
#endif //USE_AE

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          LE_AE_SetData
 *
 * @brief       This API is used to set the data used in advertising PDUs
 *              that have a data field. This command may be issued whether
 *              advertising is enabled or disabled. The Controller only
 *              supports operations Complete and Unchanged. The only
 *              allowed preference is Controller Fragments. If the Data
 *              Length is 0xFF, then the Controller will use the first two
 *              bytes of the Data as the length.
 *
 *              Note: This command is common to LE Set Extended Advertising
 *                    Data Command and LE Set Extended Scan Response Data
 *                    Command.
 *
 * input parameters
 *
 * @param       pCmdParams - Pointer to input parameters.
 * @param       dataSrc    - LE_AE_EXT_DATA_CMD_ADV
 *                           LE_AE_EXT_DATA_CMD_SCAN_RSP
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS,
 *              LL_STATUS_ERROR_BAD_PARAMETER
 *              LL_STATUS_ERROR_COMMAND_DISALLOWED
 *              LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED
 *              LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER
 */
llStatus_t LE_AE_SetData( aeSetDataCmd_t *pCmdParams,
                          uint8           dataSrc )
{
  // check parameter pointer
  if ( (!pCmdParams) ||
       ((dataSrc != LE_AE_EXT_DATA_CMD_ADV) &&
        (dataSrc != LE_AE_EXT_DATA_CMD_SCAN_RSP)) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // TODO: HANDLE CHECK ON ADV DATA VS SCAN RSP DATA WHEN ADV MODE IS CHANGED?
  //       I.E. IF ADV MODE IS SCANNABLE, BUT dataSrc IS LE_AE_EXT_DATA_CMD_ADV,
  //       DO I CARE? I DON'T THINK SO. ONLY POINTERS TO DATA HELD.

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

  // check if there's no actual data yet
  if ( pCmdParams->operation == AE_DATA_OP_NO_DATA )
  {
    // save the pointers, even though there's no data yet
    // Note: This allows the Host to create the data structure prior to having
    //       a data pointer.
    if ( dataSrc == LE_AE_EXT_DATA_CMD_ADV )
    {
      // save the pointer to advertising data parameters
      pAdvSet->pAdvData = pCmdParams;
    }
    else // LE_AE_EXT_DATA_CMD_SCAN_RSP
    {
      // save the pointer to scan response data parameters
      pAdvSet->pScanRspData = pCmdParams;
    }

    return( LL_STATUS_SUCCESS );
  }

  // check the fragmentation preference
  // ALT: Consider using the fragPref to reduce the packing of AE packets, so
  //      that more PDUs could be generated (e.g. for testing very long chains).
  //      The upper 7 bits of this parameter could be used to indicate the size
  //      of the AE packet fragment when AE_DATA_FRAG_PREF_CTRL_MAY_FRAG is
  //      used. If AE_DATA_FRAG_PREF_CTRL_MAY_NOT_FRAG is used, then the
  //      the Controller will pack the data in as few PDUs as possible.
  // Note: For now, this parameter is ignored.
  //if ( pCmdParams->fragPref == AE_DATA_FRAG_PREF_CTRL_MAY_FRAG )

  // check the fragmentation operation is allowed
  if ( (pCmdParams->operation != AE_DATA_OP_COMPLETE) &&
       (pCmdParams->operation != AE_DATA_OP_UNCHANGED) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // check for valid parameters
  // Unchanged:
  // - advertising is currently disabled
  // - there is no advertising data
  // - legacy PDU being used
  // - advertising data length is not zero
  // !Unchanged
  // - advertising data length is zero
  // - advertising data is invalid
  // Note: If the operation is Unchanged, then only DID needs to be updated.
  if ( pCmdParams->operation == AE_DATA_OP_UNCHANGED )
  {
    if ( (pAdvSet->advMode == LL_ADV_MODE_OFF)                ||
          ((pCmdParams->pData == NULL) &&
           (pAdvSet->pAdvData == NULL))                       ||
         TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps)  ||
         (pCmdParams->dataLen != 0) )
    {
      return( LL_STATUS_ERROR_BAD_PARAMETER );
    }
  }
  else if ( pCmdParams->operation != AE_DATA_OP_COMPLETE )
  {
    // check for valid length and pointer
    if ( (pCmdParams->dataLen == 0) ||
         (pCmdParams->pData   == NULL) )
    {
      return( LL_STATUS_ERROR_BAD_PARAMETER );
    }
  }

  // TODO: CHECK IF ADV EVENT DOESN'T SUPPORT DATA? E.G. SCANNABLE? NC/NS?

  // check if this will be a legacy advertisement
  if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
  {
    // check max data length for legacy advertising PDUs
    if ( pCmdParams->dataLen > LL_MAX_ADV_DATA_LEN )
    {
      return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
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
      return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED );
    }
  }
#endif
  // check if only the DDI needs to be updated
  // Note: Vol 2, Part E, Section 7.8.54 says to update DDI when the operation
  //       is AE_DATA_OP_UNCHANGED, but here we are going to update the DDI
  //       whether the data has actually changed or not.
  // ALT: if ( pCmdParams->operation == AE_DATA_OP_UNCHANGED )
  {
    uint16 rand;

    // find a new DID that is not the same as the current DID
    do
    {
      (void)MAP_LL_ENC_GenerateTrueRandNum( (uint8 *)&rand,
                                            sizeof(uint16) );

    } while( (rand & EXTHDR_DID_MASK) == (pAdvSet->adi & EXTHDR_DID_MASK) );

    // update DID without messing with SID (in case one is there)
    pAdvSet->adi &= ~EXTHDR_DID_MASK;
    pAdvSet->adi |= (rand & EXTHDR_DID_MASK);
  }

  // save the pointers
  // Note: No check is made here to see if there previously was a valid poniter.
  //       It is up to the Host to completely maintain and manage data.
  if ( dataSrc == LE_AE_EXT_DATA_CMD_ADV )
  {
    // save the pointer to advertising data parameters
    pAdvSet->pAdvData = (pCmdParams->dataLen) ? pCmdParams : NULL;
  }
  else // LE_AE_EXT_DATA_CMD_SCAN_RSP
  {
    // save the pointer to scan response data parameters
    pAdvSet->pScanRspData = (pCmdParams->dataLen) ? pCmdParams : NULL;
  }

#ifdef USE_AE
  // check if data was added after Set Params and before Enable
  if ( (pAdvSet->pAdvData     != NULL) ||
       (pAdvSet->pScanRspData != NULL) )
  {
    // set auxPtr and ADI flags in extHdrFlags
    SET_EXTHDR_FLAG( pAdvSet->extHdrFlags,
                     EXTHDR_FLAG_AUXPTR | EXTHDR_FLAG_ADI );
  }
#endif
#ifdef QUAL_TEST
  if ( pAdvSet->advMode == LL_ADV_MODE_ON )
  {
    // keep the adv handle as indication that the data was updated by Host
    aeDataUpdatedDuringAdv = pCmdParams->handle;
  }
  else
  {
    aeDataUpdatedDuringAdv = EXT_DATA_NO_UPDATE_DURING_ADV;
  }
#endif

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

////////////////////////////////////////////////////////////////////////////////
// TI Controller Support API
////////////////////////////////////////////////////////////////////////////////

/*******************************************************************************
 * @fn          LL_AE_Init
 *
 * @brief       This API is used to initialize Extended Advertising, Extended
 *              Scanner, and Extended Initiating in the Controller.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 */
llStatus_t LL_AE_Init( void )
{
  // set the size of the callback table
  // ALT: Make aeCBackTbl dynamic.
  aeCBackTbl.numCBack = (sizeof(aeCBackTbl_t) / sizeof(uint32)) - 1;

  // clear the callback table
  (void)MAP_LL_AE_RegCBack( LL_CBACK_CLEAR, NULL );

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  // ensure all Adv Sets are disabled
  MAP_LL_DisableAdvSets();

  // clear all Adv Sets
  MAP_LE_ClearAdvSets();

  // clear the current Adv Set handle
  aeCurHandle = 0;
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
  // stop the timer
  (void)MAP_osal_stop_timerEx( LL_TaskID, LL_EVT_EXT_SCAN_TIMEOUT );

  // clear the current Scanner index
  extScanIndex = 0;
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  // clear the current Initiator index
  extInitIndex = 0;
#endif // SCAN_CFG

  return( LL_STATUS_SUCCESS );
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          LL_GetAdvSet
 *
 * @brief       This API is used to get an advertising set based on the
 *              advertising set handle. If the handle is not present, then
 *              based on allocFlag, the adv set will be created if the
 *              maximum supported advertisement sets have not yet been
 *              reached.
 *
 * input parameters
 *
 * @param       handle    - Advertising handle.
 * @param       allocFlag - LE_SEARCH_ADV_SET,
 *                          LE_ALLOCATE_ADV_SET
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to advertising set, or NULL.
 */
advSet_t *LL_GetAdvSet( uint8 handle,
                        uint8 allocFlag )
{
  halIntState_t cs;

  HAL_ENTER_CRITICAL_SECTION(cs);

  advSet_t *pAdvSet = advSetList;

  // search for handle
  while( pAdvSet )
  {
    if ( handle == pAdvSet->pAdvParam->handle )
    {
      HAL_EXIT_CRITICAL_SECTION(cs);
      return( pAdvSet );
    }

    if ( pAdvSet->next == NULL ) break;

    // advance to next entry
    pAdvSet = pAdvSet->next;
  }


  // check flag to determine if a handle should be allocated
  if ( allocFlag == LE_SEARCH_ADV_SET )
  {
    HAL_EXIT_CRITICAL_SECTION(cs);
    return( NULL );
  }

  // about to allocate; check if our limit has been reached
  if ( MAP_LL_CountAdvSets( LE_COUNT_ALL_ADV_SETS ) >= maxSupportedAdvSets )
  {
    HAL_EXIT_CRITICAL_SECTION(cs);
    return( NULL );
  }

  // check if we have a valid pointer
  // Note: A valid pointer means the list wasn't empty, and we have last entry.
  //       A NULL pointer means the list was empty.
  if ( pAdvSet )
  {
    pAdvSet->next = MAP_osal_mem_alloc( sizeof( advSet_t ) );
    pAdvSet = pAdvSet->next;
  }
  else // Adv Set List is empty
  {
    pAdvSet = MAP_osal_mem_alloc( sizeof( advSet_t ) );
    advSetList = pAdvSet;
  }

  // check if we have an entry
  if ( pAdvSet )
  {
    // init pointers
    pAdvSet->next           = NULL;
    pAdvSet->llTask         = NULL;
    //pAdvSet->pOwnAddr       = NULL;
    pAdvSet->pOwnRandAddr   = NULL;
    pAdvSet->pData          = NULL;
    pAdvSet->pRfCmds        = NULL;
    pAdvSet->pAdvParam      = NULL;
    pAdvSet->pAdvData       = NULL;
    pAdvSet->pScanRspData   = NULL;
    pAdvSet->pEnable        = NULL;

    // clear various fields
#ifdef USE_AE
    pAdvSet->extHdrSize     = 0;
    pAdvSet->extHdrFlags    = 0;
    pAdvSet->auxExtHdrSize  = 0;
    pAdvSet->auxHdrFlags    = 0;
    pAdvSet->auxChanCounter = 0;
    pAdvSet->fragLen        = 0;
    pAdvSet->lastFragLen    = 0;
    pAdvSet->numFrags       = 0;
#endif
    pAdvSet->maxAvailData   = 0;
    pAdvSet->dataLen        = 0;
    pAdvSet->advMode        = LL_ADV_MODE_OFF;

    // set Peripheral SCA
    pAdvSet->scaValue       = aePeripheralSCA;

    // init DDI
    (void)MAP_LL_ENC_GenerateTrueRandNum( (uint8 *)&pAdvSet->adi,
                                          sizeof(uint16) );
    pAdvSet->adi &= EXTHDR_DID_MASK;
  }

  HAL_EXIT_CRITICAL_SECTION(cs);

  return( pAdvSet );
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          LL_CountAdvSets
 *
 * @brief       This routine is used to count the number of Adv Sets.
 *
 * input parameters
 *
 * @param       type - LE_COUNT_ALL_ADV_SETS, LE_COUNT_ENABLED_ADV_SETS
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The number of Adv Sets.
 */
uint8 LL_CountAdvSets( uint8 type )
{
  uint8     count   = 0;
  halIntState_t cs;

  HAL_ENTER_CRITICAL_SECTION(cs);

  advSet_t *pAdvSet = advSetList;

  while( pAdvSet )
  {
    if ( (type == LE_COUNT_ALL_ADV_SETS) ||
         ((type == LE_COUNT_ENABLED_ADV_SETS) && (pAdvSet->advMode == LL_ADV_MODE_ON)) )
    {
      count++;
    }

    pAdvSet = pAdvSet->next;
  }
  HAL_EXIT_CRITICAL_SECTION(cs);

  return( count );
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          LL_DisableAdvSets
 *
 * @brief       This routine is used to disable all Adv Sets.
 *
 * @design      /ref did_262316463
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The number of Adv Sets.
 */
void LL_DisableAdvSets( void )
{
  advSet_t *pAdvSet = advSetList;

  while( pAdvSet )
  {
    pAdvSet->advMode = LL_ADV_MODE_OFF;
    pAdvSet = pAdvSet->next;
  }

  halIntState_t cs;
  HAL_ENTER_CRITICAL_SECTION(cs);
  sortedAdv_t *pCurr = pNextAdvSet;
  sortedAdv_t *pCurrNext;
  sortedAdv_t *head = pCurr;

  //delete cyclic link-list.
  if ((head != NULL) && (head->next != NULL))
  {
    pCurr=pCurr->next;
    while((pCurr != NULL) && (pCurr != head))
    {
      pCurrNext = pCurr->next;
      MAP_osal_mem_free( pCurr );
      pCurr = pCurrNext;
    }
  }

  //delete the head of the cyclic link-list.
  if(head != NULL)
  {
    MAP_osal_mem_free(head);
  }

  pNextAdvSet = NULL;
  numActiveAdvSets = 0;
  HAL_EXIT_CRITICAL_SECTION(cs);

  return;
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llUpdateSortedAdvList
 *
 * @brief       This routine is used to update the head of adv sorted list
 *              to its sorted place
 *
 * @design      /ref did_262316463
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer
 */

void llUpdateSortedAdvList( void )
{
  if (numActiveAdvSets == 2)
  {
    //swap between the 2 AE nodes
    advSet_t *temp = pNextAdvSet->AdvEntry;
    uint16 tmpTimeConsume = pNextAdvSet->timeConsume;
    uint32 tmpTimeScheduled = pNextAdvSet->timeScheduled;
    pNextAdvSet->timeConsume = pNextAdvSet->next->timeConsume;
    pNextAdvSet->timeScheduled = pNextAdvSet->next->timeScheduled;
    pNextAdvSet->AdvEntry = pNextAdvSet->next->AdvEntry;
    pNextAdvSet->next->AdvEntry = temp;
    pNextAdvSet->next->timeConsume = tmpTimeConsume;
    pNextAdvSet->next->timeScheduled = tmpTimeScheduled;
  }
  else if (numActiveAdvSets > 2)
  {

    // detach the node which is unsorted
    sortedAdv_t *unSortedNode = MAP_llDetachNode(pNextAdvSet);
    sortedAdv_t *tmpNode = pNextAdvSet;

    // search for the correct place in the sorted list
    while( (MAP_llTimeCompare(unSortedNode->AdvEntry->advStartTime , tmpNode->next->AdvEntry->advStartTime)) && (tmpNode->next != pNextAdvSet))
    {
      tmpNode=tmpNode->next;
    }

    // insert node at correct place
    sortedAdv_t *replaceNode = tmpNode->next;
    tmpNode->next = unSortedNode;
    unSortedNode->next = replaceNode;
  }
}
#endif


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llFindNextAdvSet
 *
 * @brief       This routine is used find next Adv Set in adv sorted list.
 *
 * @design      /ref did_262316463
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer
 */

void *llFindNextAdvSet( void )
{
  uint32 curTime = MAP_llGetCurrentTime();
  uint16 delay;

  if ((pNextAdvSet == NULL) || (numActiveAdvSets == 0))
  {
    return(NULL);
  }

  // check if next Adv set was already disabled.
  do
  {
    if (pNextAdvSet->AdvEntry->advMode == LL_ADV_MODE_OFF)
    {
      // NOTE: in the case we're about to disblae the last adv set
      //       it will be handled in llFreeTask since it need to close
      //       the "whole" adv task.
      if (numActiveAdvSets > 1)
      {
        MAP_osal_mem_free(llDetachNode(pNextAdvSet));
        numActiveAdvSets--;
      }
    }
    // make sure the next Adv set is in the future
    while (MAP_llTimeCompare(curTime,pNextAdvSet->AdvEntry->advStartTime))
    {
      if (pNextAdvSet->AdvEntry->advEvtType == LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT)
      {
        // in high duty cycle there is no interval so set the start time in the nearest future
        pNextAdvSet->AdvEntry->advStartTime = MAP_llGetCurrentTime() + RAT_TICKS_IN_1MS;
      }
      else
      {
#ifdef CONTROLLER_ONLY
        // Do not access the zeroDelay as it is not part of the BLE SIG HCI command
        // generate random advertising delay from 0..10ms, in units of 250ns ticks
        delay = (uint16)(MAP_LL_ENC_GeneratePseudoRandNum() % 11) * RAT_TICKS_IN_1MS;
#else
        // by default, calculate delay between advertise sets
        if(pNextAdvSet->AdvEntry->pAdvParam->zeroDelay == 0)
        {
          // generate random advertising delay from 0..10ms, in units of 250ns ticks
          delay = (uint16)(MAP_LL_ENC_GeneratePseudoRandNum() % 11) * RAT_TICKS_IN_1MS;
        }
        else // if delay between advertise sets is disabled
        {
          // set delay to 0
          delay = 0;
        }
#endif
        // add an interval to the start time of the AE set
        uint32_t primIntMinTemp = BUILD_UINT32(pNextAdvSet->AdvEntry->pAdvParam->primIntMin[0],
                                           pNextAdvSet->AdvEntry->pAdvParam->primIntMin[1],
                                           pNextAdvSet->AdvEntry->pAdvParam->primIntMin[2],
                                           0);
        pNextAdvSet->AdvEntry->advStartTime += ((primIntMinTemp * RAT_TICKS_IN_625US) + delay);
      }
      // check if the next AE set in its correct position in the adv sorted list.
      if (numActiveAdvSets >= 2)
      {
        if (MAP_llTimeCompare(pNextAdvSet->AdvEntry->advStartTime,pNextAdvSet->next->AdvEntry->advStartTime))
        {
           // make sure the AE List is sorted accordoing to start time.
           MAP_llUpdateSortedAdvList();
        }
      }
    }
  }while (pNextAdvSet->AdvEntry->advMode == LL_ADV_MODE_OFF);
  // save the handle of the next scheduled AE set
  aeCurHandle = pNextAdvSet->AdvEntry->pAdvParam->handle;

  //update the start time
#ifdef USE_RCL
  ((RCL_Command *)pNextAdvSet->AdvEntry->pRfCmds)->timing.absStartTime = pNextAdvSet->AdvEntry->advStartTime;
#else
  ((rfOpCmd_t *)pNextAdvSet->AdvEntry->pRfCmds)->startTime = pNextAdvSet->AdvEntry->advStartTime;
#endif
  // set the correct rf cmd in the adv task
  pNextAdvSet->AdvEntry->llTask->command = (uint32)pNextAdvSet->AdvEntry->pRfCmds;

  return( (void *)pNextAdvSet->AdvEntry->pRfCmds );
}

#ifdef USE_PERIODIC_ADV
/*******************************************************************************
 * @fn          llSelectExtAdvOrPeriodicAdv
 *
 * @brief       This routine is used for the scheduler to select between Extended
 *              Adv and Periodic Adv task.
 *
 * @design      /ref did_286039104
 *
 * input parameters
 *
 * @param       pPeriodicAdv - periodic advertiser candidate.
 * @param       extAdvNumMissed - number of extended adv missed packets.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE - Extended Adv was selected; FALSE - Periodic Adv was selected
 */

uint8 llSelectExtAdvOrPeriodicAdv( llPeriodicAdvSet_t *pPeriodicAdv, uint16 extAdvNumMissed )
{
  uint32 periodicAdvInterval = (pPeriodicAdv->interval * RAT_TICKS_IN_1_25MS);
  uint32 extAdvInterval = (*((uint32 *)pNextAdvSet->AdvEntry->pAdvParam->primIntMin)) * RAT_TICKS_IN_625US;

  /////////////////////////////////////////////
  // check for collision
  /////////////////////////////////////////////
  //check if the periodic adv start before the ext adv
  if (MAP_llTimeCompare(pNextAdvSet->AdvEntry->advStartTime , pPeriodicAdv->startTime))
  {
    // check for collision between the two
    if (MAP_llTimeCompare(pNextAdvSet->AdvEntry->advStartTime , pPeriodicAdv->startTime + pPeriodicAdv->totalOtaTime))
    {
      // no collision - choose the periodic adv
      return FALSE;
    }
  }
  else // ext adv starts before the periodic adv
  {
    // check for collision between the two
    if (MAP_llTimeCompare(pPeriodicAdv->startTime , pNextAdvSet->AdvEntry->advStartTime + pNextAdvSet->timeConsume))
    {
      // no collision - choose the ext adv
      return TRUE;
    }
  }
  /////////////////////////////////////////////
  // check periodic priority parameter
  /////////////////////////////////////////////
  if (pPeriodicAdv->intPriority > LL_QOS_LOW_PRIORITY)
  {
    return FALSE;
  }
  /////////////////////////////////////////////
  // check priority parameter
  /////////////////////////////////////////////
  if (pPeriodicAdv->priority > pNextAdvSet->AdvEntry->priority)
  {
    return FALSE;
  }
  else if (pNextAdvSet->AdvEntry->priority > pPeriodicAdv->priority)
  {
    return TRUE;
  }
  /////////////////////////////////////////////
  // check interval parameter
  /////////////////////////////////////////////
  if (periodicAdvInterval > extAdvInterval)
  {
    if (periodicAdvInterval <= extAdvInterval * extAdvNumMissed)
    {
      return TRUE;
    }
    else
    {
      return FALSE;
    }
  }
  if (extAdvInterval > periodicAdvInterval)
  {
    if (extAdvInterval <= periodicAdvInterval * pPeriodicAdv->numMissed)
    {
      return FALSE;
    }
    else
    {
      return TRUE;
    }
  }
  /////////////////////////////////////////////
  // check missed packet parameter
  /////////////////////////////////////////////
  // same intervals
  if (pPeriodicAdv->numMissed > extAdvNumMissed)
  {
    return FALSE;
  }
  else if (extAdvNumMissed > pPeriodicAdv->numMissed)
  {
    return TRUE;
  }
  /////////////////////////////////////////////
  // check start time parameter
  /////////////////////////////////////////////
  if (MAP_llTimeCompare(pNextAdvSet->AdvEntry->advStartTime , pPeriodicAdv->startTime))
  {
    return FALSE;
  }
  else
  {
    return TRUE;
  }
}
/*******************************************************************************
 * @fn          llSelectPeriodicAdv
 *
 * @brief       This routine is used to select Periodic Adv between 2 candidates.
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       pPeriodicAdv1 - first periodic Adv candidate.
 * @param       pPeriodicAdv2 - second periodic Adv candidate.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to the selected periodic Adv
 */

llPeriodicAdvSet_t *llSelectPeriodicAdv( llPeriodicAdvSet_t *pPeriodicAdv1, llPeriodicAdvSet_t *pPeriodicAdv2 )
{
  /////////////////////////////////////////////
  // check for collision
  /////////////////////////////////////////////
  //check if the second start before the first
  if (MAP_llTimeCompare(pPeriodicAdv1->startTime , pPeriodicAdv2->startTime))
  {
    // check for collision between the two
    if (MAP_llTimeCompare(pPeriodicAdv1->startTime , pPeriodicAdv2->startTime + pPeriodicAdv2->totalOtaTime))
    {
      // no collision - choose the second
      return pPeriodicAdv2;
    }
  }
  else // first starts before the second
  {
    // check for collision between the two
    if (MAP_llTimeCompare(pPeriodicAdv2->startTime , pPeriodicAdv1->startTime + pPeriodicAdv1->totalOtaTime))
    {
      // no collision - choose the first
      return pPeriodicAdv1;
    }
  }
  /////////////////////////////////////////////
  // check internal priority parameter
  /////////////////////////////////////////////
  if ((pPeriodicAdv1->intPriority > pPeriodicAdv2->intPriority) &&
      (pPeriodicAdv1->eventCounter == pPeriodicAdv1->syncInfo.eventCounter))
  {
    return pPeriodicAdv1;
  }
  else if ((pPeriodicAdv2->intPriority > pPeriodicAdv1->intPriority) &&
           (pPeriodicAdv2->eventCounter == pPeriodicAdv2->syncInfo.eventCounter))
  {
    return pPeriodicAdv2;
  }
  /////////////////////////////////////////////
  // check priority parameter
  /////////////////////////////////////////////
  if (pPeriodicAdv1->priority > pPeriodicAdv2->priority)
  {
    return pPeriodicAdv1;
  }
  else if (pPeriodicAdv2->priority > pPeriodicAdv1->priority)
  {
    return pPeriodicAdv2;
  }
  /////////////////////////////////////////////
  // check interval parameter
  /////////////////////////////////////////////
  if (pPeriodicAdv1->interval > pPeriodicAdv2->interval)
  {
    if (pPeriodicAdv1->interval <= pPeriodicAdv2->interval * pPeriodicAdv2->numMissed)
    {
      return pPeriodicAdv2;
    }
    else
    {
      return pPeriodicAdv1;
    }
  }
  if (pPeriodicAdv2->interval > pPeriodicAdv1->interval)
  {
    if (pPeriodicAdv2->interval <= pPeriodicAdv1->interval * pPeriodicAdv1->numMissed)
    {
      return pPeriodicAdv1;
    }
    else
    {
      return pPeriodicAdv2;
    }
  }
  /////////////////////////////////////////////
  // check missed packet parameter
  /////////////////////////////////////////////
  // pPeriodicScan2->interval = pPeriodicScan1->interval
  if (pPeriodicAdv1->numMissed > pPeriodicAdv2->numMissed)
  {
    return pPeriodicAdv1;
  }
  else if (pPeriodicAdv2->numMissed > pPeriodicAdv1->numMissed)
  {
    return pPeriodicAdv2;
  }
  /////////////////////////////////////////////
  // check start time parameter
  /////////////////////////////////////////////
  // pPeriodicScan2->numMissed = pPeriodicScan1->numMissed
  if (MAP_llTimeCompare(pPeriodicAdv1->startTime , pPeriodicAdv2->startTime))
  {
    return pPeriodicAdv2;
  }
  else
  {
    return pPeriodicAdv1;
  }
}

/*******************************************************************************
 * @fn          llFindNextPeriodicAdv
 *
 * @brief       This routine is used find next Periodic Adv Set in periodic adv sorted list.
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer
 */

void *llFindNextPeriodicAdv( void )
{
  llPeriodicAdvSet_t *pPeriodicAdv = llPeriodicAdv.advList;
  uint32 currentTime = MAP_llGetCurrentTime() + RAT_TICKS_FOR_SCHED_PROCESS_TIME;
  uint16 eventCounter;
  llPeriodicAdvSet_t *pSelectPeriodicAdv = NULL;
  uint8 extAdvSelected = FALSE;
  uint16 extAdvNumMissed = 0;

  if ((pPeriodicAdv == NULL) || (llPeriodicAdv.advNumActive == 0))
  {
    return(NULL);
  }
  //check for existing ext adv
  if ((pNextAdvSet != NULL) && (numActiveAdvSets > 0) &&
      (pNextAdvSet->AdvEntry->advMode == LL_ADV_MODE_ON))
  {
    uint32 extAdvInterval = (*((uint32 *)pNextAdvSet->AdvEntry->pAdvParam->primIntMin)) * RAT_TICKS_IN_625US;
    extAdvSelected = TRUE;
    // find the number of missed ext adv packets according to the last schedule time
    extAdvNumMissed = MAP_llTimeDelta(pNextAdvSet->AdvEntry->advStartTime ,pNextAdvSet->timeScheduled)/extAdvInterval;
    if (extAdvNumMissed > 0)
    {
      extAdvNumMissed--;
    }
  }
  // look for the most appropriate periodic adv task among all active periodic adv tasks
  while (pPeriodicAdv != NULL)
  {
    if (pPeriodicAdv->state == PERIODIC_ADV_STATE_ENABLE)
    {
      eventCounter = pPeriodicAdv->eventCounter;
      // set periodic adv start time to the future
      while(MAP_llTimeCompare(currentTime , pPeriodicAdv->startTime))
      {
        pPeriodicAdv->startTime = pPeriodicAdv->startTime + (pPeriodicAdv->interval * RAT_TICKS_IN_1_25MS );
        pPeriodicAdv->eventCounter++;
        pPeriodicAdv->numMissed++;
      }
      // case counter was updated
      if (pPeriodicAdv->eventCounter != eventCounter)
      {
        //update the next channel
        pPeriodicAdv->currentChan = llSetNextPeriodicAdvChan( pPeriodicAdv->pChanMap, pPeriodicAdv->syncInfo.accessAddr ,pPeriodicAdv->eventCounter);
        pPeriodicAdv->rfCmd.chan = pPeriodicAdv->currentChan & AE_CHAN_INDEX_MASK;
        // update extended header contents
        llSetupPeriodicHdr(pPeriodicAdv);
      }
      // set the start time
      pPeriodicAdv->rfCmd.rfOpCmd.startTime = pPeriodicAdv->startTime;
      // set the chain start time
      pPeriodicAdv->rfParam.auxPtrTgtTime = pPeriodicAdv->rfCmd.rfOpCmd.startTime +
                                            US_TO_RAT_TICKS(pPeriodicAdv->otaTime +
                                                        START_SYNTH_TO_RAT_OFFSET);

      // case previously no adv task was selected
      if (extAdvSelected)
      {
        extAdvSelected = llSelectExtAdvOrPeriodicAdv(pPeriodicAdv,extAdvNumMissed);
        if (extAdvSelected == FALSE)
        {
          pSelectPeriodicAdv = pPeriodicAdv;
        }
      }
      // case previously no periodic adv task and no ext adv task was selected
      else if (pSelectPeriodicAdv == NULL)
      {
        pSelectPeriodicAdv = pPeriodicAdv;
      }
      else
      {
        // select the most appropriate periodic adv task between previously selected and currently
        pSelectPeriodicAdv = llSelectPeriodicAdv(pSelectPeriodicAdv,pPeriodicAdv);
      }
    }
    pPeriodicAdv = pPeriodicAdv->next;
  }
  if (extAdvSelected == FALSE)
  {
    // set the current periodic adv pointer
    llPeriodicAdv.currentAdv = pSelectPeriodicAdv;
  }
  else
  {
    pSelectPeriodicAdv = NULL;
  }

  if (pSelectPeriodicAdv != NULL)
  {
    // pointer to radio operation command
    llPeriodicAdv.llTask->command = (uint32)&pSelectPeriodicAdv->rfCmd;

    return( (void *)&pSelectPeriodicAdv->rfCmd );
  }
  llPeriodicAdv.llTask->command = 0;
  return NULL;
}
#endif
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

/*******************************************************************************
 * @fn          LL_AE_RegCBack
 *
 * @brief       This TI Controller Specific API is used to register various
 *              callbacks for extended advertising, extended scanning, and
 *              extended initiating.
 *
 * input parameters
 *
 * @param       cBackId - Callback ID.
 * @param       pCBack  - Void pointer to callback function.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 *              LL_STATUS_ERROR_BAD_PARAMETER
 */
llStatus_t LL_AE_RegCBack( uint8  cBackId,
                           void  *pCBack )
{
  uint32 *pCBackEntry = &aeCBackTbl.reserved + 1;

  // check input parameter
  if ( cBackId == LL_CBACK_CLEAR )
  {
    for (uint8 i=0; i<aeCBackTbl.numCBack; i++)
    {
      pCBackEntry[i] = 0;
    }

    return( LL_STATUS_SUCCESS );
  }

  // check if callback ID is valid
  if ( (cBackId != LL_CBACK_ADV_START_AFTER_ENABLE) &&
       (cBackId != LL_CBACK_ADV_END_AFTER_DISABLE)  &&
       (cBackId != LL_CBACK_ADV_START)              &&
       (cBackId != LL_CBACK_ADV_END)                &&
       (cBackId != LL_CBACK_EXT_ADV_REPORT)         &&
       (cBackId != LL_CBACK_ADV_SET_TERMINATED)     &&
       (cBackId != LL_CBACK_EXT_SCAN_REQ_RECEIVED)  &&
       (cBackId != LL_CBACK_EXT_SCAN_TIMEOUT)       &&
       (cBackId != LL_CBACK_EXT_SCAN_START)         &&
       (cBackId != LL_CBACK_EXT_SCAN_END)           &&
       (cBackId != LL_CBACK_EXT_SCAN_WINDOW_END)    &&
       (cBackId != LL_CBACK_EXT_SCAN_INTERVAL_END)  &&
       (cBackId != LL_CBACK_EXT_SCAN_DURATION_END)  &&
       (cBackId != LL_CBACK_EXT_SCAN_PERIOD_END)    &&
       (cBackId != LL_CBACK_OUT_OF_MEMORY)          &&
       (cBackId != LL_CBACK_EXT_ADV_DATA_TRUNCATED) )
  {
    return( LL_STATUS_ERROR_BAD_PARAMETER );
  }

  // set the call back
  pCBackEntry[cBackId] = (uint32)pCBack;

  return( LL_STATUS_SUCCESS );
}

////////////////////////////////////////////////////////////////////////////////
// TI Controller Internal Functions
////////////////////////////////////////////////////////////////////////////////


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
#ifdef USE_AE // scanner
/*******************************************************************************
 * @fn          llManageExtScanStateList
 *
 * @brief       Manage scan report state list
 *
 * input parameters
 *
 * @param       action     - EXT_SCAN_STATE_LIST_ADD / EXT_SCAN_STATE_LIST_FIND
 *                           EXT_SCAN_STATE_LIST_REMOVE / EXT_SCAN_STATE_LIST_CLEAR_ALL
 * @param       sid        - Set ID received from ADV_EXT_IND or AUX_ADV_IND
 * @param       timeOffset - time offset to receive the AUX PTR in usec
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      entry in scan state list or
 *              NULL
 */
extScanReportState_t *llManageExtScanStateList(uint8 action, uint8 sid, uint32 timeOffset)
{
  uint8 i;
  uint8 availble = 0xFF;
  uint32 currentTime;

  if ((action == EXT_SCAN_STATE_LIST_ADD) || (EXT_SCAN_STATE_LIST_FIND))
  {
    // Get the current time
    currentTime = MAP_llGetCurrentTime();
  }
  for (i=0; i < EXT_SCAN_STATE_LIST_MAX_ENTRIES ; i++)
  {
    if (action == EXT_SCAN_STATE_LIST_CLEAR_ALL)
    {
      extScanReportState[i].state = WAIT_FOR_ADV_EXT_IND;
    }
    else if (action == EXT_SCAN_STATE_LIST_ADD)
    {
      // Check for existing SID
      if ((extScanReportState[i].sid == sid) &&
          (extScanReportState[i].state != WAIT_FOR_ADV_EXT_IND))
      {
        // SID is already exist - drop the packet
        return NULL;
      }
      // Check for empty entry
      else if (extScanReportState[i].state == WAIT_FOR_ADV_EXT_IND)
      {
        // Get only the first availble entry
        if (availble == 0xFF)
        {
          availble = i;
        }
      }
      // check for occupied entry which is invalid
      else if (MAP_llTimeCompare(currentTime,extScanReportState[i].time))
      {
        // clear the entry
        extScanReportState[i].state = WAIT_FOR_ADV_EXT_IND;
        // Get only the first availble entry
        if (availble == 0xFF)
        {
          availble = i;
        }
      }
    }
    else // EXT_SCAN_STATE_LIST_REMOVE or EXT_SCAN_STATE_LIST_FIND
    {
      if (extScanReportState[i].sid == sid)
      {
        if (action == EXT_SCAN_STATE_LIST_REMOVE)
        {
          extScanReportState[i].state = WAIT_FOR_ADV_EXT_IND;
        }
        else //action == EXT_SCAN_STATE_LIST_FIND
        {
          // set expected time as current time + AUX offset
          // case the time offset is 0 it will add only the guard time
          extScanReportState[i].time = currentTime + (RAT_TICKS_IN_1US * timeOffset);
        }
        return &extScanReportState[i];
      }
    }
  }
  if ((action == EXT_SCAN_STATE_LIST_ADD) && (availble != 0xFF))
  {
    // set expected time as current time + AUX offset
    extScanReportState[availble].time = currentTime + (RAT_TICKS_IN_1US * timeOffset);
    // Set the state to wait for ADV AUX
    extScanReportState[availble].state = WAIT_FOR_AUX_ADV_IND;
    // Set the received SID
    extScanReportState[availble].sid = sid;
    return &extScanReportState[availble];
  }
  return NULL;
}

/*******************************************************************************
 * @fn          llSetExtendedAdvReport
 *
 * @brief       This function is used to process received extended advertising packet
 *
 * input parameters
 *
 * @param       extAdvRpt - pointer to allocated report
 * @param       pPkt - pointer to received packet
 * @param       evtType - event type
 * @param       extHdrFlgs - extended header flags
 * @param       pHdr - packet header
 * @param       dataLen - received data length
 *
 * output parameters
 *
 * @param       pSyncInfo - pointer to sync info.
 * @param       secPhy - secondary PHY.
 * @param       pChannelIndex - channel index.
 *
 * @return      Send report - True or False
 *
 */
uint8 llSetExtendedAdvReport(aeExtAdvRptEvt_t *extAdvRpt,
                             uint8 *pPkt,
                             uint16 evtType,
                             uint8 extHdrFlgs,
                             uint8 pHdr,
                             uint8 dataLen,
                             uint8 **pSyncInfo,
                             uint8 *secPhy,
                             uint8 *pChannelIndex)
{
  uint32            auxTimeOffset = 0;  // AUX PTR time offset in usec
  extScanReportState_t *pScanState = NULL; // report state per SID
  uint8 sendReport = ((pBleEvtMask[LE_EVT_INDEX_EXTENDED_ADV_REPORT] & LE_EVT_MASK_EXTENDED_ADV_REPORT) &&
                      (MAP_llCheckCBack(LL_CBACK_EXT_ADV_REPORT)));

  // init event type based on advertising mode
  extAdvRpt->evtType = evtType;

  // get AdvA, if there's one
  if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_ADVA ) )
  {
    // copy peer's address and set type
    MAP_osal_memcpy( &extAdvRpt->addr, pPkt, B_ADDR_LEN );
    extAdvRpt->addrType = MASK_ID_ADDRTYPE(pHdr >> LL_ADV_PDU_HDR_TXADDR);
    pPkt += B_ADDR_LEN;
  }
  else // AdvA not provided
  {
    MAP_osal_memset( &extAdvRpt->addr, 0, B_ADDR_LEN);

    // indicate no address provided
    extAdvRpt->addrType = AE_NO_ADDRESS_PROVIDED;
  }

  // get TargetA, if there is one
  if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_TARGETA ) )
  {
    // copy peer's address and set type
    MAP_osal_memcpy( &extAdvRpt->directAddr, pPkt, B_ADDR_LEN );
    extAdvRpt->directAddrType = MASK_ID_ADDRTYPE(pHdr >> LL_ADV_PDU_HDR_RXADDR);
    pPkt += B_ADDR_LEN;

    // set Directed bit
    extAdvRpt->evtType |= AE_PROPS_DIR_ADV;
  }
  else // TargetA not provided
  {
    MAP_osal_memset( &extAdvRpt->directAddr, 0, B_ADDR_LEN);

    // indicate no address provided
    // Note: Spec does not indicate a value to use when no TargetA is
    //       not provided (as it does for AdvA), so using same value as AdvA.
    extAdvRpt->directAddrType = AE_NO_ADDRESS_PROVIDED;
  }
  // get the CTE info if exist
  if ( TST_EXTHDR_FLAG(extHdrFlgs, EXTHDR_FLAG_CTEINFO) )
  {
    pPkt++;
  }

  // get ADI, if there is one
  if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_ADI ) )
  {
    extAdvRpt->advSid = GET_SID( *((uint16 *)pPkt) );
    extAdvRpt->advDid = GET_DID( *((uint16 *)pPkt) );
    pPkt += EXTHDR_FLAG_ADI_SIZE;
    // Save the last received SID
    lastScanAdvSid = extAdvRpt->advSid;
  }
  else // no ADI
  {
    extAdvRpt->advSid = EXT_SCAN_NO_ADI;
    extAdvRpt->advDid = EXT_SCAN_NO_DID;
  }

  // Aux Ptr
  if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_AUXPTR ) )
  {
    // Calculate the AUX time offset
    // get the offset Units (1 MSB byte) (0 = 30 usec or 1 = 300 usec) from first byte
    // multiple it with Aux offset (13 LSB bits) from second and third bytes
    auxTimeOffset = (((*pPkt >> AE_AUX_OFFSET_UNITS_OFFSET) * AE_AUX_OFFSET_UNITS_VALUE_DIFF) +
                    AE_AUX_OFFSET_30_US_UNIT_VALUE) * (( *((uint16 *)(pPkt + 1)) ) & AE_AUX_OFFSET_MASK);
    pPkt += EXTHDR_FLAG_AUXPTR_SIZE;
  }

  // Synch Info
  if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_SYNCINFO ) )
  {
    // save the sync info position for handle it later
    *pSyncInfo = pPkt;
    // get the periodic interval
    extAdvRpt->periodicAdvInt = *((uint16 *)(pPkt + 2));

    pPkt += EXTHDR_FLAG_SYNCINFO_SIZE;
  }
  else
  {
    extAdvRpt->periodicAdvInt = 0;
  }

  // get Tx Power, if there is one
  if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_TXPWR ) )
  {
    extAdvRpt->txPower = (int8)*pPkt++;
  }
  else // no Tx Power
  {
    extAdvRpt->txPower = AE_TX_POWER_NO_PREFERENCE;
  }

  // set data length, if any
  extAdvRpt->dataLen = dataLen;

  // copy the data, if there is any
  if ( dataLen )
  {
    // if there is no need to send the report - no need to allocate the adv data
    if ( sendReport )
    {
      // get a buffer for data
      // Use allocLimited to avoid overflowing the heap...
      extAdvRpt->pData = MAP_osal_mem_allocLimited( dataLen );

      // check if we got the buffer
      if ( extAdvRpt->pData )
      {
        MAP_osal_memcpy( extAdvRpt->pData, pPkt, dataLen );
      }
      else // data is lost
      {
        extAdvRpt->dataLen = 0;
      }
    }
    pPkt += dataLen;
  }

  // get the RSSI
  extAdvRpt->rssi = (int8)*pPkt++;

  // adjust RSSI based on Rx RF path compensation
  extAdvRpt->rssi += pRfPathComp->rfRxPathCompVal;
  // get the channel index
  *pChannelIndex = *pPkt++;
  // need to assign rssi and channel with RCL API when USE_AE is enabled

  // check the channel index
  if ((GET_CHANNEL_IDX(*pChannelIndex)) < LL_ADV_BASE_CHAN)
  {
    // Check that the current packet includes ADI field for SID
    if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_ADI ) )
    {
      // Get the entry from scan state list according to the received SID
      pScanState = llManageExtScanStateList(EXT_SCAN_STATE_LIST_FIND,extAdvRpt->advSid,
                                            auxTimeOffset + EXT_SCAN_STATE_RCV_AUX_GUARD_TIME);
      if (pScanState == NULL)
      {
        // case ADV_EXT_IND had no SID and the AUX has SID - we create the entry with SID = 0xFF
        // test case LL/DDI/SCN/BV-19-C round 10 and up.
        pScanState = llManageExtScanStateList(EXT_SCAN_STATE_LIST_FIND,EXT_SCAN_NO_ADI,
                                              auxTimeOffset + EXT_SCAN_STATE_RCV_AUX_GUARD_TIME);
      }
    }
    else // SID field does not exist in the current packet
    {
      // Get the entry from scan state list according to the previous received SID
      pScanState = llManageExtScanStateList(EXT_SCAN_STATE_LIST_FIND,lastScanAdvSid,
                                            auxTimeOffset + EXT_SCAN_STATE_RCV_AUX_GUARD_TIME);
      // case ADI was not includes in the AUX packet, state should be WAIT_FOR_AUX_CHAIN_IND after Scan response or
      // case ADI was not in scan response (V51_FEATURES permit to omit the ADI)
      if ((pScanState != NULL) &&
         (((pScanState->state == WAIT_FOR_AUX_CHAIN_IND) && (pScanState->lastScanRsp)) ||
          ((pScanState->state == WAIT_FOR_AUX_SCAN_RSP) && (pScanState->lastScanRsp == 0))))
      {
        extAdvRpt->advSid = lastScanAdvSid;
      }
      else
      {
        // unexpected state
        pScanState = NULL;
      }
    }
    // check for valid scan state entry
    if (pScanState == NULL)
    {
      sendReport = FALSE;
    }
    else
    {
      extAdvRpt->primPhy = pScanState->lastPrimPhy;
      // set secondary PHY
      // Note: The Rx status returns 1M/2M/S8/S2 as 0..3, so +1 to match the
      //       parameter, except for S2.
      extAdvRpt->secPhy = (*pPkt & AE_PHY_MASK);
      // save the origin phy type (for periodic scanner in case of coded phy)
      *secPhy = extAdvRpt->secPhy;
      extAdvRpt->secPhy += ((extAdvRpt->secPhy == BLE5_S2_PHY) ? 0 : 1);
      // check that the packet should be ignored and it's only for periodic syncing
      if (*pPkt & AE_SYNCINFO_ONLY_MASK)
      {
        sendReport = FALSE;
      }
      // check what we're expecting
      switch (pScanState->state)
      {
        case WAIT_FOR_AUX_ADV_IND:
          if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_AUXPTR ) )
          {
            // have an auxPtr, so expect chain next
            pScanState->state = WAIT_FOR_AUX_CHAIN_IND;

            // indicate incomplete, with more to come
            extAdvRpt->evtType |= AE_EVT_TYPE_INCOMPLETE_MORE_TO_COME;
            if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_ADVA ) )
            {
              //save the adv address for the AUX_CHAIN_IND
              MAP_osal_memcpy( &pScanState->advAddr, &extAdvRpt->addr, B_ADDR_LEN );
            }
            else
            {
              //update the adv address
              MAP_osal_memcpy( &extAdvRpt->addr, &pScanState->advAddr, B_ADDR_LEN );
              extAdvRpt->addrType = MASK_ID_ADDRTYPE(pHdr >> LL_ADV_PDU_HDR_TXADDR);
            }
          }
          else // no auxPtr
          {
            // check if Scannable
            if ( TST_AE_PROPS_FLAG( extAdvRpt->evtType, AE_PROPS_SCAN_ADV) &&
                 (extScanInfo->pScanParam->extScanParam[extScanIndex].scanType == LL_SCAN_ACTIVE) )
            {
              // scannable, so expect scan repsonse next
              pScanState->state = WAIT_FOR_AUX_SCAN_RSP;

              // indicate incomplete, with more to come
              extAdvRpt->evtType |= AE_EVT_TYPE_INCOMPLETE_MORE_TO_COME;
              // check if this is directed adv
              if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_TARGETA ) )
              {
                // set directed indication for the following scan repsonse
                pScanState->directed = AE_EVT_TYPE_DIR_ADV;
              }
              else
              {
                // set undirected indication
                pScanState->directed = 0;
              }
            }
            else // not scannable
            {
              // reset the state
              pScanState->state = WAIT_FOR_ADV_EXT_IND;

              // indicate complete
              extAdvRpt->evtType |= AE_EVT_TYPE_COMPLETE;
              if (!( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_ADVA ) ))
              {
                //update the adv address
                MAP_osal_memcpy( &extAdvRpt->addr, &pScanState->advAddr, B_ADDR_LEN );
                extAdvRpt->addrType = MASK_ID_ADDRTYPE(pHdr >> LL_ADV_PDU_HDR_TXADDR);
              }
            }
          }
          break;

        case WAIT_FOR_AUX_CHAIN_IND:
          // check if this is a chain from the scan response
          if ( pScanState->lastScanRsp )
          {
            // indicate complete
            extAdvRpt->evtType |= AE_EVT_TYPE_SCAN_ADV |
                                  AE_EVT_TYPE_SCAN_RSP |
                                  pScanState->directed;
          }

          // check for auxPtr
          if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_AUXPTR ) )
          {
            // expect chain next; indicate incomplete with more to come
            extAdvRpt->evtType |= AE_EVT_TYPE_INCOMPLETE_MORE_TO_COME;
          }
          else // no auxPtr
          {
            // reset the state
            pScanState->state = WAIT_FOR_ADV_EXT_IND;

            // indicate complete
            extAdvRpt->evtType |= AE_EVT_TYPE_COMPLETE;

            // clear flag
            pScanState->lastScanRsp = FALSE;
          }
          if (!( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_ADVA ) ))
          {
            //update the adv address
            MAP_osal_memcpy( &extAdvRpt->addr, &pScanState->advAddr, B_ADDR_LEN );
            extAdvRpt->addrType = MASK_ID_ADDRTYPE(pHdr >> LL_ADV_PDU_HDR_TXADDR);
          }
          break;

        case WAIT_FOR_AUX_SCAN_RSP:
          // check for auxPtr
          if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_AUXPTR ) )
          {
            // have an auxPtr, so expect chain next
            pScanState->state = WAIT_FOR_AUX_CHAIN_IND;

            // indicate incomplete, with more to come
            extAdvRpt->evtType |= AE_EVT_TYPE_SCAN_ADV |
                                  AE_EVT_TYPE_SCAN_RSP |
                                  AE_EVT_TYPE_INCOMPLETE_MORE_TO_COME |
                                  pScanState->directed;

            // last packet scan response
            pScanState->lastScanRsp = TRUE;
            if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_ADVA ) )
            {
              //save the adv address for the AUX_CHAIN_IND
              MAP_osal_memcpy( &pScanState->advAddr, &extAdvRpt->addr, B_ADDR_LEN );
            }
          }
          else // no auxPtr
          {
            // // reset the state
            pScanState->state = WAIT_FOR_ADV_EXT_IND;

            // so indicate complete
            extAdvRpt->evtType |= AE_EVT_TYPE_SCAN_ADV |
                                  AE_EVT_TYPE_SCAN_RSP |
                                  AE_EVT_TYPE_COMPLETE |
                                  pScanState->directed;
          }
          break;

        // unexpected state
        default:
        {
          // back to ADV_EXT_IND
          pScanState->state = WAIT_FOR_ADV_EXT_IND;

          // check if this is a chain from the scan response
          if ( pScanState->lastScanRsp )
          {
            // indicate a Scan Response
            extAdvRpt->evtType |= AE_EVT_TYPE_SCAN_ADV |
                                  AE_EVT_TYPE_SCAN_RSP;
          }

          // indicate incomplete
          extAdvRpt->evtType |= AE_EVT_TYPE_INCOMPLETE_NO_MORE_TO_COME;

          sendReport = FALSE;
        }
      }
    }
  }
  else // this is a primary channel: ADV_EXT_IND
  {
    // set the primary PHY
    // Note: The Rx status returns 1M/2M/S8/S2 as 0..3, so +1 to match the
    //       parameter, except for S2.
    extAdvRpt->primPhy = *pPkt++;
    extAdvRpt->primPhy += (extAdvRpt->primPhy == BLE5_S2_PHY) ? 0 : 1;

    // set secondary PHY
    extAdvRpt->secPhy = LL_PHY_NONE;

    // check for auxPtr
    if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_AUXPTR ) )
    {
      // send the report only after receiving the aux adv indication
      // Note: Per Vol 6, Part B, Section 4.4.3.5 - Advertising Reports
      sendReport = FALSE;

      // create the state entry according to the SID
      pScanState = llManageExtScanStateList(EXT_SCAN_STATE_LIST_ADD,extAdvRpt->advSid,
                                            auxTimeOffset + EXT_SCAN_STATE_RCV_AUX_GUARD_TIME);
      if (pScanState == NULL)
      {
        // SID already exist - Drop packet
        // enable adiStatus.state to be reset
        CLR_EXT_SCAN_FILTER_CFG_EXCLUSIVE_SID( extScanParam.extFltrCfg );
      }
      else
      {
        pScanState->lastScanRsp = FALSE;
        pScanState->lastPrimPhy = extAdvRpt->primPhy;
        // so indicate incomplete, with more to come
        extAdvRpt->evtType |= AE_EVT_TYPE_INCOMPLETE_MORE_TO_COME;
        if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_ADVA ) )
        {
          //save the adv address for the AUX_ADV_IND
          MAP_osal_memcpy( &pScanState->advAddr, &extAdvRpt->addr, B_ADDR_LEN );
        }
        else
        {
          MAP_osal_memset( &pScanState->advAddr, 0, B_ADDR_LEN );
        }
        // prevent adiStatus.state from being reset
        SET_EXT_SCAN_FILTER_CFG_EXCLUSIVE_SID( extScanParam.extFltrCfg );
      }
    }
    else // no auxPtr
    {
      // only expect another ADV_EXT_IND, so indicate complete
      extAdvRpt->evtType |= AE_EVT_TYPE_COMPLETE;
    }
  }
  return sendReport;
}
#endif

/*******************************************************************************
 * @fn          llProcessExtScanRxFIFO
 *
 * @brief       This function is used to process received advertising packets
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
void llProcessExtScanRxFIFO( void )
{
  aeExtAdvRptEvt_t *extAdvRpt = NULL;
#ifdef USE_RCL
  RCL_Buffer_DataEntry *pDataEntry;
#else
  dataEntry_t      *pDataEntry;
#endif //USE_RCL
#ifdef USE_AE
  uint8             extHdrFlgs = 0;
#endif //USE_AE
#if defined (USE_PERIODIC_SCAN) || (USE_AE)
  uint8            *pSyncInfo = NULL;
  uint8             secPhy = 0xFF;
#endif //USE_PERIODIC_SCAN || USE_AE
  uint8            *pPkt;
  uint8             pHdr;
  uint8             dataLen;
  uint16            evtType;
  uint8             sendReport;
  uint8             checkSyncInfo = FALSE;
  uint8             channelIndex = 0xFF;
  uint8             ignoreBit = 0;
  // get pointer to packet
#ifdef USE_RCL
  while ((pDataEntry = RCL_MultiBuffer_RxEntry_get(&extScanParam.rxBuffers, &scanDataQueue)) != NULL)
  {
#else
  pDataEntry = (dataEntry_t *)MAP_RFHAL_GetNextDataEntry( extScanParam.pRXQ );

  if ( (pDataEntry == NULL) || (pDataEntry->status != DATASTAT_FINISHED) )
  {
    return;
  }
#endif
  if ((extScanInfo == NULL) || (extScanInfo->scanMode == LL_SCAN_STOP))
  {
#ifdef USE_RCL
    llClearRxDataEntry(&extScanParam.rxBuffers, &scanDataQueue);
#else
    MAP_RFHAL_NextDataEntryDone( extScanParam.pRXQ );
#endif
    return;
  }
  // get pointer to BLE PDU packet
#ifdef USE_RCL
  pPkt = (uint8 *)(pDataEntry->data + (pDataEntry->numPad - 1));
#else
  pPkt = (uint8 *)(pDataEntry + 1);
#endif
  // get packet header
  pHdr = *pPkt++;
  // get data length
  if (LL_LEGACY_ADV_PDU( pHdr ))
  {
    // check that the table is full only in case of receive legacy adv,
    // duplicate filtering is enable and not using accept list
    if ((extScanInfo->pEnable->dupFiltering != LL_FILTER_REPORTS_DISABLE) &&
       ((extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_ANY_ADV_PKTS) ||
        (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_ANY_ADV_PKTS_EXT)) &&
        (alTableScan->numAlEntries == alTableScan->numBusyAlEntries))
    {
#ifdef USE_RCL
      llClearRxDataEntry(&extScanParam.rxBuffers, &scanDataQueue);
#else
      MAP_RFHAL_NextDataEntryDone( extScanParam.pRXQ );
#endif
      return;
    }
    dataLen = *pPkt++ - B_ADDR_LEN;

    // check if directed
    // Note: All advertisement payload begins with AdvA.
    // Note: For Directed, the only payload is peer and own address.
    dataLen -= (LL_ADV_DIRECT_IND_PDU(pHdr) ? B_ADDR_LEN : 0);
    evtType = LL_ADV_PDU_TYPE(pHdr);
  }
#ifdef USE_AE
  else
  {
    // Packet Length minus (Extended Header Length + 1)
    dataLen = *pPkt - (GET_EXT_HDR_LEN(*(pPkt+1)) + 1);
    // advance packet pointer to extended header information field
    pPkt++;
    // init event type based on advertising mode
    evtType = GET_ADV_MODE(*pPkt);
    // get extended header flags, if any
    if ( GET_EXT_HDR_LEN(*pPkt++) )
    {
      extHdrFlgs = *pPkt++;
    }
  }
#endif
  sendReport = (((pBleEvtMask[LE_EVT_INDEX_ADV_REPORT] & LE_EVT_MASK_ADV_REPORT) ||
                 (pBleEvtMask[LE_EVT_INDEX_EXTENDED_ADV_REPORT] & LE_EVT_MASK_EXTENDED_ADV_REPORT)) &&
                 (MAP_llCheckCBack(LL_CBACK_EXT_ADV_REPORT)));
#if defined (USE_AE) && defined (USE_PERIODIC_SCAN)
  checkSyncInfo = ((!LL_LEGACY_ADV_PDU( pHdr )) &&
                   (!TST_AE_PROPS_FLAG( evtType, AE_PROPS_CONN_ADV)) &&
                   (!TST_AE_PROPS_FLAG( evtType, AE_PROPS_SCAN_ADV)) &&
                   (TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_ADI)) &&
                   (((llPeriodicScan.createSync != NULL) &&
                   (llPeriodicScan.createSync->state == PERIODIC_SCAN_STATE_SYNCING_PENDING)) ||
                   (llPeriodicScan.scanNumActive > 0)));
#endif
  // check if the report was set by the Host and there's a registered callback
  // also check if we are in a periodic process
  if (sendReport || checkSyncInfo)
  {
    // malloc adv report
    // Use allocLimited to avoid overflowing the heap...
    extAdvRpt = MAP_osal_mem_allocLimited( sizeof(aeExtAdvRptEvt_t) );
    if ((extAdvRpt == NULL) &&
        (MAP_llCheckCBack(LL_CBACK_EXT_ADV_REPORT)) &&
        (pBleEvtMask[LE_EVT_INDEX_EXTENDED_ADV_REPORT] & LE_EVT_MASK_EXTENDED_ADV_REPORT))
    {
      // out of memory
      MAP_llExtAdvCBack( LL_CBACK_OUT_OF_MEMORY, NULL );
    }
  }
  // check if we got the memory and there's a registered callback
  if ( extAdvRpt != NULL)
  {
    // init base fields
    extAdvRpt->subCode = AE_SCAN_HCI_BLE_EXTENDED_ADV_REPORT_EVENT;
    extAdvRpt->numRpts = 1; // always one
    extAdvRpt->pData   = NULL;

    // check if legacy advertisement PDU
    if ( LL_LEGACY_ADV_PDU( pHdr ) )
    {
      switch( evtType )
      {
        case LL_PKT_TYPE_ADV_IND:
          extAdvRpt->evtType = AE_EXT_ADV_RPT_EVT_TYPE_ADV_IND;
          break;
        case LL_PKT_TYPE_ADV_DIRECT_IND:
          extAdvRpt->evtType = AE_EXT_ADV_RPT_EVT_TYPE_DIRECT_IND;
          break;
        case LL_PKT_TYPE_ADV_NONCONN_IND:
          extAdvRpt->evtType = AE_EXT_ADV_RPT_EVT_TYPE_NONCONN_IND;
          break;
        case LL_PKT_TYPE_SCAN_RSP:
          if ( extScanInfo->lastLegacyAdv == LL_PKT_TYPE_ADV_IND )
          {
            extAdvRpt->evtType = AE_EXT_ADV_RPT_EVT_TYPE_SCAN_RSP_ADV_IND;
          }
          else if ( extScanInfo->lastLegacyAdv == LL_PKT_TYPE_ADV_SCAN_IND )
          {
            extAdvRpt->evtType = AE_EXT_ADV_RPT_EVT_TYPE_SCAN_RSP_ADV_SCAN_IND;
          }
          else // unknown Adv, to which Scan Request was sent
          {
            // use this special case event type to indicate the advertisement,
            // to which the Scan Request was sent, is unknown
            extAdvRpt->evtType = AE_EXT_ADV_RPT_EVT_TYPE_SCAN_RSP;
          }
          break;
        case LL_PKT_TYPE_ADV_SCAN_IND:
          extAdvRpt->evtType = AE_EXT_ADV_RPT_EVT_TYPE_SCAN_IND;
          break;
        default:
          // Sanity Check
          LL_ASSERT( FALSE );
          break;
      }

      // save packet PDU
      extScanInfo->lastLegacyAdv = LL_ADV_PDU_TYPE(pHdr);

      // copy peer's address and set type
      MAP_osal_memcpy( extAdvRpt->addr, pPkt, B_ADDR_LEN );
      extAdvRpt->addrType = MASK_ID_ADDRTYPE(pHdr >> LL_ADV_PDU_HDR_TXADDR);
      pPkt += B_ADDR_LEN;

      // indicate complete
      // Note: Could be changed below if unable to allocate memory for data!
      extAdvRpt->evtType |= AE_EVT_TYPE_COMPLETE;

      // check if directed
      if ( LL_ADV_DIRECT_IND_PDU( pHdr ) )
      {
        // copy own address and set type
        MAP_osal_memcpy( extAdvRpt->directAddr, pPkt, B_ADDR_LEN );
        extAdvRpt->directAddrType = MASK_ID_ADDRTYPE(pHdr >> LL_ADV_PDU_HDR_RXADDR);
        pPkt += B_ADDR_LEN;

        // no data allowed
        extAdvRpt->pData   = NULL;
        extAdvRpt->dataLen = 0;
      }
      else // !directed
      {
        // clear own address and set type
        MAP_osal_memset( extAdvRpt->directAddr, 0, B_ADDR_LEN );
        extAdvRpt->directAddrType = AE_NO_ADDRESS_PROVIDED;

        // set data length, if any
        extAdvRpt->dataLen = dataLen;

        // copy the data, if there is any
        if ( dataLen )
        {
          // get a buffer for data
          // Use allocLimited to avoid overflowing the heap...
          extAdvRpt->pData = MAP_osal_mem_allocLimited( dataLen );

          // check if we got the buffer
          if ( extAdvRpt->pData )
          {
            MAP_osal_memcpy( extAdvRpt->pData, pPkt, dataLen );
          }
          else // data is lost
          {
            // clear the data length
            extAdvRpt->dataLen = 0;
          }

          pPkt += dataLen;
        }
      }

#ifndef CC23X0
      // get the RSSI
      extAdvRpt->rssi = (int8)*pPkt++;

      // adjust RSSI based on Rx RF path compensation
      extAdvRpt->rssi += pRfPathComp->rfRxPathCompVal;

      // get ignore bit
      ignoreBit = GET_IGNORE_BIT(*pPkt);
      // get the channel index
      channelIndex = GET_CHANNEL_IDX(*pPkt++);
#else
      // get the RSSI
      // TODO: This MACRO should be tested once the RCL will support appending the RSSI/Channel
      extAdvRpt->rssi = RCL_BLE5_getRxRssi(pDataEntry);
      // Get the channel index. This is saved by the RCL in "pad0"
      channelIndex = GET_CHANNEL_IDX(RCL_BLE5_getRxChannel(pDataEntry));
#endif
      // set the PHYs
      extAdvRpt->primPhy = BLE5_1M_PHY + 1;
      extAdvRpt->secPhy  = LL_PHY_NONE;

      // set ADI
      extAdvRpt->advSid = EXT_SCAN_NO_ADI;
      extAdvRpt->advDid = EXT_SCAN_NO_DID;

      // set Tx Power
      // Note: None available for legacy.
      extAdvRpt->txPower = AE_TX_POWER_NO_PREFERENCE;

      // periodic not supported in legacy
      extAdvRpt->periodicAdvInt = 0;

      // check if filtering is enabled
      if ( (extScanInfo->pEnable->dupFiltering == LL_FILTER_REPORTS_ENABLE) ||
           (extScanInfo->pEnable->dupFiltering == LL_FILTER_REPORTS_RESET_EACH_SCAN_PERIOD) )
      {
        // check the extended scan accept list policy
        if ( (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_ANY_ADV_PKTS) ||
             (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_ANY_ADV_PKTS_EXT) )
        {
          // check if the address is not already in the Standard AL
          // Note: This is done to avoid adding the same address twice, which can
          //       occur when Privacy 1.2 is being used with rpaMode.
          // check if this address was already added to the Extended AL
          // Note: When the rpaMode is used, the PHY will search the AL for the RPA
          //       when the filter policy is ANY. So a RPA that doesn't resolve
          //       will still have to be added to the Extended AL so that packet
          //       can be received. Once received, it's placed in the AL here, so
          //       order to not waste Extended AL space, it should be removed from
          //       the extended AL.
          if ( MAP_AL_SetAlIgnore( alTableScan,
                                   extAdvRpt->addr,
                                   extAdvRpt->addrType ) == LL_STATUS_ERROR_AL_ENTRY_NOT_FOUND )
          {
            // add address and address type to accept list and set the corresponding
            // denylist entry index
            if ( MAP_AL_AddEntry( alTableScan, extAdvRpt->addr, extAdvRpt->addrType, BLE_IGNORE_AL_ENTRY ) == LL_STATUS_SUCCESS )
            {
#ifdef USE_RCL
              uint8 alScanIdx = 0;

              // Find the new entry index
              alScanIdx = MAP_AL_FindEntry( alTableScan,
                                            extAdvRpt->addr,
                                            extAdvRpt->addrType);

              // Update the RCL Accept List
              MAP_llRclPrepareAndUpdateAlEntry( extScanParam.filterList, alTableScan->pAlEntries[alScanIdx].alFlags, extAdvRpt->addr, alScanIdx);
#endif
            }
          }
        }
        else if ( (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST) ||
                  (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST_EXT) )
        {
          // find the address and address type in the whilte list and set the
          // black list bit to the corresponding index
          // Note: We should only be here if a Adv packet was received and its
          //       address and address type were in the accept list!
          (void)MAP_AL_SetAlIgnore( alTable,
                                    extAdvRpt->addr,
                                    extAdvRpt->addrType );
        }
      }
    }
#ifdef USE_AE
    else //!legacy
    {
      sendReport = MAP_llSetExtendedAdvReport(extAdvRpt,pPkt,evtType,extHdrFlgs,
                                              pHdr,dataLen,&pSyncInfo,&secPhy,&channelIndex);
      ignoreBit = GET_IGNORE_BIT( channelIndex );
      channelIndex = GET_CHANNEL_IDX( channelIndex );
    }
#endif
    // check if we couldn't allocate memory for data, if there was any
    if ( sendReport && dataLen && !extAdvRpt->dataLen )
    {
      // indicate we're incomplete with no more data
      extAdvRpt->evtType &= AE_EVT_TYPE_COMPLETE_MASK;
      extAdvRpt->evtType |= AE_EVT_TYPE_INCOMPLETE_NO_MORE_TO_COME;
    }

    // only if address resolution is enabled
    if ( privInfo.addrResolution )
    {
      // check if the AdvA is an RPA
      if ( MAP_LL_PRIV_IsRPA( extAdvRpt->addrType, extAdvRpt->addr ) )
      {
        // try to resolve RPA to find ID address
        uint8 rlIndex = MAP_LL_PRIV_IsResolvable( extAdvRpt->addr, resolvingList );

        // see if the Peer Address is resolvable
        if (( rlIndex != INVALID_RESOLVE_LIST_INDEX ) &&
            ( !MAP_LL_PRIV_IsZeroIRK( resolvingList[rlIndex].IRK ) ))
        {
          // Copy the advertiser RPA to the RL
          MAP_osal_memcpy( resolvingList[rlIndex].RPA,
                           extAdvRpt->addr,
                           B_ADDR_LEN );

          // Add to Ext AL and update Ignore Bit
          ignoreBit = MAP_llAddExtAlAndSetIgnBit(extAdvRpt, ignoreBit);

          // copy ID address and address type
          MAP_osal_memcpy( extAdvRpt->addr,
                           resolvingList[rlIndex].idAddr,
                           B_ADDR_LEN );

          extAdvRpt->addrType = resolvingList[rlIndex].idAddrType | LL_DEV_ADDR_TYPE_ID_MASK;
        }
      }

      // check if own address InitA is an RPA (Directed only)
      if ( MAP_LL_PRIV_IsRPA( extAdvRpt->directAddrType, extAdvRpt->directAddr ) )
      {
        // verify own address RPA resolves to our Identity address
        if ( MAP_LL_PRIV_ResolveRPA( extAdvRpt->directAddr, resolvingList[LOCAL_RL_INDEX].IRK ) )
        {
          // copy ID address and address type
          MAP_osal_memcpy( extAdvRpt->directAddr,
                           resolvingList[LOCAL_RL_INDEX].idAddr,
                           B_ADDR_LEN );

          extAdvRpt->directAddrType = resolvingList[LOCAL_RL_INDEX].idAddrType | LL_DEV_ADDR_TYPE_ID_MASK;
        }
        else // RPA did not resolve
        {
#ifndef QUAL_TEST
          // so indicate this in the report
          extAdvRpt->directAddrType = AE_EXT_ADV_RPT_DIR_ADDR_TYPE_UNRESOLVED_RPA;
#else
          if ((extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_ANY_ADV_PKTS_EXT) ||
              (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST_EXT))
          {
            extAdvRpt->directAddrType = AE_EXT_ADV_RPT_DIR_ADDR_TYPE_UNRESOLVED_RPA;
          }
          else
          {
            sendReport = FALSE;
          }
#endif
        }
      }
    }

    // Check the ignoreBit of the RxEntry and flush/keep accordingly
    if (MAP_llFlushIgnoredRxEntry(ignoreBit))
    {
      // the RxEntry is flushed, free report allocations
      if ( extAdvRpt->pData )
      {
        MAP_osal_mem_free( extAdvRpt->pData );
      }
      MAP_osal_mem_free( extAdvRpt );
      return;
    }

    // check the advertiser sync info
#ifdef USE_PERIODIC_SCAN
    if ((pSyncInfo != NULL) && (checkSyncInfo) && (secPhy != 0xFF))
    {
      //check that we are waiting for that sync info advertiser
      MAP_llProcessPeriodicScanSyncInfo(pSyncInfo,extAdvRpt,extScanOutput.timeStamp,secPhy);
    }
#endif
    //Ensure that we have to send the report to the host
    if (((extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST) ||
        ((privInfo.addrResolution) &&
         (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST_EXT))) &&
         (MAP_AL_FindEntry(alTable, extAdvRpt->addr, extAdvRpt->addrType & LL_DEV_ADDR_TYPE_MASK) == alTable->numAlEntries))
    {
      //advertiser does not exist in the accept list
      sendReport = FALSE;
    }
    if ( sendReport )
    {
      if (llConfigTable.userCfgPtr->advReportIncChannel)
      {
        // represent the channel index on the 6 MSB of secPhy (the 2 LSB represent the PHY type)
        extAdvRpt->secPhy |= (channelIndex << 2);
      }

      // invoke the callback for this event, if not masked by the Host
      MAP_llExtAdvCBack( LL_CBACK_EXT_ADV_REPORT, (void *)extAdvRpt );
    }
    else
    {
      // free report allocations
      if ( extAdvRpt->pData )
      {
        MAP_osal_mem_free( extAdvRpt->pData );
      }
      MAP_osal_mem_free( extAdvRpt );
    }
  }
#ifdef USE_RCL
  } // while
#endif
#ifdef USE_RCL
  llClearRxDataEntry(&extScanParam.rxBuffers, &scanDataQueue);
#else
  // in all cases, mark the RX queue data entry as free
  // Note: Even if there isn't any heap to copy to, this packet is considered
  //       lost, and the queue entry is marked free for radio use.
  MAP_RFHAL_NextDataEntryDone( extScanParam.pRXQ );
#endif

  return;
}

#ifdef USE_PERIODIC_SCAN
/*******************************************************************************
 * @fn          llProcessPeriodicScanRxFIFO
 *
 * @brief       This function used to process received periodic asvertising packet
 *
 * @design      /ref did_286039104
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llProcessPeriodicScanRxFIFO( void )
{
  dataEntry_t      *pDataEntry;
  uint8            *pPkt;
  uint8             pHdr;
  uint8             dataLen;
  uint8             pktLen;
  uint8             extHdrLen;
  uint8             extHdrFlgs = 0;
  uint16            evtType;
  int8              txPower = AE_TX_POWER_NO_PREFERENCE;
  uint8             cteType = LL_CTE_TYPE_NONE;
  uint8             cteLen = 0;
  uint8             dataStatus = PERIODIC_SCAN_REPORT_DATA_COMPLETE;
  uint8             rssi;
  uint32            timeStamp;
  llPeriodicScanSet_t *pPeriodicScan = llPeriodicScan.currentScan;

  // get pointer to packet
  if (pPeriodicScan == NULL)
  {
    return;
  }
  pDataEntry = (dataEntry_t *)MAP_RFHAL_GetNextDataEntry( pPeriodicScan->rfParam.pRXQ );

  // check if it is valid, otherwise, nothing to do here
  if ( (pDataEntry == NULL) || (pDataEntry->status != DATASTAT_FINISHED) )
  {
    return;
  }
  // count the rx
  pPeriodicScan->rxCount++;

  // get pointer to BLE PDU packet
  pPkt = (uint8 *)(pDataEntry + 1);

  // get packet header
  pHdr = *pPkt++;
  // handle the legacy packet in llProcessExtScanRxFIFO function.
  if (LL_LEGACY_ADV_PDU( pHdr ))
  {
    MAP_RFHAL_NextDataEntryDone( pPeriodicScan->rfParam.pRXQ );
    return;
  }
  // get packet length
  pktLen = *pPkt++;
  // init event type based on advertising mode
  evtType = GET_ADV_MODE(*pPkt);
  // periodic adv mode is only none connectable and none scannable
  if ((TST_AE_PROPS_FLAG( evtType, AE_PROPS_CONN_ADV)) ||
      (TST_AE_PROPS_FLAG( evtType, AE_PROPS_SCAN_ADV)))
  {
    MAP_RFHAL_NextDataEntryDone( pPeriodicScan->rfParam.pRXQ );
    return;
  }
  extHdrLen = GET_EXT_HDR_LEN(*pPkt++);
  // get extended header flags, if any
  if ( extHdrLen > 0 )
  {
    uint8 countHdrSize = 1;

    extHdrFlgs = *pPkt++;
    // check for CTE info
    if ( TST_EXTHDR_FLAG(extHdrFlgs, EXTHDR_FLAG_CTEINFO) )
    {
      //get CTE type
      cteType = (*pPkt & LL_CTE_INFO_TYPE_MASK) >> LL_CTE_INFO_TYPE_OFFSET;
      cteLen = *pPkt & LL_CTE_INFO_TIME_MASK;
      pPkt++;
      countHdrSize++;
    }
    //check for AUX ptr
    if ( TST_EXTHDR_FLAG(extHdrFlgs, EXTHDR_FLAG_AUXPTR) )
    {
      dataStatus = PERIODIC_SCAN_REPORT_DATA_INCOMPLETE_MORE;
      pPkt += EXTHDR_FLAG_AUXPTR_SIZE;
      countHdrSize += EXTHDR_FLAG_AUXPTR_SIZE;
    }
    //check for Tx Power
    if ( TST_EXTHDR_FLAG(extHdrFlgs, EXTHDR_FLAG_TXPWR) )
    {
      txPower = (int8)*pPkt++;
      countHdrSize++;
    }
    //check for ACAD
    if (extHdrLen > countHdrSize)
    {
      // check for channel map update indication
      if ((*pPkt == EXTHDR_ACAD_CHANMAP_UPDATE_SIZE) &&
          (*(pPkt+ 1) == EXTHDR_ACAD_CHANMAP_UPDATE_TYPE))
      {
        // start the channel map update procedure
        llSetPeriodicScanChmapUpdate(pPeriodicScan,TRUE,pPkt + 2,*(uint16 *)(pPkt + 7));
      }
      pPkt += (extHdrLen - countHdrSize);
    }
  }
  // in case we are in syncing process - check the CTE type
  // maybe we should ignore this advertiser
  if ((llPeriodicScan.createSync != NULL) &&
      (llPeriodicScan.createSync == pPeriodicScan) &&
      (llPeriodicScan.createSync->state == PERIODIC_SCAN_STATE_SYNCING_ACTIVE) &&
      (llPeriodicScan.createSync->syncCmd.cteType != 0))
  {
    if (((cteType == LL_CTE_TYPE_AOA) && (GET_PERIODIC_CTE_TYPE_SYNC_NO_AOA(llPeriodicScan.createSync->syncCmd.cteType))) ||
       ((cteType == LL_CTE_TYPE_AOD_1US) && (GET_PERIODIC_CTE_TYPE_SYNC_NO_1U_AOD(llPeriodicScan.createSync->syncCmd.cteType))) ||
       ((cteType == LL_CTE_TYPE_AOD_2US) && (GET_PERIODIC_CTE_TYPE_SYNC_NO_2U_AOD(llPeriodicScan.createSync->syncCmd.cteType))) ||
       ((cteType == LL_CTE_TYPE_NONE) && (GET_PERIODIC_CTE_TYPE_SYNC_ONLY_CTE(llPeriodicScan.createSync->syncCmd.cteType))))
    {
      //we should ignore this advertiser
      // in case of using accept list - continue in searching for other advertiser
      if (GET_PERIODIC_SCAN_OPTIONS_LIST_USE(llPeriodicScan.createSync->syncCmd.options))
      {
        pPeriodicScan->terminate = LL_STATUS_ERROR_UNACCEPTABLE_CONN_PARAMETERS;
      }
      // in case of NOT using accept list - terminate the syncing procedure in post process
      else
      {
        pPeriodicScan->terminate = LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE;
      }
      MAP_RFHAL_NextDataEntryDone( pPeriodicScan->rfParam.pRXQ );
      return;
    }
  }
  // get data length: Packet Length minus (Extended Header Length + 1)
  dataLen = pktLen - (extHdrLen + 1);
  // get the time stamp
  timeStamp = *(uint32 *)(pPkt + dataLen + 6);
  // check that the time stamp is valid
  if (timeStamp != 0xFFFFFFFF)
  {
    if (pPeriodicScan->rxCount == 1)
    {
      uint32 timeExpect = pPeriodicScan->startTime + LL_JITTER_CORRECTION + LL_RX_RAMP_OVERHEAD;
      uint16 timeJitter = (pPeriodicScan->phy > BLE5_2M_PHY)?RAT_TICKS_IN_16MS:
                          (pPeriodicScan->phy == BLE5_2M_PHY)?RAT_TICKS_IN_1MS:RAT_TICKS_IN_2MS;
      uint16 timeGlich = MAX(PERIODIC_SCAN_TIMESTAMP_DRIFT_MIN_GLICH,
                            (PERIODIC_SCAN_DRIFT_GET_MAX_GLICH(pPeriodicScan->syncCmd.skip,pPeriodicScan->interval)));
      uint32 driftTime;
      int8   driftDirection;

      // find the drift time and direction
      if (MAP_llTimeCompare(timeStamp , timeExpect))
      {
        driftTime = MAP_llTimeDelta(timeStamp,timeExpect);
        driftDirection = PERIODIC_SCAN_DRIFT_DIRECTION_POSITIVE;
      }
      else
      {
        driftTime = MAP_llTimeDelta(timeExpect,timeStamp);
        driftDirection = PERIODIC_SCAN_DRIFT_DIRECTION_NEGATIVE;
      }

       // check if this is the first rx
      if (driftTime < timeJitter)
      {
        // check time glich
        if ((driftTime < timeGlich) || (pPeriodicScan->driftLearnCounter == 0))
        {
          // find the drift factor
          if (pPeriodicScan->driftLearnCounter <= PERIODIC_SCAN_DRIFT_LEARNING_MAX_NUM)
          {
            if (pPeriodicScan->driftLearnCounter > 0)
            {
              // claculate the drifts sum
              if (driftDirection == PERIODIC_SCAN_DRIFT_DIRECTION_NEGATIVE)
              {
                pPeriodicScan->driftFactor -= ((driftTime) / (pPeriodicScan->numMissed + 1));
              }
              else
              {
                pPeriodicScan->driftFactor += ((driftTime) / (pPeriodicScan->numMissed + 1));
              }
            }

            if (pPeriodicScan->driftLearnCounter == PERIODIC_SCAN_DRIFT_LEARNING_MAX_NUM)
            {
              driftDirection = (pPeriodicScan->driftFactor >= 0)?PERIODIC_SCAN_DRIFT_DIRECTION_POSITIVE:
                                                                 PERIODIC_SCAN_DRIFT_DIRECTION_NEGATIVE;
              // calculate the average drift
              pPeriodicScan->driftFactor = (pPeriodicScan->driftFactor / PERIODIC_SCAN_DRIFT_LEARNING_MAX_NUM) +
                                         (((pPeriodicScan->driftFactor % PERIODIC_SCAN_DRIFT_LEARNING_MAX_NUM) == 0)?0:driftDirection);
            }
            pPeriodicScan->driftLearnCounter++;
          }
          else
          {
            // check if we might do drift learning again
            if (driftTime > PERIODIC_SCAN_DRIFT_LEARNING_THRESHOLD)
            {
              // restart the drift learning
              pPeriodicScan->driftLearnCounter = 0;
            }
          }
          // update the exact periodic start time according to the first rx time with correction
          pPeriodicScan->startTime = timeStamp - (LL_JITTER_CORRECTION + LL_RX_RAMP_OVERHEAD);
        }
      }
      else // we missed the first rx
      {
        pPeriodicScan->rxCount = 0;
        MAP_RFHAL_NextDataEntryDone( pPeriodicScan->rfParam.pRXQ );
        return;
      }
    }
    //update the ota
    if (dataStatus == PERIODIC_SCAN_REPORT_DATA_COMPLETE)
    {
      // add total time from first packet start time until the last packet start time
      pPeriodicScan->totalOtaTime = (timeStamp - pPeriodicScan->startTime);
      // add the last packet CTE length and ota time
      pPeriodicScan->totalOtaTime += (US_TO_RAT_TICKS((cteLen * 8) +
                                                    (MAP_llOctets2Time((pPeriodicScan->rfCmd.phyMode & 0x03),
                                                    (pPeriodicScan->rfCmd.phyMode>>2) & 0x01,
                                                     pktLen,MIC_NOT_ENABLED))) + PERIODIC_SCAN_MARGIN_TIME_RAT_TICKS);
      pPeriodicScan->totalOtaTime = MAX(pPeriodicScan->totalOtaTime, PERIODIC_SCAN_MAX_MARGIN_TIME_RAT_TICKS);
    }
    // check if reported was enable by host
    if ((pPeriodicScan != llPeriodicScan.createSync) && (pPeriodicScan->reportEnable))
    {
      rssi = (RSSI_SUFFIX_PRESENT() && (llPeriodicScan.rfOutput.lastRssi != LL_RF_RSSI_UNDEFINED))?llPeriodicScan.rfOutput.lastRssi:LL_RF_RSSI_INVALID;
      // send report to host
      HCI_PeriodicAdvReportEvent( pPeriodicScan->handle,
                                txPower,
                                LL_CHECK_LAST_RSSI(rssi),
                                cteType,
                                dataStatus,
                                dataLen,
                                pPkt );
    }
  }
  // release the rx buffer
  MAP_RFHAL_NextDataEntryDone( pPeriodicScan->rfParam.pRXQ );
}

/*******************************************************************************
 * @fn          llSelectScanOrPeriodicScan
 *
 * @brief       This routine is used for thr scheduler to select between Extended
 *              Scan and Periodic Scan.
 *
 * @design      /ref did_286039104
 *
 * input parameters
 *
 * @param       pPeriodicScan - periodic scan candidate.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE - Extended Scan was selected; FALSE - Periodic Scan was selected
 */

uint8 llSelectScanOrPeriodicScan( llPeriodicScanSet_t *pPeriodicScan )
{
  uint32 periodicScanInterval = (pPeriodicScan->interval * (pPeriodicScan->syncCmd.skip + 1) * RAT_TICKS_IN_1_25MS);
  uint32 extScanInterval = (extScanInfo->pScanParam->extScanParam[extScanIndex].scanInterval * RAT_TICKS_IN_625US);

  /////////////////////////////////////////////
  // check for collision
  /////////////////////////////////////////////
  //check if the second start before the first
  if (MAP_llTimeCompare(extScanInfo->scanStartTime , pPeriodicScan->startTime))
  {
    // check for collision between the two
    if (MAP_llTimeCompare(extScanInfo->scanStartTime , pPeriodicScan->startTime + pPeriodicScan->totalOtaTime))
    {
      // no collision - choose the periodic scan
      return FALSE;
    }
  }
  else // first starts before the second
  {
    // check for collision between the two
    if (MAP_llTimeCompare(pPeriodicScan->startTime , extScanInfo->scanStartTime +
                         (extScanInfo->pScanParam->extScanParam[extScanIndex].scanWindow * RAT_TICKS_IN_625US)))
    {
      // no collision - choose the ext scan
      return TRUE;
    }
  }
  /////////////////////////////////////////////
  // check periodic priority parameter
  /////////////////////////////////////////////
  if (pPeriodicScan->intPriority > extScanPriority)
  {
    return FALSE;
  }
  /////////////////////////////////////////////
  // check timeout parameter
  /////////////////////////////////////////////
  if (pPeriodicScan->numMissed >= PERIODIC_SCAN_MISSED_LIMIT(pPeriodicScan->syncCmd.timeout,pPeriodicScan->interval))
  {
    return FALSE;
  }
  /////////////////////////////////////////////
  // check ext scan priority parameter
  /////////////////////////////////////////////
  if (extScanPriority > pPeriodicScan->intPriority)
  {
    return TRUE;
  }
  /////////////////////////////////////////////
  // check priority parameter
  /////////////////////////////////////////////
  if (extScanInfo->priority > pPeriodicScan->priority)
  {
    return TRUE;
  }
  if (pPeriodicScan->priority > extScanInfo->priority)
  {
    return FALSE;
  }
  /////////////////////////////////////////////
  // check interval parameter
  /////////////////////////////////////////////
  if (periodicScanInterval > extScanInterval)
  {
    if (periodicScanInterval <= extScanInterval * extScanNumMissed)
    {
      return TRUE;
    }
    else
    {
      return FALSE;
    }
  }
  if (extScanInterval > periodicScanInterval)
  {
    if (extScanInterval <= periodicScanInterval * pPeriodicScan->numMissed)
    {
      return FALSE;
    }
    else
    {
      return TRUE;
    }
  }
  /////////////////////////////////////////////
  // check missed packet parameter
  /////////////////////////////////////////////
  if (pPeriodicScan->numMissed > extScanNumMissed)
  {
    return FALSE;
  }
  else if (extScanNumMissed > pPeriodicScan->numMissed)
  {
    return TRUE;
  }
  /////////////////////////////////////////////
  // check start time parameter
  /////////////////////////////////////////////
  if (MAP_llTimeCompare(extScanInfo->scanStartTime , pPeriodicScan->startTime))
  {
    return FALSE;
  }
  else
  {
    return TRUE;
  }
}

/*******************************************************************************
 * @fn          llSelectPeriodicScan
 *
 * @brief       This routine is used to select Periodic Scan between 2 candidates.
 *
 * @design      /ref did_286039104
 *
 * input parameters
 *
 * @param       pPeriodicScan1 - first periodic scan candidate.
 * @param       pPeriodicScan2 - second periodic scan candidate.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to the selected periodic scan
 */

llPeriodicScanSet_t *llSelectPeriodicScan( llPeriodicScanSet_t *pPeriodicScan1, llPeriodicScanSet_t *pPeriodicScan2 )
{
  uint16 interval1 = (pPeriodicScan1->interval * (pPeriodicScan1->syncCmd.skip + 1));
  uint16 interval2 = (pPeriodicScan2->interval * (pPeriodicScan2->syncCmd.skip + 1));

  /////////////////////////////////////////////
  // check for collision
  /////////////////////////////////////////////
  //check if the second start before the first
  if (MAP_llTimeCompare(pPeriodicScan1->startTime , pPeriodicScan2->startTime))
  {
    // check for collision between the two
    if (MAP_llTimeCompare(pPeriodicScan1->startTime , pPeriodicScan2->startTime + pPeriodicScan2->totalOtaTime))
    {
      // no collision - choose the second
      return pPeriodicScan2;
    }
  }
  else // first starts before the second
  {
    // check for collision between the two
    if (MAP_llTimeCompare(pPeriodicScan2->startTime , pPeriodicScan1->startTime + pPeriodicScan1->totalOtaTime))
    {
      // no collision - choose the first
      return pPeriodicScan1;
    }
  }
  /////////////////////////////////////////////
  // check internal priority parameter
  /////////////////////////////////////////////
  if (pPeriodicScan1->intPriority > pPeriodicScan2->intPriority)
  {
    return pPeriodicScan1;
  }
  if (pPeriodicScan2->intPriority > pPeriodicScan1->intPriority)
  {
    return pPeriodicScan2;
  }
  /////////////////////////////////////////////
  // check timeout parameter
  /////////////////////////////////////////////
  if ((pPeriodicScan1->numMissed >= PERIODIC_SCAN_MISSED_LIMIT(pPeriodicScan1->syncCmd.timeout,pPeriodicScan1->interval)) &&
      (pPeriodicScan2->numMissed < PERIODIC_SCAN_MISSED_LIMIT(pPeriodicScan2->syncCmd.timeout,pPeriodicScan2->interval)))
  {
    return pPeriodicScan1;
  }
  if ((pPeriodicScan1->numMissed < PERIODIC_SCAN_MISSED_LIMIT(pPeriodicScan1->syncCmd.timeout,pPeriodicScan1->interval)) &&
      (pPeriodicScan2->numMissed >= PERIODIC_SCAN_MISSED_LIMIT(pPeriodicScan2->syncCmd.timeout,pPeriodicScan2->interval)))
  {
    return pPeriodicScan2;
  }
  /////////////////////////////////////////////
  // check priority parameter
  /////////////////////////////////////////////
  if (pPeriodicScan1->priority > pPeriodicScan2->priority)
  {
    return pPeriodicScan1;
  }
  if (pPeriodicScan2->priority > pPeriodicScan1->priority)
  {
    return pPeriodicScan2;
  }
  /////////////////////////////////////////////
  // check interval parameter
  /////////////////////////////////////////////
  if (interval1 > interval2)
  {
    if (interval1 <= interval2 * pPeriodicScan2->numMissed)
    {
      return pPeriodicScan2;
    }
    else
    {
      return pPeriodicScan1;
    }
  }
  if (interval2 > interval1)
  {
    if (interval2 <= interval1 * pPeriodicScan1->numMissed)
    {
      return pPeriodicScan1;
    }
    else
    {
      return pPeriodicScan2;
    }
  }
  /////////////////////////////////////////////
  // check missed packet parameter
  /////////////////////////////////////////////
  // pPeriodicScan2->interval = pPeriodicScan1->interval
  if (pPeriodicScan1->numMissed > pPeriodicScan2->numMissed)
  {
    return pPeriodicScan1;
  }
  else if (pPeriodicScan2->numMissed > pPeriodicScan1->numMissed)
  {
    return pPeriodicScan2;
  }
  /////////////////////////////////////////////
  // check start time parameter
  /////////////////////////////////////////////
  // pPeriodicScan2->numMissed = pPeriodicScan1->numMissed
  if (MAP_llTimeCompare(pPeriodicScan1->startTime , pPeriodicScan2->startTime))
  {
    return pPeriodicScan2;
  }
  else
  {
    return pPeriodicScan1;
  }
}

/*******************************************************************************
 * @fn          llFindNextPeriodicScan
 *
 * @brief       This routine is used find next Periodic Scan Set.
 *
 * @design      /ref did_286039104
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to periodic scan rf command
 */
void *llFindNextPeriodicScan( void )
{
  llPeriodicScanSet_t *pPeriodicScan = llPeriodicScan.scanList;
  uint32 currentTime = MAP_llGetCurrentTime() + RAT_TICKS_FOR_SCHED_PROCESS_TIME;
  uint8 updated;
  llPeriodicScanSet_t *pSelectedPeriodicScan = NULL;
  llPeriodicScanSet_t *pEarliestStartScan = NULL;
  uint8 extScanSelected = FALSE;

  if (((pPeriodicScan == NULL) || (llPeriodicScan.scanNumActive == 0)) &&
     (llPeriodicScan.createSync == NULL))
  {
    return(NULL);
  }
  // check if we are in a middle of synchronizing process
  if ((llPeriodicScan.createSync != NULL) &&
      (llPeriodicScan.createSync->state == PERIODIC_SCAN_STATE_SYNCING_ACTIVE) &&
      (llPeriodicScan.createSync->terminate == 0))
  {
    pSelectedPeriodicScan = llPeriodicScan.createSync;
    updated = FALSE;
    // check if the start time is in the future
    while(MAP_llTimeCompare(currentTime, llPeriodicScan.createSync->startTime))
    {
      // increase the start time
      llPeriodicScan.createSync->startTime = llPeriodicScan.createSync->startTime + (llPeriodicScan.createSync->interval * RAT_TICKS_IN_1_25MS );
      // increase the event counter
      llPeriodicScan.createSync->eventCounter++;
      llPeriodicScan.createSync->numMissed++;
      updated = TRUE;
    }
    // check for timeout
    if (llPeriodicScan.createSync->numMissed >= PERIODIC_SYNCING_LIMIT_NUM_EVENTS)
    {
      llPeriodicScan.createSync->terminate = LL_STATUS_ERROR_CONN_FAILED_TO_BE_ESTABLISHED;
      // set the first index which reserved for the create sync handle
      // the value is not important only to be different from 0xFF
      llPeriodicScan.terminateList[PERIODIC_SCAN_TERMINATE_LIST_CREATE_SYNC_INDEX] = 1;
      (void)MAP_osal_set_event( LL_TaskID, LL_EVT_PERIODIC_SCAN_CANCELLED );
    }
    else
    {
      // case start time was in the past
      if (updated)
      {
        //update the next channel
        llPeriodicScan.createSync->rfCmd.chan = llSetNextPeriodicAdvChan( &llPeriodicScan.createSync->chanMap.current, llPeriodicScan.createSync->syncInfo.accessAddr ,llPeriodicScan.createSync->eventCounter);
        llPeriodicScan.createSync->rfCmd.rfOpCmd.startTime = llPeriodicScan.createSync->startTime;
      }
      llPeriodicScan.createSync->intPriority = LL_QOS_HIGH_PRIORITY;
    }
  }
  // check the ext scan
  if ( extScanInfo->llTask != NULL )
  {
    updated = FALSE;
    // check if the start time is in the future
    while(MAP_llTimeCompare(currentTime, extScanInfo->scanStartTime))
    {
      // increase the start time
      extScanInfo->scanStartTime = extScanInfo->scanStartTime +
                                  (extScanInfo->pScanParam->extScanParam[extScanIndex].scanInterval * RAT_TICKS_IN_625US);
      // increase the missed counter
      extScanNumMissed++;
      updated = TRUE;
    }
    if (updated)
    {
      extScanCmd.rfOpCmd.startTime = extScanInfo->scanStartTime;
    }
    // increase the ext scan priority in case the periodic create sync is pending
    // or there were several missed scans
    if (((llPeriodicScan.createSync != NULL) &&
         (llPeriodicScan.createSync->state == PERIODIC_SCAN_STATE_SYNCING_PENDING)) ||
         (extScanNumMissed > EXT_SCAN_GET_MAX_NUMBER_POSSIBLE_MISSED(extScanInfo->pScanParam->extScanParam[extScanIndex].scanInterval)))
    {
      extScanPriority = LL_QOS_MEDIUM_PRIORITY;
    }
    else
    {
      extScanPriority = LL_QOS_LOW_PRIORITY;
    }
    // check that there is no periodic create sync in active state
    if (pSelectedPeriodicScan == NULL)
    {
      extScanSelected = TRUE;
    }
    else
    {
      // select between the ext scan and periodic create sync
      extScanSelected = llSelectScanOrPeriodicScan(pSelectedPeriodicScan);
      if (extScanSelected)
      {
        pSelectedPeriodicScan = NULL;
      }
    }
  }
  // run over all periodic scan sets
  while (pPeriodicScan != NULL)
  {
    // check only synced scan sets
    if ((pPeriodicScan->state == PERIODIC_SCAN_STATE_SYNCED) &&
        (pPeriodicScan->terminate == 0))
    {
      // drift time in RAT ticks per event (no drift in case drift learning in progress)
      int16 drift = (pPeriodicScan->driftLearnCounter <= PERIODIC_SCAN_DRIFT_LEARNING_MAX_NUM)?0:pPeriodicScan->driftFactor;

      updated = FALSE;
      // check if the start time is in the future
      while(MAP_llTimeCompare(currentTime , pPeriodicScan->startTime))
      {
        pPeriodicScan->startTime = pPeriodicScan->startTime + ((pPeriodicScan->interval * RAT_TICKS_IN_1_25MS) + drift);
        pPeriodicScan->eventCounter++;
        pPeriodicScan->numMissed++;
        updated = TRUE;
      }
      // check for timeout
      if ((pPeriodicScan->numMissed * pPeriodicScan->interval * 1250) >
          (pPeriodicScan->syncCmd.timeout * 10000))
      {
        uint8 i;
        // start from index 1 because index 0 is reserved for create sync cancel
        for (i=1;i<PERIODIC_SCAN_TERMINATE_LIST_MAX_HANDLES;i++)
        {
          if (llPeriodicScan.terminateList[i] == PERIODIC_SCAN_TERMINATE_LIST_INVALID_HANDLE)
          {
            // set the handle
            llPeriodicScan.terminateList[i] = pPeriodicScan->handle;
            pPeriodicScan->terminate = LL_STATUS_ERROR_CONNECTION_TIMEOUT;
            (void)MAP_osal_set_event( LL_TaskID, LL_EVT_PERIODIC_SCAN_CANCELLED );
            break;
          }
        }
      }
      else
      {
        //check if channel map was updated
        if ((pPeriodicScan->chanMap.updated) &&
            (pPeriodicScan->chanMapUpdateEvent <= pPeriodicScan->eventCounter))
        {
          // finish the channel map update procedure
          llSetPeriodicScanChmapUpdate(pPeriodicScan,FALSE,NULL,0);
          updated = TRUE;
        }
        // case start time was in the past or channel map was updated
        if (updated)
        {
          //update the next channel
          pPeriodicScan->rfCmd.chan = llSetNextPeriodicAdvChan( &pPeriodicScan->chanMap.current, pPeriodicScan->syncInfo.accessAddr ,pPeriodicScan->eventCounter);
        }
        // update start time
        pPeriodicScan->rfCmd.rfOpCmd.startTime = pPeriodicScan->startTime;

        // case previously ext scan task was selected
        if (extScanSelected)
        {
          extScanSelected = llSelectScanOrPeriodicScan(pPeriodicScan);
          if (extScanSelected == FALSE)
          {
            pSelectedPeriodicScan = pPeriodicScan;
          }
        }
        // case previously no periodic scan and no ext scan task was selected
        else if (pSelectedPeriodicScan == NULL)
        {
          pSelectedPeriodicScan = pPeriodicScan;
        }
        else
        {
          // check who has the earlier start time between previously selected and currently
          pEarliestStartScan = (MAP_llTimeCompare(pSelectedPeriodicScan->startTime , pPeriodicScan->startTime))?pPeriodicScan:pSelectedPeriodicScan;
          // select the most appropriate periodic scan task between previously selected and currently
          pSelectedPeriodicScan = llSelectPeriodicScan(pSelectedPeriodicScan,pPeriodicScan);
        }
      }
    }
    // move to next periodic scan set
    pPeriodicScan = pPeriodicScan->next;
  }
  if (extScanSelected == FALSE)
  {
    // check collision between the selected candidate and the earlier start time candidate
    // in case there is no collision between the two - select the earlier start time candidate
    if ((pEarliestStartScan != NULL) && (pSelectedPeriodicScan != pEarliestStartScan))
    {
      if (MAP_llTimeCompare(pSelectedPeriodicScan->startTime , pEarliestStartScan->startTime + pEarliestStartScan->totalOtaTime))
      {
        pSelectedPeriodicScan = pEarliestStartScan;
      }
    }
    // set the current task as the selected candidate
    llPeriodicScan.currentScan = pSelectedPeriodicScan;
  }
  else
  {
    pSelectedPeriodicScan = NULL;
  }

  if (pSelectedPeriodicScan != NULL)
  {
    // pointer to radio operation command
    llPeriodicScan.llTask->command = (uint32)&pSelectedPeriodicScan->rfCmd;

    return( (void *)&pSelectedPeriodicScan->rfCmd );
  }
  llPeriodicScan.llTask->command = 0;
  return NULL;
}
#endif
#endif // SCAN_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llAllocRfMem
 *
 * @brief       This routine is used to allocate the memory needed for all
 *              AE radio operations, if not already allocated. If allocated,
 *              then the various pointers will be updated based on this
 *              single monolithic memory block.
 *
 * input parameters
 *
 * @param       pAdvSet - Pointer to the advertising set for this command.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llAllocRfMem( advSet_t *pAdvSet )
{
  // check if we don't yet have an RF command structure, and if not, malloc one
  if ( !pAdvSet->pRfCmds )
  {
    // we don't have this memory yet, so try to allocate
    pAdvSet->pRfCmds = MAP_osal_mem_alloc( sizeof(aeRfCmdSize_t) );

    if ( !pAdvSet->pRfCmds ) return;

    // clear entire structure before initializing
    MAP_osal_memset( pAdvSet->pRfCmds, 0, sizeof(aeRfCmdSize_t) );
  }

#ifndef USE_RCL
  // check if this will be a legacy advertisement
  if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
  {
    aeLegacyRf_t *pRfCmds = (aeLegacyRf_t *)pAdvSet->pRfCmds;

    // setup adv command's next pointers
    pRfCmds->advCmd[0].rfOpCmd.pNextRfOp = (rfOpCmd_t *)&pRfCmds->advCmd[1];
    pRfCmds->advCmd[1].rfOpCmd.pNextRfOp = (rfOpCmd_t *)&pRfCmds->advCmd[2];
    pRfCmds->advCmd[2].rfOpCmd.pNextRfOp = NULL;

    // setup the parameters
    pRfCmds->advCmd[0].pParams = (uint8 *)&pRfCmds->advParam;
    pRfCmds->advCmd[1].pParams = (uint8 *)&pRfCmds->advParam;
    pRfCmds->advCmd[2].pParams = (uint8 *)&pRfCmds->advParam;

    // setup the output parameters
    pRfCmds->advCmd[0].pOutput = (uint8 *)&pRfCmds->advOutput;
    pRfCmds->advCmd[1].pOutput = (uint8 *)&pRfCmds->advOutput;
    pRfCmds->advCmd[2].pOutput = (uint8 *)&pRfCmds->advOutput;

    // update adv set
    pAdvSet->pRfCmds = (uint8 *)pRfCmds;
  }
#ifdef USE_AE
  else // !legacy
  {
    aeRf_t *pRfCmds = (aeRf_t *)pAdvSet->pRfCmds;

    // Primary channel RF command next pointer
    pRfCmds->extRfCmd[0].rfOpCmd.pNextRfOp = (rfOpCmd_t *)&pRfCmds->extRfCmd[1];
    pRfCmds->extRfCmd[1].rfOpCmd.pNextRfOp = (rfOpCmd_t *)&pRfCmds->extRfCmd[2];
    pRfCmds->extRfCmd[2].rfOpCmd.pNextRfOp = (rfOpCmd_t *)&pRfCmds->auxRfCmd;

    // Primary channel RF command parameters pointer
    pRfCmds->extRfCmd[0].pParams = (uint8 *)&pRfCmds->extRfParam;
    pRfCmds->extRfCmd[1].pParams = (uint8 *)&pRfCmds->extRfParam;
    pRfCmds->extRfCmd[2].pParams = (uint8 *)&pRfCmds->extRfParam;

    // Primary channel RF command output parameters pointer
    pRfCmds->extRfCmd[0].pOutput = (uint8 *)&pRfCmds->comOutput;
    pRfCmds->extRfCmd[1].pOutput = (uint8 *)&pRfCmds->comOutput;
    pRfCmds->extRfCmd[2].pOutput = (uint8 *)&pRfCmds->comOutput;

    // Secondary channel RF command next pointer
    pRfCmds->auxRfCmd.rfOpCmd.pNextRfOp =
      (rfOpCmd_t *)&pRfCmds->countCmd;

    // Secondary channel RF command parameters pointer
    pRfCmds->auxRfCmd.pParams =
      (uint8 *)&pRfCmds->auxRfParam;

    // Secondary channel RF command output parameters pointer
    pRfCmds->auxRfCmd.pOutput =
      (uint8 *)&pRfCmds->comOutput;

    // Secondary channel RF Counter command next pointer
    pRfCmds->countCmd.rfOpCmd.pNextRfOp =
     (rfOpCmd_t *)&pRfCmds->auxRfCmd;

    // Extended Advertising Command Extended Header pointer
    pRfCmds->extRfParam.pAdvPkt =
      (uint8 *)&pRfCmds->comPkt;

    // Secondary Advertising Command Extended Header Adv pointer
    pRfCmds->auxRfParam.pAdvPkt =
      (uint8 *)&pRfCmds->comPkt;

    // Secondary Advertising Command Extended Header Response pointer
    pRfCmds->auxRfParam.pRspPkt =
      (uint8 *)&pRfCmds->comPkt;

    // Common Format Packet Extended Buffer pointer
    pRfCmds->comPkt.pExtHeader =
      pRfCmds->extHdr;

    // update adv set
    pAdvSet->pRfCmds = (uint8 *)pRfCmds;
  }
#endif //USE_AE
#endif
  return;
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#ifdef USE_AE
/*******************************************************************************
 * @fn          llGetExtHdrLen
 *
 * @brief       This routine is used to get the size of the Extended Header
 *              based on the Extended Header Flags.
 *
 * input parameters
 *
 * @param       extHdrFlgs - Extended Header Flags
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Size of extended header.
 */
uint8 llGetExtHdrLen( uint8 extHdrFlgs )
{
  uint8 size = 0;

  // ALT: Check if less code to loop on a table of sizes.

  // AdvA
  if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_ADVA ) )
  {
    size += EXTHDR_FLAG_ADVA_SIZE;
  }

  // TargetA
  if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_TARGETA ) )
  {
    size += EXTHDR_FLAG_TARGETA_SIZE;
  }

  // ADI
  if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_ADI ) )
  {
    size += EXTHDR_FLAG_ADI_SIZE;
  }

  // Aux Ptr
  if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_AUXPTR ) )
  {
    size += EXTHDR_FLAG_AUXPTR_SIZE;
  }

  // Synch Info
  if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_SYNCINFO ) )
  {
    size += EXTHDR_FLAG_SYNCINFO_SIZE;
  }

  // Tx Power
  if ( TST_EXTHDR_FLAG( extHdrFlgs, EXTHDR_FLAG_TXPWR ) )
  {
    size += EXTHDR_FLAG_TXPWR_SIZE;
  }

  // check if we have extended header data based on flags
  if ( size != 0 )
  {
    // we do, so include one more byte for the extended header flags
    size += 1;
  }

  return( size );
}


/*******************************************************************************
 * @fn          llSetupExtHdr
 *
 * @brief       This routine is used to set the content of the Extended Header
 *              buffer for primary or secondary channels. The value of auxOffset
 *              is used as is (i.e. a value of zero means the CM0 will be used
 *              to generate the OTA auxOffset, whereas a non-zero value means
 *              this value will be sent OTA as is).
 *
 * input parameters
 *
 * @param       pAdvSet   - Pointer to the advertising set for this command.
 * @param       hdrFlags  - Common packet header flags.
 * @param       auxOffset - Auxilliary Offset, or AE_AUX_OFFSET_AUTO_INSERT.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetupExtHdr( advSet_t *pAdvSet,
                    uint8     hdrFlags,
                    uint16    auxOffset )
{
  uint8 *pBuf = ((aeRf_t *)pAdvSet->pRfCmds)->extHdr;

  // check if the AdvA flag is present
  // Note: The CM0 supports auto-insertion of AdvA, so should not need this.
  if ( TST_EXTHDR_FLAG(hdrFlags, EXTHDR_FLAG_ADVA) )
  {
    // store AdvA
  }

  // check if the TargetA flag is present
  // Note: The CM0 supports auto-insertion of TargetA, so should not need this.
  if ( TST_EXTHDR_FLAG(hdrFlags, EXTHDR_FLAG_TARGETA) )
  {
    // TODO: SEEMS CM0 DOESN'T SUPPORT THIS FOR PRIMARY CHANNELS WHERE THE
    //       SECONDARY IS NOT USED (E.G. NC/NS WITH NO DATA)! SO HAVE TO DISABLE
    //       SKIP TGTA AND EXPLICILTY PUT THE TARGETA IN THE COMMON BUFFER. THIS
    //       SUCKS AS IT ISN'T CLEAR HERE IF PRIMARY OR SECONDARY. REQUESTED THAT
    //       CM0 ADD POINTER TO Extended Advertiser Command AND SUPPORT THIS!

    // check if this is a NC/NS adertisment with no data
    if ( !TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps) &&
         !TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps) &&
         !pAdvSet->dataLen                                  &&
         !pAdvSet->pData )
    {
      // clear skip targetA bit
      CLR_EXT_ADV_HDR_CFG_SKIP_TGTA( ((aeRf_t *)pAdvSet->pRfCmds)->comPkt.extHdrConfig );

      // copy targetA address into common header
      MAP_osal_memcpy( pBuf, pAdvSet->pAdvParam->peerAddr, B_ADDR_LEN );
      pBuf += B_ADDR_LEN;
    }
  }

  // check if the ADI flag is present
  if ( TST_EXTHDR_FLAG(hdrFlags, EXTHDR_FLAG_ADI) )
  {
    // store ADI
    *pBuf++ = LO_UINT16( pAdvSet->adi );
    *pBuf++ = HI_UINT16( pAdvSet->adi );
  }

  // check if there's an auxPtr
  if ( TST_EXTHDR_FLAG(hdrFlags, EXTHDR_FLAG_AUXPTR) )
  {
    uint8  auxCA;
    uint8  auxOffsetUnits;
    uint8  auxPhy;
    uint16 auxRem;

    // complete the content of the Extended Header for ADV_EXT_IND
    // Note: AuxPtr Requires: chan index, CA, offset units, aux offset, aux phy

    // determine CA based on the Peripheral SCA
    auxCA = (pAdvSet->scaValue <= LL_CA_50_PPM) ?
            AE_AUX_CA_0_50_PPM                  :
            AE_AUX_CA_51_500_PPM;

    // set the aux offset units based on the total time
    // Note: Spec indicates 30us should be used if time if less than 245700 us.
    // ALT: ((uint32)pAdvSet->otaTimeExtAdv < AE_AUX_OFFSET_UNIT_CUTOFF_TIME) ?
    //      AE_AUX_OFFSET_UNITS_30_US                                         :
    //      AE_AUX_OFFSET_UNITS_300_US;
    auxOffsetUnits = AE_AUX_OFFSET_UNITS_30_US;

    // set the aux PHY
    // Note: The aux PHY specified by the API is +1 the value expected in the
    //       AuxPtr field (sigh).
    auxPhy = (pAdvSet->pAdvParam->secPhy & AE_PHY_CODED_SCHEME_MASK) - 1;

    // time to update the Extended Header buffer

    // store the Offset Units, CA, and Channel Index in first byte of AuxPtr
    *pBuf++ = (auxOffsetUnits << 7) |
              (auxCA << 6)          |
              (pAdvSet->auxChanIndex & AE_CHAN_INDEX_MASK);

    // build remainder of the AuxPtr
    // Note: When the auxOffset is zero, it is expected that the CM0 will
    //       automatically update the Extended Header value, so what's stored
    //       in the Extended Header now does not matter.
    auxRem = (auxPhy << AE_AUX_OFFSET_SIZE) |
             (auxOffset & AE_AUX_OFFSET_MASK);

    // store the Aux PHY and Aux Offset
    *pBuf++ = LO_UINT16( auxRem );
    *pBuf++ = HI_UINT16( auxRem );
  }

  // process synch info
  if ( TST_EXTHDR_FLAG(hdrFlags, EXTHDR_FLAG_SYNCINFO) )
  {
    MAP_llSetPeriodicSyncInfo(pAdvSet,pBuf);
    pBuf += EXTHDR_FLAG_SYNCINFO_SIZE;
  }

  // check if aux Tx Power specified
  if ( TST_EXTHDR_FLAG(hdrFlags, EXTHDR_FLAG_TXPWR) )
  {
    // adjust Tx Power based on Tx RF path compensation
    *pBuf++ = RfBleDpl_getTxPowerDbm(pAdvSet->txPowerIndex) +
              pRfPathComp->rfTxPathCompVal;
  }

  // ACAD Support
  // Note: It isn't clear from the spec what decides if ACAD is needed/used.
  //       Most likely if needed/used, the required ACAD size would be known,
  //       and ExtHdrLen would be given by extHdrSize + acadLen. But it seems
  //       it might also be necessary to determine how much of the extended
  //       header is left for ACAD. In that case, the available ACAD size
  //       would be given by EXTHDR_MAX_LEN - extHdrSize.
  //
  // Note: ACAD is currently not supported, so the Extended Header Length is
  //       only based on extHdrSize.

  return;
}

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llSetupExtAdv
 *
 * @brief       This routine is used to setup radio to execute the extended
 *              advertising command.
 *
 * input parameters
 *
 * @param       pAdvSet - Pointer to the advertising set for this command.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 */
llStatus_t llSetupExtAdv( advSet_t *pAdvSet )
{
  aeRf_t *pRf;
  uint32 aeStartTime;
  sortedAdv_t *nodePtr = NULL;
  ble5OpCmd_t *pAuxRfCmd = NULL;

  // check input parameter
  LL_ASSERT( pAdvSet != NULL );

  // get pointer to RF command
  pRf = (aeRf_t *)pAdvSet->pRfCmds;

  /*
  ** Setup First Primary RF Command
  */

  // init radio command
  pRf->extRfCmd[0].rfOpCmd.cmdNum    = CMD_BLE5_ADV_EXT;
  pRf->extRfCmd[0].rfOpCmd.status    = RFSTAT_IDLE;
  pRf->extRfCmd[0].rfOpCmd.startTrig = TRIGTYPE_AT_ABS_TIME;
  pRf->extRfCmd[0].rfOpCmd.condition = CONDTYPE_RUN_TRUE_STOP_FALSE;

  // add AE node into AE List and determine its start time.
  aeStartTime = MAP_llAddAdvSortedEntry(pAdvSet, &nodePtr);
  if (aeStartTime==AE_INVALID_START_TIME)
  {
    return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED);
  }
  if (nodePtr != NULL)
  {
    pRf->extRfCmd[0].rfOpCmd.startTime = aeStartTime;
  }

  // save the absolute start time of the advertiser for post-processing
  // Note: The start time is advanced for primary channels, so we need the
  //       original advertisement event start time to keep the advertisement
  //       interval.
  pAdvSet->advStartTime = pRf->extRfCmd[0].rfOpCmd.startTime;

  // Update the default priority for the extended advertise set.
  pAdvSet->priority = qosDefaultPriorityAdvParameter;

  // set randomly the channel number and enable BLE whitening
  // ALT: Get rid of advChan[i] from flash?
  pAdvSet->firstPrimChan = LL_ADV_BASE_CHAN + llGetRandChannelMapIndex(pAdvSet->pAdvParam->primChanMap);
  pRf->extRfCmd[0].chan = pAdvSet->firstPrimChan;
  SET_WHITENING_BLE( pRf->extRfCmd[0].whitening );

  // Sanity Check
  // Note: The 2M PHY is not allowed for primary channels.
  LL_ASSERT( pAdvSet->pAdvParam->primPhy != AE_PHY_2_MBPS );

  // set primary PHY
  // Note: Mask off the MSBit which indicates Coded Scheme.
  // Note: Parameter value is +1 the values used in the RF command.
  // set the aux PHY in the CM0
  if ( pAdvSet->pAdvParam->primPhy == AE_PHY_1_MBPS )
  {
    // set PHY
    // Note: Mask off the MSBit which indicates Coded Scheme.
    // Note: Parameter value is +1 the values used in the RF command.
    pRf->extRfCmd[0].phyMode =
      (pAdvSet->pAdvParam->primPhy & AE_PHY_CODED_SCHEME_MASK) - 1;

    // default range delay
    pRf->extRfCmd[0].rangeDelay = 0;
  }
  else // Coded
  {
    pRf->extRfCmd[0].phyMode = (pAdvSet->pAdvParam->primPhy == AE_PHY_CODED_S2) ?
                             BLE5_CODED_S2_PHY                               :
                             BLE5_CODED_S8_PHY;

    // set range delay
    // Note: This is for Long Range (worst case distance of 1km, or 4us).
    pRf->extRfCmd[0].rangeDelay = (2 * RAT_TICKS_IN_4US);
  }

  llSetPower((uint32 *)&pRf->extRfCmd[0],
              pAdvSet->txPowerIndex,
              RfBleDpl_getTxPower(pAdvSet->txPowerIndex));

  // check if the user set the Tx Power option
  if ( TST_AE_PROPS_TX_PWR(pAdvSet->pAdvParam->eventProps) )
  {
    // include Tx power flag
    // Note: This is always optional in the secondary channel header, so
    //       always include it there except when no auxPtr is present (as in
    //       the case of ADV_EXT_IND-only).
    if ( TST_EXTHDR_FLAG(pAdvSet->extHdrFlags, EXTHDR_FLAG_AUXPTR) )
    {
      SET_EXTHDR_FLAG( pAdvSet->auxHdrFlags,
                       EXTHDR_FLAG_TXPWR );
    }
    else // there is no secondary channel, so use extended header
    {
      SET_EXTHDR_FLAG( pAdvSet->extHdrFlags,
                       EXTHDR_FLAG_TXPWR );
    }
  }

  /*
  ** Setup Primary Channel Command Parameters
  */

  // set parameters advertisement configuraiton
  CLR_ADV_CFG( pRf->extRfParam.advCfg );
  SETVAR_ADV_CFG_DEV_ADDR_TYPE( pRf->extRfParam.advCfg,
                                pAdvSet->ownAddrType );

  // set pointer to device address
  pRf->extRfParam.pDeviceAddr = pAdvSet->ownAddr;

  // determine if an auxPtr is needed
  if ( TST_EXTHDR_FLAG(pAdvSet->extHdrFlags, EXTHDR_FLAG_AUXPTR) )
  {
    uint8 rem;

    // check if the AdvA should be omitted
    // Note: Spec isn't clear about coded Phy - says C1 reserved for future use.
    if ( TST_AE_PROPS_OMIT_ADVA(pAdvSet->pAdvParam->eventProps) )
    {
      // make sure no AdvA is sent
      CLR_EXTHDR_FLAG( pAdvSet->extHdrFlags,
                       EXTHDR_FLAG_ADVA );

      // also remove AdvA from Aux, but only if not connectable, not scannable
      if ( !TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps) &&
           !TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps) )
      {
        // make sure no AdvA is sent
        CLR_EXTHDR_FLAG( pAdvSet->auxHdrFlags,
                         EXTHDR_FLAG_ADVA );
      }
    }

    // determine size of Extended Header buffer
    pAdvSet->extHdrSize = MAP_llGetExtHdrLen( pAdvSet->extHdrFlags );

    // calculate the auxPtrTargetType/Time from start of ADV_EXT_IND

    // setup auxPtr's in ADV_EXT_IND relative to start of aux packet
    // Note: The CM0 takes the time (auxPtrTgtTime) and trigger (auxPtrTgtType)
    //       and updates the content of the Extended Header in terms of offset
    //       units based on the trigger type. We therefore need to calculate
    //       the time of an ADV_EXT_IND packet (based on size and PHY), and
    //       total that based on the number of primary channels being sent,
    //       the interpacket time, and T_MAFS. This time can then be specified
    //       as being relative to the start of the packet. Then, after each
    //       primary channel completes, the next time is adjusted by one less
    //       packet and interpacket time.

    // convert the OTA size to time based on the PHY
    // Note: The AUX PHY field in AuxPtr is minus one the parameters.
    // Note: The expected phy and coding by llOctets2Time:
    //         BLE5_1M_PHY, BLE5_2M_PHY, BLE5_CODED_PHY
    //         BLE5_CODED_S8_DEFAULT, BLE5_CODED_S2_DEFAULT
    pAdvSet->otaTimeAdjust = MAP_llOctets2Time( pRf->extRfCmd[0].phyMode & 0x03,      // first two bits only
                                               (pRf->extRfCmd[0].phyMode>>2) & 0x01,  // scheme
                                                (pAdvSet->extHdrSize+1),
                                                MIC_NOT_ENABLED );

    // time for last PDU, rounded up to next 30us unit
    rem = (pAdvSet->otaTimeAdjust + AE_MIN_T_MAFS_IN_US) % AE_AUX_OFFSET_30_US_UNIT_VALUE;
    pAdvSet->otaTimeExtAdv = (pAdvSet->otaTimeAdjust + AE_MIN_T_MAFS_IN_US) +
                             ((rem) ? (AE_AUX_OFFSET_30_US_UNIT_VALUE - rem) : 0);

    // time for each PDU (used to subtract from otaTimeExtAdv after each int)
    pAdvSet->otaTimeAdjust += AE_MIN_CMD_TIME_IN_US;

    // adjust time per channel to align auxPtrs to Rx window
    rem = pAdvSet->otaTimeAdjust % AE_AUX_OFFSET_30_US_UNIT_VALUE;
    pAdvSet->otaTimeAdjust += ((rem) ? (AE_AUX_OFFSET_30_US_UNIT_VALUE - rem) : 0);

    // adjust total time to align start time of AUX PDU
    pAdvSet->otaTimeExtAdv += (pAdvSet->numPrimChans-1) * pAdvSet->otaTimeAdjust;

    // set the auxPtr time and type
    // Note: CM0 expects time in RAT ticks.
    pRf->extRfParam.auxPtrTgtTime = US_TO_RAT_TICKS( pAdvSet->otaTimeExtAdv + PRIMARY_CMD_START_SYNTH_TO_RAT_OFFSET );
    pRf->extRfParam.auxPtrTgtType = TRIGTYPE_REL_FIRST_CHAIN_CMD;

    // ADI mandatory in AUX packet
    SET_EXTHDR_FLAG( pAdvSet->auxHdrFlags,
                     EXTHDR_FLAG_ADI );
    pAuxRfCmd = &pRf->auxRfCmd;
  }
  else // no auxPtr needed
  {
    // AdvA mandatory
    SET_EXTHDR_FLAG( pAdvSet->extHdrFlags,
                     EXTHDR_FLAG_ADVA );

    // check if directed
    if ( TST_AE_PROPS_DIR(pAdvSet->pAdvParam->eventProps) )
    {
      // it is, so include TargetA in extHdrFlags
      SET_EXTHDR_FLAG( pAdvSet->extHdrFlags,
                       EXTHDR_FLAG_TARGETA );
    }

    // determine size of Extended Header buffer
    pAdvSet->extHdrSize = MAP_llGetExtHdrLen( pAdvSet->extHdrFlags );
    pAdvSet->otaTimeAdjust = MAP_llOctets2Time( pRf->extRfCmd[0].phyMode & 0x03,      // first two bits only
                                               (pRf->extRfCmd[0].phyMode>>2) & 0x01,  // scheme
                                               (pAdvSet->extHdrSize+1),
                                                MIC_NOT_ENABLED );
    pAdvSet->otaTimeAdjust += AE_MIN_CMD_TIME_IN_US;
    // default aux type
    // Note: Set trigger to NOW when there is no subordinate packet
    pRf->extRfParam.auxPtrTgtTime = 0;
    pRf->extRfParam.auxPtrTgtType = TRIGTYPE_NOW;
  }

  /*
  ** Setup Common Format Packet for Primary Channel Command
  */

  // add length to Extended Header Info
  SET_EXTHDR_LEN( pAdvSet->extHdrInfo,
                  pAdvSet->extHdrSize );

  // set the Extended Header Info
  pRf->comPkt.extHdrInfo = pAdvSet->extHdrInfo;

  // set the Extended Header Flags
  pRf->comPkt.extHdrFlags = pAdvSet->extHdrFlags;

  // Extended Header Configuration
  pRf->comPkt.extHdrConfig = 0;

  SET_EXT_ADV_HDR_CFG_SKIP_ADVA( pRf->comPkt.extHdrConfig );
  SET_EXT_ADV_HDR_CFG_SKIP_TGTA( pRf->comPkt.extHdrConfig );
  SETVAR_EXT_ADV_HDR_CFG_DEV_ADDR_TYPE( pRf->comPkt.extHdrConfig,
                                        pAdvSet->ownAddrType );
  SETVAR_EXT_ADV_HDR_CFG_TGT_ADDR_TYPE( pRf->comPkt.extHdrConfig,
                                        pAdvSet->peerAddrType );

  // advertisement length and data pointer
  // Note: The ADV_EXT_IND packet never has data.
  pRf->comPkt.advDataLen = 0;
  pRf->comPkt.pAdvData   = NULL;

  // Setup Extended Header Buffer for ADV_EXT_IND

  // add SID to ADI without affecting DDI
  pAdvSet->adi &= EXTHDR_DID_MASK;
  pAdvSet->adi |= (pAdvSet->pAdvParam->sid << EXTHDR_DID_SIZE);

  // build the Extended Header Buffer for ADV_EXT_IND
  // Note: Since the CM0 auto-inserts AdvA and TargetA if needed, we can
  //       exclude them from the Extended Header buffer.
  // Note: CM0 doesn't support auto-insertion of TargetA, so don't exclude here.
  MAP_llSetupExtHdr( pAdvSet,
                     pAdvSet->extHdrFlags & ~(EXTHDR_FLAG_ADVA),
                     AE_AUX_OFFSET_AUTO_INSERT );

  /*
  ** Setup Second and third Primary Channel RF Commands
  */
  for (uint8 i=1; i < pAdvSet->numPrimChans; i++)
  {
    // copy the content from the first command
    MAP_osal_memcpy( &pRf->extRfCmd[i], &pRf->extRfCmd[0], sizeof(ble5OpCmd_t) );
    // update the start time as related to the previous command start time
    pRf->extRfCmd[i].rfOpCmd.startTrig = TRIGTYPE_REL_PREV_CMD_START;
    pRf->extRfCmd[i].rfOpCmd.startTime = US_TO_RAT_TICKS(pAdvSet->otaTimeAdjust);

    llSetPower((uint32 *)&pRf->extRfCmd[i],
                pAdvSet->txPowerIndex,
                RfBleDpl_getTxPower(pAdvSet->txPowerIndex));
  }
  // set randomly the rest of the channels
  llSetRestPrimaryChannels(pAdvSet);
  // set the next pointers
  switch (pAdvSet->numPrimChans)
  {
    case 1:
      pRf->extRfCmd[0].rfOpCmd.pNextRfOp = (rfOpCmd_t *)pAuxRfCmd;
      break;
    case 2:
      pRf->extRfCmd[0].rfOpCmd.pNextRfOp = (rfOpCmd_t *)&pRf->extRfCmd[1];
      pRf->extRfCmd[1].rfOpCmd.pNextRfOp = (rfOpCmd_t *)pAuxRfCmd;
      break;
    case 3:
      pRf->extRfCmd[0].rfOpCmd.pNextRfOp = (rfOpCmd_t *)&pRf->extRfCmd[1];
      pRf->extRfCmd[1].rfOpCmd.pNextRfOp = (rfOpCmd_t *)&pRf->extRfCmd[2];
      pRf->extRfCmd[2].rfOpCmd.pNextRfOp = (rfOpCmd_t *)pAuxRfCmd;
      break;
  }

  /*
  ** Setup Secondary RF Command
  */

  // init radio command
  pRf->auxRfCmd.rfOpCmd.cmdNum    = CMD_BLE5_ADV_AUX;
  pRf->auxRfCmd.rfOpCmd.status    = RFSTAT_IDLE;
  pRf->auxRfCmd.rfOpCmd.startTime = pRf->extRfCmd[0].rfOpCmd.startTime +
                                    US_TO_RAT_TICKS(pAdvSet->otaTimeExtAdv);
  pRf->auxRfCmd.rfOpCmd.startTrig = PAST_TRIG_START_ASAP |
                                    TRIGTYPE_AT_ABS_TIME;
  pRf->auxRfCmd.rfOpCmd.condition = CONDTYPE_RUN_TRUE_STOP_FALSE;

  // set the channel number and enable BLE whitening
  pRf->auxRfCmd.chan = pAdvSet->auxChanIndex & AE_CHAN_INDEX_MASK;
  SET_WHITENING_BLE( pRf->auxRfCmd.whitening );

  // set secondary PHY
  // Note: Mask off the MSBit which indicates Coded Scheme.
  // Note: Parameter value is +1 the values used in the RF command.
  // set the aux PHY in the CM0
  if ( (pAdvSet->pAdvParam->secPhy == AE_PHY_1_MBPS) ||
       (pAdvSet->pAdvParam->secPhy == AE_PHY_2_MBPS) )
  {
    // set PHY
    // Note: Mask off the MSBit which indicates Coded Scheme.
    // Note: Parameter value is +1 the values used in the RF command.
    pRf->auxRfCmd.phyMode =
      (pAdvSet->pAdvParam->secPhy & AE_PHY_CODED_SCHEME_MASK) - 1;

    // default range delay
    pRf->auxRfCmd.rangeDelay = LL_UNCODED_RANGE_DELAY_RAT_TICKS;
  }
  else // Coded
  {
    pRf->auxRfCmd.phyMode = (pAdvSet->pAdvParam->secPhy == AE_PHY_CODED_S2) ?
                             BLE5_CODED_S2_PHY                               :
                             BLE5_CODED_S8_PHY;

    // set range delay
    // Note: This is for Long Range (worst case distance of 1km, or 4us).
    pRf->auxRfCmd.rangeDelay = LL_CODED_RANGE_DELAY_RAT_TICKS;
  }

  // set the RF command with Tx power value based on index
  pRf->auxRfCmd.txPower = RfBleDpl_getTxPower(pAdvSet->txPowerIndex);

  /*
  ** Setup Secondary Channel Command Parameters
  */

  // set adv Rx one entry queue (only for scannable or connectable)
  pRf->auxRfParam.pRXQ = NULL;
  if ( TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps) ||
       TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps) )
  {
    // shared static Rx buffer
    // ALT: make dynamic, use llSetupAdvDataEntryQueue during init, set queue/buf here.
    // ALT: Update this to use same shared RXQ.
    pRf->auxRfParam.pRXQ = MAP_llSetupAdvDataEntryQueue();

    // set the Adv RX queue configuration
    pRf->auxRfParam.rxCfg         =
    ( RXQ_CFG_AUTOFLUSH_IGNORED_PKT |
      RXQ_CFG_AUTOFLUSH_CRC_ERR_PKT |
      RXQ_CFG_AUTOFLUSH_EMPTY_PKT   |
      RXQ_CFG_INCLUDE_PKT_LEN_BYTE  |
      ((llConfigTable.advPktSuffixPtr->suffixSel & SUFFIX_CRC_FLAG)       ? RXQ_CFG_INCLUDE_CRC      : 0) |
      ((llConfigTable.advPktSuffixPtr->suffixSel & SUFFIX_RSSI_FLAG)      ? RXQ_CFG_APPEND_RSSI      : 0) |
      ((llConfigTable.advPktSuffixPtr->suffixSel & SUFFIX_STATUS_FLAG)    ? RXQ_CFG_APPEND_STATUS    : 0) |
      ((llConfigTable.advPktSuffixPtr->suffixSel & SUFFIX_TIMESTAMP_FLAG) ? RXQ_CFG_APPEND_TIMESTAMP : 0) );
  }

  // set the Adv configuration
  CLR_ADV_CFG( pRf->auxRfParam.advCfg );
  SETVAR_ADV_CFG_FILTER_POLICY( pRf->auxRfParam.advCfg,
                                pAdvSet->pAdvParam->filterPolicy );

  SETVAR_ADV_CFG_DEV_ADDR_TYPE( pRf->auxRfParam.advCfg,
                                pAdvSet->ownAddrType );

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  SETVAR_ADV_CFG_PEER_ADDR_TYPE( pRf->auxRfParam.advCfg,
                                 pAdvSet->peerAddrType );
#endif // ADV_CONN_CFG

  // enable strict length checks by radio
  // Note: May need to check this vis-a-vis AUX_SCAN_REQ and AUX_CONNECT_REQ.
  SETVAR_ADV_CFG_STRICT_LEN_FILTER( pRf->auxRfParam.advCfg,
                                    ADV_CFG_DISCARD_BAD_LEN_MSG );

  // only if address resolution is enabled
  if ( privInfo.addrResolution )
  {
    // enable RPA Mode in Adv configuration
    // Per Erratum #6984.
    // ALT: Do not enable RPA Mode, and allow unresolved RPA if FP=ANY.
    //SET_ADV_CFG_RPA_MODE( advParam.advCfg );

    // enable Privacy Ignore in Adv Configuration
    SET_ADV_CFG_PRIV_IGN_MODE( pRf->auxRfParam.advCfg );

    // clear the Autoflush Ignored Packet flag
    pRf->auxRfParam.rxCfg &= ~RXQ_CFG_AUTOFLUSH_IGNORED_PKT;
  }

  // determine size of Extended Header buffer
  pAdvSet->auxExtHdrSize = MAP_llGetExtHdrLen( pAdvSet->auxHdrFlags );

  // determine if there is any data, and if so, how many fragments
  MAP_llSetupExtData( pAdvSet );

  // determine if an auxPtr is needed
  // Note: Done not only for Nonconnectable/Nonscannable, but for Scannable as
  //       well, even though AUX_ADV_IND for Scannable does not have AuxPtr.
  //       But if there's a AUX_SCAN_RESPONSE with data, these values are used.
  if ( TST_EXTHDR_FLAG(pAdvSet->auxHdrFlags, EXTHDR_FLAG_AUXPTR) )
  {
    uint8 pktSize = (pAdvSet->auxExtHdrSize+1);

    // Scannable?
    // Note: If NC/NS, then data is part of AUX_ADV_IND PDU. This is true too
    //       for Connectable/NS, but there's no auxPtr in that case.
    if ( TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps) )
    {
      // auxPtr not included in size
      pktSize -= EXTHDR_FLAG_AUXPTR_SIZE;
    }
    else // !Scannable
    {
      // include data fragment length
      pktSize += pAdvSet->fragLen;
    }

    // calculate the auxPtrTargetType/Time from start of ADV_EXT_IND

    // setup auxPtr's to AUX_CHAIN_IND or AUX_SCAN_RSP, relative to start of AUX_ADV_IND
    // Note: The CM0 takes the time (auxPtrTgtTime) and trigger (auxPtrTgtType)
    //       and updates the content of the Extended Header in terms of offset
    //       units based on the trigger type. Two possiblities exist: NC/NS
    //       where the next PDU is an AUX_CHAIN_IND, or NC/Scannable, where
    //       we're actually referring to the AUX_SCAN_RSP's auxPtr to a
    //       possible AUX_CHAIN_IND. We therefore need to calculate the time
    //       of an AUX_ADV_IND packet (based on size and PHY), including the
    //       T_MAFS
    //       total that based on the number of primary channels being sent,
    //       the interpacket time, and T_MAFS. This time can then be specified
    //       as being relative to the start of the packet. Then, after each
    //       primary channel completes, the next time is adjusted by one less
    //       packet and interpacket time.

    // convert the OTA size of AUX packet to time based on the PHY
    // Note: The AUX PHY field in AuxPtr is minus one the value of the parameters.
    // Note: The expected phy and coding by llOctets2Time:
    //         BLE5_1M_PHY, BLE5_2M_PHY, BLE5_CODED_PHY
    //         BLE5_CODED_S8_DEFAULT, BLE5_CODED_S2_DEFAULT
    pAdvSet->otaTimeAuxAdv = MAP_llOctets2Time( pRf->auxRfCmd.phyMode & 0x03,      // first two bits only
                                                (pRf->auxRfCmd.phyMode>>2) & 0x01, // scheme
                                                pktSize,
                                                MIC_NOT_ENABLED );

    pAdvSet->minTimeAdj = AE_MIN_OFFSET_UNIT_IN_US -
                          ((pAdvSet->otaTimeAuxAdv + AE_T_MAFS_IN_US) % AE_MIN_OFFSET_UNIT_IN_US);

    // for Scannable, don't include the T_MAFS because it is not needed to find
    // the start time of the AUX_SCAN_RSP
    pAdvSet->otaTimeAuxAdv += ((TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps)) ?
                               0                                                   :
                               AE_MIN_T_MAFS_IN_US);

    // save initial otaTimeAuxAdv time in case we're Scannable, since the
    // otaTimeAuxAdv is subsequently updated in case a AUX_SCAN_RSP is sent
    pAdvSet->otaTimeAuxAdvScan = pAdvSet->otaTimeAuxAdv;

    // set the auxPtr time and type
    pRf->auxRfParam.auxPtrTgtTime = pRf->auxRfCmd.rfOpCmd.startTime         +
                                    US_TO_RAT_TICKS( pAdvSet->otaTimeAuxAdv +
                                                     pAdvSet->minTimeAdj    +
                                                     START_SYNTH_TO_RAT_OFFSET );

    pRf->auxRfParam.auxPtrTgtType = TRIGTYPE_AT_ABS_TIME;
  }
  else // no auxPtr needed
  {
    // default aux type
    // Note: Set trigger to NOW when there is no subordinate packet
    pRf->auxRfParam.auxPtrTgtTime = 0;
    pRf->auxRfParam.auxPtrTgtType = TRIGTYPE_NOW;
  }

  // set a pointer to our device address
  pRf->auxRfParam.pDeviceAddr = pAdvSet->ownAddr;

  // set AL, if used
  if ( TST_AE_PROPS_DIR(pAdvSet->pAdvParam->eventProps) )
  {
    pRf->auxRfParam.pAcceptList = (alEntry_t *)pAdvSet->peerAddr;

    // indicate this is a directed advertisement
    // Note: There is no chSel bit since Algo #2 is always required for AE.
    SET_ADV_CFG_DIRECTED( pRf->auxRfParam.advCfg );
  }
  else // not directed, so use AL
  {
    // use standard accept list table
    pRf->auxRfParam.pAcceptList = &alTable->pAlEntries[0];

    // Note: Bit 5 is bDirected instead of Channel Selection Algo!
    CLR_ADV_CFG_DIRECTED( pRf->auxRfParam.advCfg );
  }

  /*
  ** Setup Common Format Packet for Secondary Channel Command
  */

  // Note: The common format packet's content is shared, so cannot be updated
  //       with secondary information yet. This information will be updated
  //       at the appropriate time during RF Callbacks. Note though that the
  //       extHdrConfig field will remain common to all packets.

  // add length to Extended Header Info
  SET_EXTHDR_LEN( pAdvSet->auxHdrInfo,
                  pAdvSet->auxExtHdrSize );

  /*
  ** Setup Secondary Channel RF Command Loop Counter
  */

  pRf->countCmd.rfOpCmd.cmdNum    = CMD_COUNTER;
  pRf->countCmd.rfOpCmd.status    = RFSTAT_IDLE;
  pRf->countCmd.rfOpCmd.startTime = RAT_TICKS_IN_1US *
                                     (*llConfigTable.advExtSecLoopOffsetUs);
  if ((*llConfigTable.advExtSecLoopOffsetUs) != 0)
  {
    pRf->countCmd.rfOpCmd.startTrig = TRIGTYPE_REL_END_PREV_CMD;
  }
  else
  {
    pRf->countCmd.rfOpCmd.startTrig = TRIGTYPE_NOW;
  }
  pRf->countCmd.rfOpCmd.condition = CONDTYPE_RUN_TRUE_STOP_FALSE;
  // set the counter based on the number of additional aux packets needed
  // Note: If the numFrags=0, then there is no secondary channel packet, so
  //       this counter will never be used.
  pRf->countCmd.counter           = pAdvSet->numFrags;

  if ( TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps) )
  {
    // set to one in case there's not AUX_SCAN_REQ, to end command chain
    // Note: If there is a AUX_SCAN_REQ, then we'll send a AUX_SCAN_RSP and
    //       get a Tx Done interrupt. The ISR will update the counter to
    //       pAdvSet->numFrags-1. Yes, I get there's a possible race
    //       condition here.
    // Note: If Scannable, we just send a AUX_ADV_IND, so pAdvSet->numFrags
    //       is always at least one, but can be more if there's fragmented
    //       AUX_SCAN_RSP data.
    pRf->countCmd.counter = 1;
  }
  if (nodePtr != NULL)
  {
    // update the time consumed over the air
    MAP_llSetAETimeConsume(nodePtr);
  }
  return( LL_STATUS_SUCCESS );
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG
#endif //USE_AE

#ifdef USE_PERIODIC_ADV
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llSetPeriodicSyncInfo
 *
 * @brief       This routine is used to set the content of the sync info in AUX_ADV_IND
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       pAdvSet   - Pointer to the advertising set for this command.
 * @param       pBuf  - pointer to packet header.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetPeriodicSyncInfo( advSet_t *pAdvSet, uint8 *pBuf )
{
  llPeriodicAdvSet_t *pPeriodicAdv = llGetPeriodicAdv(pAdvSet->pAdvParam->handle);
  llPeriodicChanMap_t *pChanMap = &llPeriodicAdv.chanMap.current;
  uint32 totalAuxOtaTime;
  uint16 offsetUnitVal = AE_AUX_OFFSET_30_US_UNIT_VALUE;
  uint16 packetOffsetField;
  uint8 i;

  if (pPeriodicAdv == NULL)
  {
    return;
  }
  if (pAdvSet->numFrags > 1)
  {
    uint16 chainSize;
    // AUX_ADV_IND ota time with sync info
    totalAuxOtaTime = pAdvSet->otaTimeAuxAdv;

    // CHAIN_IND ota time without sync info
    chainSize = ((pAdvSet->auxExtHdrSize - EXTHDR_FLAG_SYNCINFO_SIZE + 1) * (pAdvSet->numFrags - 1)) +
                (pAdvSet->fragLen * (pAdvSet->numFrags - 2)) + pAdvSet->lastFragLen; //last frag should be without auxptr

    totalAuxOtaTime += MAP_llOctets2Time( ((aeRf_t *)pAdvSet->pRfCmds)->auxRfCmd.phyMode & 0x03,      // first two bits only
                              (((aeRf_t *)pAdvSet->pRfCmds)->auxRfCmd.phyMode>>2) & 0x01, // scheme
                              chainSize,
                              MIC_NOT_ENABLED );

    totalAuxOtaTime += (AE_MIN_T_MAFS_IN_US * (pAdvSet->numFrags - 2));
  }
  else
  {
    totalAuxOtaTime = MAP_llOctets2Time( ((aeRf_t *)pAdvSet->pRfCmds)->auxRfCmd.phyMode & 0x03,      // first two bits only
                              (((aeRf_t *)pAdvSet->pRfCmds)->auxRfCmd.phyMode>>2) & 0x01, // scheme
                              (pAdvSet->auxExtHdrSize + 1),
                              MIC_NOT_ENABLED );
  }
  pPeriodicAdv->syncInfo.eventCounter = pPeriodicAdv->eventCounter;
  pPeriodicAdv->syncInfo.offsetUnit = AE_AUX_OFFSET_UNITS_30_US;

  // set the packet offset
  if ((pPeriodicAdv->state == PERIODIC_ADV_STATE_PENDING_ENABLE) ||
      (pPeriodicAdv->state == PERIODIC_ADV_STATE_PENDING_TRIGGER))
  {
    pPeriodicAdv->syncInfo.packetOffset = totalAuxOtaTime +
                                       (((MAP_LL_ENC_GeneratePseudoRandNum() % 4) + 1) * 1000);
  }
  else
  {
    i = 0;
    // find the proper sync indication that the sync information will point to
    while(MAP_llTimeCompare(((aeRf_t *)pAdvSet->pRfCmds)->auxRfCmd.rfOpCmd.startTime + US_TO_RAT_TICKS(totalAuxOtaTime) , pPeriodicAdv->rfCmd.rfOpCmd.startTime + (i * pPeriodicAdv->interval * RAT_TICKS_IN_1_25MS )))
    {
      i++;
      pPeriodicAdv->syncInfo.eventCounter++;
    }
    // set the sync information packet offset
    pPeriodicAdv->syncInfo.packetOffset = RAT_TICKS_TO_US(pPeriodicAdv->rfCmd.rfOpCmd.startTime + (i * pPeriodicAdv->interval * RAT_TICKS_IN_1_25MS ) - ((aeRf_t *)pAdvSet->pRfCmds)->auxRfCmd.rfOpCmd.startTime);
    // case the packet offset less then 500 usec - point to the next sync indication
    if (pPeriodicAdv->syncInfo.packetOffset < 500)
    {
      pPeriodicAdv->syncInfo.packetOffset += (pPeriodicAdv->interval * 1250);
      pPeriodicAdv->syncInfo.eventCounter++;
    }
    // case the packet offset is too far - set it to 0
    else if ((pPeriodicAdv->syncInfo.packetOffset / AE_AUX_OFFSET_300_US_UNIT_VALUE) > AE_AUX_OFFSET_MASK)
    {
      pPeriodicAdv->syncInfo.packetOffset = 0;
    }
    // set the offset unit
    else if ((pPeriodicAdv->syncInfo.packetOffset / AE_AUX_OFFSET_30_US_UNIT_VALUE) > AE_AUX_OFFSET_MASK)
    {
      pPeriodicAdv->syncInfo.offsetUnit = AE_AUX_OFFSET_UNITS_300_US;
      offsetUnitVal = AE_AUX_OFFSET_300_US_UNIT_VALUE;
    }
  }
  packetOffsetField = (((pPeriodicAdv->syncInfo.packetOffset / offsetUnitVal) & AE_AUX_OFFSET_MASK) |
                        (pPeriodicAdv->syncInfo.offsetUnit << AE_AUX_OFFSET_SIZE));
  // case channel map update in progress
  if ((pPeriodicAdv->pendingChanUpdate != PERIODIC_ADV_CHANMAP_UPDATE_NOT_PENDING) &&
      (pPeriodicAdv->chanMapUpdateEvent <= pPeriodicAdv->syncInfo.eventCounter))
  {
    // point to the new channel map
    pChanMap = &llPeriodicAdv.chanMap.next;
  }
  //copy the packet offset
  *pBuf++ = LO_UINT16( packetOffsetField );
  *pBuf++ = HI_UINT16( packetOffsetField );
  //copy the interval
  *pBuf++ = LO_UINT16( pPeriodicAdv->interval );
  *pBuf++ = HI_UINT16( pPeriodicAdv->interval );
  //copy the channel map
  *pBuf++ = pChanMap->bitmap[0];
  *pBuf++ = pChanMap->bitmap[1];
  *pBuf++ = pChanMap->bitmap[2];
  *pBuf++ = pChanMap->bitmap[3];
  *pBuf++ = pChanMap->bitmap[4] | (pPeriodicAdv->syncInfo.sca << 5);
  //copy the access address
  osal_memcpy(pBuf,(uint8 *)&pPeriodicAdv->syncInfo.accessAddr,LL_PKT_SYNCH_LEN);
  pBuf += LL_PKT_SYNCH_LEN;
  //copy the CRC init
  *pBuf++ = pPeriodicAdv->syncInfo.crcInit[0];
  *pBuf++ = pPeriodicAdv->syncInfo.crcInit[1];
  *pBuf++ = pPeriodicAdv->syncInfo.crcInit[2];
  //copy the event counter
  *pBuf++ = LO_UINT16( pPeriodicAdv->syncInfo.eventCounter );
  *pBuf++ = HI_UINT16( pPeriodicAdv->syncInfo.eventCounter );
}

/*******************************************************************************
 * @fn          llSetPeriodicHdrFlags
 *
 * @brief       This routine is used to set the Periodic adv Header flags
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       pPeriodicAdv - Pointer to the periodic advertising set.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      totalHdrSize - periodic adv header size.
 */
uint8 llSetPeriodicHdrFlags( llPeriodicAdvSet_t *pPeriodicAdv)
{
  uint8 totalHdrSize = 0;

  pPeriodicAdv->rfPkt.extHdrFlags = 0;
  // check that the host request for CTE
  if ((pPeriodicAdv->cteInfo.enable) &&
      (pPeriodicAdv->cteInfo.count > pPeriodicAdv->txCount))
  {
    // include CTE Info flag
    SET_EXTHDR_FLAG( pPeriodicAdv->rfPkt.extHdrFlags,EXTHDR_FLAG_CTEINFO );
    totalHdrSize += EXTHDR_FLAG_CTEINFO_SIZE;
  }

  // check if the user set the Tx Power option
  if ((TST_AE_PROPS_TX_PWR(pPeriodicAdv->paramsCmd.props)) &&
      (pPeriodicAdv->txCount == 0))
  {
    // include Tx power flag
    SET_EXTHDR_FLAG( pPeriodicAdv->rfPkt.extHdrFlags,EXTHDR_FLAG_TXPWR );
    totalHdrSize += EXTHDR_FLAG_TXPWR_SIZE;
  }
  // check for channel map update indication
  if ((pPeriodicAdv->pendingChanUpdate == PERIODIC_ADV_CHANMAP_UPDATE_PENDING) &&
      (pPeriodicAdv->txCount == 0))
  {
    totalHdrSize += EXTHDR_FLAG_ACAD_SIZE;
  }
  // check that there are more then 1 packet because of periodic data or CTE count
  if ((pPeriodicAdv->numFrags > (pPeriodicAdv->txCount + 1)) ||
     ((pPeriodicAdv->cteInfo.enable) &&
      (pPeriodicAdv->cteInfo.count > (pPeriodicAdv->txCount + 1))))
  {
    // include Aux ptr flag
    SET_EXTHDR_FLAG( pPeriodicAdv->rfPkt.extHdrFlags,EXTHDR_FLAG_AUXPTR );
    totalHdrSize += EXTHDR_FLAG_AUXPTR_SIZE;
  }

  if (totalHdrSize > 0)
  {
    //add 1 byte for the flags
    totalHdrSize++;
  }
  return (totalHdrSize);
}
/*******************************************************************************
 * @fn          llSetupPeriodicHdr
 *
 * @brief       This routine is used to set the content of the Periodic adv Header
 *              buffer.
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       pPeriodicAdv - Pointer to the periodic advertising set.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetupPeriodicHdr( llPeriodicAdvSet_t *pPeriodicAdv )
{
  uint8 *pBuf = pPeriodicAdv->extHdr;
  uint8 extHdrCount = (pPeriodicAdv->extHdrSize > 0)?1:0;

  if ( TST_EXTHDR_FLAG(pPeriodicAdv->rfPkt.extHdrFlags, EXTHDR_FLAG_CTEINFO) )
  {
    // set the CTE Info (length and type)
    *pBuf++ = ((pPeriodicAdv->cteInfo.len | LL_CTE_INFO_TYPE_MASK) &
              ((pPeriodicAdv->cteInfo.type << LL_CTE_INFO_TYPE_OFFSET) | LL_CTE_INFO_TIME_MASK));
    extHdrCount++;
  }

  if ( TST_EXTHDR_FLAG(pPeriodicAdv->rfPkt.extHdrFlags, EXTHDR_FLAG_AUXPTR) )
  {
    uint8  auxCA;
    uint8  auxOffsetUnits;
    uint8  auxPhy;
    uint16 auxRem;

    // determine CA
    auxCA = AE_AUX_CA_0_50_PPM;

    // set the aux offset units based on the total time
    // Note: Spec indicates 30us should be used if time if less than 245700 us.
    // ALT: ((uint32)pAdvSet->otaTimeExtAdv < AE_AUX_OFFSET_UNIT_CUTOFF_TIME) ?
    //      AE_AUX_OFFSET_UNITS_30_US                                         :
    //      AE_AUX_OFFSET_UNITS_300_US;
    auxOffsetUnits = AE_AUX_OFFSET_UNITS_30_US;

    // set the aux PHY
    // Note: The aux PHY specified by the API is +1 the value expected in the
    //       AuxPtr field (sigh).
    auxPhy = (pPeriodicAdv->phy & AE_PHY_CODED_SCHEME_MASK) - 1;

    // store the Offset Units, CA, and Channel Index in first byte of AuxPtr
    *pBuf++ = (auxOffsetUnits << 7) |
              (auxCA << 6)          |
              (pPeriodicAdv->currentChan & AE_CHAN_INDEX_MASK);

    // build remainder of the AuxPtr
    // Note: When the auxOffset is zero, it is expected that the CM0 will
    //       automatically update the Extended Header value, so what's stored
    //       in the Extended Header now does not matter.
    auxRem = (auxPhy << AE_AUX_OFFSET_SIZE) |
             (AE_AUX_OFFSET_AUTO_INSERT & AE_AUX_OFFSET_MASK);

    // store the Aux PHY and Aux Offset
    *pBuf++ = LO_UINT16( auxRem );
    *pBuf++ = HI_UINT16( auxRem );
    extHdrCount += EXTHDR_FLAG_AUXPTR_SIZE;
  }

  if ( TST_EXTHDR_FLAG(pPeriodicAdv->rfPkt.extHdrFlags, EXTHDR_FLAG_TXPWR) )
  {
    // include Tx power flag
    *pBuf++ = (uint8)llConfigTable.userCfgPtr->txPwrTblPtr->txPwrValsPtr[pPeriodicAdv->txPowerIndex].Pout;
    extHdrCount++;
  }

  // check for ACAD
  if ((pPeriodicAdv->extHdrSize > 0) && (pPeriodicAdv->txCount == 0) && (pPeriodicAdv->extHdrSize > extHdrCount))
  {
    if ((pPeriodicAdv->pendingChanUpdate == PERIODIC_ADV_CHANMAP_UPDATE_PENDING) &&
        ((pPeriodicAdv->extHdrSize - extHdrCount) == (EXTHDR_ACAD_CHANMAP_UPDATE_SIZE + 1)))
    {
      //copy the acad data length
      *pBuf++ = EXTHDR_ACAD_CHANMAP_UPDATE_SIZE;
      //copy the acad data type
      *pBuf++ = EXTHDR_ACAD_CHANMAP_UPDATE_TYPE;
      //copy the new channel map
      *pBuf++ = llPeriodicAdv.chanMap.next.bitmap[0];
      *pBuf++ = llPeriodicAdv.chanMap.next.bitmap[1];
      *pBuf++ = llPeriodicAdv.chanMap.next.bitmap[2];
      *pBuf++ = llPeriodicAdv.chanMap.next.bitmap[3];
      *pBuf++ = llPeriodicAdv.chanMap.next.bitmap[4];
      //copy the instant
      *pBuf++ = LO_UINT16( pPeriodicAdv->chanMapUpdateEvent );
      *pBuf++ = HI_UINT16( pPeriodicAdv->chanMapUpdateEvent );
    }
  }
}

#if !defined(DeviceFamily_CC13X4) && !defined(DeviceFamily_CC26X4)
/*******************************************************************************
 * @fn          llSetupPeriodicAdv
 *
 * @brief       This routine is used to setup the periodic advertising command.
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       pAdvSet - Pointer to the advertising set for this command.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 */
llStatus_t llSetupPeriodicAdv( advSet_t *pAdvSet )
{
  llPeriodicAdvSet_t *pPeriodicAdv = llGetPeriodicAdv(pAdvSet->pAdvParam->handle);

  if (pPeriodicAdv == NULL)
  {
    return( LL_STATUS_ERROR_UNKNOWN_CONN_HANDLE );
  }
  pPeriodicAdv->state = PERIODIC_ADV_STATE_PENDING_TRIGGER;
  // set the periodic adv default priority.
  pPeriodicAdv->priority = qosDefaultPriorityPerAdvParameter;
  // set the packet format struct
  SET_ADV_MODE( pPeriodicAdv->rfPkt.extHdrInfo,AE_ADV_MODE_NONCONN_NONSCAN );
  pPeriodicAdv->rfPkt.extHdrConfig = 0;
  SET_EXT_ADV_HDR_CFG_SKIP_ADVA(pPeriodicAdv->rfPkt.extHdrConfig);
  SET_EXT_ADV_HDR_CFG_SKIP_TGTA(pPeriodicAdv->rfPkt.extHdrConfig);
  SETVAR_EXT_ADV_HDR_CFG_DEV_ADDR_TYPE( pPeriodicAdv->rfPkt.extHdrConfig,pAdvSet->ownAddrType );
  SETVAR_EXT_ADV_HDR_CFG_TGT_ADDR_TYPE( pPeriodicAdv->rfPkt.extHdrConfig,pAdvSet->peerAddrType );

  // set the phy according to the AUX_ADV_IND phy
  pPeriodicAdv->rfCmd.phyMode = ((aeRf_t *)pAdvSet->pRfCmds)->auxRfCmd.phyMode;
  pPeriodicAdv->rfCmd.rangeDelay = (pPeriodicAdv->rfCmd.phyMode > BLE5_2M_PHY) ? LL_CODED_RANGE_DELAY_RAT_TICKS : LL_UNCODED_RANGE_DELAY_RAT_TICKS;
  // set the ptr to buffer with ext hdr
  pPeriodicAdv->rfPkt.pExtHeader = pPeriodicAdv->extHdr;
  // set the ptr to Adv data
  pPeriodicAdv->rfPkt.pAdvData = pPeriodicAdv->pData;
  // set the size of Adv data
  pPeriodicAdv->rfPkt.advDataLen = pPeriodicAdv->fragLen;
  pPeriodicAdv->txCount = 0;
  pPeriodicAdv->numChains = MAX(pPeriodicAdv->numFrags,
                               (pPeriodicAdv->cteInfo.enable)?
                                pPeriodicAdv->cteInfo.count:0);
  // set the header flags and return the header size
  pPeriodicAdv->extHdrSize = llSetPeriodicHdrFlags(pPeriodicAdv);
  // calculate the sync indication OTA time
  pPeriodicAdv->otaTime =  MAP_llOctets2Time( pPeriodicAdv->rfCmd.phyMode & 0x03,      // first two bits only
                                             (pPeriodicAdv->rfCmd.phyMode>>2) & 0x01, // scheme
                                             (pPeriodicAdv->extHdrSize + EXTHDR_INFO_SIZE + pPeriodicAdv->fragLen),
                                              MIC_NOT_ENABLED );
  pPeriodicAdv->otaTime += (pPeriodicAdv->cteInfo.enable)?(pPeriodicAdv->cteInfo.len * 8):0;
  if (pPeriodicAdv->numChains > 1)
  {
    pPeriodicAdv->otaTime += AE_MIN_T_MAFS_IN_US;
    // calculate the complete OTA time in post process
    pPeriodicAdv->totalOtaTime = PERIODIC_ADV_MARGIN_TIME_RAT_TICKS;
  }
  else
  {
    pPeriodicAdv->totalOtaTime = US_TO_RAT_TICKS(pPeriodicAdv->otaTime) + PERIODIC_ADV_MARGIN_TIME_RAT_TICKS;
  }
  // set periodic adv header
  llSetupPeriodicHdr(pPeriodicAdv);
  SET_EXTHDR_LEN( pPeriodicAdv->rfPkt.extHdrInfo,pPeriodicAdv->extHdrSize );
  // set the rf params struct
  pPeriodicAdv->rfParam.auxPtrTgtType = TRIGTYPE_AT_ABS_TIME;
  pPeriodicAdv->rfParam.pAdvPkt = (uint8 *)&pPeriodicAdv->rfPkt;
  pPeriodicAdv->rfParam.accessAddress = pPeriodicAdv->syncInfo.accessAddr;
  pPeriodicAdv->rfParam.crcInit0 = pPeriodicAdv->syncInfo.crcInit[0];
  pPeriodicAdv->rfParam.crcInit1 = pPeriodicAdv->syncInfo.crcInit[1];
  pPeriodicAdv->rfParam.crcInit2 = pPeriodicAdv->syncInfo.crcInit[2];

  // Set counter command
  pPeriodicAdv->rfCount.rfOpCmd.cmdNum    = CMD_COUNTER;
  pPeriodicAdv->rfCount.rfOpCmd.status    = RFSTAT_IDLE;
  pPeriodicAdv->rfCount.rfOpCmd.startTime = RAT_TICKS_IN_40US;
  pPeriodicAdv->rfCount.rfOpCmd.startTrig = TRIGTYPE_REL_END_PREV_CMD;
  pPeriodicAdv->rfCount.rfOpCmd.condition = CONDTYPE_RUN_TRUE_STOP_FALSE;
  // Secondary channel RF Counter command next pointer
  pPeriodicAdv->rfCount.rfOpCmd.pNextRfOp = (rfOpCmd_t *)&pPeriodicAdv->rfCmd;

  // set the counter based on the number of additional aux packets needed
  // according to the max value of periodic data or CTE count
  // Note: If the numFrags=0, then there is no secondary channel packet, so
  // this counter will never be used.
  pPeriodicAdv->rfCount.counter = pPeriodicAdv->numChains;

  // set the radio command number
  pPeriodicAdv->rfCmd.rfOpCmd.cmdNum = CMD_BLE5_ADV_PER;
  // set radio status
  pPeriodicAdv->rfCmd.rfOpCmd.status = RFSTAT_IDLE;
  // set the ptr to next radio command op
  pPeriodicAdv->rfCmd.rfOpCmd.pNextRfOp = (rfOpCmd_t *)&pPeriodicAdv->rfCount;
  // set the start time trigger
  pPeriodicAdv->rfCmd.rfOpCmd.startTrig = PAST_TRIG_START_ASAP | TRIGTYPE_AT_ABS_TIME;
  pPeriodicAdv->rfCmd.rfOpCmd.condition = CONDTYPE_RUN_TRUE_STOP_FALSE;

  // set the channel number and enable BLE whitening
  pPeriodicAdv->rfCmd.chan = pPeriodicAdv->currentChan & AE_CHAN_INDEX_MASK;
  SET_WHITENING_BLE( pPeriodicAdv->rfCmd.whitening );

  pPeriodicAdv->rfCmd.pParams = (uint8 *)&pPeriodicAdv->rfParam;
  pPeriodicAdv->rfCmd.pOutput = (uint8 *)&pPeriodicAdv->rfOutput;
  // set the RF command with Tx power value based on index
  pPeriodicAdv->rfCmd.txPower = RfBleDpl_getTxPower(pPeriodicAdv->txPowerIndex);

  return( LL_STATUS_SUCCESS );
}
#endif

/*******************************************************************************
 * @fn          llTrigPeriodicAdv
 *
 * @brief       This routine is used to execute the periodic advertising command.
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       pAdvSet - Pointer to the advertising set for this command.
 * @param       pPeriodicAdv - Pointer to the periodic advertising set for this command.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 */
llStatus_t llTrigPeriodicAdv( advSet_t *pAdvSet, llPeriodicAdvSet_t *pPeriodicAdv )
{
  pPeriodicAdv->state = PERIODIC_ADV_STATE_ENABLE;
  //set the start time
  pPeriodicAdv->startTime = ((aeRf_t *)pAdvSet->pRfCmds)->auxRfCmd.rfOpCmd.startTime + US_TO_RAT_TICKS( pPeriodicAdv->syncInfo.packetOffset);
  pPeriodicAdv->rfCmd.rfOpCmd.startTime = pPeriodicAdv->startTime;
  // set the aux ptr start time
  pPeriodicAdv->rfParam.auxPtrTgtTime = pPeriodicAdv->startTime + US_TO_RAT_TICKS(pPeriodicAdv->otaTime + START_SYNTH_TO_RAT_OFFSET);

  if (llPeriodicAdv.llTask == NULL)
  {
    // allocate scheduler task
    llPeriodicAdv.llTask = MAP_llAllocTask( LL_TASK_ID_PERIODIC_ADVERTISER );
    // pointer to radio operation command
    llPeriodicAdv.llTask->command = (uint32)&pPeriodicAdv->rfCmd;
    // set RF events
    llPeriodicAdv.llTask->rfEvents = RF_EventLastCmdDone   |
                                     RF_EventInternalError |
                                     RF_EventTxDone;

    // callback function for scheduler
    llPeriodicAdv.llTask->setup = llPeriodicAdvSchedSetup;
  }

  // update num active
  llPeriodicAdv.advNumActive++;

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG
#endif //USE_PERIODIC_ADV

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llSetupExtAdvLegacy
 *
 * @brief       This routine is used to setup radio to execute the extended
 *              advertising legacy command.
 *
 * input parameters
 *
 * @param       pAdvSet - Pointer to the advertising set for this command.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 */
llStatus_t llSetupExtAdvLegacy( advSet_t *pAdvSet )
{
  aeLegacyRf_t *pRf;
  uint32 aeStartTime;
  sortedAdv_t *nodePtr = NULL;

  // check input parameter
  LL_ASSERT( pAdvSet != NULL );

  // get pointer to RF command
  pRf = (aeLegacyRf_t *)pAdvSet->pRfCmds;

  // add AE node into AE List and determine its start time.
  aeStartTime = MAP_llAddAdvSortedEntry(pAdvSet, &nodePtr);
  if (aeStartTime==AE_INVALID_START_TIME)
  {
    return( LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED);
  }

  if ( pAdvSet->pAdvParam->txPower == AE_TX_POWER_NO_PREFERENCE )
  {
    // Host doesn't care, so use the current value
    pAdvSet->txPowerIndex = curTxPowerVal;
  }
  else // use the Host's choice
  {
    // use the Host's choice
    pAdvSet->txPowerIndex = RfBleDpl_getTxPowerByTxPowerDbm(pAdvSet->pAdvParam->txPower, 0);
    if (!RfBleDpl_txPowerIsValid( pAdvSet->txPowerIndex))
    {
      /* Check valid Tx Parameter. In case invalid - return with error */
      return LL_STATUS_ERROR_BAD_PARAMETER;
    }
  }
#ifdef USE_RCL
  pRf->advCmd = RCL_CmdBle5Advertiser_DefaultRuntime();

  pRf->advCmd.txPower = RfBleDpl_getTxPower(pAdvSet->txPowerIndex);
  pRf->advCmd.chanMap = pAdvSet->pAdvParam->primChanMap;
  // start trigger
  pRf->advCmd.common.scheduling = RCL_Schedule_AbsTime;
  pRf->advCmd.common.allowDelay = TRUE;
  // start time
  pRf->advCmd.common.timing.absStartTime = MAP_llGetCurrentTime() + RAT_TICKS_IN_1MS;
  // Run in increasing order
  pRf->advCmd.order = 0;
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  if ( pAdvSet->advEvtType == LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT )
  {
    // all HDC Directed Adv must be done in 3.75ms
    pRf->advCmd.highDuty = TRUE;
  }
#endif // ADV_CONN_CFG

  // use common parameters and output
  pRf->advCmd.ctx = &pRf->advParam;
  pRf->advCmd.stats = &pRf->advOutput;
  pRf->advParam = RCL_CtxAdvertiser_DefaultRuntime();
  pRf->advOutput = RCL_StatsAdvScanInit_DefaultRuntime();
#else
  // setup each radio operation, channel by channel
  for (uint8 i=0; i<LL_MAX_NUM_ADV_CHAN; i++)
  {
    // check if the channel map is set
    if ( pAdvSet->pAdvParam->primChanMap & BV(i) )
    {
      // it is, so initialize the radio operation block
      pRf->advCmd[i].rfOpCmd.cmdNum = advEvt2Cmd[ pAdvSet->advEvtType ];

      // set the channel number and enable BLE whitening
      pRf->advCmd[i].chan = advChan[i];
      SET_WHITENING_BLE( pRf->advCmd[i].whitening );
    }
    else // no radio operation for unused channel
    {
      // so set command as a NOP
      pRf->advCmd[i].rfOpCmd.cmdNum = CMD_NOP;
    }

    // common initialization
    pRf->advCmd[i].rfOpCmd.status    = RFSTAT_IDLE;
    pRf->advCmd[i].rfOpCmd.pNextRfOp = (rfOpCmd_t *)&pRf->advCmd[i+1];

    CLR_RFOP_ALT_TRIG_CMD( pRf->advCmd[i].rfOpCmd.startTrig );
    SET_RFOP_PAST_TRIG( pRf->advCmd[i].rfOpCmd.startTrig );

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
    if ( pAdvSet->advEvtType == LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT )
    {
      // all HDC Directed Adv must be done in 3.75ms, so fix at 1.25ms each
      // Note: For this relative timing value, the fist command in the chain
      //       is started immediately.

      // start trigger
      SET_RFOP_TRIG_TYPE( pRf->advCmd[i].rfOpCmd.startTrig,
                          ((i==0) ? TRIGTYPE_AT_ABS_TIME :
                                    TRIGTYPE_REL_PREV_CMD_START) );
      // start time
      pRf->advCmd[i].rfOpCmd.startTime = ((i==0) ? (MAP_llGetCurrentTime() + RAT_TICKS_IN_1MS) :  RAT_TICKS_IN_1_25MS);

      SET_RFOP_COND_RULE( pRf->advCmd[i].rfOpCmd.condition, CONDTYPE_RUN_TRUE_STOP_FALSE );
    }
    else // all other Adv Event Types
#endif // ADV_CONN_CFG
    {
      // Start Adv at an absolute time, but let all subsequent commands (if any)
      // start as quickly as possible.

      // start trigger
      SET_RFOP_TRIG_TYPE( pRf->advCmd[i].rfOpCmd.startTrig,
                          ((i==0) ? TRIGTYPE_AT_ABS_TIME :
                                    TRIGTYPE_NOW) );

      // start time
      pRf->advCmd[i].rfOpCmd.startTime = ((i==0) ? (MAP_llGetCurrentTime() + RAT_TICKS_IN_1MS) :  0);

      // command condition
      SET_RFOP_COND_RULE( pRf->advCmd[i].rfOpCmd.condition,
                          ( (i==2) ? CONDTYPE_NEVER_RUN_NEXT_CMD :
                                     CONDTYPE_RUN_TRUE_STOP_FALSE) );
    }

    // use common parameters and output
    pRf->advCmd[i].pParams = (uint8 *)&pRf->advParam;  //(uint8 *)&advParam;
    pRf->advCmd[i].pOutput = (uint8 *)&pRf->advOutput; //(uint8 *)&advOutput;
  }

  // setup next radio operation command pointer based on Adv event type
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  // Note: HDC Directed advertising is continuous until either a connection is
  //       formed, or a timeout occurs, thus it's command structure is in
  //       a loop. All others end when the last command executes, thus it's
  //       last command structure's next command is null terminated.
  if ( pAdvSet->advEvtType == LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT )
  {
    // set the chain loop by having the last command point to the hdc command
    pRf->advCmd[2].rfOpCmd.pNextRfOp = (rfOpCmd_t *)&pRf->advCmd[3];
    // create the hdc command as copy of the first command which will be use in the chain loop
    MAP_osal_memcpy(&pRf->advCmd[3],&pRf->advCmd[0],sizeof(bleOpCmd_t));
    pRf->advCmd[3].rfOpCmd.pNextRfOp = (rfOpCmd_t *)&pRf->advCmd[1];
    // set the start time
    pRf->advCmd[3].rfOpCmd.startTime = RAT_TICKS_IN_1_25MS;
    SET_RFOP_TRIG_TYPE( pRf->advCmd[3].rfOpCmd.startTrig, TRIGTYPE_REL_PREV_CMD_START );
    SET_RFOP_COND_RULE( pRf->advCmd[3].rfOpCmd.condition, CONDTYPE_RUN_TRUE_STOP_FALSE );
  }
  else // all other Adv Event Types
#endif // ADV_CONN_CFG
  {
    // bring an end to the command chain
    pRf->advCmd[2].rfOpCmd.pNextRfOp = NULL;
  }
#endif
  // save the absolute start time of the advertiser for post-processing
  // Note: The start time is advanced for primary channels, so we need the
  //       original advertisement event start time to keep the advertisement
  //       interval.
#ifdef USE_RCL
  pAdvSet->advStartTime = pRf->advCmd.common.timing.absStartTime;
#else
  pAdvSet->advStartTime = pRf->advCmd[0].rfOpCmd.startTime;
#endif
  // Update the default priority for the advertise set.
  pAdvSet->priority = qosDefaultPriorityAdvParameter;

#ifdef USE_RCL
  /* Set device address */
  // only if address resolution is enabled
  if ( privInfo.addrResolution )
  {
    // check if RPA has changed, and if so, update Adv address
    // Note: Assumes if the local IRK is valid, then the local RPA exists.
    if ( LL_IS_ADDR_TYPE_RPA(pAdvSet->ownAddrType) &&
         !MAP_LL_PRIV_IsZeroIRK( resolvingList[LOCAL_RL_INDEX].IRK ) )
    {
      // update the RPA (whether it has changed or not)
      // Note: advParam.pDeviceAddr points to pAdvSet->ownAddr.
      // Note: It would take just as long (longer actually) to first compare
      //       the address to see if it has changed. Faster to just copy.
      // Note: Sadly, we can't just point advParam.pDeviceAddr to the RL RPA
      //       (if valid) as we could end up changing it while the radio is
      //       using it.
      MAP_osal_memcpy( pAdvSet->ownAddr,
                       resolvingList[LOCAL_RL_INDEX].RPA,
                       B_ADDR_LEN );
    }
  }

  /* Update the ownAddrType to the RF based on the Adv Params value */
  if ( pAdvSet->ownAddrType == LL_DEV_ADDR_TYPE_PUBLIC )
  {
     pRf->advParam.addrType.own = LL_DEV_ADDR_TYPE_PUBLIC;
  }
  else
  {
     pRf->advParam.addrType.own = LL_DEV_ADDR_TYPE_RANDOM;
  }

  MAP_osal_memcpy(pRf->advParam.advA,pAdvSet->ownAddr,LL_DEVICE_ADDR_LEN );
  /* Initialize advertising packet */
  pRf->advPacket.state  = RCL_BufferStatePending;
  pRf->advPacket.numPad = 3;
  pRf->advPacket.pad0   = 2; /* Same padding as in PBE, but this is don't-care */
  pRf->advPacket.pad1   = 1; /* Same padding as in PBE, but this is don't-care */
  pRf->advPacket.pad2   = 0; /* Same padding as in PBE, but this is don't-care */
  // copy the advertising address
  MAP_osal_memcpy(pRf->advPacket.advA,pAdvSet->ownAddr,LL_DEVICE_ADDR_LEN );
  pRf->advPacket.payloadLen = LL_DEVICE_ADDR_LEN;

  /* Initialize scan response packet */
  pRf->scanRspPacket.state  = RCL_BufferStatePending;
  pRf->scanRspPacket.numPad = 3;
  pRf->scanRspPacket.pad0   = 2; /* Same padding as in PBE, but this is don't-care */
  pRf->scanRspPacket.pad1   = 1; /* Same padding as in PBE, but this is don't-care */
  pRf->scanRspPacket.pad2   = 0; /* Same padding as in PBE, but this is don't-care */
  pRf->scanRspPacket.header = LL_PKT_TYPE_SCAN_RSP;
  pRf->scanRspPacket.header |= LL_ADV_HDR_SET_TX_ADD(pRf->scanRspPacket.header,pRf->advParam.addrType.own);
  // copy the advertising address
  MAP_osal_memcpy(pRf->scanRspPacket.advA,pAdvSet->ownAddr,LL_DEVICE_ADDR_LEN );
  pRf->scanRspPacket.payloadLen = LL_DEVICE_ADDR_LEN;

  // setup data for non-directed advertising
  if ( !(pAdvSet->advEvtType == LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT) &&
       !(pAdvSet->advEvtType == LL_ADV_CONNECTABLE_LDC_DIRECTED_EVT) )
  {
    // check if there's adv data
    if ((pAdvSet->pAdvData != NULL) && ( pAdvSet->pAdvData->dataLen > 0))
    {
      MAP_osal_memcpy(&pRf->advPacket.advData,pAdvSet->pAdvData->pData,pAdvSet->pAdvData->dataLen );
      pRf->advPacket.payloadLen += pAdvSet->pAdvData->dataLen;
    }
    if ((pAdvSet->pScanRspData != NULL) && (pAdvSet->pScanRspData->dataLen > 0))
    {
      MAP_osal_memcpy(&pRf->scanRspPacket.scanRspData,pAdvSet->pScanRspData->pData,pAdvSet->pScanRspData->dataLen );
      pRf->scanRspPacket.payloadLen += pAdvSet->pScanRspData->dataLen;
    }
    switch (pAdvSet->advEvtType)
    {
      case LL_ADV_NONCONNECTABLE_UNDIRECTED_EVT:
        pRf->advPacket.header = LL_PKT_TYPE_ADV_NONCONN_IND;
      break;
      case LL_ADV_SCANNABLE_UNDIRECTED_EVT:
        pRf->advPacket.header = LL_PKT_TYPE_ADV_SCAN_IND;
      break;
      default: //LL_ADV_CONNECTABLE_UNDIRECTED_EVT
        pRf->advPacket.header = LL_PKT_TYPE_ADV_IND;
      break;
    }
    pRf->advPacket.header |= LL_ADV_HDR_SET_TX_ADD(pRf->advPacket.header,pRf->advParam.addrType.own);
    pRf->advParam.filterPolicy = pAdvSet->pAdvParam->filterPolicy;
    // use standard accept list table
    if ((pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_CONNECT_IND) ||
        (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_ALL_REQ))
    {
      pRf->advParam.filterListConn = (RCL_FilterList *)(((uint32)&alTable->pAlEntries[0]) - sizeof(uint32));//&alTable->numEntries;
    }
    if ((pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_SCAN_REQ) ||
        (pAdvSet->pAdvParam->filterPolicy == LL_ADV_AL_POLICY_AL_ALL_REQ))
    {
      pRf->advParam.filterListScan = (RCL_FilterList *)(((uint32)&alTable->pAlEntries[0]) - sizeof(uint32));//&alTable->numEntries;
    }
  }
  else
  {
    pRf->advPacket.header = LL_PKT_TYPE_ADV_DIRECT_IND;
    pRf->advPacket.header |= LL_ADV_HDR_SET_TX_ADD(pRf->advPacket.header,pRf->advParam.addrType.own);
    pRf->advPacket.header |= LL_ADV_HDR_SET_RX_ADD(pRf->advPacket.header,pAdvSet->peerAddrType);
    pRf->advPacket.payloadLen = 2 * LL_DEVICE_ADDR_LEN;
    // Copy the target address
    MAP_osal_memcpy(pRf->advPacket.targetA,pAdvSet->peerAddr,LL_DEVICE_ADDR_LEN );
    /* Set peer address */
    pRf->advParam.addrType.peer = pAdvSet->peerAddrType;
    MAP_osal_memcpy(pRf->advParam.peerA,pAdvSet->peerAddr,LL_DEVICE_ADDR_LEN );
  }
  pRf->advPacket.length = pRf->advPacket.payloadLen + 6;
  pRf->scanRspPacket.length = pRf->scanRspPacket.payloadLen + 6;
  // enable the chSel bit in the advertisement ind if we support Algo #2
  if ( deviceFeatureSet.featureSet[1] & (uint8)LL_FEATURE_CHAN_ALGO_2 )
  {
    pRf->advPacket.header |= LL_ADV_HDR_SET_CHSEL(pRf->advPacket.header,LL_CHANNEL_SELECT_ALGO_2);
    pRf->scanRspPacket.header |= LL_ADV_HDR_SET_CHSEL(pRf->scanRspPacket.header,LL_CHANNEL_SELECT_ALGO_2);
  }
  // Provide advertising packet to transmit
  RCL_TxBuffer_put(&pRf->advParam.txBuffers, (RCL_Buffer_TxBuffer *)&pRf->advPacket);
  RCL_TxBuffer_put(&pRf->advParam.txBuffers, (RCL_Buffer_TxBuffer *)&pRf->scanRspPacket);
  // Provide RX buffer
  RCL_MultiBuffer_put(&pRf->advParam.rxBuffers, MAP_llSetupAdvDataEntryQueue());
  // initialize the End Trigger and Time for advertising
  pRf->advCmd.common.timing.relHardStopTime = (pAdvSet->pEnable->duration == 0)?RAT_TICKS_IN_1_28S:
                                              (pAdvSet->pEnable->duration * RAT_TICKS_IN_10MS);
  pRf->advCmd.common.runtime.callback = LL_rclAdvCallback;
  pRf->advCmd.common.runtime.lrfCallbackMask.value = 0;
  pRf->advCmd.common.runtime.rclCallbackMask.value = RCL_EventLastCmdDone.value;

  // only if address resolution is enabled
  if ( privInfo.addrResolution )
  {
    // Enable privIgn
    pRf->advParam.privIgnMode = TRUE;
    // Enable rpaMode
    pRf->advParam.rpaModePeer = TRUE;
    // Enable acceptAll
    pRf->advParam.acceptAllRpaConnectInd = TRUE;
  }
  else
  {
    MAP_LL_PRIV_ClearExtAL( alTable );
    // Disable privIgn
    pRf->advParam.privIgnMode = FALSE;
    // Disable rpaMode
    pRf->advParam.rpaModePeer = FALSE;
    // Disable acceptAll
    pRf->advParam.acceptAllRpaConnectInd = FALSE;
  }

#else
  // set the advertiser's receive queue with one data entry
  // Note: Technically not needed for non-connectable or scannable advertisers.
  pRf->advParam.pRXQ = MAP_llSetupAdvDataEntryQueue();

  // set the Adv RX queue configuration
  pRf->advParam.rxCfg =
    ( RXQ_CFG_AUTOFLUSH_IGNORED_PKT |
      RXQ_CFG_AUTOFLUSH_CRC_ERR_PKT |
      RXQ_CFG_AUTOFLUSH_EMPTY_PKT   |
      RXQ_CFG_INCLUDE_PKT_LEN_BYTE  |
      ((llConfigTable.advPktSuffixPtr->suffixSel & SUFFIX_CRC_FLAG)       ? RXQ_CFG_INCLUDE_CRC      : 0) |
      ((llConfigTable.advPktSuffixPtr->suffixSel & SUFFIX_RSSI_FLAG)      ? RXQ_CFG_APPEND_RSSI      : 0) |
      ((llConfigTable.advPktSuffixPtr->suffixSel & SUFFIX_STATUS_FLAG)    ? RXQ_CFG_APPEND_STATUS    : 0) |
      ((llConfigTable.advPktSuffixPtr->suffixSel & SUFFIX_TIMESTAMP_FLAG) ? RXQ_CFG_APPEND_TIMESTAMP : 0) );

  // set the Adv configuration
  CLR_ADV_CFG( pRf->advParam.advCfg );
  SETVAR_ADV_CFG_FILTER_POLICY( pRf->advParam.advCfg, pAdvSet->pAdvParam->filterPolicy );
  SETVAR_ADV_CFG_DEV_ADDR_TYPE( pRf->advParam.advCfg, pAdvSet->ownAddrType );
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  SETVAR_ADV_CFG_PEER_ADDR_TYPE( pRf->advParam.advCfg, pAdvSet->peerAddrType );
#endif // ADV_CONN_CFG
  SETVAR_ADV_CFG_STRICT_LEN_FILTER( pRf->advParam.advCfg, ADV_CFG_DISCARD_BAD_LEN_MSG );

  // setup Adv and Scan Response lengths and pointers, if applicable
  // Note: Assume there is no data or AL (use peer address). This will work for
  //       both Directed (LDC and HDC) as well has when no data was defined
  //       for non-directed advertising.
  pRf->advParam.advLen       = 0;
  pRf->advParam.scanRspLen   = 0;
  pRf->advParam.pAdvData     = NULL;
  pRf->advParam.pScanRspData = NULL;

  // for Directed Adv, there is no AL table - use Peer address instead
  pRf->advParam.pAcceptList   = (alEntry_t *)pAdvSet->peerAddr;

  // setup data for non-directed advertising
  if ( !(pAdvSet->advEvtType == LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT) &&
       !(pAdvSet->advEvtType == LL_ADV_CONNECTABLE_LDC_DIRECTED_EVT) )
  {
    // check if there's adv data
    if ( pAdvSet->pAdvData )
    {
      pRf->advParam.advLen   = pAdvSet->pAdvData->dataLen;
      pRf->advParam.pAdvData = pAdvSet->pAdvData->pData;
    }

    // check if there's scan response data
    if ( pAdvSet->pScanRspData )
    {
      pRf->advParam.scanRspLen   = pAdvSet->pScanRspData->dataLen;
      pRf->advParam.pScanRspData = pAdvSet->pScanRspData->pData;
    }

    // use standard accept list table
    pRf->advParam.pAcceptList = &alTable->pAlEntries[0];
  }

  // set a pointer to our device address
  pRf->advParam.pDeviceAddr = pAdvSet->ownAddr;

  // initialize the End Trigger and Time for advertising
  // Note: This is not needed for non-Directed advertising, but it doesn't
  //       hurt to leave this here as only Directed advertising is continuous
  //       (i.e. command chain is in a loop). All other advertising ends when
  //       the command chain ends, and there's no way a non-Directed advertiser
  //       can take 1.28s to complete!
  CLR_RFOP_ALT_TRIG_CMD( pRf->advParam.endTrig );
  SET_RFOP_PAST_TRIG( pRf->advParam.endTrig );
  SET_RFOP_TRIG_TYPE( pRf->advParam.endTrig, TRIGTYPE_REL_FIRST_CHAIN_CMD );
  pRf->advParam.endTime = (pAdvSet->pEnable->duration == 0)?RAT_TICKS_IN_1_28S:
                          (pAdvSet->pEnable->duration * RAT_TICKS_IN_10MS);

  // only if address resolution is enabled
  if ( privInfo.addrResolution )
  {
    // enable RPA Mode in Adv configuration
    // Per Erratum #6984.
    // ALT: Do not enable RPA Mode, and allow unresolved RPA if FP=ANY.
    //SET_ADV_CFG_RPA_MODE( advParam.advCfg );

    // enable Privacy Ignore in Adv Configuration
    SET_ADV_CFG_PRIV_IGN_MODE( pRf->advParam.advCfg );

    // clear the Autoflush Ignored Packet flag
    pRf->advParam.rxCfg &= ~RXQ_CFG_AUTOFLUSH_IGNORED_PKT;
  }

  // enable the chSel bit in the advertisement ind if we support Algo #2
  // Note: Only relevent for connectable advertisement packets.
  // Note: There's a Vendor Specific command to change the Local Feature
  //       Set, so just because the build includes this feature, this
  //       does not mean our device supports it for this connection.
  if ( deviceFeatureSet.featureSet[1] & (uint8)LL_FEATURE_CHAN_ALGO_2 )
  {
    SET_ADV_CFG_CHAN_SELECT( pRf->advParam.advCfg );
  }
  else // channel selection algorithm 1
  {
    CLR_ADV_CFG_CHAN_SELECT( pRf->advParam.advCfg );
  }
#endif

  // update the time consumed over the air
  nodePtr->timeConsume = LEGACY_ADV_MAX_TIME_CONSUME;

  return( LL_STATUS_SUCCESS );
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 * @fn          llGetNextOrPreviousExtScanChannelIndex
 *
 * @brief       This function gets the next or previous scan channel index
 *              definition.
 *
 * input parameters
 *
 * @param       getNextOrPrevious - Get the next or previous channel parameter
 *                                  LL_GET_NEXT_SCAN_CHAN / LL_GET_PREV_SCAN_CHAN
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Index in Values 0-2.
 *              Failure: LL_STATUS_ERROR_INVALID_PARAMS.
 */
llStatus_t llGetNextOrPreviousExtScanChannelIndex( uint8 getNextOrPrevious )
{
  uint8 i=0;

  // find number of set bits in channel map - min 1 max 3
  uint8 numChannels = BF(extScanChanMap,LL_SCN_ADV_MAP_CHAN_37,0) + BF(extScanChanMap,LL_SCN_ADV_MAP_CHAN_38,1) + BF(extScanChanMap,LL_SCN_ADV_MAP_CHAN_39,2);

  // Get the index of the current scan channel
#ifdef USE_RCL
  uint8 indexCurrExtScanChan = (extScanCmd.channel - LL_ADV_BASE_CHAN);
#else
  uint8 indexCurrExtScanChan = (extScanCmd.chan - LL_ADV_BASE_CHAN);
#endif
  // Invalid parameters
  if ((getNextOrPrevious != LL_GET_NEXT_SCAN_CHAN) &&
      (getNextOrPrevious != LL_GET_PREV_SCAN_CHAN))
  {
    return LL_STATUS_ERROR_INVALID_PARAMS;
  }

  // In case numChannels that are defined are only 1, return the index of the current scan channel
  if ( numChannels == 1 )
  {
    return (indexCurrExtScanChan);
  }

  // Update the first index to be the current channel index
  // Or the previos one.
  /** GET NEXT INDEX **/
  if (getNextOrPrevious == LL_GET_NEXT_SCAN_CHAN)
  {
    // In case the current index is at the end of the scan channels options,
    // the next one would be the start of the scan channels options.
    if (indexCurrExtScanChan == (LL_MAX_NUM_ADV_CHAN - 1) )
    {
      i = 0;
    }
    else
    {
      i = indexCurrExtScanChan + 1;
    }
  }
  /** GET PREVIOUS INDEX **/
  else if (getNextOrPrevious == LL_GET_PREV_SCAN_CHAN)
  {
    // In case the current index is at the start of the scan channels options,
    // the prev one would be the end of the scan channels options.
    if (indexCurrExtScanChan == 0 )
    {
      i = (LL_MAX_NUM_ADV_CHAN - 1);
    }
    else
    {
      i = indexCurrExtScanChan - 1;
    }
  }

  // find the set bit of the next/prev scan channel
  while (i<LL_MAX_NUM_ADV_CHAN)
  {
    // Need to check if the current bit is set and part of the map file
    /** CHECK IF BIT IS SET **/
    if ( extScanChanMap & BV(i) )
    {
      break;
    }
    // In case the i channel is NOT in the map channels
    // look for the next/prev one active
    else
    {
      /** INCREMENT INDEX **/
      if (getNextOrPrevious == LL_GET_NEXT_SCAN_CHAN)
      {
        // In case the current index is at the end of the scan channels options,
        // the next one would be the start of the scan channels options.
        if (i == (LL_MAX_NUM_ADV_CHAN - 1) )
        {
          i = 0;
        }
        else
        {
          i++;
        }
      }
      /** DECREMENT INDEX **/
      else if (getNextOrPrevious == LL_GET_PREV_SCAN_CHAN)
      {
        // In case the current index is at the start of the scan channels options,
        // the prev one would be the end of the scan channels options.
        if (i == 0 )
        {
          i = (LL_MAX_NUM_ADV_CHAN - 1);
        }
        else
        {
          i--;
        }
      }
    }// !(if ( scanChanMap & BV(i) ))
  }//  while (i<LL_MAX_NUM_ADV_CHAN)

  // If we have reached the end of the number of scan channels
  // return the index of the current scan channel to avoid fault.
  if ( i == LL_MAX_NUM_ADV_CHAN )
  {
    return indexCurrExtScanChan;
  }

  /** RETURN NEXT / PREV INDEX NUM **/
  return i;
}

/*******************************************************************************
 * @fn          llGetFirstExtScanChannelIndex
 *
 * @brief       This function gets the first scan channel index definition.
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Channel Index in Values 37-39.
 *              Fail: LL_INVALID_SCAN_CHAN.
 */
uint8 llGetFirstExtScanChannelIndex( void )
{
  uint8 i;
  // Go over the scan channels and return the first that is
  // not NULL
  for (i=0; i<LL_MAX_NUM_ADV_CHAN; i++)
  {
    if ( extScanChanMap & BV(i) )
    {
      return (LL_ADV_BASE_CHAN + i);
    }
  }

  // In case of failure, no mapped scan channels at all.
  return LL_INVALID_SCAN_CHAN;
}

/*******************************************************************************
 * @fn          llSetupExtScan
 *
 * @brief       This routine is used to setup radio to execute the extended
 *              scanner command.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 */
llStatus_t llSetupExtScan( void )
{
#ifdef USE_RCL
  // init radio command
  extScanCmd = RCL_CmdScanner_DefaultRuntime();
  extScanCmd.common.timing.absStartTime = MAP_llGetCurrentTime() + RAT_TICKS_IN_1MS;
  // save the absolute start time of the scanner for post-processing
  extScanInfo->scanStartTime = extScanCmd.common.timing.absStartTime;
  // set start trigger
  extScanCmd.common.scheduling = RCL_Schedule_AbsTime;
  extScanCmd.common.allowDelay = TRUE;
  // set the channel number
  extScanCmd.channel = llGetFirstExtScanChannelIndex();
  if (extScanCmd.channel == LL_INVALID_SCAN_CHAN)
  {
    return LL_STATUS_ERROR_INVALID_PARAMS;
  }
  // use common parameters and output
  extScanCmd.ctx = &extScanParam;
  extScanCmd.stats = &extScanOutput;
  extScanParam = RCL_CtxScanInit_DefaultRuntime();
  extScanOutput = RCL_StatsAdvScanInit_DefaultRuntime();
  // set the Scan receive buffers
  MAP_llSetupScanDataEntryQueue();
  // set active or passive
  extScanCmd.activeScan = extScanInfo->pScanParam->extScanParam[extScanIndex].scanType;
  // set address type
  extScanParam.addrType.own = extScanInfo->ownAddrType;
  // set filter policy
  extScanParam.filterPolicy = extScanInfo->pScanParam->scanFilterPolicy;
  // set address
  MAP_osal_memcpy(extScanParam.ownA,extScanInfo->ownAddr,B_ADDR_LEN);

  if ( (extScanInfo->pEnable->dupFiltering == LL_FILTER_REPORTS_ENABLE) ||
       (extScanInfo->pEnable->dupFiltering == LL_FILTER_REPORTS_RESET_EACH_SCAN_PERIOD) )
  {
    // check the Scan accept list policy
    if ( (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST) ||
         ((privInfo.addrResolution) &&
         (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST_EXT)) )
    {
      // use standard accept list
      extScanParam.filterList = (RCL_FilterList *)((uint32)(&alTable->pAlEntries[0]) - sizeof(uint32));

      // clear any ignore bits that might be set
      MAP_AL_ClearIgnoreList( alTable );
    }
    else // LL_SCAN_AL_POLICY_ANY_ADV_PKTS
    {
      // use alternate accept list for duplicate filtering during scan
      extScanParam.filterList =  (RCL_FilterList *)((uint32)(&alTableScan->pAlEntries[0]) - sizeof(uint32));

      // clear alternate accept list used for duplicate filtering
      MAP_AL_Scan_Init( alTableScan );
    }
  }
  else // duplicate filtering disabled
  {
    // use standard accept list
    extScanParam.filterList = (RCL_FilterList *)((uint32)(&alTable->pAlEntries[0]) - sizeof(uint32));

    // clear any ignore bits that might be set
    MAP_AL_ClearIgnoreList( alTable );
  }

  // update timeout when command is actually about to start
  extScanCmd.common.timing.relGracefulStopTime = extScanInfo->pScanParam->extScanParam[extScanIndex].scanWindow * RAT_TICKS_IN_625US;
  // set rf callback and events
  extScanCmd.common.runtime.callback = LL_rclScanCallback;
  extScanCmd.common.runtime.lrfCallbackMask.value = LRF_EventRxOk.value;
  extScanCmd.common.runtime.rclCallbackMask.value =  RCL_EventLastCmdDone.value |
                                                     RCL_EventCmdStarted.value  |
                                                     RCL_EventRxEntryAvail.value;
#else
  // init radio command
  extScanCmd.rfOpCmd.cmdNum    = CMD_BLE5_SCANNER;
  extScanCmd.rfOpCmd.status    = RFSTAT_IDLE;
  extScanCmd.rfOpCmd.pNextRfOp = NULL;
  extScanCmd.rfOpCmd.startTime = MAP_llGetCurrentTime() + RAT_TICKS_IN_1MS;

  // save the absolute start time of the scanner for post-processing
  // Note: The start time has to be adjusted for AE packets because they cause
  //       the CM0 to end when a secondary channel is followed.
  extScanInfo->scanStartTime = extScanCmd.rfOpCmd.startTime;

  // set start trigger
  SET_RFOP_TRIG_TYPE( extScanCmd.rfOpCmd.startTrig, TRIGTYPE_AT_ABS_TIME );
  SET_RFOP_PAST_TRIG( extScanCmd.rfOpCmd.startTrig );

  // set the command condition
  SET_RFOP_COND_RULE( extScanCmd.rfOpCmd.condition, CONDTYPE_NEVER_RUN_NEXT_CMD );

  // set the channel number
  extScanCmd.chan = llGetFirstExtScanChannelIndex();
  if (extScanCmd.chan == LL_INVALID_SCAN_CHAN)
  {
    return LL_STATUS_ERROR_INVALID_PARAMS;
  }
  // enable BLE whitening
  SET_WHITENING_BLE( extScanCmd.whitening );

  // use common parameters and output
  extScanCmd.pParams = (uint8 *)&extScanParam;
  extScanCmd.pOutput = (uint8 *)&extScanOutput;

  // set the Scan receive queue
  extScanParam.pRXQ = MAP_llSetupScanDataEntryQueue();

  // set Scan Rx queue configuration
  extScanParam.rxCfg =
    ( RXQ_CFG_AUTOFLUSH_IGNORED_PKT |
      RXQ_CFG_AUTOFLUSH_CRC_ERR_PKT |
      RXQ_CFG_AUTOFLUSH_EMPTY_PKT   |
      RXQ_CFG_INCLUDE_PKT_LEN_BYTE  |
      ((llConfigTable.scanPktSuffixPtr->suffixSel & SUFFIX_CRC_FLAG)       ? RXQ_CFG_INCLUDE_CRC      : 0) |
      ((llConfigTable.scanPktSuffixPtr->suffixSel & SUFFIX_RSSI_FLAG)      ? RXQ_CFG_APPEND_RSSI      : 0) |
      ((llConfigTable.scanPktSuffixPtr->suffixSel & SUFFIX_STATUS_FLAG)    ? RXQ_CFG_APPEND_STATUS    : 0) |
      ((llConfigTable.scanPktSuffixPtr->suffixSel & SUFFIX_TIMESTAMP_FLAG) ? RXQ_CFG_APPEND_TIMESTAMP : 0) );

  // update the RxConfig if SCAN_OPTIMIZATION is enabled
  MAP_llSetRxCfg();

  // set Scan configuration
  CLR_SCAN_CFG( extScanParam.scanCfg );
  SETVAR_SCAN_CFG_FILTER_POLICY( extScanParam.scanCfg, extScanInfo->pScanParam->scanFilterPolicy );
  SETVAR_SCAN_CFG_ACTIVE_SCAN( extScanParam.scanCfg, extScanInfo->pScanParam->extScanParam[extScanIndex].scanType );
  SETVAR_SCAN_CFG_DEV_ADDR_TYPE( extScanParam.scanCfg, extScanInfo->ownAddrType );
  // TODO: NEEDED?
  //SETVAR_SCAN_CFG_STRICT_LEN_FILTER( extScanParam.scanCfg, SCAN_CFG_DISCARD_ILLEGAL_LEN );

  // set backoff parameters
  // Note: The Upper Limit is kept as a binary log.
  extScanParam.backoffCount = 1;
  SETVAR_SCAN_BACKOFF_CFG_UL( extScanParam.backoffParam, 0 );
  SETVAR_SCAN_BACKOFF_CFG_LAST_SUCCCEEDED( extScanParam.backoffParam, 0 );
  SETVAR_SCAN_BACKOFF_CFG_LAST_FAILED( extScanParam.backoffParam, 0 );

  // set random state value
  MAP_LL_ENC_GenerateTrueRandNum( (uint8 *)&extScanParam.randState, sizeof(uint16) );

  // set a pointer to our device address
  extScanParam.pDeviceAddr = extScanInfo->ownAddr;

  // setup the accept list based on duplicate filtering and accept list policy
  if ( (extScanInfo->pEnable->dupFiltering == LL_FILTER_REPORTS_ENABLE) ||
       (extScanInfo->pEnable->dupFiltering == LL_FILTER_REPORTS_RESET_EACH_SCAN_PERIOD) )
  {
    // set extended filter configuration
    SETVAR_EXT_SCAN_FILTER_CFG( extScanParam.extFltrCfg,
                                EXT_SCAN_CFG_CHECK_ADI        |
                                EXT_SCAN_CFG_AUTO_AL_IGNORE   |
                                EXT_SCAN_CFG_AUTO_ADI_PROCESS );

    // instruct radio to auto-set the AL ignore bit
    // Note: This operation is only valid for Scan.
    SET_SCAN_CFG_AUTO_SET_AL_IGNORE( extScanParam.scanCfg );

    // check the Scan accept list policy
    if ( (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST) ||
         ((privInfo.addrResolution) &&
          (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST_EXT)) )
    {
      // use standard accept list
      extScanParam.pAcceptList = &alTable->pAlEntries[0];

      // clear any ignore bits that might be set
      MAP_AL_ClearIgnoreList( alTable );
    }
    else // LL_SCAN_AL_POLICY_ANY_ADV_PKTS
    {
      // use alternate accept list for duplicate filtering during scan
      extScanParam.pAcceptList =  &alTableScan->pAlEntries[0];

      // clear alternate accept list used for duplicate filtering
      MAP_AL_Scan_Init( alTableScan );
    }
  }
  else // duplicate filtering disabled
  {
    // set extended filter configuration
    CLR_EXT_SCAN_FILTER_CFG( extScanParam.extFltrCfg );

    // instruct radio to not auto-set the ignore bit
    CLR_SCAN_CFG_AUTO_SET_AL_IGNORE( extScanParam.scanCfg );

    // use standard accept list
    extScanParam.pAcceptList = &alTable->pAlEntries[0];

    // clear any ignore bits that might be set
    MAP_AL_ClearIgnoreList( alTable );
  }

  // setup adiList
  extScanParam.pAdiList = adiList;

  // initialize adiList
  for (uint8 i=0; i<AE_MAX_NUM_SID; i++) adiList[i] = EXT_SCAN_ADI_INIT;

#ifdef QUAL_TEST
  // maxWaitForAux is a feature that can be used to get the device to enter
  // power saving mode if the wait time for the AUX channel is long.
  // If maxWaitForAux is set to 0, the feature is disabled, and the radio
  // will wait for the AUX packet whatever time is signaled in the AUX pointer.
  // When maxWaitForAux is set to 0xFFFF, this time limit is set to 16.3 ms.
  extScanParam.maxWaitForAux = 0;
#else
  // set max wait time for aux channel
  extScanParam.maxWaitForAux = 0xFFFF;
#endif

  // initialize Timeout Trigger and Time based on Scan window
  CLR_RFOP_ALT_TRIG_CMD( extScanParam.timeoutTrig );
  SET_RFOP_PAST_TRIG( extScanParam.timeoutTrig );
  SET_RFOP_TRIG_TYPE( extScanParam.timeoutTrig, TRIGTYPE_AT_ABS_TIME );
  // update timeout when command is actually about to start
  extScanParam.timeoutTime = extScanCmd.rfOpCmd.startTime +
                             (extScanInfo->pScanParam->extScanParam[extScanIndex].scanWindow * RAT_TICKS_IN_625US);

  // initialize the End Trigger and Time based on Scan Interval
  CLR_RFOP_ALT_TRIG_CMD( extScanParam.endTrig );
  SET_RFOP_PAST_TRIG( extScanParam.endTrig );
  SET_RFOP_TRIG_TYPE( extScanParam.endTrig, TRIGTYPE_NEVER );
  extScanParam.endTime = 0;

  // clear unused fields
  // Note: These are read-only once the command starts.
  extScanParam.rxStartTime  = 0;
  extScanParam.rxListenTime = 0;
  extScanParam.chan         = 0;
  extScanParam.phyMode      = 0;

  // only if address resolution is enabled
  // Note: RPA Mode is no longer needed for Scanner since an unresolved RPA
  //       is accepted if the filter policy is ANY (i.e. if AL is not used).
  //       In this case, the RPA is accepted by the PHY if it meets the usual
  //       PHY AdvA filter checking, and rejected otherwise.
  //if ( privInfo.addrResolution )
  //{
  //  // enable rpaMode in Scan configuration
  //  SET_SCAN_CFG_RPA_MODE( extScanParam.scanCfg );
  //}
#endif

  // use default Tx Power
  extScanCmd.txPower = RfBleDpl_getTxPower(curTxPowerVal);

  uint8 phyMode;
  // set the primary PHY; check if there's more than one Scan primary PHY
  switch( extScanInfo->pScanParam->scanPhys )
  {
    case LL_PHY_1_MBPS:
      phyMode = BLE5_1M_PHY;
      break;

    case LL_PHY_CODED:
      // OPT: ALLOW USER TO SPECIFY THE CODED SCHEME FOR AUX_SCAN_REQ?
      phyMode = BLE5_CODED_PHY;
      break;

    default:
      // Note: The index is 0 for 1M and 1 for Coded, so 2*extScanIndex provides
      //       a value of 0 for PHY 1M, and 2 for PHY Coded.
      // OPT: ALLOW USER TO SPECIFY THE CODED SCHEME FOR AUX_SCAN_REQ?
      phyMode = extScanIndex << 1;
      break;
  }

#ifdef USE_RCL
  extScanCmd.common.phyFeatures = phyMode;
#else
  extScanCmd.phyMode = phyMode;
  extScanCmd.rangeDelay =  ( extScanCmd.phyMode == BLE5_1M_PHY ) ? LL_UNCODED_RANGE_DELAY_RAT_TICKS : LL_CODED_RANGE_DELAY_RAT_TICKS;
#endif //USE_RCL

  return( LL_STATUS_SUCCESS );
}
#ifdef USE_PERIODIC_SCAN
/*******************************************************************************
 * @fn          llSetupPeriodicScan
 *
 * @brief       This routine is used to setup the periodic scan command.
 *
 * @design      /ref did_286039104
 *
 * input parameters
 *
 * @param       pPeriodicScan - Pointer to the periodic scan for this command.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 */
llStatus_t llSetupPeriodicScan( llPeriodicScanSet_t *pPeriodicScan )
{
#if !defined(DeviceFamily_CC26X1) && !defined(DeviceFamily_CC13X4) && !defined(DeviceFamily_CC26X4)
  pPeriodicScan->rfCmd.rfOpCmd.cmdNum    = CMD_BLE5_SCANNER_PER;
#else
  pPeriodicScan->rfCmd.rfOpCmd.cmdNum    = 0x1826;
#endif
  pPeriodicScan->rfCmd.rfOpCmd.status    = RFSTAT_IDLE;
  pPeriodicScan->rfCmd.rfOpCmd.pNextRfOp = NULL;

  // set the command condition
  SET_RFOP_COND_RULE( pPeriodicScan->rfCmd.rfOpCmd.condition, CONDTYPE_NEVER_RUN_NEXT_CMD );

  // enable BLE whitening
  SET_WHITENING_BLE( pPeriodicScan->rfCmd.whitening );

  // set the periodic scan default priority.
  pPeriodicScan->priority = qosDefaultPriorityPerScnParameter;

  // use default Tx Power
  pPeriodicScan->rfCmd.txPower = 0;

  // use common parameters and output
  pPeriodicScan->rfCmd.pParams = (uint8 *)&pPeriodicScan->rfParam;
  pPeriodicScan->rfCmd.pOutput = (uint8 *)&llPeriodicScan.rfOutput;

  // set the Scan receive queue
  pPeriodicScan->rfParam.pRXQ = llSetupPeriodicScanDataEntryQueue();

  // set Scan Rx queue configuration
  pPeriodicScan->rfParam.rxCfg =
    ( RXQ_CFG_AUTOFLUSH_IGNORED_PKT |
      RXQ_CFG_AUTOFLUSH_CRC_ERR_PKT |
      RXQ_CFG_AUTOFLUSH_EMPTY_PKT   |
      RXQ_CFG_INCLUDE_PKT_LEN_BYTE  |
      RXQ_CFG_INCLUDE_CRC           |
      RXQ_CFG_APPEND_RSSI           |
      RXQ_CFG_APPEND_STATUS         |
      RXQ_CFG_APPEND_TIMESTAMP );

  // set Scan configuration
  CLR_SCAN_CFG( pPeriodicScan->rfParam.scanCfg );

  // instruct radio to not auto-set the ignore bit
  CLR_SCAN_CFG_AUTO_SET_AL_IGNORE( pPeriodicScan->rfParam.scanCfg );

  // initialize adiList
  //for (uint8 i=0; i<AE_MAX_NUM_SID; i++) adiList[i] = EXT_SCAN_ADI_INIT;

#ifdef QUAL_TEST
  // maxWaitForAux is a feature that can be used to get the device to enter
  // power saving mode if the wait time for the AUX channel is long.
  // If maxWaitForAux is set to 0, the feature is disabled, and the radio
  // will wait for the AUX packet whatever time is signaled in the AUX pointer.
  // When maxWaitForAux is set to 0xFFFF, this time limit is set to 16.3 ms.
  pPeriodicScan->rfParam.maxWaitForAux = 0;
#else
  // set max wait time for aux channel
  pPeriodicScan->rfParam.maxWaitForAux = 0xFFFF;
#endif

  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * @fn          llTrigPeriodicScan
 *
 * @brief       This routine is used to execute the periodic scan command.
 *
 * @design      /ref did_286039104
 *
 * input parameters
 *
 * @param       pPeriodicScan - Pointer to the periodic scan for this command.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 */
llStatus_t llTrigPeriodicScan( llPeriodicScanSet_t *pPeriodicScan )
{
  // update the status
  pPeriodicScan->state = PERIODIC_SCAN_STATE_SYNCING_ACTIVE;
  // update event counter
  pPeriodicScan->eventCounter = pPeriodicScan->syncInfo.eventCounter;
  // set start trigger
  SET_RFOP_TRIG_TYPE( pPeriodicScan->rfCmd.rfOpCmd.startTrig, TRIGTYPE_AT_ABS_TIME );
  SET_RFOP_PAST_TRIG( pPeriodicScan->rfCmd.rfOpCmd.startTrig );
  pPeriodicScan->startTime = pPeriodicScan->startTime - (LL_JITTER_CORRECTION + LL_RX_RAMP_OVERHEAD);
  pPeriodicScan->rfCmd.rfOpCmd.startTime = pPeriodicScan->startTime;
  // set timeout trigger
  SET_RFOP_TRIG_TYPE( pPeriodicScan->rfParam.timeoutTrig, TRIGTYPE_REL_CMD_START );
  // set the timeout time
  pPeriodicScan->rfParam.timeoutTime = (LL_JITTER_CORRECTION) +
                                       LL_RX_RAMP_OVERHEAD +
                                       (RAT_TICKS_FOR_PERIODIC_SCAN_WIN_SIZE) +
                                       LL_RX_SYNCH_OVERHEAD;
  // check if we're using coded
  if ( pPeriodicScan->phy > BLE5_2M_PHY )
  {
    pPeriodicScan->rfParam.timeoutTime += LL_RX_SYNCH_OVERHEAD_CODED;
  }
  // set end trigger
  SET_RFOP_TRIG_TYPE( pPeriodicScan->rfParam.endTrig, TRIGTYPE_REL_CMD_START );
  // set end time
  pPeriodicScan->rfParam.endTime = (((pPeriodicScan->interval * *llConfigTable.connEvtCutoff) / 100) * RAT_TICKS_IN_1_25MS) -
                                     (2 * RAT_TICKS_IN_150US);
  // set the channel number
  pPeriodicScan->rfCmd.chan = llSetNextPeriodicAdvChan( &pPeriodicScan->chanMap.current,
                                                        pPeriodicScan->syncInfo.accessAddr,
                                                        pPeriodicScan->eventCounter );
  // set device address and address type
  SETVAR_SCAN_CFG_DEV_ADDR_TYPE( pPeriodicScan->rfParam.scanCfg, pPeriodicScan->ownAddrType );
  pPeriodicScan->rfParam.pDeviceAddr = ADDRTYPE_TO_OWNADDR(pPeriodicScan->ownAddrType);

  // set the access address
  pPeriodicScan->rfParam.accessAddress = pPeriodicScan->syncInfo.accessAddr;
  // set the CRC init
  pPeriodicScan->rfParam.crcInit0 = pPeriodicScan->syncInfo.crcInit[0];
  pPeriodicScan->rfParam.crcInit1 = pPeriodicScan->syncInfo.crcInit[1];
  pPeriodicScan->rfParam.crcInit2 = pPeriodicScan->syncInfo.crcInit[2];
  // set the PHY
  // Note: Mask off the MSBit which indicates Coded Scheme.
  if ( (pPeriodicScan->phy == BLE5_1M_PHY) ||
       (pPeriodicScan->phy == BLE5_2M_PHY) )
  {
    pPeriodicScan->rfCmd.phyMode = pPeriodicScan->phy;

    // default range delay
    pPeriodicScan->rfCmd.rangeDelay = LL_UNCODED_RANGE_DELAY_RAT_TICKS;
  }
  else // Coded
  {
    pPeriodicScan->rfCmd.phyMode = (pPeriodicScan->phy == BLE5_S2_PHY) ?
                                    BLE5_CODED_S2_PHY:BLE5_CODED_S8_PHY;

    // set range delay
    // Note: This is for Long Range (worst case distance of 1km, or 4us).
    pPeriodicScan->rfCmd.rangeDelay = LL_CODED_RANGE_DELAY_RAT_TICKS;
  }
  // in case there is no currently active scan
  if (llPeriodicScan.scanNumActive == 0)
  {
    // allocate ll task
    llPeriodicScan.llTask = MAP_llAllocTask( LL_TASK_ID_PERIODIC_SCANNER );
    // pointer to radio operation command
    llPeriodicScan.llTask->command = (uint32)&pPeriodicScan->rfCmd;
    // set RF events
    llPeriodicScan.llTask->rfEvents = RF_EventLastCmdDone   |
                                      RF_EventInternalError |
                                      RF_EventRxEntryDone;

    // callback function for scheduler
    llPeriodicScan.llTask->setup = llPeriodicScanSchedSetup;

    if ( llState == LL_STATE_IDLE )
    {
      llState = LL_STATE_PERIODIC_SCAN;
      // schedule this task
      MAP_llScheduler();
    }
  }
  return( LL_STATUS_SUCCESS );
}

/*******************************************************************************
 * @fn          llResetPeriodicScan
 *
 * @brief       This routine is used to reset the Periodic Scanner syncing procedure
 *
 * @design      /ref did_286039104
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
void llResetPeriodicScan( void )
{
  halIntState_t cs;

  HAL_ENTER_CRITICAL_SECTION(cs);
  // check that we are in syncing process
  if ((llPeriodicScan.createSync != NULL) &&
      (llPeriodicScan.createSync->state == PERIODIC_SCAN_STATE_SYNCING_ACTIVE))
  {
    if (llPeriodicScan.scanNumActive == 0)
    {
      // free the associated task block
      MAP_llFreeTask( &llPeriodicScan.llTask );
    }
    // reset the state to looking for sync info
    llPeriodicScan.createSync->state = PERIODIC_SCAN_STATE_SYNCING_PENDING;
    llPeriodicScan.createSync->terminate = 0;
  }
  HAL_EXIT_CRITICAL_SECTION(cs);
}
#endif
#endif // SCAN_CFG

#ifdef USE_AE
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llGetRandomChannelMapIndex
 *
 * @brief       This routine is used to get primary channel index randomaly
 *
 * input parameters
 *
 * @param       primary channel Map as bitwise
 *              bit 0 for channel 37, bit 1 for channel 38 and bit 2 for channel 39
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      0 for channel 37 or 1 for channel 38 or 2 for channel 39.
 */
uint8 llGetRandChannelMapIndex( uint8 channelMap)
{
  uint8 i,j = 0;
  uint8 randIndex;
  // find number of set bits in channel map - min 1 max 3
  uint8 numChannels = BF(channelMap,LL_ADV_CHAN_37,0) + BF(channelMap,LL_ADV_CHAN_38,1) + BF(channelMap,LL_ADV_CHAN_39,2);

  // get randomaly index between 0 and (numChannels -1)
  randIndex = (uint8)(MAP_LL_ENC_GeneratePseudoRandNum() % numChannels);
  // find the set bit with the random index
  for (i=0; i<LL_MAX_NUM_ADV_CHAN; i++)
  {
    if ( channelMap & BV(i) )
    {
      if ( j == randIndex )
      {
        break;
      }
      else
      {
        j++;
      }
    }
  }
  return i;
}

/*******************************************************************************
 * @fn          llNextChanIndex
 *
 * @brief       This routine is used to find the next secondary channel index.
 *
 * input parameters
 *
 * @param       eventCounter - The Advertising Event counter.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      The channel index.
 */
uint8 llNextChanIndex( uint16 eventCounter )
{
  uint8 chanIndex = 0xFF;
  uint8 counter = 0;
  uint8 i;
  uint8 j;

  eventCounter = eventCounter % secondaryAdvChannelMapPopCount;

  // the used channel map uses 1 bit per data channel, or 5 bytes for 37 chans
  for (i=0; i<LL_NUM_BYTES_FOR_CHAN_MAP; i++)
  {
    for (j=0; j<BITS_PER_BYTE; j++)
    {
      // check if the channel is used; only interested in used channels
      if ( (secondaryAdvChannelMap[i] >> j) & 1 )
      {
        if (counter == eventCounter)
        {
          chanIndex = (i * 8) + j;
          break;
        }
        else
        {
          counter++;
        }
      }
    }
    if (chanIndex != 0xFF)
    {
      break;
    }
  }
  return chanIndex;
}
#endif

/*******************************************************************************
 * @fn          llSetupExtendedAdvData
 *
 * @brief       This routine is used to setup the Extended Advertisement or Scan
 *              Scan Response data.
 *
 * input parameters
 *
 * @param       pAdvSet - Pointer to the advertising set for this command.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetupExtendedAdvData( advSet_t *pAdvSet )
{
  // only readjust flags and aux header size if not connectable
  // Note: Allowing subsequent calculation of number of fragments, and
  //       the fragment lengths won't affect connectable as this will be
  //       handled in the TxDone ISR.
  if ( !TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps) )
  {
    // check if fragmentation is needed
    if ( pAdvSet->dataLen > pAdvSet->maxAvailData )
    {
      // set auxPtr and ADI flags in extHdrFlags
      SET_EXTHDR_FLAG( pAdvSet->auxHdrFlags,
                       EXTHDR_FLAG_AUXPTR | EXTHDR_FLAG_ADI );
    }
    else // data fits in first aux PDU
    {
      // clear auxPtr flag in extHdrFlags
      CLR_EXTHDR_FLAG( pAdvSet->auxHdrFlags,
                       EXTHDR_FLAG_AUXPTR );

      // set ADI flag in extHdrFlags
      SET_EXTHDR_FLAG( pAdvSet->auxHdrFlags,
                       EXTHDR_FLAG_ADI );
    }

    // update size of Extended Header buffer
    pAdvSet->auxExtHdrSize = MAP_llGetExtHdrLen( pAdvSet->auxHdrFlags );

    // re-determine the amount of space available in packet for data
    // Note: ACAD currently not supported.
    pAdvSet->maxAvailData = AE_MAX_ADV_PAYLOAD_LEN - (pAdvSet->auxExtHdrSize+1);

    // calculate the number of fragments needed
    pAdvSet->numFrags = (pAdvSet->dataLen / pAdvSet->maxAvailData)  +
                        ((pAdvSet->dataLen % pAdvSet->maxAvailData) ?
                        1                                           :
                        0);

    // find the fragment length
    pAdvSet->fragLen = (pAdvSet->numFrags == 1) ?
                        pAdvSet->dataLen        :
                        pAdvSet->maxAvailData;

    // find the last fragment length
    pAdvSet->lastFragLen = (pAdvSet->numFrags == 1) ?
                            pAdvSet->fragLen        :
                           (pAdvSet->dataLen -
                            ((pAdvSet->numFrags-1)*pAdvSet->fragLen));
  }
}

/*******************************************************************************
 * @fn          llSetupExtData
 *
 * @brief       This routine is used to setup the Advertisement or Scan
 *              Scan Response data.
 *
 * input parameters
 *
 * @param       pAdvSet - Pointer to the advertising set for this command.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetupExtData( advSet_t *pAdvSet )
{
  // assume no data
  pAdvSet->numFrags    = 0;
  pAdvSet->dataLen     = 0;
  pAdvSet->fragLen     = 0;
  pAdvSet->lastFragLen = 0;
  pAdvSet->pData       = NULL;

  // determine the amount of space available in packet for data
  // Note: ACAD currently not supported.
  pAdvSet->maxAvailData = AE_MAX_ADV_PAYLOAD_LEN - (pAdvSet->auxExtHdrSize+1);

  // determine if AUX_ADV_IND is needed
  if ( TST_EXTHDR_FLAG(pAdvSet->extHdrFlags, EXTHDR_FLAG_AUXPTR) )
  {
    // there's at least one secondary channel packet
    pAdvSet->numFrags = 1;

    // check the advertising event properties
    // Note: Connectable can only have data in the AUX_ADV_IND PDU.
    if ( TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps) ||
         (!TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps) &&
          !TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps)) )

    {
      // check if there's data to be sent
      if ( pAdvSet->pAdvData        &&
           pAdvSet->pAdvData->pData &&
           pAdvSet->pAdvData->dataLen )
      {
        // set generic data pointer
        pAdvSet->pData   = pAdvSet->pAdvData->pData;
        pAdvSet->dataLen = pAdvSet->pAdvData->dataLen;
      }
    }
    else if ( TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps) )
    {
      // check if there's data to be sent
      if ( pAdvSet->pScanRspData        &&
           pAdvSet->pScanRspData->pData &&
           pAdvSet->pScanRspData->dataLen )
      {
        // set generic data pointer
        pAdvSet->pData   = pAdvSet->pScanRspData->pData;
        pAdvSet->dataLen = pAdvSet->pScanRspData->dataLen;
      }
    }

    // check if there is any data
    if ( pAdvSet->pData && pAdvSet->dataLen )
    {
      // determine Adv data length, and if fragmentation is needed

      // check if this will not be a legacy advertisement
#ifdef USE_AE
      if ( !TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
      {
        MAP_llSetupExtendedAdvData(pAdvSet);
      }
#endif
    }
  }

  return;
}
#endif

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * @fn          llSetupExtInit
 *
 * @brief       This function sets up the Initiator for a connection.
 *
 * input parameters
 *
 * @param       connId - Connection ID that Init will attempt to start.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetupExtInit( uint8 connId )
{
  llConnState_t *connPtr = MAP_llDataGetConnPtr( connId );
#ifdef USE_RCL
  uint8 ownAddrType = 0;
  uint8 peerAddrType = 0;

  // init radio command
  extInitCmd = RCL_CmdInitiator_DefaultRuntime();
  extInitCmd.common.timing.absStartTime = MAP_llGetCurrentTime() + RAT_TICKS_IN_1MS;
#else
  extInitCmd.rfOpCmd.cmdNum    = CMD_BLE5_INITIATOR;
  extInitCmd.rfOpCmd.status    = RFSTAT_IDLE;
  extInitCmd.rfOpCmd.pNextRfOp = NULL;
  extInitCmd.rfOpCmd.startTime = MAP_llGetCurrentTime() + RAT_TICKS_IN_1MS;
#endif
  // save the absolute start time of the scanner for post-processing
  // Note: The start time has to be adjusted for AE packets because they cause
  //       the CM0 to end when a secondary channel is followed.
#ifdef USE_RCL
  extInitInfo->initStartTime = extInitCmd.common.timing.absStartTime;
  // set start trigger
  extInitCmd.common.scheduling = RCL_Schedule_AbsTime;
  extInitCmd.common.allowDelay = TRUE;
  // set the channel number
  extInitCmd.channel = LL_SCAN_ADV_CHAN_37;
#else
  extInitInfo->initStartTime = extInitCmd.rfOpCmd.startTime;

  // set start trigger
  SET_RFOP_TRIG_TYPE( extInitCmd.rfOpCmd.startTrig, TRIGTYPE_AT_ABS_TIME );
  SET_RFOP_PAST_TRIG( extInitCmd.rfOpCmd.startTrig );

  // set the command condition
  SET_RFOP_COND_RULE( extInitCmd.rfOpCmd.condition, CONDTYPE_NEVER_RUN_NEXT_CMD );

  // set the channel number and enable BLE whitening
  extInitCmd.chan = LL_SCAN_ADV_CHAN_37;
  SET_WHITENING_BLE( extInitCmd.whitening );
#endif
  uint8 phyMode;
  // set the primary PHY; check if there's more than one Scan primary PHY
  switch( extInitInfo->pCreateConn->initPhys )
  {
    // 2M disallowed, not possible from parameter checks; here for completeness
    //case LL_PHY_2_MBPS:
    //  break;

    case LL_PHY_1_MBPS:
    case LL_PHY_1_MBPS+LL_PHY_2_MBPS:
      phyMode = BLE5_1M_PHY;
      break;

    case LL_PHY_CODED:
    case LL_PHY_CODED+LL_PHY_2_MBPS:
      // OPT: ALLOW USER TO SPECIFY THE CODED SCHEME FOR AUX_SCAN_REQ?
      phyMode = BLE5_CODED_PHY;
      break;

    default:
      // both primary PHYs included
      // Note: The index is 0 for 1M and 2 for Coded, so extScanIndex directly
      //       provides a value of 0 for PHY 1M, and 2 for PHY Coded.
      // OPT: ALLOW USER TO SPECIFY THE CODED SCHEME FOR AUX_SCAN_REQ?
      phyMode = extInitIndex << 1;
      break;
  }

  // use common parameters and output
#ifdef USE_RCL
  extInitCmd.ctx = &extInitParam;
  extInitCmd.stats = &extInitOutput;
  extInitParam = RCL_CtxScanInit_DefaultRuntime();
  extInitOutput = RCL_StatsAdvScanInit_DefaultRuntime();
  // Provide buffer for storing received packets
  // Note: Only need one entry to hold the Adv packet that causes the
  //       AUX_CONNECT_REQ packet to be sent.
  MAP_llSetupInitDataEntryQueue();
  // set filter policy
  extInitParam.filterPolicy = extInitInfo->pCreateConn->initFilterPolicy;
  // set address type

  if ( connPtr->peerInfo.peerAddrType == LL_DEV_ADDR_TYPE_PUBLIC )
  {
    extInitParam.addrType.peer = LL_DEV_ADDR_TYPE_PUBLIC;
  }
  else
  {
    extInitParam.addrType.peer = LL_DEV_ADDR_TYPE_RANDOM;
  }

  if ( extInitInfo->ownAddrType == LL_DEV_ADDR_TYPE_PUBLIC )
  {
    extInitParam.addrType.own = LL_DEV_ADDR_TYPE_PUBLIC;
  }
  else
  {
    extInitParam.addrType.own = LL_DEV_ADDR_TYPE_RANDOM;
  }

  /* Set device addresses */
  MAP_osal_memcpy(extInitParam.ownA, extInitInfo->ownAddr, B_ADDR_LEN);
  MAP_osal_memcpy(extInitParam.peerA, connPtr->peerInfo.peerAddr, B_ADDR_LEN);

  if ( privInfo.addrResolution )
  {
    if ( !MAP_LL_PRIV_IsZeroIRK( resolvingList[LOCAL_RL_INDEX].IRK ) )
    {
      MAP_osal_memcpy(extInitParam.ownA, resolvingList[LOCAL_RL_INDEX].RPA, B_ADDR_LEN);
      extInitParam.rpaModeOwn = TRUE;
    }
    extInitParam.rpaModePeer = TRUE;

    extInitCmd.common.runtime.lrfCallbackMask.value = LRF_EventRxOk.value;
    extInitCmd.common.runtime.rclCallbackMask.value = RCL_EventLastCmdDone.value |
                                                      RCL_EventRxEntryAvail.value;
  }
  // update timeout when command is actually about to start
  extInitCmd.common.timing.relGracefulStopTime = extInitInfo->pCreateConn->extInitParam[extInitIndex].scanWindow * RAT_TICKS_IN_625US;
  extInitCmd.common.phyFeatures = phyMode;
#else
  extInitCmd.pParams = (uint8 *)&extInitParam;
  extInitCmd.pOutput = (uint8 *)&extInitOutput;

  extInitCmd.phyMode = phyMode;
  extInitCmd.rangeDelay =  ( extInitCmd.phyMode == BLE5_1M_PHY ) ? LL_UNCODED_RANGE_DELAY_RAT_TICKS : LL_CODED_RANGE_DELAY_RAT_TICKS;

  // set the Init receive queue
  // Note: Only need one entry to hold the Adv packet that causes the
  //       AUX_CONNECT_REQ packet to be sent.
  extInitParam.pRXQ = MAP_llSetupInitDataEntryQueue();

  // set Init Rx queue configuration
  extInitParam.rxCfg =
    ( RXQ_CFG_AUTOFLUSH_IGNORED_PKT |
      RXQ_CFG_AUTOFLUSH_CRC_ERR_PKT |
      RXQ_CFG_AUTOFLUSH_EMPTY_PKT   |
      RXQ_CFG_INCLUDE_PKT_LEN_BYTE  |
      ((llConfigTable.initPktSuffixPtr->suffixSel & SUFFIX_CRC_FLAG)       ? RXQ_CFG_INCLUDE_CRC      : 0) |
      ((llConfigTable.initPktSuffixPtr->suffixSel & SUFFIX_RSSI_FLAG)      ? RXQ_CFG_APPEND_RSSI      : 0) |
      ((llConfigTable.initPktSuffixPtr->suffixSel & SUFFIX_STATUS_FLAG)    ? RXQ_CFG_APPEND_STATUS    : 0) |
      ((llConfigTable.initPktSuffixPtr->suffixSel & SUFFIX_TIMESTAMP_FLAG) ? RXQ_CFG_APPEND_TIMESTAMP : 0) );

  // start by assuming nothing
  CLR_INIT_CFG( extInitParam.initCfg );

  // set accept list policy
  SETVAR_INIT_CFG_FILTER_POLICY( extInitParam.initCfg,
                                 extInitInfo->pCreateConn->initFilterPolicy );

#ifndef LL_MANUAL_WINOFFSET
  // set use of dynamic window offset
  // Note: Normally, the window offset is managed dynamically so that precise
  //       connection start times can be achieved (necessary for multiple
  //       connnections). However, sometimes it is useful to force the window
  //       offset to something specific for testing. This can be done by here
  //       when the project is built with the above define.
  // Note: This define should only be used for testing one connection and will
  //       NOT work when multiple connections are attempted!
  // Note: This define should NEVER be used for a ROM build!
  // ROM WORKAROUND - FLASH ONLY BUILD
  SETVAR_INIT_CFG_DYN_WINOFFSET( extInitParam.initCfg,
                                 INIT_CFG_USE_DYN_WINOFFSET );
#endif // !LL_MANUAL_WINOFFSET

  // set own address type
  // Note: May be changed based on Privacy.
  // Note that the macro forces type to one bit, so ID address types okay.
  SETVAR_INIT_CFG_DEV_ADDR_TYPE( extInitParam.initCfg,
                                 extInitInfo->ownAddrType );

  // set peer address type
  // Note: May be changed based on Privacy.
  // Note that the macro forces type to one bit, so ID address types okay.
  if ( LL_IS_ADDR_TYPE_RPA(connPtr->peerInfo.peerAddrType) )
  {
    // set address type as Random.
    // RPA address is sent over the air with type LL_DEV_ADDR_TYPE_RANDOM
    SETVAR_INIT_CFG_PEER_ADDR_TYPE(extInitParam.initCfg, LL_DEV_ADDR_TYPE_RANDOM);
  }
  else
  {
    // set peer address type as provided
    SETVAR_INIT_CFG_PEER_ADDR_TYPE( extInitParam.initCfg,
                                    connPtr->peerInfo.peerAddrType );
  }

  // enable the chSel bit in the connection ind if we support Algo #2
  // Note: There's a Vendor Specific command to change the Local Feature
  //       Set, so just because the build includes this feature, this
  //       does not mean our device supports it for this connection.
  if ( deviceFeatureSet.featureSet[1] & (uint8)LL_FEATURE_CHAN_ALGO_2 )
  {
    SET_ADV_CFG_CHAN_SELECT( extInitParam.initCfg );
  }
  else // channel selection algorithm 1
  {
    CLR_ADV_CFG_CHAN_SELECT( extInitParam.initCfg );
  }

  // OPT: Optionally adhere to strict packet lengths per the specification.
  //SETVAR_INIT_CFG_STRICT_LEN_FILTER( extInitParam.initCfg,
  //                                   INIT_CFG_DISCARD_ILLEGAL_LEN);

  // set backoff parameters
  // Note: The Upper Limit is kept as a binary log.
  extInitParam.backoffCount = 1;
  SETVAR_SCAN_BACKOFF_CFG_UL( extInitParam.backoffParam, 0 );
  SETVAR_SCAN_BACKOFF_CFG_LAST_SUCCCEEDED( extInitParam.backoffParam, 0 );
  SETVAR_SCAN_BACKOFF_CFG_LAST_FAILED( extInitParam.backoffParam, 0 );

  // set random state value
  MAP_LL_ENC_GenerateTrueRandNum( (uint8 *)&extInitParam.randState, sizeof(uint16) );

  // set the size of the CONNECT_IND LL_Data
  extInitParam.connReqLen = LL_CONNECT_IND_LL_DATA_LEN;
#endif

  // use default Tx Power
  extInitCmd.txPower = RfBleDpl_getTxPower(curTxPowerVal);

  MAP_osal_memset(connReqData, 0, sizeof(connReqData_t)*LL_PHY_NUMBER_OF_PHYS);

  // setup the AUX_CONNECT_REQ/CONNECT_IND data for all possible PHYs
  for ( uint8_t i=0,j=0; i<LL_PHY_NUMBER_OF_PHYS; i++ )
  {
#ifdef USE_RCL
    connReqData[i].state = RCL_BufferStatePending;
    connReqData[i].length = LL_CONNECT_IND_PKT_LEN + 6;
    connReqData[i].numPad = 3;
    connReqData[i].pad0   = 2; /* Same padding as in PBE, but this is don't-care */
    connReqData[i].pad1   = 1; /* Same padding as in PBE, but this is don't-care */
    connReqData[i].pad2   = 0; /* Same padding as in PBE, but this is don't-care */
    connReqData[i].header = LL_PKT_TYPE_CONNECT_IND;
    // enable the chSel bit in the connection ind if we support Algo #2
    if ( deviceFeatureSet.featureSet[1] & (uint8)LL_FEATURE_CHAN_ALGO_2 )
    {
      connReqData[i].header |= LL_ADV_HDR_SET_CHSEL(connReqData[i].header, LL_CHANNEL_SELECT_ALGO_2);
    }
    connReqData[i].payloadLen = LL_CONNECT_IND_PKT_LEN;

    if ( extInitInfo->ownAddrType != LL_DEV_ADDR_TYPE_PUBLIC )
    {
      // When working with Random ID address or RPA the address type sent over the air will be RANDOM
      ownAddrType = LL_DEV_ADDR_TYPE_RANDOM;
    }

    if ( connPtr->peerInfo.peerAddrType != LL_DEV_ADDR_TYPE_PUBLIC )
    {
      // When working with Random ID address or RPA the address type sent over the air will be RANDOM
      peerAddrType = LL_DEV_ADDR_TYPE_RANDOM;
    }


    // set address type
    connReqData[i].header |= LL_ADV_HDR_SET_TX_ADD(connReqData[i].header, ownAddrType);
    connReqData[i].header |= LL_ADV_HDR_SET_RX_ADD(connReqData[i].header, peerAddrType);
    // set our address
    MAP_osal_memcpy( connReqData[i].ownAddr, extInitInfo->ownAddr, B_ADDR_LEN );
    // set peer address
    MAP_osal_memcpy( connReqData[i].peerAddr, extInitInfo->pCreateConn->peerAddr, B_ADDR_LEN );
#endif
    // set the common fields
    connReqData[i].accessAddress = connPtr->accessAddr;
    connReqData[i].crcInit[0]    = connPtr->crcInit & 0xFF;
    connReqData[i].crcInit[1]    = (connPtr->crcInit >> 8) & 0xFF;
    connReqData[i].crcInit[2]    = (connPtr->crcInit >> 16) & 0xFF;
    connReqData[i].winSize       = LL_WINDOW_SIZE;
#ifdef USE_RCL
    connReqData[i].winOffset     = connPtr->curParam.winOffset;
#else
    connReqData[i].winOffset     = LL_WINDOW_OFFSET;
#endif
    connReqData[i].chanMap[0]    = connPtr->curChanMap.chanMap[0];
    connReqData[i].chanMap[1]    = connPtr->curChanMap.chanMap[1];
    connReqData[i].chanMap[2]    = connPtr->curChanMap.chanMap[2];
    connReqData[i].chanMap[3]    = connPtr->curChanMap.chanMap[3];
    connReqData[i].chanMap[4]    = connPtr->curChanMap.chanMap[4];
    connReqData[i].hopSca        = ((connPtr->sleepClkAccuracy & 0x07) << 5) |
                                   (connPtr->hopLength & 0x1F);

    // check if PHY parameters were provided by the API
    if ( extInitInfo->pCreateConn->initPhys & BV(i) )
    {
      // set the PHY specific fields
      connReqData[i].connInterval  = extInitInfo->pCreateConn->extInitParam[j].connIntMax;
      connReqData[i].latency       = extInitInfo->pCreateConn->extInitParam[j].connLatency;
      connReqData[i].timeout       = extInitInfo->pCreateConn->extInitParam[j].connTimeout;

      // increment array index
      j++;
    }
    else // parameter was not specified for this PHY
    {
      // set the PHY based on index 0, which has to be valid
      connReqData[i].connInterval  = extInitInfo->pCreateConn->extInitParam[0].connIntMax;
      connReqData[i].latency       = extInitInfo->pCreateConn->extInitParam[0].connLatency;
      connReqData[i].timeout       = extInitInfo->pCreateConn->extInitParam[0].connTimeout;
    }
  }

#ifdef USE_RCL
  /* Provide buffer holding CONNECT_IND */
  RCL_TxBuffer_put(&extInitParam.txBuffers, (RCL_Buffer_TxBuffer *)&connReqData[extInitIndex]);
  // set accept list or peer address, depending on accept list policy
  if ( extInitInfo->pCreateConn->initFilterPolicy == LL_INIT_AL_POLICY_USE_ACCEPT_LIST )
  {
    // use accept list
    extInitParam.filterList = (RCL_FilterList *)((uint32)(&alTable->pAlEntries[0]) - sizeof(uint32));//&alTable->numEntries;
  }
#ifdef RCL_329
  else // LL_INIT_AL_POLICY_USE_PEER_ADDR
  {
    if ( LL_IS_ADDR_TYPE_RPA(connPtr->peerInfo.peerAddrType) )
    {
      /////////////////////////////////////////////////////////////////////////////
      // Temporary - Until RCL will provide a fix (an API) which allows to update
      // peer device RPA address while the command is running,
      // change the filter policy to use Accept list and send the RCL AL with a
      // single record
      ////////////////////////////////////////////////////////////////////////////
      extInitParam.filterPolicy = LL_INIT_AL_POLICY_USE_ACCEPT_LIST;

      MAP_osal_memset(&peerAddrInitCmd, 0x00, sizeof(RCL_FilterList));

      // There is only one entry in this AL
      peerAddrInitCmd.numEntries = 1;

      // Set the ignore random, and busy flag on the new
      peerAddrInitCmd.entries[0].ctl.enabled = TRUE;
      peerAddrInitCmd.entries[0].ctl.addType = LL_DEV_ADDR_TYPE_RANDOM;

      // Copy peer device address
      MAP_osal_memcpy(peerAddrInitCmd.entries[0].address, connPtr->peerInfo.peerAddr, B_ADDR_LEN);

      // Update the initiator command with the new AL
      extInitParam.filterList = &peerAddrInitCmd;
    }
  }
#endif // RCL_329
  // initialize Timeout Trigger and Time based on Scan window
  extInitCmd.common.timing.relGracefulStopTime = extInitInfo->pCreateConn->extInitParam[extInitIndex].scanWindow * RAT_TICKS_IN_625US;
  // Set callback function and events
  extInitCmd.common.runtime.callback = LL_rclInitCallback;
  extInitCmd.common.runtime.lrfCallbackMask.value = 0;
  extInitCmd.common.runtime.rclCallbackMask.value =  RCL_EventLastCmdDone.value;

  // only if address resolution is enabled
  if ( privInfo.addrResolution )
  {
    if ( !MAP_LL_PRIV_IsZeroIRK( resolvingList[LOCAL_RL_INDEX].IRK ) )
    {
      extInitParam.rpaModeOwn = TRUE;
    }
    extInitParam.rpaModePeer = TRUE;

    extInitCmd.common.runtime.lrfCallbackMask.value = LRF_EventRxOk.value;
    extInitCmd.common.runtime.rclCallbackMask.value = RCL_EventLastCmdDone.value |
                                                      RCL_EventRxEntryAvail.value;
  }
#else
  // set the pointer to the AUX_CONNECT_REQ/CONNECT_IND data
  extInitParam.pConnReqData = (uint8 *)&connReqData[extInitIndex];

  // set a pointer to our device address
  extInitParam.pDeviceAddr = extInitInfo->ownAddr;

  // set accept list or peer address, depending on accept list policy
  if ( extInitInfo->pCreateConn->initFilterPolicy == LL_INIT_AL_POLICY_USE_PEER_ADDR )
  {
    // use peer address
    // Note: The type conversion is obviously only to resolve the pointer type.
    // ALT: Make pAcceptList a pointer to uint8.
    extInitParam.pAcceptList = (alEntry_t *)connPtr->peerInfo.peerAddr;
  }
  else // LL_INIT_AL_POLICY_USE_ACCEPT_LIST
  {
    // use accept list
    extInitParam.pAcceptList = &alTable->pAlEntries[0];
  }

  // Note: extInitParam.connectTime setup in call to llSetupConn.

  // set max wait time for aux channel
  extInitParam.maxWaitForAux = 0xFFFF;

  // initialize Timeout Trigger and Time based on Scan window
  CLR_RFOP_ALT_TRIG_CMD( extInitParam.timeoutTrig );
  SET_RFOP_PAST_TRIG( extInitParam.timeoutTrig );
  SET_RFOP_TRIG_TYPE( extInitParam.timeoutTrig, TRIGTYPE_AT_ABS_TIME );
  // update timeout when command is actually about to start
  extInitParam.timeoutTime = extInitCmd.rfOpCmd.startTime +
                             (extInitInfo->pCreateConn->extInitParam[extInitIndex].scanWindow * RAT_TICKS_IN_625US);

  // initialize the End Trigger and Time based on Scan Interval
  CLR_RFOP_ALT_TRIG_CMD( extInitParam.endTrig );
  SET_RFOP_PAST_TRIG( extInitParam.endTrig );
  SET_RFOP_TRIG_TYPE( extInitParam.endTrig, TRIGTYPE_NEVER );
  extInitParam.endTime = 0;

  // clear unused fields
  // Note: These are read-only once the command starts.
  extInitParam.rxStartTime  = 0;
  extInitParam.rxListenTime = 0;
  extInitParam.chan         = 0;

  // only if address resolution is enabled
  if ( privInfo.addrResolution )
  {
    // check if own address is a public identity address and IRK is valid
    if ( (extInitInfo->ownAddrType == LL_DEV_ADDR_TYPE_PUBLIC_ID) &&
         !MAP_LL_PRIV_IsZeroIRK( resolvingList[LOCAL_RL_INDEX].IRK ) )
    {
      // make sure own address type sent OTA is random type
      // Note: Value of LL_DEV_ADDR_TYPE_RANDOM_ID ends up just being RANDOM.
      SETVAR_INIT_CFG_DEV_ADDR_TYPE( extInitParam.initCfg, LL_DEV_ADDR_TYPE_RANDOM_ID );
    }
  }
#endif
  return;
}
#endif // INIT_CFG

/*******************************************************************************
 * @fn          llExtAdvCBack
 *
 * @brief       This routine is used to invoke a callback for extended
 *              advertising, extended scanning, and extended initiating.
 *
 * input parameters
 *
 * @param       cBackId - Callback Identity.
 * @param       pParams - Pointer (generic) to parameters to return.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llExtAdvCBack( uint8  cBackId,
                    void  *pParams )
{
  uint32 *pCBackEntry;

  // check input parameter
  if ( cBackId == LL_CBACK_CLEAR ) return;

  // point to callback function table
  pCBackEntry = (uint32 *)&aeCBackTbl.reserved + 1;

  // check if there's a callback pointer to call
  // Note: The value of cbackId is not verified as only called internally.
  if ( pCBackEntry[cBackId] == 0 )
  {
    return;
  }

  // invoke callback based on ID
  ((pCBack_t)pCBackEntry[cBackId])(cBackId, pParams);

  return;
}

/*******************************************************************************
 * @fn          llCheckCBack
 *
 * @brief       This routine is used to check whether a callback has been
 *              registered.
 *
 * input parameters
 *
 * @param       cBackId - Callback Identity.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE:  There's a callback for this callback ID.
 *              FALSE: There is no callback for this callback ID.
 */
uint8 llCheckCBack( uint8 cBackId )
{
  uint32 *pCBackEntry;

  // check input parameter
  if ( cBackId == LL_CBACK_CLEAR ) return( FALSE );

  // point to callback function table
  pCBackEntry = (uint32 *)&aeCBackTbl.reserved + 1;

  // check if there's a callback pointer
  // Note: The value of cbackId is not verified as only called internally.
  return ( pCBackEntry[cBackId] != 0 );
}

/*******************************************************************************
 * @fn          llEndExtAdvTask
 *
 * @brief       This routine is used to end the Extended Advertiser task by
 *              sending any Host callbacks, freeing the task, and tearing down
 *              privacy if need be.
 *
 * input parameters
 *
 * @param       pAdvSet - Pointer to advertisement set.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llEndExtAdvTask( advSet_t *pAdvSet )
{
  // Note: Null check performed by calling function.

  // free the associated task block
  // Note: If the last task, llState will be set to Idle.
  MAP_llFreeTask( &pAdvSet->llTask );

  // check if we're the last BLE task
  if ( llState == LL_STATE_IDLE )
  {
#ifndef CC23X0
    // teardown privacy if address resolution is enabled
    if ( privInfo.addrResolution )
    {
      // restore standard accept list and disable Rx Ignore interrupt
      MAP_LL_PRIV_TeardownPrivacy( alTable );
    }
#endif
  }

  return;
}

#ifdef USE_PERIODIC_ADV
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llEndPeriodicAdvTask
 *
 * @brief       This routine is used to end the Periodic Advertiser task
 *
 * @design  /ref did_286039104
 *
 * input parameters
 *
 * @param       pPeriodicAdv - Pointer to periodic advertising set.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llEndPeriodicAdvTask( llPeriodicAdvSet_t *pPeriodicAdv )
{
  pPeriodicAdv->state = PERIODIC_ADV_STATE_DISABLE;
  pPeriodicAdv->pendingDisable = FALSE;
  // update num active
  llPeriodicAdv.advNumActive--;

  if (llPeriodicAdv.advNumActive == 0)
  {
    MAP_llFreeTask( &llPeriodicAdv.llTask );
  }
}
#endif
#endif //USE_PERIODIC_ADV

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
#ifdef USE_PERIODIC_SCAN
/*******************************************************************************
 * @fn          llEndPeriodicScanTask
 *
 * @brief       This routine is used to end the Periodic Scanner
 *
 * @design      /ref did_286039104
 *
 * input parameters
 *
 * @param       pPeriodicScan - Pointer to periodic scanner.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llEndPeriodicScanTask( llPeriodicScanSet_t *pPeriodicScan )
{
  halIntState_t cs;

  // protect from host that could send create sync cancel
  // check that we are in syncing process
  if ((pPeriodicScan->state == PERIODIC_SCAN_STATE_SYNCING_PENDING) ||
      (pPeriodicScan->state == PERIODIC_SCAN_STATE_SYNCING_ACTIVE))
  {
    if (pPeriodicScan->terminate == LL_STATUS_ERROR_UNACCEPTABLE_CONN_PARAMETERS)
    {
      llResetPeriodicScan();
    }
    else
    {
      uint8 phy = llPeriodicScan.createSync->phy + ((llPeriodicScan.createSync->phy == BLE5_S2_PHY) ? 0 : 1);
      // send event to Host
      HCI_PeriodicAdvSyncEstablishedEvent(pPeriodicScan->terminate,
                                        llPeriodicScan.createSync->handle,
                                        llPeriodicScan.createSync->syncCmd.sid,
                                        llPeriodicScan.createSync->syncCmd.addrType,
                                        llPeriodicScan.createSync->syncCmd.addr,
                                        phy,
                                        llPeriodicScan.createSync->interval,
                                        llPeriodicScan.createSync->syncInfo.sca);
      HAL_ENTER_CRITICAL_SECTION(cs);
      if (pPeriodicScan->state == PERIODIC_SCAN_STATE_SYNCING_ACTIVE)
      {
        if ((llPeriodicScan.scanNumActive == 0) && (llPeriodicScan.llTask != NULL))
        {
          // free the associated task block
          MAP_llFreeTask( &llPeriodicScan.llTask );
        }
      }
      // clear the set
      MAP_osal_mem_free(llPeriodicScan.createSync);
      llPeriodicScan.createSync = NULL;
      HAL_EXIT_CRITICAL_SECTION(cs);
    }
  }
  else if (pPeriodicScan->state == PERIODIC_SCAN_STATE_SYNCED)
  {
    if (pPeriodicScan->terminate == LL_STATUS_ERROR_CONNECTION_TIMEOUT)
    {
      // send event to Host - LE Periodic Advertising Sync Lost
      HCI_PeriodicAdvSyncLostEvent(pPeriodicScan->handle);
    }
    HAL_ENTER_CRITICAL_SECTION(cs);
    // update num active
    llPeriodicScan.scanNumActive--;

    if (llPeriodicScan.scanNumActive == 0)
    {
      // free the associated task block
      MAP_llFreeTask( &llPeriodicScan.llTask );
    }
    // update the next set pointer
    if (llPeriodicScan.scanList == pPeriodicScan)
    {
      llPeriodicScan.scanList = pPeriodicScan->next;
    }
    else
    {
      llPeriodicScanSet_t *pTmpPeriodicScan = llPeriodicScan.scanList;

      // find the previous set
      while (pTmpPeriodicScan->next != pPeriodicScan) pTmpPeriodicScan = pTmpPeriodicScan->next;
      // set the previous set next pointer
      pTmpPeriodicScan->next = pPeriodicScan->next;
    }
#ifdef RTLS_CTE
    // delete the CTE antenna array
    if ( pPeriodicScan->cteInfo.pAntenna != NULL )
    {
      MAP_osal_mem_free( pPeriodicScan->cteInfo.pAntenna );
    }
    // check the auto copy buffers
    if ((pPeriodicScan->cteInfo.enable != LL_CTE_SAMPLING_NOT_INIT) &&
        (llCteSamples.pAutoCopyBuffers != NULL))
    {
      // reset the enable parameter before release the auto copy buffer
      pPeriodicScan->cteInfo.enable = LL_CTE_SAMPLING_NOT_INIT;
      // check for release the CTE auto copy buffer
      MAP_llFreeCteSamplesEntryQueue();
    }
#endif // RTLS_CTE
    // clear the set
    MAP_osal_mem_free(pPeriodicScan);
    HAL_EXIT_CRITICAL_SECTION(cs);
  }
}
#endif //USE_PERIODIC_SCAN

/*******************************************************************************
 * @fn          llEndExtScanTask
 *
 * @brief       This routine is used to end the Extended Scanner task by
 *              sending any Host callbacks, freeing the task, and tearing down
 *              privacy if need be.
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
void llEndExtScanTask( void )
{
  // free the associated task block
  // Note: If the last task, llState will be set to Idle.
  MAP_llFreeTask( &extScanInfo->llTask );

  // check if we're the last BLE task
  if ( llState == LL_STATE_IDLE )
  {
    // teardown privacy if address resolution is enabled
#ifndef CC23X0
    if ( privInfo.addrResolution )
    {
      // restore standard accept list and disable Rx Ignore interrupt
      MAP_LL_PRIV_TeardownPrivacy( GET_AL_TABLE_POINTER(extScanParam.pAcceptList) );
    }
#endif
  }

  return;
}
#endif // SCAN_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
/*******************************************************************************
 * @fn          llEndExtInitTask
 *
 * @brief       This routine is used to end the Extended Initiator task by
 *              sending any Host callbacks, freeing the task, and tearing down
 *              privacy if need be.
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
void llEndExtInitTask( void )
{
  // free the associated task block
  // Note: If the last task, llState will be set to Idle.
  MAP_llFreeTask( &extInitInfo->llTask );

  // check if we're the last BLE task
  if ( llState == LL_STATE_IDLE )
  {
    // teardown privacy if address resolution is enabled
#ifndef CC23X0
    if ( privInfo.addrResolution )
    {
      // restore standard accept list and disable Rx Ignore interrupt
      MAP_LL_PRIV_TeardownPrivacy( alTable );
    }
#endif
  }

  return;
}
#endif // INIT_CFG

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llTermExtAdv
 *
 * @brief       This routine is used to end the Extended Advertiser by
 *              sending any Host callbacks and freeing the task.
 *
 * input parameters
 *
 * @param       pAdvSet - Pointer to advertisement set.
 * @param       reason - termination reason.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llTermExtAdv( advSet_t *pAdvSet, uint8 reason )
{
  // indicate we are no longer actively advertising
  pAdvSet->advMode = LL_ADV_MODE_OFF;

  if (numActiveAdvSets > 0)
  {
    // free task and teardown privacy if need be
    MAP_llEndExtAdvTask( pAdvSet );
  }
  // send the Adv Set End callback, if enabled
  MAP_llSendAdvSetEndEvent( pAdvSet );

  if ((reason == LL_STATUS_ERROR_DIRECTED_ADV_TIMEOUT) ||
      (reason == LL_STATUS_ERROR_LIMIT_REACHED))
  {
    // send the LE Advertisement Set Terminated Event
    MAP_llSendAdvSetTermEvent( pAdvSet, reason, LL_INVALID_CONNECTION_ID );
  }
}
#endif

/*******************************************************************************
 * @fn          llSendAdvSetTermEvent
 *
 * @brief       This routine is used to setup and send the LE Advertising Set
 *              Terminated Event.
 *
 * input parameters
 *
 * @param       pAdvSet - Pointer to advertisement set.
 * @param       status  - Status of Set Terminate Event.
 * @param       connId  - Connection ID. May not be valid.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSendAdvSetTermEvent( advSet_t *pAdvSet,
                            uint8     status,
                            uint8     connId )
{
  // check if there's a registered callback and if not masked by the Host
  if (( MAP_llCheckCBack(LL_CBACK_ADV_SET_TERMINATED) ) &&
     ( pBleEvtMask[LE_EVT_INDEX_EXTENDED_ADV_SET_TERIMINATED] &
       LE_EVT_MASK_EXTENDED_ADV_SET_TERIMINATED ))
  {
    // malloc adv set terminated event
    aeAdvSetTerm_t *pAdvSetTerm = MAP_osal_mem_allocLimited( sizeof(aeAdvSetTerm_t) );

    // check if we got the memory
    if ( pAdvSetTerm )
    {
      // set the subcode
      pAdvSetTerm->subCode = AE_ADV_HCI_BLE_ADV_SET_TERMINATED_EVENT;

      // set the status
      pAdvSetTerm->status = status;

      // set the advertising handle
      pAdvSetTerm->handle = pAdvSet->pAdvParam->handle;

      // set the connection handle
      pAdvSetTerm->connHandle = connId;

      // set the number of completed extended advertising events
      if ( pAdvSet->pEnable->maxEvents )
      {
        pAdvSetTerm->numCompAdvEvts = pAdvSet->maxAdvEvts;
      }
      else // max events not used
      {
        pAdvSetTerm->numCompAdvEvts = 0;
      }

      // invoke the callback
      MAP_llExtAdvCBack( LL_CBACK_ADV_SET_TERMINATED, (void *)pAdvSetTerm );
    }
    else // out of memory
    {
      MAP_llExtAdvCBack( LL_CBACK_OUT_OF_MEMORY, NULL );
    }
  }

  return;
}

/*******************************************************************************
 * @fn          llFindNextSecCmd
 *
 * @brief       This function is used to find the next secondary command based
 *              on link layer state.
 *
 * input parameters
 *
 * @param       llTask - Pointer to task information structure.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      Pointer to the BLE5 command.
 */
void *llFindNextSecCmd( taskInfo_t *llTask )
{
  void *nextSecCmd;

  // based on BLE task Id
  switch( llTask->taskID )
  {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
    case LL_TASK_ID_ADVERTISER:
      // find the next Adv Set, and return command to it
      nextSecCmd = MAP_llFindNextAdvSet();
      break;
#ifdef USE_PERIODIC_ADV
    case LL_TASK_ID_PERIODIC_ADVERTISER:
      // find the next Adv Set, and return command to it
      nextSecCmd = MAP_llFindNextPeriodicAdv();
      break;
#endif // USE_PERIODIC_ADV
#endif // ADV_NCONN_CFG | ADV_CONN_CFG
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
    case LL_TASK_ID_SCANNER:
      nextSecCmd = (void *)&extScanCmd;
      break;
#ifdef USE_PERIODIC_SCAN
    case LL_TASK_ID_PERIODIC_SCANNER:
      // find the next Adv Set, and return command to it
      nextSecCmd = MAP_llFindNextPeriodicScan();
      break;
#endif // USE_PERIODIC_SCAN
#endif // SCAN_CFG
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
    case LL_TASK_ID_INITIATOR:
      nextSecCmd = (void *)&extInitCmd;
      break;
#endif // INIT_CFG
    default:
      nextSecCmd  = ((void *)llTask->command);
      break;
  }

  return( nextSecCmd );
}

/*******************************************************************************
 * @fn          llSendAdvSetEndEvent
 *
 * @brief       This routine is used to check if the Advertisement Set End
 *              flag is set, and if so, make the callback for this event.
 *
 * NOTE: This function was hijacked for the cause of not being able to add code
 *       to llAdv_TaskConnect because it's in the ROM.
 *       in this function i'm also checking the value of advHalt to know if
 *       disconnection should happen due to unresolved RPA and updating other
 *       structure which is aligned with the future connection which will be formed.
 *       later on this value will be read in the scheduler and disconnection will be
 *       done accordingly.
 *
 * input parameters
 *
 * @param       pAdvSet - Pointer to advertisement set.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSendAdvSetEndEvent( advSet_t *pAdvSet )
{
  // Get the connection ptr.
  llConnState_t    *connPtr = MAP_llDataGetConnPtr( pAdvSet->connId );

  // information in "NOTE" above
  if ((connPtr) && (pAdvSet->advHalted & AE_ENABLE_DISCONNECT_AUTH_FAIL))
  {
    // enable the flag which indicates to disconnect later with reason Auth Fail (0x05)
    connPtr->extFeatureMask |= EXT_FEATURE_DISCONNECT_ENABLE;

    // disable the flag which marked on the Adv set
    pAdvSet->advHalted &= AE_DISABLE_DISCONNECT_AUTH_FAIL;
  }
  // check if there's a request for a callback for the end of Adv
  if ( pAdvSet->pAdvParam->notifyEnableFlags & AE_NOTIFY_ENABLE_ADV_SET_END )
  {
    // check if halt stopped the radio
    // Note: It is possible the radio wasn't halted by llHaltRadio, so call
    //       the Adv End callback here since there there won't be a post
    //       process event to do this.
    if ( !(pAdvSet->advHalted & AE_HALTED_TRUE))
    {
      // halt didn't kill the radio (occured between radio events), so handle
      // the end callback here as there won't be a post processing event

      // invoke callback, if there is one
      MAP_llExtAdvCBack( LL_CBACK_ADV_END_AFTER_DISABLE,
                         &pAdvSet->pAdvParam->handle );
    }
  }

  return;
}

/*******************************************************************************
 * @fn          llStartDurationTimer
 *
 * @brief       This routine is used to start the OSAL timer for Advertiser or
 *              Scanner based on the Duration parameter. Upon expiration, the
 *              Advertiser will generate a Set Termiante Event; the Scanner
 *              will generate a Scan Duration End Event, a Scan Period End
 *              Event, or a Scan Timeout Event.
 *
 * input parameters
 *
 * @param       eventID  - OSAL event ID.
 * @param       duration - OSAL timeeout time, in milliseconds.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS
 *              LL_STATUS_ERROR_DUE_TO_LIMITED_RESOURCES
 */
llStatus_t llStartDurationTimer( uint16 eventID,
                                 uint32 duration )
{
  // setup duration, if applicable
  if ( duration )
  {
    // advertise/scan for duration
    if ( MAP_osal_start_timerEx( LL_TaskID,
                                 eventID,
                                 duration ) != SUCCESS )
    {
      // generate event to the Host
      MAP_HCI_HardwareErrorEvent( HW_FAIL_NO_TIMER_AVAILABLE );

      // Note: This corresponds to the OSAL status NO_TIMER_AVAIL.
      return( LL_STATUS_ERROR_DUE_TO_LIMITED_RESOURCES );
    }
  }
  //else duration is zero: advertise/scan without duration control

  return( LL_STATUS_SUCCESS );
}

#ifdef USE_PERIODIC_ADV
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llUpdatePeriodicAdvChainPacket
 *
 * @brief       This routine is used to update RF command in case chain
 *              packet (AUX_CHAIN_IND) should be send on current periodic adv set
 *
 * @design      /ref did_286039104
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
void llUpdatePeriodicAdvChainPacket(void)
{
  llPeriodicAdvSet_t *pPeriodicAdv = llPeriodicAdv.currentAdv;

  if (pPeriodicAdv == NULL )
  {
    return;
  }
  // update tx counter
  pPeriodicAdv->txCount++;
  // update the chain packet if there is chain packet
  if (pPeriodicAdv->numChains > pPeriodicAdv->txCount)
  {
    // update start time of secondary command in case of AUX_CHAIN_IND
    pPeriodicAdv->rfCmd.rfOpCmd.startTime += US_TO_RAT_TICKS(pPeriodicAdv->otaTime);
    // check for data
    if (pPeriodicAdv->numFrags <= pPeriodicAdv->txCount)
    {
      pPeriodicAdv->rfPkt.pAdvData = NULL;
      pPeriodicAdv->rfPkt.advDataLen = 0;
    }
    else
    {
      // update data pointer
      pPeriodicAdv->rfPkt.pAdvData += pPeriodicAdv->rfPkt.advDataLen;
      // in case of last chunk
      if (pPeriodicAdv->numFrags == (pPeriodicAdv->txCount + 1))
      {
        pPeriodicAdv->rfPkt.advDataLen = pPeriodicAdv->lastFragLen;
      }
    }

    // update chain header
    if (((pPeriodicAdv->cteInfo.enable) && (pPeriodicAdv->cteInfo.count == pPeriodicAdv->txCount))  ||
        (pPeriodicAdv->numFrags == pPeriodicAdv->txCount)       ||
        (pPeriodicAdv->numFrags == (pPeriodicAdv->txCount + 1)) ||
        (pPeriodicAdv->numChains == (pPeriodicAdv->txCount + 1))||
        (TST_EXTHDR_FLAG(pPeriodicAdv->rfPkt.extHdrFlags, EXTHDR_FLAG_TXPWR)) ||
        (pPeriodicAdv->pendingChanUpdate == PERIODIC_ADV_CHANMAP_UPDATE_PENDING))
    {
      uint8 hdrSize;
      // update extended header flags
      hdrSize = llSetPeriodicHdrFlags(pPeriodicAdv);
      if (GET_EXT_HDR_LEN(pPeriodicAdv->rfPkt.extHdrInfo) != hdrSize )
      {
        // update extended header length
        SET_EXTHDR_LEN( pPeriodicAdv->rfPkt.extHdrInfo,hdrSize );
        // update extended header contents
        llSetupPeriodicHdr(pPeriodicAdv);
      }
      // update OTA time
      pPeriodicAdv->otaTime =  MAP_llOctets2Time( pPeriodicAdv->rfCmd.phyMode & 0x03,      // first two bits only
                                       (pPeriodicAdv->rfCmd.phyMode>>2) & 0x01, // scheme
                                       (hdrSize + EXTHDR_INFO_SIZE + pPeriodicAdv->rfPkt.advDataLen),
                                        MIC_NOT_ENABLED );

      pPeriodicAdv->otaTime += ((pPeriodicAdv->cteInfo.enable) &&
                                (pPeriodicAdv->cteInfo.count > pPeriodicAdv->txCount))?
                                (pPeriodicAdv->cteInfo.len * 8):0;
      pPeriodicAdv->otaTime += AE_MIN_T_MAFS_IN_US;
    }
    // update aux ptr
    if ((pPeriodicAdv->txCount + 1) == pPeriodicAdv->numChains)
    {
      pPeriodicAdv->rfParam.auxPtrTgtTime = 0;
      pPeriodicAdv->rfParam.auxPtrTgtType = TRIGTYPE_NOW;
      // remove the T_MAFS from the last chain
      // this OTA will be used to calculate the total OTA time
      pPeriodicAdv->otaTime -= AE_MIN_T_MAFS_IN_US;
    }
    else
    {
      // set the auxPtr time and type
      // Note: CM0 expects time in RAT ticks.
      pPeriodicAdv->rfParam.auxPtrTgtTime = pPeriodicAdv->rfCmd.rfOpCmd.startTime +
                                        US_TO_RAT_TICKS( pPeriodicAdv->otaTime + START_SYNTH_TO_RAT_OFFSET );

      pPeriodicAdv->rfParam.auxPtrTgtType = TRIGTYPE_AT_ABS_TIME;
    }
  }
}
#endif
#endif // USE_PERIODIC_ADV

#ifdef USE_PERIODIC_SCAN
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 * @fn          llSetupPeriodicScanDataEntryQueue
 *
 * @brief       This routine is used to setup a static ring RX buffer for the periodic scan.
 *
 * @design      /ref did_286039104
 *
 * input parameters
 *
 * @param
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Pointer to the Rx Data Entry Queue.
 */
dataEntryQ_t *llSetupPeriodicScanDataEntryQueue( void )
{
  if (llPeriodicScan.queue.dataEntryQ.pCurEntry != NULL)
  {
    return( &llPeriodicScan.queue.dataEntryQ );
  }
  // init data entries
  // ALT: Make ring buffer size configurable from ll_config.
  for (uint8 i=0; i<NUM_RX_SCAN_ENTRIES; i++)
  {
    // initialize common data entry members
    llPeriodicScan.rxBuf[i].entry.status = DATASTAT_PENDING;
    llPeriodicScan.rxBuf[i].entry.config = DATA_ENTRY_TYPE_GENERAL | DATA_ENTRY_LEN_SIZE_0;
    llPeriodicScan.rxBuf[i].entry.length = LL_PKT_HDR_LEN + MAX_BLE_ADV_PKT_SIZE + SUFFIX_MAX_SIZE;

    // point to next entry
    // ALT: Make ring buffer size configurable from ll_config.
    llPeriodicScan.rxBuf[i].entry.pNextEntry =
      (dataEntry_t *)&llPeriodicScan.rxBuf[(i+1)%NUM_RX_SCAN_ENTRIES];
  }

  // init data queue
  llPeriodicScan.queue.dataEntryQ.pCurEntry  = (dataEntry_t *)&llPeriodicScan.rxBuf[0];
  llPeriodicScan.queue.dataEntryQ.pLastEntry = NULL;
  llPeriodicScan.queue.pNextDataEntry        = (dataEntry_t *)&llPeriodicScan.rxBuf[0];
  llPeriodicScan.queue.pTempDataEntry        = NULL;

  return( &llPeriodicScan.queue.dataEntryQ );
}

#endif
#endif // USE_PERIODIC_SCAN

#ifndef USE_RCL
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 * @fn          llAddExtAlAndSetIgnBit
 *
 * @brief       Check if adv is in Extended AL, if not add.
 *              Then check if the ignore bit is on and turn it off.
 *              We don't want to ignore the first occurrence of this Advertising.
 *              Enable the ExtAL ignore bit so that this adv is ignored on the
 *              next occurneces.
 *
 * input parameters
 *
 * @param extAdvRpt pointer to advertisement
 * @param ignoreBit Ignore bit that indicates whether the RxEntry should be
 *                  ignored or processed.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      updated ignore
 */
uint8 llAddExtAlAndSetIgnBit(aeExtAdvRptEvt_t *extAdvRpt, uint8 ignoreBit)
{
  uint8 extAlIdx;
  uint8 newIgnoreBit = ignoreBit;
  if ( (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST) ||
       (extScanInfo->pScanParam->scanFilterPolicy == LL_SCAN_AL_POLICY_USE_ACCEPT_LIST_EXT) )
  {
    // Find in Ext AL
    extAlIdx = MAP_LL_PRIV_FindExtALEntry( GET_AL_TABLE_POINTER(extScanParam.pAcceptList),
                                                extAdvRpt->addr,
                                                extAdvRpt->addrType);

    if (extAlIdx == INVALID_EXT_ACCEPT_LIST_INDEX)
    {
      // not found in Extended AL, add it
      extAlIdx = MAP_LL_PRIV_AddExtALEntry(GET_AL_TABLE_POINTER(extScanParam.pAcceptList),
                                          extAdvRpt->addr,
                                          extAdvRpt->addrType,
                                          PRIV_USE_AL_ENTRY);
      // If added successfully to EXT AL
      if (extAlIdx)
      {
        // Check if Ignore bit is on
        if (newIgnoreBit)
        {
          // This is the first time we see this RPA, and we are able to resolve it.
          // so do not ignore it.
          newIgnoreBit = 0;
        }

        // If duplicate filtering is in use
        if ( (extScanInfo->pEnable->dupFiltering == LL_FILTER_REPORTS_ENABLE) ||
            (extScanInfo->pEnable->dupFiltering == LL_FILTER_REPORTS_RESET_EACH_SCAN_PERIOD) )
        {
          // set the AL ignore bit.
          SET_AL_ENTRY_IGNORE(GET_AL_TABLE_POINTER(extScanParam.pAcceptList)->pAlEntries[extAlIdx].alFlags);
        }
      }
    }
  }
  return newIgnoreBit;
}

/*******************************************************************************
 * @fn          llFlushIgnoredRxEntry
 *
 * @brief       if ignoreBit is 1 flush Rx Entry.
 *
 * input parameters
 *
 * @param ignoreBit Ignore bit that indicates whether the RxEntry should be
 *                  ignored or processed.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      TRUE if Entry is flushed. False otherwise
 */
uint8 llFlushIgnoredRxEntry(uint8 ignoreBit)
{
  // check the ignored bit
  if (ignoreBit)
  {
    // this entry should be ignored, so flush
    MAP_RFHAL_NextDataEntryDone(extScanParam.pRXQ);
    return TRUE;
  }
  return FALSE;
}

/*******************************************************************************
 * @fn          llSetRxCfg
 *
 * @brief       Set the Rx Config
 *              This function is used to set the AutoFlush bit off.
 *              As part of the Scan Optimization
 *
 * input parameters
 *
 * @param None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llSetRxCfg(void)
{
  // set Scan Rx queue configuration
  extScanParam.rxCfg =
      (RXQ_CFG_NO_AUTOFLUSH_IGNORED_PKT |
       RXQ_CFG_AUTOFLUSH_CRC_ERR_PKT |
       RXQ_CFG_AUTOFLUSH_EMPTY_PKT |
       RXQ_CFG_INCLUDE_PKT_LEN_BYTE |
       ((llConfigTable.scanPktSuffixPtr->suffixSel & SUFFIX_CRC_FLAG) ? RXQ_CFG_INCLUDE_CRC : 0) |
       ((llConfigTable.scanPktSuffixPtr->suffixSel & SUFFIX_RSSI_FLAG) ? RXQ_CFG_APPEND_RSSI : 0) |
       ((llConfigTable.scanPktSuffixPtr->suffixSel & SUFFIX_STATUS_FLAG) ? RXQ_CFG_APPEND_STATUS : 0) |
       ((llConfigTable.scanPktSuffixPtr->suffixSel & SUFFIX_TIMESTAMP_FLAG) ? RXQ_CFG_APPEND_TIMESTAMP : 0));
}
#endif // defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
#endif //# !USE_RCL

#if defined(USE_RCL) && defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 * @fn          llRclPrepareAndUpdateAlEntry
 *
 * @brief       This function will prepare a new AL entry and will call the RCL
 *              to update the relevant AL
 *
 * @param       filterList - Pointer to the filter list in use
 *              flags      - The accept list flags that needed to be marked
 *              pAddr      - The new peer address
 *              alIndex    - The index of the peer device entry in the AL
 *
 * @return      None.
 */
void llRclPrepareAndUpdateAlEntry(RCL_FilterList *filterList, uint16 flags, uint8 *pAddr, uint8 alIndex)
{
  alEntry_t newAlEntry;

  // Initialize
  osal_memset(&newAlEntry, 0, sizeof(alEntry_t));

  // Update the AL flags
  newAlEntry.alFlags = flags;

  // Copy the RPA to the new Accept list entry
  MAP_osal_memcpy(newAlEntry.devAddr, pAddr, B_ADDR_LEN);

  // Update the RCL with the new record
  RCL_BLE5_updateFilterList( (RCL_FL_Entry *)&newAlEntry,
                             (RCL_FilterList *)(filterList),
                             alIndex );
}
#endif //defined(USE_RCL) && defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
/*******************************************************************************
 */
