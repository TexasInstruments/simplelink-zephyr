/******************************************************************************

 @file  ll_adv_end_causes.c

 @brief This file contains the Link Layer (LL) handlers for the various
        advertising end causes (for Directed, Undirected,
        Discoverable, and Non-connectable advertising events) that result from
        a PHY task completion.

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

#include "../../../../include/ti/bcomdef.h"
#include "../../ll/inc/ble.h"
#include "../../ll/inc/ll.h"
#include "../../ll/inc/ll_ae.h"
#include "../../ll/inc/ll_common.h"
#include "../../ll/inc/ll_config.h"
#include "../../ll/inc/ll_enc.h"
#include "../../ll/inc/ll_privacy.h"
#include "../../ll/inc/ll_rat.h"
#include "../../ll/inc/ll_scheduler.h"
#include "../../ll/inc/ll_timer_drift.h"
#include "hal_gpio_wrapper.h"

#ifdef USE_RCL
#include <ti/drivers/rcl/RCL.h>
#include <ti/drivers/rcl/commands/ble5.h>
#endif
//
#include "rom_jt.h"

// SW Tracer
#ifdef DEBUG_SW_TRACE
#define DBG_ENABLE
#include "dbgid_sys_slv.h"
#endif // DEBUG_SW_TRACE

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */
// number of periodic adv events until channel map apply
#define PERIODIC_ADV_CHANMAP_UPDATE_NUM_EVENTS              7

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
// pointer to next AE set to be scheduled
extern sortedAdv_t *pNextAdvSet;
// number of enabled adv sets
extern uint8 numActiveAdvSets;
#ifdef QUAL_TEST
extern uint8  aeDataUpdatedDuringAdv;
#endif
#endif

#ifdef USE_RCL
extern void LL_rclPeripheralCallback(RCL_Command *cmd, LRF_Events lrfEvents, RCL_Events events);
extern RCL_MultiBuffer *pAdvDataEntry;
#endif
/*******************************************************************************
 * Functions
 */

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
/*******************************************************************************
 * @fn          llAdv_TaskConnect
 *
 * @brief       This function is called when the Adv receives a CONNECT_IND
 *              packet from an Init. The packet is processed, and if there are
 *              no errors, the connection setup procedure is started using the
 *              Peripheral task.
 *
 *              This routine is common for Directed and Undirected Advertising.
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
void llAdv_TaskConnect( void )
{
  llConnState_t *connPtr;
  uint32         timeToNextEvt;
  uint8         *pData;

#ifdef DEBUG_GPIO_CONN
  GPIO_writeDio(HAL_GPIO_1, 0);
#endif // DEBUG_GPIO_CONN

  // check if Adv is still active
  if ( MAP_llGetTaskState(LL_TASK_ID_ADVERTISER) == LL_TASK_STATE_ACTIVE )
  {
#ifdef DEBUG_SW_TRACE
    DBG_PRINT0(DBGSYS, "");
    DBG_PRINT0(DBGSYS, "##########################");
    DBG_PRINT0(DBGSYS, "ADV: CONNECT_IND received!");
    DBG_PRINT0(DBGSYS, "##########################");
    DBG_PRINT0(DBGSYS, "");
#endif // DEBUG_SW_TRACE

    // get current Adv Set
    advSet_t *pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );

    // Sanity Check
    LL_ASSERT( pAdvSet != NULL );

    // disable advertising
    pAdvSet->advMode = LL_ADV_MODE_OFF;

    // free the associated task block as we are done with advertising
    // Note: If the last task, llState will be set to Idle.
    MAP_llFreeTask( &pAdvSet->llTask );

    // get the connections ID
    if ( (connPtr = MAP_llAllocConnId()) == NULL )
    {
      // report failure to Host
      MAP_llHardwareError( HW_FAIL_OUT_OF_MEMORY );

      // determine next task (if any) and schedule it
      MAP_llScheduler();

      return;
    }

    // mark the connection as active
    connPtr->activeConn = TRUE;
    // update health check
    MAP_llHealthUpdate(LL_STATE_CONN_PERIPHERAL);

    // increment the number of active connections
    llConns.numActiveConns++;
    BLE_LOG_INT_INT(0, BLE_LOG_MODULE_CTRL, "CTRL: llAdv_TaskConnect recv CONNECT_IND status=%d, numActiveConns=%d\n", 0, llConns.numActiveConns);

    // save the connection ID for the Host event
    pAdvSet->connId = connPtr->connId;

    // save the Adv Set handle for this connection
    // Note: This is needed because, for multiple Adv Sets, another advertiser
    //       could be scheduled before the OSAL event LL_STATE_PERIPHERAL_CONN_CREATED
    //       is processed.
    aeCurConnHandle = aeCurHandle;

    // get a task block for this BLE state/role
    connPtr->llTask = MAP_llAllocTask( LL_TASK_ID_PERIPHERAL );

    /*
    ** Process the CONNNECT_REQ message parameters.
    **
    ** Note: The Advertiser data queue only contains one entry, and that's for
    **       the CONNECT_IND.
    */

    // check if this will be a legacy advertisement
    if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
    {
#ifdef USE_RCL
      /* Read parameters out of CONNECT_IND message */
      RCL_Buffer_DataEntry *rxEntry = RCL_MultiBuffer_RxEntry_get(&((aeLegacyRf_t *)pAdvSet->pRfCmds)->advParam.rxBuffers, NULL);
      pData = (uint8 *)&rxEntry->data[2];
      // get the timestamp to the start of the CONNECT_IND
      connPtr->llTask->anchorPoint = ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advCmd.connectPktTime +
                                     RAT_TICKS_FOR_CONNECT_IND;
#else
      LL_ASSERT( ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advParam.pRXQ->pCurEntry->status == DATASTAT_FINISHED );
      pData = (uint8 *)((aeLegacyRf_t *)pAdvSet->pRfCmds)->advParam.pRXQ->pCurEntry + sizeof( dataEntry_t );

      LL_ASSERT( ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advOutput.nRxConnReq == 1 );

      // get the timestamp to the start of the CONNECT_IND
      connPtr->llTask->anchorPoint = ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advOutput.timeStamp +
                                     RAT_TICKS_FOR_CONNECT_IND;
#endif
    }
#ifdef USE_AE
    else // !legacy
    {
      LL_ASSERT( ((aeRf_t *)pAdvSet->pRfCmds)->auxRfParam.pRXQ->pCurEntry->status == DATASTAT_FINISHED );
      pData = (uint8 *)((aeRf_t *)pAdvSet->pRfCmds)->auxRfParam.pRXQ->pCurEntry + sizeof( dataEntry_t );

      LL_ASSERT( ((aeRf_t *)pAdvSet->pRfCmds)->comOutput.nRxConnReq == 1 );

      // get the timestamp to the start of the AUX_CONNECT_REQ
      connPtr->llTask->anchorPoint = ((aeRf_t *)pAdvSet->pRfCmds)->comOutput.timeStamp;

      // adjust AP to end of AUX_CONNECT_REQ based on the PHY
      // ALT: Update ll_config to give offset to PHY, based on suffix size.
      //switch( pData[LL_PKT_HDR_LEN+LL_CONNECT_IND_PKT_LEN+llConfigTable.rxPktSuffixPtr->suffixSize-1] )
      switch( pData[LL_PKT_HDR_LEN+LL_CONNECT_IND_PKT_LEN+SUFFIX_RSSI_SIZE+1] )
      {
        case BLE5_1M_PHY:
          connPtr->llTask->anchorPoint += RAT_TICKS_FOR_AUX_CONN_REQ_1M;

          // set max Tx Time and max Remote Tx Time to max uncoded values
          connPtr->lenInfo.connMaxTxTime       = connInitialMaxTxTimeUncoded;
          connPtr->lenInfo.connRemoteMaxTxTime = connInitialMaxTxTimeUncoded;
          break;

        case BLE5_2M_PHY:
          connPtr->llTask->anchorPoint += RAT_TICKS_FOR_AUX_CONN_REQ_2M;

          // set max Tx Time and max Remote Tx Time to max uncoded values
          connPtr->lenInfo.connMaxTxTime       = connInitialMaxTxTimeUncoded;
          connPtr->lenInfo.connRemoteMaxTxTime = connInitialMaxTxTimeUncoded;
          break;

        case BLE5_S8_PHY:
          connPtr->llTask->anchorPoint += RAT_TICKS_FOR_AUX_CONN_REQ_S8;

          // set max Tx Time and max Remote Tx Time to max coded values
          connPtr->lenInfo.connMaxTxTime       = connInitialMaxTxTimeCoded;
          connPtr->lenInfo.connRemoteMaxTxTime = connInitialMaxTxTimeCoded;
          break;

        case BLE5_S2_PHY:
          connPtr->llTask->anchorPoint += RAT_TICKS_FOR_AUX_CONN_REQ_S2;

          // set max Tx Time and max Remote Tx Time to max coded values
          connPtr->lenInfo.connMaxTxTime       = connInitialMaxTxTimeCoded;
          connPtr->lenInfo.connRemoteMaxTxTime = connInitialMaxTxTimeCoded;
          break;

        default:
          connPtr->llTask->anchorPoint += RAT_TICKS_FOR_CONNECT_IND;
          break;
      }
    }
