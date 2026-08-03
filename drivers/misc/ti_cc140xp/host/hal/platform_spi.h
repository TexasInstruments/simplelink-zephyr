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

@file platform_spi.h

@brief Generic SPI interface definition to abstract platform specific
implementations

*/

#ifndef PLATFORM_SPI_H_
#define PLATFORM_SPI_H_

#ifdef __cplusplus
extern "C" {
#endif

/********************************** Includes **********************************/
/* Standard C Libraries */
#include <stdbool.h>
#include <stdint.h>

//*****************************************************************************
//
//! \addtogroup PLATFORM_HAL
//! @{
//
//! \addtogroup PLATFORM_SPI
//! \brief Generic SPI interface definition to abstract platform specific
//!        implementations
//! @{
//
//*****************************************************************************
/*********************************** Enums ************************************/
/*!
 *  @brief Status codes for platform SPI functions.
 *
 *  Plat_SPI_Status is reported as a return value for platform SPI
 *  functions.
 */
typedef enum
{
    Plat_SPI_Success  = 0, //!< Function returned successfully
    Plat_SPI_Failed   = 1, //!< Function returned unsuccessfully or data
                           //!< failed to transmit/receive
    Plat_SPI_Busy     = 2, //!< Service is busy and could not service the
                           //!< operation
    Plat_SPI_Canceled = 3, //!< Data requested for transmit/receive was canceled
    Plat_SPI_Cs_Not_Ready = 4, //!< The CS line wasn't ready to be asserted
    Plat_SPI_Invalid_Handle = 5 //!< Invalid handle passed to API
} Plat_SPI_Status;

/********************************* Prototypes *********************************/
/*!
 * @brief  Indicates that a SPI transaction has completed with the given
 * length. The data received has been written to the pointer indicated by the
 * `pReceiveBuf` argument to the previous call to
 * `Plat_SPI_startTransaction()`.
 *
 * This function is always called at the end of a transaction, even
 * if a null pReceiveBuf is presented to `Plat_SPI_startTransaction()`. In
 * such cases, `receiveBufLen` will be zero.
 *
 * @note Calling context: HWI, SWI
 *
 * @param[in] pTransmitBuf    Value of `pTransmitBuf` from last call to
 *                            `Plat_SPI_startTransaction()`.
 * @param[in] transmitBufLen  Value of `transmitBufLen` from last call to
 *                            `Plat_SPI_startTransaction()`.
 * @param[in] pReceiveBuf     Value of pReceiveBuf from last call to
 *                            `Plat_SPI_startTransaction()`.
 * @param[in] receiveBufLen   Value of receiveBufLen from last call to
 *                            `Plat_SPI_startTransaction()`
 * @param[in] status          Transaction successful, failed, or canceled
 * @param[in] arg             Optional argument user can specify to be passed
 *                            to each execution of this callback
 */
typedef void (*platSpiTransactionCompleteCallback)(uint8_t *pTransmitBuf,
                                                   uint16_t transmitBufLen,
                                                   uint8_t *pReceiveBuf,
                                                   uint16_t receiveBufLen,
                                                   Plat_SPI_Status status,
                                                   uintptr_t arg);

/*!
 * @brief Indicates that the TRX asserted it's interrupt line implying there
 * is data ready to be sent on the POCI line to the host. This cb implies
 * the TRX is ready and the CS line can be asserted using Plat_SPI_assertCs()
 * without returning a Plat_SPI_Cs_Not_Ready status.
 *
 * @note Calling context: HWI, SWI
 *
 * @param[in] arg        Optional argument user can specify to be passed
 *                       to each execution of this callback
 */
typedef void (*platSpiIntAssertedCallback)(uintptr_t arg);

/*!
 * @brief Indicates that the TRX is ready and the CS line can be asserted with
 * a call to Plat_SPI_assertCs()
 *
 * @note Calling context: HWI, SWI
 *
 * @param[in] arg        Optional argument user can specify to be passed
 *                       to each execution of this callback
 */
typedef void (*platSpiTrxReadyCallback)(uintptr_t arg);

/*! @brief Parameters used when initializing the platform SPI module. */
typedef struct Plat_SPI_Params_t
{
    /** SPI data rate in Hz */
    uint32_t bitRate;
    /** Optional argument passed to each callback from this module */
    uintptr_t arg;
    /** Callback indicating a request SPI transaction completed */
    platSpiTransactionCompleteCallback pTransactionCompleteCb;
    /** Callback indicating the TRX INT line was asserted */
    platSpiIntAssertedCallback pIntAssertedCb;
    /** Callback indicating the TRX is ready for SPI transactions */
    platSpiTrxReadyCallback pTrxReadyCallback;
} Plat_SPI_Params;

/*! @brief Opaque structure used to reference Platform SPI instance */
typedef struct Plat_SPI_Object_t Plat_SPI_Object;

/*! @brief Pointer to Plat_SPI_Object used to reference Platform SPI instance */
typedef Plat_SPI_Object *Plat_SPI_Handle;

/**************************** Function Definitions ****************************/
/*!
 * @brief Initialize SPI parameters structure to default values
 *
 * @note Calling context: Task, SWI, HWI
 *
 * @param[in] pParams  Pointer to Plat_SPI_Params structure. When NULL, this API
 *                     will do nothing
 *
 * @sa Plat_SPI_Params
 */
extern void Plat_SPI_Params_init(Plat_SPI_Params *pParams);

/*!
 * @brief Initialize and open the SPI interface including INT, CS, POCI, PICO, and
 * SCLK lines. The POCI line's default value is 0x00 and the interface uses
 * the following SPI settings:
 * - Significant Bit: MSB First
 * - Bits Per Transfer: 8-bits
 * - Clock State: Clock is low when active (CPOL=0)
 * - Clock Phase: Data is valid on clock leading edge (CPHA=0)
 * - Enable Line (CS): Active Low
 *
 * @note Calling context: Task
 *
 * @param[in] pParams Pointer to Plat_SPI_Params structure.
 *
 * @sa Plat_SPI_Params
 *
 * @return Plat_SPI_Handle Non-NULL if opened successfully, otherwise NULL
 */
extern Plat_SPI_Handle Plat_SPI_open(const Plat_SPI_Params *pParams);

/*!
 * @brief Close the SPI interface.
 *
 * @note Calling context: Task
 *
 * @param[in] handle Pointer to the Platform SPI module object
 *
 * @return Plat_SPI_Status indicating if SPI interface was successfully closed
 */
extern Plat_SPI_Status Plat_SPI_close(Plat_SPI_Handle handle);

/*!
 * @brief Assert the CS line to prepare for a SPI transaction.
 *
 * @note Calling context: Task, HWI, SWI
 *
 * @param[in] handle Pointer to the Platform SPI module object
 *
 * @return Plat_SPI_Cs_Not_Ready if the CS line is being guarded, else Plat_SPI_Success
 */
extern Plat_SPI_Status Plat_SPI_assertCs(Plat_SPI_Handle handle);

/*!
 * @brief Deassert the CS line to end a SPI transaction.
 *
 * @note Calling context: Task, HWI, SWI
 *
 * @param[in] handle Pointer to the Platform SPI module object
 *
 * @return Plat_SPI_Status indicating if CS line was successfully deasserted
 */
extern Plat_SPI_Status Plat_SPI_deassertCs(Plat_SPI_Handle handle);

/*!
 * @brief Returns true when TRX is ready for SPI communication
 *
 * @param handle
 * @return true when TRX is ready for SPI communication
 * @return false Otherwise
 */
extern bool Plat_SPI_isTrxReady(Plat_SPI_Handle handle);

/*!
 * @brief Start a SPI transaction. Data pointers MUST remain
 * valid and the data pointed to UNMODIFIED until the transaction complete
 * callback is called by the SPI platform driver, or until after the next call
 * to Plat_SPI_startTransaction().
 *
 * All transactions are symmetrical meaning that when the provided
 * `receiveBufLen` is greater than the provided `transmitBufLen`, the PICO pin
 * will be set high after `transmitBufLen` bytes and until a new `pTransmitBuf`
 * is provided. This is the functional equivalent of padding the end of
 * `pTransmitBuf` with `0x00` bytes out to the length of the receive buffer.
 *
 * Conversely, once `receiveBufLen` bytes of `pReceiveBuf` have been clocked in
 * from POCI, all subsequent values from the POCI pin are ignored until a new
 * `pReceiveBuf` is provided.
 *
 * @note The implementation of this function must verify the POCI line has been
 * driven low by the TRX before attempting to start a transaction.
 *
 * @note Calling context: Task, HWI, SWI
 *
 * @param[in] handle          Pointer to the Platform SPI module object
 * @param[in] pTransmitBuf    Pointer to a buffer of data to transmit. When
 *                            NULL, 0x00 will be put on the PICO line while
 *                            receiving into pReceiveBuf.
 * @param[in] transmitBufLen  The buffer length pointed to by pTransmitBuf.
 * @param[in] pReceiveBuf     Pointer to a buffer to receive data into. When
 *                            NULL, all received bytes will be ignored.
 * @param[in] receiveBufLen   The buffer length pointed to by pReceiveBuf
 *
 * @return Plat_SPI_Status indicating success, failure, or busy.
 */
extern Plat_SPI_Status Plat_SPI_startTransaction(Plat_SPI_Handle handle,
                                                 uint8_t *pTransmitBuf,
                                                 uint16_t transmitBufLen,
                                                 uint8_t *pReceiveBuf,
                                                 uint16_t receiveBufLen);

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
} // extern "C"
#endif

#endif // PLATFORM_SPI_H_
