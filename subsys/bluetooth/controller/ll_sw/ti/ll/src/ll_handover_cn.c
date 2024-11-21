/******************************************************************************

 @file  ll_handover_cn.c

 @brief This file contains the Link Layer (LL) handover candidate node APIs
        and event implementation

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
#include "ll_common.h"
#include "ll_timer_drift.h"
#include "ll_rat.h"
#include "ll_handover.h"
#include "ll_handover_cn.h"
#include "ble.h"
#include "ll_ae.h"
#include "comdef.h"
#include "map_direct.h"
#include <ti/drivers/utils/Math.h>

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */
const llHandoverCNCBs_t *pLLHandoverCNCBs = NULL;

/*******************************************************************************
 * GLOBAL VARIABLES
 */
extern void LL_rclPeripheralCallback(RCL_Command *cmd, LRF_Events lrfEvents, RCL_Events events);

/*******************************************************************************
 * LOCAL Functions
 */
static uint8 llHandoverCNBuildConnInfo(llConnState_t *connPtr, llHandoverCNParams_t *pParams);
static void  llHandoverCNBuildLinkCmd(llConnState_t *connPtr, llHandoverCNParams_t *pParams);
static void  llHandoverCalcConnEffectiveTimeOctet(llConnState_t *connPtr);

/*******************************************************************************
 * Functions
 */

/*******************************************************************************
 * @fn          LL_Handover_RegisterCNCb
 *
 * @brief       Register to the candidate node callbacks
 *
 * @Design:      BLE_LOKI-1466
 *
 * input parameters
 *
 * @param       pCBs - pointer to the callback functions
 *
 * output parameters
 *
 * @param       None
 *
 * @return      LL_STATUS_SUCCESS, LL_STATUS_ERROR_BAD_PARAMETER
 */
uint8 LL_Handover_RegisterCNCb( const llHandoverCNCBs_t *pCBs )
{
    uint8 status = LL_STATUS_SUCCESS;

    if ( pCBs == NULL )
    {
        status = LL_STATUS_ERROR_BAD_PARAMETER;
    }
    else if ( pCBs->pfnHandoverStartCNCB == NULL )
    {
        status =  LL_STATUS_ERROR_BAD_PARAMETER;
    }
    else
    {
        // Save the CB function
        pLLHandoverCNCBs = pCBs;
    }

    return status;
}

/*******************************************************************************
 * @fn          LL_Handover_StartCN
 *
 * @brief       This API check if it valid to start the handover process
 *              in the LL layer.
 *              If the controller can handle the new connection, all the
 *              advertising sets will be closed, creates a new connection
 *              with the new given data.
 *
 * @Design:     BLE_LOKI-1466
 *
 * input parameters
 *
 * @param   pParams - Pointer to the candidate node parameters
 *
 * output parameters
 *
 * @param       None
 *
 * @return      LL_STATUS_SUCCESS, LL_STATUS_ERROR_CONNECTION_LIMIT_EXCEEDED
 *              LL_STATUS_ERROR_COMMAND_DISALLOWED, LL_STATUS_ERROR_BAD_PARAMETER
 */
