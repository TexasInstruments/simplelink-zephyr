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
#include "rom_jt.h"
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
    llConnState_t* connPtr = MAP_llDataGetConnPtr(connId);
    uint8 configId = llCsDbGetCurrentConfigId(connId);
    csConfigurationSet_t csConfig;
    csDefaultSettings_t defaultSettings;
    uint16 subeventCount =
        llCsDbGetProcCounter(connId, CS_PROC_INFO_SUBEVENT_C);

    if (llCsDbGetConfiguration(connId, configId, &csConfig) ==
        CS_STATUS_DISABLED_CONFIG_ID)
    {
        return CS_STATUS_DISABLED_CONFIG_ID;
    }

    if ((llCsDbGetProcCounter(connId, CS_PROC_C) == 0) &&
        (llCsDbGetProcCounter(connId, CS_PROC_INFO_SUBEVENT_C) == 0))
    {
        /* Yes it is. Setup the command */
        csRcl.rclCmd = RCL_CmdBleCs_DefaultRuntime();

        csRcl.rclCmd.common.scheduling = RCL_Schedule_AbsTime;
        csRcl.rclCmd.common.allowDelay = FALSE;

        /* Set initiale status to idle */
        csRcl.rclCmd.common.status = RCL_CommandStatus_Idle;
        csRcl.rclCmd.common.runtime.callback = ll_rclCsCallback;
        csRcl.rclCmd.common.runtime.lrfCallbackMask.value = LRF_EventRxOk.value;
        csRcl.rclCmd.common.runtime.rclCallbackMask.value =
            RCL_EventLastCmdDone.value | RCL_EventRxBufferFinished.value |
            RCL_EventTxBufferFinished
                .value; // Need to add |RCL_EventSoftwareTriggered when S2R will
                        // be supported
        csRcl.rclCmd.common.scheduling = RCL_Schedule_AbsTime;
        csRcl.rclCmd.common.phyFeatures = csConfig.csSyncPhy;

        if (csConfig.role == CS_ROLE_INITIATOR)
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
#ifdef CS_TEST
        csRcl.rclCmd.mode.nSteps = BLE_CS_NUM_STEPS;
#else
        csRcl.rclCmd.mode.nSteps = csRclDataInt.numSteps;
#endif
        if (csConfig.csSyncPhy == 1)
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

#ifndef CS_TEST
        csRcl.rclCmd.timing.tFcs =
            RCL_BLE_CS_US_TO_MCE_TIMER(GET_TFCS(csConfig.tFCs));
        csRcl.rclCmd.timing.tFm = RCL_BLE_CS_US_TO_MCE_TIMER(CS_DEFAULT_TFM);
        csRcl.rclCmd.timing.tPm =
            RCL_BLE_CS_US_TO_MCE_TIMER(GET_TPM(csConfig.tPM));
        csRcl.rclCmd.timing.tIp1 =
            RCL_BLE_CS_US_TO_MCE_TIMER(GET_TIP(csConfig.tIP1));
        csRcl.rclCmd.timing.tIp2 =
            RCL_BLE_CS_US_TO_MCE_TIMER(GET_TIP(csConfig.tIP2));
#else
        csRcl.rclCmd.timing.tFcs = RCL_BLE_CS_US_TO_MCE_TIMER(80);
        csRcl.rclCmd.timing.tFm = RCL_BLE_CS_US_TO_MCE_TIMER(80);
        csRcl.rclCmd.timing.tPm = RCL_BLE_CS_US_TO_MCE_TIMER(40);
        csRcl.rclCmd.timing.tIp1 = RCL_BLE_CS_US_TO_MCE_TIMER(40);
        csRcl.rclCmd.timing.tIp2 = RCL_BLE_CS_US_TO_MCE_TIMER(40);
#endif

        llCsDbGetDefaultSettings(connId, &defaultSettings);

        // Setting the RX window. This value will be ignored if the CS role is
        // initiator
        csRcl.rclCmd.timing.tRxWideningR0 = RCL_BLE_CS_US_TO_MCE_TIMER(250);
        csRcl.rclCmd.timing.tSw = 0;
        csRcl.rclCmd.timing.tSwAdjustA = 0;
        csRcl.rclCmd.timing.tSwAdjustB = 0;
        csRcl.rclCmd.frontend.txPower.rawValue = defaultSettings.maxTxPower;
        csRcl.rclCmd.frontend.rxGain = 0;
        csRcl.rclCmd.frontend.foffOverride = 0;
        csRcl.rclCmd.frontend.foffOverrideEnable = 0;

        csRcl.rclCmd.stats = csRclDataInt.csOutput;
    }

    /* Setup the command start time */
    llCsSetupCmdStartTime(csRcl.rclCmd, connPtr, csConfig.role, subeventCount);

    /* Setup the buffers */
    llCsRClBufferSetup(csRclDataInt, TRUE);

    if (csRcl.rclCmd.stats != NULL)
    {
        MAP_osal_memset(csRcl.rclCmd.stats, 0x00, sizeof(RCL_CmdBleCs_Stats));
    }

    csRcl.rclCmd.results = NULL;
    // Note: to use the legacy results buffer this should be used
    // csRcl.rclCmd.results = ble_cs_step_results_internal;

    // Increase connection priority
    connPtr->connPriority = LL_QOS_CS_PRIORITY;
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
 * Internal function defined in ll_cs_rcl_internal.h
 */
