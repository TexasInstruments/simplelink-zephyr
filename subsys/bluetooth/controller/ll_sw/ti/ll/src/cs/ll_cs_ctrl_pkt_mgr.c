/******************************************************************************

  @file  ll_cs_ctrl_pkt_mgr.c

 @brief CS packet manager. Handles sending and receiving CS packets
        Processing and building those packets.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2022 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*******************************************************************************
 * INCLUDES
 */
#include "cs/ll_cs_db.h"
#include "cs/ll_cs_ctrl_pkt_mgr.h"
#include "cs/ll_cs_ctrl_pkt_internal.h"
#include "cs/ll_cs_mgr.h"
#include "cs/ll_cs_sec.h"
#include "cs/ll_cs_common.h"
#include "cs/ll_cs_procedure.h"
#include "ble.h"
#include "rom_jt.h"
#include "ll_enc.h"
#include "osal_bufmgr.h"
#include "hci_data.h"

#include <ti/log/Log.h>
/*******************************************************************************
 * CONSTANTS
 */
#define CS_DEFAULT_CONNEVENT_OFFSET    5 /* The default connEvent offset to start the CS procedure */
#define CS_T_MEAS_MIN                  150 /* The minimum subevent space. Units: us */
#define CS_SUBEVENT_SPACE              CS_T_MEAS_MIN + 20000 /* 20000 us is the time it currently takes us to finish reporting a subevent and generate new steps */

/*******************************************************************************
 * MACROS
 */
/* Subevent interval is just the subevent Len and the subevent spacing in 0.625 ms */
#define CS_SUBEVENT_INTERVAL( subeventLen ) ((subeventLen + CS_SUBEVENT_SPACE)/625)
/* Events Per Procedure is the number of whole events that would fit into a CS procedure Len */
#define CS_EVENTS_PER_PROCEDURE(procedureLen, eventInterval, connInterval) (procedureLen / (eventInterval * connInterval))
/* Select the MAX offset such that at least a single subevent interval fits into a connInterval */
#define CS_OFFSET_MAX(connInterval, subeventInterval) ((connInterval - subeventInterval) * 625)

#define CS_US_TO_625US( val ) (val/625)

/*******************************************************************************
 * EXTERNS
 */

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
 * Public function defined in ll_cs_ctrl_pkt_mgr.h
 */
