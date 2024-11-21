/******************************************************************************

 @file  ll_handover_sn.c

 @brief This file contains the Link Layer (LL) handover serving node APIs
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
#include "ll_handover.h"
#include "ll_handover_sn.h"
#include "comdef.h"
#include "map_direct.h"
#include "ble.h"
#include "ll_rat.h"

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */
#define HANDOVER_SERVING_NODE_MODE_DISCONNECT 0U

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */
// Controller SN params
llHandoverSNParams_t gSnLlParams =
{
  .connHandle       = LL_CONNHANDLE_INVALID,
  .handoverDataSize = INVALID_DATA_SIZE,
  .pHandoverData    = NULL,
  .handoverSnMode   = 0,
};

/*******************************************************************************
 * GLOBAL VARIABLES
 */
const llHandoverSNCBs_t *pLLHandoverSNCBs = NULL;

/*******************************************************************************
 * LOCAL FUNCTIONS
 */
static void  llHandoverInitSNParams(void);

/*******************************************************************************
 * Functions
 */

/*******************************************************************************
 * @fn
 *
 * @brief       Register to the serving node callbacks
 *
 * @Design:     BLE_LOKI-1458
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
uint8 LL_Handover_RegisterSNCb( const llHandoverSNCBs_t *pCBs )
{
    if ( pCBs == NULL )
    {
        return LL_STATUS_ERROR_BAD_PARAMETER;
    }
    else
    {
        if ( pCBs->pfnHandoverStartSNCB == NULL )
        {
            return LL_STATUS_ERROR_BAD_PARAMETER;
        }
        else
        {
            // Save the CB function
            pLLHandoverSNCBs = pCBs;
        }
    }
    return LL_STATUS_SUCCESS;
}

/*******************************************************************************
 * @fn          LL_Handover_GetSNDataSize
 *
 * @brief       Return the LL handover data size
 *
 * @Design:     BLE_LOKI-1458
 *
 * input parameters
 *
 * @param       pParams - Pointer to the parameters structure
 *
 * output parameters
 *
 * @param       None
 *
 * @return      The controller handover data size
 */
uint32 LL_Handover_GetSNDataSize( llHandoverSNParams_t *pParams )
{
    // pParams is done in case the controller will not have a fixed
    // data size
    return LL_HANDOVER_CONTROLLER_DATA_SIZE;
}

/*******************************************************************************
 * @fn          LL_Handover_StartSN
 *
 * @brief       Starts the Handover process on the serving node side at the LL.
 *              This function will save the upper layer data size and the connection
 *              handle on which the Handover should start
 *
 * @Design:     BLE_LOKI-1458
 *
 * input parameters
 *
 * @param       pParams - Pointer to the parameters structure
 *
 * output parameters
 *
 * @param       None
 *
 * @return      LL_STATUS_SUCCESS, LL_STATUS_ERROR_INACTIVE_CONNECTION,
 *              LL_STATUS_ERROR_COMMAND_DISALLOWED, LL_STATUS_ERROR_BAD_PARAMETER,
 *              LL_STATUS_ERROR_DUE_TO_LIMITED_RESOURCES
 */
