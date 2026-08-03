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

/* Include Files */
#include <stdint.h>
#include <ti_cc140xp/ti_cc140xp.h>
/* included via ti_cc140xp/ti_cc140xp.h */
#include "PhySettings_trx_mdr.h"
#include "radio/radio.h"

uint8_t extractPhyIndex(uint8_t phyIndex, uint8_t region, uint8_t *payload)
{
    uint8_t idx = phyIndex;
    uint8_t mdrByte;
    uint8_t modem;
    uint8_t rate = 0;
    if(TRX_PayloadHeader_Modulation_FSK == ((TRX_PayloadHeader_Common *)payload)->modulation)
    {
        mdrByte = (uint8_t)(((TRX_PayloadHeader_SunFSK *)payload)->newPhyId);
        modem = TRX_RadioCommand_Modem_FSK;
    }
    else if(TRX_PayloadHeader_Modulation_OFDM == ((TRX_PayloadHeader_Common *)payload)->modulation)
    {
        mdrByte = (uint8_t)(((TRX_PayloadHeader_SunOFDM *)payload)->newPhyId);
        modem = TRX_RadioCommand_Modem_OFDM;
        rate = (uint8_t)(((TRX_PayloadHeader_SunOFDM *)payload)->rate);

        // remove the rate or'd from the mdrByte
        mdrByte = rate ^ mdrByte;
    }
    else
    {
        mdrByte = 0;
    }
    if(mdrByte != 0)
    {
        uint8_t i;
        uint16_t optionMask = RADIO_NO_PHY;
        TRX_WisunMdrMappingTable *map = PhySettings_Trx_supportedMdrPhys.mappingTable;
        for(i = 0; i < map->numEntries; i++)
        {
            if(mdrByte == map->mappingTable[i].mdrByte)
            {
                optionMask = map->mappingTable[i].optionMask;
                break;
            }
        }
        if(RADIO_NO_PHY != optionMask)
        {
            bool indexFound = false;
            uint8_t rate_to_compare;
            for(i = 0; i < PhySettings_Trx_numSupportedMdrPhys; i++)
            {
                // Only need to check the rate if it's an OFDM phy
                if(PhySettings_Trx_supportedMdrPhys.phys[i].modem == TRX_RadioCommand_Modem_OFDM)
                {
                    rate_to_compare = PhySettings_Trx_supportedMdrPhys.phys[i].header.ofdm.rate;
                }
                else
                {
                    rate_to_compare = rate;
                }
                if((PhySettings_Trx_supportedMdrPhys.phys[i].modem == modem)
                && (PhySettings_Trx_supportedMdrPhys.phys[i].optionMask == optionMask)
                && (rate_to_compare == rate))
                {
                    indexFound = true;
                    break;
                }
            }
            if(indexFound)
            {
                idx = i;
            }
        }
    }
    return (idx);
}

uint8_t getMappingTableIdx(uint8_t region, uint8_t modem, uint16_t optionMask)
{
    uint8_t i;
    TRX_WisunMdrMappingTable *map = PhySettings_Trx_supportedMdrPhys.mappingTable;
    for(i = 0; i < map->numEntries; i++)
    {
        // Find modem table of payload tx cmd
        if(map->mappingTable[i].modem == modem)
        {
            // Find optionmask value within modem table and return index in mapping table
            if(map->mappingTable[i].optionMask == optionMask)
            {
                break;
            }
        }
    }
    return (i);
}

const uint32_t* getDeltaConfig(uint8_t region, uint8_t phyIndex)
{
    uint8_t modem = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].modem;
    uint16_t optionMask = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].optionMask;
    uint8_t idx = getMappingTableIdx(region, modem, optionMask);
    const uint32_t *deltaConfig = NULL;
    if(idx < PhySettings_Trx_supportedMdrPhys.mappingTable->numEntries)
    {
        TRX_WisunMdrMappingTable *map = PhySettings_Trx_supportedMdrPhys.mappingTable;
        uint8_t chanPlanIdIdx = map->mappingTable[idx].channelPlanIdIdx;
        uint16_t chanPlanId = PhySettings_Trx_supportedMdrRegions[region].getChannelPlanId(chanPlanIdIdx);
        deltaConfig = PhySettings_Trx_supportedMdrRegions[region].getDeltaTable(chanPlanId);
    }
    return (deltaConfig);
}

