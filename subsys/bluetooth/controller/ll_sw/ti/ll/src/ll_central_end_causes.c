/******************************************************************************

 @file  ll_central_end_causes.c

 @brief This file contains the Link Layer (LL) handlers for the various central
        end causes that result from a PHY task completion. The file also
        contains the central end causes related to connection termination, as
        well as common termination routines used by the central central central
        central.

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

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)

#include "hal_mcu.h"
#include "osal_cbtimer.h"
#include "ble.h"
#include "ll.h"
#include "ll_common.h"
#include "ll_scheduler.h"
#include "ll_enc.h"
#include "ll_rat.h"
#include "ll_config.h"
#include "hal_gpio_wrapper.h"
#include "ll_ae.h"
//
#include "rom_jt.h"

// SW Tracer
#ifdef DEBUG_SW_TRACE
#define DBG_ENABLE
#include "dbgid_sys_mst.h"
#endif // DEBUG_SW_TRACE

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

/*******************************************************************************
 * Prototypes
 */

/*******************************************************************************
 * Functions
 */

/*******************************************************************************
 * @fn          llCentral_TaskEnd
 *
 * @brief       This function is used to handle the PHY task done end cause
 *              TASK_ENDOK that can result from one of three causes. First, a
 *              a packet was successfully received with MD=0 (i.e. no more Peripheral
 *              data) after having transmitted a packet with MD=0. Second, a
 *              received packet did not fit in the RX FIFO after transmitting
 *              a packet with MD=0. Third, a packet was received from the Peripheral
 *              while BLE_L_CONF.ENDC is true or after Timer 2 Event 2 occurs.
 *
 *              Note: The TASK_ENDOK end cause will also handle the TASK_NOSYNC,
 *                    TASK_RXERR, and TASK_MAXNACK end causes as well.
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
void llCentral_TaskEnd( void )
{
  llConnState_t *connPtr;
  uint16         numPkts;
  uint8          connEvtStatus;
  uint8          channel;

#ifdef DEBUG_GPIO_CONN
  GPIO_writeDio(HAL_GPIO_3, 0);
#endif // DEBUG_GPIO_CONN

#if DEBUG
#ifdef DEBUG_SW_TRACE
  DBG_PRINT0(DBGSYS, "");
  DBG_PRINTL1(DBGSYS, "CENTRAL END RAT = 0x%08X", MAP_llGetCurrentTime() );
  DBG_PRINT0(DBGSYS, "");
#endif // DEBUG_SW_TRACE
#endif // DEBUG

  // check if the connection is still valid
  if ( llConns.currentConn == LL_INVALID_CONNECTION_ID )
  {
    // connection may have already been ended by a reset
    return;
  }

  // get connection information
  connPtr = MAP_llDataGetConnPtr( llConns.currentConn );

  // check if the connection has already been terminated
  // ALT: Halt the radio, then issue terminate immediately.
  if ( connPtr->termInfo.termIndRcvd == TRUE )
  {
    MAP_llConnTerminate( connPtr, connPtr->termInfo.reason );

    return;
  }

  // check if the user wants to be notified that a connection event ended
  // Note: An event of zero means this API is not enabled.
  if ( connPtr->taskEvent != 0 )
  {
    // set the user's event on the user's task
    // Note: There is no sanity checking of valid task ID or event flag!
    MAP_osal_set_event( connPtr->taskID, connPtr->taskEvent );
  }
  // update health check
  MAP_llHealthUpdate(LL_STATE_CONN_CENTRAL);

  // advance the connection event count
  connPtr->currentEvent = connPtr->nextEvent;

#ifdef LL_TEST_MODE
  switch( llTestMode.testCase )
  {
    case LL_TEST_MODE_TP_CON_SLA_BI_02:
      if ( connPtr->currentEvent > 10 )
      {
#ifdef USE_RCL
          llClearRxDataEntry(&rxDataQ.multiBuffers, &rxDataQ.finishedBuffers);
          // Align the global Rx buffer list
          llUpdateRxBuffersForActiveConnections(&rxDataQ.multiBuffers);
#else
        for (uint8 i=0; i<NUM_RX_DATA_ENTRIES; i++)
        {
          rxRingBuf[i].dataEntry.status = DATASTAT_FINISHED;
        }
#endif // USE_RCL
      }
      break;

    case LL_TEST_MODE_JIRA_3646:
      if ( connPtr->currentEvent == 65528 )
      {
        uint8 chanMap[] = { 0xFF, 0x00, 0xFF, 0x00, 0x1F };

        MAP_LL_ChanMapUpdate( chanMap , maxNumConns );
      }
      break;

    default:
      break;
  }
#endif // LL_TEST_MODE

  // check if an pending update parameters has been applied
  if ( connPtr->pendingParamUpdate == PARAM_UPDATE_APPLIED )
  {
    // notify the Host if connInterval, connTimeout, or peripheralLatency
    // has been changed
    if ( connPtr->paramUpdateNotifyHost )
    {
      // notify the Host
      // Note: The values are kept in units of 625us, so they must be
      //       converted back to what's used at the interface.
      MAP_LL_ConnParamUpdateCback( LL_STATUS_SUCCESS,
                                   (uint16)connPtr->connId,
                                   connPtr->curParam.connInterval >> 1,
                                   connPtr->curParam.peripheralLatency,
                                   connPtr->curParam.connTimeout >> 4 );

      connPtr->paramUpdateNotifyHost = FALSE;
      connPtr->procInitiator = FALSE;
    }

    // updated parameters now ratified
    connPtr->pendingParamUpdate = PARAM_UPDATE_NOT_PENDING;
  }

  // check if a pending update phy has been applied
  if ( connPtr->pendingPhyUpdate == PHY_UPDATE_APPLIED )
  {
    // updated phy now ratified
    connPtr->pendingPhyUpdate = FALSE;
  }
  // check if a pending update channel has been applied
  if ( connPtr->pendingChanUpdate == CHANNEL_MAP_UPDATE_APPLIED )
  {
    // updated channel now ratified
    connPtr->pendingChanUpdate = FALSE;
  }

  // get the total number of received packets
  // Note: Since Auto-Flush is enabled, nRxBufFull is incremented instead of
  //       nRxOk when there's no room in the FIFO. When Auto-Flush is
  //       disabled and there's no room in the FIFO, only nRxBufFull is
  //       incremented for any kind of received packet.
#ifndef USE_RCL
  numPkts = ( connOutput.nRxOk      +
              connOutput.nRxNok     +
              connOutput.nRxEmpty   +
              connOutput.nRxIgn     +
              connOutput.nRxBufFull );

  channel = linkCmd[connPtr->connId].chan;

  // collect RX statistics
  connPtr->rxStats.numRxOk         += connOutput.nRxOk;
  connPtr->rxStats.numRxCtrl       += connOutput.nRxCtrl;
  connPtr->rxStats.numRxCtrlAck    += connOutput.nRxCtrlAck;
  connPtr->rxStats.numRxCrcErr     += connOutput.nRxNok;
  connPtr->rxStats.numRxIgnored    += connOutput.nRxIgn;
  connPtr->rxStats.numRxEmpty      += connOutput.nRxEmpty;
  connPtr->rxStats.numRxBufFull    += connOutput.nRxBufFull;

  // collect TX statistics
  connPtr->txStats.numTx           += connOutput.nTx;
  connPtr->txStats.numTxAck        += connOutput.nTxAck;
  connPtr->txStats.numTxCtrl       += connOutput.nTxCtrl;
  connPtr->txStats.numTxCtrlAck    += connOutput.nTxCtrlAck;
  connPtr->txStats.numTxCtrlAckAck += connOutput.nTxCtrlAckAck;
  connPtr->txStats.numTxRetrans    += connOutput.nTxRetrans;
  connPtr->txStats.numTxEntryDone  += connOutput.nTxEntryDone;
#else
  numPkts = ( connOutput.nRxOk      +
              connOutput.nRxNok     +
              connOutput.nRxEmpty   +
              connOutput.nRxIgnored +
              connOutput.nRxFifoFull );

  channel = linkCmd[connPtr->connId].channel;

  // collect RX statistics
  connPtr->rxStats.numRxOk         += connOutput.nRxOk;
  connPtr->rxStats.numRxCtrl       += connOutput.nRxCtl;
  connPtr->rxStats.numRxCtrlAck    += connOutput.nRxCtlAck;
  connPtr->rxStats.numRxCrcErr     += connOutput.nRxNok;
  connPtr->rxStats.numRxIgnored    += connOutput.nRxIgnored;
  connPtr->rxStats.numRxEmpty      += connOutput.nRxEmpty;
  connPtr->rxStats.numRxBufFull    += connOutput.nRxFifoFull;

  // collect TX statistics
  connPtr->txStats.numTx           += connOutput.nTx;
  connPtr->txStats.numTxAck        += connOutput.nTxAck;
  connPtr->txStats.numTxCtrl       += connOutput.nTxCtl;
  connPtr->txStats.numTxCtrlAck    += connOutput.nTxCtlAck;
  connPtr->txStats.numTxRetrans    += connOutput.nTxRetrans;
  connPtr->txStats.numTxEntryDone  += connOutput.nTxDone;
#endif
  // collect packet error information
  connPtr->perInfo.numEvents++;
  connPtr->perInfo.numPkts   += numPkts;
  connPtr->perInfo.numCrcErr += connOutput.nRxNok;

  // check if PER by Channel is enabled
  if ( connPtr->perInfoByChan != NULL )
  {
    connPtr->perInfoByChan->numPkts[ channel ]   += numPkts;
    connPtr->perInfoByChan->numCrcErr[ channel ] += connOutput.nRxNok;
  }

  /*****************************************************/
  /************ Stop Last Connection's Timer ***********/
  /*****************************************************/