uint8 LL_Handover_StartSN( llHandoverSNParams_t *pParams )
{
    llConnState_t *connPtr = (llConnState_t *)MAP_llDataGetConnPtr( pParams->connHandle );

    // Check the upper layers registered for CB
    if ( pLLHandoverSNCBs == NULL )
    {
        return LL_STATUS_ERROR_COMMAND_DISALLOWED;
    }
    else if ( pLLHandoverSNCBs->pfnHandoverStartSNCB == NULL )
    {
        return LL_STATUS_ERROR_COMMAND_DISALLOWED;
    }

    // Check the pointer is valid
    if ( pParams->pHandoverData == NULL )
    {
        return LL_STATUS_ERROR_BAD_PARAMETER;
    }

    // Check there is no other handover process already active
    if ( pParams->handoverDataSize < LL_HANDOVER_CONTROLLER_DATA_SIZE )
    {
        return LL_STATUS_ERROR_DUE_TO_LIMITED_RESOURCES;
    }

    // Check the connection exists
    if ( connPtr == NULL )
    {
        return LL_STATUS_ERROR_INACTIVE_CONNECTION;
    }
    else
    {
        // The connection exist
        if ( connPtr->activeConn == FALSE )
        {
           // The connection is not active
           return LL_STATUS_ERROR_INACTIVE_CONNECTION;
        }
    }

    // Check if there is an active handover process
    if ( gSnLlParams.pHandoverData != NULL )
    {
        return LL_STATUS_ERROR_COMMAND_DISALLOWED;
    }

    // Save the handover parameters
    gSnLlParams.connHandle = pParams->connHandle;
    gSnLlParams.pHandoverData = pParams->pHandoverData;
    gSnLlParams.handoverSnMode = pParams->handoverSnMode;

    return LL_STATUS_SUCCESS;
}

/*******************************************************************************
 * @fn          LL_Handover_CloseSN
 *
 * @brief       Close the Handover process on the serving node side at the LL.
 *              If the handover was successful this function will generate disconnect
 *              command complete event. Otherwise, it will re-enable the connection.
 *
 * @Design:     BLE_LOKI-1458
 *
 * input parameters
 *
 * @param       pParams        - Pointer to the serving node parameters
 * @param       handoverStatus - TRUE if the candidate was able to follow the
 *                               handover connection. Otherwise, FALSE
 *
 * @return      LL_STATUS_SUCCESS, LL_STATUS_ERROR_COMMAND_DISALLOWED
 */
uint8 LL_Handover_CloseSN( llHandoverSNParams_t *pParams, uint8 handoverStatus )
{
    uint8_t status = LL_STATUS_SUCCESS;

    // Phase 2 will consider the actual status
    // In case of a failure it will not disconnect and will start answering the connection
    llConnState_t *connPtr = (llConnState_t *)MAP_llDataGetConnPtr(gSnLlParams.connHandle);

    if ( (connPtr == NULL)                       ||
         (gSnLlParams.connHandle == LL_CONNHANDLE_INVALID) ||
         (gSnLlParams.pHandoverData == NULL) )
    {
        status = LL_STATUS_ERROR_COMMAND_DISALLOWED;
    }
    else
    {
        if ( handoverStatus == SUCCESS )
        {
            uint8  numConns;

            // Get number of active connections
            MAP_LL_GetNumActiveConns( &numConns );

            // If there isn't any command active, thus the llState is IDLE or
            // the there is only connection task active and the handover connection
            // is the only connection in the system, terminate the connection
            if ( (MAP_llGetNumTasks() == 1) && (numConns == 1) )
            {
                MAP_llConnTerminate( connPtr, LL_CONN_TERMINATE_SUCCESSFUL_HANDOVER );

                // Initialize the handover parameters
                llHandoverInitSNParams();
            }
            else
            {
                // Mark the connection as terminated. The connection will be terminated
                // from llPeripheral_TaskEnd or llExtAdv_PostProcess
                connPtr->termInfo.termIndRcvd = TRUE;
                connPtr->termInfo.connId = connPtr->connId;
                connPtr->termInfo.reason = LL_CONN_TERMINATE_SUCCESSFUL_HANDOVER;
            }
        }
        else
        {
            // Serving node should start answering the connection again
            llConnState_t *connPtr = MAP_llDataGetConnPtr(gSnLlParams.connHandle);
            connPtr->handoverInProg = FALSE;

            // Initialize the handover parameters
            llHandoverInitSNParams();

            if ( llState == LL_STATE_IDLE )
            {
                MAP_llScheduler();
            }
        }
    }

    return status;
}