#endif
    // Agama Timestamp Adjustment
    // This magic number is derived from two changes that result from
    // improvements to calibration (based on override settings). The first is
    // the startSynthToRat adjusts from 256us to 166us, resulting in a delay
    // of +90us. The second affects the pilot tone duration, which was reduced
    // from 30us for uncoded and 24 us for coded to 12us for both PHYs. The
    // net increase to the AP is 72us for uncoded, and 78us for coded. However,
    // at this point, it is easier to simply start the AP a bit earlier.
#if defined(CC26X2) || defined(CC13X2) || defined(CC13X4)
    connPtr->llTask->anchorPoint += RAT_TICKS_IN_72US;
#elif defined(CC13X2P)
    // For CC13X2P, the pilot tone duration was returned to 30us.
    connPtr->llTask->anchorPoint += RAT_TICKS_IN_90US;
#endif // CC26X2 || CC13X2 || CC13X4

    // read the initiator's address type for the Host from the header
    connPtr->peerInfo.peerAddrType = (uint8)MASK_ID_ADDRTYPE(pData[0] >> LL_ADV_PDU_HDR_TXADDR);

    // read the initiator's address for the Host
    MAP_osal_memcpy( connPtr->peerInfo.peerAddr, &pData[2], LL_DEVICE_ADDR_LEN );

    //save the own address type
    connPtr->ownAddrType = pAdvSet->actualOwnAddrType;

    // save the connection's access address
    // Note: On the Peripheral, this is needed to support Channel Selection Algo #2.
    MAP_osal_memcpy( (uint8 *)&connPtr->accessAddr, &pData[14], LL_PKT_SYNCH_LEN );

#ifdef USE_RCL
    // read the CRC init value
    MAP_osal_memcpy( (uint8 *)&connPtr->crcInit, &pData[18], LL_PKT_CRC_LEN );
#else
    // set access address in the PHY
    MAP_osal_memcpy( (uint8 *)&linkParam[connPtr->connId].accessAddress, &pData[14], LL_PKT_SYNCH_LEN );

    // read the CRC init value
    MAP_osal_memcpy( (uint8 *)&linkParam[connPtr->connId].crcInit, &pData[18], LL_PKT_CRC_LEN );
#endif

#ifdef LL_TEST_MODE
  switch( llTestMode.testCase )
  {
    case LL_TEST_MODE_TP_ENC_INI_BI_01:
      // override access address to make it invalid
      linkParam[connPtr->connId].accessAddress = ~connPtr->accessAddr;
      break;

    case LL_TEST_MODE_TP_CON_INI_BI_02:
      // override CRC Init to make a packet with invalid CRC
#ifdef USE_RCL
      linkParam[connPtr->connId].crcInit = MAP_llGenerateCRC();
#else
      linkParam[connPtr->connId].crcInit[0] = MAP_LL_ENC_GeneratePseudoRandNum();
      linkParam[connPtr->connId].crcInit[1] = MAP_LL_ENC_GeneratePseudoRandNum();
      linkParam[connPtr->connId].crcInit[2] = MAP_LL_ENC_GeneratePseudoRandNum();
#endif
      break;

    case LL_TEST_MODE_TP_HCI_CM_BV_04:
      // simulate RPA Timeout event
      MAP_osal_set_event( LL_TaskID, LL_EVT_ADDRESS_RESOLUTION_TIMEOUT );

    default:
      break;
  }
#endif // LL_TEST_MODE

    // read the transmit window size
    connPtr->curParam.winSize = pData[21];

    // read the transmit window offset
    MAP_osal_memcpy( (uint8 *)&connPtr->curParam.winOffset, &pData[22], 2 );

    // read the connection interval
    MAP_osal_memcpy( (uint8 *)&connPtr->curParam.connInterval, &pData[24], 2 );

    // save the peripheralLatency value
    MAP_osal_memcpy( (uint8 *)&connPtr->curParam.peripheralLatency, &pData[26], 2 );
    connPtr->peripheralLatencyValue = connPtr->curParam.peripheralLatency;

    // read the supervision connection timeout
    MAP_osal_memcpy( (uint8 *)&connPtr->curParam.connTimeout, &pData[28], 2 );

    // convert connection parameters to 625us units
    connPtr->curParam.winSize      <<= 1;
    connPtr->curParam.winOffset    <<= 1;
    connPtr->curParam.connInterval <<= 1;

    // convert LSTO from units of 10ms to units of 625us
    connPtr->curParam.connTimeout <<= 4;

    // check that the LSTO is valid (i.e. meets the requirements) and that the
    // connection interval isn't zero
    // Note: LSTO > (1 + Peripheral Latency) * (Connection Interval * 2)
    // Note: The CI * 2 requirement based on ESR05 V1.0, Erratum 3904.
    // Note: All times are in 625us.
    // Note: Only checking if CI is zero, which could be missed if LSTO and SL
    //       are also zero.
    if ( ((uint32)connPtr->curParam.connTimeout <=
          ((uint32)(1 + connPtr->peripheralLatencyValue) *
           (uint32)(connPtr->curParam.connInterval << 1))) ||
          (connPtr->curParam.connInterval == 0) )
    {
      // schedule LL Event to notify the Host a connection was formed with
      // a bad parameter
      // Note: This event doesn't take parameters, so it is assumed there that
      //       the reason code was due to an unacceptable connection interval.
      (void)MAP_osal_set_event( LL_TaskID, LL_STATE_PERIPHERAL_CONN_CREATED_BAD_PARAM );

      // it isn't, so terminate
      MAP_llConnTerminate( connPtr, LL_UNACCEPTABLE_CONN_INTERVAL_TERM );

      // determine next task (if any) and schedule it
      MAP_llScheduler();

      return;
    }

    // convert the LSTO from time to an expiration connection event count
    MAP_llConvertLstoToEvent( connPtr, &connPtr->curParam );

    // set the expiration connection event count to a specified limited number
    // Note: This is required in case the Central never sends that first packet.
    connPtr->expirationEvent = LL_LINK_SETUP_TIMEOUT;

    // convert the Control Procedure timeout into connection event count
    MAP_llConvertCtrlProcTimeoutToEvent( connPtr );

    // read the connection channel map
    MAP_osal_memcpy( connPtr->curChanMap.chanMap, &pData[30], LL_NUM_BYTES_FOR_CHAN_MAP );

    // save central sleep clock accuracy (SCA)
    connPtr->sleepClkAccuracy = (uint8)((pData[35] >> 5) & 0x07);

    // combine peripheral SCA with central's SCA and calculate timer drift factor
    // Note: For CC26xx, this value will simply be the sum of the Central and
    //       Peripheral SCA values, in PPM.
    connPtr->scaFactor = MAP_llCalcScaFactor( connPtr->sleepClkAccuracy );