#ifndef USE_RCL
  // Clear the RAT handle timer.
  MAP_llClearRatCompare();
#endif
  // check if any data has been received
  // Note: numRxOk includes numRxCtrl
  // Note: numRxNotOk removed as 4.5.2 of spec says the timer is reset upon
  //       receipt of a "valid packet", which is taken to mean no CRC error.
#ifdef USE_RCL
  if ( connOutput.nRxOk    || connOutput.nRxIgnored ||
       connOutput.nRxEmpty || connOutput.nRxFifoFull )
#else
  if ( connOutput.nRxOk    || connOutput.nRxIgn ||
       connOutput.nRxEmpty || connOutput.nRxBufFull )
#endif
  {
    connEvtStatus = LL_CONN_EVT_STAT_SUCCESS;

    // The chosen connection miss count parameter needs to be reset.
    connPtr->connMissCount = 0;

    /**********************************************/
    /************ Connection Starvation ***********/
    /**********************************************/
    // In case the connection reached half of LSTO -> it was starved.
    // This can happen in case this connection was configured with a lower priority than other
    // connection events or secondary tasks.
    // We need to make sure the central has heard the peripheral in this starvation state,
    // otherwise the connection might disconnect later on.
    if (connPtr->StarvationMode == TRUE)
    {
      // In this half LSTO state, the central would need to send a minimum of LL_MAX_CENTRAL_NUM_LSTO_RETRIES continues retries to the peripheral
      // in order to make sure the received SN BIT from the central was changed.
      // Once the SN BIT from the central was changed -> it indicates the central has
      // received the peripheral's packet and they met.

      // Perform this only LL_MAX_CENTRAL_NUM_LSTO_RETRIES times.
      if (connPtr->numLSTORetries < LL_MAX_CENTRAL_NUM_LSTO_RETRIES)
      {
        // Increment the number of retries.
        connPtr->numLSTORetries++;
      }
      else
      {
        // Disable Starvation Mode:
        // - Reset the lsto number of retries.
        // - Reset starvation mode bit.
        MAP_llSetStarvationMode(connPtr->connId, LL_SET_STARVATION_MODE_OFF);
      }
    }//End of starvation mode handling.

    // Update the supervision expiration count.
    connPtr->expirationEvent = connPtr->currentEvent + connPtr->expirationValue;

    // clear flag that indicates we received first packet
    // Note: The first packet only really needs to be signalled when a new
    //       connection is formed. However, there's no harm in resetting it
    //       every time in order to simplify the control logic.
    // Note: True-Low logic is used here to be consistent with nR's language.
    connPtr->firstPacket = FALSE;

    // Reset DMM threshold
    MAP_llDmmSetThreshold(LL_STATE_CONN_CENTRAL,connPtr->connId,TRUE);
  }
  else // no data received, or packet received with CRC error
  {
    // Set DMM threshold
    MAP_llDmmSetThreshold(LL_STATE_CONN_CENTRAL,connPtr->connId,FALSE);
    // In case the Connection Starvation Mechanism is ON:
    // - Reset the numLSTORetries and starvation mode bit.
    if ((connPtr->numLSTORetries > 0) || (connPtr->StarvationMode == TRUE))
    {
        // Disable Starvation Mode.
        MAP_llSetStarvationMode(connPtr->connId, LL_SET_STARVATION_MODE_OFF);
    }

    // check if data was received with a CRC error, or no data was received
    if ( connOutput.nRxNok )
    {
      connEvtStatus = LL_CONN_EVT_STAT_CRC_ERROR;

      // According to spec 5.3, Vol 6, Part B, 4.5.2,
      // Since a packet with a CRC error is sufficient to establish the connection, we need to use the
      // LSTO of the connection as the expiration time-out.

      // Update the expiration event to LSTO, only if this is the first packet in the connection.
      // In any other case, expiration event should not be updated when CRC packet is received.
      if (connPtr->firstPacket)
      {
        connPtr->expirationEvent = connPtr->expirationValue;
      }
      // Connection established, so clear the flag that indicates that we have received first packet in this connection
      connPtr->firstPacket = FALSE;
    }
    else // no packet was received
    {
      connEvtStatus = LL_CONN_EVT_STAT_MISSED;

      // collect packet error information
      connPtr->perInfo.numMissedEvts++;

      //HAL_GPIO_SET( HAL_GPIO_6 );
      //HAL_GPIO_CLR( HAL_GPIO_6 );
      //HAL_GPIO_SET( HAL_GPIO_6 );
      //HAL_GPIO_CLR( HAL_GPIO_6 );
    }

    // check if we have a Supervision Timeout
    // Note: It is possible with multiple connections and collision handling of
    //       connections, that a connection's next event (which at this point
    //       is now the current event) might be beyond the expiration event. So
    //       this is checked, taking event counter wrap into account.
    //       Note: Max possible event count before LSTO: 32s/7.5ms=4267 events.
    if ( (connPtr->currentEvent == connPtr->expirationEvent) ||
          ((connPtr->currentEvent > connPtr->expirationEvent) &&
           ((connPtr->currentEvent-connPtr->expirationEvent) < LL_MAX_TIMEOUT_EVENTS)) )
    {
      /* Central return values on connection termination:
      * If connection created but not established, and received invalid access address on data channel PDU -> return 0x3e
      * If connection established, and received data packet with incorrect access address -> return 0x08
      * If connection created but not established, and received CRC on data channel PDU -> return 0x8
      * If connection created but not established, and no response from peripheral -> return 0x3e
      * */
      if (connPtr->perInfo.numPkts == 0)
      {
        // terminate with failure to establish connection
        MAP_llConnTerminate( connPtr, LL_CONN_ESTABLISHMENT_FAILED_TERM );
      }
      else
      {
        // terminate with LSTO
        MAP_llConnTerminate( connPtr, LL_SUPERVISION_TIMEOUT_TERM );
      }

      return;
    }
  }

#ifdef LL_TEST_MODE
#ifndef CC23X0
  if ( llTestMode.testCase == LL_TEST_MODE_TP_TIM_SLA_BV_05 )
  {
    // use the first Tx packet to start the test
    if ( connOutput.nTxEntryDone != 0 )
    {
      firstTx = TRUE;
    }

    // check if the test is done
    if ( (timSlvBv05Done == FALSE) && (firstTx == TRUE) )
    {
      // yep, so count event and Tx, but only if it wasn't a retransmission or control packet
      if ( (connOutput.nTxEntryDone != 0) &&
           (connOutput.nTxRetrans == 0)   &&
           (connOutput.nTxCtrl == 0) )
      {
        //numTxPkts += connOutput.nTxEntryDone;
        numTxPkts++;
        numTxEvts++;

        // check if the Peripheral ACK'ed with an empty packet and no CRC error
        if ( (connOutput.nRxEmpty == 0) && (connOutput.nRxNok) )
        {
          // it didn't, so take this as a missed event; the set of 10 tx packets
          // is considered failed, and the nominal CI should be used when setting
          // up the next event
          setFailed = TRUE;
          nomCI = TRUE;
        }
        else // Peripheral empty packet ACK received
        {
          // so don't use the nominal CI
          nomCI = FALSE;
        }

        // check if the set if finished
        if ( (numTxEvts % 10) == 0 )
        {
          // bump the number of sets and reset event count
          numSets++;

          // wait for next set and return to nominal CI
          firstTx = FALSE;
          nomCI = TRUE;

          // check if the set failed
          if ( setFailed == TRUE )
          {
            // yep, so count it
            numFailedSets++;
            setFailed = FALSE;
          }

          // check if the test is finished
          if ( numSets == 5 )
          {
            // yep, so end the test tracking
            timSlvBv05Done = TRUE;
          }
        }
      }
      else // we didn't transmit a packet during this event
      {
        numFailedTx++;
        nomCI = TRUE;
      }
    }
  }
