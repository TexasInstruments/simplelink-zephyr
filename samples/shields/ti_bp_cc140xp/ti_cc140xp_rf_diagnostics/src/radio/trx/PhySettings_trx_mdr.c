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
/* Includes Files */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "PhySettings_trx_mdr.h"
#include <ti_cc140xp/ti_cc140xp.h>

#include "settings/rcl_settings_wisun.h"
#define PTR_WISUN_RF_CONFIG  ((uint8_t *)LRF_mainRegConfig_wisun)


#include "wisun/wisun_delta_tables_NA.h"
#include "wisun/wisun_delta_tables_JP.h"
#include "wisun/wisun_delta_tables_EU1.h"

const uint8_t PhySettings_Trx_numSupportedMdrPhys = (sizeof(PhySettings_Trx_supportedMdrPhys) - sizeof(uint32_t))/sizeof(PhySettings_Trx_MdrRfEntry);

PhySettings_Trx_MdrRfSetting PhySettings_Trx_supportedMdrPhys = {
    .rfConfig = PTR_WISUN_RF_CONFIG,
    .mappingTable = NULL,
    .phys = {
        {
            .PhySettings_phyIndex       = PhySettings_Trx_MDRPHY_0,
            .modem                      = TRX_RadioCommand_Modem_FSK,
            .header.fsk.fcs_mode        = 0, // CRC32
            .header.fsk.whitening       = 1, // Enabled
            .PhySettings_phyName        = {"PHY 0: WiSUN Mode #2b, PhySettings_TEST_STUDIO_COMPL"},
            .PhySettings_testType       = PhySettings_TEST_STUDIO_COMPL,
            .optionMask                 = TRX_PHY_FEATURE_FSK_MODE_2B_WISUN,
            .loadDeltaTable             = true,
            .minChannel                 = 4,
            .maxChannel                 = 17,
            .csSettlingTimeUs           = 52, // TODO: replace with SmartRF Studio defines per PHY
            .csSamplePeriodUs           = 16, // TODO: replace with SmartRF Studio defines per PHY
            .csTimeoutUsPerRegion       = {0, 80, 128, 160}, // Indexed by region
            .csRSSIOverridePerRegion    = {0,-87,-80,-83}, // Indexed by region
            .csWindowSamplesPerRegion   = {0,5,5,5}, // Indexed by region
            .csCountSamplesPerRegion    = {0,5,5,5}, // Indexed by region
            .csExitCondPerRegion        = {TRX_CarrierSense_ExitCondition_WaitForIdle,
                                           TRX_CarrierSense_ExitCondition_Fast,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy}, // Indexed by region
        },
        {
            .PhySettings_phyIndex       = PhySettings_Trx_MDRPHY_1,
            .modem                      = TRX_RadioCommand_Modem_FSK,
            .header.fsk.fcs_mode        = 0, // CRC32
            .header.fsk.whitening       = 1, // Enabled
            .PhySettings_phyName        = {"PHY 1: WiSUN Mode #4b, PhySettings_TEST_STUDIO_COMPL"},
            .PhySettings_testType       = PhySettings_TEST_STUDIO_COMPL,
            .optionMask                 = TRX_PHY_FEATURE_FSK_MODE_4B_WISUN,
            .loadDeltaTable             = true,
            .minChannel                 = 3,
            .maxChannel                 = 11,
            .csSettlingTimeUs           = 41, // TODO: replace with SmartRF Studio defines per PHY
            .csSamplePeriodUs           = 8, // TODO: replace with SmartRF Studio defines per PHY
            .csTimeoutUsPerRegion       = {0, 40, 128, 160}, // Indexed by region
            .csRSSIOverridePerRegion    = {0,-83,-80,-83}, // Indexed by region
            .csWindowSamplesPerRegion   = {0,5,5,5}, // Indexed by region
            .csCountSamplesPerRegion    = {0,5,5,5}, // Indexed by region
            .csExitCondPerRegion        = {TRX_CarrierSense_ExitCondition_WaitForIdle,
                                           TRX_CarrierSense_ExitCondition_Fast,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy}, // Indexed by region
        }, 
        {
            .PhySettings_phyIndex       = PhySettings_Trx_MDRPHY_2,
            .modem                      = TRX_RadioCommand_Modem_OFDM,
            .header.ofdm.rate           = TRX_PayloadHeader_SunOFDM_Rate_MCS3,
            .header.ofdm.scrambler      = 0U,
            .PhySettings_phyName        = {"PHY 2: OFDM WiSUN Option 2 MCS 3, PhySettings_TEST_RF_PERF"},
            .PhySettings_testType       = PhySettings_TEST_RF_PERF,
            .optionMask                 = TRX_PHY_FEATURE_OFDM_OPTION_2_WISUN,
            .loadDeltaTable             = false,
            .minChannel                 = 2,
            .maxChannel                 = 8,
            .csSettlingTimeUs           = 29, // TODO: replace with SmartRF Studio defines per PHY
            .csSamplePeriodUs           = 6, // TODO: replace with SmartRF Studio defines per PHY
            .csTimeoutUsPerRegion       = {0, 128, 128, 160}, // Indexed by region
            .csRSSIOverridePerRegion    = {0,
                                           TRX_CarrierSenseRSSI_UsePHY,
                                           -80,
                                           TRX_CarrierSenseRSSI_UsePHY}, // Indexed by region
            .csWindowSamplesPerRegion   = {0,3,3,3}, // Indexed by region
            .csCountSamplesPerRegion    = {0,3,3,3}, // Indexed by region
            .csExitCondPerRegion        = {TRX_CarrierSense_ExitCondition_WaitForIdle,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy}, // Indexed by region
        },
        {
            .PhySettings_phyIndex       = PhySettings_Trx_MDRPHY_3,
            .modem                      = TRX_RadioCommand_Modem_OFDM,
            .header.ofdm.rate           = TRX_PayloadHeader_SunOFDM_Rate_MCS6,
            .header.ofdm.scrambler      = 0U,
            .PhySettings_phyName        = {"PHY 3: OFDM WiSUN Option 2 MCS 6, PhySettings_TEST_RF_PERF"},
            .PhySettings_testType       = PhySettings_TEST_RF_PERF,
            .optionMask                 = TRX_PHY_FEATURE_OFDM_OPTION_2_WISUN,
            .loadDeltaTable             = false,
            .minChannel                 = 2,
            .maxChannel                 = 8,
            .csSettlingTimeUs           = 29, // TODO: replace with SmartRF Studio defines per PHY
            .csSamplePeriodUs           = 6, // TODO: replace with SmartRF Studio defines per PHY
            .csTimeoutUsPerRegion       = {0, 128, 128, 160}, // Indexed by region
            .csRSSIOverridePerRegion    = {0,
                                           TRX_CarrierSenseRSSI_UsePHY,
                                           -80,
                                           TRX_CarrierSenseRSSI_UsePHY}, // Indexed by region
            .csWindowSamplesPerRegion   = {0,3,3,3}, // Indexed by region
            .csCountSamplesPerRegion    = {0,3,3,3}, // Indexed by region
            .csExitCondPerRegion        = {TRX_CarrierSense_ExitCondition_WaitForIdle,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy}, // Indexed by region
        },
        {
            .PhySettings_phyIndex       = PhySettings_Trx_MDRPHY_4,
            .modem                      = TRX_RadioCommand_Modem_OFDM,
            .header.ofdm.rate           = TRX_PayloadHeader_SunOFDM_Rate_MCS4,
            .header.ofdm.scrambler      = 0U,
            .PhySettings_phyName        = {"PHY 4: OFDM WiSUN Option 3 MCS 4, PhySettings_TEST_RF_PERF"},
            .PhySettings_testType       = PhySettings_TEST_RF_PERF,
            .optionMask                 = TRX_PHY_FEATURE_OFDM_OPTION_3MCS16_WISUN,
            .loadDeltaTable             = false,
            .minChannel                 = 4,
            .maxChannel                 = 17,
            .csSettlingTimeUs           = 29, // TODO: replace with SmartRF Studio defines per PHY
            .csSamplePeriodUs           = 6, // TODO: replace with SmartRF Studio defines per PHY
            .csTimeoutUsPerRegion       = {0, 128, 128, 160}, // Indexed by region
            .csRSSIOverridePerRegion    = {0,
                                           TRX_CarrierSenseRSSI_UsePHY,
                                           -80,
                                           TRX_CarrierSenseRSSI_UsePHY}, // Indexed by region
            .csWindowSamplesPerRegion   = {0,3,3,3}, // Indexed by region
            .csCountSamplesPerRegion    = {0,3,3,3}, // Indexed by region
            .csExitCondPerRegion        = {TRX_CarrierSense_ExitCondition_WaitForIdle,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy}, // Indexed by region
        },
        {
            .PhySettings_phyIndex       = PhySettings_Trx_MDRPHY_5,
            .modem                      = TRX_RadioCommand_Modem_OFDM,
            .header.ofdm.rate           = TRX_PayloadHeader_SunOFDM_Rate_MCS6,
            .header.ofdm.scrambler      = 0U,
            .PhySettings_phyName        = {"PHY 5: OFDM WiSUN Option 3 MCS 6, PhySettings_TEST_RF_PERF"},
            .PhySettings_testType       = PhySettings_TEST_RF_PERF,
            .optionMask                 = TRX_PHY_FEATURE_OFDM_OPTION_3MCS16_WISUN,
            .loadDeltaTable             = false,
            .minChannel                 = 4,
            .maxChannel                 = 17,
            .csSettlingTimeUs           = 29, // TODO: replace with SmartRF Studio defines per PHY
            .csSamplePeriodUs           = 6, // TODO: replace with SmartRF Studio defines per PHY
            .csTimeoutUsPerRegion       = {0, 128, 128, 160}, // Indexed by region
            .csRSSIOverridePerRegion    = {0,
                                           TRX_CarrierSenseRSSI_UsePHY,
                                           -80,
                                           TRX_CarrierSenseRSSI_UsePHY}, // Indexed by region
            .csWindowSamplesPerRegion   = {0,3,3,3}, // Indexed by region
            .csCountSamplesPerRegion    = {0,3,3,3}, // Indexed by region
            .csExitCondPerRegion        = {TRX_CarrierSense_ExitCondition_WaitForIdle,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy}, // Indexed by region
        },
        {
            .PhySettings_phyIndex       = PhySettings_Trx_MDRPHY_6,
            .modem                      = TRX_RadioCommand_Modem_OFDM,
            .header.ofdm.rate           = TRX_PayloadHeader_SunOFDM_Rate_MCS4,
            .header.ofdm.scrambler      = 0U,
            .PhySettings_phyName        = {"PHY 6: OFDM WiSUN Option 4 MCS 4, PhySettings_TEST_RF_PERF"},
            .PhySettings_testType       = PhySettings_TEST_RF_PERF,
            .optionMask                 = TRX_PHY_FEATURE_OFDM_OPTION_4MCS26_WISUN,
            .loadDeltaTable             = false,
            .minChannel                 = 9,
            .maxChannel                 = 37,
            .csSettlingTimeUs           = 29, // TODO: replace with SmartRF Studio defines per PHY
            .csSamplePeriodUs           = 6, // TODO: replace with SmartRF Studio defines per PHY
            .csTimeoutUsPerRegion       = {0, 128, 128, 160}, // Indexed by region
            .csRSSIOverridePerRegion    = {0,
                                           TRX_CarrierSenseRSSI_UsePHY,
                                           -80,
                                           TRX_CarrierSenseRSSI_UsePHY}, // Indexed by region
            .csWindowSamplesPerRegion   = {0,3,3,3}, // Indexed by region
            .csCountSamplesPerRegion    = {0,3,3,3}, // Indexed by region
            .csExitCondPerRegion        = {TRX_CarrierSense_ExitCondition_WaitForIdle,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy}, // Indexed by region
        },
        {
            .PhySettings_phyIndex       = PhySettings_Trx_MDRPHY_7,
            .modem                      = TRX_RadioCommand_Modem_OFDM,
            .header.ofdm.rate           = TRX_PayloadHeader_SunOFDM_Rate_MCS6,
            .header.ofdm.scrambler      = 0U,
            .PhySettings_phyName        = {"PHY 7: OFDM WiSUN Option 4 MCS 6, PhySettings_TEST_RF_PERF"},
            .PhySettings_testType       = PhySettings_TEST_RF_PERF,
            .optionMask                 = TRX_PHY_FEATURE_OFDM_OPTION_4MCS26_WISUN,
            .loadDeltaTable             = false,
            .minChannel                 = 9,
            .maxChannel                 = 37,
            .csSettlingTimeUs           = 29, // TODO: replace with SmartRF Studio defines per PHY
            .csSamplePeriodUs           = 6, // TODO: replace with SmartRF Studio defines per PHY
            .csTimeoutUsPerRegion       = {0, 128, 128, 160}, // Indexed by region
            .csRSSIOverridePerRegion    = {0,
                                           TRX_CarrierSenseRSSI_UsePHY,
                                           -80,
                                           TRX_CarrierSenseRSSI_UsePHY}, // Indexed by region
            .csWindowSamplesPerRegion   = {0,3,3,3}, // Indexed by region
            .csCountSamplesPerRegion    = {0,3,3,3}, // Indexed by region
            .csExitCondPerRegion        = {TRX_CarrierSense_ExitCondition_WaitForIdle,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy,
                                           TRX_CarrierSense_ExitCondition_WaitForBusy}, // Indexed by region
        }        
    }
};

