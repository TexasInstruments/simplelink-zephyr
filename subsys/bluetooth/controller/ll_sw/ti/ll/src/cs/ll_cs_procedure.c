/******************************************************************************

 @file ll_cs_procedure.c

 @brief LL CS Procedure contains the APIs that are responsible for Initialziing
        the CS module. Building the CS steps of a CS subevent.
        Manages the CS double buffers.
        Sends CS Step results to the Host.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2023 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*******************************************************************************
 * INCLUDES
 */
#include "cs/ll_cs_db.h"
#include "cs/ll_cs_procedure.h"
#include "cs/ll_cs_procedure_internal.h"
#include "cs/ll_cs_channel_selection.h"

#include "cs/ll_cs_mgr.h"
#include "cs/ll_cs_common.h"
#include "cs/ll_cs_sec.h"

#include "ll_rat.h"
#include "rom_jt.h"
#include <math.h>

#include <ti/log/Log.h>
#include "ll_scheduler.h"
#include "rom_jt.h"
#include "cs/ll_cs_logs.h"

/*******************************************************************************
 * MACROS
 */
#define CS_REVERSE_BYTES(n)                                                    \
    ((n << 24) | (((n >> 16) << 24) >> 16) | (((n << 16) >> 24) << 16) |       \
     (n >> 24))
#define CS_REVERSE_TONE_EXTENSION_BITS(bits)                                   \
    ((((bits & 0x01) << 1) | ((bits & 0x02) >> 1)) & 0x3)

/*******************************************************************************
 * CONSTANTS
 */
#define CS_TONE_EXTENSION_BITS_NUM        2
#define CS_T_SY_1M_MIN                    44  // 44 us takes proportionaly longer depending on the payload size
#define CS_TIME_PER_OCTET_1M              8   // 44us / 5.5 octets. refer to Packet formats for Channel Sounding in CS SPEC
#define CS_T_SY_2M_MIN                    26  // 26 us, takes proportionaly longer depending on the payload size
#define CS_TIME_PER_OCTET_2M              4   // 26us / 6.5 octets. refer to Packet formats for Channel Sounding in CS SPEC
#define CS_T_RD                           5   // 5 us
#define CS_T_GD                           10  // 10 us
#define CS_T_FM                           80  // 80 us
#define CS_T_SW                           0   // antenna swithc period. currently 1:1 so no antenna switching
#define N_ANTENNA_PATHS                   1   // temporary...
#define SIZE_OF_32_BITS                   31
#define N_RTT_TYPES                       7   // number of RTT Types

/*******************************************************************************
 * MACROS
 */
#define CS_REVERSE_BYTES(n)                                                    \
    ((n << 24) | (((n >> 16) << 24) >> 16) | (((n << 16) >> 24) << 16) |       \
     (n >> 24))
#define CS_GET_T_SYNC(phy, plSize)                                             \
    ((phy == CS_LE_2M_SYNC_PHY)                                                \
         ? (CS_T_SY_2M_MIN + (plSize / 8) * CS_TIME_PER_OCTET_2M)              \
         : (CS_T_SY_1M_MIN + (plSize / 8) * CS_TIME_PER_OCTET_1M))
/* Get Payload Len based on RTT Type */
#define CS_GET_PL_LEN(rttType) rttTypeTbl[rttType]

/* 2*T_SY + 2*T_RD + TIP1 */
#define CS_MODE0_DUR(tSync, tIP1)                                              \
    (2 * tSync + 2 * CS_T_RD + CS_T_GD + CS_T_FM + tIP1)

/* 2*T_SY + 2*T_RD + TIP1 */
#define CS_MODE1_DUR(tSync, tIP1) (2 * tSync + 2 * CS_T_RD + tIP1)

/* 2*(T_PM+T_SW)*(NAP+1) + 2*T_RD + TIP2 */
#define CS_MODE2_DUR(tPM, tIP2)                                                \
    (2 * (tPM + CS_T_SW) * (N_ANTENNA_PATHS + 1) + 2 * CS_T_RD + tIP2)

/* 2*(T_SYNC+T_GD) + 2*(TPM+TSW)*(NAP+1)+ 2*TRD+TIP2 */
#define CS_MODE3_DUR(tSync, tPM, tIP2)                                         \
    (2 * (tSync + CS_T_GD) + 2 * (tPM + CS_T_SW) * (N_ANTENNA_PATHS + 1) +     \
     2 * CS_T_RD + tIP2)

/*****
 * CS_TEST
 */
#ifdef CS_TEST
ble_cs_steps_buffer_t stepBuffHeader1 = {
    .header = {
        .__elem__ = {.next = NULL, .prev = NULL},
        .length = SIZE_OF_BUFFER_DATA(BLE_CS_NUM_STEPS_PER_BUFFER),
        .tailIndex = SIZE_OF_BUFFER_DATA(4),
    }};

