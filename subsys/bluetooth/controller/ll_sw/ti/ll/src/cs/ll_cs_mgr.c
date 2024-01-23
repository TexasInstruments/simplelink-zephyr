/******************************************************************************

 @file  LL CS Manager

 @brief The LL CS Manager handles the Link Layer part of the CS LE HCI commands.
        This module is responsible for parameter and condition validation
        before it decides to continue working on the command.
        This module also enqueues CS control

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
#include "cs/ll_cs_mgr.h"
#include "cs/ll_cs_mgr_internal.h"
#include "cs/ll_cs_sec.h"
#include "cs/ll_cs_common.h"

#include "ll_common.h"
#include "rom_jt.h"
#include "ll.h"
#include "ll_rat.h"
#include "ll_config.h"

/*******************************************************************************
 * MACROS
 */
#define SW_TX_POWER_TABLE_CS (llUserConfig.lrfTxPowerTablePtr)

/*******************************************************************************
 * CONSTANTS
 */
#define RAT_TICKS_IN_1S 4000000

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
 * Public function defined in ll_cs_mgr.h
 */
csStatus_e LL_CS_SecurityEnable(uint16 connId)
{
    csStatus_e status;
#ifdef DRBG_TEST
    // central from sample data
    csSecVectors_t secVectors = {
        .CSIV = {0x3E, 0x7F, 0x51, 0x86, 0xE0, 0xCA, 0x0B, 0x3B},
        .CSIN = {0x86, 0x73, 0x84, 0x0D},
        .CSPV = {0x64, 0xA6, 0x74, 0x96, 0x78, 0x68, 0xF1, 0x43}};
#else
    csSecVectors_t secVectors;
#endif
    llConnState_t* connPtr = MAP_llDataGetConnPtr(connId);
    uint8 configId = llCsDbGetCurrentConfigId(connId);
    if (connPtr == NULL)
    {
        return (CS_STATUS_INACTIVE_CONNECTION);
    }

    if ((status = llCsCheckConnection(connPtr)) != CS_STATUS_SUCCESS)
    {
        return (status);
    }

    // check if the connection is encrypted
    if (!connPtr->encEnabled)
    {
        return (CS_STATUS_COMMAND_DISALLOWED);
    }

    // make sure connection is in Central role
    if (connPtr->llTask->taskID != LL_TASK_ID_CENTRAL)
    {
        return (CS_STATUS_COMMAND_DISALLOWED);
    }

    // Reject request during CS procedure
    if (configId != INVALID_CONFIG_ID &&
        llCsDbIsProcedureEnabled(connId, configId) == CS_ENABLE)
    {
        return (CS_STATUS_COMMAND_DISALLOWED);
    }
    // generate CS security vectors
#ifndef DRBG_TEST
    llCsSecGenerateIV(secVectors.CSIV);
    llCsSecGenerateIN(secVectors.CSIN);
    llCsSecGeneratePV(secVectors.CSPV);
#endif

    // set security vectors in the cs db
    llCsDbSetSecurityVectors(connId, &secVectors, CS_SEC_USE_OFFSET);

    MAP_llEnqueueCtrlPkt(connPtr, LL_CTRL_CS_SEC_REQ);

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_mgr.h
 */
csStatus_e
LL_CS_ReadLocalSupportedCapabilites(csCapabilities_t* localCapabilities)
{
    llCsDbGetLocalCapabilities(localCapabilities);
    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_mgr.h
 */
csStatus_e LL_CS_ReadRemoteSupportedCapabilities(uint16 connId)
{
    csStatus_e status;
    csCapabilities_t peerCapabilities;
    llConnState_t* connPtr = MAP_llDataGetConnPtr(connId);

    if (connPtr == NULL)
    {
        return (CS_STATUS_INACTIVE_CONNECTION);
    }

    if ((status = llCsCheckConnection(connPtr)) != CS_STATUS_SUCCESS)
    {
        return (status);
    }

    // check if peer's CS Supported Capabilities has already been obtained
    if (llCsDbIsProcdureCompleted(connId, CS_CAPABILITIES_EXCHANGE_PROCEDURE))
    {
        // yes it has, so get the capabilities for the DB
        // and provide it to the host
        llCsDbGetPeerCapabilities(connId, &peerCapabilities);
        MAP_HCI_CS_ReadRemoteSupportedCapabilitiesCback(
            CS_STATUS_SUCCESS, connId, &peerCapabilities);
    }
    else // never done on this connection before
    {
        MAP_llEnqueueCtrlPkt(connPtr, LL_CTRL_CS_CAPABILITIES_REQ);
    }
    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_mgr.h
 */
csStatus_e LL_CS_SetDefaultSettings(uint16 connId,
                                  csDefaultSettings_t* defaultSettings)
{
    csStatus_e status;
    csConfigurationSet_t configSet;
    llConnState_t* connPtr = MAP_llDataGetConnPtr(connId);

    if (connPtr == NULL)
    {
        return (CS_STATUS_INACTIVE_CONNECTION);
    }

    if ((status = llCsCheckConnection(connPtr)) != CS_STATUS_SUCCESS)
    {
        return (status);
    }

    if ((defaultSettings->roleEn & (CS_REFLECTOR_MASK | CS_INITIATOR_MASK)) ==
        0)
    {
        return (CS_STATUS_UNEXPECTED_PARAMETER);
    }

    for (uint8 configId = 0; configId < CS_MAX_NUM_CONFIG_IDS; configId++)
    {
        // check If there is an attempt to disable a Role for which a valid CS
        // configuration is present,
        if (llCsDbGetConfiguration(connId, configId, &configSet) ==
            CS_STATUS_SUCCESS)
        {
            if ((configSet.state == CS_ENABLE) &&
                ((configSet.role & defaultSettings->roleEn) == 0))
            {
                return (CS_STATUS_UNEXPECTED_PARAMETER);
            }
        }
    }

    // If the value provided in this parameter is higher than the maximum output
    // power supported by the Controller then the Controller shall use the
    // maximum output power that it supports
    if (defaultSettings->maxTxPower > CS_MAX_TX_POWER_VALUE ||
        defaultSettings->maxTxPower < CS_MIN_TX_POWER_VALUE)
    {
        return (CS_STATUS_UNEXPECTED_PARAMETER);
    }
    if (defaultSettings->maxTxPower >
        RfBleDpl_getTxPowerDbm(RfBleDpl_getTxPowerMax()))
    {
        defaultSettings->maxTxPower =
            RfBleDpl_getTxPowerDbm(RfBleDpl_getTxPowerMax());
    }
    else if (defaultSettings->maxTxPower <
             RfBleDpl_getTxPowerDbm(RfBleDpl_getTxPowerMin()))
    {
        // The provided maxTxPower is lower than the supported minimum Tx Power
        return (CS_STATUS_UNEXPECTED_PARAMETER);
    }
    else
    {
        // If the Controller is unable to use the exact output power requested
        // by the Host, then the Controller shall use an output power that is
        // lower but closest to the requested value
        int8 tempTxPower = CS_MIN_TX_POWER_VALUE;
        const LRF_TxPowerTable* table = SW_TX_POWER_TABLE_CS;

        for (size_t i = 0; i < table->numEntries; i++)
        {
            if (table->powerTable[i].power.rawValue <=
                    defaultSettings->maxTxPower &&
                table->powerTable[i].power.rawValue > tempTxPower)
            {
                tempTxPower = table->powerTable[i].power.rawValue;
            }
        }
        defaultSettings->maxTxPower = tempTxPower;
    }

    // TODO antenna selection #BLE_LOKI-1366
    llCsDbSetDefaultSettings(connId, defaultSettings);

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_mgr.h
 */
csStatus_e LL_CS_ReadLocalFAETable(csFaeTbl_t* pFaeTable)
{
    /* Get Local FAE table from CS DB */
    return llCsDbGetLocalFaeTbl(pFaeTable);
}

/*******************************************************************************
 * Public function defined in ll_cs_mgr.h
 */
csStatus_e LL_CS_ReadRemoteFAETable(uint16 connId)
{
    csStatus_e status;
    csCapabilities_t peerCapabilities;
    llConnState_t* connPtr = MAP_llDataGetConnPtr(connId);

    if (connPtr == NULL)
    {
        return (CS_STATUS_INACTIVE_CONNECTION);
    }

    if ((status = llCsCheckConnection(connPtr)) != CS_STATUS_SUCCESS)
    {
        return (status);
    }

    // check if peer's CS Supported Capabilities has already been obtained
    if (!llCsDbIsProcdureCompleted(connId, CS_CAPABILITIES_EXCHANGE_PROCEDURE))
    {
        return (CS_STATUS_COMMAND_DISALLOWED);
    }

    // check if peer has a non-zero FAE
    llCsDbGetPeerCapabilities(connId, &peerCapabilities);

    if (peerCapabilities.noFAE)
    {
        // peer has a no FAE table send complete event with unsupported feature
        MAP_HCI_CS_ReadRemoteFAETableCompleteCback(
            CS_STATUS_FEATURE_NOT_SUPPORTED, connId, NULL);
    }
    else
    {
        // peer has a non-zero FAE table, request it.
        MAP_llEnqueueCtrlPkt(connPtr, LL_CTRL_CS_FAE_REQ);
    }
    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_mgr.h
 */
csStatus_e LL_CS_WriteRemoteFAETable(uint16 connId, int8* pFaeTbl)
{
    csStatus_e status;
    csCapabilities_t peerCapabilities;
    llConnState_t* connPtr = MAP_llDataGetConnPtr(connId);

    if (connPtr == NULL)
    {
        return (CS_STATUS_INACTIVE_CONNECTION);
    }

    if ((status = llCsCheckConnection(connPtr)) != CS_STATUS_SUCCESS)
    {
        return (status);
    }

    // check if peer has a non-zero FAE
    llCsDbGetPeerCapabilities(connId, &peerCapabilities);
    if (peerCapabilities.noFAE)
    {
        return (CS_STATUS_FEATURE_NOT_SUPPORTED);
    }

    // Set Fae Table in DB
    if (llCsDbSetRemoteFaeTbl(connId, (csFaeTbl_t*)pFaeTbl) ==
        CS_STATUS_SUCCESS)
    {
        llCsDbMarkProcedureCompleted(connId, CS_FAE_TABLE_UPDATE_PROCEDURE);
    }

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_mgr.h
 */
csStatus_e LL_CS_CreateConfig(uint16 connId, csConfigurationSet_t* pConfig,
                            uint8 createContext)
{
    csStatus_e status;
    csDefaultSettings_t dfSettings;
    llConnState_t* connPtr = MAP_llDataGetConnPtr(connId);

    if (connPtr == NULL)
    {
        return (CS_STATUS_INACTIVE_CONNECTION);
    }

    if ((status = llCsCheckConnection(connPtr)) != CS_STATUS_SUCCESS)
    {
        return (status);
    }

    // check if peer's CS Supported Capabilities has already been obtained
    if (!llCsDbIsProcdureCompleted(connId, CS_CAPABILITIES_EXCHANGE_PROCEDURE))
    {
        return (CS_STATUS_COMMAND_DISALLOWED);
    }

    llCsDbGetDefaultSettings(connId, &dfSettings);

    // Check if role is part of the default settings.
    if (CS_GET_BIT(dfSettings.roleEn, pConfig->role) == 0)
    {
        return (CS_STATUS_UNEXPECTED_PARAMETER);
    }

    // Check if it's safe to use config Id
    if (llCsConfigIdSafeToUse(connId, pConfig->configId) != CS_STATUS_SUCCESS)
    {
        return CS_STATUS_COMMAND_DISALLOWED;
    }

    // Set TIP1, TIP2, TFCS and TPM to the best match with peer.
    llCsSelectTimeConfig(connId, pConfig);

    // Filter Channel Map
    llCsDbFilterChannelMap((uint8*)&pConfig->channelMap);

    // Check configuration params
    if ((status = llCsConfigurationCheck(connId, pConfig)) != CS_STATUS_SUCCESS)
    {
        return status;
    }

    pConfig->state = CS_ENABLE;
    if ((status = llCsDbSetConfiguration(connId, pConfig)) != CS_STATUS_SUCCESS)
    {
        return (status);
    }

    llCsDbSetCurrentConfigId(connId, pConfig->configId);

    if (createContext)
    {
        /* When the Create_Context parameter is set to 0x00, the CS
           configuration is written only in the local Controller. Otherwise when
           set to 0x01, the CS configuration is written in both the local and
           remote Controllers using the Channel Sounding Configuration
           procedure. */
        MAP_llEnqueueCtrlPkt(connPtr, LL_CTRL_CS_CONFIG_REQ);
    }

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_mgr.h
 */
csStatus_e LL_CS_RemoveConfig(uint16 connId, uint8 configId)
{
    csStatus_e status;
    llConnState_t* connPtr = MAP_llDataGetConnPtr(connId);

    if (connPtr == NULL)
    {
        return (CS_STATUS_INACTIVE_CONNECTION);
    }

    if ((status = llCsCheckConnection(connPtr)) != CS_STATUS_SUCCESS)
    {
        return (status);
    }

    // Check if it's safe to remove config Id
    if ((status = llCsConfigIdSafeToUse(connId, configId)) != CS_STATUS_SUCCESS)
    {
        return status;
    }

    /* Remove config: Set config state to disabled */
    llCsDbRemoveConfiguration(connId, configId);

    /* Set curr config ID for when its time to send the ctrl pkt */
    llCsDbSetCurrentConfigId(connId, configId);

    /* Initiate the CS Configuration Procedure to remove config */
    MAP_llEnqueueCtrlPkt(connPtr, LL_CTRL_CS_CONFIG_REQ);

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_mgr.h
 */
csStatus_e LL_CS_SetChannelClassification(uint8* pChannelClassification)
{
    /* TODO Known bug for this command BLE_LOKI-797 */
    uint32 currTime = MAP_llGetCurrentTime();
    uint32 lastChmUpdateTime = llCsDbGetLastChmUpdateTime();

    // check that the time between two consecutive commands is at least 1 sec
    if ((lastChmUpdateTime > 0) &&
        ((currTime - lastChmUpdateTime) < RAT_TICKS_IN_1S))
    {
        return (CS_STATUS_COMMAND_DISALLOWED);
    }

    if (llCsDbUpdateChannelMap(pChannelClassification, currTime) !=
        CS_STATUS_SUCCESS)
    {
        return (CS_STATUS_UNEXPECTED_PARAMETER);
    }

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_mgr.h
 */
csStatus_e LL_CS_SetProcedureParameters(uint16 connId, uint8 configId,
                                      csProcedureParams_t* csProcParams)
{
    csStatus_e status;
    llConnState_t* connPtr = MAP_llDataGetConnPtr(connId);
    csCapabilities_t capabilities;
    llCsDbGetLocalCapabilities(&capabilities);

    if (connPtr == NULL)
    {
        return (CS_STATUS_INACTIVE_CONNECTION);
    }

    if ((status = llCsCheckConnection(connPtr)) != CS_STATUS_SUCCESS)
    {
        return (status);
    }
    // Validate config Id before use
    if ((status = llCsConfigIdSafeToUse(connId, configId)) != CS_STATUS_SUCCESS)
    {
        return status;
    }

    if ((capabilities.maxProcedures != CS_INDEFINITE_PROCEDURES_SUPPORTED) &&
        (csProcParams->maxProcedureCount > capabilities.maxProcedures))
    {
        return CS_STATUS_LIMITED_RESOURCES;
    }
    if (csProcParams->maxProcedureCount == 1 &&
        (csProcParams->maxProcedureInterval != 0 ||
         csProcParams->minProcedureInterval != 0))
    {
        /* Procedure interval shall be between 1 & 0xFFFF
           unless procedure count is 1. Then it should be 0. */
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }

    /* Check that minSubeventLen is smaller than the max */
    if (csProcParams->minSubEventLen > csProcParams->maxSubEventLen)
    {
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    /* if maxSubeventLen is larger than maxProcedureDur, set them to equal */
    if (csProcParams->maxSubEventLen > T625MS2US(csProcParams->maxProcedureDur))
    {
        csProcParams->maxSubEventLen = T625MS2US(csProcParams->maxProcedureDur);
    }

    if (llCsCheckACI(csProcParams->toneAntennaConfigSelection, configId,
                     connPtr->connId) != CS_STATUS_SUCCESS)
    {
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if (csProcParams->preferredPeerAntenna == 0)
    {
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }

    llCsDbSetProcedureParams(connId, configId, csProcParams);
    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_mgr.h
 */
csStatus_e LL_CS_ProcedureEnable(uint16 connId, uint8 configId, uint8 enable)
{
    csStatus_e status;
    llConnState_t* connPtr = MAP_llDataGetConnPtr(connId);
    csProcedureEnable_t enData;

    if (connPtr == NULL)
    {
        return (CS_STATUS_INACTIVE_CONNECTION);
    }

    if ((status = llCsCheckConnection(connPtr)) != CS_STATUS_SUCCESS)
    {
        return (status);
    }

    /* Make sure that that the config state is enabled */
    if (llCsDbGetConfigState(connId, configId) == CS_DISABLE)
    {
        return (CS_STATUS_COMMAND_DISALLOWED);
    }

    if ((enable == CS_ENABLE) &&
        (llCsConfigIdSafeToUse(connId, configId) == CS_STATUS_SUCCESS))
    {
        /* Set the current config in the DB */
        /* the config is safe to use so this procedure can be enabled */
        llCsDbSetCurrentConfigId(connId, configId);
        llCsDbEnableProcedureParams(connId, configId, CS_ENABLE);
        /* Trigger the Channel Sounding Start Procedure */
        MAP_llEnqueueCtrlPkt(connPtr, LL_CTRL_CS_REQ);
    }
    else
    {
        /* This procedure should be disabled */
        llCsDbGetProcedureEnableData(connPtr->connId, configId, &enData);

        /* Set terminateState field to CS_TERMINATE_RECEIVED */
        /* This indicate that the procedure state will be updated to
            CS_DISABLE at the end of the procedure. */
        llCsDbSetProcedureTerminateState(connId, configId,
                                         CS_TERMINATE_RECEIVED);

        /* Set procedure's status in the DB */
        llCsDbEnableProcedureParams(connId, configId, CS_DISABLE);

        /* send LL_CS_TERMINATE_IND PDU in order to terminate */
        MAP_llEnqueueCtrlPkt(connPtr, LL_CTRL_CS_TERMINATE_IND);
    }

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Internal function defined in ll_cs_mgr_internal.h
 */
void llCsSelectTimeConfig(uint16 connId, csConfigurationSet_t* pConfig)
{
    csCapabilities_t ownCap, peerCap;
    llCsDbGetLocalCapabilities(&ownCap);
    llCsDbGetPeerCapabilities(connId, &peerCap);

    pConfig->tIP1 = llCsGetBestTime(peerCap.tIp1Cap & ownCap.tIp1Cap, CS_T_IP1);
    pConfig->tIP2 = llCsGetBestTime(peerCap.tIp2Cap & ownCap.tIp2Cap, CS_T_IP2);
    pConfig->tFCs = llCsGetBestTime(peerCap.tFcsCap & ownCap.tFcsCap, CS_T_FCS);
    pConfig->tPM = llCsGetBestTime(peerCap.tPmCsap & ownCap.tPmCsap, CS_T_PM);
}

/*******************************************************************************
 * Internal function defined in ll_cs_mgr_internal.h
 */
uint8 llCsGetBestTime(uint8 timeCapability, uint8 type)
{
    uint8 supportedIndex = 0;
    switch (type)
    {
        case CS_T_IP1:
        case CS_T_IP2:
        {

            while (((timeCapability & 1) == 0) &&
                   (supportedIndex < CS_MANDATORY_TIP_IDX))
            {
                timeCapability >>= 1;
                supportedIndex++;
            }
            return supportedIndex;
        }
        case CS_T_FCS:
        {
            while (((timeCapability & 1) == 0) &&
                   (supportedIndex < CS_MANDATORY_TFCS_IDX))
            {
                timeCapability >>= 1;
                supportedIndex++;
            }
            return supportedIndex;
        }
        case CS_T_PM:
        {
            while (((timeCapability & 1) == 0) &&
                   (supportedIndex < CS_MANDATORY_TPM_IDX))
            {
                timeCapability >>= 1;
                supportedIndex++;
            }
            return supportedIndex;
        }
    }
    return CS_INVALID_TIME_INDX;
}

/*******************************************************************************/