/*******************************************************************************
 * @fn          llHandoverTriggerDataTransfer
 *
 * @brief       This function will check if the data copy can start. It will verify
 *              the TX queue is empty. If so, it will fill the data needed for the
 *              handover. At the end it will trigger the SN start CB to the upper
 *              layer with the controller status
 *
 * @Design:     BLE_LOKI-1458
 *
 * input parameters
 *
 * @param       connHandle - Connection Handle
 *
 * output parameters
 *
 * @param       status - LL_STATUS_SUCCESS, LL_STATUS_ERROR_BAD_PARAMETER,
 *                       LL_STATUS_ERROR_COMMAND_DISALLOWED,
 *                       LL_STATUS_ERROR_INACTIVE_CONNECTION
 *
 * @return      None
 */
uint8 llHandoverTriggerDataTransfer( void )
{
    pfnLLHandoverStartSNCB_t pfnStartSNCB;
    llConnState_t *connPtr = NULL;
    handoverDataFull_t *pDataBuf;
    RCL_CmdBle5Connection *pLinkCmd;
    uint32 status = LL_STATUS_SUCCESS;

    // Validate the upper layer requested handover
    if ( (gSnLlParams.pHandoverData != NULL) &&
          (gSnLlParams.connHandle != LL_CONNHANDLE_INVALID) )
    {
        // Get the connection pointer
        connPtr = (llConnState_t *)MAP_llDataGetConnPtr( gSnLlParams.connHandle );

        // Extract the CB function
        if ( (pLLHandoverSNCBs == NULL) || (pLLHandoverSNCBs->pfnHandoverStartSNCB == NULL) )
        {
            status = LL_STATUS_ERROR_COMMAND_DISALLOWED;
        }
        else if ( connPtr == NULL )
        {
            // Something happened return
            status = LL_STATUS_ERROR_INACTIVE_CONNECTION;
        }
        else if ( connPtr->activeConn == FALSE )
        {
            // The connection is not active
            status =  LL_STATUS_ERROR_INACTIVE_CONNECTION;
        }
        else if ( MAP_llIsHandoverInProgress(connPtr) == TRUE )
        {
            // The connection is in the middle of handover process
            // Waiting for close serving node command
            status = LL_STATUS_ERROR_REPEATED_ATTEMPTS;
        }
        else if ( (connPtr != NULL) && (llCheckConnInstant(connPtr) != FALSE) )
        {
            status = LL_STATUS_ERROR_TRANSACTION_COLLISION;
        }
        else
        {
            // There is a valid conn pointer, check the link command

            // Extract the link command pointer
            pLinkCmd = (RCL_CmdBle5Connection *)connPtr->llTask->command;

            if ( pLinkCmd == NULL )
            {
                status = LL_STATUS_ERROR_BAD_PARAMETER;
            }
        }

        if ( status == LL_STATUS_SUCCESS )
        {
            // All the validity check passed successfully
            // Extract the CB function pointer
            pfnStartSNCB = pLLHandoverSNCBs->pfnHandoverStartSNCB;

            // Get the pointer to the buffer
            pDataBuf = (handoverDataFull_t *)(gSnLlParams.pHandoverData);

            // Fill the handover data
            llHandoverPopulateSnData(connPtr, pDataBuf, pLinkCmd);

            // Calling the CB
            (*pfnStartSNCB)(gSnLlParams.connHandle, status);

            if ( gSnLlParams.handoverSnMode == HANDOVER_SERVING_NODE_MODE_DISCONNECT )
            {
              uint8 numConns;

              // Get number of active connections
              MAP_LL_GetNumActiveConns( &numConns );
              connPtr->handoverInProg = TRUE;

              // If there isn't any command active, thus the llState is IDLE or
              // the there is only connection task active and the handover connection
              // is the only connection in the system, terminate the connection
              if ( (MAP_llGetNumTasks() == 1) && (numConns == 1) )
              {
                MAP_llConnTerminate( connPtr, LL_CONN_TERMINATE_SUCCESSFUL_HANDOVER );
                llHandoverInitSNParams();
              }
              else
              {
                // Mark the connection as terminated. The connection will be terminated
                // from llPeripheral_TaskEnd or llExtAdv_PostProcess
                connPtr->termInfo.termIndRcvd = TRUE;
                connPtr->termInfo.connId = connPtr->connId;
                connPtr->termInfo.reason = LL_CONN_TERMINATE_SUCCESSFUL_HANDOVER;
              }

              // The new terminate reason code will prevent this function to call the scheduler
              // Return an Error code to trigger the call to the scheduler from ll_peripheral_end_causes
              status = LL_STATUS_ERROR_REPEATED_ATTEMPTS;
            }
            else
            {
              // Wait to the close command
              connPtr->handoverInProg = TRUE;
              status = LL_STATUS_ERROR_INACTIVE_CONNECTION; // This will trigger call to the scheduler in ll_peripherl_end_causes
            }
        }
        else
        {
            if ( status != LL_STATUS_ERROR_REPEATED_ATTEMPTS )
            {
                // The connection is no longer active - reset the handover
                llHandoverInitSNParams();

                if ( status == LL_STATUS_ERROR_COMMAND_DISALLOWED )
                {
                    // There is no way to notify the host
                    // Exit
                }
                else
                {
                    // Extract the CB function pointer
                    pfnStartSNCB = pLLHandoverSNCBs->pfnHandoverStartSNCB;

                    // Call the upper layer CB
                    (*pfnStartSNCB)(gSnLlParams.connHandle, status);
                }
            }
        }
    }
    else
    {
        status = FAILURE;
    }

    return status;
}

