/******************************************************************************

 @file  rom_init.c

 @brief This file contains the externs for BLE Controller and OSAL ROM
        initialization.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2017 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*******************************************************************************
 * INCLUDES
 */
#include "bcomdef.h" // include for DFL flag
#include "map_direct.h"

#ifdef BLE_HEALTH
#include <health_toolkit/inc/debugInfo.h>
#include <health_toolkit/inc/debugInfo_internal.h>
#endif // BLE_HEALTH

#include "ll_ae.h"
#ifndef CONTROLLER_ONLY
#include "gap_advertiser_internal.h"
#include "gap_scanner_internal.h"
#include "gap_internal.h"
#include "sm.h"
#include "l2cap_internal.h"
#include "l2cap_handover.h"
#include "gap_initiator.h"
#include "gap_initiator_internal.h"
#include "sm_internal.h"
#endif //CONTROLLER_ONLY

#ifdef CONNECTION_HANDOVER
#include "ll_handover_cn.h"
#include "ll_handover_sn.h"
#endif

#ifdef CHANNEL_SOUNDING
#include "cs/ll_cs_mgr.h"
#include "cs/ll_cs_procedure.h"
#include "cs/ll_cs_ctrl_pkt_mgr.h"
#include "cs/ll_cs_rcl.h"
#endif

#include "ll.h"
#include "ll_ae.h"
#include "ll_enc.h"
#include "ll_al.h"
#include "ll_timer_drift.h"
#include "ll_rat.h"
#include "ll_privacy.h"

/*******************************************************************************
 * TYPEDEFS
 */
// Use dynamic filter list when the device role is advertiser only and number of bond is greater than 5.
#ifndef USE_DFL
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG)) && !(CTRL_CONFIG & (SCAN_CFG | INIT_CFG)) // (If the device role is advertiser only)
#if defined(GAP_BOND_MGR) && (GAP_BONDINGS_MAX > 5) // If number of bondings greater than 5
  #define USE_DFL
#endif // (advertiser only)
#endif // (number of bondings greater than 5)
#endif // !USE_DFL


/*******************************************************************************
 * @fn          BLE ROM Spinlock
 *
 * @brief       This routine is used to trap indexing errors in R2R JT.
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
void ROM_Spinlock( void )
{
  volatile uint8 i = 1;

  while(i);
}

/*******************************************************************************
 * PROTOTYPES
 */
extern uint8 llLastCmdDoneEventHandleConnectRequest( void );
extern uint8 llRxEntryDoneEventHandleStateConnection( void );
extern uint8 llLastCmdDoneEventHandleStateTest( void );
extern void llCmdStartedEventHandle( void );
extern void llSetTaskInit( uint8 startType, taskInfo_t *nextSecTask, void *nextSecCommand, void *nextConnCmd );
extern void llSetTaskScan( uint8 startType, taskInfo_t *nextSecTask, void *nextSecCommand, void *nextConnCmd );
extern void llSetTaskAdv( uint8 startType, void *nextSecCmd );
extern void llSetTaskCentral( uint8 connId, void *nextConnCmd );
extern void llSetTaskPeripheral( uint8 connId, void *nextConnCmd );
extern void llSetTaskPeriodicAdv( void );
extern void llSetTaskPeriodicScan( void );
extern taskInfo_t *llSelectTaskAdv( uint8 secTaskID, uint32 timeGap );
extern taskInfo_t *llSelectTaskInit( uint8 secTaskID, uint32 timeGap );
extern taskInfo_t *llSelectTaskScan( uint8 secTaskID, uint32 timeGap );
extern taskInfo_t *llSelectTaskPeriodicScan( uint8 secTaskID, uint32 timeGap );
extern taskInfo_t *llSelectTaskPeriodicAdv( uint8 secTaskID, uint32 timeGap );
extern uint8 llCheckIsSecTaskCollideWithPrimTaskInLsto( taskInfo_t *secTask,uint32 timeGap,uint16 selectedConnId);
extern llStatus_t llPostProcessExtendedAdv( advSet_t *pAdvSet );
extern void llSetupExtendedAdvData( advSet_t *pAdvSet );
extern uint8 llSetExtendedAdvReport(aeExtAdvRptEvt_t *extAdvRpt, uint8 *pPkt, uint16 evtType,uint8 extHdrFlgs, uint8 pHdr,uint8 *pData, uint8 dataLen, uint8 **pSyncInfo,uint8 currPhy, uint8 *pChannelIndex);
extern void LL_rclAdvRxEntryDone( void );
extern void LL_rclInitRxEntryDone( void );

extern void llInitCompleteNotify(int status);

/*******************************************************************************
 * INIT_CFG and SCAN_CFG hooks
 */
void MAP_llProcessCentralConnectionCreated(void)
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  llProcessCentralConnectionCreated();
#endif
}

void MAP_llProcessPeripheralConnectionCreated(void)
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  llProcessPeripheralConnectionCreated();
#endif
}

void MAP_llProcessScanTimeout(void)
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
  llProcessScanTimeout();
#endif
}

void MAP_llProcessConnectionEstablishFailed(uint8 role, uint8 reason)
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  llProcessConnectionEstablishFailed(role,reason);
#endif
}

void MAP_llProcessAdvAddrResolutionTimeout(void)
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  llProcessAdvAddrResolutionTimeout();
#endif
}

uint8 MAP_llLSBPreamSimilar(uint32 AccessAddress)
{
#if defined(DeviceFamily_CC23X0R5) || defined(DeviceFamily_CC23X0R53) || defined(DeviceFamily_CC23X0R2) || defined(DeviceFamily_CC23X0R22) || defined(DeviceFamily_CC27XX)
    return llLSBPreamSimilar(AccessAddress);
#else
    return FALSE;
#endif
}

void MAP_llCmdStartedEventHandle( void )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
 llCmdStartedEventHandle();
#endif
}

uint8 MAP_llRxEntryDoneEventHandleStateConnection( void )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  return llRxEntryDoneEventHandleStateConnection();
#else
  return 0;
#endif
}

uint8 MAP_llLastCmdDoneEventHandleStateTest( void )
{
#if 1
  return llLastCmdDoneEventHandleStateTest();
#else
  return 0;
#endif
}

void MAP_llProcessCentralControlPacket(void *connPtr, uint8 *pPkt)
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  llProcessCentralControlPacket(connPtr, pPkt);
#endif
}

void MAP_llProcessPeripheralControlPacket(void *connPtr, uint8 *pPkt)
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  llProcessPeripheralControlPacket(connPtr, pPkt);
#endif
}

void MAP_llSetTaskInit( uint8 startType, void *nextSecTask, void *nextSecCmd, void *nextConnCmd )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  llSetTaskInit(startType, nextSecTask, nextSecCmd,nextConnCmd);
#endif
}

void MAP_llSetTaskScan( uint8 startType, void *nextSecTask, void *nextSecCmd, void *nextConnCmd )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
  llSetTaskScan(startType, nextSecTask, nextSecCmd, nextConnCmd);
#endif
}

void MAP_llSetTaskPeriodicScan( void )
{
#ifdef USE_PERIODIC_SCAN
  llSetTaskPeriodicScan();
#endif
}

void MAP_llSetTaskAdv( uint8 startType, void *nextSecCmd )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  llSetTaskAdv(startType, nextSecCmd);
#endif
}

