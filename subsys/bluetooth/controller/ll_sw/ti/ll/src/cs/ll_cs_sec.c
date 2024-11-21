/******************************************************************************

 @file  ll_cs_sec.c

 @brief Generates the LL CS Vectors.
        Triggers the CS DRBG to generate random bits for the different
        DRBG transactions.

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
#include "cs/ll_cs_sec.h"
#include "ll_csdrbg.h"
#include "map_direct.h"
#include "ll_common.h"

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * CONSTANTS
 */
#define N_CS_RANGE_GEN_RANDOMIZED_BITS 8

/*******************************************************************************
 * EXTERNS
 */

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */
drbgParams_t csDrbgParams = {
    .csProcedureCounter = 0,
    .CSStepCounter = 0,
    .TransactionCounter = 0,
};

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
 * Public function defined in ll_cs_sec.h
 */
uint8 llCsSecGenerateIV(uint8* csIV)
{
    llStatus_t status;
    uint8 tempIV[CS_CSIV_LEN / 2];
    status = MAP_LL_Rand(tempIV, CS_CSIV_LEN / 2);
    if (status != LL_STATUS_SUCCESS)
    {
        return status;
    }
    MAP_osal_memcpy(csIV, tempIV, CS_CSIV_LEN / 2);
    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_sec.h
 */
uint8 llCsSecGenerateIN(uint8* csIN)
{
    llStatus_t status;
    uint8 tempIN[CS_CSIN_LEN / 2];
    status = MAP_LL_Rand(tempIN, CS_CSIN_LEN / 2);
    if (status != LL_STATUS_SUCCESS)
    {
        return status;
    }
    MAP_osal_memcpy(csIN, tempIN, CS_CSIN_LEN / 2);
    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_sec.h
 */
uint8 llCsSecGeneratePV(uint8* csPV)
{
    llStatus_t status;
    uint8 tempPV[CS_CSPV_LEN / 2];
    status = MAP_LL_Rand(tempPV, CS_CSPV_LEN / 2);
    if (status != LL_STATUS_SUCCESS)
    {
        return status;
    }
    MAP_osal_memcpy(csPV, tempPV, CS_CSPV_LEN / 2);
    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_sec.h
 */
csStatus_e llCsProcedureInitDrbg(uint16 connId, uint8 configId)
{
    csStatus_e status = CS_STATUS_SUCCESS;
    csSecVectors_t secVecs;

    MAP_osal_memset(csDrbgParams.kDrbg, 0, 16);
    MAP_osal_memset(csDrbgParams.vDrbg, 0, 16);

    llCsDbGetSecurityVectors(connId, &secVecs);
    csDrbgParams.csProcedureCounter = 0;
    csDrbgParams.CSStepCounter = 0;
    csDrbgParams.TransactionCounter = 0;

    if (!llCsSecIsTestMode())
    {
        status = (csStatus_e)LL_CSDRBG_Init(secVecs.CSIV,
                                            secVecs.CSIN,
                                            secVecs.CSPV,
                                            &csDrbgParams);
    }
    return status;
}

/*******************************************************************************
 * Public function defined in ll_cs_sec.h
 */
void llCsSecIncreaseStepCount(void)
{
    csDrbgParams.CSStepCounter++;
    csDrbgParams.TransactionCounter = 0;
}

/*******************************************************************************
 * Public function defined in ll_cs_sec.h
 */
uint16 llCsSecGetStepCount(void)
{
    return csDrbgParams.CSStepCounter;
}

/*******************************************************************************
 * Public function defined in ll_cs_sec.h
 */
uint16 llCsSecGetProcedureCount(void)
{
    return csDrbgParams.csProcedureCounter;
}

/*******************************************************************************
 * Public function defined in ll_cs_sec.h
 */
void llCsSecSetProcedureCount(uint16 procCnt)
{
    csDrbgParams.csProcedureCounter = procCnt;
}

/*******************************************************************************
 * Public function defined in ll_cs_sec.h
 */
void llCsSecResetStepCount(void)
{
    csDrbgParams.CSStepCounter = 0;
    csDrbgParams.TransactionCounter = 0;
}

/*******************************************************************************
 * Public function defined in ll_cs_sec.h
 */
void llCsSecIncProcCounter(void)
{
    /* Increment Procedure Counter */
    csDrbgParams.csProcedureCounter++;
    /* Reset the transaction counter */
    csDrbgParams.TransactionCounter = 0;
    /* Set the transcation ID to BackTracking Resistence */
    csDrbgParams.TransactionID = CS_TID_RANDOM_BACKTRACKING_RESISTENCE;
    /* run DRBG backtracking resistence to generate new K and V values */
    LL_CSDRBG_BackTracking(&csDrbgParams);
    /* Reset DRBG cache to generate new bits for the new procedure */
    llCsDbResetDRBGCache();
}

/*******************************************************************************
 * Public function defined in ll_cs_sec.h
 */
uint8 hr1(uint8 r, csTransactionId_e tId)
{
    uint8 rndBits;
    uint16 tRand;
    uint8 rOut;
    if (r == 1)
    {
        return 0;
    }

    csDrbg(N_CS_RANGE_GEN_RANDOMIZED_BITS, &rndBits, tId);
    tRand = r * rndBits;

    if ((tRand & CS_1_BYTE_MASK) < (CS_MAX_STEPS_PER_PROCEDURE % r))
    {
        csDrbg(N_CS_RANGE_GEN_RANDOMIZED_BITS, &rndBits, tId);
        rOut = (((rndBits << CS_8_BITS_SIZE) * r) + tRand) >> CS_RNDM_SIZE;
    }
    else
    {
        rOut = tRand >> CS_8_BITS_SIZE;
    }
    return rOut;
}

/*******************************************************************************
 * Public function defined in ll_cs_sec.h
 */
void cr1(uint8* pChannelArray, uint8* filterdArr, uint8 nChannels, csTransactionId_e trId)
{
    uint8* shuffledChannelArray = pChannelArray;
    uint8 i, j;
    uint8 tempVal;

    if (pChannelArray && filterdArr)
    {
        MAP_osal_memcpy(shuffledChannelArray, filterdArr, nChannels);

        for (i = 0; i < nChannels; i++)
        {
            j = hr1(i + 1, trId);
            tempVal = shuffledChannelArray[i];
            if (i != j)
            {
                shuffledChannelArray[i] = shuffledChannelArray[j];
            }
            shuffledChannelArray[j] = tempVal;
        }
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_sec.h
 */
void csDrbg(uint8 numBitsRequired, uint8* pRndBits, csTransactionId_e transactionId)
{
    if (llCsDbRandomBitsAvailable(transactionId, numBitsRequired) == FALSE)
    {
        uint8 randomBits[CS_RNDM_SIZE] ALIGNED = {0};
        if (transactionId != csDrbgParams.TransactionID)
        {
            csDrbgParams.TransactionID = transactionId;
            csDrbgParams.TransactionCounter = 0U;
        }

        /* Get Random Bits from DRBG */
        VOID LL_CSDRBG_GetDrbg(randomBits, &csDrbgParams);
        csDrbgParams.TransactionCounter++;

        /* Set Random Bits in DB */
        llCsDbSetRandomBitsCache(transactionId, randomBits);
    }

    llCsDbGetRandomBitsFromCache(transactionId, numBitsRequired, pRndBits);
}

/*******************************************************************************
 * Public function defined in ll_cs_sec.h
 */
uint8 llCsSecIsTestMode(void)
{
    if(llCsDbGetTestMode() == CS_TEST_MODE_ENABLE)
    {
        csTestOverrideData_t pCsTestOverrideData;

        llCsDbGetTestOverrideData(&pCsTestOverrideData);
        csDrbgParams.vDrbg[14] = LO_UINT16(pCsTestOverrideData.drbgNonce);
        csDrbgParams.vDrbg[15] = HI_UINT16(pCsTestOverrideData.drbgNonce);
        return TRUE;
    }
    return FALSE;
}
