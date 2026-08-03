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

@file transport_spi.h

@brief Platform agnostic SPI controller for the Loki TRX SPI framing protocol

*/

#ifndef SPI_TRANSPORT_H_
#define SPI_TRANSPORT_H_

#ifdef __cplusplus
extern "C" {
#endif

/********************************** Includes **********************************/
/* Standard C Libraries */
#include <stdbool.h>
#include <stdint.h>

/* Module Header Files */
#include "TRX.h"
#include "utility/list/list.h"

//*****************************************************************************
//
//! \addtogroup TRANSPORT
//! \brief Platform agnostic SPI controller for the Loki TRX SPI framing
//!        protocols
//!
//! @details This is a private header file
//! @{
//
//*****************************************************************************

/********************************** Defines ***********************************/

/** Length of a TRX Transaction header going over the SPI interface */
#define SPI_TRANSPORT_HEADER_LEN (sizeof(TRX_TransactionHeader))

/** Length of a TRX Transaction footer going over the SPI interface */
#define SPI_TRANSPORT_CRC_SIZE   (sizeof(TRX_TransactionFooter))

/** Maximum payload length of a TRX transaction */
#define SPI_TRANSPORT_MAX_TLEN   (TRX_SPIBLOCK_SIZE - SPI_TRANSPORT_HEADER_LEN - SPI_TRANSPORT_CRC_SIZE)

/*********************************** Enums ************************************/
/*!
 *  @brief Status codes for SPI Transport functions.
 *
 *  SPI_Transport is reported as a return value for SPI Transport functions.
 */
typedef enum
{
    SPI_Transport_Success  = 0U, //!< Function returned successfully
    SPI_Transport_Failed   = 1U, //!< Function returned unsuccessfully or data
                                 //!< failed to transmit/receive
    SPI_Transport_Canceled = 2U, //!< Data requested for transmit/receive was
                                 //!< canceled
    SPI_Transport_Invalid_Handle = 5U, //!< Invalid handle passed to API
} SPI_Transport_Status;

/********************************** Structs ***********************************/
/*!
 * @brief Metadata regarding requests in a SPI_Transport_Container.trxSpiBlock
 * that aid in the mapping of a TRX_Request_LastStatus to a cmd/stream/config or
 * other operation requested by the application
 */
typedef struct SPI_Transport_Last_Status_Map_t
{
    /** Linked list entry */
    Util_List_Entry entry;
    /** Type of request issued to TRX. Set to Req_INVALID when this entry is unused */
    TRX_RequestType req;
    /** More data bit of the originating request. Applicable only to streams and configs */
    uint8_t md;
    /** Slot, stream, or config id if applicable. Otherwise RESERVED_ID. */
    uint8_t id;
    /** Callback for this operation when applicable */
    uintptr_t cb;
} SPI_Transport_Last_Status_Map;

/*!
 * @brief Object containing information regarding data to the sent or received
 * over the SPI interface
 */
typedef struct SPI_Transport_Container_t
{
    /** Linked list entry */
    Util_List_Entry entry;
    /** Linked list of last status maps for each request in the trxSpiBlock */
    Util_List lastStatusMaps;
    /** The first index of unwritten data in the trxSpiBlock */
    uint8_t index;
    /** The SPI Block that will be sent to TRX */
    TRX_SpiBlock trxSpiBlock;
} SPI_Transport_Container;

/*! @brief Opaque structure used to reference SPI Transport instance */
typedef struct SPI_Transport_Object_t SPI_Transport_Object;

/*! @brief Pointer to SPI_Transport_Object used to reference SPI Transport
 * instance */
typedef SPI_Transport_Object *SPI_Transport_Handle;

/********************************* Prototypes *********************************/
/*!
 * @brief Callback provided from the SPI Transport module to the user after
 * each transaction over the SPI bus with the TRX. It will always be triggered
 * asynchronously fter a call to SPI_Transport_transmit() but may also be
 * triggered by a transaction started due to the TRX INT line being asserted.
 *
 * @note Calling context: HWI, SWI
 *
 * @param[in] pTxSpiContainer  Pointer to the transmitted SPI block that the caller
 *                         must free
 * @param[in] pRxSpiContainer  Pointer to the received SPI block that is freed
 *                         after the callback returns
 * @param[in] status       Status of the SPI transaction
 * @param[in] arg          Optional argument user can specify to be passed
 *                         to each execution of this callback
 */
typedef void (*spiTransportCallback)(SPI_Transport_Container *pTxSpiContainer,
                                     SPI_Transport_Container *pRxSpiContainer,
                                     SPI_Transport_Status status,
                                     uintptr_t arg);

/*!
 * @brief Callback that the SPI Transport layer issues when the CS pin is deasserted.
 *
 * This callback is used by the getTime() API. The user registers a callback with
 * the getTime API, which in turn register it to this callback. It is issued as
 * soon at the chip select line is deasserted at the end of the same transaction
 * for which the user registers the callback.
 *
 * @note Calling context: HWI, SWI
 *
 * @param[in] arg Optional argument user can specify to be passed
 *                to each execution of this callback
 */
typedef void (*spiTransportCSDeassertCallback)(uintptr_t arg);

/**
 * @brief Callback provided by the upper layer to retrieve any pending
 * SPI_Transport_Container intended for transmission.
 *
 * @param[in] arg          Optional argument user can specify to be passed
 *                         to each execution of this callback
 *
 * @return SPI_Transport_Container *, Pointer to a SPI container to transmit. NULL
 *         if no container currently pending
 */
typedef SPI_Transport_Container *(*spiTransportGetNextContainer)(uintptr_t arg);

/*! @brief Parameters used when initializing the SPI transport module. */
typedef struct SPI_Transport_Params_t
{
    uint32_t bitRate; //!< SPI data rate in Hz
    uintptr_t arg; //!< Optional argument to pass to callbacks from this module
    spiTransportCallback pSpiTransportCb; //!< Callback used after each transaction
                                          //!< with the TRX including transactions
                                          //!< triggered by the INT line
    spiTransportCSDeassertCallback pCSDeassertCb; //!< Callback when the
                                                  //!< CS line is deasserted.
    spiTransportGetNextContainer pGetNextSpiContainer; //!< Callback provided
                                        //!< by the upper layer to retrieve
                                        //!< any pending SPI Containers intended
                                        //!< for transmission
} SPI_Transport_Params;

/**************************** Function Definitions ****************************/
/*!
 * @brief Initialize SPI parameters structure to default values
 *
 * @note Calling context: Task, SWI, HWI
 *
 * @param[in] pParams  Pointer to SPI_Transport_Params structure. When NULL,
 *                     this API will do nothing
 *
 * @sa SPI_Transport_Params
 */
extern void SPI_Transport_Params_init(SPI_Transport_Params *pParams);

/*!
 * @brief Performs any initialization and opens the SPI Transport module
 * including registering callbacks and setting defaults. In addition, performs
 * any required initialization of lower level modules. Must be called before
 * using any other SPI Transport Controller APIs.
 *
 * @note Calling context: Task
 *
 * @param[in] pParams Pointer to SPI_Transport_Params structure.
 *
 * @pre Buffer Manager must be initialized with call to Buffer_Manager_init()
 *
 * @sa SPI_Transport_Params
 *
 * @return SPI_Transport_Handle Non-NULL if opened successfully, otherwise NULL
 */
extern SPI_Transport_Handle SPI_Transport_open(const SPI_Transport_Params *pParams);

/*!
 * @brief Close the SPI Transport controller and any lower-level dependencies.
 *
 * @note Calling context: Task
 *
 * @param[in] handle   Pointer to the SPI Transport Module object
 *
 * @return SPI_Transport_Status indicating if the SPI Transport module
 *                              was successfully closed
 */
extern SPI_Transport_Status SPI_Transport_close(SPI_Transport_Handle handle);

/*!
 * @brief Transmits the provided data as-is on the SPI interface.
 *
 * If the transport module is already busy transmitting, receiving, or both,
 * the provided data will be queued for transmission.
 *
 * @note Calling context: Task, HWI, SWI
 *
 * @param[in] handle       Pointer to the SPI Transport Module object
 *
 * @return SPI_Transport_Status indicating successful submission, failed, etc.
 */
extern SPI_Transport_Status SPI_Transport_transmit(SPI_Transport_Handle handle);
//*****************************************************************************
//
//! Close the Doxygen group.
//! @}
//
//*****************************************************************************

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SPI_TRANSPORT_H_