void MAP_llSetTaskPeriodicAdv( void )
{
#ifdef USE_PERIODIC_ADV
  llSetTaskPeriodicAdv();
#endif
}

void MAP_llSetTaskCentral( uint8 connId, void *nextConnCmd )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  llSetTaskCentral(connId, nextConnCmd);
#endif
}

void MAP_llSetTaskPeripheral( uint8 connId, void *nextConnCmd )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  llSetTaskPeripheral(connId, nextConnCmd);
#endif
}

void *MAP_llSelectTaskAdv( uint8 secTaskID, uint32 timeGap )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  return llSelectTaskAdv(secTaskID,timeGap);
#else
  return NULL;
#endif
}

void *MAP_llSelectTaskInit( uint8 secTaskID, uint32 timeGap )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  return llSelectTaskInit(secTaskID,timeGap);
#else
  return NULL;
#endif
}

void *MAP_llSelectTaskScan( uint8 secTaskID, uint32 timeGap )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
  return llSelectTaskScan(secTaskID,timeGap);
#else
  return NULL;
#endif
}

void *MAP_llSelectTaskPeriodicScan( uint8 secTaskID, uint32 timeGap )
{
#ifdef USE_PERIODIC_SCAN
  return llSelectTaskPeriodicScan(secTaskID,timeGap);
#else
  return NULL;
#endif
}

void *MAP_llSelectTaskPeriodicAdv( uint8 secTaskID, uint32 timeGap )
{
#ifdef USE_PERIODIC_ADV
  return llSelectTaskPeriodicAdv(secTaskID,timeGap);
#else
  return NULL;
#endif
}

uint8 MAP_LE_ClearAdvSets(void)
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  return LE_ClearAdvSets();
#else
  return 0;
#endif
}

uint8 MAP_LL_ConnActive(uint16 connId)
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  return LL_ConnActive(connId);
#else
  return 0;
#endif
}

uint8 MAP_LL_CountAdvSets( uint8 type )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  return LL_CountAdvSets(type);
#else
  return 0;
#endif
}

void *MAP_LL_SearchAdvSet( uint8 handle )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  return LL_GetAdvSet( handle, LE_SEARCH_ADV_SET );
#else
  return NULL;
#endif
}

uint8 MAP_llCheckPeripheralTerminate( uint8 connId )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  return llCheckPeripheralTerminate(connId);
#else
  return 0;
#endif
}

uint8 MAP_llGetNextConn( void )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  return llGetNextConn();
#else
  return 0;
#endif
}

void *MAP_llDataGetConnPtr( uint8 connId )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  return llDataGetConnPtr(connId);
#else
  return NULL;
#endif
}

void MAP_llConnCleanup( void *connPtr )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  llConnCleanup(connPtr);
#endif
}

void MAP_llReleaseAllConnId( void )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  llReleaseAllConnId();
#endif
}

uint8 MAP_llCompareSecondaryPrimaryTasksQoSParam( uint8 qosParamType,
                                                  void *secTask,
                                                  void *primConnPtr )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG | SCAN_CFG))
  return llCompareSecondaryPrimaryTasksQoSParam(qosParamType,secTask,primConnPtr);
#else
  return 0;
#endif
}

uint8 MAP_llCheckIsSecTaskCollideWithPrimTaskInLsto( void *secTask,
                                                     uint32 timeGap,
                                                     uint16 selectedConnId )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG | SCAN_CFG))
  return llCheckIsSecTaskCollideWithPrimTaskInLsto(secTask,timeGap,selectedConnId);
#else
  return 0;
#endif
}

void MAP_llSetupConn( uint8 connId )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  llSetupConn(connId);
#endif
}

void MAP_llProcessExtScanRxFIFO( void )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
  llProcessExtScanRxFIFO();
#endif
}

void MAP_llAlignToNextEvent( void *connPtr )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  llAlignToNextEvent(connPtr);
#endif
}

uint8 MAP_LL_SetSecAdvChanMap( uint8 *chanMap )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  return LL_SetSecAdvChanMap(chanMap);
#else
  return 0;
#endif
}

uint8 MAP_LL_ChanMapUpdate( uint8 *chanMap, uint16 connID )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  return LL_ChanMapUpdate(chanMap, connID);
#else
  return 0;
#endif
}

uint8 MAP_llLastCmdDoneEventHandleConnectRequest( void )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  return llLastCmdDoneEventHandleConnectRequest();
#else
  return 0;
#endif
}

uint8 MAP_llConnExists( uint8 *peerAddr, uint8  peerAddrType)
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  return llConnExists(peerAddr,peerAddrType);
#else
  return 0;
#endif
}

uint8 MAP_llSetStarvationMode(uint16 connId, uint8 setOnOffValue)
{
  return llSetStarvationMode(connId, setOnOffValue);
}

void MAP_llCentral_TaskEnd(void)
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  llCentral_TaskEnd();
#endif
  return;
}

void MAP_llPeripheral_TaskEnd(void)
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  llPeripheral_TaskEnd();
#endif
  return;
}

void MAP_llInitFeatureSet( void )
{
  llInitFeatureSet();

#ifndef USE_AE
  llRemoveFromFeatureSet(1, LL_FEATURE_EXTENDED_ADVERTISING);
#endif // !USE_AE

#if !defined(USE_PERIODIC_ADV) && !defined(USE_PERIODIC_SCAN)
  llRemoveFromFeatureSet(1, LL_FEATURE_PERIODIC_ADVERTISING);
#endif // !USE_PERIODIC_ADV && !USE_PERIODIC_SCAN

#ifndef USE_PERIODIC_ADV
  llRemoveFromFeatureSet(2, LL_FEATURE_CONNECTIONLESS_CTE_TRANSMITTER);
#endif //USE_PERIODIC_ADV

#ifndef USE_PERIODIC_SCAN
  llRemoveFromFeatureSet(2, LL_FEATURE_CONNECTIONLESS_CTE_RECEIVER);
#endif //USE_PERIODIC_SCAN

  llRemoveFromFeatureSet(2, LL_FEATURE_CONNECTION_CTE_REQUEST);
  llRemoveFromFeatureSet(2, LL_FEATURE_CONNECTION_CTE_RESPONSE);
  llRemoveFromFeatureSet(2, LL_FEATURE_ANTENNA_SWITCHING_DURING_CTE_RX);
  llRemoveFromFeatureSet(2, LL_FEATURE_RECEIVING_CTE);
  llRemoveFromFeatureSet(2, LL_FEATURE_CONNECTIONLESS_CTE_TRANSMITTER);
  llRemoveFromFeatureSet(2, LL_FEATURE_CONNECTIONLESS_CTE_RECEIVER);

#ifdef CONNECTION_HANDOVER
  // Disable ping to avoid control procedures
  llRemoveFromFeatureSet(0, LL_FEATURE_PING);
#endif
}


/*******************************************************************************
 * Periodic Adv hooks
 */
uint8 MAP_LE_SetPeriodicAdvParams( uint8 advHandle,
                                   uint16 periodicAdvIntervalMin,
                                   uint16 periodicAdvIntervalMax,
                                   uint16 periodicAdvProp )
{
#ifdef USE_PERIODIC_ADV
  return LE_SetPeriodicAdvParams(advHandle,
                                 periodicAdvIntervalMin,
                                 periodicAdvIntervalMax,
                                 periodicAdvProp);
#else
  return 1;
#endif
}

