/******************************************************************************

 @file  ll_peripheral_end_causes.c

 @brief This file contains the Link Layer (LL) handlers for the various peripheral
        end causes that result from a PHY task completion. The file also
        contains the peripheral end causes related to connection termination, as
        well as common termination routines used by the peripheral peripheral peripheral peripheral
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

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)

#include "hal_mcu.h"
#include "osal_bufmgr.h"
#include "osal_cbtimer.h"
#include "ble.h"
#include "ll.h"
#include "ll_common.h"
#include "ll_scheduler.h"
#include "ll_timer_drift.h"
#include "ll_enc.h"
#include "ll_rat.h"
#include "ll_config.h"
#include "hal_gpio_wrapper.h"
#include "cs/ll_cs_ctrl_pkt_mgr.h"
#include "cs/ll_cs_procedure.h"
//
#include "rom_jt.h"

// SW Tracer
#ifdef DEBUG_SW_TRACE
#define DBG_ENABLE
#include "dbgid_sys_slv.h"
#endif // DEBUG_SW_TRACE

#ifdef DEBUG_GPIO
uint8 connGPIO[] = { HAL_GPIO_1, HAL_GPIO_2, HAL_GPIO_3, HAL_GPIO_4,
                     HAL_GPIO_5, HAL_GPIO_6, HAL_GPIO_7, HAL_GPIO_8 };
#endif // DEBUG_GPIO

#ifdef LL_TEST_MODE
// Typical case: CI=10ms, SL=20. Every Nth event will miss.
// Note: Connection will be off for one event by 336us, so it can take many
//       events before receive window opens enough for valid AP. Make sure
//       LSTO is sufficiently large (best to just set to 32s).
#define FORCE_MISSED_EVENT_COUNT 10
uint32 forcedMissedEvent = FORCE_MISSED_EVENT_COUNT;
#endif // LL_TEST_MODE

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

#define MAX_LSTO_IN_COARSE_TICKS  51200  // 32s in 625us
#define MIN_CI_IN_COARSE_TICKS    12     // 7.5ms in 625us
#define MAX_LSTO_NUM_OF_EVENTS    (MAX_LSTO_IN_COARSE_TICKS / MIN_CI_IN_COARSE_TICKS)

/*******************************************************************************
 * Prototypes
 */

/*******************************************************************************
 * Functions
 */