PhySettings_Trx_MdrRegionSetting PhySettings_Trx_supportedMdrRegions[] =
{
    {
        // Region 0 is MDR Disabled. Leave these as NULL or 0!
        .getChannelPlanId = NULL,
        .getChannelFromFreq = NULL,
        .getDeltaTable = NULL,
        .getBaseIndex = NULL,
        .getFreqFromChan = NULL,
    },
    {
        // Region 1 is MDR NA
        .getChannelPlanId = &(get_wisun_channelPlanId_from_channelPlanIdIdx_NA),
        .getChannelFromFreq = &(get_wisun_channel_from_frequency_NA),
        .getDeltaTable = &(get_wisun_delta_table_NA),
        .getBaseIndex = &(get_wisun_delta_table_baseindex_NA),
        .getFreqFromChan = &(get_wisun_frequency_from_channel_NA),
    },
    {
        // Region 2 is MDR JP
        .getChannelPlanId = &(get_wisun_channelPlanId_from_channelPlanIdIdx_JP),
        .getChannelFromFreq = &(get_wisun_channel_from_frequency_JP),
        .getDeltaTable = &(get_wisun_delta_table_JP),
        .getBaseIndex = &(get_wisun_delta_table_baseindex_JP),
        .getFreqFromChan = &(get_wisun_frequency_from_channel_JP),
    },
    {
        // Region 3 is MDR EU1
        .getChannelPlanId = &(get_wisun_channelPlanId_from_channelPlanIdIdx_EU1),
        .getChannelFromFreq = &(get_wisun_channel_from_frequency_EU1),
        .getDeltaTable = &(get_wisun_delta_table_EU1),
        .getBaseIndex = &(get_wisun_delta_table_baseindex_EU1),
        .getFreqFromChan = &(get_wisun_frequency_from_channel_EU1),
    },
};