csStatus_e llCsProcessCsControlPacket(uint8 ctrlType, llConnState_t* connPtr,
                                    uint8* pBuf)
{
    csStatus_e status = CS_STATUS_UNKNOWN_CTRL_PKT;

    // check conn ptr
    if (connPtr == NULL)
    {
        return (CS_STATUS_INVALID_CONN_PTR);
    }
    // check buffer
    if (pBuf == NULL)
    {
        return (CS_STATUS_INVALID_BUFFER);
    }
    // check feature support
    if (!(connPtr->featureSetInfo.featureSet[5] & LL_FEATURE_CS) ||
        !(connPtr->featureSetInfo.featureSet[5] & LL_FEATURE_CS_HOST))
    {
        // reject the request
        MAP_llSendReject(connPtr, ctrlType,
                         LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE);
        return (CS_STATUS_UNSUPPORTED_FEATURE);
    }
    // check if the connection is encrypted
    if (!connPtr->encEnabled)
    {
        // reject the request
        MAP_llSendReject(connPtr, ctrlType,
                         LL_STATUS_ERROR_INSUFFICIENT_SECURITY);
        return (CS_STATUS_COMMAND_DISALLOWED);
    }

    // process control packet
    switch (ctrlType)
    {
        case LL_CTRL_CS_CAPABILITIES_REQ:
        case LL_CTRL_CS_CAPABILITIES_RSP:
        {
            csCapabilities_t* peerCaps = (csCapabilities_t*)&pBuf[0];

            llCsDbSetPeerCapabilities(connPtr->connId, peerCaps);

            llCsDbMarkProcedureCompleted(connPtr->connId,
                                         CS_CAPABILITIES_EXCHANGE_PROCEDURE);

            // setup a response
            if (ctrlType == LL_CTRL_CS_CAPABILITIES_REQ)
            {
                if (llCsSetupCtrlPkt(connPtr, LL_CTRL_CS_CAPABILITIES_RSP,
                                     LL_CS_CAPABILITIES_RSP_PAYLOAD_LEN) ==
                    FALSE)
                {
                    // unable to malloc a packet!
                    (void)MAP_osal_set_event(LL_TaskID, LL_EVT_OUT_OF_MEMORY);
                    status = CS_STATUS_INSUFFICIENT_MEMORY;
                    break;
                }
            }

            csCapabilities_t peerCapabilities = *peerCaps;
            // yes it has, so provide it to the host
            MAP_HCI_CS_ReadRemoteSupportedCapabilitiesCback(
                LL_STATUS_SUCCESS, connPtr->connId, &peerCapabilities);
            status = CS_STATUS_SUCCESS;
            break;
        }

        case LL_CTRL_CS_CONFIG_REQ:
        {
            csConfigurationSet_t* configReq = (csConfigurationSet_t*)&pBuf[0];

            if (configReq->state == CS_DISABLE)
            {
                llCsDbRemoveConfiguration(connPtr->connId, configReq->configId);
                llCsDbGetConfiguration(connPtr->connId, configReq->configId,
                                       configReq);
            }
            else
            {
                // Check if the recieved params are acceptable
                if ((llCsConfigurationCheck(connPtr->connId, configReq) ==
                     LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED) ||
                    (llCsConfigIdSafeToUse(connPtr->connId,
                                           configReq->configId) !=
                     CS_STATUS_SUCCESS))
                {
                    // reject the request
                    MAP_llSendReject(connPtr, ctrlType,
                                     LL_STATUS_ERROR_UNSUPPORTED_PARAM_VAL);
                    return (CS_STATUS_UNSUPPORTED_FEATURE);
                }

                // Take the opposite CS role
                configReq->role = (configReq->role == CS_ROLE_INITIATOR)
                                      ? CS_ROLE_REFLECTOR
                                      : CS_ROLE_INITIATOR;

                // set CS config
                llCsDbSetConfiguration(connPtr->connId, configReq);
            }

            llCsDbSetCurrentConfigId(connPtr->connId, configReq->configId);
            // setup ctrl packet
            if (llCsSetupCtrlPkt(connPtr, LL_CTRL_CS_CONFIG_RSP,
                                 LL_CS_CONFIG_RSP_PL_LEN) == FALSE)
            {
                // unable to malloc a packet!
                (void)MAP_osal_set_event(LL_TaskID, LL_EVT_OUT_OF_MEMORY);
                status = CS_STATUS_INSUFFICIENT_MEMORY;
                break;
            }

            // If this procedure was not marked as completed, mark it as such
            if (!llCsDbIsProcdureCompleted(connPtr->connId,
                                           CS_CONFIG_PROCEDURE))
            {
                llCsDbMarkProcedureCompleted(connPtr->connId,
                                             CS_CONFIG_PROCEDURE);
            }

            // send LE_CS_Config_Complete event
            MAP_HCI_CS_ConfigCompleteCback(LL_STATUS_SUCCESS, connPtr->connId,
                                           configReq);
            llCsDbRemoveCurrentConfigId(connPtr->connId);
            status = CS_STATUS_SUCCESS;
            break;
        }

        case LL_CTRL_CS_CONFIG_RSP:
        {
            uint8_t configID = pBuf[0];
            csConfigurationSet_t configSet;

            llCsDbMarkProcedureCompleted(connPtr->connId, CS_CONFIG_PROCEDURE);

            llCsDbGetConfiguration(connPtr->connId, configID, &configSet);

            MAP_HCI_CS_ConfigCompleteCback(LL_STATUS_SUCCESS, connPtr->connId,
                                           &configSet);
            llCsDbRemoveCurrentConfigId(connPtr->connId);
            status = CS_STATUS_SUCCESS;
            break;
        }

        case LL_CTRL_CS_SEC_REQ:
        {
#ifdef DRBG_TEST
            // peri according to sample datat test
            csSecVectors_t ownSecurityVectors = {
                .CSIV = {0xE1, 0x0B, 0xC2, 0x8A, 0x0B, 0xFD, 0xDF, 0xE9},
                .CSIN = {0x9F, 0xF4, 0x77, 0xC1},
                .CSPV = {0xC9, 0x80, 0xDE, 0xDF, 0x98, 0x82, 0xED, 0x44},
            };
#else
            csSecVectors_t ownSecurityVectors;

            // Generate Own Security Vectors
            llCsSecGenerateIN(ownSecurityVectors.CSIN);
            llCsSecGenerateIV(ownSecurityVectors.CSIV);
            llCsSecGeneratePV(ownSecurityVectors.CSPV);
#endif
            // Set Own Security Vectors in the DB
            llCsDbSetSecurityVectors(connPtr->connId, &ownSecurityVectors,
                                     CS_SEC_NO_OFFSET);

            // Pasre peer security Vectors and set in DB (combined)
            llCsParseSecurityData(pBuf, connPtr->connId, CS_SEC_USE_OFFSET);

            // setup ctrl packet
            if (llCsSetupCtrlPkt(connPtr, LL_CTRL_CS_SEC_RSP,
                                 LL_CS_SEC_RSP_PL_LEN) == FALSE)
            {
                // unable to malloc a packet!
                (void)MAP_osal_set_event(LL_TaskID, LL_EVT_OUT_OF_MEMORY);
                status = CS_STATUS_INSUFFICIENT_MEMORY;
                break;
            }
            llCsDbMarkProcedureCompleted(connPtr->connId,
                                         CS_SECURITY_PROCEDURE);
            MAP_HCI_CS_SecurityEnableCompleteCback(LL_STATUS_SUCCESS,
                                                   connPtr->connId);

            status = CS_STATUS_SUCCESS;
            break;
        }

        case LL_CTRL_CS_SEC_RSP:
        {
            // Pasre peer security Vectors and set in DB (combined)
            llCsParseSecurityData(pBuf, connPtr->connId, CS_SEC_NO_OFFSET);
            llCsDbMarkProcedureCompleted(connPtr->connId,
                                         CS_SECURITY_PROCEDURE);
            MAP_HCI_CS_SecurityEnableCompleteCback(LL_STATUS_SUCCESS,
                                                   connPtr->connId);
            status = CS_STATUS_SUCCESS;
            break;
        }

        case LL_CTRL_CS_FAE_REQ:
        {
            csFaeTbl_t localFaeTbl;
            if (llCsDbGetLocalFaeTbl(&localFaeTbl) ==
                CS_STATUS_UNSUPPORTED_FEATURE)
            {
                // reject the request
                MAP_llSendReject(connPtr, ctrlType,
                                 LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE);
                status = CS_STATUS_UNSUPPORTED_FEATURE;
                break;
            }
            else
            {
                // setup ctrl packet
                if (llCsSetupCtrlPkt(connPtr, LL_CTRL_CS_FAE_RSP,
                                     LL_CS_FAE_RSP_PL_LEN) == FALSE)
                {
                    // unable to malloc a packet!
                    (void)MAP_osal_set_event(LL_TaskID, LL_EVT_OUT_OF_MEMORY);
                    status = CS_STATUS_INSUFFICIENT_MEMORY;
                    break;
                }
            }
            llCsDbMarkProcedureCompleted(connPtr->connId,
                                         CS_FAE_TABLE_UPDATE_PROCEDURE);

            status = CS_STATUS_SUCCESS;
            break;
        }

        case LL_CTRL_CS_FAE_RSP:
        {
            csFaeTbl_t* peerFaeTbl = (csFaeTbl_t*)&pBuf[0];
            if (llCsDbSetRemoteFaeTbl(connPtr->connId, peerFaeTbl) ==
                CS_STATUS_INSUFFICIENT_MEMORY)
            {
                // unable to malloc memory for FAE table
                (void)MAP_osal_set_event(LL_TaskID, LL_EVT_OUT_OF_MEMORY);
                status = CS_STATUS_INSUFFICIENT_MEMORY;
                break;
            }

            llCsDbMarkProcedureCompleted(connPtr->connId,
                                         CS_FAE_TABLE_UPDATE_PROCEDURE);

            MAP_HCI_CS_ReadRemoteFAETableCompleteCback(
                LL_STATUS_SUCCESS, connPtr->connId, (uint8*)peerFaeTbl);
            status = CS_STATUS_SUCCESS;
            break;
        }

        case LL_CTRL_CS_CHANNEL_MAP_IND:
        {
            // TODO #BLE_LOKI-984
            status = CS_STATUS_SUCCESS;
            break;
        }

        case LL_CTRL_CS_REQ:
        {
            csProcedureEnable_t csReq = {0};

            llCsParseCsReqData(&csReq, &pBuf[0]);

            if (llCsConfigIdCheck(connPtr->connId, csReq.configId))
            {
                /* invalid configId */
                MAP_llSendReject(connPtr, ctrlType, CS_STATUS_INVALID_LL_PARAM);
                return (CS_STATUS_INVALID_LL_PARAM);
            }

            llCsDbSetCurrentConfigId(connPtr->connId, csReq.configId);

            /* Negotiate CS REQ */
            /* Note, this procedure may modify the pointer csReq */
            if ((status = llCsNegotiateCsReq(connPtr, &csReq)) !=
                CS_STATUS_SUCCESS)
            {
                /* Cannot agree. Reject */
                MAP_llSendReject(connPtr, ctrlType, status);
                return (status);
            }

            /* set the procedure request data in the db */
            llCsDbSetProcedureEnableData(connPtr->connId, csReq.configId,
                                         &csReq);
            /* set the enable flag */
            llCsDbEnableProcedureParams(connPtr->connId, csReq.configId,
                                        CS_ENABLE);
            /* set events per procedure */
            llCsDbSetEventsPerProcedure(
                connPtr->connId, CS_EVENTS_PER_PROCEDURE(
                                     csReq.maxProcedureDur, csReq.eventInterval,
                                     connPtr->curParam.connInterval));
            // if central, send CS_IND
            if (connPtr->llTask->taskID == LL_TASK_ID_CENTRAL)
            {
                // Enqueue the ctrl packet
                MAP_llEnqueueCtrlPkt(connPtr, LL_CTRL_CS_IND);
                llCsDbMarkProcedureCompleted(connPtr->connId,
                                             CS_START_PROCEDURE);

                MAP_HCI_CS_ProcedureEnableCompleteCback(
                    LL_STATUS_SUCCESS, connPtr->connId, CS_ENABLE, &csReq);
            }
            else
            {
                // peripheral, send CS_RSP
                // setup ctrl packet
                if (llCsSetupCtrlPkt(connPtr, LL_CTRL_CS_RSP,
                                     LL_CS_RSP_PL_LEN) == FALSE)
                {
                    // unable to malloc a packet!
                    (void)MAP_osal_set_event(LL_TaskID, LL_EVT_OUT_OF_MEMORY);
                    status = CS_STATUS_INSUFFICIENT_MEMORY;
                    break;
                }
            }

            status = CS_STATUS_SUCCESS;
            break;
        }

        case LL_CTRL_CS_RSP:
        {
            csProcedureEnable_t csRsp = {0};
            uint8 configId = pBuf[0];

            if (llCsConfigIdCheck(connPtr->connId, configId))
            {
                /* invalid configId */
                MAP_llSendReject(connPtr, ctrlType, CS_STATUS_INVALID_LL_PARAM);
                return (CS_STATUS_INVALID_LL_PARAM);
            }
            /* load own data */
            llCsDbGetProcedureEnableData(connPtr->connId, configId, &csRsp);
            /* update it locally with response data */
            llCsParseCsRspData(&csRsp, &pBuf[0]);

            if ((status = llCsNegotiateCsRsp(connPtr, &csRsp)) !=
                CS_STATUS_SUCCESS)
            {
                /* Cannot agree. Reject */
                MAP_llSendReject(connPtr, ctrlType, status);
                return (status);
            }
            llCsDbSetProcedureEnableData(connPtr->connId, csRsp.configId,
                                         &csRsp);
            llCsDbSetEventsPerProcedure(
                connPtr->connId, CS_EVENTS_PER_PROCEDURE(
                                     csRsp.maxProcedureDur, csRsp.eventInterval,
                                     connPtr->curParam.connInterval));
            // Enqueue the ctrl pkt
            MAP_llEnqueueCtrlPkt(connPtr, LL_CTRL_CS_IND);
            llCsDbMarkProcedureCompleted(connPtr->connId, CS_START_PROCEDURE);
            // mark CS procedure as active
            llCsDbSetActiveProcedure(connPtr->connId, CS_IND);

            MAP_HCI_CS_ProcedureEnableCompleteCback(
                LL_STATUS_SUCCESS, connPtr->connId, CS_ENABLE, &csRsp);
            status = CS_STATUS_SUCCESS;
            break;
        }

        case LL_CTRL_CS_IND:
        {
            csProcedureEnable_t csInd;
            uint8 configId = pBuf[0];

            if (llCsConfigIdCheck(connPtr->connId, configId))
            {
                /* invalid configId */
                MAP_llSendReject(connPtr, ctrlType, CS_STATUS_INVALID_LL_PARAM);
                return (CS_STATUS_INVALID_LL_PARAM);
            }
            /* Load own procedure data */
            llCsDbGetProcedureEnableData(connPtr->connId, configId, &csInd);
            /* update with received IND data */
            llCsParseCsIndData(&csInd, &pBuf[0]);

            /* Check if IND params are OK */
            if ((status = llCsConfirmInd(connPtr, &csInd)) != CS_STATUS_SUCCESS)
            {
                /* Something went wrong */
                return (status);
            }

            // Set the enable flag
            llCsDbEnableProcedureParams(connPtr->connId, csInd.configId,
                                        CS_ENABLE);
            llCsDbSetEventsPerProcedure(
                connPtr->connId, CS_EVENTS_PER_PROCEDURE(
                                     csInd.maxProcedureDur, csInd.eventInterval,
                                     connPtr->curParam.connInterval));
            // Copy the indication data
            llCsDbSetProcedureEnableIndData(connPtr->connId, csInd.configId,
                                            &csInd);
            llCsDbMarkProcedureCompleted(connPtr->connId, CS_START_PROCEDURE);
            llCsDbMarkProcedureCompleted(connPtr->connId, CS_IND);
            MAP_HCI_CS_ProcedureEnableCompleteCback(
                LL_STATUS_SUCCESS, connPtr->connId, CS_ENABLE, &csInd);

            status = CS_STATUS_SUCCESS;
            break;
        }

        case LL_CTRL_CS_TERMINATE_IND:
        {
            uint8 configId = pBuf[0];
            csProcedureEnable_t enData;

            if (llCsConfigIdSafeToUse(connPtr->connId, configId) ==
                CS_STATUS_CONFIG_ENABLED)
            {
                llCsDbGetProcedureEnableData(connPtr->connId, configId,
                                             &enData);

                /* Set terminateState field to CS_TERMINATE RECEIVED */
                llCsDbSetProcedureTerminateState(connPtr->connId, configId,
                                                 CS_TERMINATE_RECEIVED);

                /* Notify Host */
                MAP_HCI_CS_ProcedureEnableCompleteCback(
                    LL_STATUS_SUCCESS, connPtr->connId, CS_DISABLE, &enData);

                /* Mark CS_IND procedure as completed */
                llCsDbMarkProcedureCompleted(connPtr->connId, CS_IND);

                /* update enable field to CS_DISABLE */
                llCsDbEnableProcedureParams(connPtr->connId, configId,
                                            CS_DISABLE);
            }

            status = CS_STATUS_SUCCESS;
            break;
        }
    }
    return status;
}