/*******************************************************************************
 * @fn          llHandoverPopulateSnData
 *
 * @brief       This function fills the buffer with the handover data
 *
 * @Design:     BLE_LOKI-1458
 *
 * input parameters
 *
 * @param       connPtr  - Connection pointer
 * @param       pContBuf - Pointer to the data buffer
 * @param       pLinkCmd - Pointer to the link command
 *
 * output parameters
 *
 * @param       pContBuf - Handover data
 *
 * @return      None
 */
void llHandoverPopulateSnData(llConnState_t *connPtr, handoverDataFull_t *pContBuf, RCL_CmdBle5Connection *pLinkCmd)
{
    uint32 curTime = MAP_llGetCurrentTime();

    /* Link Command Parameters */
    pContBuf->phyFeatures = pLinkCmd->common.phyFeatures;
    pContBuf->seqStat = pLinkCmd->ctx->seqStat;
    pContBuf->fraction = pLinkCmd->txPower.fraction;
    pContBuf->dBm = pLinkCmd->txPower.dBm;

    /* General Connection Data */
    pContBuf->firstPacket = connPtr->firstPacket;
    pContBuf->currentEvent = connPtr->nextEvent;
    pContBuf->scaFactor = connPtr->scaFactor;
    pContBuf->timeToHandoverEvent = MAP_llTimeDelta(connPtr->llTask->startTime, curTime);
    pContBuf->expirationEvent = connPtr->expirationEvent;
    pContBuf->expirationValue = connPtr->expirationValue;
    pContBuf->lastTimeoutTime = connPtr->lastTimeoutTime;
    pContBuf->timerDrift = connPtr->timerDrift;
    pContBuf->accessAddr = connPtr->accessAddr;
    pContBuf->crcInit = connPtr->crcInit;
    pContBuf->peripheralLatency = connPtr->peripheralLatency;
    pContBuf->sleepClkAccuracy = connPtr->sleepClkAccuracy;

    /* Connection Parameters */
    pContBuf->winSize = connPtr->curParam.winSize;
    pContBuf->winOffset = connPtr->curParam.winOffset;
    pContBuf->connInterval = connPtr->curParam.connInterval;
    pContBuf->curParamPeriLatency = connPtr->curParam.peripheralLatency;
    pContBuf->connTimeout = connPtr->curParam.connTimeout;

    /* Channel Map */
    pContBuf->nextChan = connPtr->nextChan;
    pContBuf->currentChan = connPtr->currentChan;
    pContBuf->currentMappedChan = connPtr->currentMappedChan;
    pContBuf->numUsedChans = connPtr->numUsedChans;
    pContBuf->hopLength = connPtr->hopLength;
    MAP_osal_memcpy(pContBuf->chanMapTable , connPtr->chanMapTable, LL_MAX_NUM_DATA_CHAN);
    MAP_osal_memcpy(pContBuf->chanMap, connPtr->curChanMap.chanMap, LL_NUM_BYTES_FOR_CHAN_MAP);

    /* Encryption Information */
    pContBuf->encEnabled = connPtr->encEnabled;
    MAP_osal_memcpy(pContBuf->IV, connPtr->encInfo.IV, LL_ENC_IV_LEN);
    MAP_osal_memcpy(pContBuf->SKD, connPtr->encInfo.SKD, LL_ENC_SKD_LEN);
    MAP_osal_memcpy(pContBuf->RAND, connPtr->encInfo.RAND, LL_ENC_RAND_LEN);
    MAP_osal_memcpy(pContBuf->EDIV, connPtr->encInfo.EDIV, LL_ENC_EDIV_LEN);
    MAP_osal_memcpy(pContBuf->nonce, connPtr->encInfo.nonce, LL_ENC_NONCE_LEN);
    MAP_osal_memcpy(pContBuf->SK, connPtr->encInfo.SK, LL_ENC_SK_LEN);
    MAP_osal_memcpy(pContBuf->LTK, connPtr->encInfo.LTK, LL_ENC_LTK_LEN);
    pContBuf->txPktCount = connPtr->encInfo.txPktCount;
    pContBuf->rxPktCount = connPtr->encInfo.rxPktCount;

    /* Feature Set */
    MAP_osal_memcpy(pContBuf->featureSet, connPtr->featureSetInfo.featureSet, LL_MAX_FEATURE_SET_SIZE);
    pContBuf->featureRspRcved = connPtr->featureSetInfo.featureRspRcved;

    /* Version Information */
    pContBuf->peerInfoValid = connPtr->verExchange.peerInfoValid;
    pContBuf->hostRequest = connPtr->verExchange.hostRequest;;
    pContBuf->verInfoSent = connPtr->verExchange.verInfoSent;;
    pContBuf->comId = connPtr->verInfo.comId;
    pContBuf->subverNum = connPtr->verInfo.subverNum;
    pContBuf->verNum = connPtr->verInfo.verNum;

    /* Peer Address Information */
    MAP_osal_memcpy(pContBuf->peerAddr, connPtr->peerInfo.peerAddr, LL_DEVICE_ADDR_LEN);
    pContBuf->peerAddrType = connPtr->peerInfo.peerAddrType;

    /* Central Contribution */
    pContBuf->mstSCA = connPtr->mstSCA;

    /* QoS */
    pContBuf->connPriority = connPtr->connPriority;

    /* Authenticated Payload Timeout  */
    pContBuf->numAptoExp = connPtr->numAptoExp;
    pContBuf->aptoValue = connPtr->aptoValue;

    /* Length Information */
    pContBuf->connMaxTxOctets = connPtr->lenInfo.connMaxTxOctets;
    pContBuf->connMaxRxOctets = connPtr->lenInfo.connMaxRxOctets;
    pContBuf->connMaxTxTime = connPtr->lenInfo.connMaxTxTime;
    pContBuf->connMaxRxTime = connPtr->lenInfo.connMaxRxTime;
    pContBuf->connRemoteMaxTxOctets = connPtr->lenInfo.connRemoteMaxTxOctets;
    pContBuf->connRemoteMaxRxOctets = connPtr->lenInfo.connRemoteMaxRxOctets;
    pContBuf->connRemoteMaxTxTime = connPtr->lenInfo.connRemoteMaxTxTime;
    pContBuf->connRemoteMaxRxTime = connPtr->lenInfo.connRemoteMaxRxTime;
    pContBuf->connIntervalPortionAvail = connPtr->lenInfo.connIntervalPortionAvail;
    pContBuf->lenFlags = connPtr->lenInfo.lenFlags;
    pContBuf->connSlowestPhy = connPtr->lenInfo.connSlowestPhy;

    /* PHY Information */
    pContBuf->curPhy = connPtr->phyInfo.curPhy;
    pContBuf->phyFlags = connPtr->phyInfo.phyFlags;
    pContBuf->phyPreference = connPtr->phyInfo.phyPreference;
    pContBuf->phyOpts = connPtr->phyInfo.phyOpts;

    /* Channel Selection Algorithm */
    pContBuf->chanSelAlgo = (connPtr->pChSelAlgo == MAP_llGetNextDataChanAlgo1) ? 1 : 2;
}

