
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


#include "wisun_delta_tables_EU1.h"

// DeltaTable for EU1, ChannelPlanID 32
// Size in bytes: 20
const uint32_t wisun_delta_table_EU1_from_ChannelPlanID32[] =
{
    0x00000004,    //  ConfigSegment(length=4, feature_mask=0, inverted=False, compound=False)
    0x00309002,    //  ConfigRegion(length=3, operation=<ConfigRegionOperation.Par_Reference_32bit: 9>, address=48)
    0x00000064,    //  ConfigValue32(value32=100)
    0x00ff0001,    //  ConfigValue32(value32=16711681)
    0x000400fe     //  ConfigValue32(value32=262398)
};

// DeltaTable for EU1, ChannelPlanID 33
// Size in bytes: 12
const uint32_t wisun_delta_table_EU1_from_ChannelPlanID33[] =
{
    0x00000002,    //  ConfigSegment(length=2, feature_mask=0, inverted=False, compound=False)
    0x00309000,    //  ConfigRegion(length=1, operation=<ConfigRegionOperation.Par_Reference_32bit: 9>, address=48)
    0x00000064     //  ConfigValue32(value32=100)
};

/*****************************************
 * HOST FUNCTIONS
 ****************************************/
const uint32_t* get_wisun_delta_table_EU1(uint16_t channelPlanId)
{
    switch (channelPlanId)
    {
        case 32: return wisun_delta_table_EU1_from_ChannelPlanID32;
        case 33: return wisun_delta_table_EU1_from_ChannelPlanID33;
        default: return 0;
    }
}

int16_t get_wisun_delta_table_baseindex_EU1(uint16_t channelPlanId, uint16_t channel)
{
    switch (channelPlanId)
    {
        // Channel to BaseIndex mapping for EU1, ChannelPlanID 32
        case 32: switch (channel)
            {
                case 0: return 0;
                case 1: return 2;
                case 2: return 0;
                case 3: return 2;
                case 4: return 0;
                case 5: return 2;
                case 6: return 0;
                case 7: return 2;
                case 8: return 0;
                case 9: return 2;
                case 10: return 0;
                case 11: return 2;
                case 12: return 0;
                case 13: return 2;
                case 14: return 0;
                case 15: return 2;
                case 16: return 0;
                case 17: return 2;
                case 18: return 0;
                case 19: return 2;
                case 20: return 0;
                case 21: return 2;
                case 22: return 0;
                case 23: return 2;
                case 24: return 0;
                case 25: return 2;
                case 26: return 0;
                case 27: return 2;
                case 28: return 0;
                case 29: return 2;
                case 30: return 0;
                case 31: return 2;
                case 32: return 0;
                case 33: return 2;
                case 34: return 0;
                case 35: return 2;
                case 36: return 0;
                case 37: return 2;
                case 38: return 0;
                case 39: return 2;
                case 40: return 0;
                case 41: return 2;
                case 42: return 0;
                case 43: return 2;
                case 44: return 0;
                case 45: return 2;
                case 46: return 0;
                case 47: return 2;
                case 48: return 0;
                case 49: return 2;
                case 50: return 0;
                case 51: return 2;
                case 52: return 0;
                case 53: return 4;
                case 54: return 6;
                case 57: return 2;
                case 58: return 0;
                case 59: return 4;
                case 60: return 6;
                case 64: return 8;
                case 67: return 2;
                case 68: return 0;
                // Invalid channel for EU1, ChannelPlanID 32
                default: return WISUN_DELTA_TABLE_BASEINDEX_INVALID;
            }
        // Channel to BaseIndex mapping for EU1, ChannelPlanID 33
        case 33: switch (channel)
            {
                case 0: return 0;
                case 1: return 0;
                case 2: return 0;
                case 3: return 0;
                case 4: return 0;
                case 5: return 0;
                case 6: return 0;
                case 7: return 0;
                case 8: return 0;
                case 9: return 0;
                case 10: return 0;
                case 11: return 0;
                case 12: return 0;
                case 13: return 0;
                case 14: return 0;
                case 15: return 0;
                case 16: return 0;
                case 17: return 0;
                case 18: return 0;
                case 19: return 0;
                case 20: return 0;
                case 21: return 0;
                case 22: return 0;
                case 23: return 0;
                case 24: return 0;
                case 25: return 0;
                case 26: return 0;
                case 29: return 0;
                case 34: return 0;
                // Invalid channel for EU1, ChannelPlanID 33
                default: return WISUN_DELTA_TABLE_BASEINDEX_INVALID;
            }
        // Invalid ChannelPlanID
        default: return WISUN_DELTA_TABLE_BASEINDEX_INVALID;
    }
}

/*****************************************
 * INTERNAL HELPER FUNCTIONS
 ****************************************/
int16_t get_wisun_channelPlanId_from_channelPlanIdIdx_EU1(uint8_t channelPlanIdIdx)
{
    switch (channelPlanIdIdx)
    {
        case 0: return 32;
        case 1: return 33;
        default: return WISUN_CHANNELPLANID_INVALID;
    }
}