ble_cs_steps_buffer_t ble_cs_steps_buffer_0 = {
    .header =
        {
            .__elem__ = {.next = NULL, .prev = NULL},
            .length = SIZE_OF_BUFFER_DATA(BLE_CS_NUM_STEPS_PER_BUFFER),
            .tailIndex = SIZE_OF_BUFFER_DATA(
                BLE_CS_NUM_STEPS_PER_BUFFER) // should be num_steps
        },
    .steps = {
#ifdef CS_INITIATOR
        BLE_CS_CREATE_BASIC_STEP(RCL_CmdBleCs_StepMode_0, 0x1458F092,
                                 0xF0921458),
        BLE_CS_CREATE_BASIC_STEP(RCL_CmdBleCs_StepMode_3, 0x1458F092,
                                 0xF0921458),
        BLE_CS_CREATE_BASIC_STEP(RCL_CmdBleCs_StepMode_3, 0x1458F092,
                                 0xF0921458),
        BLE_CS_CREATE_BASIC_STEP(RCL_CmdBleCs_StepMode_3, 0x1458F092,
                                 0xF0921458),
#else
        BLE_CS_CREATE_BASIC_STEP(RCL_CmdBleCs_StepMode_0, 0xF0921458,
                                 0x1458F092),
        BLE_CS_CREATE_BASIC_STEP(RCL_CmdBleCs_StepMode_3, 0xF0921458,
                                 0x1458F092),
        BLE_CS_CREATE_BASIC_STEP(RCL_CmdBleCs_StepMode_3, 0xF0921458,
                                 0x1458F092),
        BLE_CS_CREATE_BASIC_STEP(RCL_CmdBleCs_StepMode_3, 0xF0921458,
                                 0x1458F092),
#endif
        /*    // ... and so on ... */
    }};

uint8 ble_cs_step_results_buffer_0[CS_RESULT_BUFF_SIZE] = {0};
// uint8 ble_cs_step_results_buffer_1[ CS_RESULT_BUFF_SIZE ] = {0};
uint16 ble_cs_steps_buffer_size = sizeof(ble_cs_steps_buffer_0);
uint16 ble_cs_step_results_buffer_size = CS_RESULT_BUFF_SIZE;
#else
uint16 ble_cs_steps_buffer_size = 0;
uint16 ble_cs_step_results_buffer_size = CS_RESULT_BUFF_SIZE;
#endif

/*******************************************************************************
 * EXTERNS
 */

/*******************************************************************************
 * TYPEDEFS
 */
typedef struct stepCarryOver
{
    uint8 cM; // main Mode carry over
    uint8 cS; // sub Mode carry Over
} stepCarryOver_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */
csResultsCb_t csResultsCb = NULL;

csRclCmdData_t csRclData = {0};

// RCL_CmdBleCs_S2r      csS2rResults;      /*!< Pointer to container list */
RCL_CmdBleCs_Stats csOutput; /*!< Pointer to statistics */

/* Channel Repetition */
uint8 channelRepeat[CS_MAX_CHANNEL_REPETITIONS];

/* channel Selection Algorithm to be used  (per config) */
uint8 chSelAlg;

/* Number of Steps in Subevent */
uint8 csNumSteps = 0;

uint16 rttTypeTbl[N_RTT_TYPES] = {
    0,  // RTT_COARSE
    32, // SOUNDING_SEQ_32
    96, // SOUNDING_SEQ_96
    32, // RANDOM_SEQ_32
    64, // RANDOM_SEQ_64
    96, // RANDOM_SEQ_96
    128 // RANDOM_SEQ_128
};

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
 * Public function defined in ll_cs_procedure.h
 */
