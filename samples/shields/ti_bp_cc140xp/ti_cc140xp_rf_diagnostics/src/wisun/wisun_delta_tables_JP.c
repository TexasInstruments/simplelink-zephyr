
/*
 * Copyright (c) 2023, Texas Instruments Incorporated
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


#include "wisun_delta_tables_JP.h"

// DeltaTable for JP, ChannelPlanID 21
// Size in bytes: 68
const uint32_t wisun_delta_table_JP_from_ChannelPlanID21[] =
{
    0x00000010,    //  ConfigSegment(length=16, feature_mask=0, inverted=False, compound=False)
    0x0030900e,    //  ConfigRegion(length=15, operation=<ConfigRegionOperation.Par_Reference_32bit: 9>, address=48)
    0x02010064,    //  ConfigValue32(value32=33620068)
    0x00ff0003,    //  ConfigValue32(value32=16711683)
    0xfe010001,    //  ConfigValue32(value32=4261478401)
    0x02ff00ff,    //  ConfigValue32(value32=50266367)
    0x000100fd,    //  ConfigValue32(value32=65789)
    0xfeff0003,    //  ConfigValue32(value32=4278124547)
    0x02010001,    //  ConfigValue32(value32=33619969)
    0x00ff00ff,    //  ConfigValue32(value32=16711935)
    0xfe0100fd,    //  ConfigValue32(value32=4261478653)
    0x02ff0003,    //  ConfigValue32(value32=50266115)
    0x00010001,    //  ConfigValue32(value32=65537)
    0xfeff00ff,    //  ConfigValue32(value32=4278124799)
    0xfcff00fd,    //  ConfigValue32(value32=4244570365)
    0xfafd00fd,    //  ConfigValue32(value32=4210884861)
    0x000000fb     //  ConfigValue32(value32=251)
};

// DeltaTable for JP, ChannelPlanID 22
// Size in bytes: 40
const uint32_t wisun_delta_table_JP_from_ChannelPlanID22[] =
{
    0x00000009,    //  ConfigSegment(length=9, feature_mask=0, inverted=False, compound=False)
    0x00309007,    //  ConfigRegion(length=8, operation=<ConfigRegionOperation.Par_Reference_32bit: 9>, address=48)
    0x01000164,    //  ConfigValue32(value32=16777572)
    0x03000102,    //  ConfigValue32(value32=50331906)
    0xff0001fe,    //  ConfigValue32(value32=4278190590)
    0x01000102,    //  ConfigValue32(value32=16777474)
    0x030001fe,    //  ConfigValue32(value32=50332158)
    0xff000102,    //  ConfigValue32(value32=4278190338)
    0xfd0001fe,    //  ConfigValue32(value32=4244636158)
    0x000000fe     //  ConfigValue32(value32=254)
};

// DeltaTable for JP, ChannelPlanID 23
// Size in bytes: 28
const uint32_t wisun_delta_table_JP_from_ChannelPlanID23[] =
{
    0x00000006,    //  ConfigSegment(length=6, feature_mask=0, inverted=False, compound=False)
    0x00309004,    //  ConfigRegion(length=5, operation=<ConfigRegionOperation.Par_Reference_32bit: 9>, address=48)
    0x00ff0064,    //  ConfigValue32(value32=16711780)
    0x00010001,    //  ConfigValue32(value32=65537)
    0x00ff0003,    //  ConfigValue32(value32=16711683)
    0x000100fd,    //  ConfigValue32(value32=65789)
    0x000000ff     //  ConfigValue32(value32=255)
};

/*****************************************
 * HOST FUNCTIONS
 ****************************************/
const uint32_t* get_wisun_delta_table_JP(uint16_t channelPlanId)
{
    switch (channelPlanId)
    {
        case 21: return wisun_delta_table_JP_from_ChannelPlanID21;
        case 22: return wisun_delta_table_JP_from_ChannelPlanID22;
        case 23: return wisun_delta_table_JP_from_ChannelPlanID23;
        default: return 0;
    }
}