/*******************************************************************************
 * Public function defined in ll_cs_ctrl_pkt_mgr.h
 */
uint8 llCsProcessCsCtrlProcedures(llConnState_t* connPtr, uint8 ctrlPkt)
{
    switch (ctrlPkt)
    {
        case LL_CTRL_CS_CAPABILITIES_REQ:
        {
            return llCsProcessCsCtrlProcedure(
                connPtr, ctrlPkt, CS_CAPABILITIES_EXCHANGE_PROCEDURE,
                LL_CS_CAPABILITIES_REQ_PAYLOAD_LEN);
            break;
        }

        case LL_CTRL_CS_CONFIG_REQ:
        {
            return llCsProcessCsCtrlProcedure(
                connPtr, ctrlPkt, CS_CONFIG_PROCEDURE, LL_CS_CONFIG_REQ_PL_LEN);
            break;
        }

        case LL_CTRL_CS_SEC_REQ:
        {
            return llCsProcessCsCtrlProcedure(
                connPtr, ctrlPkt, CS_SECURITY_PROCEDURE, LL_CS_SEC_REQ_PL_LEN);
            break;
        }

        case LL_CTRL_CS_FAE_REQ:
        {
            return llCsProcessCsCtrlProcedure(connPtr, ctrlPkt,
                                              CS_FAE_TABLE_UPDATE_PROCEDURE,
                                              LL_CS_FAE_REQ_PL_LEN);
            break;
        }

        case LL_CTRL_CS_CHANNEL_MAP_IND:
        {
            break;
        }
        case LL_CTRL_CS_REQ:
        {
            if (connPtr->llTask->taskID == LL_TASK_ID_CENTRAL)
            {
                return llCsProcessCsCtrlProcedure(
                    connPtr, ctrlPkt, CS_START_PROCEDURE, LL_CS_REQ_PL_LEN);
            }
            else
            {
                return llCsProcessCsCtrlProcedure(connPtr, ctrlPkt, CS_IND,
                                                  LL_CS_REQ_PL_LEN);
            }
            break;
        }

        case LL_CTRL_CS_IND:
        {
            if (connOutput.nTxCtlAck)
            {
                llCsDbMarkProcedureCompleted(connPtr->connId, CS_IND);
            }
            return llCsProcessCsCtrlProcedure(connPtr, ctrlPkt, CS_IND,
                                              LL_CS_IND_PL_LEN);
            break;
        }

        case LL_CTRL_CS_TERMINATE_IND:
        {
            csProcedureEnable_t enData;

            uint8 configId = llCsDbGetCurrentConfigId(connPtr->connId);
            llCsDbGetProcedureEnableData(connPtr->connId, configId, &enData);

            if (!llCsDbIsProcdureCompleted(connPtr->connId, CS_IND))
            {
                llCsDbMarkProcedureCompleted(connPtr->connId, CS_IND);
            }
            else
            {
                MAP_HCI_CS_ProcedureEnableCompleteCback(
                    LL_STATUS_SUCCESS, connPtr->connId, CS_DISABLE, &enData);
            }

            return llCsProcessCsCtrlProcedure(connPtr, ctrlPkt, CS_IND,
                                              LL_CS_TERMINATE_IND_PL_LEN);
            break;
        }
    }
    return (LL_CTRL_PROC_STATUS_SUCCESS);
}