uint8 MAP_LE_SetPeriodicAdvData( uint8 advHandle, uint8 operation,
                                 uint8 dataLength, uint8 *data )
{
#ifdef USE_PERIODIC_ADV
  return LE_SetPeriodicAdvData( advHandle,operation,dataLength,data );
#else
  return 1;
#endif
}

uint8 MAP_LE_SetPeriodicAdvEnable( uint8 enable,uint8 advHandle )
{
#ifdef USE_PERIODIC_ADV
  return LE_SetPeriodicAdvEnable( enable, advHandle);
#else
  return 1;
#endif
}

void *MAP_llGetPeriodicAdv( uint8 handle )
{
#ifdef USE_PERIODIC_ADV
  return llGetPeriodicAdv( handle );
#else
  return NULL;
#endif
}

void MAP_llUpdatePeriodicAdvChainPacket( void )
{
#ifdef USE_PERIODIC_ADV
  llUpdatePeriodicAdvChainPacket();
#endif
}

void MAP_llSetPeriodicAdvChmapUpdate( uint8 set )
{
#ifdef USE_PERIODIC_ADV
  llSetPeriodicAdvChmapUpdate( set );
#endif
}

void MAP_llPeriodicAdv_PostProcess( void )
{
#ifdef USE_PERIODIC_ADV
  llPeriodicAdv_PostProcess();
#endif
}

uint8 MAP_llTrigPeriodicAdv( void *pAdvSet, void *pPeriodicAdv )
{
#ifdef USE_PERIODIC_ADV
  return llTrigPeriodicAdv( pAdvSet, pPeriodicAdv);
#else
  return 0;
#endif
}

uint8 MAP_llSetupPeriodicAdv( void *pAdvSet )
{
#ifdef USE_PERIODIC_ADV
  return llSetupPeriodicAdv( pAdvSet );
#else
  return 0;
#endif
}

void MAP_llEndPeriodicAdvTask( void *pPeriodicAdv )
{
#ifdef USE_PERIODIC_ADV
  llEndPeriodicAdvTask( pPeriodicAdv );
#endif
}

void *MAP_llFindNextPeriodicAdv( void )
{
#ifdef USE_PERIODIC_ADV
  return llFindNextPeriodicAdv();
#else
  return NULL;
#endif
}

void MAP_llSetPeriodicSyncInfo( void *pAdvSet, uint8 *pBuf )
{
#ifdef USE_PERIODIC_ADV
  llSetPeriodicSyncInfo(pAdvSet,pBuf);
#endif
}

void *MAP_llGetCurrentPeriodicAdv( void )
{
#ifdef USE_PERIODIC_ADV
  return llGetCurrentPeriodicAdv();
#else
  return NULL;
#endif
}

uint8 MAP_gapAdv_periodicAdvCmdCompleteCBs( void *pMsg )
{
#if ( HOST_CONFIG & ( PERIPHERAL_CFG | BROADCASTER_CFG ) ) && defined(USE_PERIODIC_ADV)
  return gapAdv_periodicAdvCmdCompleteCBs(pMsg);
#else
  return TRUE;
#endif
}

void MAP_llClearPeriodicAdvSets( void )
{
#ifdef USE_PERIODIC_ADV
  llClearPeriodicAdvSets();
#endif // USE_PERIODIC_ADV
}

uint8 MAP_llAddPeriodicAdvPacketToTx( void *pPeriodicAdv, uint8 pktType, uint8 payloadLen )
{
#ifdef USE_PERIODIC_ADV
  return llAddPeriodicAdvPacketToTx( pPeriodicAdv, pktType, payloadLen );
#else
  return TRUE;
#endif
}

/*******************************************************************************
 * Periodic Scan hooks
 */

uint8 MAP_LE_PeriodicAdvCreateSync( uint8  options, uint8  advSID, uint8  advAddrType, uint8  *advAddress,
                                    uint16 skip, uint16 syncTimeout, uint8  syncCteType )
{
#ifdef USE_PERIODIC_SCAN
  return LE_PeriodicAdvCreateSync( options, advSID, advAddrType, advAddress, skip, syncTimeout, syncCteType );
#else
  return 1;
#endif
}

uint8 MAP_LE_PeriodicAdvCreateSyncCancel( void )
{
#ifdef USE_PERIODIC_SCAN
  return LE_PeriodicAdvCreateSyncCancel();
#else
  return 1;
#endif
}

uint8 MAP_LE_PeriodicAdvTerminateSync( uint16 syncHandle )
{
#ifdef USE_PERIODIC_SCAN
  return LE_PeriodicAdvTerminateSync( syncHandle );
#else
  return 1;
#endif
}

uint8 MAP_LE_AddDeviceToPeriodicAdvList( uint8 advAddrType, uint8 *advAddress, uint8 advSID )
{
#ifdef USE_PERIODIC_SCAN
  return LE_AddDeviceToPeriodicAdvList( advAddrType, advAddress, advSID );
#else
  return 1;
#endif
}

uint8 MAP_LE_RemoveDeviceFromPeriodicAdvList( uint8 advAddrType, uint8 *advAddress, uint8 advSID )
{
#ifdef USE_PERIODIC_SCAN
  return LE_RemoveDeviceFromPeriodicAdvList( advAddrType, advAddress, advSID);
#else
  return 1;
#endif
}

uint8 MAP_LE_ClearPeriodicAdvList( void )
{
#ifdef USE_PERIODIC_SCAN
  return LE_ClearPeriodicAdvList();
#else
  return 1;
#endif
}

uint8 MAP_LE_ReadPeriodicAdvListSize( uint8 *listSize )
{
#ifdef USE_PERIODIC_SCAN
  return LE_ReadPeriodicAdvListSize( listSize );
#else
  return 1;
#endif
}

uint8 MAP_LE_SetPeriodicAdvReceiveEnable( uint16 syncHandle, uint8  enable )
{
#ifdef USE_PERIODIC_SCAN
  return LE_SetPeriodicAdvReceiveEnable( syncHandle, enable);
#else
  return 1;
#endif
}

void MAP_llProcessPeriodicScanSyncInfo( uint8 *pPkt, void *advEvent, uint32 timeStamp, uint8 phy )
{
#ifdef USE_PERIODIC_SCAN
  llProcessPeriodicScanSyncInfo( pPkt, advEvent, timeStamp, phy );
#endif
}

void MAP_llEndPeriodicScanTask( void *pPeriodicScan )
{
#ifdef USE_PERIODIC_SCAN
  llEndPeriodicScanTask( pPeriodicScan );
#endif
}

void MAP_llPeriodicScan_PostProcess( void )
{
#ifdef USE_PERIODIC_SCAN
  llPeriodicScan_PostProcess();
#endif
}

void MAP_llProcessPeriodicScanRxFIFO( void )
{
#ifdef USE_PERIODIC_SCAN
  llProcessPeriodicScanRxFIFO();
#endif
}

void *MAP_llFindNextPeriodicScan( void )
{
#ifdef USE_PERIODIC_SCAN
  return llFindNextPeriodicScan();
#else
  return NULL;
#endif
}

void MAP_llTerminatePeriodicScan( void )
{
#ifdef USE_PERIODIC_SCAN
  llTerminatePeriodicScan();
#endif
}

