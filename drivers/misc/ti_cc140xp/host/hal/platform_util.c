/******************************************************************************
 Group: CMCU LPRF
 Target Device: cc13xx_cc26xx

 ******************************************************************************
 
 Copyright (c) 2021-2026, Texas Instruments Incorporated
 All rights reserved.

 IMPORTANT: Your use of this Software is limited to those specific rights
 granted under the terms of a software license agreement between the user
 who downloaded the software, his/her employer (which must be your employer)
 and Texas Instruments Incorporated (the "License"). You may not use this
 Software unless you agree to abide by the terms of the License. The License
 limits your use, and you acknowledge, that the Software may not be modified,
 copied or distributed unless embedded on a Texas Instruments microcontroller
 or used solely and exclusively in conjunction with a Texas Instruments radio
 frequency transceiver, which is integrated into your product. Other than for
 the foregoing purpose, you may not use, reproduce, copy, prepare derivative
 works of, modify, distribute, perform, display or sell this Software and/or
 its documentation for any purpose.

 YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
 PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
 NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
 TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
 NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
 LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
 INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
 OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
 OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
 (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

 Should you have any questions regarding your right to use this Software,
 contact Texas Instruments Incorporated at www.TI.com.

 ******************************************************************************
 
 
 *****************************************************************************/

/*!

@file platform_util.c

@brief Implements platform specific utility functions for Zephyr.

 */

/********************************** Includes **********************************/

/* Zephyr Headers */
#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/util.h>

/* Module Header Files */
#include "hal/platform_util.h"

/********************************** Defines ***********************************/

/*************************** Variable declarations ****************************/

/********************** Static Function Implementations ***********************/

/********************** Extern Function Implementations ***********************/

/*
 * This function begins a critical section.
 *
 * Uses Zephyr's irq_lock() to disable interrupts and save the key.
 *
 * Input/Return: arg Pointer to store the interrupt key for later restore
 */
void Plat_Util_startCriticalSection(uintptr_t arg)
{
    *(unsigned int *)arg = irq_lock();
}

/*
 * This function ends a critical section.
 *
 * Uses Zephyr's irq_unlock() to restore interrupts using the saved key.
 *
 * Input/Return: arg Pointer containing the interrupt key from startCriticalSection
 */
void Plat_Util_endCriticalSection(uintptr_t arg)
{
    irq_unlock(*(unsigned int *)arg);
}

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
uint16_t Plat_Util_computeCRC(const uint8_t *pBuffer, uint16_t length)
{
    return crc16_itu_t(0xFFFF, pBuffer, length);
}

/*!
 * @brief Block the CPU for the provided duration in microseconds
 *
 * Uses Zephyr's k_busy_wait() to implement a busy-wait delay.
 * This is a blocking delay that uses CPU cycles.
 *
 * @param[in] delayUs  The delay in microseconds
 */
void Plat_Util_blockCPU(uint32_t delayUs)
{
    k_busy_wait(delayUs);
}