/*******************************************************************************
 *  Internal function defined in ll_cs_ctrl_pkt_mgr.h
 */
uint8 llCsProcessCsCtrlProcedure(llConnState_t* connPtr, uint8 ctrlPkt,
                                 uint8 procedure, uint8 payloadLen)
{
    // check if the control packet procedure is active
    if (connPtr->ctrlPktInfo.ctrlPktActive == TRUE)
    {
        // check if the CS procedure was completed
        if (llCsDbIsProcdureCompleted(connPtr->connId, procedure))
        {
            // it has been sent, so dequeue this control procedure
            MAP_llDequeueCtrlPkt(connPtr);

            llCsDbResetProcedureCompletedFlag(connPtr->connId, procedure);

            return (LL_CTRL_PROC_STATUS_SUCCESS);
        }
        else // not done yet
        {
            // check if a control procedure timeout has occurred
            // Note: No need to cleanup control packet info as we are done.
            if (--connPtr->ctrlPktInfo.ctrlTimeout == 0)
            {
                // CPTO timeout, so end it all
                // Note: No need to cleanup control packet info as we are done.
                MAP_llConnTerminate(connPtr, LL_CTRL_PKT_TIMEOUT_HOST_TERM);

                return (LL_CTRL_PROC_STATUS_TERMINATE);
            }
            else
            {
                //  control packet stays at head of queue, so exit here
                return (LL_CTRL_PROC_STATUS_SUCCESS);
            }
        }
    }
    else // control packet has not been put on the TX FIFO yet
    {
        // so try to put it there; being active depends on a success
        if ((connPtr->ctrlPktInfo.ctrlPktActive =
                 llCsSetupCtrlPkt(connPtr, ctrlPkt, payloadLen)) == TRUE)
        {
            // mark CS procedure as active
            llCsDbSetActiveProcedure(connPtr->connId, procedure);
        }
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
        return (LL_CTRL_PROC_STATUS_SUCCESS);
    }
}

/*******************************************************************************
 * Internal function defined in ll_cs_ctrl_pkt_internal.h
 */
uint8 llCsSetupCtrlPkt(llConnState_t* connPtr, uint8 ctrlType, uint8 ctrlLen)
{
    // allocate a data entry and payload to send control packet
    uint8* pData = MAP_LL_TX_bm_alloc(ctrlLen);

    // check if we have a data entry
    if (pData != NULL)
    {
        RCL_Buffer_TxBuffer* dataEntry =
            (RCL_Buffer_TxBuffer*)(pData - ((sizeof(RCL_Buffer_TxBuffer)) +
                                            RCL_BUFFER_MAX_HEADER_PAD_BYTES +
                                            LL_PKT_HDR_LEN));
        MAP_llSetupDataEntry(dataEntry, ctrlLen, connPtr->encEnabled);
        // point to the payload
        pData = &dataEntry->data[4];

        // write control type
        pData[0] = ctrlType;

        switch (ctrlType)
        {
            case LL_CTRL_CS_CAPABILITIES_REQ:
            case LL_CTRL_CS_CAPABILITIES_RSP:
            {
                llCsSetupCapabilities(&pData[1]);
                break;
            }

            case LL_CTRL_CS_CONFIG_REQ:
            {
                uint8 configId = llCsDbGetCurrentConfigId(connPtr->connId);

                llCsSetupConfigData(&pData[1], connPtr->connId, configId);

                break;
            }

            case LL_CTRL_CS_CONFIG_RSP:
            {
                pData[1] = llCsDbGetCurrentConfigId(connPtr->connId);
                break;
            }

            case LL_CTRL_CS_SEC_REQ:
            {
                if ((llCsSetupSecurityData(&pData[1], connPtr->connId,
                                           CS_SEC_USE_OFFSET)) !=
                    CS_STATUS_SUCCESS)
                {
                    return FALSE;
                }
                break;
            }

            case LL_CTRL_CS_SEC_RSP:
            {
                if ((llCsSetupSecurityData(&pData[1], connPtr->connId,
                                           CS_SEC_NO_OFFSET)) !=
                    CS_STATUS_SUCCESS)
                {
                    return FALSE;
                }
                break;
            }

            case LL_CTRL_CS_FAE_REQ:
            {
                // no data for this ctrl pkt
                break;
            }

            case LL_CTRL_CS_FAE_RSP:
            {
                llCsSetupFaeTblData(&pData[1]);
                break;
            }

            case LL_CTRL_CS_CHANNEL_MAP_IND:
            {
                llCsSetupChmInd(&pData[1], connPtr->connId);
                break;
            }

            case LL_CTRL_CS_REQ:
            {
                llCsSetupCsReq(&pData[1], connPtr->connId);
                break;
            }

            case LL_CTRL_CS_RSP:
            {
                llCsSetupCsRsp(&pData[1], connPtr->connId);
                break;
            }

            case LL_CTRL_CS_IND:
            {
                llCsSetupCsInd(&pData[1], connPtr->connId);
                break;
            }

            case LL_CTRL_CS_TERMINATE_IND:
            {
                llCsSetupTerminateInd(&pData[1], connPtr->connId);
                break;
            }

            default:
                MAP_osal_bm_free(pData);
                return FALSE;
        }

        // encrypt TX packet in place in the TX FIFO
        if (connPtr->encEnabled)
        {
            // encrypt PDU with authentication check
            MAP_LL_ENC_Encrypt(connPtr, LL_DATA_PDU_HDR_LLID_CONTROL_PKT,
                               ctrlLen, pData);
        }

        // queue it on connection TX list and queue for RF
        MAP_llAddTxDataEntry(connPtr->pTxDataEntryQ, dataEntry);

        // deactivate peripheral latency, if it was enabled
        // Note: Not used by Central.
        connPtr->peripheralLatency = 0;

        // set the control packet timeout for 40s relative to our present time
        // Note: This is done in terms of connection events.
        connPtr->ctrlPktInfo.ctrlTimeout = connPtr->ctrlPktInfo.ctrlTimeoutVal;

        return (TRUE);
    }
    return (FALSE);
}