uint8 LL_Handover_StartCN( llHandoverCNParams_t *pParams )
{
    llConnState_t *connPtr;
    uint8 numActiveConns = 0xFF;
    uint8 status;

    // Get the number of active connections
    (void)MAP_LL_GetNumActiveConns(&numActiveConns);

    // Check if the pointer is valid
    if ( pParams->pHandoverData == NULL )
    {
        return LL_STATUS_ERROR_BAD_PARAMETER;
    }

    // Check if reached the maximum number of connections allowed
    if ( numActiveConns >= maxNumConns )
    {
        return LL_STATUS_ERROR_CONNECTION_LIMIT_EXCEEDED;
    }

    // Check the upper layers registered for the CB
    if ( pLLHandoverCNCBs == NULL )
    {
        return LL_STATUS_ERROR_COMMAND_DISALLOWED;
    }

#if defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))
    /********************************************************************/
    // Find advertising set from handle
    advSet_t *pAdvSet = MAP_LL_SearchAdvSet(aeCurHandle);

    // Check if advertising set was found
    if ( pAdvSet != NULL )
    {
        // Set advertising enable params.
        pAdvSet->pEnable->enable = LL_ADV_MODE_OFF;

        if ( MAP_LE_SetExtAdvEnable(pAdvSet->pEnable) != LL_STATUS_SUCCESS )
        {
            // Couldn't turn off the advertising
            return LL_STATUS_ERROR_UNEXPECTED_PARAMETER;
        }
    }
    /********************************************************************/
#endif // defined(CTRL_CONFIG) && (CTRL_CONFIG & (ADV_NCONN_CFG | ADV_CONN_CFG))

    // get the connections ID
    if ( (connPtr = MAP_llAllocConnId()) == NULL )
    {
        return LL_STATUS_ERROR_CONNECTION_LIMIT_EXCEEDED;
    }

    // Build the connection structure
    status = llHandoverCNBuildConnInfo(connPtr, pParams);

    if ( status != LL_STATUS_SUCCESS )
    {
        // Tear down the task
        return status;
    }

    // Build the connection link command
    llHandoverCNBuildLinkCmd(connPtr, pParams);

    // Activate the connection
    connPtr->activeConn = UTRUE;

    // Increment the number of active connections
    llConns.numActiveConns++;

    // Process Peripheral
    MAP_llProcessPeripheralConnectionCreated();

    // Phase 2 - This will be conditioned if there are other tasks in the system

    if ( llConns.numActiveConns == 1 )
    {
      // Make the new connection be the current connection for the scheduler
      llConns.currentConn = connPtr->connId;
    }

    // check if there are currently no tasks that are active
    if ( llState == LL_STATE_IDLE )
    {
        // Set LL state
        llState = LL_STATE_CONN_PERIPHERAL;
        MAP_llScheduler();
    }
    else
    {
        // Preemption
    }

    return LL_STATUS_SUCCESS;
}

/*******************************************************************************
 * @fn          llHandoverNotifyConnStatus
 *
 * @brief       This function is used to notify the upper layers with the handover
 *              status on the candidate node side
 *
 * input parameters
 *
 * @param       connHandle - Connection handle
 * @param       handoverStatus - Handover status
 *
 * output parameters
 *
 * @param       None
 *
 * @return      LL_STATUS_SUCCESS, LL_STATUS_ERROR_UNEXPECTED_PARAMETER
 */
uint8 llHandoverNotifyConnStatus(uint16_t connHandle, uint32_t handoverStatus)
{
    pfnLLHandoverStartCNCB_t pfnStartCNCB;
    uint8 status = LL_STATUS_SUCCESS;

    // Check the upper layers registered for the CB
    if ( pLLHandoverCNCBs != NULL )
    {
       pfnStartCNCB = pLLHandoverCNCBs->pfnHandoverStartCNCB;

       if ( pfnStartCNCB != NULL )
       {
         (*pfnStartCNCB)(connHandle, handoverStatus);
       }
    }
    else
    {
      status = LL_STATUS_ERROR_UNEXPECTED_PARAMETER;
    }

    return status;
}

/*******************************************************************************
 * @fn          llHandoverCNBuildConnInfo
 *
 * @brief       This function is used to build the connection pointer for the
 *              connection formed using the connection handover process
 *
 * input parameters
 *
 * @param       connPtr - Pointer to connection
 * @param       pParams - Pointer to the candidate node parameters
 *
 * output parameters
 *
 * @param       None
 *
 * @return      LL_STATUS_SUCCESS, LL_STATUS_ERROR_DUE_TO_LIMITED_RESOURCES
 */