int16_t getDeltaBaseIndex(uint8_t region, uint8_t phyIndex, uint32_t frequency)
{
    int16_t baseIdx = 0;
    uint8_t modem = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].modem;
    uint16_t optionMask = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].optionMask;
    uint8_t idx = getMappingTableIdx(region, modem, optionMask);
    if(idx < PhySettings_Trx_supportedMdrPhys.mappingTable->numEntries)
    {
        TRX_WisunMdrMappingTable *map = PhySettings_Trx_supportedMdrPhys.mappingTable;
        uint8_t chanPlanIdIdx = map->mappingTable[idx].channelPlanIdIdx;
        uint16_t chanPlanId = PhySettings_Trx_supportedMdrRegions[region].getChannelPlanId(chanPlanIdIdx);
        int16_t channel = PhySettings_Trx_supportedMdrRegions[region].getChannelFromFreq(chanPlanId, frequency);
        baseIdx = PhySettings_Trx_supportedMdrRegions[region].getBaseIndex(chanPlanId, channel);
    }

    return (baseIdx);
}

void composeMdrCs(TRX_Request_CommandStore *cmd, uint8_t region, uint8_t phyIndex)
{
    // Calculating the proper CS Timeout to submit per command:
    // Timeout = Settling + RSSI Discarding + RSSI Window Settling
    uint32_t settling = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].csSettlingTimeUs;
    // RSSI Discarding = 6 Samples * (Time per Sample)
    uint32_t rssiDiscarding = 6 * PhySettings_Trx_supportedMdrPhys.phys[phyIndex].csSamplePeriodUs;

    cmd->params.cs.timeout = settling + rssiDiscarding;

    cmd->params.cs.mode = TRX_CarrierSense_Mode_Energy;
    cmd->params.cs.rssi_override = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].csRSSIOverridePerRegion[region];
    cmd->params.cs.rssi_window = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].csWindowSamplesPerRegion[region];
    cmd->params.cs.rssi_count = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].csCountSamplesPerRegion[region];
    cmd->params.cs.exit_condition = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].csExitCondPerRegion[region];
    cmd->params.cs.timeout += PhySettings_Trx_supportedMdrPhys.phys[phyIndex].csTimeoutUsPerRegion[region];
}

int composeMdrPpdu(TRX_Request_CommandStore *tx, TRX_Request_CommandStore *mdrTx, uint8_t *payload, uint8_t region, uint8_t phyIndex, uint8_t phyIndex2)
{
    int totalLength = sizeof(TRX_PayloadHeader_SunFSK);

    if(TRX_RadioCommand_Modem_FSK == tx->params.tx.modem)
    {
        ((TRX_PayloadHeader_SunFSK *)payload)->length = 0U; // 0U because no payload
        ((TRX_PayloadHeader_SunFSK *)payload)->modulation = TRX_PayloadHeader_Modulation_FSK; // 0U = FSK
        ((TRX_PayloadHeader_SunFSK *)payload)->mode_switch = 1U; // 1U = Mode switch Enabled
        ((TRX_PayloadHeader_SunFSK *)payload)->fcs_mode = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].header.fsk.fcs_mode; // 0U = CRC32; 1U = CRC16
        ((TRX_PayloadHeader_SunFSK *)payload)->whitening = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].header.fsk.whitening; // 1U = Whitening enabled
        //TODO: update this when defaultWisunMdrMappingTable is generated by RfStudio
        uint8_t i = getMappingTableIdx(region, mdrTx->params.tx.modem, mdrTx->params.tx.phy0.option_mask);
        TRX_WisunMdrMappingTable *map = PhySettings_Trx_supportedMdrPhys.mappingTable;
        uint16_t phyId = map->mappingTable[i].mdrByte;
        uint16_t rate = 0;
        if(TRX_RadioCommand_Modem_OFDM == mdrTx->params.tx.modem)
        {
            rate = PhySettings_Trx_supportedMdrPhys.phys[phyIndex2].header.ofdm.rate;
        }

        ((TRX_PayloadHeader_SunFSK *)payload)->newPhyId = (phyId | rate);
    }
    else
    {
        // Unsupported modem
        totalLength = -1;
    }

    return (totalLength);
}

