/******************************************************************************

 @file  ll_cs_test.c

 @brief CS Test Mode header file.

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
#include "cs/ll_cs_test.h"
#include "cs/ll_cs_db.h"
#include "cs/ll_cs_procedure.h"
#include "map_direct.h"

/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * EXTERNS
 */

/*******************************************************************************
 * TYPEDEFS
 */

/*******************************************************************************
 * LOCAL VARIABLES
 */

// CS Test Possible Payloads
// Note: payloads at index 0 and 3 are PRBS payloads and are currently not used since they may be removed from the SPEC
const uint8 csTestPayloads[CS_TEST_NUM_PAYLOADS][CS_RNDM_SIZE] =
{
    /* PRBS9 sequence */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F},
    {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55},
    /* PRBS15 sequence */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0},
    {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA},
};

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
 * Public function defined in ll_cs_test.h
 */
csStatus_e llCsCheckTestParams(csTestParams_t* pParams)
{
    if (pParams->mainMode > 3U)
    {
        /* Main Mode must either be 0 - 3 */
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if (pParams->subMode != 0xFFU)
    {
        /* Currently submodes are not supported */
        return CS_STATUS_FEATURE_NOT_SUPPORTED;
    }
    if (pParams->mainModeRep != 0U)
    {
        /* Currently not supported */
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if ( (pParams->role != CS_ROLE_INITIATOR) &&
         (pParams->role != CS_ROLE_REFLECTOR) )
    {
        /* role can either be Initiator or Reflector */
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if ( (pParams->rttType == 0x01U) ||
         (pParams->rttType == 0x02U) ||
         (pParams->rttType >  0x06U) )
    {
        /* Rtt Type can be one of the following: 0,3,4,5,6 */
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if ( (pParams->csSyncPhy < 0x01U) ||
         (pParams->csSyncPhy > 0x03U) )
    {
        /* CS Sync PHY can be one of the following: 1,2,3*/
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if ( (pParams->csSyncAntSel < 0x01U) ||
         (pParams->csSyncAntSel > 0x04U) )
    {
        /* Accepted values are 1-4 */
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if ((pParams->subeventLen < CS_DEFAULT_MIN_SUBEVENT_LEN) ||
        (pParams->subeventLen > CS_DEFAULT_MAX_SUBEVENT_LEN))
    {
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if (pParams->maxNumSubevents > 0x20U)
    {
        // in this case this value should be ignored!
        /* Accepted Values are between 0 and 0x20 */
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
     if ( ((pParams->tpl != 0x7E) && (pParams->tpl != 0x7F))
        && ((pParams->tpl < -127)  || (pParams->tpl > 20)) )
    {
        /* Accepted values are either: 0x7E, 0x7F or between -127 and 20 */
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if ((pParams->tIp1 != 0x0AU) && (pParams->tIp1 != 0x14U) &&
        (pParams->tIp1 != 0x1EU) && (pParams->tIp1 != 0x28U) &&
        (pParams->tIp1 != 0x32U) && (pParams->tIp1 != 0x3CU) &&
        (pParams->tIp1 != 0x50U) && (pParams->tIp1 != 0x91U) )
    {
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if ((pParams->tIp2 != 0x0AU) && (pParams->tIp2 != 0x14U) &&
        (pParams->tIp2 != 0x1EU) && (pParams->tIp2 != 0x28U) &&
        (pParams->tIp2 != 0x32U) && (pParams->tIp2 != 0x3CU) &&
        (pParams->tIp2 != 0x50U) && (pParams->tIp2 != 0x91U) )
    {
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if ((pParams->tFcs != 0x0FU) && (pParams->tFcs != 0x14U) &&
        (pParams->tFcs != 0x64U) && (pParams->tFcs != 0x28U) &&
        (pParams->tFcs != 0x32U) && (pParams->tFcs != 0x3CU) &&
        (pParams->tFcs != 0x50U) && (pParams->tFcs != 0x96U) &&
        (pParams->tFcs != 0x78U))
    {
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if ((pParams->tPm != 0x0AU) && (pParams->tPm != 0x14U) &&
        (pParams->tPm != 0x28U) )
    {
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if ((pParams->tSw != 0x00U) && (pParams->tSw != 0x01U) &&
        (pParams->tSw != 0x02U) && (pParams->tSw != 0x0AU) )
    {
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if (pParams->toneAntCfg > 0x07U)
    {
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if ( (pParams->snrCtrlInit != 0xFFU) ||
         (pParams->snrCtrlRef  != 0xFFU) )
    {
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if (pParams->chmRep == 0x00U)
    {
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if ( pParams->overrideCfg.ssMarkerPosition   != 0U ||
         pParams->overrideCfg.ssMarkerValue      != 0U )
    {
        return CS_STATUS_FEATURE_NOT_SUPPORTED;
    }

    /* Check the override Len */
    uint8 expectedLen = 0;
    if (pParams->overrideCfg.chanCfg == 1U)
    {
        uint8 chanSize = *pParams->overrideParams;
        expectedLen = (expectedLen + 1U + chanSize) & 0xFFU;
    }
    else
    {
        expectedLen = (expectedLen + CS_CHM_SIZE + 3U) & 0xFFU;
    }
    if (pParams->overrideCfg.numMainModeSteps == 1U)
    {
        expectedLen = (expectedLen + 1U) & 0xFFU;
    }
    if (pParams->overrideCfg.toneExtension == 1U)
    {
        expectedLen = (expectedLen + 1U) & 0xFFU;
    }
    if (pParams->overrideCfg.antennaPermutation == 1U)
    {
        expectedLen = (expectedLen + 1U) & 0xFFU;
    }
    if (pParams->overrideCfg.aa == 1U)
    {
        expectedLen = (expectedLen + 8U) & 0xFFU;
    }
    if (pParams->overrideCfg.payload == 1U)
    {
        expectedLen = (expectedLen + 17U) & 0xFFU;
    }
    if (pParams->overrideLen != expectedLen)
    {
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    if (pParams->overrideParams == NULL)
    {
        return CS_STATUS_UNEXPECTED_PARAMETER;
    }
    return CS_STATUS_SUCCESS;
}

/*******************************************************************************
 * Public function defined in ll_cs_test.h
 */
void llCsTestGetPayload(uint8 plPtrn, uint32_t* ptr)
{
    if (ptr != NULL)
    {
        (void)MAP_osal_memcpy((uint8*)csTestPayloads[plPtrn],(uint8*)ptr, CS_RNDM_SIZE);
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_test.h
 */
uint8 llCsInitChanIdxArrOverride(uint8 configId, uint16 connId, csConfigurationSet_t* csConfig)
{
    csStatus_e status;
    if (llCsDbGetChanOverrideCfg())
    {
        // If channel override is used then it is already initialized
        // So nothing should be done here
        status = CS_STATUS_SUCCESS;
    }
    else
    {
        // Channel override is not used, so we need to initialize the channel map as usual
        status = llCsInitChanIdxArr(configId, connId, csConfig);
    }
    return status;

}

/*******************************************************************************
 * Public function defined in ll_cs_test.h
 */
uint8 llCsSelectStepChanOverride(uint8 stepMode, uint16 connId, uint8*config)
{
    uint8 stepChan = INVALID_CS_CHANNEL_IDX;
    // Check if Channel Override is used
    if (llCsDbGetChanOverrideCfg())
    {
        // Get Channel Index from Channel Overrides
        // This function may return INVALID_CS_CHAN_INDEX in case of an issue
        stepChan = llCsDbGetChannelIndex(connId, ((csConfigurationSet_t*)config)->configId, stepMode);
    }
    else
    {
        // Get Channel Index from the regular flow
        // This function may return INVALID_CS_CHAN_INDEX in case of an issue
        stepChan = llCsSelectStepChannel(stepMode, connId, FALSE, (csConfigurationSet_t*)config);
    }
    return stepChan;
}

/*******************************************************************************
 * Public function defined in ll_cs_test.h
 */
void llCsSelectAAOverride(uint8 csRole, uint32_t* aaRx, uint32_t* aaTx)
{
    csOverrideCfg_t overrideCfg;
    // Get override config from the DB and check if AA overrides is used
    if (llCsDbGetOverrideCfg(&overrideCfg) && overrideCfg.aa)
    {
        // Use AA overrides
        llCsAAOverride(csRole, aaRx, aaTx);
    }
    else
    {
        // Use regular AA selection rules
        llCsSelectAA(csRole, aaRx, aaTx);
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_test.h
 */
uint8 llCsAAOverride(uint8 csRole, uint32_t* aaRx, uint32_t* aaTx)
{
    if (aaRx && aaTx)
    {
        // Get AA override from DB
        if (csRole == CS_ROLE_INITIATOR)
        {
            llCsDbGetAAOverride(aaTx, aaRx);
        }
        else
        {
            llCsDbGetAAOverride(aaRx, aaTx);
        }
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_test.h
 */
void llCsGetRandomSequenceOveride(uint8 csRole, uint32_t* pTx, uint32_t* pRx, uint8 plLen)
{
    csOverrideCfg_t csOverrideCfg;
    // Get Override config from the DB ad check if payload override is used
    if (llCsDbGetOverrideCfg(&csOverrideCfg) && csOverrideCfg.payload)
    {
        // Use payload override
        llCsDbGetPayloadOverride(pTx, pRx);
    }
    else
    {
        // Use regular payload selection
        llCsGetRandomSequence(csRole, pTx, pRx, plLen);
    }
}

/*******************************************************************************
 * Public function defined in ll_cs_internal.h
 */
uint8 llCsPayloadOverride(uint8 csRole, uint32_t* pTx, uint32_t* pRx)
{
    uint8 plPattern = 0;
    if (pTx && pRx)
    {
        // Get override payload from DB
        // This will also return the payload pattern
        plPattern = llCsDbGetPayloadOverride(pTx, pRx);

    }
    return plPattern;
}

/*******************************************************************************
 * Public function defined in ll_cs_test.h
 */
uint8 llCsGetToneExtentionOverride(void)
{
    uint8 toneExt = 0;
    csOverrideCfg_t csOverrideCfg;
    // Get Override Config from the DB and check if toneExtension override is used
    if (llCsDbGetOverrideCfg(&csOverrideCfg) && csOverrideCfg.toneExtension)
    {
        // Use Tone Extension Override
        toneExt = llCsToneExtentionOverride();
    }
    else
    {
        // Use regular Tone Extension
        toneExt = llCsGetToneExtention();
    }
    return toneExt;
}

/*******************************************************************************
 * Public function defined in ll_cs_test.h
 */
uint8 llCsToneExtentionOverride(void)
{
    // Use Tone Extension Override
    uint8 toneExt = llCsDbGetOverrideToneExt();
    if (toneExt & 0x04U)
    {
        // If bit 2 is on, we need to loop over the values 0,1,2
        // Take only the LSB
        toneExt = toneExt & 0x03U;
        // Need to loop, set next override tone extension
        llCsDbSetNextOverrideToneExt(toneExt);
    }
    /* Consider only 2 bits and reverse */
    toneExt = CS_REVERSE_TONE_EXTENSION_BITS(toneExt & 0x03U);
    return toneExt;
}