uint8 llHandoverCNBuildConnInfo(llConnState_t *connPtr, llHandoverCNParams_t *pParams)
{
    uint32 connIntervalInTicks;
    uint32 timeOffsetInTicks;
    uint16 numMissedConnEvents;

    /* LL Task */

    // Get a task block for this BLE state/role
    // Note: There will always be a valid pointer, so no NULL check required.
    connPtr->llTask = MAP_llAllocTask(LL_TASK_ID_PERIPHERAL);

    // Check that the task was created
    if ( connPtr->llTask == NULL )
    {
        return LL_STATUS_ERROR_DUE_TO_LIMITED_RESOURCES;
    }

    connPtr->llTask->setup = MAP_llLinkSchedSetup;

    /* General Connection Data */
    connPtr->activeConn = FALSE;
    connPtr->currentEvent = pParams->pHandoverData->currentEvent - 1;
    connPtr->nextEvent = pParams->pHandoverData->currentEvent; // Don't consider the peripheral latency for now

    // Covert the connection interval and the offset to ticks
    connIntervalInTicks = pParams->pHandoverData->connInterval*RAT_TICKS_IN_625US;
    timeOffsetInTicks = (pParams->timeDelta)*RAT_TICKS_IN_1US;

    // Calculate how many connection events the candidate might missed during the data transfer
    numMissedConnEvents = timeOffsetInTicks/connIntervalInTicks;

    connPtr->nextEvent += numMissedConnEvents;

    // If maxFailedConnEvents is 0 then the expiration event will be the connection supervision timeout
    if ( pParams->maxFailedConnEvents == 0 )
    {
        connPtr->expirationEvent = pParams->pHandoverData->expirationEvent;
    }
    else
    {
        connPtr->expirationEvent = connPtr->nextEvent +
                                   pParams->maxFailedConnEvents - 1;
    }

    connPtr->expirationValue = pParams->pHandoverData->expirationValue;
    connPtr->firstPacket = pParams->pHandoverData->firstPacket;
    connPtr->scaFactor = pParams->pHandoverData->scaFactor;
    connPtr->timerDrift = connPtr->timerDrift;

    /* Connection Parameters */
    // Phase 2 - The peripheral latency may not be 0, this must be changed in another location
    connPtr->peripheralLatency = 0;
    connPtr->peripheralLatencyAllowed = FALSE;
    connPtr->peripheralLatencyValue = 0;
    connPtr->accessAddr = pParams->pHandoverData->accessAddr;
    connPtr->crcInit = pParams->pHandoverData->crcInit;
    connPtr->sleepClkAccuracy = pParams->pHandoverData->sleepClkAccuracy;
    connPtr->curParam.winSize = pParams->pHandoverData->winSize;
    connPtr->curParam.winOffset = pParams->pHandoverData->winOffset;
    connPtr->curParam.connInterval = pParams->pHandoverData->connInterval;
    connPtr->curParam.peripheralLatency = pParams->pHandoverData->curParamPeriLatency;
    connPtr->curParam.connTimeout = pParams->pHandoverData->connTimeout;

    /* Channel Map */
    connPtr->currentChan = pParams->pHandoverData->currentChan;
    connPtr->nextChan = pParams->pHandoverData->nextChan;
    connPtr->currentMappedChan = pParams->pHandoverData->currentMappedChan;
    connPtr->hopLength = pParams->pHandoverData->hopLength;
    connPtr->numUsedChans = pParams->pHandoverData->numUsedChans;
    MAP_osal_memcpy(connPtr->chanMapTable, pParams->pHandoverData->chanMapTable, LL_MAX_NUM_DATA_CHAN);
    MAP_osal_memcpy(connPtr->curChanMap.chanMap, pParams->pHandoverData->chanMap, LL_NUM_BYTES_FOR_CHAN_MAP);
    connPtr->pChSelAlgo = ( pParams->pHandoverData->chanSelAlgo == 1 ) ?
                            MAP_llGetNextDataChanAlgo1 :
                            MAP_llGetNextDataChanAlgo2;

    /* Encryption Data */
    connPtr->encEnabled = pParams->pHandoverData->encEnabled;
    if ( connPtr->encEnabled == UTRUE )
    {
        MAP_osal_memcpy(connPtr->encInfo.IV, pParams->pHandoverData->IV, LL_ENC_IV_LEN);
        MAP_osal_memcpy(connPtr->encInfo.SKD, pParams->pHandoverData->SKD, LL_ENC_SKD_LEN);
        MAP_osal_memcpy(connPtr->encInfo.RAND, pParams->pHandoverData->RAND, LL_ENC_RAND_LEN);
        MAP_osal_memcpy(connPtr->encInfo.EDIV, pParams->pHandoverData->EDIV, LL_ENC_EDIV_LEN);
        MAP_osal_memcpy(connPtr->encInfo.nonce, pParams->pHandoverData->nonce, LL_ENC_NONCE_LEN);
        MAP_osal_memcpy(connPtr->encInfo.SK, pParams->pHandoverData->SK, LL_ENC_SK_LEN);
        MAP_osal_memcpy(connPtr->encInfo.LTK, pParams->pHandoverData->LTK, LL_ENC_LTK_LEN);
        connPtr->encInfo.SKValid = UTRUE;
        connPtr->encInfo.LTKValid = UTRUE;
        connPtr->encInfo.txPktCount = pParams->pHandoverData->txPktCount;
        connPtr->encInfo.rxPktCount = pParams->pHandoverData->rxPktCount;
    }
    else
    {
        // The encryption is not enabled. Reset the variable
        MAP_osal_memset(&connPtr->encInfo, 0xFF, sizeof(encInfo_t));
    }

    /* Feature Set */
    connPtr->featureSetInfo.featureRspRcved = pParams->pHandoverData->featureRspRcved;
    MAP_osal_memcpy(connPtr->featureSetInfo.featureSet, pParams->pHandoverData->featureSet, LL_MAX_FEATURE_SET_SIZE);

    /* Version Information */
    connPtr->verExchange.peerInfoValid = pParams->pHandoverData->peerInfoValid;
    connPtr->verExchange.hostRequest = pParams->pHandoverData->hostRequest;
    connPtr->verExchange.verInfoSent = pParams->pHandoverData->verInfoSent;
    connPtr->verInfo.verNum = pParams->pHandoverData->verNum;
    connPtr->verInfo.comId = pParams->pHandoverData->comId;
    connPtr->verInfo.subverNum = pParams->pHandoverData->subverNum;

    /* Peer Info */
    connPtr->peerInfo.peerAddrType = pParams->pHandoverData->peerAddrType;
    MAP_osal_memcpy(connPtr->peerInfo.peerAddr, pParams->pHandoverData->peerAddr, LL_DEVICE_ADDR_LEN);

    /* RX Window */
    connPtr->lastTimeoutTime = pParams->pHandoverData->lastTimeoutTime;

    /* Save off Central Contribution */
    connPtr->mstSCA = pParams->pHandoverData->mstSCA;

    /* Authenticated Payload Timeout */
    connPtr->aptoValue = pParams->pHandoverData->aptoValue;
    connPtr->numAptoExp = pParams->pHandoverData->numAptoExp;

    /* Length Info */
    connPtr->lenInfo.connMaxTxOctets = pParams->pHandoverData->connMaxTxOctets;
    connPtr->lenInfo.connMaxRxOctets = pParams->pHandoverData->connMaxRxOctets;
    connPtr->lenInfo.connMaxTxTime = pParams->pHandoverData->connMaxTxTime;
    connPtr->lenInfo.connMaxRxTime = pParams->pHandoverData->connMaxRxTime;
    connPtr->lenInfo.connRemoteMaxTxOctets = pParams->pHandoverData->connRemoteMaxTxOctets;
    connPtr->lenInfo.connRemoteMaxRxOctets = pParams->pHandoverData->connRemoteMaxRxOctets;
    connPtr->lenInfo.connRemoteMaxTxTime = pParams->pHandoverData->connRemoteMaxTxTime;
    connPtr->lenInfo.connRemoteMaxRxTime = pParams->pHandoverData->connRemoteMaxRxTime;
    connPtr->lenInfo.connIntervalPortionAvail = pParams->pHandoverData->connIntervalPortionAvail;
    connPtr->lenInfo.lenFlags = pParams->pHandoverData->lenFlags;
    connPtr->lenInfo.connSlowestPhy = pParams->pHandoverData->connSlowestPhy;

    /* Phy Info */
    connPtr->phyInfo.curPhy = pParams->pHandoverData->curPhy;
    connPtr->phyInfo.phyFlags = pParams->pHandoverData->phyFlags;
    connPtr->phyInfo.phyOpts = pParams->pHandoverData->phyOpts;
    connPtr->phyInfo.phyPreference = pParams->pHandoverData->phyPreference;

    // Calculate the effective time
    llHandoverCalcConnEffectiveTimeOctet(connPtr);


    connPtr->connPriority = pParams->pHandoverData->connPriority;

    if ( MAP_osal_isbufset(ownRandomAddr, 0xFF, LL_DEVICE_ADDR_LEN) == TRUE )
    {
        connPtr->ownAddrType = LL_DEV_ADDR_TYPE_RANDOM;
    }
    else
    {
        connPtr->ownAddrType = LL_DEV_ADDR_TYPE_PUBLIC;
    }
    connPtr->estWithHandover = UTRUE;

    return LL_STATUS_SUCCESS;
}