/*******************************************************************************
 * @fn          llHandoverInitSNParams
 *
 * @brief       This function is used to the serving node controller parameters
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
static void llHandoverInitSNParams(void)
{
  gSnLlParams.connHandle = LL_CONNHANDLE_INVALID;
  gSnLlParams.handoverDataSize = INVALID_DATA_SIZE;
  gSnLlParams.pHandoverData = NULL;
  gSnLlParams.handoverSnMode = 0;
}

/*******************************************************************************
 * @fn          llIsHandoverInProgress
 *
 * @brief       This function return the handover progress bit for the serving node
 *
 * input parameters
 *
 * @param       connPtr - Pointer to the connection
 *
 * @return      TRUE - Handover is in progress, otherwise, FALSE
 */
uint8 llIsHandoverInProgress( llConnState_t *connPtr )
{
  return connPtr->handoverInProg;
}

/*******************************************************************************
 * @fn          llReturnNonHandoverConn
 *
 * @brief       In case there are two connections and one of the connections is
 *              in the middle of handover process it will return the connection
 *              handle of the other connection. If non of the connections are in
 *              middle of handover process it will return invalid connection handle
 *              to allow the connection selection process to continue as usual
 *
 * input parameters
 *
 * @param       None.
 *
 * output parameters
 *
 * @param       None
 *
 * @return      Connection ID.
 */
