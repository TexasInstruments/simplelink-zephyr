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

#include "bcomdef.h"
#include "ble.h"
#include "ll.h"
#include "ll_common.h"
#include "ll_scheduler.h"
#include "ll_timer_drift.h"
#include "ll_enc.h"
#include "ll_config.h"
#include "ll_rat.h"
#include "ll_privacy.h"
#include "ll_ae.h"
#include "hal_gpio_wrapper.h"

#include <ti/drivers/rcl/RCL.h>
#include <ti/drivers/rcl/commands/ble5.h>
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
#endif

extern void LL_rclPeripheralCallback(RCL_Command *cmd, LRF_Events lrfEvents, RCL_Events events);
extern RCL_MultiBuffer *pAdvDataEntry;

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
 * @Design:     BLE_LOKI-1470
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
    if ( pAdvSet == NULL )
    {
      // determine next task (if any) and schedule it
      MAP_llScheduler();
      return;
    }

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
    ** Process the CONNECT_IND/AUX_CONNNECT_REQ message parameters.
    */
    RCL_Buffer_DataEntry *rxEntry = RCL_MultiBuffer_RxEntry_get(&((aeRf_t *)pAdvSet->pRfCmds)->advParam.rxBuffers, NULL);
    pData = (uint8 *)&rxEntry->data[ADV_DATA_INDEX];

    // check if this will be a legacy advertisement
    if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
    {
      /* Read parameters out of CONNECT_IND message */
      // get the timestamp to the start of the CONNECT_IND
      connPtr->llTask->anchorPoint = ((aeRf_t *)pAdvSet->pRfCmds)->advCmd.connectPktTime +
                                     RAT_TICKS_FOR_CONNECT_IND;
    }
#ifdef USE_AE
    else // !legacy
    {
      uint16 connReqPhy;

      // get the timestamp to the start of the AUX_CONNECT_REQ
      connPtr->llTask->anchorPoint = ((aeRf_t *)pAdvSet->pRfCmds)->advCmd.connectPktTime;

      // Get the Connection request Phy
      connReqPhy = pData[AUX_CONN_REQ_PHY_INDEX] & BLE5_PHY_MASK;

      // Update the anchor point start time for each AUX_CONN_REQ phy.
      switch( connReqPhy )
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

        // won't get here, for safety enter Coded phy and if it's
        // a wrong Phy the connection will drop.
        default:
            connPtr->llTask->anchorPoint += RAT_TICKS_FOR_AUX_CONN_REQ_S8;
            // set max Tx Time and max Remote Tx Time to max coded values
            connPtr->lenInfo.connMaxTxTime       = connInitialMaxTxTimeCoded;
            connPtr->lenInfo.connRemoteMaxTxTime = connInitialMaxTxTimeCoded;
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

    // read the CRC init value
    MAP_osal_memcpy( (uint8 *)&connPtr->crcInit, &pData[18], LL_PKT_CRC_LEN );

#ifdef LL_TEST_MODE
  switch( llTestMode.testCase )
  {
    case LL_TEST_MODE_TP_ENC_INI_BI_01:
      // override access address to make it invalid
      linkParam[connPtr->connId].accessAddress = ~connPtr->accessAddr;
      break;

    case LL_TEST_MODE_TP_CON_INI_BI_02:
      // override CRC Init to make a packet with invalid CRC
      linkParam[connPtr->connId].crcInit = MAP_llGenerateCRC();
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
      RCL_MultiBuffer_clear(pAdvDataEntry);
    }

    /* Initialize connection command and structures */
    linkCmd[connPtr->connId] = RCL_CmdBle5Connection_DefaultRuntime();
    linkParam[connPtr->connId] = RCL_CtxConnection_DefaultRuntime();
    // use common parameters and output
    linkCmd[connPtr->connId].ctx = &linkParam[connPtr->connId];
    linkCmd[connPtr->connId].stats = &connOutput;
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
        // Take the channel select algo info directly from the connection indication
        connPtr->pChSelAlgo = (LL_ADV_HDR_GET_CHSEL( pData[0] ) == LL_CHANNEL_SELECT_ALGO_1) ?
                              MAP_llGetNextDataChanAlgo1          :
                              MAP_llGetNextDataChanAlgo2;
      }
    }

    // set channel number and enable BLE whitening
    linkCmd[connPtr->connId].channel = connPtr->pChSelAlgo( connPtr );
    connPtr->currentMappedChan = linkCmd[connPtr->connId].channel;
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
      RfBleDpl_setConnPhy(connPtr->connId, connPtr->phyInfo.curPhy, connPtr->phyInfo.phyOpts);
      // set range delay
      llSetRangeDelay(connPtr);

      // set Tx power for this command
      linkCmd[connPtr->connId].txPower = pAdvSet->txPowerIndex;
    }