#ifndef DISABLE_RCOSC_SW_FIX
    // save off central contribution
    connPtr->mstSCA = connPtr->scaFactor - pAdvSet->scaValue;
#endif // !DISABLE_RCOSC_SW_FIX

    // save the data channel hop length
    connPtr->hopLength = (uint8)(pData[35] & 0x1F);

    // process connection channel map into the data channel table
    MAP_llProcessChanMap( connPtr, connPtr->curChanMap.chanMap );

    // check if this will be a legacy advertisement
    if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
    {
#ifdef USE_RCL
      RCL_MultiBuffer_clear(pAdvDataEntry);
#else
      ((aeLegacyRf_t *)pAdvSet->pRfCmds)->advParam.pRXQ->pCurEntry->status = DATASTAT_PENDING;
#endif
    }
#ifdef USE_AE
    else // !legacy
    {
      ((aeRf_t *)pAdvSet->pRfCmds)->auxRfParam.pRXQ->pCurEntry->status = DATASTAT_PENDING;
    }
#endif
#ifdef USE_RCL
    /* Initialize connection command and structures */
    linkCmd[connPtr->connId] = RCL_CmdBle5Connection_DefaultRuntime();
    linkParam[connPtr->connId] = RCL_CtxConnection_DefaultRuntime();
    // use common parameters and output
    linkCmd[connPtr->connId].ctx = &linkParam[connPtr->connId];
    linkCmd[connPtr->connId].stats = &connOutput;
#else
    // only one BLE operation command here
    linkCmd[connPtr->connId].rfOpCmd.cmdNum    = CMD_BLE5_PERIPHERAL;
    linkCmd[connPtr->connId].rfOpCmd.status    = RFSTAT_IDLE;
    linkCmd[connPtr->connId].rfOpCmd.pNextRfOp = NULL;
#endif
    // determine the data channel algorithm to use for this connection