/*******************************************************************************
 * @fn          llPeripheral_TaskEnd
 *
 * @brief       This function is used to handle the PHY task done end cause
 *              TASK_ENDOK that can result from one of two causes. First, a
 *              normal connection event closure occurred since the Peripheral
 *              transmitted a packet with MD=0 (i.e. no more Peripheral data) after
 *              having successfully received a packet from the Central with MD=0
 *              (i.e. no more Central data). Second, the Peripheral transmitted a
 *              packet and the BLE_L_CONF.ENDC=1. Since the ENDC bit is
 *              currently not used, only the first case is handled.
 *
 *              This function needs to read and save the anchor point, which is
 *              used for timing the next connection event. All RX FIFO data
 *              (data and control packets) are to be counted (for encryption)
 *              and processed. All data that needs to be sent are placed in the
 *              TX FIFO. The connecting event is counted, and the start of peripheral
 *              latency is checked. If the connection hasn't been terminated
 *              via a control packet request, the next data channel is selected,
 *              and the timing for the start of the next Peripheral tasks it
 *              determined based on the connection interval, relative to the
 *              anchor point.
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
void llPeripheral_TaskEnd( void )
{
  llConnState_t *connPtr;
  uint16         numPkts;
  uint8          connEvtStatus;
  uint8          channel;
#ifdef DEBUG_GPIO_CONN
  GPIO_writeDio(HAL_GPIO_4, 0);
#endif // DEBUG_GPIO_CONN

#if DEBUG
#ifdef DEBUG_SW_TRACE
  DBG_PRINT0(DBGSYS, "");
  DBG_PRINTL1(DBGSYS, "PERIPHERAL END RAT = 0x%08X", MAP_llGetCurrentTime() );
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

  // check if an pending update parameters has been applied
  if ( connPtr->pendingParamUpdate == PARAM_UPDATE_APPLIED )
  {
    // notify the Host if connInterval, connTimeout, or peripheralLatency
    // has been changed by the central
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

  // check if the user wants to be notified that a connection event ended
  // Note: An event of zero means this API is not enabled.
  if ( connPtr->taskEvent != 0 )
  {
    // set the user's event on the user's task
    // Note: There is no sanity checking of valid task ID or event flag!
    MAP_osal_set_event( connPtr->taskID, connPtr->taskEvent );
  }
  // update health check
  MAP_llHealthUpdate(LL_STATE_CONN_PERIPHERAL);

  // advance the connection event count
  connPtr->currentEvent = connPtr->nextEvent;

#ifdef LL_TEST_MODE
    switch( llTestMode.testCase )
    {
      case LL_TEST_MODE_TP_CON_MAS_BI_02:
        if ( connPtr->currentEvent > 10 )
        {
          llClearRxDataEntry(&rxDataQ.multiBuffers, &rxDataQ.finishedBuffers);
          // Align the global Rx buffer list
          llUpdateRxBuffersForActiveConnections(&rxDataQ.multiBuffers);
        }
        break;

      default:
        break;
    }
#endif // LL_TEST_MODE

  // get the total number of received packets
  // Note: Since Auto-Flush is enabled, nRxBufFull is incremented instead of
  //       nRxOk when there's no room in the FIFO. When Auto-Flush is
  //       disabled and there's no room in the FIFO, only nRxBufFull is
  //       incremented for any kind of received packet.
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
  // check if any data has been received
  // Note: numRxOk includes numRxCtrl
  // Note: numRxNotOk removed as 4.5.2 of spec says the LSTO is reset upon
  //       receipt of a "valid packet", which is taken to mean no CRC error.
  if ( connOutput.nRxOk    || connOutput.nRxIgnored ||
       connOutput.nRxEmpty || connOutput.nRxFifoFull )
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
      // In this half LSTO state, the peripheral would have LL_MAX_PERIPHERAL_NUM_LSTO_RETRIES continues retries
      // in order to increase the probability that the SN BIT from the central was changed (and the central heard the peripheral).

      // Perform this only LL_MAX_NUM_LSTO_RETRIES times.
      if (connPtr->numLSTORetries < LL_MAX_PERIPHERAL_NUM_LSTO_RETRIES)
      {
        // Increment the number of retries.
        connPtr->numLSTORetries++;
      }
      // In case reaching LL_MAX_PERIPHERAL_NUM_LSTO_RETRIES.
      else
      {
        // Disable Starvation Mode:
        // - Reset the lsto number of retries.
        // - Reset starvation mode bit.
        MAP_llSetStarvationMode(connPtr->connId, LL_SET_STARVATION_MODE_OFF);
      }
    }//End of Starvation mode handling.

    // Update the supervision expiration count.
    connPtr->expirationEvent = connPtr->currentEvent + connPtr->expirationValue;

    // clear flag that indicates we received first packet
    // Note: The first packet only really needs to be signaled when a new
    //       connection is formed or a connection's parameters are updated.
    //       However, there's no harm in resetting in every time in order to
    //       simplify the control logic.
    // Note: True-Low logic is used here to be consistent with NR's language.
    connPtr->firstPacket = FALSE;

    // peripheral latency may have been disabled because a packet from the central
    // was not received (see Core spec V4.0, Vol 6, Section 4.5.1), so restore
    // the peripheral latency value; but note that if this is the beginning of a
    // connection, you can not allow peripheral latency until the first NESN change
    // has occurred from the central (i.e. until central ACK's peripheral)
    if ( connPtr->peripheralLatencyAllowed == TRUE )
    {
      // activate peripheral latency
      connPtr->peripheralLatency = connPtr->peripheralLatencyValue;
    }
    // Reset DMM threshold
    MAP_llDmmSetThreshold(LL_STATE_CONN_PERIPHERAL,connPtr->connId,TRUE);
  }
  else // either no packets received, or packets received with CRC error
  {
    // Set DMM threshold
    MAP_llDmmSetThreshold(LL_STATE_CONN_PERIPHERAL,connPtr->connId,FALSE);
    // so listen to every event until a packet is received
    connPtr->peripheralLatency = 0;

    // In case the Connection Starvation Mechanism is ON
    // - Reset the lsto number of retries.
    // - Reset starvation mode bit.
    if ((connPtr->numLSTORetries > 0) || (connPtr->StarvationMode == TRUE))
    {
        // Disable Starvation Mode:
        MAP_llSetStarvationMode(connPtr->connId, LL_SET_STARVATION_MODE_OFF);
    }

    // check if we received any packets with a CRC error
    // Note: Spec change (section 4.5.5) indicates any packet received,
    //       regardless of CRC result, determines the anchor point.
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

      // peripheral latency may have been disabled because a packet from the central
      // was not received (see Core spec V4.0, Vol 6, Section 4.5.1), so restore
      // the peripheral latency value; but note that if this is the beginning of a
      // connection, you can not allow peripheral latency until the first NESN change
      // has occurred from the central (i.e. until central ACK's peripheral)
      if ( connPtr->peripheralLatencyAllowed == TRUE )
      {
        // activate peripheral latency
        connPtr->peripheralLatency = connPtr->peripheralLatencyValue;
      }
    }
    else // no packet was received from the central
    {
      connEvtStatus = LL_CONN_EVT_STAT_MISSED;

      // collect packet error information
      connPtr->perInfo.numMissedEvts++;

      HAL_GPIO_SET(connGPIO[connPtr->connId]);
      HAL_GPIO_CLR(connGPIO[connPtr->connId]);
      HAL_GPIO_SET(connGPIO[connPtr->connId]);
      HAL_GPIO_CLR(connGPIO[connPtr->connId]);

      // check if we're still waiting for the first packet from the central.
      // in that case we should increase the timeoutTime to make sure we don't miss the anchor.
      if (connPtr->firstPacket)
      {
        linkCmd[connPtr->connId].relRxTimeoutTime =
                           (2 * connPtr->timerDrift)                                +
                           (2 * LL_JITTER_CORRECTION)                               +
                           LL_RX_RAMP_OVERHEAD                                      +
                           ((uint32)connPtr->curParam.winSize * RAT_TICKS_IN_625US) +
                           LL_RX_SYNCH_OVERHEAD;
      }
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
      /* Peripheral return values on connection termination:
       * If connection created but not established, and received a request with incorrect access address -> return 0x3e
       * If connection created but not established for Direct Advertisement, but no data packets received -> return 0x3e
       * If connection created but not established, and received data packet with CRC -> return 0x8
       * If connection established, and received data packet with incorrect access address -> return 0x08
       * */
      if ( connPtr->perInfo.numPkts == 0 )
      { // this is a failure to establish the connection
        // so terminate with failure to establish connection
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

  // for a new connection, peripheral latency isn't enabled until the central's NESN
  // bit changes, which is equivalent to receiving a TX ACK; if this is an
  // update parameters, peripheral latency is also disabled until any first packet arrives,
  // however, to keep things simple for now, we will use the same ACK constraint
  if ( connOutput.nTxAck > 0 )
  {
    // set a flag to indicates it is now okay to use peripheral latency on this
    // connection, if specified
    // Note: This is now needed due to a change in spec (section 4.5.1) which
    //       requires that peripheral latency be disabled when a packet is not
    //       received from the central. Since this routine is common for END_OK
    //       and RX_TIMEOUT, there's no way to know if the first Central NESN
    //       bit change has taken place. This flag will indicate it has, so if
    //       a central packet is received, the peripheral latency value can be
    //       restored.
    connPtr->peripheralLatencyAllowed = TRUE;

    // only update peripheral latency if no control procedure is active
    // Note: When a control procedure is active, peripheral latency has to be
    //       disabled in case it exceeds the control procedure timeout.
    // Note: Even when a control procedure is active, but a control transaction
    //       timeout isn't used, we can still skip setting SL since that kind
    //       of control procedure wouldn't have disabled peripheral latency to begin
    //       with.
    // ALT: Could reset SL in llProcessPeripheralControlProcedures.
    if ( (connPtr->ctrlPktInfo.ctrlPktActive == FALSE) &&
         (connPtr->pendingParamUpdate == PARAM_UPDATE_NOT_PENDING) &&
         (connPtr->pendingChanUpdate  == FALSE) )
    {
      // at least one ACK, so Peripheral Latency is operational
      // Note: This really only happens once for the very first connection
      //       interval, and when an update parameters procedure is taking place.
      connPtr->peripheralLatency = connPtr->peripheralLatencyValue;
    }
  }

  // check if any update is pending and we're still waiting for the Central
  // confirmation of the Peripheral's ACK
  // Note: Without a counter from the PHY, this has to be done in two stages.
  //       First, we have to ensure the Peripheral ACK'ed the Central's control
  //       packet. Second, we must ensure that the Central is not retransmitting.
  //       That is, a new transmission from the Central means the Central received
  //       the Peripheral's ACK. Unfortunately, it's possible we are delaying SL
  //       for retransmissions not due to the update control packet, but due to
  //       subsequent packets that happen to occur in the same connection event.
  //       This cannot be avoided, and at worst, we wasted some power until the
  //       update instant is reached.
  if ( connPtr->updateSLPending != UPDATE_PL_OKAY )
  {
    // check if the Peripheral has ACK'ed the Update and no Central retransmissions
    if ( (connPtr->updateSLPending == UPDATE_NEW_TRANS_PENDING) &&
         (connOutput.nRxIgnored == 0) )
    {
      // allow peripheral latency based on Central confirming Peripheral Ack to update
      connPtr->updateSLPending = UPDATE_PL_OKAY;
    }
    // check if the Peripheral has ACK'ed the update request
    else if ( connOutput.nRxCtlAck != 0 )
    {
      // indicate udpate control packet has been ACK'ed
      connPtr->updateSLPending = UPDATE_NEW_TRANS_PENDING;
    }
  }

  // check if the radio performed an anchor point capture
  // Note: This bit is cleared at the start of a new task.
  // Note: The anchor capture will occur even if the RX FIFO was too full to
  //       accept the packet.
  if ( connOutput.anchorValid )
  {
    //GPIO_writeDio(HAL_GPIO_2, 1);

    // read/save the the anchor point capture from RAT
    connPtr->llTask->anchorPoint = connOutput.anchorPoint;

#ifdef CC23X0
    /* TODO: Find characterized value */
#else
#if defined(CC26X2) || defined(CC13X2) || defined(CC13X4)
    // Agama Timestamp Adjustment
    // This magic number is derived from two changes that result from
    // improvements to calibration (based on override settings). The first is
    // the startSynthToRat adjusts from 256us to 166us, resulting in a delay
    // of +90us. The second affects the pilot tone duration, which was reduced
    // from 30us for uncoded and 24 us for coded to 12us for both PHYs. The
    // net increase to the AP is 72us for uncoded, and 78us for coded. However,
    // at this point, it is easier to simply start the AP a bit earlier.
    connPtr->llTask->anchorPoint += RAT_TICKS_IN_72US;
#elif defined(CC13X2P)
    // For CC13X2P, the pilot tone duration was returned to 30us.
    connPtr->llTask->anchorPoint += RAT_TICKS_IN_90US;
#endif // CC26X2 ||CC13X2 || CC13X4
#endif
    // clear window widening
    linkCmd[connPtr->connId].relRxTimeoutTime = 0;
#ifdef DEBUG_SW_TRACE
    DBG_PRINT0(DBGSYS, "");
    DBG_PRINTL1(DBGSYS, "PERIPHERAL AP VALID = 0x%08X", connPtr->llTask->anchorPoint );
    DBG_PRINT0(DBGSYS, "");
#endif // DEBUG_SW_TRACE

    //GPIO_writeDio(HAL_GPIO_2, 0);
  }
  else // invalid anchor point due to RX Timeout
  {
    //GPIO_writeDio(HAL_GPIO_3, 1);

    // save last Timeout only when a miss has occurred to preserve timeoutTime
    connPtr->lastTimeoutTime = linkCmd[connPtr->connId].relRxTimeoutTime;

    // set the AP to last known start time
    connPtr->llTask->anchorPoint = linkCmd[connPtr->connId].common.timing.absStartTime;

    // disable peripheral latency
    connPtr->peripheralLatency = 0;
    connPtr->peripheralLatencyAllowed = FALSE;

#ifdef DEBUG_SW_TRACE
    DBG_PRINT0(DBGSYS, "");
    DBG_PRINT0(DBGSYS, "PERIPHERAL AP INVALID!!!!");
    DBG_PRINT0(DBGSYS, "");
#endif // DEBUG_SW_TRACE

    //GPIO_writeDio(HAL_GPIO_3, 0);
  }
#ifndef CC23X0
  // obtain the RSSI, if present
  connPtr->lastRssi = (RSSI_SUFFIX_PRESENT() && (connOutput.lastRssi != LL_RF_RSSI_UNDEFINED))?connOutput.lastRssi:LL_RF_RSSI_INVALID;
#else
  connPtr->lastRssi = (LRF_RSSI_INVALID == connOutput.lastRssi) ? LL_RF_RSSI_INVALID : connOutput.lastRssi;
#endif // CC23X0

#ifdef RTLS_CTE
  // get the CTE information in case received CTE response packet
  if (llCteSamples.autoCopyCompleted > 0)
  {
    MAP_llGetCteInfo( CTE_TASK_ID_CONNECTION, connPtr );
  }
#endif // RTLS_CTE

  // Check if it's time to build the CS StepList
  MAP_llCsStartStepListGen(connPtr);
  // Check if it's time to begin the CS procedure
  MAP_llCsStartProcedure(connPtr);

  // check Control Procedure Processing
  if ( MAP_llProcessPeripheralControlProcedures( connPtr ) == LL_CTRL_PROC_STATUS_TERMINATE )
  {
    // this connection is terminated, so nothing to schedule
    return;
  }

  // Processing Tx data (if any)
  MAP_llProcessTxData( connPtr, LL_TX_DATA_CONTEXT_POST_PROCESSING );

  //align the RX buffers head and tail pointers with all other active connections
  llUpdateRxBuffersForActiveConnections(&rxDataQ.multiBuffers);

  // Send the callback before calculating the next channel
  llSendConnEvtCallback(connEvtStatus, numPkts, connPtr);

  // update next event, calculate time to next event, calculate timer drift,
  // update anchor points, setup NR T2E1 and T2E2 events
  if ( MAP_llSetupNextPeripheralEvent() == LL_SETUP_NEXT_LINK_STATUS_TERMINATE )
  {
    // this connection is terminated, so nothing to schedule
    return;
  }

  // update CTE state
#ifdef RTLS_CTE
  MAP_llUpdateCteState( connPtr );
#endif // RTLS_CTE

  // determine next task (if any) and schedule it
  MAP_llScheduler();

  return;
}