#ifdef USE_AE
    else // !legacy
    {
      RCL_Command_TxPower txPower;
      txPower = ((aeRf_t *)pAdvSet->pRfCmds)->advCmd.txPower;

      // Configure phyOpts connection Coded type.
      // No need to Set Coded S8, it's the default value.
      if(pAdvSet->pAdvParam->secPhy == AE_PHY_CODED_S2)
      {
        connPtr->phyInfo.phyOpts = BLE5_CODED_S2_DEFAULT;
      }

      uint8 phyMode = (uint8)((aeRf_t *)pAdvSet->pRfCmds)->auxPhyFeature;
      // Convert the secondary Phy to Ble Phy.
      llConvertAePhyToBlePhy(pAdvSet->pAdvParam->secPhy, &phyMode);
      // Update the Phy the connection will use.
      if ((!RfBleDpl_txPowerIsValid( txPower ) ||
              (llSetPhy(connPtr, (uint8)((phyMode) & BLE5_PHY_MASK)) == UFALSE)) == true)
      {
          // tx power or phy value is invalid, so terminate

          // schedule LL Event to notify the Host a connection was formed with
          // a bad parameter
          (void)MAP_osal_set_event( LL_TaskID, LL_STATE_PERIPHERAL_CONN_CREATED_BAD_PARAM );

          MAP_llConnTerminate( connPtr, LL_UNACCEPTABLE_CONN_INTERVAL_TERM );

          // determine next task (if any) and schedule it
          MAP_llScheduler();

          return;
      }
      llSetPower((uint32 *)&linkCmd[connPtr->connId], curTxPowerVal, txPower );
    }