void llCsSetupCmdStartTime(RCL_CmdBleCs rclCmd, llConnState_t* connPtr,
                           uint8 role, uint8 subEventCount)
{
    csProcedureEnable_t csData;
    uint8 configId = llCsDbGetCurrentConfigId(connPtr->connId);
    uint32_t anchorPoint;

    llCsDbGetProcedureEnableData(connPtr->connId, configId, &csData);
    /* Clear next procedure flag */
    llCsDbSetNextProcedureFlag(connPtr->connId, FALSE);
    if (subEventCount == 0)
    {
        anchorPoint = connPtr->llTask->anchorPoint;
        llCsDbSetEventAnchorPoint(connPtr->connId, anchorPoint);
    }
    else
    {
        anchorPoint = llCsDbGetEventAnchorPoint(connPtr->connId);
    }

    if (CS_ROLE_INITIATOR == llCsDbGetConfigRole(connPtr->connId, configId))
    {
        csRcl.rclCmd.common.timing.absStartTime =
            anchorPoint + (csData.offset * RAT_TICKS_IN_1US) +
            (csData.subEventInterval * RAT_TICKS_IN_625US) * subEventCount +
            RAT_TICKS_IN_64US;
        // 64us anchor point = last start time but we need to add the time
        // between the start time and the actual time the radio starts the
        // preamble = (frequency synthisizes calibration) fs + pilot tune
    }
    else
    {
        csRcl.rclCmd.common.timing.absStartTime =
            anchorPoint + (csData.offset * RAT_TICKS_IN_1US) +
            (csData.subEventInterval * RAT_TICKS_IN_625US) * subEventCount -
            connPtr->timerDrift;
    }
}

/*******************************************************************************
 * Internal function defined in ll_cs_rcl_internal.h
 */
void llCsRClBufferSetup(csRclCmdData_t csRclDataInt, bool init)
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

    ble_cs_steps_buffer_size =
        sizeof(ble_cs_steps_buffer_t) +
        sizeof(RCL_CmdBleCs_Step) * (CS_STEP_BUFF_MAX_SIZE);
    ble_cs_step_results_buffer_size = CS_RESULT_BUFF_SIZE;

    /* Prepare the TX buffer containing the step list */
    /* ------------------------------------- */
    RCL_MultiBuffer* pStepBuffer;
    uint8 bufferSteps = csRclDataInt.numSteps >= CS_STEP_BUFF_MAX_SIZE
                            ? CS_STEP_BUFF_MAX_SIZE
                            : csRclDataInt.numSteps;

    pStepBuffer = (RCL_MultiBuffer*)csRclDataInt.csStepsBuff0;
    if (init)
        RCL_MultiBuffer_init(pStepBuffer, ble_cs_steps_buffer_size);
    RCL_MultiBuffer_commitBytes(pStepBuffer,
                                sizeof(RCL_CmdBleCs_Step) * bufferSteps);
    RCL_MultiBuffer_put(&csRcl.rclCmd.stepBuffers, pStepBuffer);

    if (csRclDataInt.numSteps > CS_STEP_BUFF_MAX_SIZE)
    {
        bufferSteps = (csRclDataInt.numSteps - CS_STEP_BUFF_MAX_SIZE) >=
                              CS_STEP_BUFF_MAX_SIZE
                          ? CS_STEP_BUFF_MAX_SIZE
                          : csRclDataInt.numSteps - CS_STEP_BUFF_MAX_SIZE;
        pStepBuffer = (RCL_MultiBuffer*)csRclDataInt.csStepsBuff1;
        if (init)
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
    RCL_MultiBuffer* pBuffer;
    llConnState_t* connPtr = MAP_llDataGetConnPtr(llConns.currentConn);
    uint8 numSteps =
        llCsDbGetSubeventInfo(connPtr->connId, CS_SE_INFO_NUM_STPES);
    uint8 stepCount =
        llCsDbGetSubeventInfo(connPtr->connId, CS_SE_INFO_STEP_COUNT);
    uint8 bufferSteps;

    /* Get Step Buffers Done */
    pBuffer = RCL_MultiBuffer_get(&csRcl.rclCmd.stepBuffersDone);
    if (pBuffer)
    {
        RCL_MultiBuffer_clear(pBuffer);
        if (stepCount < numSteps)
        {
            if (numSteps - stepCount < CS_STEP_BUFF_MAX_SIZE)
            {
                bufferSteps = numSteps - stepCount;
            }
            else
            {
                bufferSteps = CS_STEP_BUFF_MAX_SIZE;
            }
            RCL_MultiBuffer_init(pBuffer, ble_cs_steps_buffer_size);
            RCL_MultiBuffer_put(&csRcl.rclCmd.stepBuffers, pBuffer);
            RCL_MultiBuffer_commitBytes(pBuffer, sizeof(RCL_CmdBleCs_Step) *
                                                     bufferSteps);
            llCsGenerateMoreSteps(connPtr->connId, bufferSteps,
                                  (ble_cs_steps_buffer_t*)pBuffer);
        }
    }
}

