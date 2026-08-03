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
#ifndef PHYSETTINGS_TRX_H
#define PHYSETTINGS_TRX_H

/* Include Files */
#include <stdint.h>
#include <ti_cc140xp/ti_cc140xp.h>
/* included via ti_cc140xp/ti_cc140xp.h */

typedef enum
{
    PhySettings_Trx_PHY_0,
    PhySettings_Trx_PHY_1,
    PhySettings_Trx_PHY_2,
    PhySettings_Trx_PHY_3,
    PhySettings_Trx_PHY_4,
    PhySettings_Trx_PHY_5,
    PhySettings_Trx_PHY_6,
    PhySettings_Trx_PHY_7,
    PhySettings_Trx_PHY_8,
    PhySettings_Trx_PHY_9,
    PhySettings_Trx_PHY_10,
    PhySettings_Trx_PHY_11,
    PhySettings_Trx_PHY_12,
    PhySettings_Trx_PHY_13,
    PhySettings_Trx_PHY_14,
    PhySettings_Trx_PHY_15,
    PhySettings_Trx_PHY_16,
    PhySettings_Trx_PHY_17,
    PhySettings_Trx_PHY_18,
    PhySettings_Trx_PHY_19,
    PhySettings_Trx_PHY_20,
    PhySettings_Trx_PHY_21,
    PhySettings_Trx_PHY_22,
    //PhySettings_Trx_PHY_23, // Uncomment this line to add a PHY at index 23
    Num_Phy_Settings,
} PhySettings_Trx_phyIndex;

typedef enum
{
    PhySettings_TEST_RF_PERF,
    PhySettings_TEST_STUDIO_COMPL,
    Num_Test_Settings,
} PhySettings_testType;

typedef union Header_Settings_t {
    struct {
        uint16_t whitening   : 1; /*!< Whitening enabled (1) or not (0) */
        uint16_t fcs_mode    : 1; /*!< CRC32 (0) or CRC16 (1) */
        uint16_t reserved    : 14; /*!< Not used */
    } fsk;
    struct {
        uint16_t rate        : 4; /*!< Rate */
        uint16_t scrambler   : 2; /*!< Scrambler */
        uint16_t reserved    : 10; /*!< Not used */
    } ofdm;
} Header_Settings;

typedef struct PhySettings_RfSetting_t{
    PhySettings_Trx_phyIndex PhySettings_phyIndex;
    TRX_RadioCommand_Modem modem;
    Header_Settings header;
    uint16_t optionMask;
    uint8_t *rfConfig;
    char PhySettings_phyName[120];
    PhySettings_testType  PhySettings_testType;
}PhySettings_Trx_RfSetting;

extern const unsigned char PhySettings_Trx_numSupportedPhys;
extern PhySettings_Trx_RfSetting PhySettings_Trx_supportedPhys[];

#define PHY_SETTINGS_GENFSK_PHY_INDEX_START (PhySettings_Trx_PHY_13)
#define PHY_SETTINGS_GENFSK_PHY_INDEX_END (PhySettings_Trx_PHY_17)

#define PHY_SETTINGS_OFDMLR_PHY_INDEX_START (PhySettings_Trx_PHY_19)
#define PHY_SETTINGS_OFDMLR_PHY_INDEX_END (PhySettings_Trx_PHY_21)

#define PHY_SETTINGS_XLR_PHY_INDEX_START (PhySettings_Trx_PHY_22)
#define PHY_SETTINGS_XLR_PHY_INDEX_END (PhySettings_Trx_PHY_22)

#endif // PHYSETTINGS_TRX_H