#endif
#endif // LL_TEST_MODE

#ifndef CC23X0
  // obtain the RSSI, if present
  connPtr->lastRssi = (RSSI_SUFFIX_PRESENT() && (connOutput.lastRssi != LL_RF_RSSI_UNDEFINED))?connOutput.lastRssi:LL_RF_RSSI_INVALID;
#else
  connPtr->lastRssi = (LRF_RSSI_INVALID == connOutput.lastRssi) ? LL_RF_RSSI_INVALID : connOutput.lastRssi;
#endif

#ifdef RTLS_CTE
  // get the CTE information in case received CTE response packet
  if (llCteSamples.autoCopyCompleted > 0)
  {
    MAP_llGetCteInfo( CTE_TASK_ID_CONNECTION, connPtr );
  }
#endif // RTLS_CTE

  // check Control Procedure Processing
  if ( MAP_llProcessCentralControlProcedures( connPtr ) == LL_CTRL_PROC_STATUS_TERMINATE )
  {
    // this connection is terminated, so nothing to schedule
    return;
  }

  // procoessing Tx data (if any)
  MAP_llProcessTxData( connPtr, LL_TX_DATA_CONTEXT_POST_PROCESSING );

#ifdef USE_RCL
  //align the RX buffers head and tail pointers with all other active connections
  llUpdateRxBuffersForActiveConnections(&rxDataQ.multiBuffers);
#endif // USE_RCL

  // Send the callback before calculating the next channel
  llSendConnEvtCallback(connEvtStatus, numPkts, connPtr);

  // update next event, calculate time to next event, calculate timer drift,
  // update anchor points, setup NR T2E1 and T2E2 events
  if ( MAP_llSetupNextCentralEvent() == LL_SETUP_NEXT_LINK_STATUS_TERMINATE )
  {
    // this connection is terminated, so nothing to schedule
    return;
  }

#ifdef RTLS_CTE
  // update CTE state
  MAP_llUpdateCteState( connPtr );
#endif // RTLS_CTE

  // determine next task (if any) and schedule it
  MAP_llScheduler();

  return;
}


/*******************************************************************************
 * @fn          llSetupNextCentralEvent
 *
 * @brief       This function is used to setup the next Central connection
 *              event, checks if an Update Parameter, Update Data Channel,
 *              and/or an Update PHY has occurred, and sets the next data
 *              channel.
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      boolean - Indicates whether a link terminate occurred:
 *                        LL_SETUP_NEXT_LINK_STATUS_TERMINATE: Terminate due to a LSTO.
 *                        LL_SETUP_NEXT_LINK_STATUS_SUCCESS: Do not terminate.
 */
uint8 llSetupNextCentralEvent( void )
{
  llConnState_t *connPtr;
  uint32        timeToNextEvt;

  LL_ASSERT( llConns.currentConn != LL_INVALID_CONNECTION_ID );

  // get pointer to connection info
  connPtr = MAP_llDataGetConnPtr( llConns.currentConn );

  // update the next connection event count
  connPtr->nextEvent = (uint16)(connPtr->currentEvent + 1);
  /*
  ** Check for a Update RX Buffers length
  **
  */
#ifdef USE_RCL
  if ( TST_FEATURE_FLAG( connPtr->lenInfo.lenFlags, REPLACE_RX_BUFFERS ) )
  {
    // Clear the flag
    CLR_FEATURE_FLAG( connPtr->lenInfo.lenFlags, REPLACE_RX_BUFFERS );
    // Check for update the RX buffers
    MAP_llReplaceRxBuffers( connPtr );
  }
#endif
  /*
  ** Check for a PHY Update
  **
  ** For now, just override SL.
  */

  // is a PHY updated pending
  if ( (connPtr->pendingPhyUpdate == TRUE) &&
       (connPtr->phyUpdateEvent == (connPtr->currentEvent+1)) )
  {
    // set the current phy to the updated phy on the next event
    connPtr->phyInfo.curPhy = connPtr->phyInfo.updatePhy;

    // clear the PHY update flag
    // Note: The phy will be set in llScheduleTask.
    connPtr->pendingPhyUpdate = PHY_UPDATE_APPLIED;

    // notify the Host
    MAP_LL_PhyUpdateCompleteEventCback( LL_STATUS_SUCCESS,
                                        connPtr->connId,
                                        connPtr->phyInfo.curPhy,
                                        connPtr->phyInfo.curPhy );
  }

  /*
  ** Check for a Parameter Update
  */

  // check if there's a connection parameter udpate to the connection, and if
  // so, check if the update event is before or equal to the next active event
  if ( (connPtr->pendingParamUpdate == PARAM_UPDATE_PENDING) &&
       (connPtr->nextEvent == connPtr->paramUpdateEvent) )
  {
#ifdef DEBUG_SW_TRACE
    DBG_PRINT0(DBGSYS, "");
    DBG_PRINT0(DBGSYS, "CENTRAL UPDATE INSTANT!" );
    DBG_PRINT0(DBGSYS, "");
#endif // DEBUG_SW_TRACE

    // find the number of events between this event and the update parameter
    // event, based on the original connection interval
    // Note: The old connection interval must be used!
    timeToNextEvt = (uint32)connPtr->curParam.connInterval +
                    (uint32)connPtr->paramUpdate.winOffset;

    llApplyParamUpdate(connPtr);
  }
  else // no parameter update, so...
  {
    // time to next event continues as usual
    timeToNextEvt = connPtr->curParam.connInterval;
  }

  // set next start time based on previous anchor point
  connPtr->llTask->lastStartTime = connPtr->llTask->anchorPoint;

  connPtr->llTask->anchorPoint  += (timeToNextEvt * RAT_TICKS_IN_625US);

  connPtr->llTask->startTime     = connPtr->llTask->anchorPoint;

#ifdef LL_TEST_MODE
  if ( llTestMode.testCase == LL_TEST_MODE_TP_TIM_SLA_BV_05 )
  {
    // check if CI is nominal
    if ( nomCI == FALSE )
    {
      //HAL_GPIO_SET( HAL_GPIO_2 );

      // it isn't, so add 15.5us to start time
      connPtr->llTask->anchorPoint += RAT_TICKS_IN_15_5US;
    }
  }
#endif // LL_TEST_MODE

#ifdef USE_RCL
  // setup the start time for first Central packet
  linkCmd[connPtr->connId].common.timing.absStartTime = connPtr->llTask->anchorPoint;
  // clear command status value
  linkCmd[connPtr->connId].common.status = RCL_CommandStatus_Idle;
  // setup the connection event End Time
  // Note: Per the spec, this an be as late as 150us (i.e. T_IFS) before the
  //       next connection event. For now, we'll leave some slots for safety.
  linkCmd[connPtr->connId].common.timing.relHardStopTime =
    ((((uint32)connPtr->curParam.connInterval * *llConfigTable.connEvtCutoff) / 100) * RAT_TICKS_IN_625US) -
    (2 * RAT_TICKS_IN_150US);

#else //USE_RCL
  // setup the start time for first Central packet
  linkCmd[connPtr->connId].rfOpCmd.startTime = connPtr->llTask->anchorPoint;

  // set start trigger
  SET_RFOP_TRIG_TYPE( linkCmd[connPtr->connId].rfOpCmd.startTrig, TRIGTYPE_AT_ABS_TIME );

  // clear command status value
  linkCmd[connPtr->connId].rfOpCmd.status = RFSTAT_IDLE;

#ifdef DEBUG_SW_TRACE
  DBG_PRINT0(DBGSYS, "");
  DBG_PRINTL1(DBGSYS, "CENTRAL Start Time = 0x%08X", linkCmd[connPtr->connId].rfOpCmd.startTime );
  DBG_PRINT0(DBGSYS, "");
#endif // DEBUG_SW_TRACE

  // setup the connection event End Time
  // Note: Per the spec, this an be as late as 150us (i.e. T_IFS) before the
  //       next connection event. For now, we'll leave some slots for safety.
  // Note: End trigger remains as it was when connection was formed.
  // set the End Time
  // Note: The Central connection parameter structure differs from the Peripheral
  //       connection parameter structure in that there is no timeout trigger
  //       or timeout time. In all other ways these two structures are
  //       identical. So when using the Peripheral connection parameter structure
  //       as the common link parameter structure, the end time offset in
  //       the structure corresponds to the timeoutTime field.
  //linkParam[connPtr->connId].endTime =
  linkParam[connPtr->connId].timeoutTime =
    ((((uint32)connPtr->curParam.connInterval * *llConfigTable.connEvtCutoff) / 100) * RAT_TICKS_IN_625US) -
    (2 * RAT_TICKS_IN_150US);


  // check if one packet per event is enabled or there are multiple connections
  // Note: For now, no more than one packet per event per connection when there
  //       are multiple central connections.
  if ( onePktPerEvt == TRUE )
  {
    // set limit for the number of packets to transmit before it ends
    linkParam[connPtr->connId].maxTxPkt = ONE_PKT_PER_EVENT;
  }
  else // one packet per event is disabled and number of connections is one
  {
    // so restore configured max number of packets
    linkParam[connPtr->connId].maxTxPkt = llConfigTable.maxPktsPerEvtPtr->maxMstPktsPerEvt;
  }
#endif //USE_RCL

  // set Tx power for this command
  llSetPower((uint32 *)&linkCmd[connPtr->connId], curTxPowerVal, RfBleDpl_getTxPower(curTxPowerVal));

  // set PHY mode
  // Note: The user interface uses 0 to mean No Phy, while the radio command
  //       uses 0 to mean 1M. At this point, the curPhy should never be
  LL_ASSERT( connPtr->phyInfo.curPhy != LL_PHY_NONE );

  RfBleDpl_setConnPhy(connPtr->connId, connPtr->phyInfo.curPhy, connPtr->phyInfo.phyOpts);
  llSetRangeDelay(connPtr);

  // Note: Output Parameter Counters are cleared in llScheduleTask!

  // pointer to first radio operation command
  connPtr->llTask->command = (uint32)&linkCmd[connPtr->connId];

  // check for a Data Channel Update and calculate and set next data channel
  // Note: The Data Channel Update must come after the Parameters Update in
  //       case the latter updates the next event count.
  MAP_llSetNextDataChan( connPtr );

  return( LL_SETUP_NEXT_LINK_STATUS_SUCCESS );
}