/*******************************************************************************
 * @fn          llSetupNextPeripheralEvent
 *
 * @brief       This function is used to setup the next Timer 2 Event 1 and
 *              Event 2 times, checks if an Update Parameters and/or an
 *              Update Data Channel has occurred, and sets the next data
 *              channel.
 *
 *              Side Effects:
 *              t2e1
 *              t2e2
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Indicates whether a link terminate occurred:
 *              LL_SETUP_NEXT_LINK_STATUS_TERMINATE: Terminate due to a LSTO.
 *              LL_SETUP_NEXT_LINK_STATUS_SUCCESS: Do not terminate.
 */
uint8 llSetupNextPeripheralEvent( void )
{
  llConnState_t *connPtr;
  uint32         timeToNextEvt;
#if !defined(DISABLE_RCOSC_SW_FIX)
  static uint16  applyExtraSCA = 0;
#endif // !DISABLE_RCOSC_SW_FIX
  uint8_t phyWasChanged = LL_PHY_NONE;

  LL_ASSERT( llConns.currentConn != LL_INVALID_CONNECTION_ID );

  // get pointer to connection info
  connPtr = MAP_llDataGetConnPtr( llConns.currentConn );

  /*
  ** Check for a Update RX Buffers length
  **
  */
  if ( TST_FEATURE_FLAG( connPtr->lenInfo.lenFlags, REPLACE_RX_BUFFERS ) )
  {
    // Clear the flag
    CLR_FEATURE_FLAG( connPtr->lenInfo.lenFlags, REPLACE_RX_BUFFERS );
    // Check for update the RX buffers
    MAP_llReplaceRxBuffers( connPtr );
  }

  // check if there's any TX data pending, and if so, disable peripheral latency
  if ( RCL_TxBuffer_head(((txDataQ_t *)connPtr->pTxDataEntryQ)->rfDataBuffers) != NULL )
  {
    connPtr->peripheralLatency = 0;
  }
  else // restore peripheral latency
  {
    // but only if enabled and no updates or control procedures are pending
    // and the first NESN change has occurred from the central (i.e. the Central
    // has ACK'ed the Peripheral)
    if ( (connPtr->ctrlPktInfo.ctrlPktActive == FALSE)             &&
         (connPtr->pendingParamUpdate        == PARAM_UPDATE_NOT_PENDING) &&
         (connPtr->pendingChanUpdate         == FALSE)                    &&
         (connPtr->pendingPhyUpdate          == FALSE)                    &&
         (connPtr->peripheralLatencyAllowed       == TRUE) )
    {
      // activate peripheral latency
      connPtr->peripheralLatency = connPtr->peripheralLatencyValue;
    }
  }

  // update the next connection event count, taking peripheral latency into account
  connPtr->nextEvent = connPtr->currentEvent + connPtr->peripheralLatency + (uint8)1;

  // check if the supervision timeout is going to occur while we are in peripheral
  // latency, or the user has suspended peripheral latency
  // Note: The check for LSTO during peripheral latency requires that nextEvent was
  //       already updated based on peripheral latency!
  if ( (MAP_llCheckForLstoDuringSL(connPtr) == TRUE) ||
       (slOverride == LL_EXT_ENABLE_PL_OVERRIDE) )
  {
    // yes, so disable peripheral latency to ensure we don't miss the LSTO
    connPtr->peripheralLatency = 0;

    // and set the next event to +1 the current event
    connPtr->nextEvent = connPtr->currentEvent + (uint8)1;
  }

  /*
  ** Check for a Parameter Update
  **
  ** Note: The Parameters Update must come before the Data Channel Update in
  **       case the next event count is changed.
  */

  // check if there's a upcoming connection parameter update
  if ( (connPtr->pendingParamUpdate == PARAM_UPDATE_PENDING)  &&
       (MAP_llEventInRange( connPtr->currentEvent,
                            connPtr->nextEvent,
                            connPtr->paramUpdateEvent)) )
  {
    // check that the next event number and the update parameter event number
    // are not the next event (i.e. current event plus one)
    if ( (connPtr->nextEvent != ((connPtr->currentEvent+1) & 0xFFFF)) &&
         (connPtr->paramUpdateEvent != ((connPtr->currentEvent+1) & 0xFFFF)) )
    {
      // override the next active event to the event before the instant
      connPtr->nextEvent = connPtr->paramUpdateEvent - 1;

      // and allow sleep up till one event before the instant
      connPtr->peripheralLatency = MAP_llEventDelta( connPtr->nextEvent,
                                                connPtr->currentEvent ) - 1;
    }
  }
  // check if there's a pending update to the data channel for next event and
  // whether the update event count is prior to the next active event count
  else if ( ( connPtr->pendingChanUpdate == TRUE ) &&
            ( MAP_llEventInRange( connPtr->currentEvent,
                                  connPtr->nextEvent,
                                  connPtr->chanMapUpdateEvent ) ) )
  {
    // check that the next event number and the update parameter event number
    // are not the next event (i.e. current event plus one)
    if ( (connPtr->nextEvent != ((connPtr->currentEvent+1) & 0xFFFF)) &&
         (connPtr->chanMapUpdateEvent != ((connPtr->currentEvent+1) & 0xFFFF)) )
    {
      // override the next active event to the event before the instant
      connPtr->nextEvent = connPtr->chanMapUpdateEvent - 1;

      // and allow sleep up till one event before the instant
      connPtr->peripheralLatency = MAP_llEventDelta( connPtr->nextEvent,
                                                connPtr->currentEvent ) - 1;
    }
  }
  else if ( ( connPtr->pendingPhyUpdate == TRUE ) &&
            ( MAP_llEventInRange( connPtr->currentEvent,
                                  connPtr->nextEvent,
                                  connPtr->phyUpdateEvent ) ) )
  {
    // check that the next event number and the update parameter event number
    // are not the next event (i.e. current event plus one)
    if ( (connPtr->nextEvent      != ((connPtr->currentEvent+1) & 0xFFFF)) &&
         (connPtr->phyUpdateEvent != ((connPtr->currentEvent+1) & 0xFFFF)) )
    {
      // override the next active event to the event before the instant
      connPtr->nextEvent = connPtr->phyUpdateEvent - 1;

      // and allow sleep up till one event before the instant
      connPtr->peripheralLatency = MAP_llEventDelta( connPtr->nextEvent,
                                                connPtr->currentEvent ) - 1;
    }
  }

  // check if it is time for the PHY update
  if ( (connPtr->pendingPhyUpdate == TRUE) &&
       (connPtr->phyUpdateEvent <= ((connPtr->currentEvent+1) & 0xFFFF)) )
  {
    // set the current phy to the updated phy on the next event
    connPtr->phyInfo.curPhy = connPtr->phyInfo.updatePhy;

    // clear the PHY update flag
    // Note: The actual PHY change will occur when the BLE task is scheduled.
    connPtr->pendingPhyUpdate = PHY_UPDATE_APPLIED;
    phyWasChanged = connPtr->phyInfo.curPhy;

    // notify the Host
    MAP_LL_PhyUpdateCompleteEventCback( LL_STATUS_SUCCESS,
                                        connPtr->connId,
                                        connPtr->phyInfo.curPhy,
                                        connPtr->phyInfo.curPhy );
  }

#ifdef LL_TEST_MODE
      switch( llTestMode.testCase )
      {
        case LL_TEST_MODE_JIRA_220:
         // check if there's a connection parameter udpate to the connection, and if
         // so, check if the update event is before or equal to the next active event
         if ( (connPtr->pendingParamUpdate == PARAM_UPDATE_PENDING)  &&
              (connPtr->paramUpdateEvent == ((connPtr->currentEvent+2) & 0xFFFF)) )
         {
           MAP_llEnqueueCtrlPkt( connPtr, LL_CTRL_TERMINATE_IND );
         }

        default:
          break;
      }
#endif // LL_TEST_MODE

  // override peripheral latency until Central confirmation of Peripheral ACK is received
  if ( connPtr->updateSLPending != UPDATE_PL_OKAY )
  {
    // yes, so disable peripheral latency to ensure we don't miss it
    connPtr->peripheralLatency = 0;

    // and set the next event to +1 the current event
    connPtr->nextEvent = connPtr->currentEvent + (uint8)1;
  }

  // check if there's a connection parameter udpate to the connection, and if
  // so, check if the update event is before or equal to the next active event
  if ( (connPtr->pendingParamUpdate == PARAM_UPDATE_PENDING)  &&
       (connPtr->paramUpdateEvent == ((connPtr->currentEvent+1) & 0xFFFF)) )
  {
    // override the next active event to be the update event
    connPtr->nextEvent = connPtr->paramUpdateEvent;
    // find the number of events between this event and the update parameter
    // event, based on the original connection interval
    // Note: The old connection interval must be used!
    timeToNextEvt = ( (uint32)MAP_llEventDelta( connPtr->paramUpdateEvent,
                                                connPtr->currentEvent ) *
                                                (uint32)connPtr->curParam.connInterval ) +
                                                (uint32)connPtr->paramUpdate.winOffset;
    llApplyParamUpdate(connPtr);
  }
  else // no parameter update, so...
  {
    // time to next event is just the connection interval in 625us ticks
    // Note: Peripheral latency will be taken into account below.
    timeToNextEvt = connPtr->curParam.connInterval;
  }

  // account for peripheral latency in 625us ticks
  timeToNextEvt *= ((uint32)connPtr->peripheralLatency + 1);

  // advance the anchor point in RAT ticks
  connPtr->llTask->anchorPoint += (timeToNextEvt * RAT_TICKS_IN_625US);

#ifdef DEBUG_SW_TRACE
  DBG_PRINT0(DBGSYS, "");
  DBG_PRINTL1(DBGSYS, "PERIPHERAL Time to Next = 0x%08X", timeToNextEvt*RAT_TICKS_IN_625US );
  DBG_PRINTL1(DBGSYS, "PERIPHERAL Time to Next Start = 0x%08X", connPtr->llTask->anchorPoint );
  DBG_PRINT0(DBGSYS, "");
#endif // DEBUG_SW_TRACE

  // Calculate Timer Drift Based on Time to Next Event
  // Note: SCA Factor is in PPM, and time to next event is in 625us ticks.
  //       The result is the timer drift in RAT ticks.
  // Note: Round up one RAT tick to account for floor effect.
#if !defined(DISABLE_RCOSC_SW_FIX)
  // check if the RCOSC is being used as the source clock
#ifdef CC23X0
  if (llUserConfig.useSrcClkLFOSC)
#else
  if ( ((*sclkSrc & SCLK_LF_MASK) >> 6) == SCLK_LF_RCOSC_LF )
#endif
  {
    // check if we received an anchor point or not
    if ( connOutput.anchorValid )
    {
      applyExtraSCA = 0;

      connPtr->timerDrift = ( (timeToNextEvt * connPtr->scaFactor) /
                              RAT_TICKS_IN_100US ) + 1;
    }
    else // RxTimeout
    {
      // Note: Max possible event count before LSTO: 32s/7.5ms=4267 events.
      applyExtraSCA++;

      // Note: Correction applied on the first miss only!
      // Note: Central SCA included in correction.
      connPtr->timerDrift = ( (timeToNextEvt * ((applyExtraSCA == 1)                              ?
                                                (llUserConfig.cfgLFOSCExtraPPM + connPtr->mstSCA) :
                                                (connPtr->scaFactor)))                            /
                                                 RAT_TICKS_IN_100US ) + 1;
    }
  }
  else // RCOSC is not being used as SCLK
#endif // !DISABLE_RCOSC_SW_FIX
  {
    // calculate/recalculate timer drift conditionally(?)
    // Note: It is only necessary to recalculate timer drift if the time to next
    //       event changes. This is based on the connection interval and peripheral
    //       latency. This can happen when the connection is formed, from an
    //       update parameter control procedure, or from anything that affects
    //       whether peripheral latency is to be used (assuming it is non-zero). For
    //       example, whether or not there is data to transmit.
    //if ( ((connPtr->lastTimeToNextEvt != timeToNextEvt) ||
    //      (connPtr->lastPeripheralLatency != connPtr->peripheralLatency)) )
    // ALT: Can always do this on ARM, so can get rid of "last" variables.
    {
      connPtr->timerDrift = ( (timeToNextEvt * connPtr->scaFactor) /
                               RAT_TICKS_IN_100US ) + 1;
    }
  }

  // save time to next event for next connection event
  connPtr->lastTimeToNextEvt = timeToNextEvt;

  // update last peripheral latency used
  connPtr->lastPeripheralLatency = connPtr->peripheralLatency;

  // save the start time as the last event start time used
  // Note: This is needed to handle sending TX data during long event intervals
  //       due to peripheral latency. It is used to figure out where the next event
  //       time is between the previous event and the next event (i.e. the event
  //       time given by (SL+1)*CI).
  connPtr->llTask->lastStartTime = linkCmd[connPtr->connId].common.timing.absStartTime;
  linkCmd[connPtr->connId].common.timing.absStartTime = connPtr->llTask->anchorPoint -
                                                        connPtr->timerDrift;

  connPtr->llTask->startTime = linkCmd[connPtr->connId].common.timing.absStartTime;
  // clear command status value
  linkCmd[connPtr->connId].common.status = RCL_CommandStatus_Idle;
  // setup the receiver Timeout time
  // Note: If the AP is valid, then timeoutTime was previously cleared and any
  //       previous window widening accumulation was therefore reset to zero.
  // Note: Timeout trigger remains as it was when connection was formed.
  linkCmd[connPtr->connId].relRxTimeoutTime += (2 * connPtr->timerDrift);
  // add the window size if a new connection or update connection is pending
  if ( connPtr->pendingParamUpdate == PARAM_UPDATE_APPLIED )
  {
    linkCmd[connPtr->connId].relRxTimeoutTime +=
      ( (uint32)connPtr->curParam.winSize * RAT_TICKS_IN_625US );
  }
  if ( connOutput.anchorValid )
  {
    // account for radio startup overhead and jitter per the spec
    linkCmd[connPtr->connId].common.timing.absStartTime -= (LL_RX_RAMP_OVERHEAD + LL_JITTER_CORRECTION);

    connPtr->llTask->startTime = linkCmd[connPtr->connId].common.timing.absStartTime;

    // override the lastStartTime based on this valid AP
    // Note: Even though the post-processing associated with this connection
    //       resulted in a Hit, it could have been based on a start time that
    //       had been adjusted by many missed/skipped events. If so, then the
    //       start time could be off when used as the lastST in subsequent
    //       missed/skipped events as the Central's AP would have moved and this
    //       would not have been takeen into account.
    connPtr->llTask->lastStartTime = connPtr->llTask->startTime -
                                     (timeToNextEvt * RAT_TICKS_IN_625US);
    // additional widening based on whether the AP is valid
    // Note: The overhead to receive a preamble and synch word to detect a
    //       packet is added in case a packet arrives right at the end of the
    //       receive window.
    linkCmd[connPtr->connId].relRxTimeoutTime += LL_RX_RAMP_OVERHEAD        +
                                              (2 * LL_JITTER_CORRECTION) +
                                              LL_RX_SYNCH_OVERHEAD;

    // check if we're using coded
    if ( connPtr->phyInfo.curPhy == LL_PHY_CODED )
    {
      // adjust backend of Rx window based on PHY
      linkCmd[connPtr->connId].relRxTimeoutTime += LL_RX_SYNCH_OVERHEAD_CODED;
    }

    // override last Timeout to current timeout time
    // Note: Without updating lastTimeoutTime here, we could end up with an
    //       ever increasing timeoutTime. This could happen when a hit occurs
    //       after a series of of missed events. When a post-processing finally
    //       occurs, we save lastTimeoutTime, which would a large value due to
    //       all the previous missed events. Then timeoutTime would be updated.
    //       If this were followed by a series of connection realignments, then
    //       timeoutTime would be updated with an unsually large value. What's
    //       more, under the right circumstances, it would never be reset to
    //       the original base value even when hits occur, eventually leading
    //       to a disconnect due to the below check after a miss.
    connPtr->lastTimeoutTime = linkCmd[connPtr->connId].relRxTimeoutTime;
  }
  else // RX Timeout
  {
    // Note: If we are in a new connection or at the start of an Update
    //       Parameter control procedure, and firstPacket has not been received,
    //       the timer drift is recalcualted based on the connection interval
    //       (the new one for an Update Parameter control procedure), and the
    //       rxTimeout still includes the window size, so this does not have to
    //       be added in again.

    // check if the window is wider than 1/2 the connection interval, per spec
    // Note: Since we're comparing 1/2 Rx window to 1/2 CI, might as well
    //       skip the divide by two, and compare directly.
    // Note: Don't want to include the overhead pad for RX window, so take it
    //       out before the compare to (CI - T_IFS).

    // we should add the sync coded over head to the timeout to insure the followed condition
    // in case we just changed the phy to coded and we got rx timeout
    if ( phyWasChanged == LL_PHY_CODED )
    {
      // adjust backend of Rx window based on PHY
      linkCmd[connPtr->connId].relRxTimeoutTime += LL_RX_SYNCH_OVERHEAD_CODED;
    }
#ifndef CC23X0
    // check if we're using coded
    if ( (linkCmd[connPtr->connId].relRxTimeoutTime -
          (LL_RX_RAMP_OVERHEAD  +
           LL_RX_SYNCH_OVERHEAD +
           ((connPtr->phyInfo.curPhy == LL_PHY_CODED)?LL_RX_SYNCH_OVERHEAD_CODED:0))) >=
         ((connPtr->curParam.connInterval * RAT_TICKS_IN_625US) - RAT_TICKS_IN_150US) )
    {
      // yes, so terminate immediately as the connection establishment failed
      MAP_llConnTerminate( connPtr, LL_SUPERVISION_TIMEOUT_TERM );

      return( LL_SETUP_NEXT_LINK_STATUS_TERMINATE );
    }
#endif
  }
  // setup the connection event End Time relative to the timestamp
    linkCmd[connPtr->connId].common.timing.relHardStopTime =
      ((((uint32)connPtr->curParam.connInterval * *llConfigTable.connEvtCutoff) / 100) * RAT_TICKS_IN_625US) -
      (2 * RAT_TICKS_IN_150US);

  // set Tx power for this command
  // Note: A value of zero means use default Tx power from Radio Setup.
  llSetPower((uint32 *)&linkCmd[connPtr->connId], curTxPowerVal, RfBleDpl_getTxPower(curTxPowerVal));

  // set PHY mode
  // Note: The user interface uses 0 to mean No Phy, while the radio command
  //       uses 0 to mean 1M. At this point, the curPhy should never be
  //       LL_PHY_NONE (and we'll use an assert to check that).
  LL_ASSERT( connPtr->phyInfo.curPhy != LL_PHY_NONE );

  RfBleDpl_setConnPhy(connPtr->connId, connPtr->phyInfo.curPhy, connPtr->phyInfo.phyOpts);
  llSetRangeDelay(connPtr);

  // pointer to first radio operation command
  connPtr->llTask->command = (uint32)&linkCmd[connPtr->connId];

  // Note: Output Parameter Counters are clearned in llScheduleTask!

  // set state
  llState = LL_STATE_CONN_PERIPHERAL;

  // check for a Data Channel Update and calculate and set next data channel
  // Note: The Data Channel Update must come after the Parameters Update in
  //       case the latter updates the next event count.
  MAP_llSetNextDataChan( connPtr );

  return( LL_SETUP_NEXT_LINK_STATUS_SUCCESS );
}