int16_t get_wisun_delta_table_baseindex_JP(uint16_t channelPlanId, uint16_t channel)
{
    switch (channelPlanId)
    {
        // Channel to BaseIndex mapping for JP, ChannelPlanID 21
        case 21: switch (channel)
            {
                case 9: return 0;
                case 10: return 4;
                case 11: return 8;
                case 12: return 12;
                case 13: return 16;
                case 14: return 20;
                case 15: return 24;
                case 16: return 28;
                case 17: return 32;
                case 18: return 36;
                case 19: return 40;
                case 20: return 44;
                case 21: return 0;
                case 22: return 4;
                case 23: return 8;
                case 24: return 12;
                case 25: return 16;
                case 26: return 20;
                case 27: return 24;
                case 28: return 28;
                case 29: return 32;
                case 30: return 36;
                case 31: return 40;
                case 32: return 44;
                case 33: return 0;
                case 34: return 4;
                case 35: return 8;
                case 36: return 48;
                case 37: return 52;
                // Invalid channel for JP, ChannelPlanID 21
                default: return WISUN_DELTA_TABLE_BASEINDEX_INVALID;
            }
        // Channel to BaseIndex mapping for JP, ChannelPlanID 22
        case 22: switch (channel)
            {
                case 4: return 0;
                case 5: return 4;
                case 6: return 8;
                case 7: return 12;
                case 8: return 16;
                case 9: return 20;
                case 10: return 0;
                case 11: return 4;
                case 12: return 8;
                case 13: return 12;
                case 14: return 16;
                case 15: return 20;
                case 16: return 0;
                case 17: return 24;
                // Invalid channel for JP, ChannelPlanID 22
                default: return WISUN_DELTA_TABLE_BASEINDEX_INVALID;
            }
        // Channel to BaseIndex mapping for JP, ChannelPlanID 23
        case 23: switch (channel)
            {
                case 3: return 0;
                case 4: return 4;
                case 5: return 8;
                case 6: return 12;
                case 7: return 0;
                case 8: return 4;
                case 9: return 8;
                case 10: return 12;
                case 11: return 0;
                // Invalid channel for JP, ChannelPlanID 23
                default: return WISUN_DELTA_TABLE_BASEINDEX_INVALID;
            }
        // Invalid ChannelPlanID
        default: return WISUN_DELTA_TABLE_BASEINDEX_INVALID;
    }
}

/*****************************************
 * INTERNAL HELPER FUNCTIONS
 ****************************************/
int16_t get_wisun_channelPlanId_from_channelPlanIdIdx_JP(uint8_t channelPlanIdIdx)
{
    switch (channelPlanIdIdx)
    {
        case 0: return 21;
        case 1: return 22;
        case 2: return 23;
        case 3: return 24;
        default: return WISUN_CHANNELPLANID_INVALID;
    }
}

int16_t get_wisun_channel_from_frequency_JP(uint16_t channelPlanId, uint32_t frequency)
{
    switch (channelPlanId)
    {
        // Frequency to channel mapping for JP, ChannelPlanID 21
        case 21: switch (frequency)
            {
                case 922400: return 9;
                case 922600: return 10;
                case 922800: return 11;
                case 923000: return 12;
                case 923200: return 13;
                case 923400: return 14;
                case 923600: return 15;
                case 923800: return 16;
                case 924000: return 17;
                case 924200: return 18;
                case 924400: return 19;
                case 924600: return 20;
                case 924800: return 21;
                case 925000: return 22;
                case 925200: return 23;
                case 925400: return 24;
                case 925600: return 25;
                case 925800: return 26;
                case 926000: return 27;
                case 926200: return 28;
                case 926400: return 29;
                case 926600: return 30;
                case 926800: return 31;
                case 927000: return 32;
                case 927200: return 33;
                case 927400: return 34;
                case 927600: return 35;
                case 927800: return 36;
                case 928000: return 37;
                // Invalid frequency for JP, ChannelPlanID 21
                default: return WISUN_CHANNEL_INVALID;
            }
        // Frequency to channel mapping for JP, ChannelPlanID 22
        case 22: switch (frequency)
            {
                case 922500: return 4;
                case 922900: return 5;
                case 923300: return 6;
                case 923700: return 7;
                case 924100: return 8;
                case 924500: return 9;
                case 924900: return 10;
                case 925300: return 11;
                case 925700: return 12;
                case 926100: return 13;
                case 926500: return 14;
                case 926900: return 15;
                case 927300: return 16;
                case 927700: return 17;
                // Invalid frequency for JP, ChannelPlanID 22
                default: return WISUN_CHANNEL_INVALID;
            }
        // Frequency to channel mapping for JP, ChannelPlanID 23
        case 23: switch (frequency)
            {
                case 922600: return 3;
                case 923200: return 4;
                case 923800: return 5;
                case 924400: return 6;
                case 925000: return 7;
                case 925600: return 8;
                case 926200: return 9;
                case 926800: return 10;
                case 927400: return 11;
                // Invalid frequency for JP, ChannelPlanID 23
                default: return WISUN_CHANNEL_INVALID;
            }
        // Frequency to channel mapping for JP, ChannelPlanID 24
        case 24: switch (frequency)
            {
                case 922700: return 2;
                case 923500: return 3;
                case 924300: return 4;
                case 925100: return 5;
                case 925900: return 6;
                case 926700: return 7;
                case 927500: return 8;
                // Invalid frequency for JP, ChannelPlanID 24
                default: return WISUN_CHANNEL_INVALID;
            }
        default: return WISUN_CHANNEL_INVALID;
    }
}

