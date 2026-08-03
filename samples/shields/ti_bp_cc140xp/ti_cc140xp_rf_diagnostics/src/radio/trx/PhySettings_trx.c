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
#include "PhySettings_trx.h"
#include <stdint.h>
#include <ti_cc140xp/ti_cc140xp.h>

#include "settings/rcl_settings_wisun.h"
#include "settings/rcl_settings_2gfsk50kbps.h"
#include "settings/rcl_settings_2gfsk38p4kbps.h"
#include "settings/rcl_settings_2gfsk9p6kbps.h"
#include "settings/rcl_settings_general_purpose_gfsk.h"
#include "settings/rcl_settings_2gfsk50kbps_lp.h"
#include "settings/rcl_settings_sunoqpsk.h"
#include "settings/rcl_settings_ofdmlr_8p33ksps.h"
#include "settings/rcl_settings_ofdmlr_16p67ksps.h"
#include "settings/rcl_settings_ofdmlr_66p67ksps.h"
#include "settings/rcl_settings_xlr_20ksps.h"

#define PTR_WISUN_RF_CONFIG                 ((uint8_t *)LRF_mainRegConfig_wisun)
#define PTR_2GFSK_50KBPS_RF_CONFIG          ((uint8_t *)LRF_mainRegConfig_2gfsk50kbps)
#define PTR_2GFSK38_4KBPS_RF_CONFIG         ((uint8_t *)LRF_mainRegConfig_2gfsk38p4kbps)
#define PTR_2GFSK9P6KBPS_RF_CONFIG          ((uint8_t *)LRF_mainRegConfig_2gfsk9p6kbps)
#define PTR_GENERAL_PURPOSE_GFSK_RF_CONFIG  ((uint8_t *)LRF_mainRegConfig_generalPurposeGfsk)
#define PTR_2GFSK_50KBPS_LP_RF_CONFIG       ((uint8_t *)LRF_mainRegConfig_2gfsk50kbpsLp)
#define PTR_SUNOQPSK_RF_CONFIG              ((uint8_t *)LRF_mainRegConfig_sunoqpsk)
#define PTR_OFDMLR_8P33KSPS_RF_CONFIG       ((uint8_t *)LRF_mainRegConfig_ofdmlr8p33ksps)
#define PTR_OFDMLR_16P67KSPS_RF_CONFIG      ((uint8_t *)LRF_mainRegConfig_ofdmlr16p67ksps)
#define PTR_OFDMLR_66P67KSPS_RF_CONFIG      ((uint8_t *)LRF_mainRegConfig_ofdmlr66p67ksps)
#define PTR_XLR_20KSPS_RF_CONFIG            ((uint8_t *)LRF_mainRegConfig_xlr20ksps)

