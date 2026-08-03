/*
 * Copyright (c) 2015-2022, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef PHYSETTINGS_TRX_MDR_H
#define PHYSETTINGS_TRX_MDR_H

/* Include Files */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <ti_cc140xp/ti_cc140xp.h>
/* included via ti_cc140xp/ti_cc140xp.h */
#include "PhySettings_trx.h"

/* Relative delay between end of MDR mode switch packet
  * on base PHY and start of payload packet on new PHY
  */
#define WISUN_MDR_TX_SETTLING_DELAY 1000
#define WISUN_FREQ_INVALID 0xFFFFFFFF

typedef enum
{
    PhySettings_Trx_MDRPHY_0,
    PhySettings_Trx_MDRPHY_1,
    PhySettings_Trx_MDRPHY_2,
    PhySettings_Trx_MDRPHY_3,
    PhySettings_Trx_MDRPHY_4,
    PhySettings_Trx_MDRPHY_5,
    PhySettings_Trx_MDRPHY_6,
    PhySettings_Trx_MDRPHY_7,
    Num_MdrPhy_Settings,
} PhySettings_Trx_mdrPhyIndex;

typedef enum
{
    PhySettings_Trx_Region_Disabled = 0U,
    PhySettings_Trx_Region_NA       = 1U,
    PhySettings_Trx_Region_JP       = 2U,
    PhySettings_Trx_Region_EU       = 3U,
    Num_Phy_Regions,
} PhySettings_Trx_region;

typedef struct PhySettings_MdrRfEntry_t{
    PhySettings_Trx_mdrPhyIndex PhySettings_phyIndex;
    TRX_RadioCommand_Modem modem;
    Header_Settings header;
    uint16_t optionMask;
    char PhySettings_phyName[100];
    PhySettings_testType  PhySettings_testType;
    bool loadDeltaTable;
    uint16_t minChannel;
    uint16_t maxChannel;
    uint32_t csSettlingTimeUs;
    uint32_t csSamplePeriodUs;
    uint32_t csTimeoutUsPerRegion[Num_Phy_Regions];
    int8_t csRSSIOverridePerRegion[Num_Phy_Regions];
    uint8_t csWindowSamplesPerRegion[Num_Phy_Regions];
    uint8_t csCountSamplesPerRegion[Num_Phy_Regions];
    TRX_CarrierSenseExitCondition csExitCondPerRegion[Num_Phy_Regions];
}PhySettings_Trx_MdrRfEntry;

typedef struct PhySettings_Trx_MdrRfSetting_t{
    uint8_t *rfConfig;
    TRX_WisunMdrMappingTable *mappingTable;
    PhySettings_Trx_MdrRfEntry phys[Num_MdrPhy_Settings];
}PhySettings_Trx_MdrRfSetting;

/**
 * Returns the channel plan ID based on the channel plan ID Index
 *
 * @param[in] channelPlanIdIdx Current channel plan ID Index
 *
 * @return channel plan ID based on the channel plan ID Index
 */
typedef int16_t (*MDR_Delta_GetChannelPlanId)(uint8_t channelPlanIdIdx);

/**
 * Returns the channel based on frequency depending on the channel plan
 *
 * @param[in] channelPlanId Current channel plan ID
 * @param[in] frequency Current frequency to be converted
 *
 * @return channel converted from input frequency
 */
typedef int16_t (*MDR_Delta_GetChannelFromFreq)(uint16_t channelPlanId, uint32_t frequency);

/**
 * Returns the delta table associated with the channel plan
 *
 * @param[in] channelPlanId Current channel plan ID
 *
 * @return delta table associated with the input channel plan
 */
typedef const uint32_t *(*MDR_Delta_GetDeltaTable)(uint16_t channelPlanId);

/**
 * Returns the base index for the input channel and channel plan
 *
 * @param[in] channelPlanId Current channel plan ID
 * @param[in] channel Current channel
 *
 * @return base index for the input channel and channel plan
 */
typedef int16_t (*MDR_Delta_GetBaseIndex)(uint16_t channelPlanId, uint16_t channel);

/**
 * Returns the frequency converted from channel depending on the channel plan
 *
 * @param[in] channelPlanId Current channel plan ID
 * @param[in] channel Current channel
 *
 * @return frequency converted from channel depending on the channel plan
 */
typedef uint32_t (*MDR_Delta_GetFreqFromChan)(uint16_t channelPlanId, int16_t channel);

typedef struct PhySettings_Trx_MdrRegionSetting_t{
    MDR_Delta_GetChannelPlanId getChannelPlanId;
    MDR_Delta_GetChannelFromFreq getChannelFromFreq;
    MDR_Delta_GetDeltaTable getDeltaTable;
    MDR_Delta_GetBaseIndex getBaseIndex;
    MDR_Delta_GetFreqFromChan getFreqFromChan;
}PhySettings_Trx_MdrRegionSetting;

extern const unsigned char PhySettings_Trx_numSupportedMdrPhys;

extern PhySettings_Trx_MdrRfSetting PhySettings_Trx_supportedMdrPhys;

extern PhySettings_Trx_MdrRegionSetting PhySettings_Trx_supportedMdrRegions[Num_Phy_Regions];


#endif // PHYSETTINGS_TRX_MDR_H