#endif
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

    /* Clear the pTxDataEntryQ */
    llClearTxDataQueue(connPtr->pTxDataEntryQ);

    connPtr->pRxDataEntryQ = (void *)&linkParam[connPtr->connId].rxBuffers;

    connOutput = RCL_StatsConnection_DefaultRuntime();

    // schedule LL Event to post process
    (void)MAP_osal_set_event( LL_TaskID, LL_STATE_PERIPHERAL_CONN_CREATED );

    // find amount of time to the connection receive window in 625us ticks
    // check if this is a connection from a legacy advertisement
    if ( TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
    {
      // Legacy has a constant Transmit Window Delay time
      timeToNextEvt = (uint32)LL_TRANSMIT_WIN_DELAY_LEGACY;
    }
#ifdef USE_AE
    else // !legacy
    {
      uint8 secPhy;
      // Takes the secondary Phy from the Adv Set.
      secPhy = pAdvSet->pAdvParam->secPhy & BLE5_PHY_MASK;

      // Coded and Uncoded has different Transmit Window Delay
      if( secPhy == AE_PHY_CODED )
      {
        timeToNextEvt = (uint32)LL_TRANSMIT_WIN_DELAY_LEGACY_AE_CODED;
      }
      else // Coded
      {
        timeToNextEvt = (uint32)LL_TRANSMIT_WIN_DELAY_AE_UNCODED;
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

    // callback function for scheduler
    connPtr->llTask->setup = MAP_llLinkSchedSetup;
    // Set callback function and events
    linkCmd[connPtr->connId].common.runtime.callback = LL_rclPeripheralCallback;
    linkCmd[connPtr->connId].common.runtime.lrfCallbackMask.value = LRF_EventTxDone.value |LRF_EventRxOk.value;
    linkCmd[connPtr->connId].common.runtime.rclCallbackMask.value =
                               RCL_EventLastCmdDone.value  |
                               RCL_EventRxEntryAvail.value |
                               RCL_EventTxBufferFinished.value;
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
 * @Design:     BLE_LOKI-1453
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
  uint16    delay = 0;
  advSet_t *pAdvSet;

#ifdef USE_AE
  llStatus_t status;
#endif

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
    // clear the scan requests packets
    RCL_MultiBuffer_clear(pAdvDataEntry);
#ifdef USE_AE
    if ( !TST_AE_PROPS_LEGACY(pAdvSet->pAdvParam->eventProps) )
    {
      pAdvSet->firstAdvEvt = 1;

      /* Clear TX queue before preparing the next advertising.
       * Needed for extended only because we constantly changing the
       * data in each txBuffer
       */
      RCL_Buffer_TxBuffer *pDataEntry;

      do
      {
        pDataEntry = RCL_TxBuffer_get(&(((aeRf_t*)pAdvSet->pRfCmds)->advParam.txBuffers));
      } while( pDataEntry!=NULL);
    }
#endif

    // Sync info could be only on AUX_ADV_IND in not connactable and not scannable mode
    if ((!TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps)) &&
        (!TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps)))
    {
#ifdef USE_PERIODIC_ADV
      llPeriodicAdvSet_t *pPeriodicAdv = NULL;
      pPeriodicAdv = MAP_llGetPeriodicAdv(pAdvSet->pAdvParam->handle);

      // Check if there a pending periodic adv set
      if ((pPeriodicAdv != NULL) &&
          (TST_EXTHDR_FLAG(pAdvSet->auxHdrFlags, EXTHDR_FLAG_SYNCINFO)) &&
          (pPeriodicAdv->state == PERIODIC_ADV_STATE_PENDING_TRIGGER))
      {
        // Trigger pending periodic advertisement set
        MAP_llTrigPeriodicAdv(pAdvSet, pPeriodicAdv);
      }
#endif // USE_PERIODIC_ADV
    }

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
//#ifndef CONFIG_SOC_CC2340R5
    delay = (uint16)(MAP_LL_ENC_GeneratePseudoRandNum() % 11) * RAT_TICKS_IN_1MS;
//#endif
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
   ((RCL_Command *)pAdvSet->pRfCmds)->status = RCL_CommandStatus_Idle;
   ((RCL_Command *)pAdvSet->pRfCmds)->timing.absStartTime = pAdvSet->advStartTime +
                                                            (primIntMinTemp * RAT_TICKS_IN_625US) + delay;

    // save next advertising event start time
    pAdvSet->advStartTime = ((RCL_Command *)pAdvSet->pRfCmds)->timing.absStartTime;

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
        ((RCL_Command *)pAdvSet->pRfCmds)->timing.relHardStopTime = RAT_TICKS_IN_1_28S;
      }
    }