/*******************************************************************************
 * @fn          llHandoverCNBuildLinkCmd
 *
 * @brief       This funtion is used to build the the link command for the
 *              connection formed using the connection handover process
 *
 * input parameters
 *
 * @param       connPtr - Pointer to connection
 * @param       pParams - Pointer to the candidate node parameters
 *
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llHandoverCNBuildLinkCmd(llConnState_t *connPtr, llHandoverCNParams_t *pParams)
{
    uint32 timeToNextEvt;
    uint32 connIntervalInTicks;
    uint32 timeOffsetInTicks;
    uint16 numMissedConnEvents;

    /* Initialize connection command and structures */
    linkCmd[connPtr->connId] = RCL_CmdBle5Connection_DefaultRuntime();
    linkParam[connPtr->connId] = RCL_CtxConnection_DefaultRuntime();
    // use common parameters and output
    linkCmd[connPtr->connId].ctx = &linkParam[connPtr->connId];
    linkCmd[connPtr->connId].stats = &connOutput;

    // Data from the serving node
    linkCmd[connPtr->connId].common.phyFeatures = pParams->pHandoverData->phyFeatures;
    linkCmd[connPtr->connId].ctx->seqStat = pParams->pHandoverData->seqStat;
    linkCmd[connPtr->connId].txPower.fraction = pParams->pHandoverData->fraction;
    linkCmd[connPtr->connId].txPower.dBm = pParams->pHandoverData->dBm;

    // Set callback function and events
    linkCmd[connPtr->connId].common.runtime.callback = LL_rclPeripheralCallback;
    linkCmd[connPtr->connId].common.runtime.lrfCallbackMask.value = LRF_EventTxDone.value |LRF_EventRxOk.value;
    linkCmd[connPtr->connId].common.runtime.rclCallbackMask.value =
                               RCL_EventLastCmdDone.value  |
                               RCL_EventRxEntryAvail.value |
                               RCL_EventTxBufferFinished.value;

    // Set start time trigger
    linkCmd[connPtr->connId].common.scheduling = RCL_Schedule_AbsTime;
    linkCmd[connPtr->connId].common.allowDelay = TRUE;

    // Connection event channel
    linkCmd[connPtr->connId].channel = connPtr->currentMappedChan;

    // RCL CTX
    linkParam[connPtr->connId].isPeripheral = TRUE;
    linkParam[connPtr->connId].accessAddress = connPtr->accessAddr;
    linkParam[connPtr->connId].crcInit = connPtr->crcInit;

    // Phase 2 - Handling multiple connections - This might be needed to be handled differently

    // Setup the Peripheral Receive Queue
    MAP_llSetupConnRxDataEntryQueue( connPtr->connId );

    // Attach data queues to the connection
    txDataQ[connPtr->connId].rfDataBuffers = &linkParam[connPtr->connId].txBuffers;

    connPtr->pTxDataEntryQ = (void *)&txDataQ[connPtr->connId];
    connPtr->pRxDataEntryQ = (void *)&linkParam[connPtr->connId].rxBuffers;

    // Initialize the statistic structure
    connOutput = RCL_StatsConnection_DefaultRuntime();

    timeToNextEvt = connPtr->curParam.connInterval;

    // calculate timer drift correction
    // Note: SCA Factor is in PPM, and time to next event is in 625us ticks. The
    //       result is the timer drift in RAT ticks.
    // Note: Round up one RAT tick to account for floor effect.
    connPtr->timerDrift = ((timeToNextEvt * connPtr->scaFactor) / RAT_TICKS_IN_100US) + 1;

    uint32 currentTime = MAP_llGetCurrentTime();

    connIntervalInTicks = connPtr->curParam.connInterval*RAT_TICKS_IN_625US;
    timeOffsetInTicks = (pParams->timeDelta)*RAT_TICKS_IN_1US;
    numMissedConnEvents = timeOffsetInTicks/connIntervalInTicks;

    if ( connPtr->nextEvent == (connPtr->currentEvent + 1) )
    {
	    // Setting up start time in the link command
        linkCmd[connPtr->connId].common.timing.absStartTime = currentTime + pParams->pHandoverData->timeToHandoverEvent -
                                                              ((pParams->timeDelta)*RAT_TICKS_IN_1US) -
                                                              (connPtr->timerDrift +
                                                               LL_RX_RAMP_OVERHEAD +
                                                               LL_JITTER_CORRECTION) -
                                                               LL_HANDOVER_EARLIER_ST; // Start the window earlier (3ms)
    }
    else
    {
        // Based on the current time and the SN time to next event when the connection should start
        uint32 nextEventEstimate = currentTime + pParams->pHandoverData->timeToHandoverEvent - ((pParams->timeDelta)*RAT_TICKS_IN_1US);

        // Advance the start time based on the number of conn event the candidate missed
        uint32 actualStartTime = nextEventEstimate + ((connPtr->curParam.connInterval*RAT_TICKS_IN_625US)*numMissedConnEvents);




        linkCmd[connPtr->connId].common.timing.absStartTime = actualStartTime -
                                                              (connPtr->timerDrift +
                                                              LL_RX_RAMP_OVERHEAD +
                                                              LL_JITTER_CORRECTION) -
                                                              LL_HANDOVER_EARLIER_ST; // Start the window earlier (3ms)

        // TODO: If there is less time than the 3ms guard we want then do next event
    }

    connPtr->llTask->startTime = linkCmd[connPtr->connId].common.timing.absStartTime;
    connPtr->llTask->lastStartTime = connPtr->llTask->startTime;

    // Setting the RX window 0 in order for him to open RX window until it catches the central
    // or until it reach the hard stop time. This is done to increase the probability the candidate
    // will be able to sync to the central
    linkCmd[connPtr->connId].relRxTimeoutTime = 0;

    // setup the connection event End Time relative to the timestamp
    linkCmd[connPtr->connId].common.timing.relHardStopTime =
      (((connPtr->curParam.connInterval * *llConfigTable.connEvtCutoff) / 100) * RAT_TICKS_IN_625US) -
      (2 * RAT_TICKS_IN_150US);

    // Pointer to first radio operation command
    connPtr->llTask->command = (uint32)&linkCmd[connPtr->connId];

    // Candidate missed at least one connection event
    if ( connPtr->nextEvent != (connPtr->currentEvent + 1) )
    {
        connPtr->nextChan = connPtr->currentChan;
        MAP_llSetNextDataChan( connPtr );
    }
}