void *MAP_llGetCurrentPeriodicScan( uint8 state )
{
#ifdef USE_PERIODIC_SCAN
  return llGetCurrentPeriodicScan(state);
#else
  return NULL;
#endif
}

void *MAP_llGetPeriodicScan( uint16 handle )
{
#ifdef USE_PERIODIC_SCAN
  return llGetPeriodicScan(handle);
#else
  return NULL;
#endif
}

uint8_t MAP_gapScan_periodicAdvCmdCompleteCBs( void *pMsg )
{
  uint8_t status = TRUE;
#ifndef CONTROLLER_ONLY
#ifdef USE_PERIODIC_SCAN
  status = gapScan_periodicAdvCmdCompleteCBs(pMsg);
#endif // USE_PERIODIC_SCAN
#endif // CONTROLLER_ONLY
  return status;
}

uint8_t MAP_gapScan_periodicAdvCmdStatusCBs( void *pMsg )
{
  uint8_t status = TRUE;
#ifndef CONTROLLER_ONLY
#ifdef USE_PERIODIC_SCAN
  status = gapScan_periodicAdvCmdStatusCBs(pMsg);
#endif // USE_PERIODIC_SCAN
#endif // CONTROLLER_ONLY
  return status;
}

uint8_t MAP_gapScan_processBLEPeriodicAdvCBs( void *pMsg )
{
  uint8_t status = TRUE;
#ifndef CONTROLLER_ONLY
#ifdef USE_PERIODIC_SCAN
  status = gapScan_processBLEPeriodicAdvCBs(pMsg);
#endif // USE_PERIODIC_SCAN
#endif // CONTROLLER_ONLY
  return status;
}

void MAP_llClearPeriodicScanSets( void )
{
#ifdef USE_PERIODIC_SCAN
  llClearPeriodicScanSets();
#endif
}

void MAP_llUpdateExtScanAcceptSyncInfo( void )
{
#ifdef USE_PERIODIC_SCAN
  llUpdateExtScanAcceptSyncInfo();
#endif
}

/*******************************************************************************
 * Extended Advertising hooks
 */
uint8 MAP_llSetExtendedAdvParams( void *pAdvSet, void *pCmdParams )
{
#ifdef USE_AE
  return llSetExtendedAdvParams(pAdvSet,pCmdParams);
#else
  return 1;
#endif
}

uint8 MAP_llSetupExtAdv( void *pAdvSet )
{
#ifdef USE_AE
  return llSetupExtAdv(pAdvSet);
#else
  return 1;
#endif
}

llStatus_t MAP_llPostProcessExtendedAdv( void *pAdvSet )
{
#ifdef USE_AE
  return llPostProcessExtendedAdv(pAdvSet);
#else
  return 0;
#endif
}

void MAP_llSetupExtendedAdvData( void *pAdvSet )
{
#ifdef USE_AE
  llSetupExtendedAdvData(pAdvSet);
#endif
}

uint8 MAP_llSetExtendedAdvReport(void *extAdvRpt,
                                 uint8 *pPkt,
                                 uint16 evtType,
                                 uint8 extHdrFlgs,
                                 uint8 pHdr,
                                 uint8 *pData,
                                 uint8 dataLen,
                                 uint8 **pSyncInfo,
                                 uint8 currPhy,
                                 uint8 *pChannelIndex)
{
#ifdef USE_AE
  return llSetExtendedAdvReport(extAdvRpt,pPkt,evtType,extHdrFlgs,
                                pHdr,pData ,dataLen,pSyncInfo,currPhy,pChannelIndex);
#else
  return 0;
#endif
}

uint8 MAP_llAddExtAdvPacketToTx(void *pAdvSet, uint8 pktType, uint8 payloadLen)
{
#ifdef USE_AE
  return llAddExtAdvPacketToTx(pAdvSet, pktType, payloadLen);
#else
  return 1;
#endif
}

uint8 MAP_llBuildExtAdvPacket(void *pPkt, void *comPkt, uint8 pktType, uint8 payloadLen, uint8 peerAddrType, uint8 ownAddrType)
{
#ifdef USE_AE
  return llBuildExtAdvPacket(pPkt, comPkt, pktType, payloadLen, peerAddrType, ownAddrType);
#else
  return 1;
#endif
}

uint8 MAP_llupdateAuxHdrPacket(void *pAdvSet)
{
#ifdef USE_AE
  return llupdateAuxHdrPacket(pAdvSet);
#else
  return 1;
#endif
}

/**
* These hooks created to change the call to the relevant HCI command
* instead of calling the controller directly.
* This is needed to support the relevant command complete events
* that are passed to the application when using BLE3_CMD
* compilation flag
*/
uint8_t LE_SetExtAdvData_hook( void * pMsg )
{
#ifdef BLE3_CMD
  return HCI_LE_SetExtAdvData(pMsg);
#else
  return LE_SetExtAdvData(pMsg);
#endif
}
uint8_t LE_SetExtScanRspData_hook( void * pMsg)
{
#ifdef BLE3_CMD
  return HCI_LE_SetExtScanRspData(pMsg);
#else
  return LE_SetExtScanRspData(pMsg);
#endif
}

uint8_t LE_SetExtAdvEnable_hook( void * pMsg)
{
#ifdef BLE3_CMD
  return HCI_LE_SetAdvStatus(pMsg);
#else
  return LE_SetExtAdvEnable(pMsg);
#endif
}


uint8 MAP_gapAdv_handleAdvHciCmdComplete( void *pMsg )
{
#ifdef BLE3_CMD
  return gapAdv_handleAdvHciCmdComplete(pMsg);
#else
  return TRUE;
#endif
}

/*******************************************************************************
 * Health check
 */
int8 MAP_llHealthCheck( void )
{
#ifdef USE_HEALTH_CHECK
  return llHealthCheck();
#else
  return 1;
#endif
}

void MAP_llHealthUpdate(uint8 state)
{
#ifdef USE_HEALTH_CHECK
  llHealthUpdate(state);
#endif
}

void MAP_llHealthSetThreshold(uint32 connTime, uint32 scanTime, uint32 initTime, uint32 advTime)
{
#ifdef USE_HEALTH_CHECK
  llHealthSetThreshold(connTime, scanTime, initTime, advTime);
#endif
}

/*******************************************************************************
 * Check legacy command status
 */
#ifdef LEGACY_CMD
extern uint8_t checkLegacyHCICmdStatus(uint16_t opcode);
#endif

uint8_t MAP_checkLegacyHCICmdStatus(uint16_t opcode)
{
#ifdef LEGACY_CMD
  return checkLegacyHCICmdStatus(opcode);
#else
  return FALSE;
#endif
}

/*******************************************************************************
 * Check auto feature exchange status
 */
uint8_t MAP_checkAutoFeatureExchangeStatus(void)
{
#ifdef DISABLE_AUTO_FEATURE_REQ
  return FALSE;
#else
  return TRUE;
#endif
}

/*******************************************************************************
 * Check vendor specific events status
 */
uint8_t MAP_checkVsEventsStatus(void)
{
#ifdef DISABLE_VS_EVENTS
  return FALSE;
#else
  return TRUE;
#endif
}

/*******************************************************************************
 * Scan Optimization
 */
uint8 MAP_llAddExtAlAndSetIgnBit(void *extAdvRpt, uint8 ignoreBit)
{
  return ignoreBit;
}

