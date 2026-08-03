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
#ifndef RADIO_TRX_MDR_H
#define RADIO_TRX_MDR_H

/* Include Files */
#include <stdint.h>
#include <ti_cc140xp/ti_cc140xp.h>
/* included via ti_cc140xp/ti_cc140xp.h */

/**
 * Returns the index of the MDR Phy that was received on.
 *
 * @param[in] phyIndex Current base phy index into the phy table.
 * @param[in] region Current region that the device is operating in.
 * @param[in] payload Payload received after MDR RX TestMode.
 *
 * @return Index of the MDR phy that MDR Test received on.
 */
extern uint8_t extractPhyIndex(uint8_t phyIndex, uint8_t region,
                               uint8_t *payload);

/**
 * Returns the index of the MDR Wisun Phy map.
 *
 * @param[in] region Current region that the device is operating in.
 * @param[in] modem Modem of the base phy that the device is set to.
 * @param[in] optionMask OptionMask of the base phy the device is set to.
 *
 * @return Index of the MDR Wisun Phy Map.
 */
extern uint8_t getMappingTableIdx(uint8_t region, uint8_t modem,
                                  uint8_t optionMask);

/**
 * Returns the Delta Config Array for MDR Frequency hopping on RX.
 *
 * @param[in] region Current region that the device is operating in.
 * @param[in] phyIndex Current base phy index into the phy table.
 *
 * @return Delta Config Array for MDR Frequency hopping on RX.
 */
extern const uint32_t* getDeltaConfig(uint8_t region, uint8_t phyIndex);

/**
 * Returns the Base Index for a specific Channel Plan when using MDR Frequency
 * hopping on RX.
 *
 * @param[in] region Current region that the device is operating in.
 * @param[in] phyIndex Current base phy index into the phy table.
 * @param[in] frequency Current frequency the base phy is set to.
 *
 * @return Base Index for a specific Channel Plan when using MDR Frequency
 * hopping on RX.
 */
extern int16_t getDeltaBaseIndex(uint8_t region, uint8_t phyIndex,
                                 uint32_t frequency);

/**
 * Configures the CS command with proper region settings for MDR.
 *
 * @param[in] cmd CS Command to configure.
 * @param[in] region Current region that the device is operating in.
 * @param[in] phyIndex Current base phy index into the phy table.
 */
extern void composeMdrCs(TRX_Request_CommandStore *cmd, uint8_t region,
                         uint8_t phyIndex);

/**
 * Configures the MDR PPDU TX and returns the length of the PPDU MDR Packet.
 *
 * @param[in] tx TX Command that will contain the PPDU Payload.
 * @param[in] mdrTx TX Command that will contain the MDR new phy Payload.
 * @param[in] payload Payload to be configured and attached to the PPDU MDR
 * packet.
 * @param[in] region Current region that the device is operating in.
 * @param[in] phyIndex Current base phy index into the phy table.
 * @param[in] phyIndex Current base phy index into the phy table.
 *
 * @return Length of the PPDU MDR Packet.
 */
extern int composeMdrPpdu(TRX_Request_CommandStore *tx, TRX_Request_CommandStore *mdrTx,
                          uint8_t *payload, uint8_t region,
                          uint8_t phyIndex, uint8_t phyIndex2);

/**
 * Configures the MDR Payload TX and returns the length of the MDR payload
 * Packet.
 *
 * @param[in] mdrTx TX Command that will contain the MDR new phy Payload.
 * @param[in] dataLength Length of the data to be attached to the MDR Payload
 * TX.
 * @param[in] payload Payload to be configured and attached to the Payload TX.
 * @param[in] region Current region that the device is operating in.
 * @param[in] phyIndex Current base phy index into the phy table.
 *
 * @return Length of the MDR Payload Packet.
 */
extern int composeMdrPayload(TRX_Request_CommandStore *mdrTx, uint32_t dataLength,
                             uint8_t *payload, uint8_t region, uint8_t phyIndex);

/**
 * Returns the frequency from a set Wisun MDR Channel based on region and
 * channel plan.
 *
 * @param[in] region Current region that the device is operating in.
 * @param[in] phyIndex Current base phy index into the phy table.
 * @param[in] channel Channel that is to be converted to frequency.
 *
 * @return Frequency of the MDR Channel based on channel plan and region.
 */
extern uint32_t getFrequencyFromChannel(uint8_t region, uint8_t phyIndex,
                                        int16_t channel);

/**
 * Returns the delta frequency that is the closest on the new PHY
 *
 * @param[in] region Current region that the device is operating in.
 * @param[in] phyIndex Next MDR base phy index into the phy table.
 * @param[in] frequency Current Frequency that the base phy is using
 *
 * @return Delta Frequency of the MDR Channel based on channel plan and region.
 */
extern uint32_t getNextDeltaFrequency(uint8_t region, uint8_t phyIndex,
                                      uint32_t frequency);

#endif //RADIO_TRX_MDR_H