/*******************************************************************************
 * @fn          llProcessPeripheralControlProcedures
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
 * @return      Status of control procedure processing, which can be:
 *              LL_CTRL_PROC_STATUS_SUCCESS: Continue normally.
 *              LL_CTRL_PROC_STATUS_TERMINATE: We have terminated.
 */
uint8 llProcessPeripheralControlProcedures( llConnState_t *connPtr )
{
  // Sanity Check
  if ( connPtr == NULL )
  {
    LL_ASSERT( connPtr != NULL );
    return UFAILURE;
  }

  uint8 status = USUCCESS;
  // check if there are any control packets ready for processing
  while (( status == USUCCESS ) && ( connPtr->ctrlPktInfo.ctrlPktCount > 0 ))
  {
    // processing based on control packet type at the head of the queue
    if ((connPtr->ctrlPktInfo.ctrlPkts[0] >= LL_CTRL_CS_SEC_RSP) &&
        (connPtr->ctrlPktInfo.ctrlPkts[0] <= LL_CTRL_CS_SEC_REQ) )
    {
        if(LL_CTRL_PROC_STATUS_SUCCESS == MAP_llCsProcessCsCtrlProcedures(connPtr, connPtr->ctrlPktInfo.ctrlPkts[0]))
        {
           return LL_CTRL_PROC_STATUS_SUCCESS;
        }
    }
    switch( connPtr->ctrlPktInfo.ctrlPkts[0] )
    {
      /*
      ** Terminated Indication
      */
      case LL_CTRL_TERMINATE_IND:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // we have already place packet on TX FIFO, so check if its been ACK'ed
          if ( connOutput.nTxCtlAck )
          {
            // yes, so process the termination
            // Note: No need to cleanup control packet info as we are done.
            MAP_llConnTerminate( connPtr, LL_HOST_REQUESTED_TERM );

            return( LL_CTRL_PROC_STATUS_TERMINATE );
          }
          else // no done yet
          {
            // check if a termination control procedure timeout has occurred
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
              MAP_llConnTerminate( connPtr, LL_CTRL_PKT_TIMEOUT_HOST_TERM );

              return( LL_CTRL_PROC_STATUS_TERMINATE );
            }
            else // no control procedure timeout yet
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        else // control packet has not been put on the TX FIFO yet
        {
          // so try to put it there; being active depends on a success
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupCtrlPkt( connPtr, LL_CTRL_TERMINATE_IND);

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

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
      /*
      ** Encryption Response
      */
      case LL_CTRL_ENC_RSP:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
#ifdef LL_TEST_MODE
          if ( llTestMode.testCase == LL_TEST_MODE_TP_SEC_MAS_BV_14 )
          {
            MAP_llSetupCtrlPkt( connPtr, LL_CTRL_VERSION_IND);
          }
          else if ( llTestMode.testCase == LL_TEST_MODE_TP_SEC_MAS_BI_09 )
          {
            uint8 *buf = (uint8 *)LL_TX_bm_alloc( 4 );

            // set the L2CAP header with the length and connection ID
            // Note: The L2CAP Header Length is -4 the payload size.
            *((uint32 *)buf) = 0;

            // allow the data packet to get queued
            connPtr->txDataEnabled = TRUE;

            MAP_LL_TxData( connPtr->connId, buf, 4, LL_DATA_FIRST_PKT_HOST_TO_CTRL );
          }
#endif // LL_TEST_MODE

          // yes, so check if it has been transmitted yet
          // Note: This does not mean this packet has been ACK'ed or NACK'ed.
          if ( connOutput.nTxCtl )
          {
            // set flag to discard all incoming data transmissions
            connPtr->rxDataEnabled = FALSE;

            // done with this control packet, but leave a dummy packet at the
            // head of the queue to prevent another control packet from
            // interleaving this control procedure
            MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_DUMMY_PLACE_HOLDER_TRANSMIT,
                                  LL_CTRL_UNDEFINED_PKT );

            // notify the Host with RAND and EDIV after sending the RSP
            // Note: Need to wait for the Host reply to determine if the LTK
            //       is available or not.
            MAP_LL_EncLtkReqCback( connPtr->connId,
                                   connPtr->encInfo.RAND,
                                   connPtr->encInfo.EDIV );
          }
          else // not done yet
          {
            // check if a update param req control procedure timeout has occurred
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
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupCtrlPkt( connPtr, LL_CTRL_ENC_RSP);

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
#endif // ADV_CONN_CFG

      /*
      ** Start Encryption Request
      */
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & ADV_CONN_CFG)
      case LL_CTRL_START_ENC_REQ:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // yes, so check if it has been transmitted yet
          // Note: This only means the packet has been transmitted, not that it
          //       has been ACK'ed or NACK'ed.
          if ( connOutput.nTxCtl )
          {
            // enable encryption once start encryption request is sent
            // Note: We can not receive data once the encryption control
            //       procedure has begun, so there is no risk of a race
            //       condition here.
            connPtr->encEnabled = TRUE;

            /**** UPDATE DEBUG INFO MODULE ****/
            (void)MAP_DbgInf_addConnEst(connPtr->connId, HCI_EVT_PERIPHERAL_ROLE, UTRUE);

            // clear packet counters
            connPtr->encInfo.txPktCount = 0;
            connPtr->encInfo.rxPktCount = 0;
          }

          // not done until the LL_CTRL_START_ENC_RSP is received, so check it
          // Note: The following code can not be in the previous "if" statement
          //       since it is possible that numTxCtrl could be true, yet the
          //       flag startEncRspRcved isn't. Then on the next event,
          //       numTxCtrl wouldn't be true, and we would never check the
          //       startEncRspRcved flag again. Since we can't get the
          //       LL_START_ENC_RSP until we send the LL_CTRL_START_ENC_REQ,
          //       this isn't an issue.
          if ( connPtr->encInfo.startEncRspRcved == TRUE )
          {
            // replace control procedure at head of queue to prevent interleaving
            MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_START_ENC_RSP,
                                  LL_CTRL_UNDEFINED_PKT );
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
          // first, check if the SK has been calculated
          if ( connPtr->encInfo.SKValid == TRUE )
          {
            // so try to begin the last step of the encryption procedure
            if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_START_ENC_REQ) == TRUE )
            {
              // ready the flag that indicates that we've received the response
              connPtr->encInfo.startEncRspRcved = FALSE;

              // the control packet is now active
              connPtr->ctrlPktInfo.ctrlPktActive = TRUE;
            }

            // Note: Two cases are possible:
            //       a) We successfully placed the packet in the TX FIFO.
            //       b) We did not.
            //
            //       In case (a), it may be possible that a previously just
            //       completed control packet happened to complete based on
            //       rfCounters.numTxCtrl. Since the current control
            //       procedure is now active, it could falsely detect
            //       rfCounters.numTxCtrl, when in fact this was from the
            //       previous control procedure. Consequently, return.
            //
            //       In case (b), the control packet stays at the head of the
            //       queue, and there's nothing more to do. Consequently, return.
            //
            //       So, in either case, return.
            return( LL_CTRL_PROC_STATUS_SUCCESS );
          }
          else // SK isn't valid yet, so see if we've received the LTK yet
          {
            if ( connPtr->encInfo.LTKValid )
            {
              // generate the Session Key (i.e. SK = AES128(LTK, SKD))
              LL_ENC_GenerateSK( connPtr->encInfo.LTK,
                                 connPtr->encInfo.SKD,
                                 connPtr->encInfo.SK );

              // indicate the SK is valid, and drop through
              connPtr->encInfo.SKValid = TRUE;
            }
            else // not done yet
            {
              //  control packet stays at head of queue, so exit here
              return( LL_CTRL_PROC_STATUS_SUCCESS );
            }
          }
        }
        break;