uint8 MAP_llFlushIgnoredRxEntry(uint8 ignoreBit)
{
  return FALSE;
}

/*******************************************************************************
 * Link time configuration functions
 */

// (CENTRAL_CFG | OBSERVER_CFG) functions
uint8 MAP_gapScan_init(void)
{
#if ( HOST_CONFIG & ( CENTRAL_CFG | OBSERVER_CFG ) )
  return gapScan_init();
#else
  return LL_STATUS_SUCCESS;
#endif
}
uint8 MAP_SM_InitiatorInit(void)
{
#if ( HOST_CONFIG & ( CENTRAL_CFG | OBSERVER_CFG ) )
  return SM_InitiatorInit();
#else
  return LL_STATUS_SUCCESS;
#endif
}
void MAP_gap_CentConnRegister(void)
{
#if ( HOST_CONFIG & ( CENTRAL_CFG | OBSERVER_CFG ) )
  gap_CentConnRegister();
#endif
}
void MAP_gapScan_processSessionEndEvt(void* pSession, uint8_t status)
{
#if ( HOST_CONFIG & ( CENTRAL_CFG | OBSERVER_CFG ) )
  gapScan_processSessionEndEvt( pSession, status);
#endif
}

// (PERIPHERAL_CFG | BROADCASTER_CFG) functions
uint8 MAP_gapAdv_init(void)
{
#if ( HOST_CONFIG & ( PERIPHERAL_CFG | BROADCASTER_CFG ) )
  return gapAdv_init();
#else
  return LL_STATUS_SUCCESS;
#endif
}
uint8 MAP_SM_ResponderInit(void)
{
#if ( HOST_CONFIG & ( PERIPHERAL_CFG ) )
  return SM_ResponderInit();
#else
  return LL_STATUS_SUCCESS;
#endif
}
void MAP_gap_PeriConnRegister(void)
{
#if ( HOST_CONFIG & ( PERIPHERAL_CFG ) )
  gap_PeriConnRegister();
#endif
}

// (ADV_CONN_CFG) functions
void MAP_llExtAdv_PostProcess(void)
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  llExtAdv_PostProcess();
#endif
}

// (SCAN_CFG) functions
void MAP_llExtScan_PostProcess(void)
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
  llExtScan_PostProcess();
#endif
}

// (SCAN_CFG) functions
void MAP_LL_rclScanRxEntryDone(void)
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & SCAN_CFG)
  LL_rclScanRxEntryDone();
#endif
}

// (INIT_CFG) functions
void MAP_llExtInit_PostProcess(void)
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  llExtInit_PostProcess();
#endif
}

// (L2CAP_COC_CFG) functions
uint8  MAP_l2capSendNextSegment(void)
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  return l2capSendNextSegment();
#else
  return ( FALSE );
#endif
}
uint8  MAP_l2capReassembleSegment(void *pPkt )
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  return l2capReassembleSegment( pPkt );
#else
  return ( TRUE );
#endif
}
uint8  MAP_L2CAP_ParseConnectReq( void *pCmd, uint8 *pData, uint16 len )
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  return L2CAP_ParseConnectReq( pCmd, pData, len );
#else
  return ( FAILURE );
#endif
}
uint8  MAP_l2capParseConnectRsp( void *pCmd, uint8 *pData, uint16 len )
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  return l2capParseConnectRsp( pCmd, pData, len );
#else
  return ( FAILURE );
#endif
}
uint8  MAP_L2CAP_ParseFlowCtrlCredit( void *pCmd, uint8 *pData, uint16 len )
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  return L2CAP_ParseFlowCtrlCredit( pCmd, pData, len );
#else
  return ( FAILURE );
#endif
}
uint8  MAP_l2capParseDisconnectReq( void *pCmd, uint8 *pData, uint16 len )
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  return l2capParseDisconnectReq( pCmd, pData, len );
#else
  return ( FAILURE );
#endif
}
uint8  MAP_l2capParseDisconnectRsp( void *pCmd, uint8 *pData, uint16 len )
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  return l2capParseDisconnectRsp( pCmd, pData, len );
#else
  return ( FAILURE );
#endif
}
uint8  MAP_L2CAP_DisconnectReq( uint16 connHandle, uint16 CID )
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  return L2CAP_DisconnectReq( connHandle, CID );
#else
  return ( INVALIDPARAMETER );
#endif
}
uint16 MAP_l2capBuildDisconnectRsp( uint8 *pBuf, uint8 *pData )
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  return l2capBuildDisconnectRsp( pBuf, pData );
#else
  return ( FAILURE );
#endif
}
void   MAP_l2capProcessConnectReq( uint16 connHandle, uint8 id, void *pConnReq )
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  l2capProcessConnectReq( connHandle, id, pConnReq );
#endif
}
void   MAP_l2capGetCoChannelInfo( void *pChannel, void *pInfo )
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  l2capGetCoChannelInfo( pChannel, pInfo );
#endif
}
void   MAP_l2capNotifyChannelEstEvt( void *pChannel, uint8 status, uint16 result )
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  l2capNotifyChannelEstEvt( pChannel, status, result );
#endif
}
void  *MAP_l2capFindRemoteCID( uint16 connHandle, uint16 CID )
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  return l2capFindRemoteCID( connHandle, CID );
#else
  return ( NULL );
#endif
}
void   MAP_l2capNotifyChannelTermEvt( void *pChannel, uint8 status, uint16 reason )
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  l2capNotifyChannelTermEvt( pChannel, status, reason );
#endif
}
void  *MAP_l2capFindLocalCID( uint16 connHandle, uint16 CID )
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  return l2capFindLocalCID( connHandle, CID );
#else
  return ( NULL );
#endif
}
void   MAP_l2capDisconnectChannel( void *pChannel, uint16 reason )
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  l2capDisconnectChannel( pChannel, reason );
#endif
}

// (CENTRAL_CFG) functions
uint8 MAP_gapIsInitiating( void )
{
#if (HOST_CONFIG & CENTRAL_CFG)
  return gapIsInitiating();
#else
  return ( FALSE );
#endif
}
uint8 MAP_GapInit_cancelConnect( void )
{
#if (HOST_CONFIG & CENTRAL_CFG)
  return GapInit_cancelConnect();
#else
  return ( bleIncorrectMode );
#endif
}
uint8 MAP_smpInitiatorContProcessPairingPubKey( void )
{
#if (HOST_CONFIG & CENTRAL_CFG)
  return smpInitiatorContProcessPairingPubKey();
#else
  return LL_STATUS_ERROR_INVALID_PARAMS;
#endif
}
void MAP_gapInit_initiatingEnd( void )
{
#if (HOST_CONFIG & CENTRAL_CFG)
  gapInit_initiatingEnd();
#endif
}
void MAP_gapInit_sendConnCancelledEvt( void )
{
#if (HOST_CONFIG & CENTRAL_CFG)
  gapInit_sendConnCancelledEvt();
#endif
}