uint8 llCsInit(void)
{
    /* Initialize the CS DB */
    return llCsInitDb();
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure.h
 */
void llCsClearConnProcedures(uint16 connId)
{
    /* Clear Connection's Procedures info */
    llCsDbClearCsConnData(connId);
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure.h
 */
void llCsFreeAll(void)
{
    /* Free All memory allocated for CS */
    llCsDbFree();
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure.h
 */
void llCsSetFeatureBit(void)
{
    /* Set the Channel Sounding Feature Bit */
    deviceFeatureSet.featureSet[5] |= (uint8)LL_FEATURE_CS;
    // NOTE while host is not currently supported, this bit was required in
    // order to allow us to perform CS with other peers. Therefore it is
    // enabled.
    deviceFeatureSet.featureSet[5] |= (uint8)LL_FEATURE_CS_HOST;
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure.h
 */
uint8 llCsStartProcedure(llConnState_t* connPtr)
{
    csProcedureEnable_t csData;
    uint8 configId = llCsDbGetCurrentConfigId(connPtr->connId);

    if (configId == INVALID_CONFIG_ID)
    {
        return (CS_STATUS_INVALID_CONFIG_ID);
    }

    /* Check if the procedure connected to this config ID is enabled */
    if (llCsDbIsProcedureEnabled(connPtr->connId, configId) == CS_DISABLE)
    {
        llCsRclFreeTask(connPtr->connId, configId);
        return (CS_STATUS_PROCEDURE_DISABLED);
    }

    if (llCsDbIsProcdureCompleted(connPtr->connId, CS_IND))
    {
        llCsDbGetProcedureEnableData(connPtr->connId, configId, &csData);
        /* Is this the connEvent counter to begin the CS procedure? */
        if ((csData.connEventCount == connPtr->currentEvent) &&
            (connPtr->currentEvent != 0))
        {
            llCsDbSetActiveConnId(llConns.currentConn);
            // TODO add a check here to make sure that DRBG was initialized
            if (connPtr->llTask->taskID == LL_TASK_ID_CENTRAL)
            {
                llCsDbSetBleRole(CS_BLE_ROLE_CENTRAL);
            }
            else
            {
                llCsDbSetBleRole(CS_BLE_ROLE_PERIPHERAL);
            }

            llCsSetupSubEvent(connPtr);

            /* Schedule the CS */
            if (llState == LL_STATE_IDLE)
            {
                MAP_llScheduler();
            }
        }
    }

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure.h
 */
uint8 llCsSetupSubEvent(llConnState_t* connPtr)
{
    if (llCsSetupRcl(connPtr->connId, csRclData) != CS_STATUS_SUCCESS)
    {
        return (CS_STATUS_RCL_SETUP_ERROR);
    }

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure.h
 */
uint8 llCsStartStepListGen(llConnState_t* connPtr)
{
    uint8 status = CS_STATUS_SUCCESS;
    uint8 configId = llCsDbGetCurrentConfigId(connPtr->connId);
    if (llCsDbIsProcdureCompleted(connPtr->connId, CS_START_PROCEDURE))
    {
        /* Initialize the DRBG engine */
        if (llCsProcedureInitDrbg(connPtr->connId, configId) !=
            CS_STATUS_SUCCESS)
        {
            /* An error occured, disable procedure */
            llCsDbEnableProcedureParams(connPtr->connId, configId, CS_DISABLE);
            return (CS_STATUS_DRBG_INIT_FAIL);
        }
        status = llCsSetupStepList(connPtr, configId, TRUE);
        if (status != CS_STATUS_SUCCESS)
        {
            /* An error occured, disable procedure */
            llCsDbEnableProcedureParams(connPtr->connId, configId, CS_DISABLE);
        }
        llCsDbClearProcedureCompleted(connPtr->connId, CS_START_PROCEDURE);
    }
    else if (llCsDbGetNextProcedureFlag(connPtr->connId, configId))
    {
        status = llCsSetupStepList(connPtr, configId, TRUE);
        if (status != CS_STATUS_SUCCESS)
        {
            /* An error occured, disable procedure */
            llCsDbEnableProcedureParams(connPtr->connId, configId, CS_DISABLE);
        }
        llCsDbSetNextProcedureFlag(connPtr->connId, FALSE);
    }
    return status;
}
/*******************************************************************************
 * Public function defined in ll_cs_procedure.h
 */
csStatus_e llCsSetupStepList(llConnState_t* connPtr, uint8 configId,
                           uint8 isfirstSE)
{
    csConfigurationSet_t csConfig;
    csProcedureEnable_t procParams;
    uint8 numChans;
    uint16 numMainModeSteps = 0;
    uint16 maxStepsPerSubevent = 0;
    uint16 remSteps = 0;

    llCsDbGetConfiguration(connPtr->connId, configId, &csConfig);
    llCsDbGetProcedureEnableData(connPtr->connId, configId, &procParams);

    /* Get max number of steps per subevent, based on subevent len */
    maxStepsPerSubevent = llCsNumStepsPerSubEvent(&csConfig, &procParams);
    if (csConfig.subMode != 0xFF)
    {
        // currently, submodes are not supported
        // BLE_LOKI-952
        return CS_STATUS_UNSUPPORTED_FEATURE;
    }

    if (isfirstSE)
    {
        /* this is the first subevent */
        /* Init Channel Index Array and shuffle it */
        llCsInitChanIdxArr(configId, connPtr->connId, &csConfig);
        numChans = llCsDbGetNumChan(connPtr->connId, configId);
        numMainModeSteps = numChans * csConfig.chMRepetition;
    }
    else
    {
        /* not the first subevent, check if more steps are needed */
        numMainModeSteps = llCsDbGetRemainingMmSteps(connPtr->connId, configId);
        /* if all mainMode steps were done
           or, stepCount reached the max.*/
        if ((numMainModeSteps == 0) ||
            (llCsSecGetStepCount() >= CS_MAX_STEPS_PER_PROCEDURE))
        {
            /* No more steps are needed! */
            /* set remaining steps to 0 (if not already so) */
            llCsDbSetRemainingMmSteps(connPtr->connId, configId, 0);
            return CS_STATUS_SUCCESS;
        }
    }

    /* if num steps exceed the max for the subevent len */
    if ((numMainModeSteps + csConfig.modeZeroSteps) > maxStepsPerSubevent)
    {
        /* set remaining steps to do in the next subevent */
        remSteps =
            numMainModeSteps + csConfig.modeZeroSteps - maxStepsPerSubevent;
        /* take only what fits into the subevent */
        numMainModeSteps = maxStepsPerSubevent - csConfig.modeZeroSteps;
    }

    /* csNumSteps is the total for the subevent */
    csNumSteps = numMainModeSteps + csConfig.modeZeroSteps;

    /* confirm that that it will not exceed the max steps for the procedure */
    if (csNumSteps + llCsSecGetStepCount() > CS_MAX_STEPS_PER_PROCEDURE)
    {
        /* it exceeds, so only take what remains */
        csNumSteps = CS_MAX_STEPS_PER_PROCEDURE - llCsSecGetStepCount() + 1;
        /* num main mode steps is all steps without mode 0 steps */
        numMainModeSteps = csNumSteps - csConfig.modeZeroSteps;
        /* no more steps to do since the max-per-procedure is reached */
        remSteps = 0;
    }
    llCsDbSetRemainingMmSteps(connPtr->connId, configId, remSteps);
    /* set number of steps for the RCL command data */
    csRclData.numSteps = csNumSteps;

    /* set the subevent info counters */
    llCsDbSetSubeventCount(connPtr->connId, CS_SE_INFO_NUM_STPES, csNumSteps);
    llCsDbSetSubeventCount(connPtr->connId, CS_SE_INFO_STEP_COUNT, 0);
    llCsDbSetSubeventCount(connPtr->connId, CS_SE_INFO_REPORT_COUNT, 0);

    /* Initialize the step buffers (allocate, set to zero, etc) */
    if (llCsInitStepBuffers() != CS_STATUS_SUCCESS)
    {
        return (CS_STATUS_INSUFFICIENT_MEMORY);
    }

#ifdef CS_TEST
    llCsTestStepList();
#else
    /* build the step buffers */
    llCsSetupStepBuffers(connPtr->connId, &csConfig, csNumSteps, isfirstSE);
#endif
    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
csStatus_e llCsInitChanIdxArr(uint8 configId, uint16 connId,
                            csConfigurationSet_t* csConfig)
{
    uint8 channelIndexArray[CS_FILTERED_CHAN_MAX_SIZE];
    uint8 numChans;
    csChanInfo_t* chanInfo;
    csStatus_e status;
    if ((uint8*)&csConfig->channelMap == NULL)
    {
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    /* Channel Map to Filtered Channel Index Array */
    numChans =
        llCsChm2FilteredChanArr((uint8*)&channelIndexArray,
                            (uint8*)&(csConfig->channelMap), CS_CHM_SIZE);
    status = llCsDbInitChanIndexInfo(connId, configId, numChans,
                                     (uint8*)&channelIndexArray);
    if (status == CS_STATUS_SUCCESS)
    {
        chanInfo = llCsDbGetChanInfo(connId, configId);
        /* shuffle mode 0 */
        llCsShuffleIndexArray(CS_MODE_0, numChans, &chanInfo->mode0,
                              (uint8*)&chanInfo->filteredChanArr);
    }
    return status;
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
csStatus_e llCsInitStepBuffers(void)
{
    if (csRclData.buffsAllocated)
    {
        /* Buffers already allocated */
        return (CS_STATUS_SUCCESS);
    }
    /* Allocate Memory for StepList */
    csRclData.csStepsBuff0 = (ble_cs_steps_buffer_t*)MAP_osal_mem_alloc(
        sizeof(ble_cs_steps_buffer_t) +
        sizeof(RCL_CmdBleCs_Step) * CS_STEP_BUFF_MAX_SIZE);
    if (csRclData.csStepsBuff0 == NULL)
    {
        return (CS_STATUS_INSUFFICIENT_MEMORY);
    }

    csRclData.csStepsBuff1 = (ble_cs_steps_buffer_t*)MAP_osal_mem_alloc(
        sizeof(ble_cs_steps_buffer_t) +
        sizeof(RCL_CmdBleCs_Step) * CS_STEP_BUFF_MAX_SIZE);
    if (csRclData.csStepsBuff1 == NULL)
    {
        MAP_osal_mem_free(csRclData.csStepsBuff0);
        return (CS_STATUS_INSUFFICIENT_MEMORY);
    }

    /* Allocate Memory for Step Result List */
    csRclData.csStepResultsBuff0 =
        (uint8_t*)MAP_osal_mem_alloc(CS_RESULT_BUFF_SIZE);
    if (csRclData.csStepResultsBuff0 == NULL)
    {
        MAP_osal_mem_free(csRclData.csStepsBuff0);
        MAP_osal_mem_free(csRclData.csStepsBuff1);
        return (CS_STATUS_INSUFFICIENT_MEMORY);
    }

    /* Set Initial Values */
    MAP_osal_memset(csRclData.csStepResultsBuff0, 0, CS_RESULT_BUFF_SIZE);
    MAP_osal_memset(&csOutput, 0x00, sizeof(RCL_CmdBleCs_Stats));
    csRclData.csOutput = &csOutput;
    csRclData.buffsAllocated = TRUE;

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
csStatus_e llCsSetupStep(uint8 stepMode, uint16 connId, uint8 isRepetition,
                       RCL_CmdBleCs_Step* stepData,
                       csConfigurationSet_t* csConfig)
{
    stepData->antennaPermIdx = 0; // currently irrelevant
    stepData->channelIdx =
        llCsSelectStepChannel(stepMode, connId, isRepetition, csConfig);
    if (stepData->channelIdx == INVALID_CS_CHANNEL_IDX)
    {
        return CS_STATUS_INVALID_CHAN_IDX;
    }

    switch (stepMode)
    {
        case CS_MODE_0:
        {
            stepData->mode = CS_MODE_0;
            llCsSetupStep0(csConfig->role, stepData, csConfig->rttType);
            break;
        }
        case CS_MODE_1:
        {
            stepData->mode = CS_MODE_1;
            llCsSetupStep1(csConfig->role, stepData, csConfig->rttType);
            break;
        }
        case CS_MODE_2:
        {
            stepData->mode = CS_MODE_2;
            llCsSetupStep2(csConfig->role, stepData);
            break;
        }
        case CS_MODE_3:
        {
            stepData->mode = CS_MODE_3;
            llCsSetupStep3(csConfig->role, stepData, csConfig->rttType);
            break;
        }
        default:
        {
            break;
        }
    }
    return CS_STATUS_SUCCESS;
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
void llCsSetupStep0(uint8 role, RCL_CmdBleCs_Step* stepData, uint8 rttType)
{
    /* Select AA */
    llCsSelectAA(role, &stepData->aaRx, &stepData->aaTx);

    stepData->payloadLen = llCsConvertRttType(rttType);

    /* Get Random Sequence (optional) */
    llCsGetRandomSequence(role, &(stepData->payloadRx[0]),
                          &(stepData->payloadTx[0]), CS_GET_PL_LEN(rttType));

    /* No Extension Bit for mode 0 */
    stepData->toneExtension = 0;
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
void llCsSetupStep1(uint8 role, RCL_CmdBleCs_Step* stepData, uint8 rttType)
{
    /* Select AA */
    llCsSelectAA(role, &stepData->aaRx, &stepData->aaTx);

    /* Set Payload Len */
    stepData->payloadLen = llCsConvertRttType(rttType);

    /* Get Random Sequence (optional) */
    llCsGetRandomSequence(role, &stepData->payloadRx[0],
                          &stepData->payloadTx[0], CS_GET_PL_LEN(rttType));

    /* No extension bit for stepMode 1 */
    stepData->toneExtension = 0;
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
void llCsSetupStep2(uint8 role, RCL_CmdBleCs_Step* stepData)
{
    /* Set Tone Extention Bit */
    stepData->toneExtension = llCsGetToneExtention();

    /* Set Mode-2 irrelevant step info to 0 */
    stepData->aaRx = 0;
    stepData->aaTx = 0;
    stepData->payloadLen = 0;
    MAP_osal_memset(stepData->payloadRx, 0, RCL_BLE_CS_MAX_PAYLOAD_SIZE);
    MAP_osal_memset(stepData->payloadTx, 0, RCL_BLE_CS_MAX_PAYLOAD_SIZE);
    // TODO when working with multiple antennas #BLE_LOKI-1366
    // llCsRandomizeAntennaPaths
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
void llCsSetupStep3(uint8 role, RCL_CmdBleCs_Step* stepData, uint8 rttType)
{
    /* Select AA */
    llCsSelectAA(role, &stepData->aaRx, &stepData->aaTx);

    /* Set Payload Len */
    stepData->payloadLen = llCsConvertRttType(rttType);

    /* Get Random Sequence (optional) */
    llCsGetRandomSequence(role, &(stepData->payloadRx[0]),
                          &(stepData->payloadTx[0]), CS_GET_PL_LEN(rttType));

    /* Set Tone Extention Bit */
    stepData->toneExtension = llCsGetToneExtention();
    // TODO when working with multiple antennas #BLE_LOKI-1366
    // llCsRandomizeAntennaPaths
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
uint16 llCsConvertRttType(uint8 rttType)
{
    uint16 cmdValue;
    switch (rttType)
    {
        case (0):
        {
            // None
            cmdValue = 0;
            break;
        }
        case (3):
        {
            // 32-bit
            cmdValue = 1;
            break;
        }
        case (4):
        {
            // 64-bit
            cmdValue = 2;
            break;
        }
        case (5):
        {
            // 96-bit
            cmdValue = 3;
            break;
        }
        case (6):
        {
            // 128-bit
            cmdValue = 4;
            break;
        }
        default:
        {
            cmdValue = 0;
            break;
        }
    }
    return cmdValue;
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure.h
 */
uint8 llCsSelectStepChannel(uint8 stepMode, uint16 connId, uint8 isRepetition,
                            csConfigurationSet_t* csConfig)
{
    /* Return the channel index */
    if (stepMode != CS_MODE_0)
    {
        llCsShuffleMainModeChannelIndexArray(TRUE, connId, csConfig);
    }
    return llCsDbGetChannelIndex(connId, csConfig->configId, stepMode);
}

/*******************************************************************************
 * Internal function defined in ll_cs_mgr_internal.h
 */
void llCsShuffleMainModeChannelIndexArray(uint8 isFirstSE, uint16 connId,
                                      csConfigurationSet_t* csConfig)
{
    csChanInfo_t* chanInfo = llCsDbGetChanInfo(connId, csConfig->configId);
    uint8 numChan = chanInfo->numChans;
    modeSpecificChanInfo_t chanArr = chanInfo->nonMode0;
    if ((chanArr.numChanUsed == 0) ||
        ((chanArr.numChanUsed == numChan) &&
         (chanArr.numRepetitions < csConfig->chMRepetition)))
    {

        llCsShuffleIndexArray(CS_NON_MODE_0, numChan, &chanArr,
                              (uint8*)&chanInfo->filteredChanArr);
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure.h
 */
uint8 llCsShuffleIndexArray(uint8 mode, uint8 numChan,
                            modeSpecificChanInfo_t* chanArr, uint8* filteredArr)
{
    if (mode == CS_MODE_0)
    {
        llCsChSel3aAnd3b((uint8*)chanArr->shuffledChanIdxArray, filteredArr,
                         numChan, CS_TID_CHANNEL_SELECTION_MODE_0);
    }
    else
    {
        if (chanArr->selectionAlgo == CS_CHANNEL_SEL_ALG_3B)
        {
            llCsChSel3aAnd3b((uint8*)chanArr->shuffledChanIdxArray, filteredArr,
                             numChan, CS_TID_CHANNEL_SELECTION_NON_MODE_0);
        }
        else
        {
            // Channel Selection Algo 3C
            // TODO BLE_LOKI-296
        }
    }

    return CS_STATUS_SUCCESS;
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure.h
 */
void freeCsStepsAndResults(void)
{
    if (csRclData.buffsAllocated)
    {
        if (csRclData.csStepsBuff0 != NULL)
        {
            MAP_osal_mem_free(csRclData.csStepsBuff0);
            csRclData.csStepsBuff0 = NULL;
        }
        if (csRclData.csStepsBuff1 != NULL)
        {
            MAP_osal_mem_free(csRclData.csStepsBuff1);
            csRclData.csStepsBuff1 = NULL;
        }
        if (csRclData.csStepResultsBuff0 != NULL)
        {
            MAP_osal_mem_free(csRclData.csStepResultsBuff0);
            csRclData.csStepResultsBuff0 = NULL;
        }
        csRclData.buffsAllocated = FALSE;
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure_internal.h.
 */
uint8 llCsGetNumMainModeSteps(uint8 mainModeMaxSteps, uint8 mainModeMinSteps)
{
    return (hr1(mainModeMaxSteps - mainModeMinSteps + 1,
                CS_TID_SUB_MODE_INSERTION) +
            mainModeMinSteps);
}

/*******************************************************************************
 * Get number of steps in a subevent. Includes mode-0, main mode, sub mode
 * and their repetition.
 */
/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
uint8 llCsNumStepsPerSubEvent(csConfigurationSet_t* config,
                              csProcedureEnable_t* procParams)
{
    uint16 t0 = GET_TFCS(config->tFCs) +
                CS_MODE0_DUR(CS_GET_T_SYNC(config->csSyncPhy,
                                           CS_GET_PL_LEN(config->rttType)),
                             GET_TIP(config->tIP1));
    uint16 mainModeTime = llCsMainModeDur(config->mainMode, config);
    uint16 timeLeft = 0; // time left for mainMode Steps
    uint8 numSteps = config->modeZeroSteps;

    /* time left after mode0Steps are done */
    timeLeft = procParams->subEventLen - (config->modeZeroSteps * t0);
    numSteps += (timeLeft / mainModeTime);

    /* num Main Mode steps that fit into the subevent */
    if (numSteps > CS_MAX_STEPS_PER_SUBEVENT)
    {
        numSteps = CS_MAX_STEPS_PER_SUBEVENT + 1;
    }
    return numSteps;
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
uint16 llCsMainModeDur(uint8 mode, csConfigurationSet_t* config)
{
    uint8 tFCs = GET_TFCS(config->tFCs);
    uint16 csPlSize = CS_GET_PL_LEN(config->rttType);
    switch (mode)
    {
        case CS_MODE_1:
            return tFCs +
                   CS_MODE1_DUR(CS_GET_T_SYNC(config->csSyncPhy, csPlSize),
                                GET_TIP(config->tIP1));
        case CS_MODE_2:
            return tFCs +
                   CS_MODE2_DUR(GET_TPM(config->tPM), GET_TIP(config->tIP2));
        case CS_MODE_3:
            return tFCs +
                   CS_MODE3_DUR(CS_GET_T_SYNC(config->csSyncPhy, csPlSize),
                                GET_TPM(config->tPM), GET_TIP(config->tIP2));
    }
    return 0;
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
void llCsSetupStepBuffers(uint16 connId, csConfigurationSet_t* csConfig,
                          uint8 nSubeventSteps, uint8 isFirstSE)
{
    /* Setup mode-0 steps */
    uint16 stepCount;
    uint8 stepListSize = nSubeventSteps > CS_STEP_BUFF_MAX_SIZE
                             ? CS_STEP_BUFF_MAX_SIZE
                             : nSubeventSteps;

    for (stepCount = 0; stepCount < csConfig->modeZeroSteps; stepCount++)
    {
        llCsSetupStep(CS_MODE_0, connId, FALSE,
                      &csRclData.csStepsBuff0->steps[stepCount], csConfig);
        llCsSecIncreaseStepCount();
        llCsDbIncrementSubeventInfoCounter(connId, CS_SE_INFO_STEP_COUNT, 1);
        llCsLogStep(&csRclData.csStepsBuff0->steps[stepCount], stepCount);
    }

    /* Setup the first buffer */
    for (stepCount = csConfig->modeZeroSteps; stepCount < stepListSize;
         stepCount++)
    {
        llCsSetupStep(csConfig->mainMode, connId, FALSE,
                      &csRclData.csStepsBuff0->steps[stepCount], csConfig);
        llCsSecIncreaseStepCount();
        llCsDbIncrementSubeventInfoCounter(connId, CS_SE_INFO_STEP_COUNT, 1);
        llCsLogStep(&csRclData.csStepsBuff0->steps[stepCount], stepCount);
    }
    /* Setup the second buffer, if needed */
    if (stepCount < nSubeventSteps)
    {
        uint8 nBuffSteps = nSubeventSteps - stepCount;
        stepListSize = nBuffSteps > CS_STEP_BUFF_MAX_SIZE
                           ? CS_STEP_BUFF_MAX_SIZE
                           : nBuffSteps;
        for (stepCount = 0; stepCount < stepListSize; stepCount++)
        {
            llCsSetupStep(csConfig->mainMode, connId, FALSE,
                          &csRclData.csStepsBuff1->steps[stepCount], csConfig);
            llCsSecIncreaseStepCount();
            llCsDbIncrementSubeventInfoCounter(connId, CS_SE_INFO_STEP_COUNT,
                                               1);
            llCsLogStep(&csRclData.csStepsBuff1->steps[stepCount], stepCount);
        }
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_rcl.h
 */
void llCsGenerateMoreSteps(uint16 connId, uint8 numSteps,
                           ble_cs_steps_buffer_t* stepListBuf)
{
    csConfigurationSet_t csConfig;
    uint8 configId = llCsDbGetCurrentConfigId(connId);
    llCsDbGetConfiguration(connId, configId, &csConfig);

    if (numSteps > CS_STEP_BUFF_MAX_SIZE)
    {
        numSteps = CS_STEP_BUFF_MAX_SIZE;
    }
    for (uint8 i = 0; i < numSteps; i++)
    {
        llCsSetupStep(csConfig.mainMode, connId, FALSE, &stepListBuf->steps[i],
                      &csConfig);
        llCsSecIncreaseStepCount();
        llCsDbIncrementSubeventInfoCounter(connId, CS_SE_INFO_STEP_COUNT, 1);
    }
}

/*******************************************************************************
 * function defined in ll_cs_procedure.h.
 */
uint8 llCsSelectAA(uint8 csRole, uint32_t* aaRx, uint32_t* aaTx)
{
    uint8 csDrbgRes[CS_RNDM_SIZE];
    uint32_t s0, s1, s2, s3;

    /* Get 128 bit vector from DRBG */
    csDrbg(128, csDrbgRes, CS_TID_AA_GENERATION);

    /* Split vector into 4 32bit sections */
    MAP_osal_memcpy(&s0, &csDrbgRes[0], 4);
    s0 = CS_REVERSE_BYTES(s0);
    MAP_osal_memcpy(&s1, &csDrbgRes[4], 4);
    s1 = CS_REVERSE_BYTES(s1);
    MAP_osal_memcpy(&s2, &csDrbgRes[8], 4);
    s2 = CS_REVERSE_BYTES(s2);
    MAP_osal_memcpy(&s3, &csDrbgRes[12], 4);
    s3 = CS_REVERSE_BYTES(s3);

    /* Select the AAs */
    if (csRole == CS_ROLE_INITIATOR)
    {
        *aaTx = llCsAASelectionRules(s0, s1);
        *aaRx = llCsAASelectionRules(s2, s3);
    }
    else
    {
        *aaRx = llCsAASelectionRules(s0, s1);
        *aaTx = llCsAASelectionRules(s2, s3);
    }
    llCsLogSelectedAccessAddress(aaTx, aaRx);
    return TRUE;
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure_internal.h.
 */
uint8 llCsGetRandomSequence(uint8 csRole, uint32_t* pTx, uint32_t* pRx,
                            uint8 payloadLen)
{
    if (payloadLen == 0)
    {
        MAP_osal_memset(pTx, 0, RCL_BLE_CS_MAX_PAYLOAD_SIZE);
        MAP_osal_memset(pRx, 0, RCL_BLE_CS_MAX_PAYLOAD_SIZE);
    }
    if (csRole == CS_ROLE_INITIATOR)
    {
        csDrbg(payloadLen, (uint8*)pTx, CS_TID_RANDOM_SEQUENCE_GENERATION);
        csDrbg(payloadLen, (uint8*)pRx, CS_TID_RANDOM_SEQUENCE_GENERATION);
    }
    else
    {
        csDrbg(payloadLen, (uint8*)pRx, CS_TID_RANDOM_SEQUENCE_GENERATION);
        csDrbg(payloadLen, (uint8*)pTx, CS_TID_RANDOM_SEQUENCE_GENERATION);
    }
    llCsLogRandomSequence(pTx, pRx, payloadLen);
    return TRUE;
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure_internal.h.
 */
uint8 llCsGetToneExtention()
{
    uint8 drbgBits = 0;
    uint8 toneExtension = 0;

    /* Get Tone Bits from DRBG */
    csDrbg(CS_TONE_EXTENSION_BITS_NUM, &drbgBits, CS_TID_CS_TONE_SLOT);

    /* Reverse bits */
    toneExtension = CS_REVERSE_TONE_EXTENSION_BITS(drbgBits);

    return toneExtension;
}

#ifdef CS_TEST
/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
void llCsTestStepList(void)
{
    MAP_osal_memcpy(csRclData.csStepsBuff0, &ble_cs_steps_buffer_0,
                    sizeof(RCL_CmdBleCs_Step) * BLE_CS_NUM_STEPS_PER_BUFFER +
                        sizeof(ble_cs_steps_buffer_t));
}
#endif // cs test

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
uint8 llCsAutoCorrelation(uint32_t s)
{
    uint8 score;
    uint8 si = 0, si1 = 0, si2 = 0, si3 = 0;
    uint8 c1 = 0, c2 = 0, c3 = 0;
    uint8 i;

    for (i = 0; i <= SIZE_OF_32_BITS - 1; i++)
    {
        si = s & 1;
        si1 = CS_SHIFT_RIGHT(s, 1) & 1;

        /* calculate C1 */
        c1 += (si ^ si1);

        if (i <= SIZE_OF_32_BITS - 2)
        {
            si2 = CS_SHIFT_RIGHT(s, 2) & 1;
            /* calculate C2 */
            c2 += (si ^ si2);
        }

        if (i <= SIZE_OF_32_BITS - 3)
        {
            si3 = CS_SHIFT_RIGHT(s, 3) & 1;
            /* calculate C3 */
            c3 += (si ^ si3);
        }
        s = CS_SHIFT_RIGHT(s, 1);
    }

    /* Calc the CS autocorrelation score  */
    score = ABS(2 * c1 - 31) + ABS(2 * c2 - 30) + ABS(2 * c3 - 29);

    return score;
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
uint32_t llCsAASelectionRules(uint32_t si, uint32_t sj)
{
    uint8 score1;
    uint8 score2;

    /* Calc Auto Corr Score of 1st sequence */
    score1 = llCsAutoCorrelation(si);
    /* Calc Auto Corr Score of 2nd Sequence */
    score2 = llCsAutoCorrelation(sj);

    /* check if score is equal */
    if (score1 == score2)
    {
        /* Select the second of the sequence pair */
        return sj;
    }
    /* Select the sequence with the lower score */
    else if (score1 < score2)
    {
        return si;
    }
    else
    {
        return sj;
    }
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h.
 */
uint8 llCsChm2FilteredChanArr(uint8* pDecimalArray, uint8* pBitMapArray,
                          uint8 mapSize)
{
    int8 channelCounter = 0;
    uint8 i, j;

    for (i = 0; i < mapSize; i++)
    {
        // Going through all bytes to check which bit is valid (bit set to 1)
        // Save each indices for every bit that's set to 1
        for (j = 0; j < BITS_PER_BYTE; j++)
        {
            // Check if the bit is valid. only interested in valid bits
            // Going through each bit inside the byte
            if ((pBitMapArray[i] >> j) & 1)
            {
                // Insert to pChannelDecimalArray the bit to use (in decimal
                // format) (i*8U)+j = i - the byte we are, j - the bit we are
                // (the indices inside the byte) For example - i = 0 (first
                // octat), j = 5  ---> that means in decimal base it is channel
                // number 5
                pDecimalArray[channelCounter] = (i * 8U) + j;

                // Encreasment by 1 the number of valid chanels
                channelCounter += 1;
            }
        }
    }
    return channelCounter;
}
