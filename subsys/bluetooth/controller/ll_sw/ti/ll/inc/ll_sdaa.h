/******************************************************************************

 @file  ll_sdaa.h

 @brief This file contains the SDAA (Selective Detect And Avoid) moudule,
        This module is responsible for monitoring and limiting TX consumption
        per channel, the module will be activated by SDAA_ENABLE define

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2009 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*********************************************************************
 *
 * WARNING!!!
 *
 * THE API'S FOUND IN THIS FILE ARE FOR INTERNAL STACK USE ONLY!
 * FUNCTIONS SHOULD NOT BE CALLED DIRECTLY FROM APPLICATIONS, AND ANY
 * CALLS TO THESE FUNCTIONS FROM OUTSIDE OF THE STACK MAY RESULT IN
 * UNEXPECTED BEHAVIOR.
 *
 */

#ifndef LL_SDAA_H
#define LL_SDAA_H

#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * CONSTANTS
 */
//*****************************************************************************
// INCLUDES
//*****************************************************************************

#include "bcomdef.h"

//*****************************************************************************
// MACROS
//*****************************************************************************

//*****************************************************************************
// CONSTANTS
//*****************************************************************************


//! @brief Default value for gSdaaInternalDB.channelInSample when
//!        there isn't channel in ample process.
//!
#define LL_SDAA_NONE_ACTIVE_CHANNEL   0xFF

//*****************************************************************************
// TYPEDEFS
//*****************************************************************************

//! @brief Channel status enum, store in DB 2 bit per channel must consist up
//!        to 4 state
//!
typedef enum
{
    //! Default value channel is clear to transmit data.
    //!
    SDAA_CH_CLEAR,

    //! Data was sent on this channel is an overload
    //!
    SDAA_CH_OVERLOAD,

    //! The channel sampled and was busy therefore blocked user selected time
    //!
    SDAA_CH_BLOCK,

    //! Error state for a channel that does not exist
    //!
    SDAA_CH_INVALID,

} sdaaChState_e;

//*****************************************************************************
// LOCAL VARIABLES
//*****************************************************************************

//*****************************************************************************
// GLOBAL VARIABLES
//*****************************************************************************

//*****************************************************************************
// API FUNCTIONS
//*****************************************************************************

//*****************************************************************************
//! @fn    LL_SDAA_Init
//
//! @brief This API Initializes the SDAA module, with input configurations
//         from sysconfig/opt file/pre defined
//
//*****************************************************************************
extern void LL_SDAA_Init();

//*****************************************************************************
//! @fn    LL_SDAA_RecordTxUsage
//
//! @brief This API is divided into 3 parts:
//!
//!            1. Call to the function that handles the observation period,
//!               if it is over, set new obs period and clear all databases.
//!            2. Calculate and record TX usage based on input variables.
//!            3. Call to the function that handles usage exception
//!               if necessary.
//
//! @param numOfBytes - The number of bytes sent over the air,
//!                     0 for empty packet.
//
//!        phyType    - phyType is affected by a convert table between
//!                     the sending rate and air time.
//
//!        txPower    - txPower affectes the usage by the convert ratio table,
//!                     the weaker the power, the smaller the ratio.
//
//!        channel    - The data channel to which the usage will be added.
//
//*****************************************************************************
extern void LL_SDAA_RecordTxUsage(uint16 numOfBytes, uint8 phyType,
                                  uint8 txPower, uint8 channel);

//*****************************************************************************
//! @fn    LL_SDAA_AddDwtRecord
//
//! @brief This API add Dwt Record to the specific task
//
//! @param dwt   - DwellTime in rat ticks
//
//!        index - Connection handle ID or Task
//
//*****************************************************************************
extern void LL_SDAA_AddDwtRecord(uint32 dwT, uint8 task, uint8 index);


//*****************************************************************************
//! @fn    LL_SDAA_SampleRXWindow
//
//! @brief This API used to sample the RSSI values when the RX window is open.
//!        RSSI values equal to RF_GET_RSSI_ERROR_VAL will be count as
//!        invalid samples.
//!        RSSI values greater than RSSI Threshold will be count as
//!        noisy samples.
//!        the function determine the channel state according to
//!        the counters (invalid samples/ noisy samples) and update
//!        the channel state in SDAA DB's.
//
//*****************************************************************************
extern void LL_SDAA_SampleRXWindow(void);

//*****************************************************************************
//! @fn    LL_SDAA_GetChannelState
//
//! @brief This API is used to get the channel state from SDAA DB
//
//! @param channel - The channel on which the data will be received
//
//! @return sdaaChState_e - SDAA_CH_CLEAR
//!                         SDAA_CH_OVERLOAD
//!                         SDAA_CH_BLOCK
//!                         SDAA_CH_INVALID
//
//*****************************************************************************
extern sdaaChState_e LL_SDAA_GetChannelState(uint8 channel);

//*****************************************************************************
//! @fn    LL_SDAA_GetRXWindowDuration
//
//! @brief This API used to get the RX window duration in rats.
//
//! @return uint16 RXWindowDuration
//
//*****************************************************************************
extern uint16 LL_SDAA_GetRXWindowDuration(void);

//*****************************************************************************
//! @fn    LL_SDAA_SetChannelInSample
//
//! @brief This API used to set the channel which in sample mode.
//
//! @param channel - channel which in sample mode
//
//*****************************************************************************
extern void LL_SDAA_SetChannelInSample(uint8 channel);

//*****************************************************************************
//! @fn    LL_SDAA_GetTimeOutOfBlockedState
//
//! @brief This API used to get the time at which the elapsed time during which
//!        the channel state is block.
//
//! @param channel - The block channel.
//
//! @return uint32 time.
//
//*****************************************************************************
extern uint32 LL_SDAA_GetTimeOutOfBlockedState(uint8 channel);

#ifdef __cplusplus
}
#endif

#endif /* LL_SDAA_H */