/*******************************************************************************
* Control packet setup function
*/
void MAP_llBuildCtrlPktPeri( llConnState_t *connPtr, uint8 *pData, uint8_t ctrlPkt )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  llBuildCtrlPktPeri(connPtr, pData, ctrlPkt);
#endif
}
void MAP_llBuildCtrlPktCent( llConnState_t *connPtr, uint8 *pData, uint8_t ctrlPkt )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  llBuildCtrlPktCent(connPtr, pData, ctrlPkt);
#endif
}
void MAP_llPostSetupCtrlPktPeri( llConnState_t *connPtr, uint8_t ctrlPkt )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  llPostSetupCtrlPktPeri(connPtr, ctrlPkt);
#endif
}
void MAP_llPostSetupCtrlPktCent( llConnState_t *connPtr, uint8_t ctrlPkt )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  llPostSetupCtrlPktCent(connPtr, ctrlPkt);
#endif
}
/*******************************************************************************
* Health Toolkit
*/

uint8_t MAP_llDbgInf_addSchedRec(void * const llTask)
{
#ifdef BLE_HEALTH
   return llDbgInf_addSchedRec(llTask);
#else
   return UFAILURE;
#endif
}

uint8_t MAP_DbgInf_addSchedRec(void * const newRec)
{
#ifdef BLE_HEALTH
   return DbgInf_addSchedRec(newRec);
#else
   return UFAILURE;
#endif
}

uint8_t MAP_DbgInf_addConnEst(uint16_t connHandle, uint8_t connRole, uint8_t encEnabled)
{
#ifdef BLE_HEALTH
   return DbgInf_addConnEst(connHandle, connRole, encEnabled);
#else
   return UFAILURE;
#endif
}

uint8_t MAP_llDbgInf_addConnTerm(uint16_t connHandle, uint8_t reasonCode)
{
#ifdef BLE_HEALTH
   return llDbgInf_addConnTerm(connHandle, reasonCode);
#else
   return UFAILURE;
#endif
}

uint8_t MAP_DbgInf_addConnTerm(void * const newRec)
{
#ifdef BLE_HEALTH
   return DbgInf_addConnTerm(newRec);
#else
   return UFAILURE;
#endif
}

uint8_t MAP_DbgInf_addErrorRec(uint16_t newError)
{
#ifdef BLE_HEALTH
   return DbgInf_addErrorRec(newError);
#else
   return UFAILURE;
#endif
}

/*******************************************************************************
 * BLE Scheduler preemption
 */

uint8 MAP_llCheckRfCmdPreemption(uint32 endTime, uint8 priority)
{
#ifdef BLE_SCHEDULER_PREEMPTION
    return llCheckRfCmdPreemption(endTime, priority);
#else
    return (FALSE);
#endif
}

/*******************************************************************************
 * Channel Sounding
 */