/*******************************************************************************
 * External function defined in ll_cs_rcl.h
 */
void llCsSubevent_PostProcess(void)
{
    /* a subevent was completed */
    llConnState_t* connPtr = MAP_llDataGetConnPtr(llConns.currentConn);
    if (connPtr != NULL)
    {
        uint16 connId = connPtr->connId;
        uint8 configId = llCsDbGetCurrentConfigId(connId);
        uint16 subeventCounter = 0;
        uint16 procedureCounter = 0;
        csProcedureEnable_t procEnable;
        uint8 endProcedure = 0;

        if ((configId == INVALID_CONFIG_ID) || (!connPtr))
        {
            /* Config ID is invalid */
            llCsClearRclBuffers();
            llCsRclFreeTask(connId, configId);
            return;
        }

        llCsDbResetSubeventInfo(connId);

        /* Get Enabled Procedure Info */
        llCsDbGetProcedureEnableData(connId, configId, &procEnable);

        if (llCsDbGetRemainingMmSteps(connId, configId))
        {
            subeventCounter =
                llCsDbIncrementProcCounter(connId, CS_PROC_INFO_SUBEVENT_C);
            if (subeventCounter < procEnable.subEventsPerEvent)
            {
                /* Process the results */
                llCsResults_PostProcess(CS_REPORT_NOT_DONE);
                /* Need to schedule another subevent */
                llCsClearRclBuffers();
                llCsSetupStepList(connPtr, configId, FALSE);
                llCsSetupSubEvent(connPtr);
            }
            else if (llCsDbIncrementProcCounter(connId, CS_PROC_INFO_EVENT_C) <
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
                /* setup the next subevent steplist */
                llCsSetupStepList(connPtr, configId, FALSE);

                if (csRcl.csTask != NULL)
                {
                    MAP_llFreeTask(&csRcl.csTask);
                }
            }
            else
            {
                /* Procedure ended because Num Events Per Procedure was reached
                 */
                endProcedure = 1;
            }
        }
        else
        {

            endProcedure = 1;
        }
        if (endProcedure)
        {

            /* Process the results */
            llCsResults_PostProcess(CS_REPORT_DONE);
            /* Procedure ended */

            procedureCounter = llCsDbIncrementProcCounter(connId, CS_PROC_C);
            llCsSecIncProcCounter();
            llCsSecResetStepCount();

            if (procedureCounter == procEnable.procedureCount)
            {
                /* Done all the procedures, time to disable */
                llCsDbEnableProcedureParams(connId, configId, CS_DISABLE);
                llCsDbSetNextProcedureFlag(connId, FALSE);
                llCsDbResetProcCounter(connId, CS_PROC_ALL_C);
                /* It's time to end the CS procedure! */
                llCsClearRclBuffers();

                llCsRclFreeTask(connId, configId);
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

        MAP_llScheduler();
    }
}

/*******************************************************************************
 * External function defined in ll_cs_rcl.h
 */
void llCsResults_PostProcess(uint8 procedureDone)
{
    /* Results are available */
    RCL_MultiBuffer* pBuffer;
    llConnState_t* connPtr = MAP_llDataGetConnPtr(llConns.currentConn);
    uint8 numSteps =
        llCsDbGetSubeventInfo(connPtr->connId, CS_SE_INFO_NUM_STPES);

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
    uint8 aclCounter = llCsDbGetAclCounter(connId, configId);
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
        resBuf->connectionHandle = connId;
        resBuf->configID = configId;
        resBuf->procedureCounter = llCsDbGetProcCounter(connId, CS_PROC_C);
        resBuf->procedureDoneStatus = isProcedureDone;
        resBuf->abortReason =
            CS_NO_ABORT; // aborting a procedure not implemented yet
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
void llCsRclFreeTask(uint16 connHandle, uint8 configId)
{
    // Free the step if already allocated
    freeCsStepsAndResults();

    if (llCsDbGetProcedureTerminateState(connHandle, configId) ==
        CS_TERMINATE_RECEIVED)
    {
        // reset terminateState to CS_TERMINATE_DISABLE
        llCsDbSetProcedureTerminateState(connHandle, configId,
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
