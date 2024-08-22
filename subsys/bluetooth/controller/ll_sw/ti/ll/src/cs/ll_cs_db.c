/******************************************************************************

 @file  ll_cs_db.c

 @brief The LL CS Database contains the setter and getter functions for the
        CS database and structres.

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
#include "cs/ll_cs_common.h"

#include "ll_common.h"
#include "ll_config.h"

#include "rom_jt.h"

/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * MACROS
 */
#define GET_REMAINING_BITS(transactionId)                                      \
    (CS_DRBG_NUM_BITS - rndBitTransactions[transactionId].numBitsUsed)
#define BITS2BYTES(bits) bits / 8;
#define BIT2BYTE_REMAINDER(bits) bits % 8;

/*******************************************************************************
 * EXTERNS
 */

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

// LL CS Information
llCs_t* llCs = NULL;

uint16 activeConnId = LL_INVALID_CONNECTION_ID;

// Random bit transactions cache
rndmBitCache_t* rndBitTransactions = NULL;

const csCapabilities_t ownCapabilities = {
    .optionalModes = CS_MODE_3_SUPPORTED,
    .rttCap = CS_RTT_SUPPORTED,
    .rttAAOnlyN = CS_NUM_CS_SYNC_EXCHANGES_SUPPORTED,
    .rttSoundingN = CS_RTT_CAPABILITY_NOT_SUPPORTED,
    .rttRandomPayloadN = CS_NUM_CS_SYNC_EXCHANGES_SUPPORTED,
    .nadmSounding = CS_CAPABILITY_NOT_SUPPORTED,
    .nadmRandomSeq = CS_CAPABILITY_NOT_SUPPORTED,
    .optionalCsSyncPhy = CS_LE_2M_PHY_SUPPORTED,
    .numAntennas = CS_MAX_NUM_ANT_SUPPORTED,
    .maxAntPath = CS_MAX_ANT_PATH_SUPPORTED,
    .role = CS_INITIATOR_MASK | CS_REFLECTOR_MASK,
    .companionSignal = CS_CAPABILITY_NOT_SUPPORTED,
#ifdef FAE_TEST
    .noFAE = 0,
#else
    .noFAE = 1,
#endif
    .chSel3c = CS_CAPABILITY_NOT_SUPPORTED,
    .csBasedRanging = CS_CAPABILITY_NOT_SUPPORTED,
    .rfu = 0,
    .numConfig = CS_MAX_NUM_CONFIG_SUPPORTED,
    .maxProcedures = CS_INDEFINITE_PROCEDURES_SUPPORTED,
    .tSwCap = CS_T_SW_CAP,
    .tIp1Cap = CS_T_IP1_IP2_CAP,
    .tIp2Cap = CS_T_IP1_IP2_CAP,
    .tFcsCap = CS_T_FCS_CAP,
    .tPmCsap = CS_T_PM_CAP,
    .rfu2 = 0};

uint8 aciTable[CS_NUM_ACI][CS_NUM_ROLES] = {
/* ACI *//* Init N_AP , ref N_AP */
/* 0 */ {    1     ,     1     },
/* 1 */ {    2     ,     1     },
/* 2 */ {    3     ,     1     },
/* 3 */ {    4     ,     1     },
/* 4 */ {    1     ,     2     },
/* 5 */ {    1     ,     3     },
/* 6 */ {    1     ,     4     },
/* 7 */ {    2     ,     2     }
};

// LOKI does not have a FAE. So this is NULL by default.
// for testing purposes and for supporting other devices with FAE
// a temporaty FAE table is added.
csFaeTbl_t* localFaeTbl = NULL;

#ifdef FAE_TEST
// Each value spans the [-4, +3.96875] ppm range, with a resolution
//  of 0.03125 ppm. A value of -128 then represents -4 ppm and a value
//  of 127 represents 3.96875 ppm, rounded to the nearest selectable ppm
int8 testFaeTbl[CS_FAE_TBL_LEN] = {
    -128, -50,  -25, 0,   25,   50,   100, 127, 64,  10,  50, 20,
    0,    -10,  -25, -40, 60,   70,   -80, 100, 127, 100, 75, 50,
    25,   0,    -25, -50, -75,  -100, -50, -25, 0,   10,  20, 30,
    40,   60,   80,  100, -120, -100, -80, -60, -40, -20, 0,  20,
    40,   60,   10,  20,  30,   40,   50,  60,  70,  80,  90, 100,
    -128, -100, -50, -25, 0,    25,   50,  100, 127, -80, 6,  8};