#endif // ADV_CONN_CFG

      /*
      ** Start Encryption Response
      */
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
      case LL_CTRL_START_ENC_RSP:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // yes, so check if it has been transmitted yet
          // Note: This only means the packet has been transmitted, not that it
          //       has been ACK'ed or NACK'ed.
          if ( connOutput.nTxCtl )
          {
            // packet TX'ed, so we are done with the encryption procedure

            // re-activate peripheral latency
            connPtr->peripheralLatency = connPtr->peripheralLatencyValue;

            // remove control packet from processing queue and drop through
            MAP_llDequeueCtrlPkt( connPtr );

#ifdef LL_TEST_MODE
            if ( (llTestMode.testCase == LL_TEST_MODE_TP_CON_MAS_BV_28) ||
                 (llTestMode.testCase == LL_TEST_MODE_TP_SEC_MAS_BV_12) ||
                 (llTestMode.testCase == LL_TEST_MODE_TP_SEC_MAS_BV_13) )
            {
              // connection parameter request, version indication sent, or
              // peripheral feature request sent, so don't send again
              connPtr->ctrlPktInfo.ctrlPktActive = 1;
            }
#endif // LL_TEST_MODE

            // set flag to allow outgoing data transmissions
            connPtr->txDataEnabled = TRUE;

            // return any data entries that were stalled on temp queue to the RF queue
            MAP_llMoveTempTxDataEntries( connPtr );

            // okay to receive data again
            connPtr->rxDataEnabled = TRUE;

            // notify the Host
            if ( connPtr->encInfo.encRestart == TRUE )
            {
              // check if the Ping Feature is a supported feature set item
              if ( (connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_PING) )
              {
                // reset the expiration toggle flag
                connPtr->numAptoExp = 0;

                // re-start encryption, so just update timer
                MAP_osal_CbTimerUpdate( connPtr->aptoTimerId,
                                        (connPtr->aptoValue / 2) );
              }

              // a key change was requested
              MAP_LL_EncKeyRefreshCback( connPtr->connId,
                                         LL_ENC_KEY_REQ_ACCEPTED );

#ifdef LL_TEST_MODE
              if ( llTestMode.testCase == LL_TEST_MODE_TP_SEC_MAS_BV05 )
              {
                uint8 *buf = (uint8 *)LL_TX_bm_alloc( 1 );

                // send one byte per the spec
                *buf = 0x11;

                LL_TxData( connPtr->connId, buf, 1, LL_DATA_FIRST_PKT_HOST_TO_CTRL );
              }
#endif // LL_TEST_MODE

            }
            else // first time encryption
            {
              // check if the Ping Feature is a supported feature set item
              if ( (connPtr->featureSetInfo.featureSet[0] & LL_FEATURE_PING) )
              {
                // restart the APTO timer
                MAP_osal_CbTimerStart( MAP_llCBTimer_AptoExpiredCback,
                                       (uint8 *)connPtr,
                                       (connPtr->aptoValue / 2),
                                       &connPtr->aptoTimerId );
              }

              // a new encryption was requested
              MAP_LL_EncChangeCback( connPtr->connId,
                                     LL_ENC_KEY_REQ_ACCEPTED,
                                     LL_ENCRYPTION_ON );
            }

            // clear the restart flag in case of another key change request,
            // and all other encryption flags
            // Note: But in reality, there isn't a disable encryption in BLE,
            //       so once encryption is enabled, any call to LL_StartEncrypt
            //       will result in an encryption key change callback.
            connPtr->encInfo.encRestart       = FALSE;
            connPtr->encInfo.encReqRcved      = FALSE;
            connPtr->encInfo.pauseEncRspRcved = FALSE;
            connPtr->encInfo.startEncRspRcved = FALSE;
          }
          else // not done yet
          {
            // check if a update param req control procedure timeout has occurred
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
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupCtrlPkt( connPtr, LL_CTRL_START_ENC_RSP);

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
      ** Pause Encryption Response
      */
#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_CONN_CFG | INIT_CFG))
      case LL_CTRL_PAUSE_ENC_RSP:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // not done until the LL_CTRL_PAUSE_ENC_RSP is received, so check it
          if ( connPtr->encInfo.pauseEncRspRcved == TRUE )
          {
            // done with this control packet, but leave a dummy packet at the
            // head of the queue to prevent another control packet from
            // interleaving this control procedure
            MAP_llReplaceCtrlPkt( connPtr, LL_CTRL_DUMMY_PLACE_HOLDER_TRANSMIT,
                                  LL_CTRL_UNDEFINED_PKT );
          }
          else // not received yet, so decrement and check control procedure timeout
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
          // so try to put it there
          // Note: All pending transmissions must also be finished before this
          //       packet is placed in the TX FIFO.
          if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_PAUSE_ENC_RSP) == TRUE )
          {
            // clear the flag that indicates an Encryption Request has been
            // received, which is used by this control procedure to restart the
            // control procedure timeout
            connPtr->encInfo.pauseEncRspRcved = FALSE;

            // disable encryption
            // Note: Not really necessary as no data is supposed to be sent
            //       or received.
            connPtr->encEnabled = FALSE;

            /**** UPDATE DEBUG INFO MODULE ****/
            (void)MAP_DbgInf_addConnEst(connPtr->connId, HCI_EVT_PERIPHERAL_ROLE, UFALSE);

            // the control packet is now active; drop through
            connPtr->ctrlPktInfo.ctrlPktActive = TRUE;
          }
          else // not done yet
          {
            //  control packet stays at head of queue, so exit here
            return( LL_CTRL_PROC_STATUS_SUCCESS );
          }
        }

        break;