#ifdef USE_AE
    else // !legacy
    {
      status = MAP_llPostProcessExtendedAdv(pAdvSet);
      if ( status != LL_STATUS_SUCCESS )
      {
        // Error occurred - the advertising should be stopped

        // Disable the advertising
        pAdvSet->advMode = LL_ADV_MODE_OFF;

        // Remove the task from the tasks list
        if (numActiveAdvSets > 0)
        {
          // free task and teardown privacy if need be
          MAP_llEndExtAdvTask( pAdvSet );
        }

        // Notify the host the advertising stopped
        MAP_llSendAdvSetEndEvent( pAdvSet );

        // Notify the host with the error
        MAP_llHardwareError(HW_FAIL_START_EXT_ADV_ERROR);

        // schedule another task, if any
        MAP_llScheduler();

        return;
      }
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
 * @Design   /ref did_286039104
 * @Design:  BLE_LOKI-1795
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
  uint8 chmapUpdated = FALSE;
  periodicRf_t *pRf = NULL;

  // Check if it is valid, otherwise, nothing to do here
  if (pPeriodicAdv == NULL)
  {
    // Schedule next task
    MAP_llScheduler();

    return;
  }

  /* Clear TX queue before preparing the next periodic advertising.
   * Needed only because we constantly changing the
   * data in each txBuffer
   */
  RCL_Buffer_TxBuffer *pDataEntry;

  do
  {
    pDataEntry = RCL_TxBuffer_get(&((pPeriodicAdv->pRfCmds)->perAdvParam.txBuffers));
  } while( pDataEntry!=NULL);

  // Check if need to terminate the current periodic adv set
  if (pPeriodicAdv->pendingDisable)
  {
    MAP_llEndPeriodicAdvTask(pPeriodicAdv);
  }
  else if (pPeriodicAdv->state != PERIODIC_ADV_STATE_DISABLE)
  {
#ifdef RTLS_CTE
    // Check for add or remove CTE
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
#endif

    // Check if host update the periodic data
    if (pPeriodicAdv->dataUpdated == TRUE)
    {
      llSetPeriodicAdvData(pPeriodicAdv);
      dataUpdated = TRUE;
    }
    // Check if host update the channel map
    if (llPeriodicAdv.chanMap.updated)
    {
      switch (pPeriodicAdv->pendingChanUpdate)
      {
        case PERIODIC_ADV_CHANMAP_UPDATE_NOT_PENDING:
          // Move the channel map state to pending
          pPeriodicAdv->pendingChanUpdate = PERIODIC_ADV_CHANMAP_UPDATE_PENDING;
          // Apply the new channel map in 6 periodic events
          pPeriodicAdv->chanMapUpdateEvent = pPeriodicAdv->eventCounter + PERIODIC_ADV_CHANMAP_UPDATE_NUM_EVENTS;
          pPeriodicAdv->maxAvailData = (AE_MAX_ADV_PAYLOAD_LEN - (EXTHDR_FLAG_CTEINFO_SIZE + EXTHDR_FLAG_AUXPTR_SIZE +
                                        EXTHDR_FLAG_TXPWR_SIZE + EXTHDR_FLAG_ACAD_SIZE + EXTHDR_INFO_SIZE + EXTHDR_FLAGS_SIZE + 1));
          // Call this function only to update the frags because the max header size was increased
          llSetPeriodicAdvData(pPeriodicAdv);
          dataUpdated = TRUE;
        break;
        case PERIODIC_ADV_CHANMAP_UPDATE_PENDING:
          if (pPeriodicAdv->chanMapUpdateEvent <= ((pPeriodicAdv->eventCounter + 1) & 0xFFFF))
          {
            // Move the channel map state to applied
            pPeriodicAdv->pendingChanUpdate = PERIODIC_ADV_CHANMAP_UPDATE_APPLIED;
            // Update the new channel map
            pPeriodicAdv->pChanMap = &llPeriodicAdv.chanMap.next;
            pPeriodicAdv->maxAvailData = (AE_MAX_ADV_PAYLOAD_LEN - (EXTHDR_FLAG_CTEINFO_SIZE + EXTHDR_FLAG_AUXPTR_SIZE +
                                          EXTHDR_FLAG_TXPWR_SIZE + EXTHDR_INFO_SIZE + EXTHDR_FLAGS_SIZE + 1));
            // Call this function only to update the frags because the max header size was decreased
            llSetPeriodicAdvData(pPeriodicAdv);
            dataUpdated = TRUE;
          }
        break;
        case PERIODIC_ADV_CHANMAP_UPDATE_APPLIED:
        {
          // Check that all periodics updated the channel map
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
            // Advance to next entry
            pTmpPeriodicAdv = pTmpPeriodicAdv->next;
          }
          if (chmapUpdated)
          {
            // All periodics finished the channel map update procedure
            // Disable the channel map update flag
            MAP_llSetPeriodicAdvChmapUpdate(FALSE);
            // Copy the new channel map to the current
            MAP_osal_memcpy(&llPeriodicAdv.chanMap.current,&llPeriodicAdv.chanMap.next,sizeof(llPeriodicChanMap_t));
            // Update all periodics
            pTmpPeriodicAdv = llPeriodicAdv.advList;
            while( pTmpPeriodicAdv != NULL )
            {
              // Move status to not pending
              pTmpPeriodicAdv->pendingChanUpdate = PERIODIC_ADV_CHANMAP_UPDATE_NOT_PENDING;
              // Update the new channel map
              pPeriodicAdv->pChanMap = &llPeriodicAdv.chanMap.current;
              // Advance to next entry
              pTmpPeriodicAdv = pTmpPeriodicAdv->next;
            }
          }
        }
        break;
      }
    }

    // Pointer to the RCL command
    pRf = pPeriodicAdv->pRfCmds;

    // Update the missed packets counter
    if ( pRf->perAdvCmd.common.status == RCL_CommandStatus_Finished )
    {
      pPeriodicAdv->numMissed = 0;
    }
    else
    {
      pPeriodicAdv->numMissed++;
    }

    // Clear Tx Counter
    pPeriodicAdv->txCount = 0;
    // Update event counter
    pPeriodicAdv->eventCounter++;
    // Find secondary channel index
    pPeriodicAdv->currentChan = llSetNextPeriodicAdvChan( pPeriodicAdv->pChanMap, pPeriodicAdv->syncInfo.accessAddr, pPeriodicAdv->eventCounter );
    // Update the next data channel before setup the periodic header (llSetupPeriodicHdr)
    pRf->perAdvCmd.channel = pPeriodicAdv->currentChan & AE_CHAN_INDEX_MASK;

    // Check if need to update the sync indication packet
    if ((dataUpdated) || (pPeriodicAdv->numChains > 1) || (pPeriodicAdv->totalOtaTime == PERIODIC_ADV_MARGIN_TIME_RAT_TICKS))
    {
      pPeriodicAdv->extHdrSize = llSetPeriodicHdrFlags(pPeriodicAdv);

      SET_EXTHDR_LEN( pRf->comPkt.extHdrInfo ,pPeriodicAdv->extHdrSize );
      llSetupPeriodicHdr(pPeriodicAdv);
      pRf->comPkt.advDataLen = pPeriodicAdv->fragLen;
      pRf->comPkt.pAdvData = pPeriodicAdv->pData;
      if (pPeriodicAdv->numMissed == 0)
      {
        // Calculate the total OTA according to the OTA of the last chain packet
        pPeriodicAdv->totalOtaTime = MAP_llTimeDelta( pRf->perAdvCmd.common.timing.absStartTime, pPeriodicAdv->startTime ) +
                                                      US_TO_RAT_TICKS(pPeriodicAdv->otaTime) + PERIODIC_ADV_MARGIN_TIME_RAT_TICKS;
      }
      // Calculate the OTA of the first chain packet
      pPeriodicAdv->otaTime =  MAP_llOctets2Time( pRf->phyFeatures & 0x03,      // first two bits only
                                                 (pRf->phyFeatures>>2) & 0x01, // scheme
                                                 (pPeriodicAdv->extHdrSize + EXTHDR_INFO_SIZE + pPeriodicAdv->fragLen),MIC_NOT_ENABLED );
#ifdef RTLS_CTE
      pPeriodicAdv->otaTime += (pPeriodicAdv->cteInfo.enable)?(pPeriodicAdv->cteInfo.len * 8):0;

      // Calculate number of chunks according to the max value of periodic data or CTE count
      pPeriodicAdv->numChains = MAX(pPeriodicAdv->numFrags,
                                   (pPeriodicAdv->cteInfo.enable)?
                                    pPeriodicAdv->cteInfo.count:0);
#else
      pPeriodicAdv->numChains = pPeriodicAdv->numFrags;
#endif //RTLS_CTE

      if (pPeriodicAdv->numChains > 1)
      {
        pPeriodicAdv->otaTime += AE_MIN_T_MAFS_IN_US;
      }
    }

    // Update the next start time event
    pPeriodicAdv->startTime = pPeriodicAdv->startTime + (pPeriodicAdv->interval * RAT_TICKS_IN_1_25MS );
    pRf->perAdvCmd.common.timing.absStartTime = pPeriodicAdv->startTime;

    // Build AUX_SYNC_IND packet and add it to the command TX queue
    pPeriodicAdv->extHdrSize = MAP_llGetExtHdrLen(pRf->comPkt.extHdrFlags);
    uint8_t payloadLen = 1 + pPeriodicAdv->extHdrSize + pRf->comPkt.advDataLen;
    pRf->buffNo = 0;
    MAP_llAddPeriodicAdvPacketToTx( pPeriodicAdv, LL_PKT_TYPE_AUX_SYNC_IND, payloadLen );

    // Update the chain packet if there is chain packet
    if (pPeriodicAdv->numChains > 1)
    {
      MAP_llUpdatePeriodicAdvChainPacket( pPeriodicAdv );
    }
  }
  // Schedule this task
  MAP_llScheduler();

  return;
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG
#endif // USE_PERIODIC_ADV

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
llStatus_t llPostProcessExtendedAdv( advSet_t *pAdvSet )
{
  aeRf_t *pRf = (aeRf_t *)pAdvSet->pRfCmds;
  llStatus_t status = LL_STATUS_SUCCESS;

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
        MAP_llSetupExtHdr( pAdvSet, pAdvSet->auxHdrFlags, AE_AUX_OFFSET_AUTO_INSERT );
      }
      else if (pPeriodicAdv->state == PERIODIC_ADV_STATE_PENDING_ENABLE)
      {
        // Set the sync info flag
        SET_EXTHDR_FLAG( pAdvSet->auxHdrFlags,EXTHDR_FLAG_SYNCINFO );
        // Set auxPtr and ADI flags in extHdrFlags
        SET_EXTHDR_FLAG( pAdvSet->extHdrFlags,EXTHDR_FLAG_AUXPTR | EXTHDR_FLAG_ADI );
        // Update the adv set in order to add the sync info
        MAP_llSetupExtHdr( pAdvSet, pAdvSet->auxHdrFlags, AE_AUX_OFFSET_AUTO_INSERT );
        MAP_llSetupPeriodicAdv(pAdvSet);
      }
      else if ((pPeriodicAdv->state == PERIODIC_ADV_STATE_DISABLE) ||
              (pPeriodicAdv->pendingDisable))
      {
        // Clear the sync info flag
        CLR_EXTHDR_FLAG( pAdvSet->auxHdrFlags,EXTHDR_FLAG_SYNCINFO );

        // Check if we need to clear the auxPtr and ADI flags in extHdrFlags
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

        // Update the adv set in order to clear the sync info
        MAP_llSetupExtHdr( pAdvSet, pAdvSet->auxHdrFlags, AE_AUX_OFFSET_AUTO_INSERT );
      }
    }
