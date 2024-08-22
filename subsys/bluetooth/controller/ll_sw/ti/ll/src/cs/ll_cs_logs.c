/******************************************************************************

 @file  ll_cs_logs.h

 @brief This file contains the CS log functions.
        The following logs should be enabled by predefined symbols.
        The logs will cause a delayed performance.
        e.g. step generation may take twice as much time

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************/
/*******************************************************************************
 * INCLUDES
 */
#include "cs/ll_cs_logs.h"
#include <ti/log/Log.h>

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
 * Function Description is in ll_cs_logs.h
 */
void llCsLogDrbgInitResult(uint8* CS_IV, uint8* CS_IN, uint8* CS_PV,
                           drbgParams_t* params)
{
#ifdef CS_LOG_DRBG_RESULTS
    uint8 i = 0;
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS llCsLogDrbgInitResult DRBG Init");
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS llCsLogDrbgInitResult CS_IV, CS_PV");
    for (i = 0; i < CS_CSIV_LEN; i++)
    {
        Log_printf(LogModule_RCL, Log_DEBUG,
                   "DEBUG - STACK CS llCsLogDrbgInitResult 0x%02X, 0x%02X",
                   CS_IV[i], CS_PV[i]);
    }
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS llCsLogDrbgInitResult CS_IN");
    for (i = 0; i < CS_CSIN_LEN; i++)
    {
        Log_printf(LogModule_RCL, Log_DEBUG,
                   "DEBUG - STACK CS llCsLogDrbgInitResult 0x%02X", CS_IN[i]);
    }

    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS llCsLogDrbgInitResult K_drbg, V_drbg");
    for (i = 0; i < CS_CSIV_LEN; i++)
    {
        Log_printf(LogModule_RCL, Log_DEBUG,
                   "DEBUG - STACK CS llCsLogDrbgInitResult 0x%02X, 0x%02X",
                   params->kDrbg[i], params->vDrbg[i]);
    }
#endif
}

/*******************************************************************************
 * Function Description is in ll_cs_logs.h
 */
void llCsLogDrbgParamsAndResult(uint8* prandomBits, drbgParams_t* params)
{
#ifdef CS_LOG_DRBG_RESULTS
    uint8 i = 0;
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS llCsLogDrbgParamsAndResult procedureCounter: "
               "%d, StepCounter: %d",
               params->csProcedureCounter, params->CSStepCounter);
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS llCsLogDrbgParamsAndResult transactionID: %d, "
               "transactionCounter: %d",
               params->TransactionID, params->TransactionCounter);
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS llCsLogDrbgParamsAndResult RandomBits:");
    for (i = 0; i < CS_CSIV_LEN; i++)
    {
        Log_printf(LogModule_RCL, Log_DEBUG,
                   "DEBUG - STACK CS llCsLogDrbgParamsAndResult 0x%02X",
                   prandomBits[i]);
    }
#endif
}

/*******************************************************************************
 * Function Description is in ll_cs_logs.h
 */
void llCsLogChannelIndexArray(uint8* pShuffledArray, uint8 lengthOfarray)
{
#ifdef CS_LOG_CHANNEL_INDEX_ARRAY
    uint8 i = 0;
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS llCsLogChannelIndexArray Shuffled Channel "
               "Index Array:");
    for (i = 0; i < lengthOfarray; i++)
    {
        Log_printf(LogModule_RCL, Log_DEBUG,
                   "DEBUG - STACK CS llCsLogChannelIndexArray %d",
                   pShuffledArray[i]);
    }
#endif
}

/*******************************************************************************
 * Function Description is in ll_cs_logs.h
 */
void llCsLogSelectedAccessAddress(uint32_t* aaRx, uint32_t* aaTx)
{
#ifdef CS_LOG_ACCESS_ADDRESS
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS llCsLogSelectedAccessAddress (aaRx, aaTx) "
               "(0x%04X, 0x%04X)",
               *aaRx, *aaTx);
#endif
}

/*******************************************************************************
 * Function Description is in ll_cs_logs.h
 */
void llCsLogRandomSequence(uint32_t* pTx, uint32_t* pRx, uint8 payloadLen)
{
#ifdef CS_LOG_DRBG_RESULTS
    uint8 i = 0;
    for (i = 0; i < (payloadLen / 32); i++)
    {
        Log_printf(LogModule_RCL, Log_DEBUG,
                   "DEBUG - STACK CS llCsLogRandomSequence payload Tx 0x%04X",
                   pTx[i]);
    }
    for (i = 0; i < (payloadLen / 32); i++)
    {
        Log_printf(LogModule_RCL, Log_DEBUG,
                   "DEBUG - STACK CS llCsLogRandomSequence payload Rx 0x%04X",
                   pRx[i]);
    }
#endif
}

/*******************************************************************************
 * Function Description is in ll_cs_logs.h
 */
void llCsLogStep(RCL_CmdBleCs_Step* csStep, uint8 stepNum)
{
#ifdef CS_LOG_STEPS
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS llCsLogStepList: stepNum:%d", stepNum);
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS llCsLogStepList: stepMode:%d,chIdx:%d",
               csStep->mode, csStep->channelIdx);
    if (csStep->mode == 2 || csStep->mode == 3)
    {
        Log_printf(LogModule_RCL, Log_DEBUG,
                   "DEBUG - STACK CS llCsLogStepList: toneExtensionBit %x",
                   csStep->toneExtension);
    }
    if (csStep->mode == 1 || csStep->mode == 3 || csStep->mode == 0)
    {
        Log_printf(LogModule_RCL, Log_DEBUG,
                   "DEBUG - STACK CS llCsLogStepList: AA RX:%x, AA Tx:%x",
                   csStep->aaRx, csStep->aaTx);
        Log_printf(LogModule_RCL, Log_DEBUG,
                   "DEBUG - STACK CS llCsLogStepList: PL Rx:%x, PL Tx:%x",
                   csStep->payloadRx[0], csStep->payloadTx[0]);
    }
#endif
}

/*******************************************************************************
 * Function Description is in ll_cs_logs.h
 */
void llCsLogStepResults(uint8 csNumSteps, csRclCmdData_t csRclData)
{
#ifdef CS_LOG_STEPS
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS llCsLogStepResults: nStepsDone:%d",
               csRclData.csOutput->nStepsDone);
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS llCsLogStepResults: nRxOk:%d",
               csRclData.csOutput->nRxOk);
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS llCsLogStepResults: nRxNOk:%d",
               csRclData.csOutput->nRxNok);
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS llCsLogStepResults: nResultsRead:%d",
               csRclData.csOutput->nResultsRead);
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS llCsLogStepResults: nStepsWritten:%d",
               csRclData.csOutput->nStepsWritten);
#endif
}

/*******************************************************************************
 * Function Description is in ll_cs_logs.h
 */
void llCsLogResultStats(RCL_CmdBleCs_Stats* stats)
{
#ifdef CS_LOG_STATS
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS llCsSubevent_PostProcess Stats:");
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS stepsDone     %d, resultsRead %d",
               stats->nStepsDone, stats->nResultsRead);
    Log_printf(LogModule_RCL, Log_DEBUG,
               "DEBUG - STACK CS nStepsWritten %d, nRxOk       %d",
               stats->nStepsWritten, stats->nRxOk);
    Log_printf(LogModule_RCL, Log_DEBUG, "DEBUG - STACK CS nRxNok        %d",
               stats->nRxNok);
#endif
}
