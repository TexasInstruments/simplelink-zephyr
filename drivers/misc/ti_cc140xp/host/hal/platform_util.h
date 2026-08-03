/******************************************************************************
 Group: CMCU LPRF
 Target Device: cc13xx_cc26xx

 ******************************************************************************
 
 Copyright (c) 2021-2026, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************
 
 
 *****************************************************************************/

/*!

@file platform_util.h

@brief Generic utility interface to abstract platform specific implementations

*/

#ifndef PLATFORM_UTIL_H_
#define PLATFORM_UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

/********************************** Includes **********************************/
/* Standard C Libraries */
#include <stdint.h>

//*****************************************************************************
//
//! \addtogroup PLATFORM_HAL
//! @{
//
//! \addtogroup PLATFORM_UTIL
//! \brief Generic utility interface to abstract platform specific
//!        implementations
//! @{
//
//*****************************************************************************

/********************************** Defines ***********************************/

/*********************************** Enums ************************************/

/********************************** Typedefs **********************************/

/********************************** Structs ***********************************/

/***************************** Extern Prototypes ******************************/

/**
 * This function begins a critical section.
 *
 * Device specific code to enable the critical section
 * must be substituted in the body of the function.
 *
 * @param[in, out] arg Pointer to a unspecified argument for this function
 */
extern void Plat_Util_startCriticalSection(uintptr_t arg);

/**
 * This function ends a critical section.
 *
 * Device specific code to enable the critical section
 * must be substituted in the body of the function.
 *
 * @param[in, out] arg Pointer to a unspecified argument for this function
 */
extern void Plat_Util_endCriticalSection(uintptr_t arg);

/*!
 * @brief Calculate 16 bit CRC-16 CCITT-FALSE
 *
 * Polynomial: 0x1021
 * Starting value: 0xFFFF
 * Input: Not reflected
 * Output: Not reflected
 * Final Xor Value: 0x0
 *
 * @param[in] pBuffer Pointer to buffer with message to calculate CRC
 * @param[in] length  Length of message
 *
 * @return 16-bit CRC calculated over provided buffer
 */
extern uint16_t Plat_Util_computeCRC(const uint8_t *pBuffer,
                                                           uint16_t length);

/*!
 * @brief Block the CPU for the provided duration in microseconds
 *
 * @param[in] delayUs  The delay in microseconds
 */
extern void Plat_Util_blockCPU(uint32_t delayUs);

//*****************************************************************************
//
//! Close the Doxygen group.
//! @}
//
//! Close the Doxygen group.
//! @}
//
//*****************************************************************************

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_UTIL_H_