#endif
  }

  // If there is a pending data update to the ext adv or scan response
  switch (pAdvSet->pendingDataUpdate)
  {
    case LE_AE_EXT_DATA_NO_PENDING:
    {
      // There is no data pending, break
      break;
    }

    case LE_AE_EXT_DATA_ADV_PENDING:
    {
      // Call the AE set data function to update the pointer with the new data
      status = LE_AE_SetData(pAdvSet->pPendingData, LE_AE_EXT_DATA_CMD_ADV_LAST_CMD_DONE);
      if (status != LL_STATUS_SUCCESS)
      {
        // AE set data failed, do not proceed with the update data process
        break;
      }

      // Set pPendingAdvData to NULL
      pAdvSet->pPendingData = NULL;

      // Set the pending update flag to no pending
      pAdvSet->pendingDataUpdate = LE_AE_EXT_DATA_NO_PENDING;

      break;
    }

    case LE_AE_EXT_DATA_SCAN_RSP_PENDING:
    {
      // Call the set data to update the pointer with the new data
      status = LE_AE_SetData(pAdvSet->pPendingData, LE_AE_EXT_DATA_CMD_SCAN_LAST_CMD_DONE);
      if (status != LL_STATUS_SUCCESS)
      {
        // Set data failed, do not proceed with the update data process
        break;
      }

      // Set pPendingScanRspData to NULL
      pAdvSet->pPendingData = NULL;

      // Set the pending update flag to no pending
      pAdvSet->pendingDataUpdate = LE_AE_EXT_DATA_NO_PENDING;

      break;
    }
    default:
        break;
  }

  // initialize the Extended Header Buffer
  MAP_llSetupExtHdr( pAdvSet,
                      pAdvSet->extHdrFlags,
                      AE_AUX_OFFSET_AUTO_INSERT );

  if ( TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps) )
  {
    // restore the aux otaTimeAuxAdv
    // For RCL otaTimeAuxAdvScan is not used. This is preparation before adding
    // scannable advertising
    pAdvSet->otaTimeAuxAdv = pAdvSet->otaTimeAuxAdvScan;

    // restore Adv Mode for Scannable
    SET_ADV_MODE( pAdvSet->auxHdrInfo,
                  AE_ADV_MODE_SCANNABLE );
  }

  // Build EXT_ADV_IND packet and add it to the command TX queue
  pAdvSet->extHdrSize = MAP_llGetExtHdrLen(pAdvSet->extHdrFlags);
  uint8_t payloadLen = 1 + pAdvSet->extHdrSize;
  pRf->buffNo = 0;
  status = MAP_llAddExtAdvPacketToTx(pAdvSet, LL_PKT_TYPE_ADV_EXT_IND, payloadLen);

  // If AUX pointer included, build the AUX pointer Packet
  if ( TST_EXTHDR_FLAG(pAdvSet->extHdrFlags, EXTHDR_FLAG_AUXPTR) )
  {
    // determine if there is any data, and if so, how many fragments
    MAP_llSetupExtData( pAdvSet );

    uint8 pktSize = 0;

    // AUX_ADV_IND pkt in scannable mode
    if( TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps) )
    {
      // AUX_ADV_IND pkt is not allowed to send advData in scannable mode
      pRf->comPkt.advDataLen = 0U;
      pRf->comPkt.pAdvData = NULL;

      // No Aux ptr or syncinfo in scannable AUX_ADV_IN pkt
      CLR_EXTHDR_FLAG(pAdvSet->auxHdrFlags, EXTHDR_FLAG_AUXPTR);
      CLR_EXTHDR_FLAG(pAdvSet->auxHdrFlags, EXTHDR_FLAG_SYNCINFO);
    }
    else if(TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps) != 0)
    {
      // AUX_ADV_IND pkt in connectable mode
      pRf->comPkt.advDataLen = (uint8)pAdvSet->dataLen;
      if(pAdvSet->dataLen > 0U)
      {
        pRf->comPkt.pAdvData = pAdvSet->pData;
      }
      else
      {
        pRf->comPkt.pAdvData = NULL;
      }
      pktSize += (uint8)pAdvSet->dataLen;
      // No aux ptr is allowed in connectable mode
      CLR_EXTHDR_FLAG(pAdvSet->auxHdrFlags, EXTHDR_FLAG_AUXPTR);
    }
    else  // NC/NS
    {
      pRf->comPkt.advDataLen = (pAdvSet->dataLen) ? pAdvSet->fragLen : 0U;
      pRf->comPkt.pAdvData   = (pAdvSet->dataLen) ? pAdvSet->pData : NULL;
      pktSize += pAdvSet->fragLen;
    }

    // Setup EXT_ADV_IND extended header flags
    pRf->comPkt.extHdrFlags = pAdvSet->auxHdrFlags;

    // set AL, if used
    if ( TST_AE_PROPS_DIR(pAdvSet->pAdvParam->eventProps) )
    {
      MAP_osal_memcpy((uint8 *)pRf->advParam.peerA, pAdvSet->peerAddr, B_ADDR_LEN);
    }

    // determine size of Extended Header buffer
    pAdvSet->auxExtHdrSize = MAP_llGetExtHdrLen( pAdvSet->auxHdrFlags );

    // Build AUX_ADV_IND packet and add it to the command TX queue
    MAP_llSetupExtHdr(pAdvSet, pAdvSet->auxHdrFlags, 0 );

    pktSize += pAdvSet->auxExtHdrSize+ 1U;
    payloadLen = pktSize;
    status = MAP_llAddExtAdvPacketToTx(pAdvSet, LL_PKT_TYPE_AUX_ADV_IND, payloadLen);
  }

  /*
  ** Setup AUX_SCAN_RSP packet when scannable mode is in use
  */

  // When using Scannable mode, RCL expect that all the first 3 PDUs will
  // be inserted on command start include AUX_SCAN_RSP
  if ( TST_AE_PROPS_SCAN(pAdvSet->pAdvParam->eventProps))
  {
      uint8 scanRspExtHdrSize;
      if(pAdvSet->numFrags > 1)
      {
        SET_EXTHDR_FLAG(pRf->comPkt.extHdrFlags, EXTHDR_FLAG_AUXPTR);
      }
      // Tx power is optional. add Tx Power if asked by App
      if(TST_AE_PROPS_TX_PWR(pAdvSet->pAdvParam->eventProps) == UTRUE)
      {
        SET_EXTHDR_FLAG(pRf->comPkt.extHdrFlags, EXTHDR_FLAG_TXPWR);
      }
      // Sent TARGETA in aux_scan_rsp packet is not allowed
      // in scannable directed / undirected
      CLR_EXTHDR_FLAG(pRf->comPkt.extHdrFlags, EXTHDR_FLAG_TARGETA);

      // Adding scan response data
      pRf->comPkt.advDataLen = (pAdvSet->dataLen) ? pAdvSet->fragLen : 0U;
      pRf->comPkt.pAdvData   = (pAdvSet->dataLen) ? pAdvSet->pData : NULL;

      // Build the Extended header, RCL deal with Aux pointer values
      MAP_llSetupExtHdr( pAdvSet,
                         pRf->comPkt.extHdrFlags ,
                         AE_AUX_OFFSET_AUTO_INSERT );

      // Get ExtHdrLen and calculate the payload
      scanRspExtHdrSize = MAP_llGetExtHdrLen( pRf->comPkt.extHdrFlags );
      payloadLen = 1U + scanRspExtHdrSize + pRf->comPkt.advDataLen;

      // AUX_SCAN_RSP pkt in need to be sent as NC/NS mode
      SET_ADV_MODE( pRf->comPkt.extHdrInfo,
                    AE_ADV_MODE_NONCONN_NONSCAN );
      // Build ae packet and add it to the command TX queue
      status = MAP_llAddExtAdvPacketToTx(pAdvSet, LL_PKT_TYPE_AUX_SCAN_RSP, payloadLen);
      // Change adv mode back to scannable
      SET_ADV_MODE( pRf->comPkt.extHdrInfo,
                    AE_ADV_MODE_SCANNABLE );
  }

  // Build AUX_CONNECT_RSP pkt
  if ( TST_AE_PROPS_CONN(pAdvSet->pAdvParam->eventProps) != 0 )
  {
    uint8 connRspExtHdrSize;

    // No data allowed
    pRf->comPkt.advDataLen = 0;
    pRf->comPkt.pAdvData   = NULL;

    // Only TargetA and AdvA flags are mandatory and any other
    // flag is forbidden
    pRf->comPkt.extHdrFlags = 0;
    SET_EXTHDR_FLAG(pRf->comPkt.extHdrFlags, EXTHDR_FLAG_TARGETA);
    SET_EXTHDR_FLAG(pRf->comPkt.extHdrFlags, EXTHDR_FLAG_ADVA);
    MAP_llSetupExtHdr( pAdvSet,
                       pRf->comPkt.extHdrFlags ,
                       0 );

    // Get ExtHdrLen and calculate the payload
    connRspExtHdrSize = MAP_llGetExtHdrLen( pRf->comPkt.extHdrFlags );
    payloadLen = 1U + connRspExtHdrSize + pRf->comPkt.advDataLen;

    // AUX_CONN_RSP pkt is needed to be sent as NC/NS mode
    SET_ADV_MODE( pRf->comPkt.extHdrInfo,
                  AE_ADV_MODE_NONCONN_NONSCAN );
    // Build ae packet and add it to the command TX queue
    status = MAP_llAddExtAdvPacketToTx(pAdvSet, LL_PKT_TYPE_AUX_CONNECT_RSP, payloadLen);
    // Change adv_mode back to connectable
    SET_ADV_MODE( pRf->comPkt.extHdrInfo,
                  AE_ADV_MODE_CONNECTABLE );
  }

  return status;
}
#endif // ADV_NCONN_CFG | ADV_CONN_CFG
#endif // USE_AE
/*******************************************************************************
 */
