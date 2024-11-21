/******************************************************************************

 @file  ll_cs_rf.c

 @brief CS RF funcs

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
#include "cs/ll_cs_rcl.h"
#include "cs/ll_cs_rcl_internal.h"
#include "cs/ll_cs_procedure.h"
#include "cs/ll_cs_common.h"
#include "cs/ll_cs_mgr.h"
#include "cs/ll_cs_common.h"
#include "cs/ll_cs_sec.h"
#include "cs/ll_cs_logs.h"
#include "ll.h"
#include "ll_common.h"
#include "ble.h"
#include "ll_rat.h"
#include "map_direct.h"
#include <ti/log/Log.h>

/*******************************************************************************
 * CONSTANTS
 */


/*******************************************************************************
 * MACROS
 */
#define CS_SUBEVENT_DONE(i, nStepsReported)                                    \
    (i >= nStepsReported) ? CS_REPORT_DONE : CS_REPORT_NOT_DONE

/*******************************************************************************
 * EXTERNS
 */
extern void ll_rclCsCallback(RCL_Command* cmd, LRF_Events lrfEvents,
                             RCL_Events rclEvents);

// Note: to check if the RTT is correct this buffer should be used
// RCL_CmdBleCs_StepResult_Internal
// ble_cs_step_results_internal[BLE_CS_NUM_STEPS] = {0};
/*******************************************************************************
 * TYPEDEFS
 */
// CS RCL information
typedef struct
{
    RCL_Handle rclHandle;
    RCL_Client rclClient;
    RCL_CmdBleCs rclCmd;
    taskInfo_t* csTask;
} llCsRCL_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */
/* CS RCL info including rclHanlde, rclClient, rclCmd */
llCsRCL_t csRcl;

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
 * Public function defined in ll_cs_rcl.h.
 */
csStatus_e llCsSetupRcl(uint16 connId, csRclCmdData_t csRclDataInt)
{
    uint8 configId = llCsDbGetCurrentConfigId(connId);
    csConfigurationSet_t csConfig;
    uint16 subeventCount = llCsDbGetProcCounter(connId, CS_PROC_INFO_SUBEVENT_C);

    if (llCsDbGetConfiguration(connId, configId, &csConfig) ==
        CS_STATUS_DISABLED_CONFIG_ID)
    {
        return CS_STATUS_DISABLED_CONFIG_ID;
    }

    if ((llCsDbGetProcCounter(connId, CS_PROC_C) == 0) &&
        (llCsDbGetProcCounter(connId, CS_PROC_INFO_SUBEVENT_C) == 0))
    {
        /* This the first subevent and first procedure */
        /* Init the RCL command */
        llCsInitRclCmd(connId, csRclDataInt, &csConfig);
    }

    /* Setup the command start time */
    csRcl.rclCmd.common.timing.absStartTime = llCsSetupCmdStartTime( connId,
                                                                     csConfig.role,
                                                                     subeventCount,
                                                                     csRcl.rclCmd);

    /* Setup the buffers */
    llCsRClBufferSetup(csRclDataInt);

    if (csRcl.rclCmd.stats != NULL)
    {
        MAP_osal_memset(csRcl.rclCmd.stats, 0x00, sizeof(RCL_CmdBleCs_Stats));
    }

    csRcl.rclCmd.results = NULL;
    // Note: to use the legacy results buffer this should be used
    // csRcl.rclCmd.results = ble_cs_step_results_internal;

    if (csRcl.csTask->taskID != LL_TASK_ID_CS)
    {
        // Create a CS task
        csRcl.csTask = MAP_llAllocTask(LL_TASK_ID_CS);
        csRcl.csTask->command = (uint32)&csRcl.rclCmd;
        // The scheduler shouldn't do anything sbefore scheduling this command
        csRcl.csTask->setup = NULL;
    }

    csRcl.csTask->startTime = csRcl.rclCmd.common.timing.absStartTime;

    return CS_STATUS_SUCCESS;
}

/*******************************************************************************
 * Public function defined in ll_cs_rcl.h.
 */