/*******************************************************************************
 * Internal function defined in ll_cs_ctrl_pkt_internal.h
 */
void llCsSetupCapabilities(uint8* data)
{
    csCapabilities_t localCsCapabilities;

    llCsDbGetLocalCapabilities(&localCsCapabilities);

    MAP_osal_memcpy(data, &localCsCapabilities, sizeof(csCapabilities_t));
}

/*******************************************************************************
 * Public function defined in ll_cs_ctrl_pkt_mgr.h
 */
uint8 llCsSetupConfigData(uint8* data, uint16 connId, uint8 configId)
{
    csConfigurationSet_t configSet;
    llCsDbGetConfiguration(connId, configId, &configSet);
    MAP_osal_memcpy(data, &configSet, sizeof(csConfigurationSet_t));
    return CS_STATUS_SUCCESS;
}

/*******************************************************************************
 * Public function defined in ll_cs_ctrl_pkt_mgr.h
 */
uint8 llCsSetupSecurityData(uint8* data, uint16 connId, uint8 offset)
{
    csSecVectors_t pSecVec;

    /* make sure offset value is valid */
    if (offset > CS_SEC_USE_OFFSET)
    {
        offset = CS_SEC_USE_OFFSET;
    }

    llCsDbGetSecurityVectors(connId, &pSecVec);

    if (offset == CS_SEC_USE_OFFSET)
    {
        MAP_HCI_ReverseBytes(pSecVec.CSIV + (CS_CSIV_LEN / 2) * offset,
                             CS_IV_C_LEN);
        MAP_HCI_ReverseBytes(pSecVec.CSIN + (CS_CSIN_LEN / 2) * offset,
                             CS_IN_C_LEN);
        MAP_HCI_ReverseBytes(pSecVec.CSPV + (CS_CSPV_LEN / 2) * offset,
                             CS_PV_C_LEN);
    }
    else
    {
        MAP_HCI_ReverseBytes(pSecVec.CSIV, CS_IV_C_LEN);
        MAP_HCI_ReverseBytes(pSecVec.CSIN, CS_IN_C_LEN);
        MAP_HCI_ReverseBytes(pSecVec.CSPV, CS_PV_C_LEN);
    }

    MAP_osal_memcpy(data, pSecVec.CSIV + (CS_CSIV_LEN / 2) * offset,
                    CS_CSIV_LEN / 2);
    data += CS_CSIV_LEN / 2;
    MAP_osal_memcpy(data, pSecVec.CSIN + (CS_CSIN_LEN / 2) * offset,
                    CS_CSIN_LEN / 2);
    data += CS_CSIN_LEN / 2;
    MAP_osal_memcpy(data, pSecVec.CSPV + (CS_CSPV_LEN / 2) * offset,
                    CS_CSPV_LEN / 2);

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_ctrl_pkt_mgr.h
 */
void llCsParseSecurityData(uint8* data, uint16 connId, uint8 offset)
{
    csSecVectors_t secVectors;

    MAP_osal_memcpy(secVectors.CSIV, data, CS_CSIV_LEN / 2);

    data += (CS_CSIV_LEN / 2);
    MAP_osal_memcpy(secVectors.CSIN, data, CS_CSIN_LEN / 2);

    data += (CS_CSIN_LEN / 2);
    MAP_osal_memcpy(secVectors.CSPV, data, CS_CSPV_LEN / 2);

    MAP_HCI_ReverseBytes(secVectors.CSIV, CS_IV_C_LEN);
    MAP_HCI_ReverseBytes(secVectors.CSIN, CS_IN_C_LEN);
    MAP_HCI_ReverseBytes(secVectors.CSPV, CS_PV_C_LEN);

    llCsDbSetSecurityVectors(connId, &secVectors, offset);
}

/*******************************************************************************
 * Public function defined in ll_cs_ctrl_pkt_mgr.h
 */
void llCsSetupFaeTblData(uint8* data)
{
    csFaeTbl_t faeTbl;
    llCsDbGetLocalFaeTbl(&faeTbl);
    MAP_osal_memcpy(data, (uint8*)&faeTbl, CS_FAE_TBL_LEN);
}

/*******************************************************************************
 * Public function defined in ll_cs_ctrl_pkt_mgr.h
 */
void llCsSetupCsReq(uint8* data, uint16 connId)
{
    llConnState_t* connPtr = MAP_llDataGetConnPtr(connId);
    uint8 configId = llCsDbGetCurrentConfigId(connId);
    csProcedureParams_t procedureParams; // default procedure params
    csProcedureEnable_t csReq = {0};

    llCsDbGetProcedureParams(connId, configId, &procedureParams);

    csReq.configId = configId;
    csReq.connEventCount = connPtr->nextEvent + CS_DEFAULT_CONNEVENT_OFFSET;
    csReq.offsetMin = CS_OFFSET_MIN;
    csReq.maxProcedureDur = procedureParams.maxProcedureDur; // 0.625 ms
    csReq.eventInterval = CS_MIN_EVENT_INTERVAL;
    csReq.procedureInterval = procedureParams.minProcedureInterval;
    csReq.ACI = procedureParams.toneAntennaConfigSelection;
    csReq.preferredPeerAntenna = procedureParams.preferredPeerAntenna;
    csReq.phy = procedureParams.phy;
    csReq.pwrDelta = procedureParams.txPwrDelta;
    csReq.subEventLen =
        llCsGetSubeventLen(procedureParams.maxSubEventLen, csReq.offsetMin,
                       csReq.maxProcedureDur, connPtr->curParam.connInterval);
    csReq.subEventInterval =
        CS_SUBEVENT_INTERVAL(procedureParams.maxSubEventLen);
    csReq.offsetMax =
        CS_OFFSET_MAX(connPtr->curParam.connInterval, csReq.subEventInterval);
    csReq.subEventsPerEvent = llCsSubeventsPerEvent(
        procedureParams.maxProcedureDur, connPtr->curParam.connInterval,
        csReq.subEventInterval);
    csReq.procedureCount = procedureParams.maxProcedureCount;

    if (csReq.subEventsPerEvent == 1)
    {
        csReq.subEventInterval = 0;
    }
    if (csReq.procedureCount == 1)
    {
        csReq.procedureInterval = 0;
    }

    /* set the procedure request data in the db */
    llCsDbSetProcedureEnableData(connId, configId, &csReq);
    llCsDbSetEventsPerProcedure(
        connId,
        CS_EVENTS_PER_PROCEDURE(csReq.maxProcedureDur, csReq.eventInterval,
                                connPtr->curParam.connInterval));
    llCsReq2Data(data, &csReq);
}

/*******************************************************************************
 * Internal function defined in ll_cs_ctrl_pkt_internal.h
 */
void llCsReq2Data(uint8* data, csProcedureEnable_t* csReq)
{
    *data++ = csReq->configId;
    *data++ = LO_UINT16(csReq->connEventCount);
    *data++ = HI_UINT16(csReq->connEventCount);
    *data++ = BREAK_UINT32(csReq->offsetMin, 0);
    *data++ = BREAK_UINT32(csReq->offsetMin, 1);
    *data++ = BREAK_UINT32(csReq->offsetMin, 2);
    *data++ = BREAK_UINT32(csReq->offsetMax, 0);
    *data++ = BREAK_UINT32(csReq->offsetMax, 1);
    *data++ = BREAK_UINT32(csReq->offsetMax, 2);
    *data++ = LO_UINT16(csReq->maxProcedureDur);
    *data++ = HI_UINT16(csReq->maxProcedureDur);
    *data++ = LO_UINT16(csReq->eventInterval);
    *data++ = HI_UINT16(csReq->eventInterval);
    *data++ = csReq->subEventsPerEvent;
    *data++ = LO_UINT16(csReq->subEventInterval);
    *data++ = HI_UINT16(csReq->subEventInterval);
    *data++ = BREAK_UINT32(csReq->subEventLen, 0);
    *data++ = BREAK_UINT32(csReq->subEventLen, 1);
    *data++ = BREAK_UINT32(csReq->subEventLen, 2);
    *data++ = LO_UINT16(csReq->procedureInterval);
    *data++ = HI_UINT16(csReq->procedureInterval);
    *data++ = LO_UINT16(csReq->procedureCount);
    *data++ = HI_UINT16(csReq->procedureCount);
    *data++ = csReq->ACI;
    *data++ = csReq->preferredPeerAntenna;
    *data++ = csReq->phy;
    *data++ = csReq->pwrDelta;
}

/*******************************************************************************
 * Internal function defined in ll_cs_ctrl_pkt_internal.h
 */
void llCsParseCsReqData(csProcedureEnable_t* csReq, uint8* pBuf)
{
    csReq_t* llCsReqPkt = (csReq_t*)pBuf;
    csReq->configId = llCsReqPkt->configId;
    csReq->connEventCount = llCsReqPkt->connEventCount;
    csReq->offsetMin = llCsReqPkt->offsetMin;
    csReq->offsetMax = llCsReqPkt->offsetMax;
    csReq->maxProcedureDur = llCsReqPkt->maxProcedureDur;
    csReq->eventInterval = llCsReqPkt->eventInterval;
    csReq->subEventsPerEvent = llCsReqPkt->subEventsPerEvent;
    csReq->subEventInterval = llCsReqPkt->subEventInterval;
    csReq->subEventLen = llCsReqPkt->subEventLen;
    csReq->procedureInterval = llCsReqPkt->procedureInterval;
    csReq->procedureCount = llCsReqPkt->procedureCount;
    csReq->ACI = llCsReqPkt->ACI;
    csReq->preferredPeerAntenna = llCsReqPkt->preferredPeerAntenna;
    csReq->phy = llCsReqPkt->phy;
    csReq->pwrDelta = llCsReqPkt->pwrDelta;
}

/*******************************************************************************
 * Internal function defined in ll_cs_ctrl_pkt_internal.h
 */
void llCsSetupCsRsp(uint8* data, uint16 connId)
{
    csProcedureEnable_t csRsp;
    llConnState_t* connPtr = MAP_llDataGetConnPtr(connId);
    uint8 configId = llCsDbGetCurrentConfigId(connId);

    llCsDbGetProcedureEnableData(connId, configId, &csRsp);
    llCsRsp2Data(data, &csRsp);
}

/*******************************************************************************
 * Internal function defined in ll_cs_ctrl_pkt_internal.h
 */
void llCsRsp2Data(uint8* data, csProcedureEnable_t* csRsp)
{
    *data++ = csRsp->configId;
    *data++ = LO_UINT16(csRsp->connEventCount);
    *data++ = HI_UINT16(csRsp->connEventCount);
    *data++ = BREAK_UINT32(csRsp->offsetMin, 0);
    *data++ = BREAK_UINT32(csRsp->offsetMin, 1);
    *data++ = BREAK_UINT32(csRsp->offsetMin, 2);
    *data++ = BREAK_UINT32(csRsp->offsetMax, 0);
    *data++ = BREAK_UINT32(csRsp->offsetMax, 1);
    *data++ = BREAK_UINT32(csRsp->offsetMax, 2);
    *data++ = LO_UINT16(csRsp->eventInterval);
    *data++ = HI_UINT16(csRsp->eventInterval);
    *data++ = csRsp->subEventsPerEvent;
    *data++ = LO_UINT16(csRsp->subEventInterval);
    *data++ = HI_UINT16(csRsp->subEventInterval);
    *data++ = BREAK_UINT32(csRsp->subEventLen, 0);
    *data++ = BREAK_UINT32(csRsp->subEventLen, 1);
    *data++ = BREAK_UINT32(csRsp->subEventLen, 2);
    *data++ = csRsp->ACI;
    *data++ = csRsp->phy;
    *data++ = csRsp->pwrDelta;
}

/*******************************************************************************
 * Internal function defined in ll_cs_ctrl_pkt_internal.h
 */
void llCsParseCsRspData(csProcedureEnable_t* csRsp, uint8* pBuf)
{
    csRsp_t* csRspPkt = (csRsp_t*)pBuf;
    csRsp->configId = csRspPkt->configId;
    csRsp->connEventCount = csRspPkt->connEventCount;
    csRsp->offsetMin = csRspPkt->offsetMin;
    csRsp->offsetMax = csRspPkt->offsetMax;
    csRsp->eventInterval = csRspPkt->eventInterval;
    csRsp->subEventsPerEvent = csRspPkt->subEventsPerEvent;
    csRsp->subEventInterval = csRspPkt->subEventInterval;
    csRsp->subEventLen = csRspPkt->subEventLen;
    csRsp->ACI = csRspPkt->ACI;
    csRsp->phy = csRspPkt->phy;
    csRsp->pwrDelta = csRspPkt->pwrDelta;
}

/*******************************************************************************
 * Internal function defined in ll_cs_ctrl_pkt_internal.h
 */
void llCsSetupCsInd(uint8* data, uint16 connId)
{
    csProcedureEnable_t csInd;
    uint8 configId = llCsDbGetCurrentConfigId(connId);
    llConnState_t* connPtr = MAP_llDataGetConnPtr(connId);

    llCsDbGetProcedureEnableData(connId, configId, &csInd);
    llCsInd2Data(data, &csInd);
}

/*******************************************************************************
 * Internal function defined in ll_cs_ctrl_pkt_internal.h
 */
void llCsInd2Data(uint8* data, csProcedureEnable_t* csInd)
{
    *data++ = csInd->configId;
    *data++ = LO_UINT16(csInd->connEventCount);
    *data++ = HI_UINT16(csInd->connEventCount);
    *data++ = BREAK_UINT32(csInd->offset, 0);
    *data++ = BREAK_UINT32(csInd->offset, 1);
    *data++ = BREAK_UINT32(csInd->offset, 2);
    *data++ = LO_UINT16(csInd->eventInterval);
    *data++ = HI_UINT16(csInd->eventInterval);
    *data++ = csInd->subEventsPerEvent;
    *data++ = LO_UINT16(csInd->subEventInterval);
    *data++ = HI_UINT16(csInd->subEventInterval);
    *data++ = BREAK_UINT32(csInd->subEventLen, 0);
    *data++ = BREAK_UINT32(csInd->subEventLen, 1);
    *data++ = BREAK_UINT32(csInd->subEventLen, 2);
    *data++ = csInd->ACI;
    *data++ = csInd->phy;
    *data++ = csInd->pwrDelta;
}

/*******************************************************************************
 * Internal function defined in ll_cs_ctrl_pkt_internal.h
 */
void llCsParseCsIndData(csProcedureEnable_t* csInd, uint8* pBuf)
{
    csInd_t* csIndPkt = (csInd_t*)pBuf;
    csInd->configId = csIndPkt->configId;
    csInd->connEventCount = csIndPkt->connEventCount;
    csInd->offset = csIndPkt->offset;
    csInd->eventInterval = csIndPkt->eventInterval;
    csInd->subEventsPerEvent = csIndPkt->subEventsPerEvent;
    csInd->subEventInterval = csIndPkt->subEventInterval;
    csInd->subEventLen = csIndPkt->subEventLen;
    csInd->ACI = csIndPkt->ACI;
    csInd->phy = csIndPkt->phy;
    csInd->pwrDelta = csIndPkt->pwrDelta;
}

/*******************************************************************************
 * Internal function defined in ll_cs_ctrl_pkt_internal.h
 */
void llCsSetupTerminateInd(uint8* data, uint16 connId)
{
    uint8 configId = llCsDbGetCurrentConfigId(connId);
    *data++ = configId;
    // Note: the following should be the error code. Will be implemented
    // as part of BLE_LOKI-1356
    *data++ = 0;
}

/*******************************************************************************
 * Internal function defined in ll_cs_ctrl_pkt_internal.h
 */
void llCsSetupChmInd(uint8* data, uint16 connId) { llCsDbGetChannelMap(data); }

/*******************************************************************************
 * Internal function defined in ll_cs_ctrl_pkt_internal.h
 */
csStatus_e llCsNegotiateCsReq(llConnState_t* connPtr, csProcedureEnable_t* csReq)
{
    csProcedureParams_t procParams;
    uint32 offsetMax;

    llCsDbGetProcedureParams(connPtr->connId, csReq->configId, &procParams);

    /* The following parameters are not negotiable.
     * so if we cannot handle, we reject. */
    if ((csReq->maxProcedureDur > procParams.maxProcedureDur))
    {
        return (CS_STATUS_UNSUPPORTED_FEATURE);
    }
    if (csReq->procedureCount > procParams.maxProcedureCount)
    {
        return (CS_STATUS_UNSUPPORTED_FEATURE);
    }

    /* Negotiate the rest of the params */
    if (csReq->connEventCount <= connPtr->nextEvent)
    {
        csReq->connEventCount =
            connPtr->nextEvent + CS_DEFAULT_CONNEVENT_OFFSET;
    }
    /* check event interval */
    if (csReq->eventInterval < CS_MIN_EVENT_INTERVAL)
    {
        csReq->eventInterval = CS_MIN_EVENT_INTERVAL;
    }
    /* check the offset */
    if ((csReq->offsetMin < CS_OFFSET_MIN))
    {
        if (csReq->offsetMax > CS_OFFSET_MIN)
        {
            /* cannot deal with a smaller minimum atm */
            csReq->offsetMin = CS_OFFSET_MIN;
        }
        else
        {
            // Known issue, min offset is big
            // TODO #BLE_LOKI-1370
            return (CS_STATUS_UNSUPPORTED_FEATURE);
        }
    }

    /* Ensure that the subevent interval and minimum offset do not exceed the
     * connInterval */
    if ((((csReq->offsetMin + T625MS2US(csReq->subEventInterval)) >
          T625MS2US(connPtr->curParam.connInterval))) ||
        (csReq->subEventLen > T625MS2US(csReq->maxProcedureDur)))
    {
        /* EXCEEDS. resuggest the parameters */
        csReq->subEventLen = llCsGetSubeventLen(
            csReq->subEventLen, csReq->offsetMin, csReq->maxProcedureDur,
            connPtr->curParam.connInterval);

        if (csReq->subEventLen == 0)
        {
            /* can't handle. reject */
            return (CS_STATUS_UNEXPECTED_PARAMETER);
        }
        csReq->subEventInterval = CS_SUBEVENT_INTERVAL(csReq->subEventLen);
        csReq->subEventsPerEvent = llCsSubeventsPerEvent(
            csReq->maxProcedureDur, connPtr->curParam.connInterval,
            csReq->subEventInterval);
        if (csReq->subEventsPerEvent == 1)
        {
            csReq->subEventInterval = 0;
        }
    }
    offsetMax =
        CS_OFFSET_MAX(connPtr->curParam.connInterval, csReq->subEventInterval);
    if (csReq->offsetMax > offsetMax)
    {
        csReq->offsetMax = offsetMax;
    }

    if (llCsCheckACI(csReq->ACI, csReq->configId, connPtr->connId) !=
        CS_STATUS_SUCCESS)
    {
        /* Select Alternative ACI */
        csReq->ACI = llCsSelectACI(csReq->configId, connPtr->connId);
    }

    if (csReq->phy != procParams.phy)
    {
        csReq->phy = procParams.phy;
    }
    /* TODO check PWR Delta #BLE_LOKI-1417*/

    if (connPtr->llTask->taskID == LL_TASK_ID_CENTRAL)
    {
        /* Select the offset to be used */
        /* prefer the smaller in the range */
        csReq->offset = csReq->offsetMin;
    }

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Internal function defined in ll_cs_ctrl_pkt_internal.h
 */
csStatus_e llCsNegotiateCsRsp(llConnState_t* connPtr, csProcedureEnable_t* csRsp)
{
    csProcedureParams_t procParams;
    uint32 offsetMax =
        CS_OFFSET_MAX(connPtr->curParam.connInterval, csRsp->subEventInterval);

    /* compare response to the request  */
    if (!llCsDbCompareProcedureData(connPtr->connId, csRsp->configId, csRsp))
    {
        /* some params were re-suggested. Check them */
        llCsDbGetProcedureParams(connPtr->connId, csRsp->configId, &procParams);

        if (csRsp->connEventCount <= connPtr->nextEvent)
        {
            csRsp->connEventCount =
                connPtr->nextEvent + CS_DEFAULT_CONNEVENT_OFFSET;
        }
        /* check event interval */
        if (csRsp->eventInterval < CS_MIN_EVENT_INTERVAL)
        {
            csRsp->eventInterval = CS_MIN_EVENT_INTERVAL;
        }
        /* check the offset */
        if ((csRsp->offsetMin < CS_OFFSET_MIN))
        {
            if (csRsp->offsetMax > CS_OFFSET_MIN)
            {
                /* cannot deal with a smaller minimum atm */
                csRsp->offsetMin = CS_OFFSET_MIN;
            }
            else
            {
                // TODO BLE_LOKI-1370
                return (CS_STATUS_UNSUPPORTED_FEATURE);
            }
        }

        /* Ensure that the subevent interval and minimum offset do not exceed
         * the connInterval */
        if ((((csRsp->offsetMin + T625MS2US(csRsp->subEventInterval)) >
              T625MS2US(connPtr->curParam.connInterval))) ||
            (csRsp->subEventLen > T625MS2US(csRsp->maxProcedureDur)))

        {
            /* EXCEEDS. resuggest the parameters */
            csRsp->subEventLen = llCsGetSubeventLen(
                csRsp->subEventLen, csRsp->offsetMin, csRsp->maxProcedureDur,
                connPtr->curParam.connInterval);
            if (csRsp->subEventLen == 0)
            {
                /* can't handle. reject */
                return (CS_STATUS_UNEXPECTED_PARAMETER);
            }
            csRsp->subEventInterval = CS_SUBEVENT_INTERVAL(csRsp->subEventLen);
            csRsp->subEventsPerEvent = llCsSubeventsPerEvent(
                csRsp->maxProcedureDur, connPtr->curParam.connInterval,
                csRsp->subEventInterval);

            if (csRsp->subEventsPerEvent == 1)
            {
                csRsp->subEventInterval = 0;
            }
        }
        offsetMax = CS_OFFSET_MAX(connPtr->curParam.connInterval,
                                  csRsp->subEventInterval);
        if (csRsp->offsetMax > offsetMax)
        {
            csRsp->offsetMax = offsetMax;
        }

        if (llCsCheckACI(csRsp->ACI, csRsp->configId, connPtr->connId) !=
            CS_STATUS_SUCCESS)
        {
            /* Select Alternative ACI */
            csRsp->ACI = llCsSelectACI(csRsp->configId, connPtr->connId);
        }

        if (csRsp->phy != procParams.phy)
        {
            csRsp->phy = procParams.phy;
        }
        /* TODO check PWR Delta #BLE_LOKI-1417*/
    }
    /* Select the offset to be used */
    /* prefer the smaller in the range */
    csRsp->offset = csRsp->offsetMin;

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Internal function defined in ll_cs_ctrl_pkt_internal.h
 */
csStatus_e llCsConfirmInd(llConnState_t* connPtr, csProcedureEnable_t* csInd)
{
    csProcedureParams_t procParams;
    uint32 offsetMax =
        CS_OFFSET_MAX(connPtr->curParam.connInterval, csInd->subEventInterval);

    /* compare ind to the rsp/req  */
    if (!llCsDbCompareProcedureData(connPtr->connId, csInd->configId, csInd))
    {
        /* some params were re-suggested. Check them */
        llCsDbGetProcedureParams(connPtr->connId, csInd->configId, &procParams);

        if (csInd->connEventCount < connPtr->nextEvent)
        {
            return (CS_STATUS_UNEXPECTED_PARAMETER);
        }
        if (csInd->subEventLen > procParams.maxSubEventLen)
        {
            return (CS_STATUS_UNEXPECTED_PARAMETER);
        }
        if (csInd->ACI != 0)
        {
            return (CS_STATUS_UNEXPECTED_PARAMETER);
        }
        if (csInd->phy != procParams.phy)
        {
            return (CS_STATUS_UNEXPECTED_PARAMETER);
        }
        if ((csInd->offset < CS_OFFSET_MIN) || (csInd->offset > offsetMax))
        {
            return (CS_STATUS_UNEXPECTED_PARAMETER);
        }
        if (csInd->subEventsPerEvent == 1 && csInd->subEventInterval != 0)
        {
            return (CS_STATUS_UNEXPECTED_PARAMETER);
        }
        if ((csInd->subEventsPerEvent >
             llCsSubeventsPerEvent(csInd->maxProcedureDur,
                                   connPtr->curParam.connInterval,
                                   csInd->subEventInterval)))
        {
            return (CS_STATUS_UNEXPECTED_PARAMETER);
        }
        if ((csInd->subEventInterval) > 0 &&
            (csInd->subEventInterval <
             CS_SUBEVENT_INTERVAL(csInd->subEventLen)))
        {
            return (CS_STATUS_UNEXPECTED_PARAMETER);
        }
        /* TODO check PWR Delta #BLE_LOKI-1417*/
        if (llCsCheckACI(csInd->ACI, csInd->configId, connPtr->connId) !=
            CS_STATUS_SUCCESS)
        {
            return (CS_STATUS_UNEXPECTED_PARAMETER);
        }
    }

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Internal function defined in ll_cs_ctrl_pkt_internal.h
 */
uint32 llCsGetSubeventLen(uint32 maxSubeventLen, uint32 offsetMin, uint16 procLen,
                      uint16 connInterval)
{
    /* transform connInterval to units of us */
    uint32 connInt = T625MS2US(connInterval);
    uint32 procDur = T625MS2US(procLen);
    if (maxSubeventLen > procDur)
    {
        maxSubeventLen = procDur;
    }
    if ((maxSubeventLen + CS_SUBEVENT_SPACE + offsetMin) > connInt)
    {
        /* subeventInterval and minimal offset exceed the connInterval */
        if (connInt - (CS_SUBEVENT_SPACE + offsetMin) < CS_MIN_SUBEVENT_LEN)
        {
            /* cannot  */
            return 0;
        }
        else
        {
            return (connInt - (CS_SUBEVENT_SPACE + offsetMin));
        }
    }
    else
    {
        /* all good. Not change in subeventLen */
        return maxSubeventLen;
    }
}

/*******************************************************************************
 * Internal function defined in ll_cs_ctrl_pkt_internal.h
 */
csACI_e llCsSelectACI(uint8 configId, uint16 connId)
{
    csCapabilities_t own, peer;
    uint8 role;
    uint8 maxInitNAP, maxRefNAP;

    llCsDbGetLocalCapabilities(&own);
    llCsDbGetPeerCapabilities(connId, &peer);
    role = llCsDbGetConfigRole(connId, configId);

    if (role == CS_ROLE_INITIATOR)
    {
        maxInitNAP = own.numAntennas;
        maxRefNAP = peer.numAntennas;
    }
    else
    {
        maxInitNAP = peer.numAntennas;
        maxRefNAP = own.numAntennas;
    }
    return llCsDbGetACI(maxInitNAP, maxRefNAP);
}

/*******************************************************************************
 * Public function defined in ll_cs_ctrl_pkt_internal.h
 */
uint8 llCsSubeventsPerEvent(uint16 procedureLen, uint16 connInterval,
                            uint16 subeventInterval)
{
    uint16 usedInterval =
        (procedureLen > connInterval) ? connInterval : procedureLen;
    uint8 subeventsPerEvent = usedInterval / subeventInterval;
    return (subeventsPerEvent == 0) ? 1 : subeventsPerEvent;
}
