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
#include "cs/ll_cs_test.h"

#include "ll_common.h"
#include "ll_config.h"
#include "map_direct.h"

/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * MACROS
 */
#define GET_REMAINING_BITS(transactionId)                                      \
    (CS_DRBG_NUM_BITS - rndBitTransactions[transactionId].numBitsUsed)
/* Get Num Bytes from bits (equivalent to / 8) */
#define BITS2BYTES(bits)         bits >> 3;
/* Get Remainder from 8 by taking only the first 3 LSBs */
#define BIT2BYTE_REMAINDER(bits) bits & 0x07;

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

// Global flag to indicate if Test mode is enable turn on with HCI_LE_CS_TEST
csTestMode_e gCsTestMode = CS_TEST_MODE_DISABLE;

// CS Test Overrides Data
csTestOverrideData_t csTestOverrides = {0};

csOverrideCfg_t csOverrideCfgReset = {0};

const csCapabilities_t ownCapabilities =
{
    .optionalModes = CS_MODE_3_SUPPORTED,
    .rttCap = CS_RTT_SUPPORTED,
    .rttAAOnlyN = CS_NUM_CS_SYNC_EXCHANGES_SUPPORTED,
    .rttSoundingN = CS_RTT_CAPABILITY_NOT_SUPPORTED,
    .rttRandomPayloadN = CS_NUM_CS_SYNC_EXCHANGES_SUPPORTED,
    .nadmSounding = CS_CAPABILITY_NOT_SUPPORTED,
    .nadmRandomSeq = CS_CAPABILITY_NOT_SUPPORTED,
    .optionalCsSyncPhy = CS_OPTIONAL_PHY_SUPPORTED,
    .numAntennas = CS_MAX_NUM_ANT_SUPPORTED,
    .maxAntPath = CS_MAX_ANT_PATH_SUPPORTED,
    .role = CS_INITIATOR_MASK | CS_REFLECTOR_MASK,
    .rfu0 = 0,
#ifdef FAE_TEST
    .noFAE = 0,
#else
    .noFAE = 1,
#endif
    .chSel3c = CS_CAPABILITY_NOT_SUPPORTED,
    .csBasedRanging = CS_CAPABILITY_NOT_SUPPORTED,
    .rfu1 = 0,
    .numConfig = CS_MAX_NUM_CONFIG_SUPPORTED,
    .maxProcedures = CS_INDEFINITE_PROCEDURES_SUPPORTED,
    .tSwCap = CS_T_SW_CAP,
    .tIp1Cap = CS_T_IP1_IP2_CAP,
    .tIp2Cap = CS_T_IP1_IP2_CAP,
    .tFcsCap = CS_T_FCS_CAP,
    .tPmCsap = CS_T_PM_CAP,
    .snrTxCap = CS_CAPABILITY_NOT_SUPPORTED,
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

// Antenna Switch Time, 0 by default.
uint8 gSwTime = CS_T_0US;

// ACI hash table
// Maps the Antenna Config Index to the num Antenna Path per role
const uint8 aciTable[CS_NUM_ACI][CS_NUM_ROLES] =
{
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

// tIP hash table units microseconds
const uint8 tIpTbl[CS_NUM_TIP_OPTIONS] = { 10, 20, 30, 40, 50, 60, 80, 145};

// tFcs hash Table, units microseconds
const uint8 tFcsTbl[CS_NUM_TFCS_OPTIONS] = { 15, 20, 30, 40, 50, 60, 80, 100, 120 , 150};

// tPM hash table, units microseconds
// Note: the value 40 is added twice because in tFcs and tIP 40us is at index 3.
// and to allow uniform retrieval of the values, it was added to tPM table at
// index 3 as well as the original index 2.
const uint8 tPmTbl[CS_NUM_TPM_OPTIONS] = { 10, 20, 40, 40 };

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
    llCs[connId].terminateInfo.errorCode = 0;
    llCs[connId].terminateInfo.procedureCnt = 0;
    llCs[connId].terminateInfo.terminateState = CS_TERMINATE_DISABLE;
    for (uint8 configId = 0; configId < CS_MAX_NUM_CONFIG_IDS; configId++)
    {
        MAP_osal_memset(&llCs[connId].configSet[configId], 0,
                        sizeof(csConfigurationSet_t));
        llCsDbClearProcedureData(connId, configId);
    }
    llCs[connId].peerFaeTbl = NULL;
    MAP_osal_memcpy(&csFilteredChM, &defaultCSChM, sizeof(csChm_t));
    llCs[connId].currentConfigId = INVALID_CONFIG_ID;
    llCsDbResetProcCounter(connId, CS_PROC_ALL_C);
    llCsDbSetNextProcedureFlag(connId, FALSE);
    llCsDbSetNextSubeventFlag(connId, CS_PREP_CURR_SUBEVENT);
}

/*******************************************************************************
 * Public function defined in ll_cs_common.h.
 */
void llCsDbClearProcedureData(uint16 connId, uint8 configId)
{
    // Initialize ProcedureParameters
    MAP_osal_memset(&llCs[connId].procedureEnableData[configId], 0, sizeof(csProcedureEnable_t));
    llCs[connId].procedureParams[configId].maxProcedureDur      = CS_DEFAULT_PROCEDURE_DUR;
    llCs[connId].procedureParams[configId].minProcedureInterval = CS_DEFAULT_MIN_PROC_INTERVAL;
    llCs[connId].procedureParams[configId].maxProcedureInterval = CS_DEFAULT_MAX_PROC_INTERVAL;
    llCs[connId].procedureParams[configId].maxProcedureCount    = CS_DEFAULT_MAX_PROC_COUNT;
    llCs[connId].procedureParams[configId].minSubEventLen       = CS_DEFAULT_MIN_SUBEVENT_LEN;
    llCs[connId].procedureParams[configId].maxSubEventLen       = CS_DEFAULT_MAX_SUBEVENT_LEN;
    llCs[connId].procedureParams[configId].toneAntennaConfigSelection = ACI_A1_B1;
    llCs[connId].procedureParams[configId].phy                  = CS_DEFAULT_PHY;
    llCs[connId].procedureParams[configId].txPwrDelta           = CS_DEFAULT_TX_PWR_DELTA;
    llCs[connId].procedureParams[configId].preferredPeerAntenna = CS_DEFAULT_PEER_ANTENNA;
    llCs[connId].procedureParams[configId].enable               = CS_DEFAULT_ENABLE;
    llCs[connId].filteredChanIdx[configId].numChans             = 0;
    llCs[connId].filteredChanIdx[configId].mode0.numChanUsed    = 0;
    llCs[connId].filteredChanIdx[configId].mode0.numRepetitions = 0;
    llCsDbFreeChannelIndexArray(connId, configId);
    llCs[connId].filteredChanIdx[configId].nonMode0.numChanUsed = 0;
    llCs[connId].filteredChanIdx[configId].nonMode0.numRepetitions = 0;
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
    if (pRemoteCapabilities != NULL)
    {
        llCs[connId].peerCapabilities = *pRemoteCapabilities;
    }
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
    csStatus_e status = CS_STATUS_SUCCESS;
    if (pFae)
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
    }
    else
    {
        status = CS_STATUS_SUCCESS;
    }
    return status;
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
    csStatus_e status = CS_STATUS_SUCCESS;
    if (pConfig != NULL)
    {
        uint8 configId = pConfig->configId;
        llCs[connId].configSet[configId] = *pConfig;
    }
    else
    {
        status = CS_STATUS_UNEXPECTED_PARAMETER;
    }

    return status;
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
    if (pProcParams != NULL)
    {
        llCs[connId].procedureParams[configId] = *pProcParams;
    }
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
csTerminateState_e llCsDbGetProcedureTerminateState(uint16 connId)
{
    return llCs[connId].terminateInfo.terminateState;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetProcedureTerminateState(uint16 connId, csTerminateState_e terminateState)
{
    llCs[connId].terminateInfo.terminateState = terminateState;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbGetTerminateReason(uint16 connId)
{
    return llCs[connId].terminateInfo.errorCode;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetTerminateReason(uint16 connId, uint8 errorCode)
{
    llCs[connId].terminateInfo.errorCode = errorCode;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetDefaultSettings(uint16 connId,
                              csDefaultSettings_t* defaultSettings)
{
    if(defaultSettings != NULL )
    {
        llCs[connId].defaultSettings = *defaultSettings;
    }
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
    if (enData != NULL)
    {
        llCs[connId].procedureEnableData[configId] = *enData;
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetProcedureEnableIndData(uint16 connId, uint8 configId,
                                     csProcedureEnable_t* enData)
{
    if (enData != NULL)
    {
        llCs[connId].procedureEnableData[configId].connEventCount = enData->connEventCount;
        llCs[connId].procedureEnableData[configId].offset = enData->offset;
        llCs[connId].procedureEnableData[configId].eventInterval = enData->eventInterval;
        llCs[connId].procedureEnableData[configId].subEventsPerEvent = enData->subEventsPerEvent;
        llCs[connId].procedureEnableData[configId].subEventInterval = enData->subEventInterval;
        llCs[connId].procedureEnableData[configId].subEventLen = enData->subEventLen;
        llCs[connId].procedureEnableData[configId].ACI = enData->ACI;
        llCs[connId].procedureEnableData[configId].phy = enData->phy;
        llCs[connId].procedureEnableData[configId].pwrDelta = enData->pwrDelta;
    }
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
uint8 llCsDbGetSubeventsPerEvent(uint16 connId, uint8 configId)
{
    return llCs[connId].procedureEnableData[configId].subEventsPerEvent;
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
 * @fn          llCsDbGetNextSubeventFlag
 *
 * @brief       Get the next subevent flag for a given connection.
 *
 * @param       connId - connection Id
 *
 * @return      Next subevent flag
 */
csNextSubevent_e llCsDbGetNextSubeventFlag(uint16 connId)
{
    return llCs[connId].procedureInfo.nextSubevent;
}

/*******************************************************************************
 * @fn          llCsDbSetNextSubeventFlag
 *
 * @brief       Set the next subevent flag for a given connection.
 *
 * @param       connId - connection Id
 * @param       next - next subevent flag
 *
 * @return      None
 */
void llCsDbSetNextSubeventFlag(uint16 connId, csNextSubevent_e next)
{
    llCs[connId].procedureInfo.nextSubevent = next;
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

uint32 llCsDbGetLastChmUpdateTime(void)
{
    return chmUpdate.lastUpdateTime;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbGetChannelMap(uint8* pChm)
{
    if (pChm != NULL)
    {
        MAP_osal_memcpy(pChm, &csFilteredChM, sizeof(csChm_t));
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbFilterChannelMap(uint8* pFilteredChM)
{
    if (pFilteredChM != NULL)
    {
        for (uint8 i = 0; i < CS_CHM_SIZE; i++)
        {
            pFilteredChM[i] = pFilteredChM[i] & defaultCSChM.channelMap[i];
        }
    }
    return;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetActiveConnId(uint16 connId)
{
    activeConnId = connId;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint16 llCsDbGetActiveConnId(void)
{
    return activeConnId;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetBleRole(csBleRole role)
{
    bleRole = role;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
csBleRole llCsDbGetBleRole(void)
{
    return bleRole;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8* llCsDbGetAciTable(csACI_e ACI)
{
    return (uint8*)&aciTable[ACI];
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
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
inline uint8 llCsDbGetTip(uint8 idx)
{
    return tIpTbl[idx];
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
inline uint8 llCsDbGetTfcs(uint8 idx)
{
    return tFcsTbl[idx];
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
inline uint8 llCsDbGetTpm(uint8 idx)
{
    return tPmTbl[idx];
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsGetTimingIndex(uint8 csTime, uint8 type)
{
    uint8 i = 0;
    uint8 retVal = 0;

    if (type == CS_T_PM)
    {
        // search for csTime in tPMTbl
        for (i=0; i < CS_NUM_TPM_OPTIONS; i++)
        {
            if (tPmTbl[i] == csTime)
            {
                retVal = i;
                break;
            }
        }
    }
    else if (type == CS_T_FCS)
    {
        // search for csTime in tFcsTbl
        for (i=0; i < CS_NUM_TFCS_OPTIONS; i++)
        {
            if (tFcsTbl[i] == csTime)
            {
                retVal = i;
                break;
            }
        }
    }
    else
    {
        // search for csTime in tFcsTbl
        for (i=0; i < CS_NUM_TIP_OPTIONS; i++)
        {
            if (tIpTbl[i] == csTime)
            {
                retVal = i;
                break;
            }
        }
    }

    return retVal;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
csStatus_e llCsDbInitChanIndexInfo(uint16 connId, uint8 configId, uint8 numChan,
                                 uint8* chanIdxArr)
{
    csStatus_e status = CS_STATUS_SUCCESS;
    if (chanIdxArr != NULL)
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
                filteredChanIndex->mode0.shuffledChanIdxArray = NULL;
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
    }
    else
    {
        status = CS_STATUS_UNEXPECTED_PARAMETER;
    }

    return status;
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
    if (chanArr != NULL)
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
    uint8* nonMode0ShuffleArr = llCs[connId].filteredChanIdx[configId].nonMode0.shuffledChanIdxArray;
    uint8* mode0ShuffleArr = llCs[connId].filteredChanIdx[configId].mode0.shuffledChanIdxArray;
    uint8* filteredArr = (uint8*)&llCs[connId].filteredChanIdx[configId].filteredChanArr;

    /* Check if shuffled channel index array is not NULL before freeing it */
    /* But also make sure it is not pointing to the filtered Channel Index array
       as is the case when channel overrides are used in CS Test Mode.
       In this case, it should not be freed since this array is static,
       but DO set the pointer to NULL.*/
    if ( (nonMode0ShuffleArr != NULL) &&
         (nonMode0ShuffleArr != filteredArr))
    {
        MAP_osal_mem_free(nonMode0ShuffleArr);
    }
    if ( (mode0ShuffleArr != NULL) &&
         (mode0ShuffleArr != filteredArr))
    {
        MAP_osal_mem_free(mode0ShuffleArr);
    }
    nonMode0ShuffleArr = NULL;
    mode0ShuffleArr = NULL;
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
    if (pRandomBits != NULL)
    {
        MAP_osal_memcpy(rndBitTransactions[transactionId].rndBits, pRandomBits,
                        CS_RNDM_SIZE);
        rndBitTransactions[transactionId].numBitsUsed = 0;
    }
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
void llCsDbGetRandomBitsFromCache(uint8 transactionId, uint8 numBitsRequired, uint8* pRandomBits)
{
    if (pRandomBits != NULL)
    {
        uint8 remainingBits = GET_REMAINING_BITS(transactionId);
        uint8 numBitsUsed = rndBitTransactions[transactionId].numBitsUsed;
        uint8 numBytesUsed = BITS2BYTES(numBitsUsed);
        uint8 bitsUsedRem = BIT2BYTE_REMAINDER(numBitsUsed);
        uint8 numBytesReq = BITS2BYTES(numBitsRequired);
        uint8 bitsReqRem = BIT2BYTE_REMAINDER(numBitsRequired);

        if (remainingBits >= numBitsRequired)
        {
            /* Point to the unused byte in the cached buffer */
            uint8* pSrcBits = &rndBitTransactions[transactionId].rndBits[numBytesUsed];

            if (bitsUsedRem != 0U)
            {
                /* There is a remainder so need to copy bits */
                uint8 remBits2Byte = CS_8_BITS_SIZE - bitsUsedRem;
                if (numBitsRequired <= remBits2Byte)
                {
                    /* bitsRequired smaller than leftover bits */
                    *pRandomBits++ = llCsDbGetBits(*pSrcBits, bitsUsedRem, numBitsRequired);
                    rndBitTransactions[transactionId].numBitsUsed += numBitsRequired;
                    return;
                }
                else
                {
                    /* bitsRequired larger than leftover bits */
                    *pRandomBits++ = llCsDbGetBits(*pSrcBits++, bitsUsedRem, remBits2Byte);
                    numBitsRequired -= remBits2Byte;
                    numBytesReq = BITS2BYTES(numBitsRequired);
                    bitsReqRem = BIT2BYTE_REMAINDER(numBitsRequired);
                }
            }

            /* Copy full bytes */
            (void)MAP_osal_memcpy(pRandomBits, pSrcBits, numBytesReq);

            /* Copy remaining bits */
            if (bitsReqRem != 0U)
            {
                *pRandomBits = llCsDbGetBits(pSrcBits[numBytesReq], 0, bitsReqRem);
            }

            rndBitTransactions[transactionId].numBitsUsed += numBitsRequired;
        }
        else
        {
            // Not enough random bits left. Set to 0.
            MAP_osal_memset(pRandomBits, 0U, numBytesReq);
        }
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

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint16 llCsDbGetCurrentConnId( void )
{
    if (gCsTestMode == CS_TEST_MODE_ENABLE)
    {
        // If it's a test mode, use the test connId
        return CS_TEST_MODE_CONN_ID;
    }
    else
    {
        // Not a test mode, return current connId
        return llConns.currentConn;
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbGetDefaultChMap(uint8 * pChm)
{
    (void)MAP_osal_memcpy(pChm, &defaultCSChM, CS_CHM_SIZE);
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetTestMode(csTestMode_e mode)
{
    gCsTestMode = mode;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
csTestMode_e llCsDbGetTestMode(void)
{
    return gCsTestMode;
}


/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetSwitchTime( uint8 swT )
{
    gSwTime = swT;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbGetSwitchTime( void )
{
    return gSwTime;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetTestConfig(csTestParams_t* pTestParams)
{
    csConfigurationSet_t *testConfig =
                &llCs[CS_TEST_MODE_CONN_ID].configSet[CS_TEST_MODE_CONFIG_ID];
    llCsDbGetDefaultChMap((uint8*)&testConfig->channelMap);
    if (pTestParams)
    {
        testConfig->configId           = CS_TEST_MODE_CONFIG_ID;
        testConfig->state              = CS_ENABLE;
        testConfig->chMRepetition      = pTestParams->chmRep;
        testConfig->mainMode           = pTestParams->mainMode;
        testConfig->subMode            = pTestParams->subMode;
        testConfig->mainModeMaxSteps   = 0;
        testConfig->mainModeMinSteps   = 0;
        testConfig->mainModeRepetition = pTestParams->mainModeRep;
        testConfig->modeZeroSteps      = pTestParams->nMode0Steps;
        testConfig->role               = pTestParams->role;
        testConfig->rttType            = pTestParams->rttType;
        testConfig->csSyncPhy          = pTestParams->csSyncPhy;
        testConfig->chSel              = CS_CHANNEL_SEL_ALG_3B;
        testConfig->ch3cShape          = 0;
        testConfig->ch3CJump           = 0;
        testConfig->tIP1               = llCsGetTimingIndex( pTestParams->tIp1,
                                                            CS_T_IP1);
        testConfig->tIP2               = llCsGetTimingIndex( pTestParams->tIp2,
                                                            CS_T_IP2);
        testConfig->tFCs               = llCsGetTimingIndex( pTestParams->tFcs,
                                                            CS_T_FCS);
        testConfig->tPM                = llCsGetTimingIndex( pTestParams->tPm,
                                                            CS_T_PM);
        llCsDbSetActiveConnId(CS_TEST_MODE_CONN_ID);
        llCsDbSetCurrentConfigId(CS_TEST_MODE_CONN_ID, CS_TEST_MODE_CONFIG_ID);
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetTestDefaultSettings(csTestParams_t* pTestParams)
{
    csDefaultSettings_t *pDefSettings =
                            &llCs[CS_TEST_MODE_CONN_ID].defaultSettings;
    if (pTestParams)
    {
        pDefSettings->roleEn = CS_INITIATOR_MASK | CS_REFLECTOR_MASK;
        pDefSettings->csSyncAntennaSelection = pTestParams->csSyncAntSel;
        pDefSettings->maxTxPower = (int8)pTestParams->tpl;
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetTestProcedureEnable(csTestParams_t* pTestParams)
{
    csProcedureEnable_t *pProcEn =
        &llCs[CS_TEST_MODE_CONN_ID].procedureEnableData[CS_TEST_MODE_CONFIG_ID];
    if (pTestParams)
    {
        pProcEn->configId             = CS_TEST_MODE_CONFIG_ID;
        pProcEn->connEventCount       = 0;
        pProcEn->offset               = 0;
        pProcEn->offsetMin            = 0;
        pProcEn->offsetMax            = 0;
        pProcEn->maxProcedureDur      = CS_MAX_PROCEDURE_LEN;
        pProcEn->eventInterval        = 0;
        if (pTestParams->maxNumSubevents == 0)
        {
            /* When this field is 0, it should be ignored
            essentially, run as many subevents as needed to complete the CS procedure
            namely, set it to the max possible value*/
            pProcEn->subEventsPerEvent    = 0xFF;
        }
        else
        {
            pProcEn->subEventsPerEvent    = pTestParams->maxNumSubevents;
        }
        pProcEn->subEventInterval     = pTestParams->subeventInterval;
        pProcEn->subEventLen          = pTestParams->subeventLen;
        pProcEn->procedureInterval    = 0;
        pProcEn->procedureCount       = 1;
        pProcEn->ACI                  = (csACI_e)pTestParams->toneAntCfg;
        pProcEn->preferredPeerAntenna = 0;
        pProcEn->phy                  = 1;
        pProcEn->pwrDelta             = 0;
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetTestProcedureParams(csTestParams_t* pTestParams)
{
    csProcedureParams_t *pProcParams =
        &llCs[CS_TEST_MODE_CONN_ID].procedureParams[CS_TEST_MODE_CONFIG_ID];
    if (pTestParams)
    {
        pProcParams->maxProcedureDur            = CS_MAX_PROCEDURE_LEN;
        pProcParams->minProcedureInterval       = 0;
        pProcParams->maxProcedureInterval       = 0;
        pProcParams->maxProcedureCount          = 1;
        pProcParams->minSubEventLen             = pTestParams->subeventLen;
        pProcParams->maxSubEventLen             = pTestParams->subeventLen;
        pProcParams->toneAntennaConfigSelection = (csACI_e)pTestParams->toneAntCfg;
        pProcParams->phy                        = 1;
        pProcParams->txPwrDelta                 = 0;
        pProcParams->preferredPeerAntenna       = 0;
        pProcParams->enable                     = CS_ENABLE;
        pProcParams->snrCtrlI                   = 0xFFU;
        pProcParams->snrCtrlR                   = 0xFFU;
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
csStatus_e llCsDbSetTestOverrideData(csTestParams_t* pTestParams)
{
    if (pTestParams)
    {
        csTestOverrides.drbgNonce      = pTestParams->drbgNonce;
        csTestOverrides.overrideCfg    = pTestParams->overrideCfg;
        csTestOverrides.overrideLen    = pTestParams->overrideLen;
        csTestOverrides.overrideParams = pTestParams->overrideParams;
        csConfigurationSet_t* testCfg =
            &llCs[CS_TEST_MODE_CONN_ID].configSet[CS_TEST_MODE_CONFIG_ID];

        if (csTestOverrides.overrideCfg.chanCfg)
        {
            /* Bit 0 of the overrides is set */
            /* channel length */
            uint8 numChans = *csTestOverrides.overrideParams++;
            if ( (numChans == 0x00) || (numChans > 0x48))
            {
                return CS_STATUS_UNEXPECTED_PARAMETER;
            }
            /* channel [i] */
            uint8* chanOverride = csTestOverrides.overrideParams;
            llCsDbInitChanIndexInfo( CS_TEST_MODE_CONN_ID,
                                    CS_TEST_MODE_CONFIG_ID,
                                    numChans,
                                    chanOverride);
            csTestOverrides.overrideParams += numChans;
            llCsDbOverrideChanIndexArr();
        }
        else
        {
            /* The channel Config bit is not set, so we need to use a channel map */
            /* Copy the channel map from the overrides parameters */
            MAP_osal_memcpy( (void*)&testCfg->channelMap,
                            csTestOverrides.overrideParams,
                            CS_CHM_SIZE);
            /* Filter Channel Map */
            llCsDbFilterChannelMap((uint8*)&testCfg->channelMap);
            /* increment the pointer to the next variable */
            /* and set the rest of the variables */
            csTestOverrides.overrideParams+=CS_CHM_SIZE;
            testCfg->chSel     = *csTestOverrides.overrideParams++;
            testCfg->ch3cShape = *csTestOverrides.overrideParams++;
            testCfg->ch3CJump  = *csTestOverrides.overrideParams++;
            if ((testCfg->chSel     != 0x00 && testCfg->chSel     != 0x01) ||
                (testCfg->ch3cShape != 0x00 && testCfg->ch3cShape != 0x01) ||
                (testCfg->ch3CJump  >  0x08))
            {
                return CS_STATUS_UNEXPECTED_PARAMETER;
            }

        }

        if (csTestOverrides.overrideCfg.numMainModeSteps)
        {
            /* Set num main modes steps */
            /* Skip submode inersion override because it's not yet supported */
            csTestOverrides.overrideParams++;
        }

        if (csTestOverrides.overrideCfg.toneExtension)
        {
            /* Set tone extension */
            csTestOverrides.parsedParams.toneExt = *csTestOverrides.overrideParams++;
            if (csTestOverrides.parsedParams.toneExt > 0x04)
            {
                return CS_STATUS_UNEXPECTED_PARAMETER;
            }
        }

        if (csTestOverrides.overrideCfg.antennaPermutation)
        {
            /* Set tone antenna permutation */
            /* Not supported, Skip */
            csTestOverrides.overrideParams++;
        }

        if (csTestOverrides.overrideCfg.aa)
        {
            /* Set acccess address */
            MAP_osal_memcpy((uint8*)&csTestOverrides.parsedParams.aaTxInit, csTestOverrides.overrideParams, 4);
            csTestOverrides.overrideParams += 4;
            MAP_osal_memcpy((uint8*)&csTestOverrides.parsedParams.aaTxRef, csTestOverrides.overrideParams, 4);
            csTestOverrides.overrideParams += 4;
        }

        if (csTestOverrides.overrideCfg.ssMarkerPosition)
        {
            /* Sounding Sequence not supported, skip */
            csTestOverrides.overrideParams++;
        }

        if (csTestOverrides.overrideCfg.ssMarkerValue)
        {
            /* Sounding Sequence not supported, skip */
            csTestOverrides.overrideParams++;
        }

        if (csTestOverrides.overrideCfg.payload)
        {
            /* Payload of CS_SYNC */
            csTestOverrides.parsedParams.payloadPattern = *csTestOverrides.overrideParams++;
            if ((csTestOverrides.parsedParams.payloadPattern != CS_TEST_USER_PAYLOAD)   &&
                (csTestOverrides.parsedParams.payloadPattern == CS_TEST_PRBS9_PAYLOAD)  &&
                (csTestOverrides.parsedParams.payloadPattern == CS_TEST_PRBS15_PAYLOAD) &&
                (csTestOverrides.parsedParams.payloadPattern >= CS_TEST_NUM_PAYLOADS ))
            {
                return CS_STATUS_UNEXPECTED_PARAMETER;
            }
            MAP_osal_memcpy((uint8*)&csTestOverrides.parsedParams.userPayload,
                            csTestOverrides.overrideParams, CS_RNDM_SIZE);
        }
    }
    else
    {
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    return CS_STATUS_SUCCESS;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbGetTestOverrideData(csTestOverrideData_t *pCsTestOverrideData)
{
    if (pCsTestOverrideData)
    {
        *pCsTestOverrideData = csTestOverrides;
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint16 llCsDbGetReportedConnId(void)
{
    if (gCsTestMode == CS_TEST_MODE_ENABLE)
    {
        return 0x0FFF;
    }
    else
    {
        return llCsDbGetCurrentConnId();
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbGetChanOverrideCfg(void)
{
    return csTestOverrides.overrideCfg.chanCfg;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbGetOverrideCfg(csOverrideCfg_t* pOverrideCfg)
{
    if (pOverrideCfg)
    {
        if (gCsTestMode == CS_TEST_MODE_ENABLE)
        {
            *pOverrideCfg = csTestOverrides.overrideCfg;
            return TRUE;
        }
        else
        {
            *pOverrideCfg = csOverrideCfgReset;
            return FALSE;
        }
    }
    else
    {
        return FALSE;
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbOverrideChanIndexArr(void)
{
    /* Override the channel index arrays*/
    /* Get the filtered channel index field from llCs */
    csChanInfo_t* filteredChanIndex =
        &llCs[CS_TEST_MODE_CONN_ID].filteredChanIdx[CS_TEST_MODE_CONFIG_ID];

    if (!(filteredChanIndex->mode0.shuffledChanIdxArray ==
          (uint8*)&filteredChanIndex->filteredChanArr)  &&
        !(filteredChanIndex->nonMode0.shuffledChanIdxArray ==
          (uint8*)&filteredChanIndex->filteredChanArr))
    {
        /* We will not use shuffled index arrays, so free them to use the override instead */
        llCsDbFreeChannelIndexArray(CS_TEST_MODE_CONN_ID, CS_TEST_MODE_CONFIG_ID);
    }

    /* But use the filtered channel index array pointer, since when
    selecting a channel, these fields will be used and the cannot
    stay NULL */
    filteredChanIndex->mode0.shuffledChanIdxArray    =
                                    (uint8*)&filteredChanIndex->filteredChanArr;
    filteredChanIndex->nonMode0.shuffledChanIdxArray =
                                    (uint8*)&filteredChanIndex->filteredChanArr;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbGetOverrideToneExt(void)
{
    return csTestOverrides.parsedParams.toneExt;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbSetNextOverrideToneExt(uint8 toneExt)
{
    csTestOverrides.parsedParams.toneExt = (toneExt + 1) | 0x04;
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
void llCsDbGetAAOverride(uint32_t* initTxAA, uint32_t* refTxAA)
{
    if (initTxAA && refTxAA)
    {
        *initTxAA = csTestOverrides.parsedParams.aaTxInit;
        *refTxAA = csTestOverrides.parsedParams.aaTxRef;
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_db.h
 */
uint8 llCsDbGetPayloadOverride(uint32_t* pPlTx, uint32_t* pPlRx)
{
    uint8 plPattern = csTestOverrides.parsedParams.payloadPattern;
    if (pPlTx && pPlRx)
    {
        if (plPattern == CS_TEST_USER_PAYLOAD)
        {
            MAP_osal_memcpy(pPlTx,
                            csTestOverrides.parsedParams.userPayload,
                            CS_RNDM_SIZE);
            MAP_osal_memcpy(pPlRx,
                            csTestOverrides.parsedParams.userPayload,
                            CS_RNDM_SIZE);
        }
        else
        {
            llCsTestGetPayload(plPattern, pPlTx);
            llCsTestGetPayload(plPattern, pPlRx);
        }
    }
    return plPattern;
}