int composeMdrPayload(TRX_Request_CommandStore *mdrTx, uint32_t dataLength, uint8_t *payload, uint8_t region, uint8_t phyIndex)
{
    static uint16_t sequenceNumber = 0;
    int totalLength;

    int i = 0;
    for (; i < sizeof(TRX_PayloadHeader); i++)
    {
        payload[i++] = 0;
    }

    payload[i++] = sequenceNumber & 0x00FF;
    payload[i++] = (sequenceNumber++ >> 8) & 0x00FF;

    totalLength = i + dataLength - sizeof(sequenceNumber);

    /** Update the PHY header **/
    if(TRX_RadioCommand_Modem_FSK == mdrTx->params.tx.modem)
    {
        ((TRX_PayloadHeader_SunFSK *)payload)->length = dataLength; // This length excludes the header
        ((TRX_PayloadHeader_SunFSK *)payload)->modulation = TRX_PayloadHeader_Modulation_FSK; // 0U = FSK
        ((TRX_PayloadHeader_SunFSK *)payload)->mode_switch = 0U; // 0U = No mode switch
        ((TRX_PayloadHeader_SunFSK *)payload)->fcs_mode = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].header.fsk.fcs_mode; // 0U = CRC32; 1U = CRC16
        ((TRX_PayloadHeader_SunFSK *)payload)->whitening = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].header.fsk.whitening; // 1U = Whitening enabled
    }
    else if(TRX_RadioCommand_Modem_OFDM == mdrTx->params.tx.modem)
    {
        ((TRX_PayloadHeader_SunOFDM *)payload)->length = dataLength; // This length excludes the header
        ((TRX_PayloadHeader_SunOFDM *)payload)->modulation = TRX_PayloadHeader_Modulation_OFDM; // 1U = OFDM
        ((TRX_PayloadHeader_SunOFDM *)payload)->rate = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].header.ofdm.rate; // MCS
        ((TRX_PayloadHeader_SunOFDM *)payload)->scrambler = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].header.ofdm.scrambler;
        ((TRX_PayloadHeader_SunOFDM *)payload)->newPhyId = 0U;
    }
    else
    {
        // Unsupported modem
        totalLength = -1;
    }

    for (; i < totalLength; i++)
    {
        payload[i] = rand();
    }

    return (totalLength);
}


uint32_t getFrequencyFromChannel(uint8_t region, uint8_t phyIndex, int16_t channel)
{
 
    uint32_t freq = WISUN_FREQ_INVALID;
    uint8_t modem = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].modem;
    uint16_t optionMask = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].optionMask;
    uint8_t idx = getMappingTableIdx(region, modem, optionMask);

    if(idx < PhySettings_Trx_supportedMdrPhys.mappingTable->numEntries)
    {
        TRX_WisunMdrMappingTable *map = PhySettings_Trx_supportedMdrPhys.mappingTable;
        uint8_t chanPlanIdIdx = map->mappingTable[idx].channelPlanIdIdx;
        uint16_t chanPlanId = PhySettings_Trx_supportedMdrRegions[region].getChannelPlanId(chanPlanIdIdx);      
        freq = PhySettings_Trx_supportedMdrRegions[region].getFreqFromChan(chanPlanId, channel);
    }
    return (freq);
}

uint32_t getNextDeltaFrequency(uint8_t region, uint8_t phyIndex, uint32_t frequency)
{
    uint32_t deltaFreq = WISUN_FREQ_INVALID;
    // get the modem and option mask of the mdr phy
    uint8_t modem = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].modem;
    uint16_t optionMask = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].optionMask;
    uint8_t idx = getMappingTableIdx(region, modem, optionMask);

    if(idx < PhySettings_Trx_supportedMdrPhys.mappingTable->numEntries)
    {
        TRX_WisunMdrMappingTable *map = PhySettings_Trx_supportedMdrPhys.mappingTable;
        uint8_t chanPlanIdIdx = map->mappingTable[idx].channelPlanIdIdx;
        uint16_t chanPlanId = PhySettings_Trx_supportedMdrRegions[region].getChannelPlanId(chanPlanIdIdx);
        uint16_t minChan = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].minChannel;
        uint16_t maxChan = PhySettings_Trx_supportedMdrPhys.phys[phyIndex].maxChannel;
        for(int i = minChan; i <= maxChan; i++)
        {
            deltaFreq = PhySettings_Trx_supportedMdrRegions[region].getFreqFromChan(chanPlanId, i);
            if((frequency < deltaFreq) && (WISUN_FREQ_INVALID != deltaFreq))
            {
                // check to see if this channel or the previous channel frequency is closer
                uint32_t prevDeltaFreq = PhySettings_Trx_supportedMdrRegions[region].getFreqFromChan(chanPlanId, i-1);
                if((deltaFreq - frequency) > (frequency - prevDeltaFreq))
                {
                    deltaFreq = prevDeltaFreq;
                }
                break;
            }
        }

    }

    return (deltaFreq);
}