int16_t get_wisun_channel_from_frequency_EU1(uint16_t channelPlanId, uint32_t frequency)
{
    switch (channelPlanId)
    {
        // Frequency to channel mapping for EU1, ChannelPlanID 32
        case 32: switch (frequency)
            {
                case 863100: return 0;
                case 863200: return 1;
                case 863300: return 2;
                case 863400: return 3;
                case 863500: return 4;
                case 863600: return 5;
                case 863700: return 6;
                case 863800: return 7;
                case 863900: return 8;
                case 864000: return 9;
                case 864100: return 10;
                case 864200: return 11;
                case 864300: return 12;
                case 864400: return 13;
                case 864500: return 14;
                case 864600: return 15;
                case 864700: return 16;
                case 864800: return 17;
                case 864900: return 18;
                case 865000: return 19;
                case 865100: return 20;
                case 865200: return 21;
                case 865300: return 22;
                case 865400: return 23;
                case 865500: return 24;
                case 865600: return 25;
                case 865700: return 26;
                case 865800: return 27;
                case 865900: return 28;
                case 866000: return 29;
                case 866100: return 30;
                case 866200: return 31;
                case 866300: return 32;
                case 866400: return 33;
                case 866500: return 34;
                case 866600: return 35;
                case 866700: return 36;
                case 866800: return 37;
                case 866900: return 38;
                case 867000: return 39;
                case 867100: return 40;
                case 867200: return 41;
                case 867300: return 42;
                case 867400: return 43;
                case 867500: return 44;
                case 867600: return 45;
                case 867700: return 46;
                case 867800: return 47;
                case 867900: return 48;
                case 868000: return 49;
                case 868100: return 50;
                case 868200: return 51;
                case 868300: return 52;
                case 868400: return 53;
                case 868500: return 54;
                case 868800: return 57;
                case 868900: return 58;
                case 869000: return 59;
                case 869100: return 60;
                case 869500: return 64;
                case 869800: return 67;
                case 869900: return 68;
                // Invalid frequency for EU1, ChannelPlanID 32
                default: return WISUN_CHANNEL_INVALID;
            }
        // Frequency to channel mapping for EU1, ChannelPlanID 33
        case 33: switch (frequency)
            {
                case 863100: return 0;
                case 863300: return 1;
                case 863500: return 2;
                case 863700: return 3;
                case 863900: return 4;
                case 864100: return 5;
                case 864300: return 6;
                case 864500: return 7;
                case 864700: return 8;
                case 864900: return 9;
                case 865100: return 10;
                case 865300: return 11;
                case 865500: return 12;
                case 865700: return 13;
                case 865900: return 14;
                case 866100: return 15;
                case 866300: return 16;
                case 866500: return 17;
                case 866700: return 18;
                case 866900: return 19;
                case 867100: return 20;
                case 867300: return 21;
                case 867500: return 22;
                case 867700: return 23;
                case 867900: return 24;
                case 868100: return 25;
                case 868300: return 26;
                case 868900: return 29;
                case 869900: return 34;
                // Invalid frequency for EU1, ChannelPlanID 33
                default: return WISUN_CHANNEL_INVALID;
            }
        default: return WISUN_CHANNEL_INVALID;
    }
}

uint32_t get_wisun_frequency_from_channel_EU1(uint16_t channelPlanId, int16_t channel)
{
    switch (channelPlanId)
    {
        // Frequency to channel mapping for EU1, ChannelPlanID 32
        case 32: switch (channel)
            {
                case 0: return 863100;
                case 1: return 863200;
                case 2: return 863300;
                case 3: return 863400;
                case 4: return 863500;
                case 5: return 863600;
                case 6: return 863700;
                case 7: return 863800;
                case 8: return 863900;
                case 9: return 864000;
                case 10: return 864100;
                case 11: return 864200;
                case 12: return 864300;
                case 13: return 864400;
                case 14: return 864500;
                case 15: return 864600;
                case 16: return 864700;
                case 17: return 864800;
                case 18: return 864900;
                case 19: return 865000;
                case 20: return 865100;
                case 21: return 865200;
                case 22: return 865300;
                case 23: return 865400;
                case 24: return 865500;
                case 25: return 865600;
                case 26: return 865700;
                case 27: return 865800;
                case 28: return 865900;
                case 29: return 866000;
                case 30: return 866100;
                case 31: return 866200;
                case 32: return 866300;
                case 33: return 866400;
                case 34: return 866500;
                case 35: return 866600;
                case 36: return 866700;
                case 37: return 866800;
                case 38: return 866900;
                case 39: return 867000;
                case 40: return 867100;
                case 41: return 867200;
                case 42: return 867300;
                case 43: return 867400;
                case 44: return 867500;
                case 45: return 867600;
                case 46: return 867700;
                case 47: return 867800;
                case 48: return 867900;
                case 49: return 868000;
                case 50: return 868100;
                case 51: return 868200;
                case 52: return 868300;
                case 53: return 868400;
                case 54: return 868500;
                case 57: return 868800;
                case 58: return 868900;
                case 59: return 869000;
                case 60: return 869100;
                case 64: return 869500;
                case 67: return 869800;
                case 68: return 869900;
                // Invalid channel for EU1, ChannelPlanID 32
                default: return WISUN_FREQ_INVALID;
            }
        // Frequency to channel mapping for EU1, ChannelPlanID 33
        case 33: switch (channel)
            {
                case 0: return 863100;
                case 1: return 863300;
                case 2: return 863500;
                case 3: return 863700;
                case 4: return 863900;
                case 5: return 864100;
                case 6: return 864300;
                case 7: return 864500;
                case 8: return 864700;
                case 9: return 864900;
                case 10: return 865100;
                case 11: return 865300;
                case 12: return 865500;
                case 13: return 865700;
                case 14: return 865900;
                case 15: return 866100;
                case 16: return 866300;
                case 17: return 866500;
                case 18: return 866700;
                case 19: return 866900;
                case 20: return 867100;
                case 21: return 867300;
                case 22: return 867500;
                case 23: return 867700;
                case 24: return 867900;
                case 25: return 868100;
                case 26: return 868300;
                case 29: return 868900;
                case 34: return 869900;
                // Invalid channel for EU1, ChannelPlanID 33
                default: return WISUN_FREQ_INVALID;
            }
        default: return WISUN_FREQ_INVALID;
    }
}