#ifdef USE_AE
    if ( !TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
    {
      // for AE, CSA#2 must be supported
      connPtr->pChSelAlgo = MAP_llGetNextDataChanAlgo2;
    }
    else // legacy
#endif
    {
      // check if we support Channel Selection Algorithm #2
      // Note: Connection creation defaults to Channel Selection Algorithm #1.
      if ( deviceFeatureSet.featureSet[1] & (uint8)LL_FEATURE_CHAN_ALGO_2 )
      {
        // channel selection algorithm #2 is supported by our device, but which
        // algorithm we use depends on the peer
#ifdef USE_RCL
        // Take the channel select algo info directly from the connection indication
        connPtr->pChSelAlgo = (LL_ADV_HDR_GET_CHSEL( pData[0] ) == LL_CHANNEL_SELECT_ALGO_1) ?
#else
        connPtr->pChSelAlgo = (taskEndStatus == BLESTAT_DONE_CONNECT_CHSEL0) ?
#endif
                              MAP_llGetNextDataChanAlgo1          :
                              MAP_llGetNextDataChanAlgo2;
      }
    }

    // set channel number and enable BLE whitening
#ifdef USE_RCL
    linkCmd[connPtr->connId].channel = connPtr->pChSelAlgo( connPtr );
    connPtr->currentMappedChan = linkCmd[connPtr->connId].channel;
#else
    linkCmd[connPtr->connId].chan = connPtr->pChSelAlgo( connPtr );
    connPtr->currentMappedChan = linkCmd[connPtr->connId].chan;
    SET_WHITENING_BLE( linkCmd[connPtr->connId].whitening );
#endif

    connPtr->currentChan = connPtr->nextChan;

#ifdef DEBUG_SW_TRACE
    DBG_PRINT0(DBGSYS, "");
    DBG_PRINT1(DBGSYS, "ADV Set Next Chan: %d", linkCmd[connPtr->connId].chan);
    DBG_PRINT0(DBGSYS, "");
#endif // DEBUG_SW_TRACE
    // check if this will not be a legacy advertisement
    if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
    {
      // set PHY mode
      LL_ASSERT( connPtr->phyInfo.curPhy != LL_PHY_NONE );

      // Note: At this point, the curPhy should be properly updated
      RfBleDpl_setPhy(connPtr->connId, connPtr->phyInfo.curPhy, connPtr->phyInfo.phyOpts);
      // set range delay
      llSetRangeDelay(connPtr);

      // set Tx power for this command
      linkCmd[connPtr->connId].txPower = pAdvSet->txPowerIndex;
    }
#ifdef USE_AE
    else // !legacy
    {
      uint8 phyMode = ((aeRf_t *)pAdvSet->pRfCmds)->auxRfCmd.phyMode;

      // copy the PHY and Range Delay information for link
      connPtr->phyInfo.phyOpts =  (phyMode & BLE5_CODED_PHY ) ?
                                  ((phyMode >> 2) ? LL_PHY_OPT_S2 : LL_PHY_OPT_S8)
                                  : LL_PHY_OPT_NONE;
      // save off in terms of current PHY and PHY Opts
      llSetPhy(connPtr,  (phyMode) & BLE5_PHY_MASK);
      // set range delay
      RfBleDpl_setRangeDelay(connPtr->connId, ((aeRf_t *)pAdvSet->pRfCmds)->auxRfCmd.rangeDelay );

      if (!RfBleDpl_txPowerIsValid( ((aeRf_t *)pAdvSet->pRfCmds)->auxRfCmd.txPower))
      {
        // tx power value is invalid, so terminate

        // schedule LL Event to notify the Host a connection was formed with
        // a bad parameter
        (void)MAP_osal_set_event( LL_TaskID, LL_STATE_PERIPHERAL_CONN_CREATED_BAD_PARAM );

        MAP_llConnTerminate( connPtr, LL_UNACCEPTABLE_CONN_INTERVAL_TERM );

        // determine next task (if any) and schedule it
        MAP_llScheduler();

        return;
      }
      llSetPower((uint32 *)&linkCmd[connPtr->connId], curTxPowerVal, ((aeRf_t *)pAdvSet->pRfCmds)->auxRfCmd.txPower);
    }
#endif
#ifdef USE_RCL
    linkParam[connPtr->connId].isPeripheral = TRUE;

    // set access address in the PHY
    linkParam[connPtr->connId].accessAddress = connPtr->accessAddr;
    // set CRC init in the PHY
    linkParam[connPtr->connId].crcInit = connPtr->crcInit;

    // setup the Peripheral Receive Queue
    MAP_llSetupConnRxDataEntryQueue( connPtr->connId );
    // attach data queues to the connection
    txDataQ[connPtr->connId].rfDataBuffers = &linkParam[connPtr->connId].txBuffers;

    connPtr->pTxDataEntryQ = (void *)&txDataQ[connPtr->connId];
    connPtr->pRxDataEntryQ = (void *)&linkParam[connPtr->connId].rxBuffers;

    connOutput = RCL_StatsConnection_DefaultRuntime();
#else
    // use common parameters and output
    linkCmd[connPtr->connId].pParams = (uint8 *)&linkParam[connPtr->connId];
    linkCmd[connPtr->connId].pOutput = (uint8 *)&connOutput;

    // set the Peripheral RX queue configuration
    linkParam[connPtr->connId].rxCfg =
      ( RXQ_CFG_AUTOFLUSH_IGNORED_PKT                                                                      |
        RXQ_CFG_AUTOFLUSH_CRC_ERR_PKT                                                                      |
        RXQ_CFG_AUTOFLUSH_EMPTY_PKT                                                                        |
        RXQ_CFG_INCLUDE_PKT_LEN_BYTE                                                                       |
        ((llConfigTable.rxPktSuffixPtr->suffixSel & SUFFIX_CRC_FLAG)       ? RXQ_CFG_INCLUDE_CRC      : 0) |
        ((llConfigTable.rxPktSuffixPtr->suffixSel & SUFFIX_RSSI_FLAG)      ? RXQ_CFG_APPEND_RSSI      : 0) |
        ((llConfigTable.rxPktSuffixPtr->suffixSel & SUFFIX_STATUS_FLAG)    ? RXQ_CFG_APPEND_STATUS    : 0) |
        ((llConfigTable.rxPktSuffixPtr->suffixSel & SUFFIX_TIMESTAMP_FLAG) ? RXQ_CFG_APPEND_TIMESTAMP : 0) );

    // init connection flow control and enable first packet flag on connection
    linkParam[connPtr->connId].seqStat = SEQ_NUM_CFG_LAST_RX_SN |
                                         SEQ_NUM_CFG_LAST_TX_SN |
                                         SEQ_NUM_CFG_FIRST_PKT;

    // limit the number of NACKS allowed to be received before ending task
    linkParam[connPtr->connId].maxNAck = LL_MAX_NUM_RX_NACKS_ALLOWED;

    // check if one packet per event is enabled
    // Note: Only one Peripheral connection allowed.
    if ( onePktPerEvt == TRUE )
    {
      // set limit for the number of packets to transmit before it ends
      linkParam[connPtr->connId].maxTxPkt = ONE_PKT_PER_EVENT;
    }
    else // one packet per event is disabled
    {
      // so restore configured max number of packets
      linkParam[connPtr->connId].maxTxPkt = llConfigTable.maxPktsPerEvtPtr->maxSlvPktsPerEvt;
    }

    // set max packet length allowed on connection
    // Note: Default to standard size.
    linkParam[connPtr->connId].maxRxPktLen   = LL_MIN_LINK_DATA_LEN +
                                               LL_PKT_MIC_LEN;

    // set max Tx packet length allowed on connection when using LR S=8.
    // Note: A value of zero means "no limit".
    linkParam[connPtr->connId].maxTxLenForLR = 0;

    // setup the Peripheral Receive Queue
    linkParam[connPtr->connId].pRXQ = MAP_llSetupConnRxDataEntryQueue( connPtr->connId );

    // check if the receive ring buffer is properly setup
    if ( linkParam[connPtr->connId].pRXQ == NULL )
    {
      // it isn't, so terminate
      MAP_llConnTerminate( connPtr, LL_STATUS_ERROR_OUT_OF_CONN_RESOURCES );

      // determine next task (if any) and schedule it
      MAP_llScheduler();

      return;
    }

    // setup the Peripheral Transmit Linked List Queue
    // Note: Initialize the static TX data queue.
    MAP_RFHAL_InitDataQueue( (dataEntryQ_t *)&txDataQ[connPtr->connId] );
    linkParam[connPtr->connId].pTXQ = (dataEntryQ_t *)&txDataQ[connPtr->connId];

    // attach data queues to the connection
    connPtr->pTxDataEntryQ = linkParam[connPtr->connId].pTXQ;
    connPtr->pRxDataEntryQ = linkParam[connPtr->connId].pRXQ;

    // Note: Output Parameter Counters are clearned in llScheduleTask!
#endif
    // schedule LL Event to post process
    (void)MAP_osal_set_event( LL_TaskID, LL_STATE_PERIPHERAL_CONN_CREATED );

    // find amount of time to the connection receive window in 625us ticks
    // check if this is a connection from a legacy advertisement
    if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
    {
      timeToNextEvt = (uint32)LL_LINK_MIN_WIN_OFFSET;
    }
#ifdef USE_AE
    else // !legacy
    {
      // check if 1M or 2M
      if ( (linkCmd[connPtr->connId].phyMode & BLE5_PHY_MASK) != BLE5_CODED_PHY )
      {
        timeToNextEvt = (uint32)LL_LINK_MIN_WIN_OFFSET_AE_UNCODED;
      }
      else // Coded
      {
        timeToNextEvt = (uint32)LL_LINK_MIN_WIN_OFFSET_AE_CODED;
      }
    }
#endif
    // add the window offset
    timeToNextEvt += (uint32)connPtr->curParam.winOffset;

#if defined( LL_TEST_MODE )
#ifndef CC23X0
    if ( llTestMode.testCase == LL_TEST_MODE_TP_CON_INI_BV03 )
    {
      // skip the first three connection events (0..2)
      timeToNextEvt += (3 * (uint32)connPtr->curParam.connInterval);
      connPtr->currentChan = (connPtr->currentChan + (connPtr->hopLength * 2)) % LL_MAX_NUM_DATA_CHAN;
      connPtr->currentEvent = 2;
      connPtr->nextEvent    = 3;

      linkCmd[connPtr->connId].chan = connPtr->pChSelAlgo( connPtr );
    }
#endif
#endif // LL_TEST_MODE

    // save previous time to next event for peripheral task end processing
    // Note: Used to avoid unnecessary timer drift calculation.
    connPtr->lastTimeToNextEvt = timeToNextEvt;

    // calculate timer drift correction
    // Note: SCA Factor is in PPM, and time to next event is in 625us ticks. The
    //       result is the timer drift in RAT ticks.
    // Note: Round up one RAT tick to account for floor effect.
    connPtr->timerDrift = ((timeToNextEvt * connPtr->scaFactor) / RAT_TICKS_IN_100US) + 1;

#ifdef DEBUG_SW_TRACE
    DBG_PRINT0(DBGSYS, "");
    DBG_PRINTL1(DBGSYS, "ADV Time to Next = 0x%08X", timeToNextEvt*RAT_TICKS_IN_625US );
    DBG_PRINT1(DBGSYS,  "ADV SCA Factor   = %d", connPtr->scaFactor );
    DBG_PRINTL1(DBGSYS, "ADV Timer Drift  = 0x%08X", connPtr->timerDrift );
    DBG_PRINT0(DBGSYS, "");
#endif // DEBUG_SW_TRACE

#ifdef USE_RCL
    // setup the start time of the receive window
    linkCmd[connPtr->connId].common.timing.absStartTime =
      (connPtr->llTask->anchorPoint + (timeToNextEvt * RAT_TICKS_IN_625US)) -
      (connPtr->timerDrift +
       LL_RX_RAMP_OVERHEAD +
       LL_JITTER_CORRECTION);
    connPtr->llTask->startTime = linkCmd[connPtr->connId].common.timing.absStartTime;
    // set start time trigger
    linkCmd[connPtr->connId].common.scheduling = RCL_Schedule_AbsTime;
    linkCmd[connPtr->connId].common.allowDelay = TRUE;

    // set last Start Time to be same as Start Time in case CT > ST
    // Note: Reason for this is the CT-ST delta is divided by the CI to get the
    //       number of events in order to align to the start of the next
    //       connectino event.
    connPtr->llTask->lastStartTime = linkCmd[connPtr->connId].common.timing.absStartTime;
    // setup the receiver timeout time
    linkCmd[connPtr->connId].relRxTimeoutTime =
                           (2 * connPtr->timerDrift)                                +
                           (2 * LL_JITTER_CORRECTION)                               +
                           LL_RX_RAMP_OVERHEAD                                      +
                           ((uint32)connPtr->curParam.winSize * RAT_TICKS_IN_625US) +
                           LL_RX_SYNCH_OVERHEAD;
    // set last Timeout Time to be same as Timeout Time.
    // Note: The last Timeout Time will be used if we'll miss the first
    //       connection event.
    connPtr->lastTimeoutTime = linkCmd[connPtr->connId].relRxTimeoutTime;
    // setup the connection event End Time relative to the timestamp
    linkCmd[connPtr->connId].common.timing.relHardStopTime =
      (((connPtr->curParam.connInterval * *llConfigTable.connEvtCutoff) / 100) * RAT_TICKS_IN_625US) -
      (2 * RAT_TICKS_IN_150US);

#else
    // setup the start time of the receive window
    // Note: In the case we don't receive a packet at the first connection
    //       event, (and thus, don't have an updated anchor point), this anchor
    //       point will be used for finding the start of the connection event
    //       after that. That is, the update is relative the last valid anchor
    //       point.
    linkCmd[connPtr->connId].rfOpCmd.startTime =
      (connPtr->llTask->anchorPoint + (timeToNextEvt * RAT_TICKS_IN_625US)) -
      (connPtr->timerDrift +
       LL_RX_RAMP_OVERHEAD +
       LL_JITTER_CORRECTION);

    connPtr->llTask->startTime = linkCmd[connPtr->connId].rfOpCmd.startTime;

    // set start time trigger
    CLR_RFOP_ALT_TRIG_CMD( linkCmd[connPtr->connId].rfOpCmd.startTrig );
    CLR_RFOP_PAST_TRIG( linkCmd[connPtr->connId].rfOpCmd.startTrig );
    SET_RFOP_TRIG_TYPE( linkCmd[connPtr->connId].rfOpCmd.startTrig, TRIGTYPE_AT_ABS_TIME );

    // set condition
    SET_RFOP_COND_RULE( linkCmd[connPtr->connId].rfOpCmd.condition, CONDTYPE_NEVER_RUN_NEXT_CMD );

#ifdef DEBUG_SW_TRACE
    DBG_PRINT0(DBGSYS, "");
    DBG_PRINTL1(DBGSYS, "PERIPHERAL Start Time = 0x%08X", linkCmd[connPtr->connId].rfOpCmd.startTime );
    DBG_PRINTL1(DBGSYS, "PERIPHERAL RAT = 0x%08X", MAP_llGetCurrentTime() );
    DBG_PRINT0(DBGSYS, "");
#endif // DEBUG_SW_TRACE

    // set last Start Time to be same as Start Time in case CT > ST
    // Note: Reason for this is the CT-ST delta is divided by the CI to get the
    //       number of events in order to align to the start of the next
    //       connectino event.
    connPtr->llTask->lastStartTime = linkCmd[connPtr->connId].rfOpCmd.startTime;

    // setup the receiver timeout time and trigger
    linkParam[connPtr->connId].timeoutTime =
                           (2 * connPtr->timerDrift)                                +
                           (2 * LL_JITTER_CORRECTION)                               +
                           LL_RX_RAMP_OVERHEAD                                      +
                           ((uint32)connPtr->curParam.winSize * RAT_TICKS_IN_625US) +
                           LL_RX_SYNCH_OVERHEAD;

    // check if we're using coded and adjust backend of Rx window based on PHY
    if ( connPtr->phyInfo.curPhy == LL_PHY_CODED )
    {
      linkParam[connPtr->connId].timeoutTime += LL_RX_SYNCH_OVERHEAD_CODED;
    }

    // set last Timeout Time to be same as Timeout Time.
    // Note: The last Timeout Time will be used if we'll miss the first
    //       connection event.
    connPtr->lastTimeoutTime = linkParam[connPtr->connId].timeoutTime;

    // set timeout trigger
    SET_RFOP_TRIG_TYPE( linkParam[connPtr->connId].timeoutTrig, TRIGTYPE_REL_CMD_START );

    // setup the connection event End Time relative to the timestamp
    // Note: Per the spec, this an be as late as 150us (i.e. T_IFS) before the
    //       next connection event, but we'll provide 2*T_IFS for some extra
    //       margin for post processing. Extended Data could potentially require
    //       us to end the connection event at least 4.54ms before. In any case,
    //       to allow some build time flexibility, the amount of back-off can
    //       be set at build time using llConfig.connEvtCutoff as a percent
    //       of the connection interval wanted.
    linkParam[connPtr->connId].endTime =
      (((connPtr->curParam.connInterval * *llConfigTable.connEvtCutoff) / 100) * RAT_TICKS_IN_625US) -
      (2 * RAT_TICKS_IN_150US);

    // set end trigger
    SET_RFOP_TRIG_TYPE( linkParam[connPtr->connId].endTrig, TRIGTYPE_REL_CMD_START ); //TRIGTYPE_REL_SYNC );
#endif
    // pointer to first radio operation command
    connPtr->llTask->command = (uint32)&linkCmd[connPtr->connId];

#ifdef LL_TEST_MODE
    // Do not initiate the peripheral autonomously feature exchange under test mode
    // and test case is LL/CON/CEN/BV-19. Lower tester should avoid it so link will
    // no be maintained and control transaction timer expires.
    if ( llTestMode.testCase != LL_TEST_MODE_TP_CON_MAS_BV_19 )
#endif // LL_TEST_MODE
    // Do not initiate the peripheral autonomously feature exchange if disabled
    // This is recommended to be disabled for qualification testing
    if (MAP_checkAutoFeatureExchangeStatus())
    {
      // initiate a peripheral feature set control procedure in case it is supported
      if ( connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_SLV_FEATURES_EXCHANGE )
      {
        MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_PERIPHERAL_FEATURE_REQ );
      }
    }

    // set state
    llState = LL_STATE_CONN_PERIPHERAL;
    // make this new connection be the current connection for the scheduler
    llConns.currentConn = connPtr->connId;

    // send the Adv Set End callback, if enabled
    MAP_llSendAdvSetEndEvent( pAdvSet );

    // send the LE Advertisement Set Terminated Event
    MAP_llSendAdvSetTermEvent( pAdvSet,
                               LL_STATUS_SUCCESS,
                               connPtr->connId );

    // callback function for scheduler
    connPtr->llTask->setup = MAP_llLinkSchedSetup;