/*******************************************************************************
 * @fn          llProcessCentralControlProcedures
 *
 * @brief       This function is used to process any control procedures that
 *              may be active.
 *
 *              Note: There can only be one active control procedure at a time.
 *
 *              Note: It is assumed the NR counters have been updated at the
 *                    end of the task before calling this routine.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      uint8 - Status of control procedure processing, which can be:
 *                      LL_CTRL_PROC_STATUS_SUCCESS: Continue normally.
 *                      LL_CTRL_PROC_STATUS_TERMINATE: We have terminated.
 */
uint8 llProcessCentralControlProcedures( llConnState_t *connPtr )
{
  LL_ASSERT( connPtr != NULL );

  // check if there are any control packets ready for processing
  while ( connPtr->ctrlPktInfo.ctrlPktCount > 0 )
  {
    if (connPtr->ctrlPktInfo.ctrlPkts[0] <= LL_CTRL_BLE_LOG_STRINGS_MAX)
    {
      BLE_LOG_INT_STR(0, BLE_LOG_MODULE_CTRL, "CTRL: llProcessCentralControlProcedures: connId=%d, ctrlType=%s\n", connPtr->connId, llCtrl_BleLogStrings[connPtr->ctrlPktInfo.ctrlPkts[0]]);
    }
    else
    {
      BLE_LOG_INT_INT(0, BLE_LOG_MODULE_CTRL, "CTRL: llProcessCentralControlProcedures: connId=%d, ctrlType=0x%x\n", connPtr->connId, connPtr->ctrlPktInfo.ctrlPkts[0]);
    }
    // processing based on control packet type at the head of the queue
    switch( connPtr->ctrlPktInfo.ctrlPkts[0] )
    {
      /*
      ** Terminated Indication
      */
      case LL_CTRL_TERMINATE_IND:
        // check if the control packet procedure is is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // we have already place packet on TX FIFO, so check if its been ACK'ed
#ifdef USE_RCL
          if ( connOutput.nTxCtlAck )
#else
          if ( connOutput.nTxCtrlAck )
#endif
          {
            // done with this control packet, so remove from the processing queue
            MAP_llDequeueCtrlPkt( connPtr );

            // yes, so process the termination
            // Note: No need to cleanup control packet info as we are done.
            MAP_llConnTerminate( connPtr, LL_HOST_REQUESTED_TERM );

            return( LL_CTRL_PROC_STATUS_TERMINATE );
          }
          else // no done yet
          {
            // check if a termination control procedure timeout has occurred
            // Note: No need to cleanup control packet info as we are done.
            if ( --connPtr->ctrlPktInfo.ctrlTimeout == 0 )
            {
              // we're done waiting, so end it all
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_TIMEOUT_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            else
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupTermInd( connPtr );

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }

        // Note: Unreachable statement generates compiler warning!
        //break;

      /*
      ** Connection Parameter Update
      */
      case LL_CTRL_CONNECTION_UPDATE_IND:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // we have already place packet on TX FIFO, so check if its been ACK'ed
#ifdef USE_RCL
          if ( connOutput.nTxCtlAck )
#else
          if ( connOutput.nTxCtrlAck )
#endif
          {
            // yes, so adjust all time values to units of 625us
            connPtr->paramUpdate.winSize      <<= 1;
            connPtr->paramUpdate.winOffset    <<= 1;
            connPtr->paramUpdate.connInterval <<= 1;
            connPtr->paramUpdate.connTimeout  <<= 4;

            // and activate the update
            connPtr->pendingParamUpdate = PARAM_UPDATE_PENDING;

            // done with this control packet, so remove from the processing queue
            MAP_llDequeueCtrlPkt( connPtr );
          }
          else // no done yet
          {
            // Core Spec V4.0 now indicates there is no control procedure
            // timeout. However, it still seems prudent to monitor for the
            // instant while waiting for the peripheral's ACK.
            if ( connPtr->nextEvent == connPtr->paramUpdateEvent )
            {
              // this event is the instant, and the control procedure still
              // has not been ACK'ed, we the instant has passed
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_INSTANT_PASSED_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            else // continue waiting for the peripheral's ACK
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupUpdateParamReq( connPtr );

          // set the control packet timeout for 40s relative to our present time
          // Note: This is done in terms of connection events.
          connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }
        break;

      /*
      ** Channel Map Update
      */
      case LL_CTRL_CHANNEL_MAP_IND:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // we have already place packet on TX FIFO, so check if its been ACK'ed
#ifdef USE_RCL
          if ( connOutput.nTxCtlAck )
#else
          if ( connOutput.nTxCtrlAck )
#endif
          {
            // yes, so activate the update
            connPtr->pendingChanUpdate = TRUE;

            // done with this control packet, so remove from the processing queue
            MAP_llDequeueCtrlPkt( connPtr );
          }
          else // no done yet
          {
            // Core Spec V4.0 now indicates there is no control procedure
            // timeout. However, it still seems prudent to monitor for the
            // instant while waiting for the peripheral's ACK.
            if ( connPtr->nextEvent == connPtr->chanMapUpdateEvent )
            {
              // this event is the instant, and the control procedure still
              // has not been ACK'ed, we the instant has passed
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_INSTANT_PASSED_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            else // continue waiting for the peripheral's ACK
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupUpdateChanReq( connPtr );

          // set the control packet timeout for 40s relative to our present time
          // Note: This is done in terms of connection events.
          connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }
        break;

      /*
      ** Encryption Request
      */
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
      case LL_CTRL_ENC_REQ:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // yes, so check if it has been transmitted yet
          // Note: This does not mean this packet has been ACK'ed or NACK'ed.
#ifdef USE_RCL
          if ( connOutput.nTxCtl )
#else
          if ( connOutput.nTxCtrl )
#endif
          {
            // check if a collision occurred
            if ( connPtr->connParamReqFlags.connParamReqRcved == TRUE )
            {
              // clear the flag
              connPtr->connParamReqFlags.connParamReqRcved = FALSE;

              // and setup the Update after the encryption is done
              MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_CONNECTION_UPDATE_IND );
            }
          }

          // we have already placed a packet on TX FIFO, so wait now until we
          // get the peripheral's LL_START_ENC_REQ
          if ( connPtr->encInfo.startEncReqRcved == TRUE )
          {
            // clear packet counters
            connPtr->encInfo.txPktCount = 0;
            connPtr->encInfo.rxPktCount = 0;
            // enable encryption
            connPtr->encEnabled = TRUE;

            /**** UPDATE DEBUG INFO MODULE ****/
            (void)MAP_DbgInf_addConnEst(connPtr->connId, HCI_EVT_CENTRAL_ROLE, UTRUE);

            // replace control procedure at head of queue to prevent interleaving
            MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_START_ENC_RSP,
                                  LL_CTRL_UNDEFINED_PKT );
          }
          else if ( connPtr->encInfo.rejectIndRcved  == TRUE )
          {
            // the peripheral's Host has failed to provide an LTK, so the encryption
            // setup has been rejected; end the start encryption procedure

            // done with this control packet, so remove from the processing queue
            MAP_llDequeueCtrlPkt( connPtr );

            // disable encryption
            // Note: Not really necessary as no data is supposed to be sent
            //       or received.
            connPtr->encEnabled = FALSE;

            /**** UPDATE DEBUG INFO MODULE ****/
            (void)MAP_DbgInf_addConnEst(connPtr->connId, HCI_EVT_CENTRAL_ROLE, UFALSE);

            // set flag to allow outgoing transmissions again
            connPtr->txDataEnabled = TRUE;

            // return any data entries that were stalled on temp queue to the RF queue
            MAP_llMoveTempTxDataEntries( connPtr );

            // set flag to allow all incoming data transmissions
            connPtr->rxDataEnabled = TRUE;

            // clear encryption in-progress flag
            connPtr->encInfo.encInProgress = FALSE;

            // check the rejection indication error code
            if ( connPtr->encInfo.encRejectErrCode == LL_STATUS_ERROR_PIN_OR_KEY_MISSING )
            {
              // notify the Host
              MAP_LL_EncChangeCback( connPtr->connId,
                                     LL_ENC_KEY_REQ_REJECTED,
                                     LL_ENCRYPTION_OFF );
            }
            else // LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE
            {
              // notify the Host
              MAP_LL_EncChangeCback( connPtr->connId,
                                     LL_ENC_KEY_REQ_UNSUPPORTED_FEATURE,
                                     LL_ENCRYPTION_OFF );
            }
          }
          else if ( connPtr->termInfo.termIndRcvd == TRUE )
          {
            // the peripheral's Host has failed to provide an LTK, so the encryption
            // setup has been rejected; end the start encryption procedure

            // done with this control packet, so remove from the processing queue
            MAP_llDequeueCtrlPkt( connPtr );
          }
          else // no done yet
          {
            // check if a update param req control procedure timeout has occurred
            // Note: No need to cleanup control packet info as we are done.
            if ( --connPtr->ctrlPktInfo.ctrlTimeout == 0 )
            {
              // notify the Host
              if ( connPtr->encInfo.encRestart == TRUE )
              {
                // a key change was requested
                MAP_LL_EncKeyRefreshCback( connPtr->connId,
                                           LL_CTRL_PKT_TIMEOUT_TERM );
              }
              else
              {
                // a new encryption was requested
                MAP_LL_EncChangeCback( connPtr->connId,
                                       LL_CTRL_PKT_TIMEOUT_TERM,
                                       LL_ENCRYPTION_OFF );
              }

              // we're done waiting, so end it all
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_TIMEOUT_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            else
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupEncReq( connPtr );

          // set the control packet timeout for 40s relative to our present time
          // Note: This is done in terms of connection events.
          connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;

          // set a flag to indicate we have received LL_START_ENC_REQ
          // Note: The LL_ENC_RSP will be received first, which will result in
          //       the central calculating its IVm and SKDm, concatenating it
          //       with the peripheral's IVs and SKDs, and calculating the SK from
          //       the LTK and SKD. After that, we will receive the
          //       LL_START_ENC_REQ from the central. So, it is okay to stay in
          //       this control procedure until LL_START_ENC_REQ is received.
          // Note: It is okay to repeatedly set this flag in the event the
          //       setup routine hasn't completed yet (e.g. if the TX FIFO
          //       has not yet become empty).
          connPtr->encInfo.startEncReqRcved = FALSE;
          connPtr->encInfo.rejectIndRcved   = FALSE;

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }
        break;
#endif // INIT_CFG

      /*
      ** Start Encryption Response
      */
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
      case LL_CTRL_START_ENC_RSP:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // we have already placed a packet on TX FIFO, so wait now until we
          // get the peripheral's LL_START_ENC_RSP
          if ( connPtr->encInfo.startEncRspRcved == TRUE )
          {
            // done with this control packet, so remove from the processing queue
            MAP_llDequeueCtrlPkt( connPtr );

            // clear the restart flag, in case of another key change request,
            // and all other encryption flags
            // Note: But in reality, there isn't a disable encryption in BLE,
            //       so once encryption is enabled, any call to LL_StartEncrypt
            //       will result in an encryption key change callback.
            connPtr->encInfo.encRestart       = FALSE;
            connPtr->encInfo.encReqRcved      = FALSE;
            connPtr->encInfo.pauseEncRspRcved = FALSE;
            connPtr->encInfo.startEncReqRcved = FALSE;
            connPtr->encInfo.startEncRspRcved = FALSE;
            connPtr->encInfo.rejectIndRcved   = FALSE;
            connPtr->encInfo.encInProgress    = FALSE;
          }
          else // no done yet
          {
            // check if a update param req control procedure timeout has occurred
            // Note: No need to cleanup control packet info as we are done.
            if ( --connPtr->ctrlPktInfo.ctrlTimeout == 0 )
            {
              // notify the Host
              if ( connPtr->encInfo.encRestart == TRUE )
              {
                // a key change was requested
                MAP_LL_EncKeyRefreshCback( connPtr->connId,
                                           LL_CTRL_PKT_TIMEOUT_TERM );
              }
              else
              {
                // a new encryption was requested
                MAP_LL_EncChangeCback( connPtr->connId,
                                       LL_CTRL_PKT_TIMEOUT_TERM,
                                       LL_ENCRYPTION_OFF );
              }

              // we're done waiting, so end it all
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_TIMEOUT_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            else
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          // Note: The llSetupStartEncRsp routine will *not* reset the control
          //       timeout value since the entire encryption procedure starts
          //       with the central sending the LL_ENC_REQ, and ends when the
          //       central receives the LL_START_ENC_RSP from the central.
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupStartEncRsp( connPtr );

          // set the control packet timeout for 40s relative to our present time
          // Note: This is done in terms of connection events.
          connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;

          // set a flag to indicate we have received LL_START_ENC_RSP
          // Note: It is okay to repeatedly set this flag in the event the
          //       setup routine hasn't completed yet (e.g. if the TX FIFO
          //       has not yet become empty).
          connPtr->encInfo.startEncRspRcved = FALSE;

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }
        break;
#endif // ADV_CONN_CFG | INIT_CFG

      /*
      ** Encryption Pause Request
      */
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & INIT_CFG)
      case LL_CTRL_PAUSE_ENC_REQ:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // we have already placed a packet on TX FIFO, so wait now until we
          // get the peripheral's LL_PAUSE_ENC_RSP
          if ( connPtr->encInfo.pauseEncRspRcved == TRUE )
          {
            // disable encryption
            connPtr->encEnabled = FALSE;

            /**** UPDATE DEBUG INFO MODULE ****/
            (void)MAP_DbgInf_addConnEst(connPtr->connId, HCI_EVT_CENTRAL_ROLE, UFALSE);

            // replace control procedure at head of queue to prevent interleaving
            MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_PAUSE_ENC_RSP,
                                  LL_CTRL_UNDEFINED_PKT );
          }
          else // no done yet
          {
            // check if a update param req control procedure timeout has occurred
            // Note: No need to cleanup control packet info as we are done.
            if ( --connPtr->ctrlPktInfo.ctrlTimeout == 0 )
            {
              // notify the Host
              if ( connPtr->encInfo.encRestart == TRUE )
              {
                // a key change was requested
                MAP_LL_EncKeyRefreshCback( connPtr->connId,
                                           LL_CTRL_PKT_TIMEOUT_TERM );
              }
              else
              {
                // a new encryption was requested
                MAP_LL_EncChangeCback( connPtr->connId,
                                       LL_CTRL_PKT_TIMEOUT_TERM,
                                       LL_ENCRYPTION_OFF );
              }

              // we're done waiting, so end it all
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_TIMEOUT_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            else
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupPauseEncReq( connPtr );

          // set the control packet timeout for 40s relative to our present time
          // Note: This is done in terms of connection events.
          connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;

          // set a flag to indicate we have received LL_START_ENC_RSP
          // Note: It is okay to repeatedly set this flag in the event the
          //       setup routine hasn't completed yet (e.g. if the TX FIFO
          //       has not yet become empty).
          connPtr->encInfo.pauseEncRspRcved = FALSE;

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }

        break;
#endif // INIT_CFG

      /*
      ** Encryption Pause Response
      */
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
      case LL_CTRL_PAUSE_ENC_RSP:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // yes, so check if it has been transmitted yet
          // Note: This only means the packet has been transmitted, not that it
          //       has been ACK'ed or NACK'ed.
#ifdef USE_RCL
          if ( connOutput.nTxCtl )
#else
          if ( connOutput.nTxCtrl )
#endif
          {
            // replace control procedure at head of queue to prevent interleaving
            MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_ENC_REQ,
                                  LL_CTRL_UNDEFINED_PKT );
          }
          else // no done yet
          {
            // check if a update param req control procedure timeout has occurred
            // Note: No need to cleanup control packet info as we are done.
            if ( --connPtr->ctrlPktInfo.ctrlTimeout == 0 )
            {
              // notify the Host
              if ( connPtr->encInfo.encRestart == TRUE )
              {
                // a key change was requested
                MAP_LL_EncKeyRefreshCback( connPtr->connId,
                                           LL_CTRL_PKT_TIMEOUT_TERM );
              }
              else
              {
                // a new encryption was requested
                MAP_LL_EncChangeCback( connPtr->connId,
                                       LL_CTRL_PKT_TIMEOUT_TERM,
                                       LL_ENCRYPTION_OFF );
              }

              // we're done waiting, so end it all
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_TIMEOUT_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            else
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupPauseEncRsp( connPtr );

          // set the control packet timeout for 40s relative to our present time
          // Note: This is done in terms of connection events.
          connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }

        break;
#endif // ADV_CONN_CFG | INIT_CFG

      /*
      ** Feature Set Request
      */
      case LL_CTRL_FEATURE_REQ:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // we have already placed a packet on TX FIFO, so wait now until we
          // get the peripheral's LL_CTRL_FEATURE_RSP
          // Note: LL_FEATURE_RSP_FAILED cannot occur on Central.
          if ( connPtr->featureSetInfo.featureRspRcved == LL_FEATURE_RSP_DONE )
          {
            // notify the Host
            MAP_LL_ReadRemoteUsedFeaturesCompleteCback( LL_STATUS_SUCCESS,
                                                        connPtr->connId,
                                                        connPtr->featureSetInfo.featureSetMask );

            // done with this control packet, so remove from the processing queue
            MAP_llDequeueCtrlPkt( connPtr );
          }
          else // no done yet
          {
            // check if a update param req control procedure timeout has occurred
            // Note: No need to cleanup control packet info as we are done.
            if ( --connPtr->ctrlPktInfo.ctrlTimeout == 0 )
            {
              // indicate a control procedure timeout on this request
              // Note: The parameters are not valid.
              MAP_LL_ReadRemoteUsedFeaturesCompleteCback( LL_CTRL_PKT_TIMEOUT_TERM,
                                                          connPtr->connId,
                                                          connPtr->featureSetInfo.featureSetMask );
              // we're done waiting, so end it all
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_TIMEOUT_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            else
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupFeatureSetReq( connPtr );

          // set the control packet timeout for 40s relative to our present time
          // Note: This is done in terms of connection events.
          connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;

          // set flag while we wait for response
          // Note: It is okay to repeatedly set this flag in the event the
          //       setup routine hasn't completed yet.
          connPtr->featureSetInfo.featureRspRcved = LL_FEATURE_RSP_PENDING;

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }

        break;

      /*
      ** Feature Set Response
      */
      case LL_CTRL_FEATURE_RSP:
        // check if the control packet procedure is is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // yes, so check if it has been transmitted yet
          // Note: This does not mean this packet has been ACK'ed or NACK'ed.
#ifdef USE_RCL
          if ( connOutput.nTxCtl )
#else
          if ( connOutput.nTxCtrl )
#endif
          {
            // enable SL
            connPtr->peripheralLatency = connPtr->peripheralLatencyValue;

            // remove control packet from processing queue and drop through
            MAP_llDequeueCtrlPkt( connPtr );
          }
          else // not done yet
          {
            // check if a start enc req control procedure timeout has occurred
            // Note: No need to cleanup control packet info as we are done.
            // ALT: The control procedure timeout of 40s is converted to the
            //      corresponding number of events stored in ctrlTimeout. But
            //      this counter is only decremented during post-processing,
            //      which is fine when SL is zero. But when SL is not zero,
            //      multiple events have occurred, and the amount of time
            //      elapsed will be underestimated. To determine how many
            //      events have actually expired, the SL should be taken
            //      into account. However, when Fast Tx is used, the next event
            //      may get changed to an earlier value than originally used
            //      after the last current event, so Fast Tx must also be
            //      taken into account. Best to always track the number of
            //      events elapsed since the last event, and adjust ctrlTimeout
            //      based on this value.
            if ( --connPtr->ctrlPktInfo.ctrlTimeout == 0 )
            {
              // we're done waiting, so end it all
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_TIMEOUT_PEER_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            else
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          // Note: There is no control procedure timeout associated with this
          //       control packet.
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupFeatureSetRsp( connPtr );

          // set the control packet timeout for 40s relative to our present time
          // Note: This is done in terms of connection events.
          connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }

        break;

      /*
      ** Vendor Information Exchange (Request or Reply)
      */
      case LL_CTRL_VERSION_IND:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // yes, so check if the peer's version information is valid
          if ( connPtr->verExchange.peerInfoValid == TRUE )
          {
            // yes, so check if the host has requested this information
            if ( connPtr->verExchange.hostRequest == TRUE )
            {
              // yes, so provide it
              MAP_LL_ReadRemoteVersionInfoCback( LL_STATUS_SUCCESS,
                                                 connPtr->connId,
                                                 connPtr->verInfo.verNum,
                                                 connPtr->verInfo.comId,
                                                 connPtr->verInfo.subverNum );
            }

            // in any case, dequeue this control procedure
            MAP_llDequeueCtrlPkt( connPtr );
          }
          else // no done yet
          {
            // check if a update param req control procedure timeout has occurred
            // Note: No need to cleanup control packet info as we are done.
            if ( --connPtr->ctrlPktInfo.ctrlTimeout == 0 )
            {
              // we're done waiting, so complete the callback with error
              MAP_LL_ReadRemoteVersionInfoCback( LL_CTRL_PKT_TIMEOUT_TERM,
                                                 connPtr->connId,
                                                 connPtr->verInfo.verNum,
                                                 connPtr->verInfo.comId,
                                                 connPtr->verInfo.subverNum );

              // and end it all
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_TIMEOUT_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            else
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // since we are in the process of sending the version indication,
          // it is okay to set this flag here even if it is set repeatedly
          // in the of llSetupVersionIndReq failures
          connPtr->verExchange.verInfoSent = TRUE;

          // so try to put it there; being active depends on a success
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupVersionIndReq( connPtr );

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }
        break;

      /*
      ** Ping Control Procedure (Request)
      */
      case LL_CTRL_PING_REQ:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // yes, so check if it has been transmitted yet
          // Note: This does not mean this packet has been ACK'ed or NACK'ed.
#ifdef USE_RCL
          if ( connOutput.nTxCtl )
#else
          if ( connOutput.nTxCtrl )
#endif
          {
            // it has been sent, so dequeue this control procedure
            MAP_llDequeueCtrlPkt( connPtr );
          }
          else // no done yet
          {
            // check if a update param req control procedure timeout has occurred
            // Note: No need to cleanup control packet info as we are done.
            if ( --connPtr->ctrlPktInfo.ctrlTimeout == 0 )
            {
              // CPTO timeout, so end it all
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_TIMEOUT_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            else
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupPingReq( connPtr );

          // set the control packet timeout for 40s relative to our present time
          // Note: This is done in terms of connection events.
          connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }
        break;

      /*
      ** Connection Parameter Request
      */
      case LL_CTRL_CONNECTION_PARAM_REQ:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // check if this request was rejected
          if ( connPtr->connParamReqFlags.rejectIndExtRcved == TRUE )
          {
            // Sanity Check:
            // The rejected opcode should be LL_CTRL_CONNECTION_PARAM_REQ.
            // TBD: Check connPtr->rejectIndExt.errorCode as well?

            LL_ASSERT( connPtr->rejectIndExt.rejectOpcode == LL_CTRL_CONNECTION_PARAM_REQ );

            // clear reject indication extended received flag
            connPtr->connParamReqFlags.rejectIndExtRcved = FALSE;

            // note: test case LL/CON/MAS/BV-29-C
            if (connPtr->rejectIndExt.errorCode == LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE)
            {
              // simply replace with Update Parameter
              MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_CONNECTION_UPDATE_IND,
                                  LL_CTRL_UNDEFINED_PKT );
            }
            else
            {
              // peer rejected, so notify Host, and end
              // Expected error codes:
              //   LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE
              //   LL_STATUS_ERROR_UNACCEPTABLE_CONN_PARAMETERS
              //   LL_STATUS_ERROR_INVALID_PARAMS
              MAP_LL_ConnParamUpdateCback( connPtr->rejectIndExt.errorCode,
                                         0,
                                         0,
                                         0,
                                         0 );

              // remove control packet from processing queue and drop through
              MAP_llDequeueCtrlPkt( connPtr );
            }
          }
          // check if a collision occurred
          else if ( connPtr->connParamReqFlags.connParamReqRcved == TRUE )
          {
            // clear the flag
            connPtr->connParamReqFlags.connParamReqRcved = FALSE;

            // and send a Reject Indication
            // Note: Continue to wait for this procedure to complete properly.
            MAP_llSendReject( connPtr,
                              LL_CTRL_CONNECTION_PARAM_REQ,
                              LL_STATUS_ERROR_TRANSACTION_COLLISION );
          }
          // check if this request is not supported by the peer device
          else if ( connPtr->connParamReqFlags.unknownRspRcved == TRUE )
          {
            // clear unknown response received flag
            connPtr->connParamReqFlags.unknownRspRcved = FALSE;

            // TBD: Use the parameters that are already stored in the
            //      connection's paramUpdate field. Note that the relative
            //      paramUpdateEvent field is already set.

            // simply replace with Update Parameter
            MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_CONNECTION_UPDATE_IND,
                                  LL_CTRL_UNDEFINED_PKT );
          }
          else // no done yet
          {
            // check if a update param req control procedure timeout has occurred
            // Note: No need to cleanup control packet info as we are done.
            if ( --connPtr->ctrlPktInfo.ctrlTimeout == 0 )
            {
              // CPTO timeout, so end it all
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_TIMEOUT_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            else
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupConnParamReq( connPtr );

          // set the control packet timeout for 40s relative to our present time
          // Note: This is done in terms of connection events.
          connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }
        break;

      /*
      ** Reject Indication Extended
      */
      case LL_CTRL_REJECT_EXT_IND:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // we have already place packet on TX FIFO, so check if its been ACK'ed
#ifdef USE_RCL
          if ( connOutput.nTxCtlAck )
#else
          if ( connOutput.nTxCtrlAck )
#endif
          {
            // remove control packet from processing queue and drop through
            MAP_llDequeueCtrlPkt( connPtr );
          }
          else // no done yet
          {
            // check if a update param req control procedure timeout has occurred
            // Note: No need to cleanup control packet info as we are done.
            if ( --connPtr->ctrlPktInfo.ctrlTimeout == 0 )
            {
              // CPTO timeout, so end it all
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_TIMEOUT_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            else
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupRejectIndExt( connPtr );

          // set the control packet timeout for 40s relative to our present time
          // Note: This is done in terms of connection events.
          connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }
        break;

      /*
      ** PHY Request
      */
      case LL_CTRL_PHY_REQ:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          if ( TST_FEATURE_FLAG( connPtr->phyInfo.phyFlags, PHY_RSP_RECEIVED ) )
          {
#if defined( LL_TEST_MODE )
            if ( llTestMode.testCase == LL_TEST_MODE_TP_CON_SLA_BV_51 )
            {
              // done with this control packet, so remove from the processing queue
              MAP_llDequeueCtrlPkt( connPtr );
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
#endif // LL_TEST_MODE
            // Note: Do not clear this flag yet as we need to use it below in
            //       Update Request to distinguish whether the Central's Host
            //       initiated the Phy Update.

#if defined( LL_TEST_MODE )
            if ( (llTestMode.testCase == LL_TEST_MODE_TP_CON_SLA_BV59) &&
                 (llTestMode.counter-- != 0) )
            {
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
#endif // LL_TEST_MODE

            // replace LL_CTRL_PHY_REQ
            MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_PHY_UPDATE_REQ,
                                  LL_CTRL_UNDEFINED_PKT );
          }
          // check if this request was rejected
          else if ( TST_FEATURE_FLAG( connPtr->phyInfo.phyFlags, REJECT_EXT_IND_RECEIVED ) )
          {
            // Sanity Check:
            // The rejected opcode should be LL_CTRL_PHY_REQ.
            LL_ASSERT( connPtr->rejectIndExt.rejectOpcode == LL_CTRL_PHY_REQ );

            // clear reject indication extended received flag
            CLR_FEATURE_FLAG( connPtr->phyInfo.phyFlags, REJECT_EXT_IND_RECEIVED );

            // notify the Host
            MAP_LL_PhyUpdateCompleteEventCback( connPtr->rejectIndExt.errorCode,
                                                connPtr->connId,
                                                0,
                                                0 );
            // done with this control packet, so remove from the processing queue
            MAP_llDequeueCtrlPkt( connPtr );
          }
          // check if this request is not supported by the peer device
          else if ( TST_FEATURE_FLAG( connPtr->phyInfo.phyFlags, UNKNOWN_RSP_RECEIVED ) )
          {
            // clear unknown response received flag
            CLR_FEATURE_FLAG( connPtr->phyInfo.phyFlags, UNKNOWN_RSP_RECEIVED );

            // notify the Host
            MAP_LL_PhyUpdateCompleteEventCback( LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE,
                                                connPtr->connId,
                                                0,
                                                0 );

            // done with this control packet, so remove from the processing queue
            MAP_llDequeueCtrlPkt( connPtr );
          }
          else // not done yet
          {
            // check if a update param req control procedure timeout has occurred
            // Note: No need to cleanup control packet info as we are done.
            if ( --connPtr->ctrlPktInfo.ctrlTimeout == 0 )
            {
              // CPTO timeout, so end it all
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_TIMEOUT_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            else
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupPhyCtrlPkt( connPtr,
                                                                      LL_CTRL_PHY_REQ );

          // set the control packet timeout for 40s relative to our present time
          // Note: This is done in terms of connection events.
          connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;

#if defined( LL_TEST_MODE )
          if ( llTestMode.testCase == LL_TEST_MODE_TP_CON_SLA_BV59 )
          {
            // number of events to skip
            llTestMode.counter = 10;
          }
#endif // LL_TEST_MODE

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }
        break;

      /*
      ** PHY Update
      */
      case LL_CTRL_PHY_UPDATE_REQ:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // packet on TX FIFO, so check if its been ACK'ed
#ifdef USE_RCL
          if ( connOutput.nTxCtlAck )
#else
          if ( connOutput.nTxCtrlAck )
#endif
          {
            // check if the PHY will change
            if ( connPtr->phyInfo.updatePhy == LL_PHY_NONE )
            {
              // check if the Central's Host initiated the Phy Update procedure
              // Note: If the Central initiated the phy update procedure, then
              //       it would have sent the phy request, and the Peripheral would
              //       have sent a phy response.
              if ( TST_FEATURE_FLAG( connPtr->phyInfo.phyFlags, PHY_RSP_RECEIVED ) )
              {
                // then clear the flag
                CLR_FEATURE_FLAG( connPtr->phyInfo.phyFlags, PHY_RSP_RECEIVED );

                // no phy change, so no update instant required; notify Host now
                MAP_LL_PhyUpdateCompleteEventCback( LL_STATUS_SUCCESS,
                                                    connPtr->connId,
                                                    connPtr->phyInfo.curPhy,
                                                    connPtr->phyInfo.curPhy );
              }
              connPtr->pendingPhyUpdate =  PHY_UPDATE_APPLIED;
              connPtr->phyUpdatedNoChange = TRUE;
            }
            else // a PHY change will take place at instant
            {
              // be sure the clear the flag here as well
              CLR_FEATURE_FLAG( connPtr->phyInfo.phyFlags, PHY_RSP_RECEIVED );

              // indicate a pending update
              connPtr->pendingPhyUpdate = TRUE;
            }

            // done with control packet, so remove from the processing queue
            MAP_llDequeueCtrlPkt( connPtr );
          }
          else // not done yet
          {
            // Core Spec V5.0 indicates there is no control procedure timeout
            // once the Phy Update Req has been sent. However, it still seems
            // prudent to monitor for the instant while waiting for the Peripheral's
            // ACK.
            if ( connPtr->phyUpdateEvent == (connPtr->currentEvent+1) )
            {
              // this event is the instant, and the control procedure still
              // has not been ACK'ed, so the instant has passed
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_INSTANT_PASSED_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            else // continue waiting for the peripheral's ACK
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupPhyCtrlPkt( connPtr,
                                                                      LL_CTRL_PHY_UPDATE_REQ );

          // set the control packet timeout for 40s relative to our present time
          // Note: This is done in terms of connection events.
          connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }
        break;
      /*
      ** Length Request Update
      */
      case LL_CTRL_LENGTH_REQ:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          if ( TST_FEATURE_FLAG( connPtr->lenInfo.lenFlags, LEN_RSP_RECEIVED ) )
          {
            // clear the flag
            CLR_FEATURE_FLAG( connPtr->lenInfo.lenFlags, LEN_RSP_RECEIVED );

            // check if any data info change has occurred
            if ( TST_FEATURE_FLAG(connPtr->lenInfo.lenFlags, NOTIFY_HOST) )
            {
              // clear the flag
              CLR_FEATURE_FLAG( connPtr->lenInfo.lenFlags, NOTIFY_HOST );

              // yes, so notify Host now
              MAP_LL_DataLengthChangeEventCback( connPtr->connId,
                                                 connPtr->lenInfo.connEffectiveMaxTxOctets,
                                                 connPtr->lenInfo.connEffectiveMaxTxTime,
                                                 connPtr->lenInfo.connEffectiveMaxRxOctets,
                                                 connPtr->lenInfo.connEffectiveMaxRxTime );
            }

            // clear flag to indicate that a Length control procedure is in progress
            connPtr->pendingLenUpdate = FALSE;

            // done with this control packet, so remove from the processing queue
            MAP_llDequeueCtrlPkt( connPtr );
          }
          // check if this request is not supported by the peer device
          else if ( TST_FEATURE_FLAG( connPtr->lenInfo.lenFlags, UNKNOWN_RSP_RECEIVED ) )
          {
            // clear unknown response received flag
            CLR_FEATURE_FLAG( connPtr->lenInfo.lenFlags, UNKNOWN_RSP_RECEIVED );

            // clear flag to indicate that a Length control procedure is in progress
            connPtr->pendingLenUpdate = FALSE;

            // done with this control packet, so remove from the processing queue
            MAP_llDequeueCtrlPkt( connPtr );
          }
          else // not done yet
          {
            // check if a update param req control procedure timeout has occurred
            // Note: No need to cleanup control packet info as we are done.
            if ( --connPtr->ctrlPktInfo.ctrlTimeout == 0 )
            {
              // CPTO timeout, so end it all
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_TIMEOUT_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }

            //  control packet stays at head of queue, so exit here
            return( LL_CTRL_PROC_STATUS_SUCCESS );
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupLenCtrlPkt( connPtr,
                                                                      LL_CTRL_LENGTH_REQ );

          // set the control packet timeout for 40s relative to our present time
          // Note: This is done in terms of connection events.
          connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }
        break;

      /*
      ** Length Response Update
      */
      case LL_CTRL_LENGTH_RSP:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
#ifdef USE_RCL
          if ( connOutput.nTxCtl )
#else
          if ( connOutput.nTxCtrl )
#endif
          {

            // remove control packet from processing queue and drop through
            MAP_llDequeueCtrlPkt( connPtr );
          }
          else
          {
            // check if a update param req control procedure timeout has occurred
            // Note: No need to cleanup control packet info as we are done.
            if ( --connPtr->ctrlPktInfo.ctrlTimeout == 0 )
            {
              // CPTO timeout, so end it all
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_TIMEOUT_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            //  control packet stays at head of queue, so exit here
            return( LL_CTRL_PROC_STATUS_SUCCESS );
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupLenCtrlPkt( connPtr,
                                                                      LL_CTRL_LENGTH_RSP );

          // set the control packet timeout for 40s relative to our present time
          // Note: This is done in terms of connection events.
          connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }
        break;

      /*
      ** Constant Tone Extension Request
      */
#ifdef RTLS_CTE
      case LL_CTRL_CTE_REQ:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // check that the CTE request or response procedure was done
          if ( llCte[connPtr->connId].initiator.sendRequest == FALSE )
          {
            // remove control packet from processing queue and drop through
            MAP_llDequeueCtrlPkt( connPtr );
          }
          else // no done yet
          {
            // check if control procedure timeout has occurred
            // Note: No need to cleanup control packet info as we are done.
            if ( --connPtr->ctrlPktInfo.ctrlTimeout == 0 )
            {
              // CPTO timeout, so end it all
              // Note: No need to cleanup control packet info as we are done.
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_TIMEOUT_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            else
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupCte( connPtr,TRUE );

          // set the control packet timeout for 40s relative to our present time
          // Note: This is done in terms of connection events.
          connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;

          // Note: Two cases are possible:
          //       a) We successfully placed the packet in the TX FIFO.
          //       b) We did not.
          //
          //       In case (a), it may be possible that a previously just
          //       completed control packet happened to complete based on
          //       rfCounters.numTxCtrlAck. Since the current control
          //       procedure is now active, it could falsely detect
          //       rfCounters.numTxCtrlAck, when in fact this was from the
          //       previous control procedure. Consequently, return.
          //
          //       In case (b), the control packet stays at the head of the
          //       queue, and there's nothing more to do. Consequently, return.
          //
          //       So, in either case, return.
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }
        break;
#endif // RTLS_CTE
      /*
      ** Unknown Control Type Received Response
      */
      case LL_CTRL_UNKNOWN_RSP:
        // try to place control packet in the TX FIFO
        // Note: Since there are no dependencies for this control packet, we
        //       do not have to bother with the active flag.
        if ( MAP_llSetupUnknownRsp( connPtr ) == TRUE )
        {
          // all we have to do is put this control packet on the TX FIFO, so
          // remove control packet from the processing queue and drop through
          MAP_llDequeueCtrlPkt( connPtr );
        }
        else // not done yet
        {
          // control packet stays at head of queue, so exit here
          return( LL_CTRL_PROC_STATUS_SUCCESS );
        }
        break;

      /*
      ** Control Internal - Wait for Control ACK
      */
      case LL_CTRL_TERMINATE_RX_WAIT_FOR_TX_ACK:
        // check if the control packet has been ACK'ed (i.e. is not pending)
        // Note: Normally this routine is used for control procedures where
        //       control packets are sent by this role. This is a special case
        //       where a terminate indication was received, but we must as a
        //       central wait for our ACK to be sent before terminating.
#ifdef USE_RCL
        if ( connOutput.nRxCtlAck )
#else
        if ( connOutput.nRxCtrlAck )
#endif
        {
          // yes, so terminate
          // Note: No need to cleanup control packet info as we are done.
          MAP_llConnTerminate( connPtr, connPtr->termInfo.reason );

          return( LL_CTRL_PROC_STATUS_TERMINATE );
        }
        // control packet stays at head of queue, so exit here
        return( LL_CTRL_PROC_STATUS_SUCCESS );

        // Note: Unreachable statement generates compiler warning!
        //break;

      /*
      ** Dummy Place Holder Transmit
      */
      case LL_CTRL_DUMMY_PLACE_HOLDER_TRANSMIT:
        //  dummy packet stays at head of queue, so exit here
        return( LL_CTRL_PROC_STATUS_SUCCESS );

        // Note: Unreachable statement generates compiler warning!
        //break;

      case LL_CTRL_DUMMY_PLACE_HOLDER_TX_PENDING:
        // replace place holder with transmit place holder
        MAP_llReplaceCtrlPkt( connPtr, connPtr->ctrlPktInfo.ctrlPktPending,
                              LL_CTRL_UNDEFINED_PKT );

        // and make it active
        connPtr->ctrlPktInfo.ctrlPktActive = 1;

        break;

      /*
      ** Dummy Place Holder Receive
      */
      case LL_CTRL_DUMMY_PLACE_HOLDER_RECEIVE:
        // check which control packet type is pending processing
        switch( connPtr->ctrlPktInfo.ctrlPktPending )
        {
          case LL_CTRL_CONNECTION_PARAM_REQ:
            // receipt of a Connection Parameter Request control packet could
            // not be processed because some other control procedure was active
            // Note: All other types of collisions have already been handled.

            // replace place holder with transmit place holder
            MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_DUMMY_PLACE_HOLDER_TRANSMIT,
                                  LL_CTRL_UNDEFINED_PKT );

            // pass the parameters to the Host
            MAP_LL_RemoteConnParamReqCback( connPtr->connId,
                                            connPtr->connParams.intervalMin,
                                            connPtr->connParams.intervalMax,
                                            connPtr->connParams.latency,
                                            connPtr->connParams.timeout );
            break;

          default:
            // Sanity Check:
            // Nothing else is expected here!
            LL_ASSERT( FALSE );
            break;
        }

        return( LL_CTRL_PROC_STATUS_SUCCESS );

        // Note: Unreachable statement generates compiler warning!
        //break;

      //
      // Unknown Control Packet
      //
      default:
        // fatal error - a unknown control procedure value was used
        LL_ASSERT( FALSE );

        break;
    }
  }

  return( LL_CTRL_PROC_STATUS_SUCCESS );
}

#endif // INIT_CFG

/*******************************************************************************
 */