void llCsInitRclCmd(uint16 connId, csRclCmdData_t csRclDataInt, csConfigurationSet_t* csConfig)
{
    csDefaultSettings_t defaultSettings;
    /* Yes it is. Setup the command */
    csRcl.rclCmd = RCL_CmdBleCs_DefaultRuntime();

    csRcl.rclCmd.common.scheduling = RCL_Schedule_AbsTime;
    csRcl.rclCmd.common.allowDelay = FALSE;

    /* Set initiale status to idle */
    csRcl.rclCmd.common.status = RCL_CommandStatus_Idle;
    csRcl.rclCmd.common.runtime.callback = ll_rclCsCallback;
    csRcl.rclCmd.common.runtime.lrfCallbackMask.value = LRF_EventRxOk.value;
    // Need to add |RCL_EventSoftwareTriggered when S2R will be supported
    csRcl.rclCmd.common.runtime.rclCallbackMask.value = RCL_EventLastCmdDone.value      |
                                                        RCL_EventRxBufferFinished.value |
                                                        RCL_EventTxBufferFinished.value;
    csRcl.rclCmd.common.phyFeatures = csConfig->csSyncPhy;

    if (csConfig->role == CS_ROLE_INITIATOR)
    {
        /* Define the role of the device */
        csRcl.rclCmd.mode.role = RCL_CmdBleCs_Role_Initiator;
    }
    else
    {
        csRcl.rclCmd.mode.role = RCL_CmdBleCs_Role_Reflector;
    }

    /* Update the command descriptor with the input arguments given from the
        * user */
    csRcl.rclCmd.mode.nSteps = csRclDataInt.numSteps;
    if (csConfig->csSyncPhy == 1)
    {
        csRcl.rclCmd.mode.phy = 0;
    }
    else
    {
        csRcl.rclCmd.mode.phy = 1;
    }
    csRcl.rclCmd.mode.repeatSteps = 0;
    csRcl.rclCmd.mode.chFilterEnable = 0;
    csRcl.rclCmd.antennaConfig.select = 0; // #BLE_LOKI-1366
    csRcl.rclCmd.antennaConfig.gpoMask = 0x0F;
    csRcl.rclCmd.antennaConfig.gpoVal[0] = 0x01;
    csRcl.rclCmd.antennaConfig.gpoVal[1] = 0x02;
    csRcl.rclCmd.antennaConfig.gpoVal[2] = 0x03;
    csRcl.rclCmd.antennaConfig.gpoVal[3] = 0x04;

    csRcl.rclCmd.timing.tFcs =
        RCL_BLE_CS_US_TO_MCE_TIMER(llCsDbGetTfcs(csConfig->tFCs));
    csRcl.rclCmd.timing.tFm = RCL_BLE_CS_US_TO_MCE_TIMER(CS_DEFAULT_TFM);
    csRcl.rclCmd.timing.tPm =
        RCL_BLE_CS_US_TO_MCE_TIMER(llCsDbGetTpm(csConfig->tPM));
    csRcl.rclCmd.timing.tIp1 =
        RCL_BLE_CS_US_TO_MCE_TIMER(llCsDbGetTip(csConfig->tIP1));
    csRcl.rclCmd.timing.tIp2 =
        RCL_BLE_CS_US_TO_MCE_TIMER(llCsDbGetTip(csConfig->tIP2));

    llCsDbGetDefaultSettings(connId, &defaultSettings);

    // Setting the RX window. This value will be ignored if the CS role is
    // initiator
    csRcl.rclCmd.timing.tRxWideningR0 = llCsGetRxWidening();
    csRcl.rclCmd.timing.tSw = llCsDbGetSwitchTime();
    csRcl.rclCmd.timing.tSwAdjustA = 0;
    csRcl.rclCmd.timing.tSwAdjustB = 0;
    csRcl.rclCmd.frontend.rxGain = 0;
    csRcl.rclCmd.frontend.foffOverride = 0;
    csRcl.rclCmd.frontend.foffOverrideEnable = 0;
    csRcl.rclCmd.stats = csRclDataInt.csOutput;
    csRcl.rclCmd.frontend.txPower = llCsRclGetTxPower(defaultSettings.maxTxPower);
}

/*******************************************************************************
 * Public function defined in ll_cs_rcl.h.
 */