uint32_t get_wisun_frequency_from_channel_JP(uint16_t channelPlanId, int16_t channel)
{
    switch (channelPlanId)
    {
        // Frequency to channel mapping for JP, ChannelPlanID 21
        case 21: switch (channel)
            {
                case 9: return 922400;
                case 10: return 922600;
                case 11: return 922800;
                case 12: return 923000;
                case 13: return 923200;
                case 14: return 923400;
                case 15: return 923600;
                case 16: return 923800;
                case 17: return 924000;
                case 18: return 924200;
                case 19: return 924400;
                case 20: return 924600;
                case 21: return 924800;
                case 22: return 925000;
                case 23: return 925200;
                case 24: return 925400;
                case 25: return 925600;
                case 26: return 925800;
                case 27: return 926000;
                case 28: return 926200;
                case 29: return 926400;
                case 30: return 926600;
                case 31: return 926800;
                case 32: return 927000;
                case 33: return 927200;
                case 34: return 927400;
                case 35: return 927600;
                case 36: return 927800;
                case 37: return 928000;
                // Invalid channel for JP, ChannelPlanID 21
                default: return WISUN_FREQ_INVALID;
            }
        // Frequency to channel mapping for JP, ChannelPlanID 22
        case 22: switch (channel)
            {
                case 4: return 922500;
                case 5: return 922900;
                case 6: return 923300;
                case 7: return 923700;
                case 8: return 924100;
                case 9: return 924500;
                case 10: return 924900;
                case 11: return 925300;
                case 12: return 925700;
                case 13: return 926100;
                case 14: return 926500;
                case 15: return 926900;
                case 16: return 927300;
                case 17: return 927700;
                // Invalid channel for JP, ChannelPlanID 22
                default: return WISUN_FREQ_INVALID;
            }
        // Frequency to channel mapping for JP, ChannelPlanID 23
        case 23: switch (channel)
            {
                case 3: return 922600;
                case 4: return 923200;
                case 5: return 923800;
                case 6: return 924400;
                case 7: return 925000;
                case 8: return 925600;
                case 9: return 926200;
                case 10: return 926800;
                case 11: return 927400;
                // Invalid channel for JP, ChannelPlanID 23
                default: return WISUN_FREQ_INVALID;
            }
        // Frequency to channel mapping for JP, ChannelPlanID 24
        case 24: switch (channel)
            {
                case 2: return 922700;
                case 3: return 923500;
                case 4: return 924300;
                case 5: return 925100;
                case 6: return 925900;
                case 7: return 926700;
                case 8: return 927500;
                // Invalid channel for JP, ChannelPlanID 24
                default: return WISUN_FREQ_INVALID;
            }
        default: return WISUN_FREQ_INVALID;
    }
}