uint16 llReturnNonHandoverConn( void )
{
  llConnState_t	*connPtr = NULL;
  uint16 handoverConnId = LL_CONNHANDLE_INVALID;
  uint16 connId = LL_CONNHANDLE_INVALID;
  uint8 i;

  for (i = 0; i < maxNumConns; i++)
  {
    connPtr = MAP_llDataGetConnPtr( i );
    if ( connPtr != NULL && connPtr->activeConn == TRUE)
    {
      if ( MAP_llIsHandoverInProgress(connPtr) )
      {
        // Save the handover connection ID
        handoverConnId = connPtr->connId;
      }
      else
      {
        // Save the connection ID that is not in the middle of an
        // handover process
        connId = connPtr->connId;
      }
    }
  }

  // There isn't a connection in the middle of an handover process
  if ( handoverConnId == LL_CONNHANDLE_INVALID )
  {
    connId = LL_CONNHANDLE_INVALID;
  }

  return connId;
}

/*******************************************************************************
 * @fn          llHandoverCheckTermConnAndTerm
 *
 * @brief       This function is used to check if the connection needed to be
 *              terminated. This function must be called after a command was
 *              finished and before the scheduler is called again
 *
 * input parameters
 *
 * @param       None
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None.
 */
void llHandoverCheckTermConnAndTerm( void )
{
  llConnState_t *connPtr;

  if ( gSnLlParams.connHandle != LL_CONNHANDLE_INVALID )
  {
    connPtr = MAP_llDataGetConnPtr(gSnLlParams.connHandle);

    if ( connPtr != NULL )
    {
      if ( connPtr->termInfo.termIndRcvd == TRUE )
      {
        MAP_llConnTerminate(connPtr, connPtr->termInfo.reason);
        llHandoverInitSNParams();
      }
    }
  }
}
