/******************************************************************************

 @file  ll_csdrbg.h

 @brief This file contains the CS DRBG constants, typedefs and externs.

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
#ifndef LL_CSDRBG_H
#define LL_CSDRBG_H
#include "ll_al.h"
#include "ll_common.h"
#include "ll_config.h"
#include <ti/drivers/rcl/RCL.h>
#include <ti/drivers/rcl/commands/ble5.h>

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */
/*******************************************************************************
 * TYPEDEFS
 */
typedef struct drbgParams_t
{
    uint8 kDrbg[16]; // kDrbg initailzed to zero. updated evrey cs procedure
    uint8 vDrbg[16]; // vDrbg initailzed to zero. updated evrey cs procedure
    uint8 csProcedureCounter; // vDrbg initailzed to zero. updated evrey cs
                              // procedure
    uint16 CSStepCounter;     // 16 bits - CS Step Counter = (VDRBG[31:16] +
                              // CSStepCount) mod 2^16
    uint8 TransactionID; //   8 bits - CS TransactionIdentifier = (VDRBG[15:8] +
                         //   CSpTransactionID) mod 2^8
    uint8 TransactionCounter; // 8 bits - (VDRBG[7:0] + CSpTransactionCounter)
                              // mod 2^8
} drbgParams_t;

/*******************************************************************************
 * LOCAL VARIABLES
 */

/*******************************************************************************
 * GLOBAL VARIABLES
 */

/*******************************************************************************
 * APIs
 */
/****************************************************************************
 * @fn LL_CSDRBG_Init
 *
 * @brief the function initalize the first kDrbg, vDrbg from CS_IV, CS_IN,
 * CS_PV.
 *
 * input parameters
 * @param CS_IV 128 bits result of the CS Security Start procedure.
 *
 * @param CS_IN 64 bits result of the CS Security Start procedure.
 *
 * @param CS_PV 128 bits result of the CS Security Start procedure.
 *
 * @param params - pointer to drbgParams_t. see typedef
 *
 * output parameters
 * @param params - params -> kDrbg, params -> vDrbg are initialized.
 *
 * @return LL_STATUS_ERROR_BAD_PARAMETER if one of the pointers is null or
 * LL_STATUS_SUCCESS.
 */
llStatus_t LL_CSDRBG_Init(uint8* CS_IV, uint8* CS_IN, uint8* CS_PV,
                          drbgParams_t* params);

/****************************************************************************
 * @fn LL_CSDRBG_GetDrbg
 *
 * @brief the function create a new DRBG
 *
 *  input parameters
 *
 * @param prandomBits - pointer to the 128 bits block.
 *
 * @param params - pointer to drbgParams_t. see typedef
 *
 * output parameters:
 *
 * @param prandomBits 128 bits of random numbers
 *
 * @return LL_STATUS_ERROR_BAD_PARAMETER if one of the pointers is null or
 * LL_STATUS_SUCCESS.
 */
llStatus_t LL_CSDRBG_GetDrbg(uint8* prandomBits, drbgParams_t* params);

/****************************************************************************
 * @fn LL_CSDRBG_BackTracking
 *
 * @brief Backtracking resistance shall be invoked every time the CSProcCount is
 * incremented. the backtracting uses f9() to update kDrbg, vDrbg by taking SM =
 * {0}, kDrbg and vDrbg[31:16] + CSStepCount + 1. (CSStepCount from last proc.)
 * and cs proc. counter increses by 1.
 *
 * input parameters
 * @param params - pointer to drbgParams_t. see typedef
 *
 * output parameters
 * @param params - params -> kDrbg, params -> vDrbg are updated. cs proc.
 * counter + 1.
 *
 * @return
 */
llStatus_t LL_CSDRBG_BackTracking(drbgParams_t* params);

/****************************************************************************
 * @fn csDrbgSelfTest
 *
 * @brief known answer tset - tests the CS deterministic random bit generator
 * with sample data from the spec.
 *
 * @return TRUE if all the test for several K, V to generate randombits and
 * several backtaring test all passed. FALSE otherwise.
 */
uint8 csDrbgSelfTest();

#endif