PhySettings_Trx_RfSetting PhySettings_Trx_supportedPhys[] =
{
    /* FSK PHYS */
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_0,
        .modem                      = TRX_RadioCommand_Modem_FSK,
        .header.fsk.fcs_mode        = 0, // CRC32
        .header.fsk.whitening       = 1, // Enabled
        .PhySettings_phyName        = {"PHY 0: WiSUN Mode #2b, PhySettings_TEST_STUDIO_COMPL"},
        .PhySettings_testType       = PhySettings_TEST_STUDIO_COMPL,
        .rfConfig                   = PTR_WISUN_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_FSK_MODE_2B_WISUN
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_1,
        .modem                      = TRX_RadioCommand_Modem_FSK,
        .header.fsk.fcs_mode        = 0, // CRC32
        .header.fsk.whitening       = 1, // Enabled
        .PhySettings_phyName        = {"PHY 1: WiSUN Mode #4b, PhySettings_TEST_STUDIO_COMPL"},
        .PhySettings_testType       = PhySettings_TEST_STUDIO_COMPL,
        .rfConfig                   = PTR_WISUN_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_FSK_MODE_4B_WISUN
    },
    /* OFDM PHYS */
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_2,
        .modem                      = TRX_RadioCommand_Modem_OFDM,
        .header.ofdm.rate           = TRX_PayloadHeader_SunOFDM_Rate_MCS6,
        .header.ofdm.scrambler      = 0U,
        .PhySettings_phyName        = {"PHY 2: Available ONLY ON TRX CC1407: OFDM WiSUN Option 1 MCS 6, PhySettings_TEST_RF_PERF"},
        .PhySettings_testType       = PhySettings_TEST_RF_PERF,
        .rfConfig                   = PTR_WISUN_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_OFDM_OPTION_1_WISUN
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_3,
        .modem                      = TRX_RadioCommand_Modem_OFDM,
        .header.ofdm.rate           = TRX_PayloadHeader_SunOFDM_Rate_MCS3,
        .header.ofdm.scrambler      = 0U,
        .PhySettings_phyName        = {"PHY 3: Available ONLY ON TRX CC1407: OFDM WiSUN Option 2 MCS 3, PhySettings_TEST_RF_PERF"},
        .PhySettings_testType       = PhySettings_TEST_RF_PERF,
        .rfConfig                   = PTR_WISUN_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_OFDM_OPTION_2_WISUN
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_4,
        .modem                      = TRX_RadioCommand_Modem_OFDM,
        .header.ofdm.rate           = TRX_PayloadHeader_SunOFDM_Rate_MCS4,
        .header.ofdm.scrambler      = 0U,
        .PhySettings_phyName        = {"PHY 4: Available ONLY ON TRX CC1407: OFDM WiSUN Option 2 MCS 4, PhySettings_TEST_RF_PERF"},
        .PhySettings_testType       = PhySettings_TEST_RF_PERF,
        .rfConfig                   = PTR_WISUN_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_OFDM_OPTION_2_WISUN
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_5,
        .modem                      = TRX_RadioCommand_Modem_OFDM,
        .header.ofdm.rate           = TRX_PayloadHeader_SunOFDM_Rate_MCS5,
        .header.ofdm.scrambler      = 0U,
        .PhySettings_phyName        = {"PHY 5: Available ONLY ON TRX CC1407: OFDM WiSUN Option 2 MCS 5, PhySettings_TEST_RF_PERF"},
        .PhySettings_testType       = PhySettings_TEST_RF_PERF,
        .rfConfig                   = PTR_WISUN_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_OFDM_OPTION_2_WISUN
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_6,
        .modem                      = TRX_RadioCommand_Modem_OFDM,
        .header.ofdm.rate           = TRX_PayloadHeader_SunOFDM_Rate_MCS6,
        .header.ofdm.scrambler      = 0U,
        .PhySettings_phyName        = {"PHY 6: Available ONLY ON TRX CC1407: OFDM WiSUN Option 2 MCS 6, PhySettings_TEST_RF_PERF"},
        .PhySettings_testType       = PhySettings_TEST_RF_PERF,
        .rfConfig                   = PTR_WISUN_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_OFDM_OPTION_2_WISUN
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_7,
        .modem                      = TRX_RadioCommand_Modem_OFDM,
        .header.ofdm.rate           = TRX_PayloadHeader_SunOFDM_Rate_MCS4,
        .header.ofdm.scrambler      = 0U,
        .PhySettings_phyName        = {"PHY 7: Available ONLY ON TRX CC1407: OFDM WiSUN Option 3 MCS 4, PhySettings_TEST_RF_PERF"},
        .PhySettings_testType       = PhySettings_TEST_RF_PERF,
        .rfConfig                   = PTR_WISUN_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_OFDM_OPTION_3MCS16_WISUN
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_8,
        .modem                      = TRX_RadioCommand_Modem_OFDM,
        .header.ofdm.rate           = TRX_PayloadHeader_SunOFDM_Rate_MCS5,
        .header.ofdm.scrambler      = 0U,
        .PhySettings_phyName        = {"PHY 8: Available ONLY ON TRX CC1407: OFDM WiSUN Option 3 MCS 5, PhySettings_TEST_RF_PERF"},
        .PhySettings_testType       = PhySettings_TEST_RF_PERF,
        .rfConfig                   = PTR_WISUN_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_OFDM_OPTION_3MCS16_WISUN
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_9,
        .modem                      = TRX_RadioCommand_Modem_OFDM,
        .header.ofdm.rate           = TRX_PayloadHeader_SunOFDM_Rate_MCS6,
        .header.ofdm.scrambler      = 0U,
        .PhySettings_phyName        = {"PHY 9: Available ONLY ON TRX CC1407: OFDM WiSUN Option 3 MCS 6, PhySettings_TEST_RF_PERF"},
        .PhySettings_testType       = PhySettings_TEST_RF_PERF,
        .rfConfig                   = PTR_WISUN_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_OFDM_OPTION_3MCS16_WISUN
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_10,
        .modem                      = TRX_RadioCommand_Modem_OFDM,
        .header.ofdm.rate           = TRX_PayloadHeader_SunOFDM_Rate_MCS4,
        .header.ofdm.scrambler      = 0U,
        .PhySettings_phyName        = {"PHY 10: Available ONLY ON TRX CC1407: OFDM WiSUN Option 4 MCS 4, PhySettings_TEST_RF_PERF"},
        .PhySettings_testType       = PhySettings_TEST_RF_PERF,
        .rfConfig                   = PTR_WISUN_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_OFDM_OPTION_4MCS26_WISUN
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_11,
        .modem                      = TRX_RadioCommand_Modem_OFDM,
        .header.ofdm.rate           = TRX_PayloadHeader_SunOFDM_Rate_MCS5,
        .header.ofdm.scrambler      = 0U,
        .PhySettings_phyName        = {"PHY 11: Available ONLY ON TRX CC1407: OFDM WiSUN Option 4 MCS 5, PhySettings_TEST_RF_PERF"},
        .PhySettings_testType       = PhySettings_TEST_RF_PERF,
        .rfConfig                   = PTR_WISUN_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_OFDM_OPTION_4MCS26_WISUN
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_12,
        .modem                      = TRX_RadioCommand_Modem_OFDM,
        .header.ofdm.rate           = TRX_PayloadHeader_SunOFDM_Rate_MCS6,
        .header.ofdm.scrambler      = 0U,
        .PhySettings_phyName        = {"PHY 12: Available ONLY ON TRX CC1407: OFDM WiSUN Option 4 MCS 6, PhySettings_TEST_RF_PERF"},
        .PhySettings_testType       = PhySettings_TEST_RF_PERF,
        .rfConfig                   = PTR_WISUN_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_OFDM_OPTION_4MCS26_WISUN
    },
    /* GENFSK PHYs */
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_13,
        .modem                      = TRX_RadioCommand_Modem_FSK,
        .header.fsk.fcs_mode        = 0, // not used
        .header.fsk.whitening       = 0, // not used
        .PhySettings_phyName        = {"PHY 13: 50 kbps 25 kHz deviation 2-GFSK High performance, PhySettings_TEST_STUDIO_COMPL"},
        .PhySettings_testType       = PhySettings_TEST_STUDIO_COMPL,
        .rfConfig                   = PTR_2GFSK_50KBPS_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_WHITENING_DISABLED_2GFSK50KBPS
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_14,
        .modem                      = TRX_RadioCommand_Modem_FSK,
        .header.fsk.fcs_mode        = 0, // not used
        .header.fsk.whitening       = 0, // not used
        .PhySettings_phyName        = {"PHY 14: 38.4 kbps 19.775 kHz deviation 2-GFSK, PhySettings_TEST_STUDIO_COMPL"},
        .PhySettings_testType       = PhySettings_TEST_STUDIO_COMPL,
        .rfConfig                   = PTR_2GFSK38_4KBPS_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_WHITENING_DISABLED_2GFSK38P4KBPS
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_15,
        .modem                      = TRX_RadioCommand_Modem_FSK,
        .header.fsk.fcs_mode        = 0, // not used
        .header.fsk.whitening       = 0, // not used
        .PhySettings_phyName        = {"PHY 15: 9.6 kbps 2.4 kHz deviation 2-GFSK, PhySettings_TEST_STUDIO_COMPL"},
        .PhySettings_testType       = PhySettings_TEST_STUDIO_COMPL,
        .rfConfig                   = PTR_2GFSK9P6KBPS_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_WHITENING_DISABLED_2GFSK9P6KBPS
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_16,
        .modem                      = TRX_RadioCommand_Modem_FSK,
        .header.fsk.fcs_mode        = 0, // not used
        .header.fsk.whitening       = 0, // not used
        .PhySettings_phyName        = {"PHY 16: General Purpose GFSK PHY, PhySettings_TEST_STUDIO_COMPL"},
        .PhySettings_testType       = PhySettings_TEST_STUDIO_COMPL,
        .rfConfig                   = PTR_GENERAL_PURPOSE_GFSK_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_WHITENING_DISABLED_GENERAL_PURPOSE_GFSK
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_17,
        .modem                      = TRX_RadioCommand_Modem_FSK,
        .header.fsk.fcs_mode        = 0, // not used
        .header.fsk.whitening       = 0, // not used
        .PhySettings_phyName        = {"PHY 17: 50 kbps 25kHz deviation 2-GFSK Low Power , PhySettings_TEST_STUDIO_COMPL"},
        .PhySettings_testType       = PhySettings_TEST_STUDIO_COMPL,
        .rfConfig                   = PTR_2GFSK_50KBPS_LP_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_WHITENING_DISABLED_2GFSK50KBPS_LP
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_18,
        .modem                      = TRX_RadioCommand_Modem_OQPSK,
        .header.fsk.fcs_mode        = 0, // not used
        .header.fsk.whitening       = 0, // not used
        .PhySettings_phyName        = {"PHY 18: SUN OQPSK PHY RATE MODE 3 (50 kbps), PhySettings_TEST_STUDIO_COMPL"},
        .PhySettings_testType       = PhySettings_TEST_STUDIO_COMPL,
        .rfConfig                   = PTR_SUNOQPSK_RF_CONFIG,
        .optionMask                 = 0
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_19,
        .modem                      = TRX_RadioCommand_Modem_OFDM,
        .header.ofdm.rate           = TRX_PayloadHeader_LrOFDM_Rate_DSSS6,
        .header.ofdm.scrambler      = 0U, // not used
        .PhySettings_phyName        = {"PHY 19: Available ONLY ON TRX CC1407: OFDM-LR (8.33 ksps, DSSS=(6,1)), PhySettings_TEST_STUDIO_COMPL"},
        .PhySettings_testType       = PhySettings_TEST_STUDIO_COMPL,
        .rfConfig                   = PTR_OFDMLR_8P33KSPS_RF_CONFIG,
        .optionMask                 = 0
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_20,
        .modem                      = TRX_RadioCommand_Modem_OFDM,
        .header.ofdm.rate           = TRX_PayloadHeader_LrOFDM_Rate_DSSS6,
        .header.ofdm.scrambler      = 0U, // not used
        .PhySettings_phyName        = {"PHY 20: Available ONLY ON TRX CC1407: OFDM-LR (16.67 ksps, DSSS=(6,1)), PhySettings_TEST_STUDIO_COMPL"},
        .PhySettings_testType       = PhySettings_TEST_STUDIO_COMPL,
        .rfConfig                   = PTR_OFDMLR_16P67KSPS_RF_CONFIG,
        .optionMask                 = 0
    },
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_21,
        .modem                      = TRX_RadioCommand_Modem_OFDM,
        .header.ofdm.rate           = TRX_PayloadHeader_LrOFDM_Rate_DSSS2,
        .header.ofdm.scrambler      = 0U, // not used
        .PhySettings_phyName        = {"PHY 21: Available ONLY ON TRX CC1407: OFDM-LR (66.67 ksps, DSSS=(2,1)), PhySettings_TEST_STUDIO_COMPL"},
        .PhySettings_testType       = PhySettings_TEST_STUDIO_COMPL,
        .rfConfig                   = PTR_OFDMLR_66P67KSPS_RF_CONFIG,
        .optionMask                 = 0
    },
    /* XLR PHY */
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_22,
        .modem                      = TRX_RadioCommand_Modem_FSK,
        .header.fsk.fcs_mode        = 0, // not used
        .header.fsk.whitening       = 0, // not used
        .PhySettings_phyName        = {"PHY 22: XLR 20KSPS (DSSS 2), PhySettings_TEST_STUDIO_COMPL"},
        .PhySettings_testType       = PhySettings_TEST_STUDIO_COMPL,
        .rfConfig                   = PTR_XLR_20KSPS_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_DSSS_2_XLR_20KSPS
    },
    /* EXAMPLE STRUCT */
    /*
    {
        .PhySettings_phyIndex       = PhySettings_Trx_PHY_23,
        .modem                      = TRX_RadioCommand_Modem_FSK,
        .header.fsk.fcs_mode        = 0, // CRC32
        .header.fsk.whitening       = 1, // Enabled
        .PhySettings_phyName        = {"PHY 23: WiSUN Mode 5, PhySettings_TEST_STUDIO_COMPL"},
        .PhySettings_testType       = PhySettings_TEST_STUDIO_COMPL,
        .rfConfig                   = PTR_WISUN_RF_CONFIG,
        .optionMask                 = TRX_PHY_FEATURE_FSK_MODE_5_WISUN
    },
    */
};

const uint8_t PhySettings_Trx_numSupportedPhys = sizeof(PhySettings_Trx_supportedPhys)/sizeof(PhySettings_Trx_RfSetting);