#ifdef USE_RCL
    // Set callback function and events
    linkCmd[connPtr->connId].common.runtime.callback = LL_rclPeripheralCallback;
    linkCmd[connPtr->connId].common.runtime.lrfCallbackMask.value = LRF_EventTxDone.value |LRF_EventRxOk.value;
    linkCmd[connPtr->connId].common.runtime.rclCallbackMask.value =
                               RCL_EventLastCmdDone.value  |
                               RCL_EventRxEntryAvail.value |
                               RCL_EventTxBufferFinished.value;
#else
    // set RF events
    connPtr->llTask->rfEvents = RF_EventLastCmdDone   |
                                RF_EventInternalError |
                                RF_EventRxEntryDone   |
                                RF_EventTxEntryDone;
#endif
  }

  // determine next task (if any) and schedule it
  MAP_llScheduler();

  return;
}
#endif // ADV_CONN_CFG


#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llExtAdv_PostProcess
 *
 * @brief       This routine is used to post process the Extended Advertising
 *              command.
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
void llExtAdv_PostProcess( void )
{
  uint16    delay;
  advSet_t *pAdvSet;

#ifdef DEBUG_GPIO_CONN
  GPIO_writeDio(HAL_GPIO_1, 0);
#endif // DEBUG_GPIO_CONN

#ifdef DEBUG_GPIO_ADV_SCAN
  GPIO_writeDio(HAL_GPIO_1, 0);
#endif // DEBUG_GPIO_ADV_SCAN

  // get adv set based on currently used handle
  pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );

  // Sanity Check
  LL_ASSERT( pAdvSet != NULL );

  // got pointer
  if ( pAdvSet )
  {
#ifdef USE_RCL
    // clear the scan requests packets
    RCL_MultiBuffer_clear(pAdvDataEntry);
#endif
    // check if still active
    if ( pAdvSet->advMode == LL_ADV_MODE_OFF )
    {
      // terminate the advertiser
      llTermExtAdv(pAdvSet,0);
      // schedule another task, if any
      MAP_llScheduler();

      return;
    }

    // update health check
    MAP_llHealthUpdate(LL_STATE_EXT_ADV);
    // bump the number of events
    pAdvSet->maxAdvEvts++;

    // check if max events is used, and we've reached it
    if ( (pAdvSet->pEnable->maxEvents) &&
         (pAdvSet->maxAdvEvts == pAdvSet->pEnable->maxEvents) )
    {
      // terminate the advertiser
      llTermExtAdv(pAdvSet,LL_STATUS_ERROR_LIMIT_REACHED);
      // schedule another task, if any
      MAP_llScheduler();

      return;
    }

    // clear Tx Counter
    pAdvSet->txCount = 0;

#ifdef CONTROLLER_ONLY
    // Do not access the zeroDelay as it is not part of the BLE SIG HCI command
    // generate random advertising delay from 0..10ms, in units of 250ns ticks
    delay = (uint16)(MAP_LL_ENC_GeneratePseudoRandNum() % 11) * RAT_TICKS_IN_1MS;
#else
    // by default, calculate delay between advertise sets
    if(pAdvSet->pAdvParam->zeroDelay == 0)
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

    uint32_t primIntMinTemp = BUILD_UINT32(pAdvSet->pAdvParam->primIntMin[0],
                                           pAdvSet->pAdvParam->primIntMin[1],
                                           pAdvSet->pAdvParam->primIntMin[2],
                                           0);
    // reset NOP command and set new start time
#ifdef USE_RCL
   ((RCL_Command *)pAdvSet->pRfCmds)->status = RCL_CommandStatus_Idle;
   ((RCL_Command *)pAdvSet->pRfCmds)->timing.absStartTime =
#else
    ((rfOpCmd_t *)pAdvSet->pRfCmds)->startTime =
#endif
      pAdvSet->advStartTime +
      (primIntMinTemp * RAT_TICKS_IN_625US) + delay;

    // save next advertising event start time
#ifdef USE_RCL
    pAdvSet->advStartTime = ((RCL_Command *)pAdvSet->pRfCmds)->timing.absStartTime;
#else
    pAdvSet->advStartTime = ((rfOpCmd_t *)pAdvSet->pRfCmds)->startTime;
#endif

    // check if duration is used, and we've reached it
    if ( (pAdvSet->pEnable->duration) && (pAdvSet->durationExpireTime) &&
         (pAdvSet->advEvtType != LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT) &&
         (pAdvSet->durationExpireTime <= pAdvSet->advStartTime + US_TO_RAT_TICKS(pNextAdvSet->timeConsume)) )
    {
      // terminate the advertiser
      llTermExtAdv(pAdvSet,LL_STATUS_ERROR_DIRECTED_ADV_TIMEOUT);
      // schedule another task, if any
      MAP_llScheduler();

      return;
    }

    // in order to keep the adv sorted list sorted we need to find new position for the adv set
    // that was scheduled and ended.
    if (numActiveAdvSets >= 2)
    {
      // check if the next AE set in its correct position in the AE List.
      if (MAP_llTimeCompare(pNextAdvSet->AdvEntry->advStartTime,pNextAdvSet->next->AdvEntry->advStartTime))
      {
        // make sure the AE List is sorted accordoing to start time.
        MAP_llUpdateSortedAdvList();
      }
    }
    // check if this will be a legacy advertisement
    if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
    {
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
      if ( pAdvSet->advEvtType == LL_ADV_CONNECTABLE_HDC_DIRECTED_EVT )
      {
        // disable advertising
        pAdvSet->advMode = LL_ADV_MODE_OFF;

        if (numActiveAdvSets > 0)
        {
          // free task and teardown privacy if need be
          MAP_llEndExtAdvTask( pAdvSet );
        }

        // notify the host with appropriate reason code
        (void)MAP_osal_set_event( LL_TaskID, LL_EVT_DIRECTED_ADV_FAILED );
      }
      else // all other Adv Event Types
#endif // ADV_CONN_CFG
      {
#ifdef USE_RCL
        ((RCL_Command *)pAdvSet->pRfCmds)->timing.relHardStopTime = RAT_TICKS_IN_1_28S;
#else
        aeLegacyRf_t *pRf = (aeLegacyRf_t *)pAdvSet->pRfCmds;

        // restore the End Trigger and Time for advertising
        // Note: This is needed in case the End Trigger was set to cutoff the Adv
        //       due to a scheduled connection.
        // Note: This is not needed for non-Directed advertising, but it doesn't
        //       hurt to leave this here as only Directed advertising is continuous
        //       (i.e. command chain is in a loop). All other advertising ends when
        //       the command chain ends, and there's no way a non-Directed advertiser
        //       can take 1.28s to complete!
        CLR_RFOP_ALT_TRIG_CMD( pRf->advParam.endTrig );
        SET_RFOP_PAST_TRIG( pRf->advParam.endTrig );
        SET_RFOP_TRIG_TYPE( pRf->advParam.endTrig, TRIGTYPE_REL_FIRST_CHAIN_CMD );
        pRf->advParam.endTime = RAT_TICKS_IN_1_28S;
#endif
      }
    }
#ifdef USE_AE
    else // !legacy
    {
      MAP_llPostProcessExtendedAdv(pAdvSet);
    }
#endif
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
  }

  // schedule this task
  MAP_llScheduler();

  // check if there's a request for a callback for the end of Adv
  if (( pAdvSet != NULL) &&
      ( pAdvSet->pAdvParam->notifyEnableFlags & AE_NOTIFY_ENABLE_ADV_END ))
  {
    MAP_llExtAdvCBack( LL_CBACK_ADV_END, (void *)&aeCurHandle );
  }

  return;
}
#endif

