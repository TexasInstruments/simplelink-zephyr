/******************************************************************************

 @file  ll_cs_common.c

 @brief This file includes some helper functions that are commonly used by
        multiple cs modules.

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

#include "cs/ll_cs_common.h"
#include "cs/ll_cs_db.h"
#include "ll_csdrbg.h"

#include "ll.h"

#include "rom_jt.h"

/*******************************************************************************
 * CONSTANTS
 */

/*******************************************************************************
 * MACROS
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

/*******************************************************************************
 * FUNCTIONS
 */

/*******************************************************************************
 * Public function defined in ll_cs_common.h
 */
csStatus_e llCsCheckConnection(llConnState_t* connPtr)
{
    llStatus_t status;

    // make sure connection ID is valid
    if ((status = MAP_LL_ConnActive(connPtr->connId)) != LL_STATUS_SUCCESS)
    {
        return (csStatus_e)(status);
    }

    // check if CS feature is supported for this connection
    if (!(connPtr->featureSetInfo.featureSet[5] & LL_FEATURE_CS))
    {
        return (CS_STATUS_FEATURE_NOT_SUPPORTED);
    }

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_common.h
 */
csStatus_e llCsConfigurationCheck(uint16 connId, void* pBuf)
{
    // check if configuration set complies with local and remote capabilities.
    csConfigurationSet_t* pConfig = (csConfigurationSet_t*)pBuf;
    csCapabilities_t ownCapabilities;
    csCapabilities_t peerCapabilities;

    llCsDbGetLocalCapabilities(&ownCapabilities);
    llCsDbGetPeerCapabilities(connId, &peerCapabilities);

    // First check if disallowed channels 0-1, 23-25, 77-78 are used
    if (CS_TEST_CHANNEL_MAP_RESTRICTED(pConfig->channelMap.channelMap))
    {
        return (CS_STATUS_UNEXPECTED_PARAMETER);
    }

    // Check minimal amount of channels
    if (llCsNumOnBit(pConfig->channelMap.channelMap, CS_CHM_SIZE) <
        CS_MIN_NUM_OF_CHN)
    {
        return (CS_STATUS_UNEXPECTED_PARAMETER);
    }

    /* Validate mode parameter input */
    if ((pConfig->mainMode != CS_MODE_1) && (pConfig->mainMode != CS_MODE_2) &&
        (pConfig->mainMode != CS_MODE_3))
    {
        return (CS_STATUS_UNEXPECTED_PARAMETER);
    }
    if ((pConfig->subMode != CS_MODE_1) && (pConfig->subMode != CS_MODE_2) &&
        (pConfig->subMode != CS_MODE_3) && (pConfig->subMode != CS_MODE_UNUSED))
    {
        return (CS_STATUS_UNEXPECTED_PARAMETER);
    }

    /* Check mode 3 support, if used */
    if ((pConfig->mainMode == CS_MODE_3) || (pConfig->subMode == CS_MODE_3))
    {
        if ((ownCapabilities.optionalModes != 1) ||
            (peerCapabilities.optionalModes != 1))
        {
            return (CS_STATUS_FEATURE_NOT_SUPPORTED);
        }
    }

    // check CS sync PHy support
    if ((pConfig->csSyncPhy != CS_LE_1M_SYNC_PHY) &&
        (pConfig->csSyncPhy != CS_LE_2M_SYNC_PHY))
    {
        return (CS_STATUS_FEATURE_NOT_SUPPORTED);
    }

    if (pConfig->csSyncPhy == CS_LE_2M_SYNC_PHY)
    {
        if ((ownCapabilities.optionalCsSyncPhy != CS_LE_2M_PHY_SUPPORTED) ||
            (peerCapabilities.optionalCsSyncPhy != CS_LE_2M_PHY_SUPPORTED))
        {
            return (CS_STATUS_FEATURE_NOT_SUPPORTED);
        }
    }

    // check RTT type support
    if (!(((pConfig->rttType == 0) && ownCapabilities.rttAAOnlyN &&
           peerCapabilities.rttAAOnlyN) ||
          ((pConfig->rttType >= 1 && pConfig->rttType <= 2) &&
           ownCapabilities.rttSoundingN && peerCapabilities.rttSoundingN) ||
          ((pConfig->rttType >= 3 && pConfig->rttType <= 6) &&
           ownCapabilities.rttRandomPayloadN &&
           peerCapabilities.rttRandomPayloadN)))
    {
        return (CS_STATUS_FEATURE_NOT_SUPPORTED);
    }
    if (!(((pConfig->role == CS_ROLE_INITIATOR) &&
           (ownCapabilities.role & CS_INITIATOR_MASK) &&
           (peerCapabilities.role & CS_REFLECTOR_MASK)) ||
          ((pConfig->role == CS_ROLE_REFLECTOR) &&
           (ownCapabilities.role & CS_REFLECTOR_MASK) &&
           (peerCapabilities.role & CS_INITIATOR_MASK))))
    {
        return (CS_STATUS_FEATURE_NOT_SUPPORTED);
    }

    // check companion signal support
    if ((pConfig->companionSignal == CS_ENABLE) &&
        (peerCapabilities.companionSignal == CS_DISABLE))
    {
        return (CS_STATUS_FEATURE_NOT_SUPPORTED);
    }
    if ((pConfig->chSel == CS_CHANNLE_SELECTION_ALG_3C) &&
        ((peerCapabilities.chSel3c != CS_CHANNLE_SELECTION_ALG_3C) ||
         (ownCapabilities.chSel3c != CS_CHANNLE_SELECTION_ALG_3C)))
    {
        return (CS_STATUS_FEATURE_NOT_SUPPORTED);
    }

    // check TIP1 support
    if ((pConfig->tIP1 != CS_MANDATORY_TIP_IDX) &&
        !(CS_GET_BIT(ownCapabilities.tIp1Cap, pConfig->tIP1) &&
          CS_GET_BIT(peerCapabilities.tIp1Cap, pConfig->tIP1)))
    {
        return (CS_STATUS_FEATURE_NOT_SUPPORTED);
    }

    // Check TIP2 support
    if ((pConfig->tIP2 != CS_MANDATORY_TIP_IDX) &&
        !(CS_GET_BIT(ownCapabilities.tIp2Cap, pConfig->tIP2) &&
          CS_GET_BIT(peerCapabilities.tIp2Cap, pConfig->tIP2)))
    {
        return (CS_STATUS_FEATURE_NOT_SUPPORTED);
    }

    // check T FCS support
    if ((pConfig->tFCs != CS_MANDATORY_TFCS_IDX) &&
        !(CS_GET_BIT(ownCapabilities.tFcsCap, pConfig->tFCs) &&
          CS_GET_BIT(peerCapabilities.tFcsCap, pConfig->tFCs)))
    {
        return (CS_STATUS_FEATURE_NOT_SUPPORTED);
    }

    // Check T PM support
    if ((pConfig->tPM != CS_MANDATORY_TPM_IDX) &&
        !(CS_GET_BIT(ownCapabilities.tPmCsap, pConfig->tPM) &&
          CS_GET_BIT(peerCapabilities.tPmCsap, pConfig->tPM)))
    {
        return (CS_STATUS_FEATURE_NOT_SUPPORTED);
    }

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_common.h
 */
csStatus_e llCsCheckACI(csACI_e ACI, uint8 configId, uint16 connId)
{
    csCapabilities_t own, peer;
    uint8* aciTable;
    uint8 role;
    uint8 initNAP, refNAP;

    llCsDbGetLocalCapabilities(&own);
    llCsDbGetPeerCapabilities(connId, &peer);
    role = llCsDbGetConfigRole(connId, configId);

    aciTable = llCsDbGetAciTable(ACI);
    initNAP = aciTable[0];
    refNAP = aciTable[1];

    if (role == CS_ROLE_INITIATOR)
    {
        if ((own.numAntennas < initNAP) || (peer.numAntennas < refNAP))
        {
            return CS_STATUS_UNEXPECTED_PARAMETER;
        }
    }
    else
    {
        if ((own.numAntennas < refNAP) || (peer.numAntennas < initNAP))
        {
            return CS_STATUS_UNEXPECTED_PARAMETER;
        }
    }
    return CS_STATUS_SUCCESS;
}

/*******************************************************************************
 * Public function defined in ll_cs_common.h
 */
csStatus_e llCsConfigIdSafeToUse(uint16 connId, uint8 configId)
{
    csProcedureParams_t procedureParams;

    // check if configId is valid (0...4)
    if (configId > CS_MAX_NUM_CONFIG_IDS)
    {
        return (CS_STATUS_UNEXPECTED_PARAMETER);
    }

    // check if config Id was enabled by LL_CS_ProcedureEnable
    llCsDbGetProcedureParams(connId, configId, &procedureParams);
    if (procedureParams.enable == CS_ENABLE)
    {
        return (CS_STATUS_CONFIG_ENABLED);
    }

    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_common.h
 */
csStatus_e llCsConfigIdCheck(uint16 connId, uint8 configId)
{
    /* First check if configID is in range [0-3] */
    if (configId > CS_MAX_NUM_CONFIG_IDS)
    {
        return (CS_STATUS_UNEXPECTED_PARAMETER);
    }

    if (llCsDbGetConfigState(connId, configId) == CS_DISABLE)
    {
        return (CS_STATUS_DISABLED_CONFIG_ID);
    }
    return (CS_STATUS_SUCCESS);
}

/*******************************************************************************
 * Public function defined in ll_cs_common.h.
 */
uint8 llCsNumOnBit(uint8* pBitMapArray, uint8 amountOfBytesInMapArray)
{
    uint8 chCount = 0;
    uint8 i, j;
    for (i = 0; i < amountOfBytesInMapArray; i++)
    {
        uint8 hexDigit = pBitMapArray[i];
        // Count the number of bits that are on
        for (j = 0; j < BITS_PER_BYTE; j++)
        {
            chCount += (hexDigit >> j) & 0x01;
        }
    }
    return chCount;
}