csStatus_e llCsSubmitTestCmd(void)
{
    if (llCsDbGetTestMode() == CS_TEST_MODE_ENABLE)
    {
        RCL_CommandStatus submitStatus = RCL_Command_submit(
                                    MAP_llScheduler_getHandle(LL_TASK_ID_CS),
                                    (RCL_Command_Handle)&csRcl.rclCmd);
        if ((submitStatus >= RCL_CommandStatus_Error)||
            (submitStatus >= RCL_CommandStatus_Finished) ||
            (submitStatus == RCL_CommandStatus_Idle))
        {
            return CS_STATUS_RCL_SUBMIT_ERROR;
        }
        else
        {
            return CS_STATUS_SUCCESS;
        }
    }
    else
    {
        // Not in a Test Mode, shouldn't be here
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
}

/*******************************************************************************
 * Internal function defined in ll_cs_rcl_internal.h
 */
uint16_t llCsGetRxWidening(void)
{
    if (llCsDbGetTestMode() == CS_TEST_MODE_ENABLE)
    {
        // Test Mode, so wait forever
        return 0xFFFF;
    }
    else
    {
        return RCL_BLE_CS_US_TO_MCE_TIMER(250);
    }
}

/*******************************************************************************
 * Internal function defined in ll_cs_rcl_internal.h
 */
void llCsRclScheduleNextSubevent(void)
{
    if (llCsDbGetTestMode() == CS_TEST_MODE_DISABLE)
    {
        MAP_llScheduler();
    }
    else
    {
        llCsSubmitTestCmd();
    }
}

/*******************************************************************************
 * Internal function defined in ll_cs_rcl_internal.h
 */
RCL_Command_TxPower llCsRclGetTxPower(int8 maxTxPower)
{
    RCL_Command_TxPower retVal;
    switch (maxTxPower)
    {
        case (CS_USE_MIN_TX_POWER):
        {
            retVal = LRF_TxPower_Use_Max;
            break;
        }
        case (CS_USE_MAX_TX_POWER):
        {
            retVal = LRF_TxPower_Use_Min;
            break;
        }
        default:
        {
            retVal = (RCL_Command_TxPower)maxTxPower;
        }
    }
    return retVal;
}

/*******************************************************************************
 * Internal function defined in ll_cs_rcl_internal.h
 */

uint32_t llCsSetupCmdStartTime( uint16 connId, uint8 role, uint8 subEventCount,
                                RCL_CmdBleCs rclCmd )
{
    uint32_t anchorPoint = 0;
    uint32_t cmdStartTime = 0;
    uint32_t timerDrift = 0;
    uint8 configId = llCsDbGetCurrentConfigId(connId);
    uint8 testMode = llCsDbGetTestMode();
    csProcedureEnable_t csData;
    llConnState_t* connPtr = MAP_llDataGetConnPtr(connId);
    if (testMode == CS_TEST_MODE_DISABLE && connPtr)
    {
        timerDrift = connPtr->timerDrift;
        // Increase connection priority
        connPtr->connPriority = LL_QOS_CS_PRIORITY;
        if (subEventCount == 0)
        {
            anchorPoint = connPtr->llTask->anchorPoint;
            llCsDbSetEventAnchorPoint(connId, anchorPoint);
        }
        else
        {
            anchorPoint = llCsDbGetEventAnchorPoint(connId);
        }
    }
    else
    {
        timerDrift = 0;
        if (subEventCount == 0)
        {
            anchorPoint = llGetCurrentTime() + RAT_TICKS_IN_1MS;
            llCsDbSetEventAnchorPoint(connId, anchorPoint);
        }
        else
        {
            anchorPoint = llCsDbGetEventAnchorPoint(connId);
        }
    }

    /* Clear next procedure flag */
    llCsDbSetNextProcedureFlag(connId, FALSE);
    llCsDbGetProcedureEnableData(connId, configId, &csData);
    cmdStartTime = anchorPoint + csData.offset*RAT_TICKS_IN_1US +
                csData.subEventInterval*RAT_TICKS_IN_625US*subEventCount;

    if (CS_ROLE_INITIATOR == role )
    {
        return cmdStartTime + RAT_TICKS_IN_64US;
        // 64us anchor point = last start time but we need to add the time
        // between the start time and the actual time the radio starts the
        // preamble = (frequency synthisizes calibration) fs + pilot tune
    }
    else
    {
        if ( testMode == CS_TEST_MODE_ENABLE )
        {
            csRcl.rclCmd.common.scheduling = RCL_Schedule_Now;
            return 0;
        }
        else
        {
            return cmdStartTime - timerDrift;
        }
    }
}

/*******************************************************************************
 * Internal function defined in ll_cs_rcl_internal.h
 */
void llCsRClBufferSetup(csRclCmdData_t csRclDataInt)
{
    if ((csRclDataInt.csStepsBuff0 == NULL) ||
        (csRclDataInt.csStepsBuff1 == NULL) ||
        (csRclDataInt.csStepResultsBuff0 == NULL) ||
        (csRclDataInt.csOutput == NULL))
    {
        return;
    }
    List_clearList(&csRcl.rclCmd.resultBuffers);
    List_clearList(&csRcl.rclCmd.resultBuffersDone);
    // List_clearList(&csRcl.rclCmd.s2rBuffers);
    // List_clearList(&csRcl.rclCmd.s2rBuffersDone);

    ble_cs_steps_buffer_size = sizeof(csStepsBuffer_t) + sizeof(RCL_CmdBleCs_Step) * (CS_STEP_BUFF_MAX_SIZE);
    ble_cs_step_results_buffer_size = CS_RESULT_BUFF_SIZE;

    /* Prepare the TX buffer containing the step list */
    /* ------------------------------------- */
    RCL_MultiBuffer* pStepBuffer;
    uint8 bufferSteps = csRclDataInt.numSteps >= CS_STEP_BUFF_MAX_SIZE ? CS_STEP_BUFF_MAX_SIZE : csRclDataInt.numSteps;

    pStepBuffer = (RCL_MultiBuffer*)csRclDataInt.csStepsBuff0;
    RCL_MultiBuffer_init(pStepBuffer, ble_cs_steps_buffer_size);
    RCL_MultiBuffer_commitBytes(pStepBuffer,
                                sizeof(RCL_CmdBleCs_Step) * bufferSteps);
    RCL_MultiBuffer_put(&csRcl.rclCmd.stepBuffers, pStepBuffer);

    if (csRclDataInt.numSteps > CS_STEP_BUFF_MAX_SIZE)
    {
        bufferSteps = (csRclDataInt.numSteps - CS_STEP_BUFF_MAX_SIZE) >= CS_STEP_BUFF_MAX_SIZE ? CS_STEP_BUFF_MAX_SIZE : csRclDataInt.numSteps - CS_STEP_BUFF_MAX_SIZE;
        pStepBuffer = (RCL_MultiBuffer*)csRclDataInt.csStepsBuff1;
        RCL_MultiBuffer_init(pStepBuffer, ble_cs_steps_buffer_size);
        RCL_MultiBuffer_commitBytes(pStepBuffer,
                                    sizeof(RCL_CmdBleCs_Step) * bufferSteps);
        RCL_MultiBuffer_put(&csRcl.rclCmd.stepBuffers, pStepBuffer);
    }

    /* Prepare an RX multibuffer for results */
    /* ------------------------------------- */
    RCL_MultiBuffer* pResultBuffer;

    pResultBuffer = (RCL_MultiBuffer*)csRclDataInt.csStepResultsBuff0;
    RCL_MultiBuffer_init(pResultBuffer, ble_cs_step_results_buffer_size);
    RCL_MultiBuffer_put(&csRcl.rclCmd.resultBuffers, pResultBuffer);
}

/*******************************************************************************
 * Internal function defined in ll_cs_rcl_internal.h
 */
void llCsClearRclBuffers()
{
    /* Clear the done pointers */
    List_clearList(&csRcl.rclCmd.stepBuffers);
    List_clearList(&csRcl.rclCmd.stepBuffersDone);
    List_clearList(&csRcl.rclCmd.resultBuffers);
    List_clearList(&csRcl.rclCmd.resultBuffersDone);
    // List_clearList(&csRcl.rclCmd.s2rBuffersDone);
}

/*******************************************************************************
 * External function defined in ll_cs_rcl.h
 */
void llCsSteps_PostProcess(void)
{
    /* A steps buffer was consumed. */
    csStepsBuffer_t* pBuffer;
    uint16 connId = llCsDbGetCurrentConnId();
    uint8 configId = llCsDbGetCurrentConfigId(connId);
    uint8 numBuffSteps = llCsGetNumStepsInBuffer(connId);

    /* Get Step Buffers Done */
    pBuffer = (csStepsBuffer_t*)RCL_MultiBuffer_get(&csRcl.rclCmd.stepBuffersDone);
    if (pBuffer != NULL)
    {
        RCL_MultiBuffer_clear((RCL_MultiBuffer*)pBuffer);
        if ((llCsDbGetNextSubeventFlag(connId) == CS_PREP_CURR_SUBEVENT) &&
            (numBuffSteps > 0U))
        {
            RCL_MultiBuffer_init((RCL_MultiBuffer*)pBuffer, ble_cs_steps_buffer_size);
            RCL_MultiBuffer_put(&csRcl.rclCmd.stepBuffers, (RCL_MultiBuffer*)pBuffer);
            RCL_MultiBuffer_commitBytes((RCL_MultiBuffer*)pBuffer,
                                        sizeof(RCL_CmdBleCs_Step)*numBuffSteps);
            llCsSetupStepBuffers(connId, configId, CS_CONTINUE_SUBEVENT, NULL, pBuffer);
        }
        else
        {
            /* Prepare first buffer of the next subevent */
            llCsPrepareForNextSubevent(connId, configId, numBuffSteps, pBuffer);
        }
    }
}

/*******************************************************************************
 * External function defined in ll_cs_rcl.h
 */
void llCsPrepareForNextSubevent(uint16 connId, uint8 configId, uint8 numBuffSteps, csStepsBuffer_t* pBuffer)
{
    if (numBuffSteps == 0U)
    {
        /* This Subevent setup is DONE. But we may need another Subevent */
        /* Check the number of remaining steps in the Procedure */
        /* And the number of Subevents in the CS Event */
        if ( (llCsDbGetRemainingMmSteps(connId, configId) > 0U) &&
            (llCsDbGetProcCounter(connId, CS_PROC_INFO_SUBEVENT_C) < llCsDbGetSubeventsPerEvent(connId, configId)))
        {
            llCsDbSetNextSubeventFlag(connId, CS_PREP_NEXT_SUBEVENT);
            if (llCsInitProcedureStepList(connId, configId, FALSE) == CS_STATUS_SUCCESS)
            {
                /* Prepare only the FIRST buffer since the second buffer is busy */
                llCsSetupStepBuffers(connId, configId, CS_NEW_SUBEVENT, pBuffer, NULL);
            }
        }
    }
    else
    {
        /* Prepare only the SECOND buffer since the first is ready. */
        llCsSetupStepBuffers(connId, configId, CS_CONTINUE_SUBEVENT, NULL, pBuffer);
    }
}

/*******************************************************************************
 * External function defined in ll_cs_rcl.h
 */
void llCsSubevent_PostProcess(void)
{
    /* A subevent was completed */
    uint16 connId = llCsDbGetCurrentConnId();
    uint8 configId = llCsDbGetCurrentConfigId(connId);
    uint16 procedureCounter = 0;
    csProcedureEnable_t procEnable;
    bool endProcedure = false;

    if (configId == INVALID_CONFIG_ID)
    {
        /* Config ID is invalid */
        llCsClearRclBuffers();
        llCsRclFreeTask(connId);
        return;
    }

    /* Get Enabled Procedure Info */
    llCsDbGetProcedureEnableData(connId, configId, &procEnable);

    if ((llCsDbGetTestMode() != CS_TEST_MODE_DISABLE) &&
        (procEnable.subEventInterval == 0U))
    {
        /* In case of a test mode, if subeventInterval is 0
        This means the the test mode consists of a single subevent
        hence, the procedure is done. */
        endProcedure = true;
    }

    if (llCsDbGetNextSubeventFlag(connId) == CS_PREP_NEXT_SUBEVENT)
    {
        /* Need to schedule another subevent */
        VOID llCsDbIncrementProcCounter(connId, CS_PROC_INFO_SUBEVENT_C);
        /* Process the results */
        llCsResults_PostProcess(CS_REPORT_NOT_DONE);
        /* setup subevent (CS rcl command) */
        llCsSetupSubEvent(connId);
        llCsDbSetNextSubeventFlag(connId, CS_PREP_CURR_SUBEVENT);
    }
    else if ( llCsDbGetRemainingMmSteps(connId, configId) &&
              (llCsDbGetProcedureTerminateState(connId) != CS_TERMINATE_RECEIVED) &&
              (!endProcedure))
    {
        if (llCsDbIncrementProcCounter(connId, CS_PROC_INFO_EVENT_C) <
            llCsDbGetEventsPerProcedure(connId))
        {
            /* Process the results */
            llCsResults_PostProcess(CS_REPORT_NOT_DONE);
            /* Need to schedule another event */
            llCsDbSetNextProcedureConnEvent(connId, configId,
                                            procEnable.connEventCount +
                                                procEnable.eventInterval);
            llCsDbResetProcCounter(connId, CS_PROC_INFO_SUBEVENT_C);
            llCsClearRclBuffers();
            /* Setup the next subevent steplist */
            if (llCsInitProcedureStepList(connId, configId, FALSE) == CS_STATUS_SUCCESS)
            {
                /* Build the step buffers */
                llCsSetupStepBuffers(connId, configId, CS_NEW_SUBEVENT,
                                    csRclData.csStepsBuff0,
                                    csRclData.csStepsBuff1);
            }

            if (csRcl.csTask != NULL)
            {
                MAP_llFreeTask(&csRcl.csTask);
            }
        }
        else
        {
            /* Procedure ended because Num Events Per Procedure was reached */
            endProcedure = true;
        }
    }
    else
    {
        endProcedure = true;
    }

    if (endProcedure)
    {
        /* Process the results */
        llCsResults_PostProcess(CS_REPORT_DONE);
        /* Procedure ended */

        procedureCounter = llCsDbIncrementProcCounter(connId, CS_PROC_C);
        llCsSecIncProcCounter();
        llCsSecResetStepCount();

        if (procedureCounter >= procEnable.procedureCount)
        {
            /* Done all the procedures, time to disable */
            /* Disable the procedure params */
            llCsDbEnableProcedureParams(connId, configId, CS_DISABLE);
            /* Set next procedure flag to FALSE */
            llCsDbSetNextProcedureFlag(connId, FALSE);
            /* Reset Procedure Counter */
            llCsDbResetProcCounter(connId, CS_PROC_ALL_C);
            /* Disable Test Mode if it was enabled */
            if (llCsDbGetTestMode() == CS_TEST_MODE_TERMINATE)
            {
                MAP_HCI_CS_TestEndCompleteCback(CS_STATUS_SUCCESS);
            }
            if (llCsDbGetTestMode() == CS_TEST_MODE_ENABLE)
            {
                llCsDbSetTestMode(CS_TEST_MODE_FINISHED);
            }
            /* Clear RCL buffers */
            llCsClearRclBuffers();
            llCsDbClearProcedureData(connId, configId);
            /* Free RCL Task */
            llCsRclFreeTask(connId);
        }
        else
        {
            /* Need to repeat this procedure */
            llCsDbSetNextProcedureConnEvent(
                connId, configId,
                procEnable.connEventCount + procEnable.procedureInterval);
            llCsDbSetNextProcedureFlag(connId, TRUE);
            llCsDbResetProcCounter(connId, CS_PROC_INFO_SUBEVENT_C);
            llCsDbResetProcCounter(connId, CS_PROC_INFO_EVENT_C);

            llCsClearRclBuffers();

            if (csRcl.csTask != NULL)
            {
                MAP_llFreeTask(&csRcl.csTask);
            }
        }
    }
    llCsRclScheduleNextSubevent();
}

/*******************************************************************************
 * External function defined in ll_cs_rcl.h
 */
void llCsResults_PostProcess(uint8 procedureDone)
{
    /* Results are available */
    RCL_MultiBuffer* pBuffer;
    uint16 connId = llCsDbGetCurrentConnId();
    uint8 numSteps =
        llCsDbGetSubeventInfo(connId, CS_SE_INFO_NUM_STPES);

    if ((pBuffer = RCL_MultiBuffer_get(&csRcl.rclCmd.resultBuffersDone)) !=
        NULL)
    {
        llCsProcessResults((RCL_CmdBleCs_SubeventResults*)&pBuffer->data,
                           procedureDone);
        RCL_MultiBuffer_clear(pBuffer);
    }
    llCsLogResultStats(csRcl.rclCmd.stats);
}

/*******************************************************************************
 * Public function defined in ll_cs_rcl.h
 */
void llCsProcessResults(RCL_CmdBleCs_SubeventResults* resBuf,
                        uint8 isProcedureDone)
{
    uint16 connId = llCsDbGetActiveConnId();
    uint8 configId = llCsDbGetCurrentConfigId(connId);
    uint16 aclCounter = llCsDbGetAclCounter(connId, configId);
    uint8 i = 0;
    uint8 totalStepsReported;
    uint16 bufSize;
    uint8* pBuf;
    uint8* data;
    csStepResHdr_t* resHdr;

    if (resBuf)
    {
        totalStepsReported = resBuf->numStepsReported;
        /* Prepare subevent report header */
        resBuf->connectionHandle = llCsDbGetReportedConnId();
        resBuf->configID = configId;
        resBuf->procedureCounter = llCsDbGetProcCounter(connId, CS_PROC_C);
        resBuf->abortReason = llCsGetAbortReason(connId);
        resBuf->procedureDoneStatus = llCsGetProcDoneStatus(isProcedureDone,
                                                            resBuf->abortReason);
        resBuf->frequencyCompensation = CS_FC_UNAVAILABLE; // no FAE
        resBuf->numAntennaPath = CS_MAX_ANT_PATH_SUPPORTED;
        pBuf = resBuf->data;

        // This is the First SUBEVENT REPORT
        resBuf->subeventCode = CS_SUBEVENT_RESULT_OPCODE;
        resBuf->startAclConnectionEvent = aclCounter;
        bufSize = CS_SUBEVENT_RESULTS_LEN;

        resHdr = (csStepResHdr_t*)pBuf;
        while (((bufSize + CS_STEP_RES_HEADER_LEN + resHdr->dataLen) <
                LL_MAX_HCI_EVENT_LEN) &&
               (i < totalStepsReported))
        {
            bufSize += (CS_STEP_RES_HEADER_LEN + resHdr->dataLen);
            pBuf += (CS_STEP_RES_HEADER_LEN + resHdr->dataLen);
            i++;
            resHdr = (csStepResHdr_t*)pBuf;
        }
        resBuf->numStepsReported = i;
        resBuf->subeventDoneStatus = CS_SUBEVENT_DONE(i, totalStepsReported);
        MAP_HCI_CS_SubeventResultCback(resBuf, bufSize);

        /* If we didn't report everything. Now we do the rest. */
        while (i < totalStepsReported)
        {
            data = pBuf;
            bufSize = CS_SUBEVENT_RESULTS_CONT_LEN;
            resBuf->subeventCode = CS_CONTINUE_SUBEVENT_RESULT_OPCODE;
            resBuf->numStepsReported = 0;
            while (((bufSize + CS_STEP_RES_HEADER_LEN + resHdr->dataLen) <
                    LL_MAX_HCI_EVENT_LEN) &&
                   (i < totalStepsReported))
            {
                resHdr = (csStepResHdr_t*)pBuf;
                bufSize += (CS_STEP_RES_HEADER_LEN + resHdr->dataLen);
                pBuf += (CS_STEP_RES_HEADER_LEN + resHdr->dataLen);
                resBuf->numStepsReported++;
                i++;
            }
            resBuf->subeventDoneStatus =
                CS_SUBEVENT_DONE(i, totalStepsReported);
            MAP_HCI_CS_SubeventResultContinueCback(resBuf, data, bufSize);
        }
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_rcl.h
 */
void llCsRclFreeTask(uint16 connHandle)
{
    // Free the step if already allocated
    llCsFreeStepsAndResults();

    if (llCsDbGetProcedureTerminateState(connHandle) ==
        CS_TERMINATE_RECEIVED)
    {
        // reset terminateState to CS_TERMINATE_DISABLE
        llCsDbSetProcedureTerminateState(connHandle,
                                         CS_TERMINATE_DISABLE);
    }

    if (llCsDbIsProcdureCompleted(connHandle, CS_IND))
    {
        // Change the CS_IND procedure status
        llCsDbClearProcedureCompleted(connHandle, CS_IND);
    }

    // Init the procedure counter
    llCsDbResetProcCounter(connHandle, CS_PROC_C);

    // Release the CS task
    if (csRcl.csTask != NULL)
    {
        MAP_llFreeTask(&csRcl.csTask);
    }
}