#ifdef USE_PERIODIC_ADV
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llPeriodicAdv_PostProcess
 *
 * @brief       This routine is used to post process the periodic Advertising
 *              command.
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
 * @return      None.
 */
void llPeriodicAdv_PostProcess( void )
{
  llPeriodicAdvSet_t *pPeriodicAdv = llPeriodicAdv.currentAdv;
  uint8 dataUpdated = FALSE;
  uint8 chmapUpdated;

  // check if need to terminate the current periodic adv set
  if (pPeriodicAdv->pendingDisable)
  {
    MAP_llEndPeriodicAdvTask(pPeriodicAdv);
  }
  else if (pPeriodicAdv->state != PERIODIC_ADV_STATE_DISABLE)
  {
    // check for add or remove CTE
    if (pPeriodicAdv->cteInfo.pending == PERIODIC_ADV_CTE_PENDING_ENABLE)
    {
      pPeriodicAdv->cteInfo.enable = TRUE;
      pPeriodicAdv->cteInfo.pending = PERIODIC_ADV_CTE_NO_PENDING;
      dataUpdated = TRUE;
    }
    else if (pPeriodicAdv->cteInfo.pending == PERIODIC_ADV_CTE_PENDING_DISABLE)
    {
      pPeriodicAdv->cteInfo.enable = FALSE;
      pPeriodicAdv->cteInfo.pending = PERIODIC_ADV_CTE_NO_PENDING;
      dataUpdated = TRUE;
    }

    // check if host update the periodic data
    if (pPeriodicAdv->dataUpdated == TRUE)
    {
      llSetPeriodicAdvData(pPeriodicAdv);
      dataUpdated = TRUE;
    }
    // check if host update the channel map
    if (llPeriodicAdv.chanMap.updated)
    {
      switch (pPeriodicAdv->pendingChanUpdate)
      {
        case PERIODIC_ADV_CHANMAP_UPDATE_NOT_PENDING:
          // move the channel map state to pending
          pPeriodicAdv->pendingChanUpdate = PERIODIC_ADV_CHANMAP_UPDATE_PENDING;
          // apply the new channel map in 6 periodic events
          pPeriodicAdv->chanMapUpdateEvent = pPeriodicAdv->eventCounter + PERIODIC_ADV_CHANMAP_UPDATE_NUM_EVENTS;
          pPeriodicAdv->maxAvailData = (AE_MAX_ADV_PAYLOAD_LEN - (EXTHDR_FLAG_CTEINFO_SIZE + EXTHDR_FLAG_AUXPTR_SIZE +
                                        EXTHDR_FLAG_TXPWR_SIZE + EXTHDR_FLAG_ACAD_SIZE + EXTHDR_INFO_SIZE + EXTHDR_FLAGS_SIZE + 1));
          // call this function only to update the frags because the max header size was increased
          llSetPeriodicAdvData(pPeriodicAdv);
          dataUpdated = TRUE;
        break;
        case PERIODIC_ADV_CHANMAP_UPDATE_PENDING:
          if (pPeriodicAdv->chanMapUpdateEvent <= ((pPeriodicAdv->eventCounter + 1) & 0xFFFF))
          {
            // move the channel map state to applied
            pPeriodicAdv->pendingChanUpdate = PERIODIC_ADV_CHANMAP_UPDATE_APPLIED;
            // update the new channel map
            pPeriodicAdv->pChanMap = &llPeriodicAdv.chanMap.next;
            pPeriodicAdv->maxAvailData = (AE_MAX_ADV_PAYLOAD_LEN - (EXTHDR_FLAG_CTEINFO_SIZE + EXTHDR_FLAG_AUXPTR_SIZE +
                                          EXTHDR_FLAG_TXPWR_SIZE + EXTHDR_INFO_SIZE + EXTHDR_FLAGS_SIZE + 1));
            // call this function only to update the frags because the max header size was decreased
            llSetPeriodicAdvData(pPeriodicAdv);
            dataUpdated = TRUE;
          }
        break;
        case PERIODIC_ADV_CHANMAP_UPDATE_APPLIED:
        {
          // check that all periodics updated the channel map
          llPeriodicAdvSet_t *pTmpPeriodicAdv = llPeriodicAdv.advList;

          chmapUpdated = TRUE;
          while ((pTmpPeriodicAdv != NULL) && (chmapUpdated))
          {
            if ((pTmpPeriodicAdv->state == PERIODIC_ADV_STATE_ENABLE) &&
                (pTmpPeriodicAdv->pendingChanUpdate != PERIODIC_ADV_CHANMAP_UPDATE_APPLIED))
            {
              chmapUpdated = FALSE;
              break;
            }
            // advance to next entry
            pTmpPeriodicAdv = pTmpPeriodicAdv->next;
          }
          if (chmapUpdated)
          {
            // all periodics finished the channel map update procedure
            // disable the channel map update flag
            MAP_llSetPeriodicAdvChmapUpdate(FALSE);
            // copy the new channel map to the current
            MAP_osal_memcpy(&llPeriodicAdv.chanMap.current,&llPeriodicAdv.chanMap.next,sizeof(llPeriodicChanMap_t));
            // update all periodics
            pTmpPeriodicAdv = llPeriodicAdv.advList;
            while( pTmpPeriodicAdv != NULL )
            {
              // move status to not pending
              pTmpPeriodicAdv->pendingChanUpdate = PERIODIC_ADV_CHANMAP_UPDATE_NOT_PENDING;
              // update the new channel map
              pPeriodicAdv->pChanMap = &llPeriodicAdv.chanMap.current;
              // advance to next entry
              pTmpPeriodicAdv = pTmpPeriodicAdv->next;
            }
          }
        }
        break;
      }
    }
    // update the missed packets counter
    if (taskEndStatus == BLESTAT_DONE_OK)
    {
      pPeriodicAdv->numMissed = 0;
    }
    else
    {
      pPeriodicAdv->numMissed++;
    }
    pPeriodicAdv->txCount = 0;
    // initialize the command status
    pPeriodicAdv->rfCmd.rfOpCmd.status  = RFSTAT_IDLE;
    // update event counter
    pPeriodicAdv->eventCounter++;
    // find secondary channel index
    pPeriodicAdv->currentChan = llSetNextPeriodicAdvChan( pPeriodicAdv->pChanMap, pPeriodicAdv->syncInfo.accessAddr, pPeriodicAdv->eventCounter );
    // update the next data channel before setup the periodic header (llSetupPeriodicHdr)
    pPeriodicAdv->rfCmd.chan = pPeriodicAdv->currentChan & AE_CHAN_INDEX_MASK;

    // check if need to update the sync indication packet
    if ((dataUpdated) || (pPeriodicAdv->numChains > 1) || (pPeriodicAdv->totalOtaTime == PERIODIC_ADV_MARGIN_TIME_RAT_TICKS))
    {
      pPeriodicAdv->extHdrSize = llSetPeriodicHdrFlags(pPeriodicAdv);

      SET_EXTHDR_LEN( pPeriodicAdv->rfPkt.extHdrInfo,pPeriodicAdv->extHdrSize );
      llSetupPeriodicHdr(pPeriodicAdv);
      pPeriodicAdv->rfPkt.advDataLen = pPeriodicAdv->fragLen;
      pPeriodicAdv->rfPkt.pAdvData = pPeriodicAdv->pData;
      if (pPeriodicAdv->numMissed == 0)
      {
        // calculate the total OTA according to the OTA of the last chain packet
        pPeriodicAdv->totalOtaTime = MAP_llTimeDelta( pPeriodicAdv->rfCmd.rfOpCmd.startTime, pPeriodicAdv->startTime ) +
                                     US_TO_RAT_TICKS(pPeriodicAdv->otaTime) + PERIODIC_ADV_MARGIN_TIME_RAT_TICKS;
      }
      // calculate the OTA of the first chain packet
      pPeriodicAdv->otaTime =  MAP_llOctets2Time( pPeriodicAdv->rfCmd.phyMode & 0x03,      // first two bits only
                                                 (pPeriodicAdv->rfCmd.phyMode>>2) & 0x01, // scheme
                                                 (pPeriodicAdv->extHdrSize + EXTHDR_INFO_SIZE + pPeriodicAdv->fragLen),MIC_NOT_ENABLED );
      pPeriodicAdv->otaTime += (pPeriodicAdv->cteInfo.enable)?(pPeriodicAdv->cteInfo.len * 8):0;

      // calculate number of chunks according to the max value of periodic data or CTE count
      pPeriodicAdv->numChains = MAX(pPeriodicAdv->numFrags,
                                   (pPeriodicAdv->cteInfo.enable)?
                                    pPeriodicAdv->cteInfo.count:0);
      if (pPeriodicAdv->numChains > 1)
      {
        pPeriodicAdv->otaTime += AE_MIN_T_MAFS_IN_US;
      }
    }
    // update the next start time event
    pPeriodicAdv->startTime = pPeriodicAdv->startTime + (pPeriodicAdv->interval * RAT_TICKS_IN_1_25MS );
    pPeriodicAdv->rfCmd.rfOpCmd.startTime = pPeriodicAdv->startTime;
    // initialize the counter command status
    pPeriodicAdv->rfCount.rfOpCmd.status = RFSTAT_IDLE;
    // set the counter based on the number of additional aux packets needed
    // Note: If the numFrags=0, then there is no secondary channel packet, so
    //       this counter will never be used.
    pPeriodicAdv->rfCount.counter = pPeriodicAdv->numChains;
    // update chain start time
    pPeriodicAdv->rfParam.auxPtrTgtTime = pPeriodicAdv->rfCmd.rfOpCmd.startTime +
        US_TO_RAT_TICKS(pPeriodicAdv->otaTime + START_SYNTH_TO_RAT_OFFSET);
    pPeriodicAdv->rfParam.auxPtrTgtType = TRIGTYPE_AT_ABS_TIME;
  }
  // schedule this task
  MAP_llScheduler();
  return;
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG
#endif // USE_PERIODIC_ADV

#ifndef USE_RCL
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llAdv_TaskAbort
 *
 * @brief       This function is used to handle the PHY task done end cause
 *              TASK_ABORT that can result from one of two causes. First, a
 *              command was issued to start a new task while the hardware was
 *              already executing a task. Second, a CMD_SHUTDOWN command
 *              was received while executing a task. Since the former is
 *              controlled by the LL software it will never happen. Therefore,
 *              this handler is only for handling a hardware shutdown.
 *
 *              Note: Issuing a CMD_SHUTDOWN when the hardware is not running
 *                    does not cause this end cause to occur.
 *
 *              Possible reasons for the LL issuing this command are:
 *              - The Host stops advertising.
 *
 *              In all cases, the RX and TX FIFOs are reset.
 *
 *              This routine is common for all Adv events.
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
void llAdv_TaskAbort( void )
{
  advSet_t *pAdvSet = MAP_LL_SearchAdvSet( aeCurHandle );

  // check if Adv is still active and pAdvSet still exist for this handle
  if ( ( MAP_llGetTaskState(LL_TASK_ID_ADVERTISER) == LL_TASK_STATE_ACTIVE ) &&
       ( pAdvSet ) )
  {
    // disable advertising
    pAdvSet->advMode = LL_ADV_MODE_OFF;

    // free the associated task block
    // Note: If the last task, llState will be set to Idle.
    MAP_llFreeTask( &pAdvSet->llTask );
  }

  // determine next task (if any) and schedule it
  MAP_llScheduler();

  return;
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG
#endif

#ifdef USE_AE
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
/*******************************************************************************
 * @fn          llPostProcessExtendedAdv
 *
 * @brief       This routine is used to post process the Extended Advertising
 *              command.
 *
 * input parameters
 *
 * @param       pAdvSet - pointer to advertising set.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llPostProcessExtendedAdv( advSet_t *pAdvSet )
{
  aeRf_t *pRf = (aeRf_t *)pAdvSet->pRfCmds;

  pRf->extRfCmd[0].rfOpCmd.status = RFSTAT_IDLE;
  // set randomly the channel number
  pAdvSet->firstPrimChan = LL_ADV_BASE_CHAN + llGetRandChannelMapIndex(pAdvSet->pAdvParam->primChanMap);
  pRf->extRfCmd[0].chan = pAdvSet->firstPrimChan;
  // set randomly the rest of the channels
  llSetRestPrimaryChannels(pAdvSet);
  // set the Extended Header Info
  pRf->comPkt.extHdrInfo = pAdvSet->extHdrInfo;

  // set the Extended Header Flags
  pRf->comPkt.extHdrFlags = pAdvSet->extHdrFlags;

  // advertisement length and data pointer
  // Note: The ADV_EXT_IND packet never has data.
  pRf->comPkt.advDataLen = 0;
  pRf->comPkt.pAdvData   = NULL;

  // bump the secondary channel counter and find next channel index
  // Note: Must do this before calling llSetupExtHdr!
  pAdvSet->auxChanIndex = MAP_llNextChanIndex( ++pAdvSet->auxChanCounter );

  // Sync info could be only on AUX_ADV_IND in not connactable and not scannable mode
  if ((!TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps)) &&
      (!TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps)))
  {
#ifdef USE_PERIODIC_ADV
    llPeriodicAdvSet_t *pPeriodicAdv = MAP_llGetPeriodicAdv(pAdvSet->pAdvParam->handle);
    if (pPeriodicAdv != NULL)
    {
      if ((pPeriodicAdv->state == PERIODIC_ADV_STATE_ENABLE) && (!TST_EXTHDR_FLAG(pAdvSet->auxHdrFlags, EXTHDR_FLAG_SYNCINFO)))
      {
        SET_EXTHDR_FLAG( pAdvSet->auxHdrFlags,EXTHDR_FLAG_SYNCINFO );
      }
      else if (pPeriodicAdv->state == PERIODIC_ADV_STATE_PENDING_ENABLE)
      {
        // set the sync info flag
        SET_EXTHDR_FLAG( pAdvSet->auxHdrFlags,EXTHDR_FLAG_SYNCINFO );
        // set auxPtr and ADI flags in extHdrFlags
        SET_EXTHDR_FLAG( pAdvSet->extHdrFlags,EXTHDR_FLAG_AUXPTR | EXTHDR_FLAG_ADI );
        MAP_llSetupExtAdv( pAdvSet );
        MAP_llSetupPeriodicAdv(pAdvSet);
      }
      else if ((pPeriodicAdv->state == PERIODIC_ADV_STATE_DISABLE) ||
              (pPeriodicAdv->pendingDisable))
      {
        // clear the sync info flag
        CLR_EXTHDR_FLAG( pAdvSet->auxHdrFlags,EXTHDR_FLAG_SYNCINFO );
        // check if we need to clear the auxPtr and ADI flags in extHdrFlags
        if ((pAdvSet->pAdvData == NULL) && (pAdvSet->pScanRspData == NULL) &&
            (pAdvSet->pAdvParam->primPhy != AE_PHY_CODED_S2) && (pAdvSet->pAdvParam->primPhy != AE_PHY_CODED_S8))
        {
          if (TST_EXTHDR_FLAG(pAdvSet->extHdrFlags, EXTHDR_FLAG_AUXPTR))
          {
            CLR_EXTHDR_FLAG( pAdvSet->extHdrFlags,EXTHDR_FLAG_AUXPTR );
          }
          if (TST_EXTHDR_FLAG(pAdvSet->extHdrFlags, EXTHDR_FLAG_ADI))
          {
            CLR_EXTHDR_FLAG( pAdvSet->extHdrFlags,EXTHDR_FLAG_ADI );
          }
        }
        // update the adv set in order to clear the sync info
        MAP_llSetupExtAdv( pAdvSet );
      }
    }
#endif
  }

#ifdef QUAL_TEST
  // Check that Host updated the data for current adv set during advertising
  if (pAdvSet->pAdvParam->handle == aeDataUpdatedDuringAdv)
  {
    aeDataUpdatedDuringAdv = EXT_DATA_NO_UPDATE_DURING_ADV;
    MAP_llSetupExtAdv( pAdvSet );
  }
  else
#endif
  {
    // initialize the Extended Header Buffer
    MAP_llSetupExtHdr( pAdvSet,
                       pAdvSet->extHdrFlags & ~(EXTHDR_FLAG_ADVA),
                       AE_AUX_OFFSET_AUTO_INSERT );
  }

  // initialize Aux
  pRf->auxRfCmd.rfOpCmd.status  = RFSTAT_IDLE;

  // set the next secondary channel
  pRf->auxRfCmd.chan = pAdvSet->auxChanIndex & AE_CHAN_INDEX_MASK;

  pRf->auxRfCmd.rfOpCmd.startTime = pRf->extRfCmd[0].rfOpCmd.startTime +
                                    US_TO_RAT_TICKS(pAdvSet->otaTimeExtAdv);

  // initialize the status
  pRf->countCmd.rfOpCmd.status = RFSTAT_IDLE;

  // set the counter based on the number of additional aux packets needed
  // Note: If the numFrags=0, then there is no secondary channel packet, so
  //       this counter will never be used.
  pRf->countCmd.counter = pAdvSet->numFrags;

  if ( TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps) )
  {
    // restore the aux otaTimeAuxAdv
    pAdvSet->otaTimeAuxAdv = pAdvSet->otaTimeAuxAdvScan;

    // restore Adv Mode for Scannable
    SET_ADV_MODE( pAdvSet->auxHdrInfo,
                  AE_ADV_MODE_SCANNABLE );

    // set to one in case there's not AUX_SCAN_REQ, to end command chain
    // Note: If there is a AUX_SCAN_REQ, then we'll send a AUX_SCAN_RSP and
    //       get a Tx Done interrupt. The ISR will update the counter to
    //       pAdvSet->numFrags. Yes, I get there's a possible race
    //       condition here.
    // Note: If Scannable, we just send a AUX_ADV_IND, so pAdvSet->numFrags
    //       is always at least one, but can be more if there's fragmented
    //       AUX_SCAN_RSP data.
    pRf->countCmd.counter = 1;
  }

  // determine if an auxPtr is needed
  if ( TST_EXTHDR_FLAG(pAdvSet->auxHdrFlags, EXTHDR_FLAG_AUXPTR) )
  {
    pRf->auxRfParam.auxPtrTgtTime = pRf->auxRfCmd.rfOpCmd.startTime        +
                                    US_TO_RAT_TICKS(pAdvSet->otaTimeAuxAdv +
                                                    START_SYNTH_TO_RAT_OFFSET);
    pRf->auxRfParam.auxPtrTgtType = TRIGTYPE_AT_ABS_TIME;
  }
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG
#endif // USE_AE
/*******************************************************************************
 */