#endif

uint8 randomSequenceSize = 0;

// The Default Channel Map: includes all allowed channels
csChm_t defaultCSChM = {0xFC, 0xFF, 0x7F, 0xFC, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF, 0x1F};

// CSFilteredChM - Filtered Channel Map
csChm_t csFilteredChM;

// flag that indicates whether the channel map was updated
// and an indication has to be sent
csChmUpdate_t chmUpdate = {.update = CHM_NO_CHANGE, .lastUpdateTime = 0};

csBleRole bleRole = CS_BLE_ROLE_NOT_SET;

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsInitDb(void)
{
    // malloc array used for CS per connection
    if (llCs == NULL)
    {
        llCs = (llCs_t*)MAP_osal_mem_alloc(sizeof(llCs_t) * maxNumConns);
        // check that there was enough heap
        if (llCs == NULL)
        {
            return (CS_STATUS_INSUFFICIENT_MEMORY);
        }
        MAP_osal_memset(llCs, 0x00, sizeof(llCs_t) * maxNumConns);
    }
    else
    {
        // CS DB was already initiated
        return (CS_STATUS_SUCCESS);
    }

    for (int i = 0; i < maxNumConns; i++)
    {
        // initialize CS connection Data
        llCsDbClearCsConnData(i);
    }

#ifdef FAE_TEST
    if (!ownCapabilities.noFAE)
    {
        localFaeTbl = (csFaeTbl_t*)MAP_osal_mem_alloc(sizeof(csFaeTbl_t));
        if (localFaeTbl == NULL)
        {
            return (CS_STATUS_INSUFFICIENT_MEMORY);
        }
        else
        {
            MAP_osal_memcpy(localFaeTbl->faeTbl, testFaeTbl, CS_FAE_TBL_LEN);
        }
    }
#endif
    return llCsDbInitDRBGCache();
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbClearCsConnData(uint16 connId)
{
    llCs[connId].completedProcedures = 0;
    llCs[connId].activeCsCtrlProcedure = CS_NO_ACTIVE_PROCEDURE;
    llCs[connId].csProcCounter = 0;
    llCs[connId].defaultSettings.roleEn = 0;
    llCs[connId].defaultSettings.csSyncAntennaSelection = 0x01;
    llCs[connId].defaultSettings.maxTxPower = 0x00; //?
    llCs[connId].subEventInfo.allInfo = 0;
    for (uint8 configId = 0; configId < CS_MAX_NUM_CONFIG_IDS; configId++)
    {
        MAP_osal_memset(&llCs[connId].configSet[configId], 0,
                        sizeof(csConfigurationSet_t));
        // Initialize ProcedureParameters
        MAP_osal_memset(&llCs[connId].procedureEnableData[configId], 0,
                        sizeof(csProcedureEnable_t));
        llCs[connId].procedureParams[configId].maxProcedureDur =
            CS_DEFAULT_PROCEDURE_DUR;
        llCs[connId].procedureParams[configId].minProcedureInterval =
            CS_DEFAULT_MIN_PROC_INTERVAL;
        llCs[connId].procedureParams[configId].maxProcedureInterval =
            CS_DEFAULT_MAX_PROC_INTERVAL;
        llCs[connId].procedureParams[configId].maxProcedureCount =
            CS_DEFAULT_MAX_PROC_COUNT;
        llCs[connId].procedureParams[configId].minSubEventLen =
            CS_DEFAULT_MIN_SUBEVENT_LEN;
        llCs[connId].procedureParams[configId].maxSubEventLen =
            CS_DEFAULT_MAX_SUBEVENT_LEN;
        llCs[connId].procedureParams[configId].toneAntennaConfigSelection =
            ACI_A1_B1;
        llCs[connId].procedureParams[configId].phy = CS_DEFAULT_PHY;
        llCs[connId].procedureParams[configId].txPwrDelta =
            CS_DEFAULT_TX_PWR_DELTA;
        llCs[connId].procedureParams[configId].preferredPeerAntenna =
            CS_DEFAULT_PEER_ANTENNA;
        llCs[connId].procedureParams[configId].enable = CS_DEFAULT_ENABLE;
        llCs[connId].procedureParams[configId].terminateState =
            CS_DEFAULT_TERMINATE_STATE;
        llCs[connId].filteredChanIdx[configId].numChans = 0;
        llCs[connId].filteredChanIdx[configId].mode0.numChanUsed = 0;
        llCs[connId].filteredChanIdx[configId].mode0.numRepetitions = 0;
        llCs[connId].filteredChanIdx[configId].mode0.shuffledChanIdxArray =
            NULL;
        llCs[connId].filteredChanIdx[configId].nonMode0.numChanUsed = 0;
        llCs[connId].filteredChanIdx[configId].nonMode0.numRepetitions = 0;
        llCs[connId].filteredChanIdx[configId].nonMode0.shuffledChanIdxArray =
            NULL;
    }
    llCs[connId].peerFaeTbl = NULL;
    MAP_osal_memcpy(&csFilteredChM, &defaultCSChM, sizeof(csChm_t));
    llCs[connId].currentConfigId = INVALID_CONFIG_ID;
    llCsDbResetProcCounter(connId, CS_PROC_ALL_C);
    llCsDbSetNextProcedureFlag(connId, FALSE);
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbFree(void)
{
    if (llCs)
    {
        for (uint8 i = 0; i < maxNumConns; i++)
        {
            if (llCs[i].peerFaeTbl != NULL)
            {
                MAP_osal_mem_free(llCs[i].peerFaeTbl);
                llCs[i].peerFaeTbl = NULL;
            }
            for (uint8 j = 0; j < CS_MAX_NUM_CONFIG_IDS; j++)
            {
                llCsDbFreeChannelIndexArray(i, j);
            }
        }
        MAP_osal_mem_free(llCs);
        llCs = NULL;
    }
    llCsDbFreeDRBGCache();
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbMarkProcedureCompleted(uint16 connId, uint8 csProcedureId)
{
    llCs[connId].completedProcedures |= csProcedureId;
    llCs[connId].activeCsCtrlProcedure = CS_NO_ACTIVE_PROCEDURE;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbClearProcedureCompleted(uint16 connId, uint8 csProcedureId)
{
    llCs[connId].completedProcedures &= ~csProcedureId;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbIsProcdureCompleted(uint16 connId, uint8 procedureId)
{
    return ((llCs[connId].completedProcedures & procedureId) != 0);
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbResetProcedureCompletedFlag(uint16 connId, uint8 procedureId)
{
    // this is for procedures that can be done more than once.
    switch (procedureId)
    {
        case CS_CONFIG_PROCEDURE:
        case CS_CHM_UPDATE_PROCEDURE:
        {
            llCsDbClearProcedureCompleted(connId, procedureId);
            break;
        }
        default:
            break;
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbSetActiveProcedure(uint16 connId, uint8 procedureId)
{
    // first, check that no other procedure is active
    if (llCs[connId].activeCsCtrlProcedure == CS_NO_ACTIVE_PROCEDURE)
    {
        // set active procedure
        llCs[connId].activeCsCtrlProcedure = procedureId;
        return (CS_STATUS_SUCCESS);
    }
    else
    {
        // there is already an active CS procedure.
        // only one procedure is allowed at a time
        return (CS_STATUS_PROCEDURE_IN_PROGRESS);
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetSecurityVectors(uint16 connId, csSecVectors_t* pSecurityVectors,
                              uint8 offset)
{
    MAP_osal_memcpy(llCs[connId].securityVectors.CSIV +
                        (CS_CSIV_LEN / 2) * offset,
                    pSecurityVectors->CSIV, CS_CSIV_LEN / 2);
    MAP_osal_memcpy(llCs[connId].securityVectors.CSIN +
                        (CS_CSIN_LEN / 2) * offset,
                    pSecurityVectors->CSIN, CS_CSIN_LEN / 2);
    MAP_osal_memcpy(llCs[connId].securityVectors.CSPV +
                        (CS_CSPV_LEN / 2) * offset,
                    pSecurityVectors->CSPV, CS_CSPV_LEN / 2);
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbGetSecurityVectors(uint16 connId, csSecVectors_t* pSecurityVectors)
{
    *pSecurityVectors = llCs[connId].securityVectors;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbGetLocalCapabilities(csCapabilities_t* pLocalCapabilities)
{
    *pLocalCapabilities = ownCapabilities;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetPeerCapabilities(uint16 connId,
                               csCapabilities_t* pRemoteCapabilities)
{
    llCs[connId].peerCapabilities = *pRemoteCapabilities;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbGetPeerCapabilities(uint16 connId,
                               csCapabilities_t* pRemoteCapabilities)
{
    *pRemoteCapabilities = llCs[connId].peerCapabilities;
}

csStatus_e llCsDbGetLocalFaeTbl(csFaeTbl_t* pFae)
{
    if (ownCapabilities.noFAE)
    {
        MAP_osal_memset(pFae, 0, CS_FAE_TBL_LEN);
        return (CS_STATUS_FEATURE_NOT_SUPPORTED);
    }
    else
    {
        *pFae = *localFaeTbl;
        return (CS_STATUS_SUCCESS);
    }
}

csStatus_e llCsDbSetRemoteFaeTbl(uint16 connId, csFaeTbl_t* pFae)
{
    if (llCs[connId].peerFaeTbl == NULL)
    {
        llCs[connId].peerFaeTbl =
            (csFaeTbl_t*)MAP_osal_mem_alloc(sizeof(csFaeTbl_t));
        if (llCs[connId].peerFaeTbl == NULL)
        {
            return (CS_STATUS_INSUFFICIENT_MEMORY);
        }
    }

    *llCs[connId].peerFaeTbl = *pFae;

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbGetRemoteFaeTbl(uint16 connId, csFaeTbl_t* pFae)
{
    *pFae = *llCs[connId].peerFaeTbl;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
csStatus_e llCsDbSetConfiguration(uint16 connId, csConfigurationSet_t* pConfig)
{
    uint8 configId = pConfig->configId;

    llCs[connId].configSet[configId] = *pConfig;

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbRemoveConfiguration(uint16 connId, uint8 configId)
{
    llCs[connId].configSet[configId].state = CS_DISABLE;
    /* The channel Index array is allocated per config.
       Free it now! */
    llCsDbFreeChannelIndexArray(connId, configId);
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbGetConfigState(uint16 connId, uint8 configId)
{
    return llCs[connId].configSet[configId].state;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbGetConfigRole(uint16 connId, uint8 configId)
{
    return llCs[connId].configSet[configId].role;
}

csStatus_e llCsDbGetConfiguration(uint16 connId, uint8 configId,
                                csConfigurationSet_t* pConfig)
{
    *pConfig = llCs[connId].configSet[configId];
    if (llCs[connId].configSet[configId].state == CS_DISABLE)
    {
        return (CS_STATUS_DISABLED_CONFIG_ID);
    }
    else
    {
        return (CS_STATUS_SUCCESS);
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetCurrentConfigId(uint16 connId, uint8 configId)
{
    llCs[connId].currentConfigId = configId;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbGetCurrentConfigId(uint16 connId)
{
    return llCs[connId].currentConfigId;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbRemoveCurrentConfigId(uint16 connId)
{
    llCs[connId].currentConfigId = INVALID_CONFIG_ID;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetProcedureParams(uint16 connId, uint8 configId,
                              csProcedureParams_t* pProcParams)
{
    llCs[connId].procedureParams[configId] = *pProcParams;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbGetProcedureParams(uint16 connId, uint8 configId,
                              csProcedureParams_t* pProcParams)
{
    *pProcParams = llCs[connId].procedureParams[configId];
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbEnableProcedureParams(uint16 connId, uint8 configId, uint8 enable)
{
    llCs[connId].procedureParams[configId].enable = enable;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbIsProcedureEnabled(uint16 connId, uint8 configId)
{
    return llCs[connId].procedureParams[configId].enable;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbGetProcedureTerminateState(uint16 connId, uint8 configId)
{
    return llCs[connId].procedureParams[configId].terminateState;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetProcedureTerminateState(uint16 connId, uint8 configId,
                                      uint8 terminateState)
{
    llCs[connId].procedureParams[configId].terminateState = terminateState;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetDefaultSettings(uint16 connId,
                              csDefaultSettings_t* defaultSettings)
{
    llCs[connId].defaultSettings = *defaultSettings;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbGetDefaultSettings(uint16 connId,
                              csDefaultSettings_t* defaultSettings)
{
    *defaultSettings = llCs[connId].defaultSettings;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetProcedureEnableData(uint16 connId, uint8 configId,
                                  csProcedureEnable_t* enData)
{
    llCs[connId].procedureEnableData[configId] = *enData;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetProcedureEnableIndData(uint16 connId, uint8 configId,
                                     csProcedureEnable_t* enData)
{
    llCs[connId].procedureEnableData[configId].connEventCount =
        enData->connEventCount;
    llCs[connId].procedureEnableData[configId].offset = enData->offset;
    llCs[connId].procedureEnableData[configId].eventInterval =
        enData->eventInterval;
    llCs[connId].procedureEnableData[configId].subEventsPerEvent =
        enData->subEventsPerEvent;
    llCs[connId].procedureEnableData[configId].subEventInterval =
        enData->subEventInterval;
    llCs[connId].procedureEnableData[configId].subEventLen =
        enData->subEventLen;
    llCs[connId].procedureEnableData[configId].ACI = enData->ACI;
    llCs[connId].procedureEnableData[configId].phy = enData->phy;
    llCs[connId].procedureEnableData[configId].pwrDelta = enData->pwrDelta;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbGetProcedureEnableData(uint16 connId, uint8 configId,
                                  csProcedureEnable_t* enData)
{
    *enData = llCs[connId].procedureEnableData[configId];
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbCompareProcedureData(uint16 connId, uint8 configId,
                                 csProcedureEnable_t* procData)
{
    return MAP_osal_memcmp(procData,
                           &llCs[connId].procedureEnableData[configId],
                           sizeof(csProcedureEnable_t));
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbGetAclCounter(uint16 connId, uint8 configId)
{
    return llCs[connId].procedureEnableData[configId].connEventCount;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetNextProcedureConnEvent(uint16 connId, uint8 configId,
                                     uint8 connEvtCount)
{
    llCs[connId].procedureEnableData[configId].connEventCount = connEvtCount;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbGetNextProcedureFlag(uint16 connId, uint8 configId)
{
    return llCs[connId].procedureInfo.nextProcedure;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetNextProcedureFlag(uint16 connId, uint8 next)
{
    llCs[connId].procedureInfo.nextProcedure = next;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetEnableProcedureDuration(uint16 connId, uint8 configId,
                                      uint16 duration)
{
    llCs[connId].procedureEnableData[configId].maxProcedureDur = duration;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetEnableProcedureCount(uint16 connId, uint8 configId, uint16 count)
{
    llCs[connId].procedureEnableData[configId].procedureCount = count;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetEnableProcedureInterval(uint16 connId, uint8 configId,
                                      uint16 procedureInterval)
{
    llCs[connId].procedureEnableData[configId].procedureInterval =
        procedureInterval;
}

csStatus_e llCsDbUpdateChannelMap(uint8* pChM, uint32 currentTime)
{
    uint8 amountOfOneBit = 0;
    llCsDbFilterChannelMap(pChM);
    MAP_osal_memcpy((uint8*)&csFilteredChM, pChM, CS_CHM_SIZE);
    chmUpdate.update = CHM_CHANGE;
    chmUpdate.lastUpdateTime = currentTime;
    amountOfOneBit = llCsNumOnBit((uint8*)&csFilteredChM, CS_CHM_SIZE);
    if (amountOfOneBit < CS_MIN_NUM_OF_CHN)
    {
        return CS_STATUS_INVALID_CHM;
    }
    return CS_STATUS_SUCCESS;
}

uint32 llCsDbGetLastChmUpdateTime(void) { return chmUpdate.lastUpdateTime; }

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbGetChannelMap(uint8* pChm)
{
    MAP_osal_memcpy(pChm, &csFilteredChM, sizeof(csChm_t));
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbFilterChannelMap(uint8* pFilteredChM)
{
    for (uint8 i = 0; i < CS_CHM_SIZE; i++)
    {
        pFilteredChM[i] = pFilteredChM[i] & defaultCSChM.channelMap[i];
    }

    return;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetActiveConnId(uint16 connId) { activeConnId = connId; }

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint16 llCsDbGetActiveConnId(void) { return activeConnId; }

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetBleRole(csBleRole role) { bleRole = role; }

csBleRole llCsDbGetBleRole(void) { return bleRole; }

uint8* llCsDbGetAciTable(csACI_e ACI) { return (uint8*)&aciTable[ACI]; }

csACI_e llCsDbGetACI(uint8 initNAP, uint8 refNAP)
{
    /* this function returns the index of the
       antenna selection config based on num antenna
       path of the initiator and reflecto.
       depending on the table aciTable */
    if (initNAP == refNAP && refNAP == 2)
    {
        return ACI_A2_B2;
    }
    else if (initNAP >= refNAP)
    {
        return (csACI_e)(initNAP - refNAP);
    }
    else
    {
        return (csACI_e)(initNAP + refNAP + 1);
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
csStatus_e llCsDbInitChanIndexInfo(uint16 connId, uint8 configId, uint8 numChan,
                                 uint8* chanIdxArr)
{
    /* Get the filtered channel index field from llCs */
    csChanInfo_t* filteredChanIndex = &llCs[connId].filteredChanIdx[configId];

    /* if numChan has changed, need to free previously allocated memory */
    /* this is to allocate memory using new size */
    if (numChan != filteredChanIndex->numChans)
    {
        llCsDbFreeChannelIndexArray(connId, configId);
    }
    /* allocate memory for mode 0 chan index array */
    if (filteredChanIndex->mode0.shuffledChanIdxArray == NULL)
    {
        filteredChanIndex->mode0.shuffledChanIdxArray =
            MAP_osal_mem_alloc(numChan);
        if (filteredChanIndex->mode0.shuffledChanIdxArray == NULL)
        {
            return CS_STATUS_INSUFFICIENT_MEMORY;
        }
    }
    /* allocate memory for non-mode 0 chan index array */
    if (filteredChanIndex->nonMode0.shuffledChanIdxArray == NULL)
    {
        filteredChanIndex->nonMode0.shuffledChanIdxArray =
            MAP_osal_mem_alloc(numChan);
        if (filteredChanIndex->nonMode0.shuffledChanIdxArray == NULL)
        {
            MAP_osal_mem_free(filteredChanIndex->mode0.shuffledChanIdxArray);
            return CS_STATUS_INSUFFICIENT_MEMORY;
        }
    }

    /* Initialize the fields of the channel index array structure */
    filteredChanIndex->numChans = numChan;
    filteredChanIndex->mode0.numChanUsed = 0;
    filteredChanIndex->nonMode0.numChanUsed = 0;
    filteredChanIndex->nonMode0.numRepetitions = 0;
    filteredChanIndex->mode0.numRepetitions = 0;
    MAP_osal_memcpy(filteredChanIndex->filteredChanArr, chanIdxArr, numChan);
    filteredChanIndex->nonMode0.selectionAlgo =
        llCs[connId].configSet[configId].chSel;

    return CS_STATUS_SUCCESS;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
csChanInfo_t* llCsDbGetChanInfo(uint16 connId, uint8 configId)
{
    return &llCs[connId].filteredChanIdx[configId];
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbGetNumChan(uint16 connId, uint8 configId)
{
    return llCs[connId].filteredChanIdx[configId].numChans;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbUpdateChanIndexArray(uint8 mode, uint16 connId, uint8 configId,
                                modeSpecificChanInfo_t* chanArr)
{
    if (mode == CS_MODE_0)
    {
        llCs[connId].filteredChanIdx[configId].mode0 = *chanArr;
    }
    else
    {
        llCs[connId].filteredChanIdx[configId].nonMode0 = *chanArr;
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetRemainingMmSteps(uint16 connId, uint8 configId, uint16 mmSteps)
{
    llCs[connId].procedureInfo.mMStepsRemain = mmSteps;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint16 llCsDbGetRemainingMmSteps(uint16 connId, uint8 configId)
{
    return llCs[connId].procedureInfo.mMStepsRemain;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbGetChannelIndex(uint16 connId, uint8 configId, uint8 mode)
{
    uint8 chIdx = INVALID_CS_CHANNEL_IDX;
    uint8 chMRepition = llCs[connId].configSet[configId].chMRepetition;
    uint8 numChans = llCs[connId].filteredChanIdx[configId].numChans;
    modeSpecificChanInfo_t* chanInfo =
        mode ? &llCs[connId].filteredChanIdx[configId].nonMode0
             : &llCs[connId].filteredChanIdx[configId].mode0;
    if (chanInfo->numChanUsed == numChans)
    {
        if (chanInfo->numRepetitions < chMRepition)
        {
            /* channel map is exhausted but shall be repeated */
            /* reset the chanMapUsed counter */
            chanInfo->numChanUsed = 0;
            chanInfo->numRepetitions++;
        }
        else
        {
            /* channel map exhausted, but no more repetitions are left */
            /* return invalid channel index as we shouldn't be here */
            /* the caller of the func is expected to check this value */
            chIdx = INVALID_CS_CHANNEL_IDX;
        }
    }

    /* select the current channel index and then increment the numChanUsed
     * counter */
    chIdx = chanInfo->shuffledChanIdxArray[chanInfo->numChanUsed];
    chanInfo->numChanUsed++;
    return chIdx;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbFreeChannelIndexArray(uint16 connId, uint8 configId)
{
    if (llCs[connId].filteredChanIdx[configId].nonMode0.shuffledChanIdxArray !=
        NULL)
    {
        MAP_osal_mem_free(llCs[connId]
                              .filteredChanIdx[configId]
                              .nonMode0.shuffledChanIdxArray);
        llCs[connId].filteredChanIdx[configId].nonMode0.shuffledChanIdxArray =
            NULL;
    }
    if (llCs[connId].filteredChanIdx[configId].mode0.shuffledChanIdxArray !=
        NULL)
    {
        MAP_osal_mem_free(
            llCs[connId].filteredChanIdx[configId].mode0.shuffledChanIdxArray);
        llCs[connId].filteredChanIdx[configId].mode0.shuffledChanIdxArray =
            NULL;
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbGetSubeventInfo(uint16 connId, csSubeventInfo_e type)
{
    return llCs[connId].subEventInfo.subEventInfo[type];
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetSubeventCount(uint16 connId, csSubeventInfo_e type, uint8 count)
{
    llCs[connId].subEventInfo.subEventInfo[type] = count;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbResetSubeventInfo(uint16 connId)
{
    llCs[connId].subEventInfo.allInfo = 0;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbIncrementSubeventInfoCounter(uint16 connId, csSubeventInfo_e type,
                                        uint8 incVal)
{
    llCs[connId].subEventInfo.subEventInfo[type] += incVal;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint16 llCsDbIncrementProcCounter(uint16 connId, csProcedureCounter_e counter)
{
    return ++llCs[connId].procedureInfo.counters.counters[counter];
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint16 llCsDbGetProcCounter(uint16 connId, csProcedureCounter_e counter)
{
    return llCs[connId].procedureInfo.counters.counters[counter];
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbResetProcCounter(uint16 connId, csProcedureCounter_e counter)
{
    if (counter == CS_PROC_ALL_C)
    {
        llCs[connId].procedureInfo.counters.counters[CS_PROC_INFO_SUBEVENT_C] =
            0;
        llCs[connId].procedureInfo.counters.counters[CS_PROC_INFO_EVENT_C] = 0;
        llCs[connId].procedureInfo.counters.counters[CS_PROC_C] = 0;
    }
    else
    {
        llCs[connId].procedureInfo.counters.counters[counter] = 0;
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetEventsPerProcedure(uint16 connId, uint16 eventsPerProcedure)
{
    llCs[connId].procedureInfo.eventsPerProcedure = eventsPerProcedure;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint16 llCsDbGetEventsPerProcedure(uint16 connId)
{
    return llCs[connId].procedureInfo.eventsPerProcedure;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetEventAnchorPoint(uint16 connId, uint32_t anchorPoint)
{
    llCs[connId].procedureInfo.eventAnchorPoint = anchorPoint;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint32_t llCsDbGetEventAnchorPoint(uint16 connId)
{
    return llCs[connId].procedureInfo.eventAnchorPoint;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
csStatus_e llCsDbInitDRBGCache(void)
{
    if (rndBitTransactions == NULL)
    {
        rndBitTransactions = (rndmBitCache_t*)MAP_osal_mem_alloc(
            sizeof(rndmBitCache_t) * CS_MAX_TRANSACTION_IDS);
        if (rndBitTransactions == NULL)
        {
            return (CS_STATUS_INSUFFICIENT_MEMORY);
        }
    }
    return llCsDbResetDRBGCache();
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
csStatus_e llCsDbResetDRBGCache(void)
{
    if (rndBitTransactions)
    {
        MAP_osal_memset(rndBitTransactions, 0,
                        sizeof(rndmBitCache_t) * CS_MAX_TRANSACTION_IDS);
        for (uint8 trnId = 0; trnId <= 9; trnId++)
        {
            /* Set numBitsUsed to the MAX in order to generate new random bits
             */
            rndBitTransactions[trnId].numBitsUsed = 128;
        }
        return (CS_STATUS_SUCCESS);
    }
    return (CS_STATUS_LIMITED_RESOURCES);
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbFreeDRBGCache(void)
{
    if (rndBitTransactions)
    {
        MAP_osal_mem_free(rndBitTransactions);
        rndBitTransactions = NULL;
    }
    if (localFaeTbl != NULL)
    {
        MAP_osal_mem_free(localFaeTbl);
        localFaeTbl = NULL;
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetRandomBitsCache(uint8 transactionId, uint8* pRandomBits)
{
    MAP_osal_memcpy(rndBitTransactions[transactionId].rndBits, pRandomBits,
                    CS_RNDM_SIZE);
    rndBitTransactions[transactionId].numBitsUsed = 0;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbRandomBitsAvailable(uint8 transactionId, uint8 numBitsRequired)
{
    uint8 remainingBits = GET_REMAINING_BITS(transactionId);
    if (((remainingBits) >= numBitsRequired) && (remainingBits > 0))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbGetRandomBitsFromCache(uint8 transactionId, uint8 numBitsRequired,
                                  uint8* pRandomBits)
{
    uint8 newNumBitsReq = numBitsRequired;
    uint8 remainingBits = GET_REMAINING_BITS(transactionId);
    uint8 numBitsUsed = rndBitTransactions[transactionId].numBitsUsed;
    uint8 numBytesUsed = BITS2BYTES(numBitsUsed);
    uint8 bitsUsedRem = BIT2BYTE_REMAINDER(numBitsUsed);
    uint8 numBytesReq = BITS2BYTES(newNumBitsReq);
    uint8 bitsReqRem = BIT2BYTE_REMAINDER(newNumBitsReq);

    if (remainingBits >= newNumBitsReq)
    {
        /* point to the unused byte in the cached buffer */
        uint8* pSrcBits =
            &rndBitTransactions[transactionId].rndBits[numBytesUsed];

        if (bitsUsedRem != 0)
        {
            /* There is a remainder so need to copy bits */
            if (newNumBitsReq < (8 - bitsUsedRem))
            {
                /* bitsRequired smaller than leftover bits */
                *pRandomBits++ =
                    llCsDbGetBits(*pSrcBits, bitsUsedRem, newNumBitsReq);
            }
            else
            {
                /* bitsRequired larger than leftover bits */
                uint8 remBits2Byte = 8 - bitsUsedRem;
                *pRandomBits++ =
                    llCsDbGetBits(*pSrcBits++, bitsUsedRem, remBits2Byte);

                newNumBitsReq = newNumBitsReq - remBits2Byte;
                numBytesReq = BITS2BYTES(newNumBitsReq);
                bitsReqRem = BIT2BYTE_REMAINDER(numBytesReq);
            }
        }
        while (numBytesReq > 0)
        {
            /* can copy byte */
            *pRandomBits++ = *pSrcBits++;
            numBytesReq--;
        }
        /* copy bits */
        if (bitsReqRem != 0)
        {
            *pRandomBits = llCsDbGetBits(*pSrcBits, 0, bitsReqRem);
        }
        rndBitTransactions[transactionId].numBitsUsed += numBitsRequired;
    }
    else
    {
        // not enough random bits left.
        pRandomBits = NULL;
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
csModeRole_e getRoleModeSpecificDataType(uint8 role, uint8 mode)
{
    switch (mode)
    {
        case CS_MODE_0:
        {
            if (role == CS_ROLE_INITIATOR)
            {
                return MODE_0_INITIATOR;
            }
            else
            {
                return MODE_0_REFLECTOR;
            }
            break;
        }
        case CS_MODE_1:
            return MODE_1_INIT_REFL;
        case CS_MODE_2:
            return MODE_2_INIT_REFL;
        case CS_MODE_3:
            return MODE_3_INIT_REFL;
    }
    return MODE_UNKOWN;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbGetBits(uint8 num, uint8 startIdx, uint8 numBits)
{
    /* mask of size num bits */
    uint8 mask = (1 << numBits) - 1;
    /* Take the bits from startIdx from the end, with numBits offset */
    return (num >> (8 - (startIdx + numBits))) & mask;
}