// CS APIs
uint8 MAP_LL_CS_ReadLocalSupportedCapabilites(void * pCapabilities)
{
#ifdef CHANNEL_SOUNDING
  return LL_CS_ReadLocalSupportedCapabilites(pCapabilities);
#else
  return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

uint8 MAP_LL_CS_ReadRemoteSupportedCapabilities(uint16 connId)
{
#ifdef CHANNEL_SOUNDING
  return LL_CS_ReadRemoteSupportedCapabilities(connId);
#else
  return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

uint8 MAP_LL_CS_CreateConfig(uint16 connId, void *pConfig, uint8 createContext)
{
#ifdef CHANNEL_SOUNDING
  return LL_CS_CreateConfig(connId, pConfig, createContext);
#else
  return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

uint8 MAP_LL_CS_RemoveConfig(uint16 connId, uint8 configId)
{
#ifdef CHANNEL_SOUNDING
  return LL_CS_RemoveConfig(connId, configId);
#else
  return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

uint8 MAP_LL_CS_SecurityEnable(uint16 connId)
{
#ifdef CHANNEL_SOUNDING
  return LL_CS_SecurityEnable(connId);
#else
  return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

uint8 MAP_LL_CS_SetDefaultSettings(uint16 connId, void * pDefSettings)
{
#ifdef CHANNEL_SOUNDING
  return LL_CS_SetDefaultSettings(connId, pDefSettings);
#else
  return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

uint8 MAP_LL_CS_ReadLocalFAETable(void * pFaeTbl)
{
#ifdef CHANNEL_SOUNDING
  return LL_CS_ReadLocalFAETable(pFaeTbl);
#else
	return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

uint8 MAP_LL_CS_ReadRemoteFAETable(uint16 connId)
{
#ifdef CHANNEL_SOUNDING
  return LL_CS_ReadRemoteFAETable(connId);
#else
	return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

uint8 MAP_LL_CS_WriteRemoteFAETable(uint16 connId, void * pFaeTbl)
{
#ifdef CHANNEL_SOUNDING
  return LL_CS_WriteRemoteFAETable(connId, pFaeTbl);
#else
	return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

uint8 MAP_LL_CS_SetChannelClassification(void * pChannelClassification)
{
#ifdef CHANNEL_SOUNDING
  return LL_CS_SetChannelClassification(pChannelClassification);
#else
	return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

uint8 MAP_LL_CS_SetProcedureParameters(uint16 connId, uint8 configId, void * pProcParams)
{
#ifdef CHANNEL_SOUNDING
  return LL_CS_SetProcedureParameters(connId, configId, pProcParams);
#else
	return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

uint8 MAP_LL_CS_ProcedureEnable(uint16 connId, uint8 configId, uint8 enable)
{
#ifdef CHANNEL_SOUNDING
  return LL_CS_ProcedureEnable(connId, configId, enable);
#else
	return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

uint8 MAP_LL_CS_Test( uint8* pTestParams )
{
#if defined(CHANNEL_SOUNDING) && defined(CS_TEST)
  return LL_CS_Test( (csTestParams_t*)pTestParams);
#else
  *pTestParams = 0U;
  return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

uint8 MAP_LL_CS_TestEnd(void)
{
#if defined(CHANNEL_SOUNDING) && defined(CS_TEST)
  return LL_CS_TestEnd();
#else
  return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

void MAP_HCI_CS_ReadRemoteSupportedCapabilitiesCback(uint8 status, uint16 connHandle, void * pCapabilities)
{
#ifdef CHANNEL_SOUNDING
  HCI_CS_ReadRemoteSupportedCapabilitiesCback(status, connHandle, pCapabilities);
#endif
}

void MAP_HCI_CS_ConfigCompleteCback(uint8 status, uint16 connHandle, void * pConfig)
{
#ifdef CHANNEL_SOUNDING
  HCI_CS_ConfigCompleteCback(status, connHandle, pConfig);
#endif
}

void MAP_HCI_CS_ReadRemoteFAETableCompleteCback(uint8 status, uint16 connHandle, void * pFaeTbl)
{
#ifdef CHANNEL_SOUNDING
  HCI_CS_ReadRemoteFAETableCompleteCback(status, connHandle, pFaeTbl);
#endif
}

void MAP_HCI_CS_SecurityEnableCompleteCback(uint8 status, uint16 connId)
{
#ifdef CHANNEL_SOUNDING
  HCI_CS_SecurityEnableCompleteCback(status, connId);
#endif
}

void MAP_HCI_CS_ProcedureEnableCompleteCback(uint8 status, uint16 connId, uint8 enable, void * pEnableData)
{
#ifdef CHANNEL_SOUNDING
  HCI_CS_ProcedureEnableCompleteCback(status, connId, enable, pEnableData);
#endif
}

void MAP_HCI_CS_SubeventResultCback(void* pData, uint16 dataLength)
{
#ifdef CHANNEL_SOUNDING
  HCI_CS_SubeventResultCback(pData, dataLength);
#endif
}

void MAP_HCI_CS_SubeventResultContinueCback(void * hdr, void * data, uint16 dataLength)
{
#ifdef CHANNEL_SOUNDING
  HCI_CS_SubeventResultContinueCback(hdr, data, dataLength);
#endif
}

void MAP_HCI_CS_TestEndCompleteCback(uint8 status)
{
#if defined(CHANNEL_SOUNDING) && defined(CS_TEST)
  HCI_CS_TestEndCompleteCback(status);
#endif
}

// CS LL PKT MGR
uint8 MAP_llCsProcessCsControlPacket(uint8 ctrlType, void * connPtr, void * pBuf)
{
#ifdef CHANNEL_SOUNDING
  return llCsProcessCsControlPacket(ctrlType, connPtr, pBuf);
#else
	return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

uint8 MAP_llCsProcessCsCtrlProcedures(void * connPtr, uint8 ctrlPkt)
{
#ifdef CHANNEL_SOUNDING
  return llCsProcessCsCtrlProcedures(connPtr, ctrlPkt);
#else
  return LL_CTRL_PROC_STATUS_SUCCESS;
#endif
}

// CS PROCEDURES
uint8 MAP_llCsInit(void)
{
#ifdef CHANNEL_SOUNDING
    return llCsInit();
#else
  return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

void MAP_llCsClearConnProcedures(uint16 connId)
{
#ifdef CHANNEL_SOUNDING
  llCsClearConnProcedures(connId);
#endif
}

void MAP_llCsFreeAll(void)
{
#ifdef CHANNEL_SOUNDING
  llCsFreeAll();
#endif
}

void MAP_llCsSetFeatureBit(void)
{
#ifdef CHANNEL_SOUNDING
  llCsSetFeatureBit();
#endif
}

uint8 MAP_llCsStartProcedure(void * connPtr)
{
#ifdef CHANNEL_SOUNDING
  return llCsStartProcedure(connPtr);
#else
  return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

uint8 MAP_llCsStartStepListGen(uint16 connId)
{
#ifdef CHANNEL_SOUNDING
  return llCsStartStepListGen(connId);
#else
  return LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

void MAP_llCsSubevent_PostProcess(void)
{
#ifdef CHANNEL_SOUNDING
  llCsSubevent_PostProcess();
#else
  return;
#endif
}

void MAP_llCsSteps_PostProcess(void)
{
#ifdef CHANNEL_SOUNDING
  llCsSteps_PostProcess();
#else
  return;
#endif
}

void* MAP_llScheduler_getHandle(uint16 taskID)
{
#ifdef CHANNEL_SOUNDING
  return (void*) llScheduler_getHandle(taskID);
#else
  return (void*) llScheduler_getBleHandle();
#endif
}

uint32 MAP_llScheduler_getSwitchTime(uint16 taskID)
{
#ifdef CHANNEL_SOUNDING
  return llScheduler_getSwitchTime(taskID);
#else
  return 0;
#endif
}

uint8 MAP_llCsInitChanIdxArr(uint8 configId, uint16 connId, uint8* config)
{
#ifdef CHANNEL_SOUNDING
#ifdef CS_TEST
  return llCsInitChanIdxArrOverride(configId, connId, (csConfigurationSet_t*)config);
#else
  return llCsInitChanIdxArr(configId, connId, (csConfigurationSet_t*)config);
#endif
#else
  return 0;
#endif
}

uint8 MAP_llCsSelectStepChannel(uint16 connId, uint8* config, uint8 stepMode)
{
#ifdef CHANNEL_SOUNDING
#ifdef CS_TEST
  return llCsSelectStepChanOverride(stepMode, connId, config);
#else
  return llCsSelectStepChannel(stepMode, connId, FALSE, (csConfigurationSet_t*)config);
#endif
#else
  return 0;
#endif
}

void MAP_llCsSelectAA(uint8 csRole, uint32_t* aaRx, uint32_t* aaTx)
{
#ifdef CHANNEL_SOUNDING
#ifdef CS_TEST
  llCsSelectAAOverride(csRole, aaRx, aaTx);
#else
  llCsSelectAA(csRole, aaRx, aaTx);
#endif
#endif
}

void MAP_llCsGetRandomSequence(uint8 csRole, uint32_t* pTx, uint32_t* pRx, uint8 plLen)
{
#ifdef CHANNEL_SOUNDING
#ifdef CS_TEST
  llCsGetRandomSequenceOveride(csRole, pTx, pRx, plLen);
#else
  llCsGetRandomSequence(csRole, pTx, pRx, plLen);
#endif
#endif
}

uint8 MAP_llCsGetToneExtention(void)
{
#ifdef CHANNEL_SOUNDING
#ifdef CS_TEST
  return llCsGetToneExtentionOverride();
#else
  return llCsGetToneExtention();
#endif
#else
  return 0xFF;
#endif
}

/*******************************************************************************
* Connection Handover
*/
uint8 MAP_llHandoverTriggerDataTransfer( void )
{
#ifdef CONNECTION_HANDOVER
  return llHandoverTriggerDataTransfer();
#else
  return FAILURE;
#endif
}

void MAP_llRemoveHandoverConn(uint8 *activeConnsArray, uint8 numActiveConns)
{
#ifdef CONNECTION_HANDOVER
  llRemoveHandoverConn(activeConnsArray, numActiveConns);
#endif
}

uint16_t MAP_llReturnNonHandoverConn( void )
{
#ifdef CONNECTION_HANDOVER
  return llReturnNonHandoverConn();
#else
  return LL_CONNHANDLE_INVALID;
#endif
}

void MAP_llHandoverCheckTermConnAndTerm( void )
{
#ifdef CONNECTION_HANDOVER
  llHandoverCheckTermConnAndTerm();
#endif
}

uint8 MAP_llHandoverNotifyConnStatus(uint16_t connHandle, uint32_t handoverStatus)
{
#ifdef CONNECTION_HANDOVER
  return llHandoverNotifyConnStatus(connHandle, handoverStatus);
#else
  return USUCCESS;
#endif
}

uint8 MAP_llIsHandoverInProgress( llConnState_t *connPtr )
{
#ifdef CONNECTION_HANDOVER
  return llIsHandoverInProgress(connPtr);
#else
  return UFALSE;
#endif
}

void MAP_L2CAP_HandoverInitSN( void )
{
#if defined (CONNECTION_HANDOVER) && defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  L2CAP_HandoverInitSN();
#endif
}

uint8 MAP_L2CAP_Handover_StartCN(uint8_t *pHandoverData, uint32_t dataSize )
{
#if defined (CONNECTION_HANDOVER) && defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  return L2CAP_Handover_StartCN(pHandoverData, dataSize);
#else
  return FAILURE;
#endif
}
uint8 MAP_L2CAP_Handover_GetSNDataSize( uint16_t connHandle )
{
#if defined (CONNECTION_HANDOVER) && defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  return L2CAP_Handover_GetSNDataSize(connHandle);
#else
  return 0;
#endif
}

void MAP_L2CAP_HandoverApplyDataCN(uint16_t connHandle)
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  L2CAP_HandoverApplyDataCN(connHandle);
#endif
}

void *MAP_l2capHandoverGetPsm(void *pHandoverPsmData)
{
#if defined (BLE_V41_FEATURES) && (BLE_V41_FEATURES & L2CAP_COC_CFG)
  return (void *)l2capHandoverGetPsm((l2capHandoverPsmData_t *)pHandoverPsmData);
#else
  return NULL;
#endif
}

/*******************************************************************************
* HCI CMD parser functions
*/

extern hciStatus_t hciCmdParserLegacy( uint8 *pData, uint16 cmdOpCode );
extern hciStatus_t hciCmdParserConnection( uint8 *pData, uint16 cmdOpCode );
extern hciStatus_t hciCmdParserAdvertiser( uint8 *pData, uint16 cmdOpCode );
extern hciStatus_t hciCmdParserInitiator( uint8 *pData, uint16 cmdOpCode );
extern hciStatus_t hciCmdParserPeripheral( uint8 *pData, uint16 cmdOpCode );
extern hciStatus_t hciCmdParserPeriodicAdv( uint8 *pData, uint16 cmdOpCode );
extern hciStatus_t hciCmdParserPeriodicScan( uint8 *pData, uint16 cmdOpCode );
extern hciStatus_t hciCmdParserChannelSounding( uint8 *pData, uint16 cmdOpCode );
extern hciStatus_t hciCmdParserHost( uint8 *pData, uint16 cmdOpCode );
extern hciStatus_t hciCmdParserVendorSpecificConnection( uint8 *pData, uint16 cmdOpCode );
extern hciStatus_t hciCmdParserVendorSpecificInitiator( uint8 *pData, uint16 cmdOpCode );
extern hciStatus_t hciCmdParserVendorSpecificPeripheral( uint8 *pData, uint16 cmdOpCode );
extern hciStatus_t hciCmdParserVendorSpecificBroadcaster( uint8 *pData, uint16 cmdOpCode );

uint8 MAP_hciCmdParserLegacy( uint8 *pData, uint16 cmdOpCode )
{
#ifdef LEGACY_CMD
  return hciCmdParserLegacy( pData, cmdOpCode );
#else
  return FAILURE;
#endif
}
uint8 MAP_hciCmdParserConnection( uint8 *pData, uint16 cmdOpCode )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  return hciCmdParserConnection( pData, cmdOpCode );
#else
  return FAILURE;
#endif
}
uint8 MAP_hciCmdParserAdvertiser( uint8 *pData, uint16 cmdOpCode )
{
#if defined(CTRL_CONFIG) && ((CTRL_CONFIG & ADV_NCONN_CFG) || (CTRL_CONFIG & ADV_CONN_CFG))
  return hciCmdParserAdvertiser( pData, cmdOpCode );
#else
  return FAILURE;
#endif
}
uint8 MAP_hciCmdParserInitiator( uint8 *pData, uint16 cmdOpCode )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  return hciCmdParserInitiator( pData, cmdOpCode );
#else
  return FAILURE;
#endif
}
uint8 MAP_hciCmdParserPeripheral( uint8 *pData, uint16 cmdOpCode )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  return hciCmdParserPeripheral( pData, cmdOpCode );
#else
  return FAILURE;
#endif
}
uint8 MAP_hciCmdParserPeriodicAdv( uint8 *pData, uint16 cmdOpCode )
{
#ifdef USE_PERIODIC_ADV
  return hciCmdParserPeriodicAdv( pData, cmdOpCode );
#else
  return FAILURE;
#endif
}
uint8 MAP_hciCmdParserPeriodicScan( uint8 *pData, uint16 cmdOpCode )
{
#ifdef USE_PERIODIC_SCAN
  return hciCmdParserPeriodicScan( pData, cmdOpCode );
#else
  return FAILURE;
#endif
}
uint8 MAP_hciCmdParserChannelSounding( uint8 *pData, uint16 cmdOpCode )
{
#ifdef CHANNEL_SOUNDING
  return hciCmdParserChannelSounding( pData, cmdOpCode );
#else
  return FAILURE;
#endif
}
uint8 MAP_hciCmdParserHost( uint8 *pData, uint16 cmdOpCode )
{
#ifdef HOST_CONFIG
  return hciCmdParserHost( pData, cmdOpCode );
#else
  return FAILURE;
#endif
}
uint8 MAP_hciCmdParserVendorSpecificConnection( uint8 *pData, uint16 cmdOpCode )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
  return hciCmdParserVendorSpecificConnection( pData, cmdOpCode );
#else
  return FAILURE;
#endif
}
uint8 MAP_hciCmdParserVendorSpecificInitiator( uint8 *pData, uint16 cmdOpCode )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  return hciCmdParserVendorSpecificInitiator( pData, cmdOpCode );
#else
  return FAILURE;
#endif
}
uint8 MAP_hciCmdParserVendorSpecificPeripheral( uint8 *pData, uint16 cmdOpCode )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  return hciCmdParserVendorSpecificPeripheral( pData, cmdOpCode );
#else
  return FAILURE;
#endif
}
uint8 MAP_hciCmdParserVendorSpecificBroadcaster( uint8 *pData, uint16 cmdOpCode )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_NCONN_CFG)
  return hciCmdParserVendorSpecificBroadcaster( pData, cmdOpCode );
#else
  return FAILURE;
#endif
}

// SID filtering
uint8 MAP_llUpdateSIDFilterScanRsp(uint8 sendReport,uint8 advSid,uint8 advScanState)
{
#ifdef SID_FILTERING
  return llUpdateSIDFilterScanRsp(sendReport,advSid,advScanState);
#else
  return sendReport;
#endif
}

void MAP_llSetSIDFilterScanRsp(void)
{
#ifdef SID_FILTERING
  llSetSIDFilterScanRsp();
#else
  return;
#endif
}

uint32 MAP_llReturnCurrentPeriodicStartTime(void)
{
#ifdef USE_PERIODIC_SCAN
  return llReturnCurrentPeriodicStartTime();
#else
  return 0;
#endif
}

uint32 MAP_llExtAdvTxTime(void * pAdvSet, uint8 primPhy, uint8 secPhy)
{
#ifdef USE_AE
  return llExtAdvTxTime(pAdvSet, primPhy, secPhy);
#else
  return 0;
#endif
}

uint32 MAP_llEstimateAuxOtaTime(void * pAdvSet, uint8 secPhy)
{
#ifdef USE_AE
  return llEstimateAuxOtaTime(pAdvSet, secPhy);
#else
  return 0;
#endif
}

void MAP_LL_rclAdvRxEntryDone( void )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
    LL_rclAdvRxEntryDone();
#endif
}

void MAP_LL_rclAdvTxFinished( void )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
  LL_rclAdvTxFinished();
#endif
}

void MAP_llAdv_TaskConnect( void )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
  llAdv_TaskConnect();
#endif
}

void MAP_LL_rclInitRxEntryDone( void )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  LL_rclInitRxEntryDone();
#endif
}

void MAP_llInit_TaskConnect( void )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  llInit_TaskConnect();
#endif
}

void MAP_llExtInit_ResolveConnRsp( void )
{
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
  llExtInit_ResolveConnRsp();
#endif
}
/*******************************************************************************
 */

/*******************************************************************************
* Synchronous LL Init
*/
void MAP_llInitCompleteNotify(int status)
{
#ifdef BLE_LL_INIT_SYNC
  llInitCompleteNotify(status);
#endif // BLE_LL_INIT_SYNC
}
