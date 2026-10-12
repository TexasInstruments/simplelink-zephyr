
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


#ifndef WISUN_DELTA_TABLES_JP_H
#define WISUN_DELTA_TABLES_JP_H

#include <stdint.h>

// DeltaTable for JP, ChannelPlanID 21
extern const uint32_t wisun_delta_table_JP_from_ChannelPlanID21[];

// DeltaTable for JP, ChannelPlanID 22
extern const uint32_t wisun_delta_table_JP_from_ChannelPlanID22[];

// DeltaTable for JP, ChannelPlanID 23
extern const uint32_t wisun_delta_table_JP_from_ChannelPlanID23[];

/*****************************************
 * HOST FUNCTIONS
 ****************************************/

#define WISUN_DELTA_TABLE_BASEINDEX_INVALID -1

const uint32_t* get_wisun_delta_table_JP(uint16_t channelPlanId);
int16_t get_wisun_delta_table_baseindex_JP(uint16_t channelPlanId, uint16_t channel);

/*****************************************
 * INTERNAL HELPER FUNCTIONS
 ****************************************/

#define WISUN_CHANNELPLANID_INVALID -1
#define WISUN_CHANNEL_INVALID -1

#define WISUN_FREQ_INVALID 0xFFFFFFFF

int16_t get_wisun_channelPlanId_from_channelPlanIdIdx_JP(uint8_t channelPlanIdIdx);
int16_t get_wisun_channel_from_frequency_JP(uint16_t channelPlanId, uint32_t frequency);
uint32_t get_wisun_frequency_from_channel_JP(uint16_t channelPlanId, int16_t channel);
#endif // WISUN_DELTA_TABLES_JP_H