#endif // ADV_CONN_CFG | INIT_CFG

      /*
      ** Reject Indication
      */
      case LL_CTRL_REJECT_IND:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // yes, so check if it has been transmitted yet
          // Note: This only means the packet has been transmitted, not that it
          //       has been ACK'ed or NACK'ed.
          // Note: The control procedure does not end until the Reject is ACKed.
          //       However, if the ACK is a data packet, it will be tossed
          //       unless data is allowed hereafter. So to avoid this, only
          //       the confirmed transmission of this will be used to qualify
          //       the related flags, but a new procedure will not be able to
          //       begin until this procedure completes, per the spec.
          if ( connOutput.nTxCtl )
          {
            // disable encryption
            // Note: Never really enabled so this isn't necessary.
            connPtr->encEnabled = FALSE;

            /**** UPDATE DEBUG INFO MODULE ****/
            (void)MAP_DbgInf_addConnEst(connPtr->connId, HCI_EVT_PERIPHERAL_ROLE, UFALSE);

            // set flag to allow outgoing data transmissions
            connPtr->txDataEnabled = TRUE;

            // return any data entries that were stalled on temp queue to the RF queue
            MAP_llMoveTempTxDataEntries( connPtr );

            // okay to receive data again
            connPtr->rxDataEnabled = TRUE;
          }

          // we have already place packet on TX FIFO, so check if its been ACK'ed
          if ( connOutput.nTxCtlAck )
          {
            // done with this control packet, so remove from the processing
            // queue and drop through
            MAP_llDequeueCtrlPkt( connPtr );
          }
          else // not ack'ed yet
          {
            // check if a control procedure timeout has occurred
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
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupCtrlPkt( connPtr, LL_CTRL_REJECT_IND);

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
          if ( connOutput.nTxCtl )
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
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupCtrlPkt( connPtr, LL_CTRL_FEATURE_RSP);

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
      ** Peripheral Feature Set Request
      */
      case LL_CTRL_PERIPHERAL_FEATURE_REQ:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // we have already placed a packet on TX FIFO, so wait now until we
          // get the peripheral's LL_CTRL_FEATURE_RSP
          if ( connPtr->featureSetInfo.featureRspRcved == LL_FEATURE_RSP_DONE )
          {
            // notify the Host
            MAP_LL_ReadRemoteUsedFeaturesCompleteCback( LL_STATUS_SUCCESS,
                                                        connPtr->connId,
                                                        connPtr->featureSetInfo.featureSetMask );

            // done with this control packet, so remove from the processing queue
            MAP_llDequeueCtrlPkt( connPtr );
          }
          else if ( connPtr->featureSetInfo.featureRspRcved == LL_FEATURE_RSP_FAILED )
          {
            // notify the Host
            MAP_LL_ReadRemoteUsedFeaturesCompleteCback( LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE,
                                                        connPtr->connId,
                                                        connPtr->featureSetInfo.featureSetMask );

            // clear the Feature Set Exchange response flag
            connPtr->featureSetInfo.featureRspRcved = LL_FEATURE_RSP_INIT;

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
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupCtrlPkt( connPtr, LL_CTRL_PERIPHERAL_FEATURE_REQ);

          // set flag while we wait for response
          // Note: It is okay to repeatedly set this flag in the event the
          //       setup routine hasn't completed yet (e.g. if the TX FIFO
          //       has not yet become empty).
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
      ** Version Information Indication
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
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupCtrlPkt( connPtr, LL_CTRL_VERSION_IND);

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
      ** Ping Request
      */
      case LL_CTRL_PING_REQ:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // yes, so check if it has been transmitted yet
          // Note: This does not mean this packet has been ACK'ed or NACK'ed.
          if ( connOutput.nTxCtl )
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
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupCtrlPkt( connPtr, LL_CTRL_PING_REQ);

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
            LL_ASSERT( (connPtr->rejectIndExt).rejectOpcode == LL_CTRL_CONNECTION_PARAM_REQ );

            // clear reject indication extended received flag
            connPtr->connParamReqFlags.rejectIndExtRcved = FALSE;

            // notify the Host
            MAP_LL_ConnParamUpdateCback( connPtr->rejectIndExt.errorCode,
                                         (uint16)connPtr->connId,
                                         connPtr->paramUpdate.connInterval >> 1,
                                         connPtr->paramUpdate.peripheralLatency,
                                         connPtr->paramUpdate.connTimeout >> 4 );

            // done with this control packet, so remove from the processing queue
            MAP_llDequeueCtrlPkt( connPtr );
          }
          // check if this request is not supported by the peer device
          else if ( connPtr->connParamReqFlags.unknownRspRcved == TRUE )
          {
            // clear unknown response received flag
            connPtr->connParamReqFlags.unknownRspRcved = FALSE;

            // notify the Host
            MAP_LL_ConnParamUpdateCback( LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE,
                                         (uint16)connPtr->connId,
                                         connPtr->paramUpdate.connInterval >> 1,
                                         connPtr->paramUpdate.peripheralLatency,
                                         connPtr->paramUpdate.connTimeout >> 4 );

            // done with this control packet, so remove from the processing queue
            MAP_llDequeueCtrlPkt( connPtr );
          }
          // check if an Update Parameter control packet was received
          else if ( connPtr->pendingParamUpdate )
          {
            // done with this control packet, so remove from the processing queue
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
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupCtrlPkt( connPtr, LL_CTRL_CONNECTION_PARAM_REQ);

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
      ** Connection Parameter Response
      */
      case LL_CTRL_CONNECTION_PARAM_RSP:
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

            // notify the Host
            MAP_LL_ConnParamUpdateCback( connPtr->rejectIndExt.errorCode,
                                         (uint16)connPtr->connId,
                                         connPtr->paramUpdate.connInterval >> 1,
                                         connPtr->paramUpdate.peripheralLatency,
                                         connPtr->paramUpdate.connTimeout >> 4 );

            // done with this control packet, so remove from the processing queue
            MAP_llDequeueCtrlPkt( connPtr );
          }
          // check if an Update Parameter control packet was received
          else if ( connPtr->pendingParamUpdate )
          {
            // done with this control packet, so remove from the processing queue
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
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupCtrlPkt( connPtr, LL_CTRL_CONNECTION_PARAM_RSP);

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
          // yes, so check if it has been transmitted yet
          // Note: This only means the packet has been transmitted, not that it
          //       has been ACK'ed or NACK'ed.
          // Note: The control procedure does not end until the Reject is ACKed.
          //       However, if the ACK is a data packet, it will be tossed
          //       unless data is allowed hereafter. So to avoid this, only
          //       the confirmed transmission of this will be used to qualify
          //       the related flags, but a new procedure will not be able to
          //       begin until this procedure completes, per the spec.
          if ( connOutput.nTxCtl )
          {
            // disable encryption
            // Note: Never really enabled so this isn't necessary.
            connPtr->encEnabled = FALSE;

            /**** UPDATE DEBUG INFO MODULE ****/
            (void)MAP_DbgInf_addConnEst(connPtr->connId, HCI_EVT_PERIPHERAL_ROLE, UFALSE);

            // set flag to allow outgoing data transmissions
            connPtr->txDataEnabled = TRUE;

            // return any data entries that were stalled on temp queue to the RF queue
            MAP_llMoveTempTxDataEntries( connPtr );

            // okay to receive data again
            connPtr->rxDataEnabled = TRUE;
          }

          // we have already place packet on TX FIFO, so check if its been ACK'ed
          if ( connOutput.nTxCtlAck )
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
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupCtrlPkt( connPtr, LL_CTRL_REJECT_EXT_IND);

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
          // check if the update phy request was received
          if ( TST_FEATURE_FLAG( connPtr->phyInfo.phyFlags, UPDATE_PHY_RECEIVED ) )
          {
            // clear the flag
            CLR_FEATURE_FLAG( connPtr->phyInfo.phyFlags, UPDATE_PHY_RECEIVED );

            // done; wait for instant
            MAP_llDequeueCtrlPkt( connPtr );
          }
          // check if this request was rejected
          else if ( TST_FEATURE_FLAG( connPtr->phyInfo.phyFlags, REJECT_EXT_IND_RECEIVED ) )
          {
            // ALT: Sanity check connExt->rejectIndExt.rejectOpcode.

            // clear flag
            CLR_FEATURE_FLAG( connPtr->phyInfo.phyFlags, REJECT_EXT_IND_RECEIVED );

            // notify the Host
            MAP_LL_PhyUpdateCompleteEventCback( connPtr->rejectIndExt.errorCode,
                                                connPtr->connId,
                                                0,
                                                0 );
            // request was rejected, due to collision, so we're done here
            // Note: If say a collision occurred due to a Central PHY request,
            //       then that received request was processed and a PHY
            //       response was queued. When we dequeue here, the response
            //       will be sent.
            MAP_llDequeueCtrlPkt( connPtr );
          }
          // check if this request is not supported by the peer device
          else if ( TST_FEATURE_FLAG( connPtr->phyInfo.phyFlags, UNKNOWN_RSP_RECEIVED ) )
          {
            // clear the flag
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
              // notify the Host
              MAP_LL_PhyUpdateCompleteEventCback( LL_CTRL_PKT_TIMEOUT_HOST_TERM,
                                                  connPtr->connId,
                                                  0,
                                                  0 );
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
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupCtrlPkt( connPtr, LL_CTRL_PHY_REQ);

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
      ** PHY Response
      */
      case LL_CTRL_PHY_RSP:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // check if the update phy request was received
          if ( TST_FEATURE_FLAG( connPtr->phyInfo.phyFlags, UPDATE_PHY_RECEIVED ) )
          {
            // clear the flag
            CLR_FEATURE_FLAG( connPtr->phyInfo.phyFlags, UPDATE_PHY_RECEIVED );

            // done; wait for instant
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
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupCtrlPkt( connPtr, LL_CTRL_PHY_RSP);

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
      ** Length Update
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
          connPtr->ctrlPktInfo.ctrlPktActive = MAP_llSetupCtrlPkt( connPtr, LL_CTRL_LENGTH_REQ);

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
      ** Constant Tone Extension request
      */
#ifdef RTLS_CTE
      case LL_CTRL_CTE_REQ:
        // check if the control packet procedure is active
        if ( connPtr->ctrlPktInfo.ctrlPktActive == TRUE )
        {
          // check that the CTE request procedure was done
          if ( llCte[connPtr->connId].initiator.sendRequest == FALSE )
          {
            // it has been sent, so dequeue this control procedure
            MAP_llDequeueCtrlPkt( connPtr );
          }
          else // no done yet
          {
            // check if a control procedure timeout has occurred
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
        if ( MAP_llSetupCtrlPkt( connPtr, LL_CTRL_UNKNOWN_RSP) == TRUE )
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
      ** Dummy Place Holder Transmit
      */
      case LL_CTRL_DUMMY_PLACE_HOLDER_TRANSMIT:
        //  dummy packet stays at head of queue, so exit here
        return( LL_CTRL_PROC_STATUS_SUCCESS );

        // Note: Unreachable statement generates compiler warning!
        //break;

      /*
      ** Dummy Place Holder Transmit Pending
      */
      case LL_CTRL_DUMMY_PLACE_HOLDER_TX_PENDING:
        // replace place holder with transmit place holder
        MAP_llReplaceCtrlPkt( connPtr,
                              connPtr->ctrlPktInfo.ctrlPktPending,
                              LL_CTRL_UNDEFINED_PKT );

        // and make it active
        connPtr->ctrlPktInfo.ctrlPktActive = 1;

        break;

      /*
      ** Dummy Place Holder Receive
      */
      case LL_CTRL_DUMMY_PLACE_HOLDER_RECEIVE:
        // check which control packet type is pending processing
        switch ( connPtr->ctrlPktInfo.ctrlPktPending )
        {
          // Note: Peripheral can never receive a LL_CTRL_CONNECTION_PARAM_RSP.
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

          case LL_CTRL_VERSION_IND:
          case LL_CTRL_PERIPHERAL_FEATURE_REQ:
          case LL_CTRL_LENGTH_REQ:
            // receipt of a control packet could not be processed because
            // some other control procedure was active
            // Note: All other types of collisions have already been handled.

            // replace place holder with control packet
            MAP_llReplaceCtrlPkt( connPtr,
                                  connPtr->ctrlPktInfo.ctrlPktPending,
                                  LL_CTRL_UNDEFINED_PKT );

            // and indicate this packet has already been sent
            connPtr->ctrlPktInfo.ctrlPktActive = TRUE;

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
        // fatal error - not possible
        LL_ASSERT( FALSE );
        break;
    }
  }

  return( LL_CTRL_PROC_STATUS_SUCCESS );
}


/*******************************************************************************
 * @fn          llCheckForLstoDuringSL
 *
 * @brief       This function is used to determine if a link supervision timeout
 *              (LSTO) is going to occur during peripheral latency. Typically, the
 *              expiration event ought to remain ahead of the next event unless
 *              a LSTO is going to occur. This routine is used after the next
 *              event is updated, taking peripheral latency into account, to find out
 *              if the LSTO will result during the ignored events. This is done
 *              by checking if the next event advances past the expiration
 *              event, taking wrap into consideration.
 *
 *              Note: Next event can not advance more than the max peripheral latency
 *                    value, and expiration event can not advance more than the
 *                    max LSTO. Since the event counter is 64K, rather than
 *                    finding the number of events between the two in
 *                    consecutive order, the difference between the two will be
 *                    found instead. If this difference is larger than max
 *                    possible difference, then a wrap has taken place. This
 *                    saves the extra calculation otherwise needed.
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the current connection.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      Indicates whether a supervision timeout is going to occur
 *              during peripheral latency:
 *              TRUE:  Terminate due to a LSTO.
 *              FALSE: Do not terminate.
 */
uint8 llCheckForLstoDuringSL( llConnState_t *connPtr )
{
  // first check if the next event is after the expiration event
  if ( connPtr->expirationEvent < connPtr->nextEvent )
  {
    // yes, but check if that's only because expiration event wrapped around
    // Note: The delta is taken instead of finding the number of events between
    //       as this calculation is faster.
    if ( (connPtr->nextEvent - connPtr->expirationEvent) < MAX_LSTO_NUM_OF_EVENTS )
    {
      // expiration event did not wrap and is actually behind next event
      return( TRUE );
    }
  }
  else // expiration event is ahead of or equal to the next event
  {
    // so check if expiration event is only ahead because next event wrapped
    if ( (connPtr->expirationEvent - connPtr->nextEvent) > MAX_LSTO_NUM_OF_EVENTS )
    {
      // next event did wrap and is actually ahead of expiration event
      return( TRUE );
    }
  }

  return( FALSE );
}

#endif // ADV_CONN_CFG

/*******************************************************************************
 */
