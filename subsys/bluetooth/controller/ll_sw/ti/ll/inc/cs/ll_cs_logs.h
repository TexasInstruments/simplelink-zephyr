/******************************************************************************

 @file  ll_cs_logs.h

 @brief Channel Sounding common header

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2023 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef LL_CS_LOGS_H
#define LL_CS_LOGS_H
/*******************************************************************************
 * INCLUDES
 */
#include "bcomdef.h"
#include "cs/ll_cs_common.h"
#include "cs/ll_cs_rcl.h"
#include "ll_csdrbg.h"

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
 * @fn          llCsLogDrbgInitResults
 *
 * @brief       Log the K and V that are a result of CS DRBG init.
 * This will also log the Security Vectors since they affect the final Result.
 * This log requires the flag LOG_DRBG_RESULTS.
 *
 * input parameters
 *
 * @param       CS_IV - pointer to CS IV vector
 * @param       CS_IN - pointer to CS IN vector
 * @param       CS_PV - pointer to CS PV vector
 * @param       params - pointer of drbg params
 *
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsLogDrbgInitResult(uint8* CS_IV, uint8* CS_IN, uint8* CS_PV,
                           drbgParams_t* params);

/*******************************************************************************
 * @fn          llCsLogDrbgParamsAndResults
 *
 * @brief       This will log the CS procedureCounter, Step Counter,
 * TransactionID and transactionCounter, and the Random Bits that were
 * generated as a result of calling getCSDrbg.
 * This requires the flag LOG_DRBG_RESULTS
 *
 * input parameters
 *
 * @param       prandomBits - pointer to randomBits
 * @param       params - pointer to drbg params
 *
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsLogDrbgParamsAndResult(uint8* prandomBits, drbgParams_t* params);

/*******************************************************************************
 * @fn          llCsLogChannelIndexArray
 *
 * @brief       Logs the shuffled channel index array, requires flag
 *              LOG_CS_CHANNEL_INDEX_ARRAY
 *
 * input parameters
 *
 * @param       pShuffledArray - pointer to shuffled chanIndex array
 * @param       lengthOfArray - length of the array
 *
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsLogChannelIndexArray(uint8* pShuffledArray, uint8 lengthOfarray);

/*******************************************************************************
 * @fn          llCsLogSelectedAccessAddress
 *
 * @brief       Log the selected Access Address
 *              Requires flag LOG_CS_ACCESS_ADDRESS
 *
 * input parameters
 *
 * @param       aaRx - pointer RX access Address
 * @param       aaTx - pointer to TX access Address
 *
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsLogSelectedAccessAddress(uint32_t* aaRx, uint32_t* aaTx);

/*******************************************************************************
 * @fn          llCsLogRandomSequence
 *
 * @brief       Logs the random sequence payload
 *
 * input parameters
 *
 * @param       pTx - pointer to Tx payload
 * @param       pTx - pointer to RX payload
 * @param       payloadLen - length of the payload
 *
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsLogRandomSequence(uint32_t* pTx, uint32_t* pRx, uint8 payloadLen);

/*******************************************************************************
 * @fn          llCsLogStep
 *
 * @brief       Log the step details includes: stepNum, stepmode, chanIdx
 *              Tone Extension Bit, AA Rx/Tx, Payload Rx/Tx
 *
 * input parameters
 *
 * @param       csStep - pointer to CS steps
 * @param       stepNum - number of step
 *
 * output parameters
 *
 * @param       None
 *
 * @return      None
 */
void llCsLogStep(RCL_CmdBleCs_Step* csStep, uint8 stepNum);

/*******************************************************************************
 * @fn          lCsLogResultStats
 *
 * @brief       Log CS result stats
 *
 * input parameters
 *
 * @param       stats - cs stats
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsLogResultStats(RCL_CmdBleCs_Stats* stats);

/*******************************************************************************
 * @fn          llCsLogStepResults
 *
 * @brief       Log step results
 *
 * input parameters
 *
 * @param       csNumSteps - number of steps to log
 * @param       csRclData - RCL data includes the step results
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      None
 */
void llCsLogStepResults(uint8 csNumSteps, csRclCmdData_t csRclData);
#endif