/*******************************************************************************
 * @fn          llHandoverCalcConnEffectiveTimeOctet
 *
 * @brief       Calculates the effective time and octets in the connection
 *              based on the connection phy parameters
 *
 * input parameters
 *
 * @param       connPtr       - Pointer to connection
 *
 * output parameters
 *
 * @param       None
 *
 * @return      LL_STATUS_SUCCESS, LL_STATUS_ERROR_DUE_TO_LIMITED_RESOURCES
 */
void llHandoverCalcConnEffectiveTimeOctet(llConnState_t *connPtr)
{
    uint16 newEffectiveVal;

    //////////////////////////////////////////////////////////////////////////
    // Effective Maximum Rx Time
    // Note: Needed by Effective Maximum Tx Time!
    //////////////////////////////////////////////////////////////////////////

    // find the effective Rx time
    newEffectiveVal = Math_MIN( connPtr->lenInfo.connMaxRxTime,
                                connPtr->lenInfo.connRemoteMaxTxTime );

    // based on current phy
    if ( connPtr->phyInfo.curPhy == LL_PHY_CODED )
    {
      newEffectiveVal = Math_MAX( 2704, newEffectiveVal );
    }

    // update the effective Rx buffer size
    connPtr->lenInfo.connEffectiveMaxRxTime = newEffectiveVal;

    //////////////////////////////////////////////////////////////////////////
    // Effective Maximum Tx Time
    // Note: Needed by Effective Maximum Tx Octets!
    //////////////////////////////////////////////////////////////////////////

    // based on current phy
    if ( connPtr->phyInfo.curPhy == LL_PHY_CODED )
    {
      newEffectiveVal = MAP_llSetCodedMaxTxTime( connPtr );

      connPtr->lenInfo.connEffectiveMaxTxTime = newEffectiveVal;
    }
    else // !LL_PHY_CODED
    {
      // find the effective Tx time
      newEffectiveVal = Math_MIN( connPtr->lenInfo.connMaxTxTime,
                                  connPtr->lenInfo.connRemoteMaxRxTime );

      // update the effective Rx buffer size
      connPtr->lenInfo.connEffectiveMaxTxTime = newEffectiveVal;
    }

    //////////////////////////////////////////////////////////////////////////
    // Effective Maximum Tx Octets
    //////////////////////////////////////////////////////////////////////////

    // find the effective Tx buffer size
    newEffectiveVal = Math_MIN( connPtr->lenInfo.connMaxTxOctets,
                                connPtr->lenInfo.connRemoteMaxRxOctets );

    // update the effective Tx buffer size
    connPtr->lenInfo.connEffectiveMaxTxOctets = newEffectiveVal;

    // update the effective Tx buffer size, factoring in Time
    // Note: The specification requires that the Controller not transmit PDUs
    //       with payload greater than connEffectiveMaxTxOctets OR take more
    //       than connEffectiveMaxTxTime in us to transmit. By converting
    //       connEffectiveMaxTxTime into octets at the current PHY data rate,
    //       connEffectiveMaxTxOctets can be the lesser of octets and time,
    //       but all in terms of octets. However, in order not to confuse
    //       the Host, connEffectiveMaxTxOctets has to be properly maintained
    //       (i.e. based only on its comparison to connRemoteMaxRxOctets),
    //       and reported, while a different internally used value can be
    //       used to cap Tx based on PHY and PHY changes (as given by
    //       connSlowestPhy).
    connPtr->lenInfo.connActualMaxTxOctets =
      Math_MIN( connPtr->lenInfo.connEffectiveMaxTxOctets,
                MAP_llTime2Octets( connPtr->phyInfo.curPhy,
                                   connPtr->phyInfo.phyOpts,
                                   connPtr->lenInfo.connEffectiveMaxTxTime,
                                   MIC_ENABLED ) );

    //////////////////////////////////////////////////////////////////////////
    // Effective Maximum Rx Octets
    //////////////////////////////////////////////////////////////////////////

    // find the effective Rx buffer size
    newEffectiveVal = Math_MIN( connPtr->lenInfo.connMaxRxOctets,
                                connPtr->lenInfo.connRemoteMaxTxOctets );

    // update the effective Rx buffer size
    connPtr->lenInfo.connEffectiveMaxRxOctets = newEffectiveVal;
}
