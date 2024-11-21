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
#include <math.h>

#include <ti/log/Log.h>
#include "ll_scheduler.h"
#include "map_direct.h"
#include "cs/ll_cs_logs.h"
#include "ll_enc.h"

/*******************************************************************************
 * MACROS
 */

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

uint16 ble_cs_steps_buffer_size = 0;
uint16 ble_cs_step_results_buffer_size = CS_RESULT_BUFF_SIZE;

/*******************************************************************************
 * EXTERNS
 */

/*******************************************************************************
 * TYPEDEFS
 */
typedef void (*pfnSetupStepFunc)(uint8 role, RCL_CmdBleCs_Step* stepData, uint8 rttType);

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

/* RTT Type Table */
const uint16 rttTypeTbl[N_RTT_TYPES] = {
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
csStatus_e llCsStartProcedure(llConnState_t* connPtr)
{
    csProcedureEnable_t csData;
    uint8 configId;
    csStatus_e status;

    if (connPtr != NULL)
    {
        configId = llCsDbGetCurrentConfigId(connPtr->connId);
        if (configId != INVALID_CONFIG_ID)
        {
            /* Check if the procedure connected to this config ID is enabled */
            if (llCsDbIsProcedureEnabled(connPtr->connId, configId) == CS_DISABLE)
            {
                llCsRclFreeTask(connPtr->connId);
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
                    if (connPtr->llTask->taskID == LL_TASK_ID_CENTRAL)
                    {
                        llCsDbSetBleRole(CS_BLE_ROLE_CENTRAL);
                    }
                    else
                    {
                        llCsDbSetBleRole(CS_BLE_ROLE_PERIPHERAL);
                    }

                    llCsSetupSubEvent(connPtr->connId);

                    /* Schedule the CS */
                    if (llState == LL_STATE_IDLE)
                    {
                        MAP_llScheduler();
                    }
                }
            }
        }
        status = CS_STATUS_SUCCESS;
    }
    else
    {
        status = CS_STATUS_UNEXPECTED_PARAMETER;
    }
    return status;
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure.h
 */
csStatus_e llCsStartTestProcedure(void)
{
    csStatus_e status = CS_STATUS_SUCCESS;

    status = llCsProcedureInitDrbg( CS_TEST_MODE_CONN_ID,
                                    CS_TEST_MODE_CONFIG_ID);
    if (status == CS_STATUS_SUCCESS)
    {
        status = llCsInitProcedureStepList(CS_TEST_MODE_CONN_ID,
                                           CS_TEST_MODE_CONFIG_ID,
                                           TRUE);
        if (status == CS_STATUS_SUCCESS)
        {
            /* Build the step buffers */
            llCsSetupStepBuffers(CS_TEST_MODE_CONN_ID,
                                CS_TEST_MODE_CONFIG_ID,
                                CS_NEW_SUBEVENT,
                                csRclData.csStepsBuff0,
                                csRclData.csStepsBuff1);
        }
    }

    if(status == CS_STATUS_SUCCESS)
    {
        status = llCsSetupRcl(CS_TEST_MODE_CONN_ID, csRclData);
    }

    if (status == CS_STATUS_SUCCESS)
    {
        status = llCsSubmitTestCmd();
    }

    return (status);
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure.h
 */
uint8 llCsSetupSubEvent(uint16 connId)
{
    if (llCsSetupRcl(connId, csRclData) != CS_STATUS_SUCCESS)
    {
        return (CS_STATUS_RCL_SETUP_ERROR);
    }

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure.h
 */
uint8 llCsStartStepListGen(uint16 connId)
{
    uint8 status = CS_STATUS_SUCCESS;
    uint8 configId = llCsDbGetCurrentConfigId(connId);
    if (llCsDbIsProcdureCompleted(connId, CS_START_PROCEDURE))
    {
        /* Initialize the DRBG engine */
        if (llCsProcedureInitDrbg(connId, configId) !=
            CS_STATUS_SUCCESS)
        {
            /* An error occured, disable procedure */
            llCsDbEnableProcedureParams(connId, configId, CS_DISABLE);
            return (CS_STATUS_DRBG_INIT_FAIL);
        }
        status = llCsInitProcedureStepList(connId, configId, TRUE);
        if (status == CS_STATUS_SUCCESS)
        {
            /* Build the step buffers */
            llCsSetupStepBuffers(connId, configId, CS_NEW_SUBEVENT,
                                csRclData.csStepsBuff0,
                                csRclData.csStepsBuff1);
        }

        if (status != CS_STATUS_SUCCESS)
        {
            /* An error occured, disable procedure */
            llCsDbEnableProcedureParams(connId, configId, CS_DISABLE);
        }
        llCsDbClearProcedureCompleted(connId, CS_START_PROCEDURE);
    }
    else if (llCsDbGetNextProcedureFlag(connId, configId))
    {
        status = llCsInitProcedureStepList(connId, configId, TRUE);
        if ( status == CS_STATUS_SUCCESS)
        {
            /* build the step buffers */
            llCsSetupStepBuffers(connId, configId, CS_NEW_SUBEVENT,
                                csRclData.csStepsBuff0,
                                csRclData.csStepsBuff1);
        }
        if (status != CS_STATUS_SUCCESS)
        {
            /* An error occured, disable procedure */
            llCsDbEnableProcedureParams(connId, configId, CS_DISABLE);
        }
        llCsDbSetNextProcedureFlag(connId, FALSE);
    }
    return status;
}
/*******************************************************************************
 * Public function defined in ll_cs_procedure.h
 */
csStatus_e llCsInitProcedureStepList(uint16 connId, uint8 configId, uint8 isfirstSE)
{
    csConfigurationSet_t csConfig;
    csProcedureEnable_t procParams;
    uint8 numChans;
    uint16 numMainModeSteps = 0;
    uint16 maxStepsPerSubevent = 0;
    uint16 remSteps = 0;

    llCsDbGetConfiguration(connId, configId, &csConfig);
    llCsDbGetProcedureEnableData(connId, configId, &procParams);

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
        MAP_llCsInitChanIdxArr(configId, connId, (uint8*)&csConfig);
        numChans = llCsDbGetNumChan(connId, configId);
        numMainModeSteps = numChans * csConfig.chMRepetition;
    }
    else
    {
        /* not the first subevent, check if more steps are needed */
        numMainModeSteps = llCsDbGetRemainingMmSteps(connId, configId);
        /* if all mainMode steps were done
           or, stepCount reached the max.*/
        if ((numMainModeSteps == 0) ||
            (llCsSecGetStepCount() >= CS_MAX_STEPS_PER_PROCEDURE))
        {
            /* No more steps are needed! */
            /* set remaining steps to 0 (if not already so) */
            llCsDbSetRemainingMmSteps(connId, configId, 0);
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
    llCsDbSetRemainingMmSteps(connId, configId, remSteps);
    /* set number of steps for the RCL command data */
    csRclData.numSteps = csNumSteps;

    /* set the subevent info counters */
    llCsDbSetSubeventCount(connId, CS_SE_INFO_NUM_STPES, csNumSteps);
    llCsDbSetSubeventCount(connId, CS_SE_INFO_STEP_COUNT, 0);
    llCsDbSetSubeventCount(connId, CS_SE_INFO_REPORT_COUNT, 0);

    /* Initialize the step buffers (allocate, set to zero, etc) */
    if (llCsInitStepBuffers() != CS_STATUS_SUCCESS)
    {
        return (CS_STATUS_INSUFFICIENT_MEMORY);
    }

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
csStatus_e llCsInitChanIdxArr(uint8 configId, uint16 connId,
                            csConfigurationSet_t* csConfig)
{
    csStatus_e status = CS_STATUS_SUCCESS;
    if (csConfig != NULL)
    {
        /* Channel Override is not used, so go ahead and init the channel indes array */
        uint8 channelIndexArray[CS_FILTERED_CHAN_MAX_SIZE];
        uint8 numChans;
        csChanInfo_t* chanInfo;
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
    }
    else
    {
        status = CS_STATUS_UNEXPECTED_PARAMETER;
    }

    return status;
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
csStatus_e llCsInitStepBuffers(void)
{
    if (csRclData.buffsAllocated == FALSE)
    {
        /* Allocate Memory for StepList */
        csRclData.csStepsBuff0 = (csStepsBuffer_t*)MAP_osal_mem_alloc( sizeof(csStepsBuffer_t) + sizeof(RCL_CmdBleCs_Step) * CS_STEP_BUFF_MAX_SIZE);
        if (csRclData.csStepsBuff0 == NULL)
        {
            return (CS_STATUS_INSUFFICIENT_MEMORY);
        }

        csRclData.csStepsBuff1 = (csStepsBuffer_t*)MAP_osal_mem_alloc( sizeof(csStepsBuffer_t) + sizeof(RCL_CmdBleCs_Step) * CS_STEP_BUFF_MAX_SIZE);
        if (csRclData.csStepsBuff1 == NULL)
        {
            llCsFreeStepsAndResults();
            return (CS_STATUS_INSUFFICIENT_MEMORY);
        }

        /* Allocate Memory for Step Result List */
        csRclData.csStepResultsBuff0 = (uint8_t*)MAP_osal_mem_alloc(CS_RESULT_BUFF_SIZE);
        if (csRclData.csStepResultsBuff0 == NULL)
        {
            llCsFreeStepsAndResults();
            return (CS_STATUS_INSUFFICIENT_MEMORY);
        }

        /* Set Initial Values */
        MAP_osal_memset(csRclData.csStepResultsBuff0, 0, CS_RESULT_BUFF_SIZE);
        MAP_osal_memset(&csOutput, 0x00, sizeof(RCL_CmdBleCs_Stats));
        csRclData.csOutput = &csOutput;
        csRclData.buffsAllocated = TRUE;
    }

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
csStatus_e llCsSetupStep(uint8 stepMode, uint16 connId,
                         RCL_CmdBleCs_Step* stepData,
                         csConfigurationSet_t* csConfig)
{
    csStatus_e status = CS_STATUS_SUCCESS;
    static const pfnSetupStepFunc setupStepFuncs[] = { llCsSetupStep0,
                                                       llCsSetupStep1,
                                                       llCsSetupStep2,
                                                       llCsSetupStep3 };

    stepData->antennaPermIdx = 0; // currently irrelevant
    stepData->channelIdx = MAP_llCsSelectStepChannel(connId, (uint8*)csConfig, stepMode);

    if (stepData->channelIdx == INVALID_CS_CHANNEL_IDX)
    {
        status = CS_STATUS_INVALID_CHAN_IDX;
    }
    else
    {
        if (stepMode < (sizeof(setupStepFuncs) / sizeof(setupStepFuncs[0])))
        {
            setupStepFuncs[stepMode](csConfig->role, stepData, csConfig->rttType);
        }
        else
        {
            status = CS_STATUS_INVALID_STEP_MODE;
        }
    }
    return status;
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
void llCsSetupStep0(uint8 role, RCL_CmdBleCs_Step* stepData, uint8 rttType)
{
    if (stepData != NULL)
    {
        stepData->mode = CS_MODE_0;
        /* Select AA */
        MAP_llCsSelectAA(role, &stepData->aaRx, &stepData->aaTx);

        /* No Payload for step 0*/
        stepData->payloadLen = 0;

        /* No Extension Bit for mode 0 */
        stepData->toneExtension = 0;
    }
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
void llCsSetupStep1(uint8 role, RCL_CmdBleCs_Step* stepData, uint8 rttType)
{
    if (stepData != NULL)
    {
        stepData->mode = CS_MODE_1;
        /* Select AA */
        MAP_llCsSelectAA(role, &stepData->aaRx, &stepData->aaTx);

        /* Set Payload Len */
        stepData->payloadLen = llCsConvertRttType(rttType);
        /* Get Random Sequence (optional) */
        MAP_llCsGetRandomSequence(role, &stepData->payloadTx[0],
                            &stepData->payloadRx[0], CS_GET_PL_LEN(rttType));

        /* No extension bit for stepMode 1 */
        stepData->toneExtension = 0;
    }
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
void llCsSetupStep2(uint8 role, RCL_CmdBleCs_Step* stepData, uint8 rttType)
{
    if (stepData != NULL)
    {
        stepData->mode = CS_MODE_2;
        /* Set Tone Extention Bit */
        stepData->toneExtension = MAP_llCsGetToneExtention();

        /* Set Mode-2 irrelevant step info to 0 */
        stepData->aaRx = 0;
        stepData->aaTx = 0;
        stepData->payloadLen = 0;
        // TODO when working with multiple antennas #BLE_LOKI-1366
        // llCsRandomizeAntennaPaths
        VOID rttType;
    }
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
void llCsSetupStep3(uint8 role, RCL_CmdBleCs_Step* stepData, uint8 rttType)
{
    if (stepData != NULL)
    {
        stepData->mode = CS_MODE_3;
        /* Select AA */
        MAP_llCsSelectAA(role, &stepData->aaRx, &stepData->aaTx);

        /* Set Payload Len */
        stepData->payloadLen = llCsConvertRttType(rttType);

        /* Get Random Sequence (optional) */
        MAP_llCsGetRandomSequence(role, &(stepData->payloadTx[0]),
                            &(stepData->payloadRx[0]), CS_GET_PL_LEN(rttType));

        /* Set Tone Extention Bit */
        stepData->toneExtension = MAP_llCsGetToneExtention();
    }
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
    uint8 chan;
    if (csConfig != NULL)
    {
        /* Return the channel index */
        if (stepMode != CS_MODE_0)
        {
            llCsShuffleMainModeChannelIndexArray(TRUE, connId, csConfig);
        }
        chan = llCsDbGetChannelIndex(connId, csConfig->configId, stepMode);
    }
    else
    {
        chan = INVALID_CS_CHANNEL_IDX;
    }
    return chan;
}

/*******************************************************************************
 * Internal function defined in ll_cs_mgr_internal.h
 */
void llCsShuffleMainModeChannelIndexArray(uint8 isFirstSE, uint16 connId,
                                      csConfigurationSet_t* csConfig)
{
    // Check if Channel Override is used
    if (csConfig != NULL)
    {
        // Channel Override is not used, so shuffle main mode index array
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
void llCsFreeStepsAndResults(void)
{
    if (csRclData.buffsAllocated == TRUE)
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
    uint16 t0 = llCsDbGetTfcs(config->tFCs) +
                CS_MODE0_DUR(CS_GET_T_SYNC(config->csSyncPhy,
                             CS_GET_PL_LEN(config->rttType)),
                             llCsDbGetTip(config->tIP1));
    uint16 mainModeTime = llCsMainModeDur(config->mainMode, config);
    uint32 timeLeft = 0; // time left for mainMode Steps
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
uint16 llCsMainModeDur(uint8 mode, csConfigurationSet_t* pConfig)
{
    uint16 retVal = 0;
    if (pConfig != NULL)
    {
        uint16 csPlSize = CS_GET_PL_LEN(pConfig->rttType);
        uint8 tFCs = llCsDbGetTfcs(pConfig->tFCs);
        switch (mode)
        {
            case CS_MODE_1:
            {
                retVal = tFCs +
                        CS_MODE1_DUR(CS_GET_T_SYNC(pConfig->csSyncPhy, csPlSize),
                                    llCsDbGetTip(pConfig->tIP1));
                break;
            }
            case CS_MODE_2:
            {
                retVal = tFCs +
                        CS_MODE2_DUR(llCsDbGetTpm(pConfig->tPM),
                                    llCsDbGetTip(pConfig->tIP2));
                break;
            }
            case CS_MODE_3:
            {
                retVal =  tFCs +
                        CS_MODE3_DUR(CS_GET_T_SYNC(pConfig->csSyncPhy, csPlSize),
                        llCsDbGetTpm(pConfig->tPM), llCsDbGetTip(pConfig->tIP2));
                break;
            }
            default:
            {
                retVal = 0;
            }
        }
    }
    return retVal;
}

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h
 */
void llCsSetupStepBuffers(uint16 connId, uint8 configId,
                          csNewSubevent_e isNewSubevent,
                          csStepsBuffer_t* csStepsBuff0,
                          csStepsBuffer_t* csStepsBuff1 )
{
    csConfigurationSet_t csConfig;
    uint8 nSubeventSteps = llCsDbGetSubeventInfo(connId, CS_SE_INFO_NUM_STPES);
    uint8 stepListSize = CS_NUM_BUFF_STEPS(nSubeventSteps);

    VOID llCsDbGetConfiguration(connId, configId, &csConfig);

    /* Setup first buffer of a new subevent */
    if ( (csStepsBuff0 != NULL) && (isNewSubevent == CS_NEW_SUBEVENT) )
    {
        /* Setup mode-0 steps */
        llCsFillBuffer( connId, CS_MODE_0, csConfig.modeZeroSteps, &csStepsBuff0->steps[0]);

        /* Setup the first buffer */
        llCsFillBuffer( connId, csConfig.mainMode, stepListSize - csConfig.modeZeroSteps, &csStepsBuff0->steps[csConfig.modeZeroSteps]);
    }

    /* Either setup second buffer of a new subevent
    *  Or, setup the provided buffer of an ongoing subevent */
    if ((csStepsBuff1 != NULL) &&
        (((isNewSubevent == CS_NEW_SUBEVENT) && (stepListSize < nSubeventSteps)) ||
         ((isNewSubevent == CS_CONTINUE_SUBEVENT) && (nSubeventSteps > 0U))))
    {
        uint16 stepCount = llCsDbGetSubeventInfo(connId, CS_SE_INFO_STEP_COUNT);
        uint8 nBuffSteps = nSubeventSteps - stepCount;
        stepListSize = CS_NUM_BUFF_STEPS(nBuffSteps);
        llCsFillBuffer(connId, csConfig.mainMode, stepListSize, &csStepsBuff1->steps[0]);
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure.h
 */
void llCsFillBuffer(uint16 connId, uint8 mode, uint8 numSteps, RCL_CmdBleCs_Step* steps)
{
    uint8 configId = llCsDbGetCurrentConfigId(connId);
    csConfigurationSet_t csConfig;
    VOID llCsDbGetConfiguration(connId, configId, &csConfig);

    for (uint8 i = 0; i < numSteps; i++)
    {
        llCsSetupStep(mode, connId, &steps[i], &csConfig);
        llCsSecIncreaseStepCount();
        llCsDbIncrementSubeventInfoCounter(connId, CS_SE_INFO_STEP_COUNT, 1);
    }
}

/*******************************************************************************
 * function defined in ll_cs_procedure.h.
 */
void llCsSelectAA(uint8 csRole, uint32_t* aaRx, uint32_t* aaTx)
{
    if (aaRx && aaTx)
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
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure_internal.h.
 */
void llCsGetRandomSequence(uint8 csRole, uint32_t* pTx, uint32_t* pRx,
                            uint8 payloadLen)
{
    if (pTx && pRx)
    {
        // There ar eno overrides, so generate payload
        if (payloadLen > 0U)
        {
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
            /* The DRBG output is transmitted in reverse */
            llCsReversePayload((uint8*)pRx, (uint8*)pTx, payloadLen >> 3);
        }
        llCsLogRandomSequence(pTx, pRx, payloadLen);
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_procedure_internal.h.
 */
uint8 llCsGetToneExtention(void)
{
    uint8 toneExtension = 0;

    /* Override is not used so get Tone Bits from DRBG */
    csDrbg(CS_TONE_EXTENSION_BITS_NUM, &toneExtension, CS_TID_CS_TONE_SLOT);

    /* Consider only 2 bits and reverse */
    toneExtension = CS_REVERSE_TONE_EXTENSION_BITS(toneExtension & 0x03U);

    return toneExtension;
}

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

/*******************************************************************************
 * Internal function defined in ll_cs_procedure_internal.h.
 */
void llCsReversePayload(uint8* pl1, uint8* pl2, uint8 size)
{
    if (pl1 && pl2)
    {
        MAP_LL_ENC_ReverseBytes(pl1, size);
        MAP_LL_ENC_ReverseBytes(pl2, size);
    }
}
