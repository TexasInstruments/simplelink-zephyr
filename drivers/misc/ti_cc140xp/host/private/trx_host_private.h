/******************************************************************************
 Group: CMCU LPRF
 Target Device: cc13xx_cc26xx

 ******************************************************************************
 
 Copyright (c) 2023-2026, Texas Instruments Incorporated
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

#ifndef TRX_HOST_PRIVATE_H_
#define TRX_HOST_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

/********************************** Includes **********************************/

/* Standard C Libraries */
#include <stdint.h>
#include <stdbool.h>

/* Module Header Files */
#include "TRX.h"
#include "trx_host.h"
#include "utility/list/list.h"
#include "transport/spi_transport.h"

//*****************************************************************************
//
//! \addtogroup TRX_HOST_PRIVATE
//! \brief Private definitions internal to the host driver
//!
//! @{
//
//*****************************************************************************

/********************************** Defines ***********************************/

/** Transaction Header: tid_sent maximum value */
#define TRN_HDR_TID_TX_MAX_VALUE  (0x10)

/** Maximum amount of data that can fit in a Transaction containing only a TRX_Request_StreamStore */
#define MAX_STREAM_DATA_CHUNK (SPI_TRANSPORT_MAX_TLEN - sizeof(TRX_Request_StreamStore))

/**
 * Maximum amount of data that can fit in a Transaction containing only a TRX_Request_ConfigStore
 * Note that this is not aligned to a 4-byte boundary as required by the TRX
 */
#define MAX_CONFIG_DATA_CHUNK_NOT_ALIGNED  (SPI_TRANSPORT_MAX_TLEN - sizeof(TRX_Request_ConfigStore))

/** Maximum number of SPI Blocks allocated for TX operations */
#define MAX_HOST_TX_SPIBLOCKS (12U)

/** Maximum number of SPI Blocks on the TRX */
#define MAX_TRX_SPIBLOCKS (0x0FU)

/** Bitmask that when set, indicates the TRX Host module is reset */
#define TRX_HOST_RESET         (0b00000000)

/** Bitmask that when set, indicates the TRX Host module is open */
#define TRX_HOST_OPEN_MASK     (0b00000001)

/** Bitmask that when set, indicates the TRX Host module is open */
#define TRX_HOST_PENDING_MASK  (0b00000010)

/*********************************** Macros ***********************************/

/** Evaluates to the 32-bit boundary closest to len */
#define CLOSEST_WORD_BOUNDARY_BYTES(len) ((len + 3) & ~0x03)

/** Evaluates to the amount of padding required to increment to a  32-bit boundary */
#define PADDING_REQUIRED(len) (CLOSEST_WORD_BOUNDARY_BYTES(len) - len)

/** Macro for checking if the TRX Host module is open */
#define TRX_HOST_IS_OPEN(handle)      (handle->status & TRX_HOST_OPEN_MASK)

/** Macro for checking if the TRX Host module is pending */
#define TRX_HOST_IS_PENDING(handle)   (handle->status & TRX_HOST_PENDING_MASK)

/** Macro for setting the TRX Host module status to open */
#define TRX_HOST_SET_OPEN(handle)     (handle->status |= TRX_HOST_OPEN_MASK)

/** Macro for setting the TRX Host module status to pending */
#define TRX_HOST_SET_PENDING(handle)  (handle->status |= TRX_HOST_PENDING_MASK)

/** Macro for setting the TRX Host module status to closed */
#define TRX_HOST_SET_CLOSED(handle)   (handle->status &= ~(TRX_HOST_OPEN_MASK))

/** Macro for freeing any TRX Host function pending on a response from the TRX */
#define TRX_HOST_POST(handle)         (handle->status &= ~(TRX_HOST_PENDING_MASK))

/** Maximum amount of data that can fit in a Transaction containing only a TRX_Request_ConfigStore and aligned to a 4-byte boundary*/
#define MAX_CONFIG_DATA_CHUNK (SPI_TRANSPORT_MAX_TLEN - sizeof(TRX_Request_ConfigStore) - (4U - PADDING_REQUIRED(MAX_CONFIG_DATA_CHUNK_NOT_ALIGNED)))

/*********************************** Enums ************************************/

/** Enumerator defining a type of TRX_Host_Stream */
typedef enum TRX_Stream_Type_t
{
    TRX_Stream_Tx = 0,      //!< TX Stream
    TRX_Stream_Rx = 1,      //!< RX Stream
    TRX_Stream_Tx_Start = 2,//!< TX Stream Continuous Start
    TRX_Stream_Tx_Cont = 3, //!< TX Stream Continuous Continuation
    TRX_Stream_Tx_End = 4   //!< TX Stream Continuous End
} TRX_Stream_Type;

/********************************** Structs ***********************************/

/** Metadata to track cmds throughout their lifecycle */
typedef struct TRX_Host_Cmd_t
{
    /** Slot the cmd resides in on the TRX */
    uint8_t slot;
    /** The events the application has subscribed to for this cmd */
    uint64_t subscribedEvents;
    /** Pointer to the TRX_Request_CommandStore provided by the application */
    TRX_Request_CommandStore *cmdStore;
    /** The application provided callback for this cmd */
    TRX_Host_Callback cb;
} TRX_Host_Cmd;

/** Metadata to track streams throughout their lifecycle */
typedef struct TRX_Host_Stream_t
{
    /** The unique identifier of the stream */
    uint8_t id;
    /** The type of the stream */
    TRX_Stream_Type type;
    /** The length of the data. For an RX stream this is the max size of
     * data that can fit in the buffer */
    uint16_t len;
    /** The current index into the data buffer */
    uint16_t index;
    /** The retention policy for this stream on the TRX */
    TRX_Stream_Retention retention;
    /** The events this stream is subscribed to */
    uint64_t subscribedEvents;
    /** The application provided callback for this stream */
    TRX_Host_Callback cb;
    /** Pointer to the applications data buffer */
    uint8_t* data;
} TRX_Host_Stream;

/** Metadata to track configs throughout their lifecycle */
typedef struct TRX_Host_Config_t
{
    /** The unique identifier of the config */
    uint8_t id;
    /** Boolean whether the configuration is persisted to NV */
    uint8_t persisted;
    /** The length of the data */
    uint16_t len;
    /** The events this config is subscribed to */
    uint64_t subscribedEvents;
    /** The unique reference for this config on the TRX */
    uint32_t reference;
    /** The application provided callback for this config */
    TRX_Host_Callback cb;
    /** Pointer to the applications data buffer */
    uint8_t* data;
} TRX_Host_Config;

/*!
 *  @brief Object global to the TRC Host module used to
 *  organize important information regarding operation.
 */
typedef struct TRX_Host_Object_t
{
    /** Bitmask of the status of the TRX Host module including open status */
    volatile uint8_t status;
    /** Variable that can be used as a general purpose global for operations
     * that require pending on responses from the TRX such as configuration discovery */
    volatile uint32_t generalPurposeGlobal;
    /** Global Transaction ID sent to the TRX */
    volatile uint8_t tid_tx;
    /** Global Transaction ID acknowledgment sent to the TRX */
    volatile uint8_t tid_ack;
    /** Number of SPI blocks currently free on the TRX */
    volatile uint8_t numFreeTrxSpiBlocks;
    /** Number of SPI containers currently free on the Host */
    volatile uint8_t numFreeHostSpiContainers;
    /** Handle to the SPI transport module */
    SPI_Transport_Handle transportHandle;
    /** Callback for requests received from the TRX that don't map to a user initiated operation */
    TRX_Host_Callback generalCb;
    /** Callback indicating the application should capture its current time that
     * can be associated with the time returned from the TRX on ::TRX_EventReceivedTrxTime */
    TRX_Host_Callback captureHostTimeCb;
    /** Optional argument to pass to all callbacks from the driver */
    uintptr_t arg;
    /** Queue containing mapping information for received last statuses */
    Util_List lastStatusMapQueue;
    /** Queue of SPI_Transport_Container to be transmitted on the SPI bus */
    Util_List spiTxQueue;
    /** Stream currently being flushed to the TRX */
    TRX_Host_Stream *streamBeingFlushed;
    /** Current index into lastStatusMap */
    volatile uint8_t lastStatusMapIndex;
    /** Cmds known to the TRX and the Host */
    SPI_Transport_Last_Status_Map lastStatusMap[TRX_MAX_STREAMS];
    /** Cmds known to the TRX and the Host */
    TRX_Host_Cmd registeredCmds[TRX_MAX_NUM_COMMANDS];
    /** Streams known to the TRX and the Host */
    TRX_Host_Stream registeredStreams[TRX_MAX_STREAMS];
    /** Configurations known to the TRX and the Host */
    TRX_Host_Config registeredConfigs[TRX_MAX_CONFIGURATIONS];
} TRX_Host_Object;

/********************************** Typedefs **********************************/

/***************************** Extern Prototypes ******************************/

/**
 * @brief Get the next request within the provide pSpiContainer.trxSpiBlock
 * with enough space for the provided reqSize
 *
 * @param pSpiContainer  The SPI_Transport_Container to search within
 * @param reqSize The size the request is expected to occupy
 * @return TRX_Request*, NULL if not enough space available
 */
TRX_Request *getNextEmptyRequest(SPI_Transport_Container *pSpiContainer, uint8_t reqSize);

/**
 * @brief Get an Empty Last Status Map object
 *
 * @param handle Handle to the TRX Host module
 * @return SPI_Transport_Last_Status_Map*, NULL if there are no empty maps
 */
SPI_Transport_Last_Status_Map *getEmptyLastStatusMap(TRX_Host_Handle handle);


/**
 * @brief Frees the provided SPI_Transport_Last_Status_Map
 *
 * @param pMap Pointer to the map to free
 */
void freeLastStatusMap(SPI_Transport_Last_Status_Map *pMap);

/**
 * @brief Get a Spi Container object
 *
 * @param handle Handle to the TRX Host module
 * @param minLength Minimum amount of space required in the SPI_Transport_Container
 * @return SPI_Transport_Container*, NULL if no container available
 */
SPI_Transport_Container *getSpiContainer(TRX_Host_Handle handle, uint8_t minLength);

/**
 * @brief Free a SPI_Transport_Container
 *
 * @param handle Handle to the TRX Host module
 * @param pSpiContainer Pointer to the SPI_Transport_Container to free
 */
void freeSpiContainer(TRX_Host_Handle handle, SPI_Transport_Container *pSpiContainer);

/**
 * @brief Resolves a stream based on its ID to an entry in the TRX Host module's
 * stream "database"
 *
 * @param handle Handle to the TRX Host module
 * @param id The unique identifier of the stream within range [0, TRX_MAX_STREAMS)
 * @return TRX_Host_Stream*, NULL if the id is invalid/out of range
 */
TRX_Host_Stream *resolveStream(TRX_Host_Handle handle, uint8_t id);

/**
 * @brief Resolves a config based on its ID to an entry in the TRX Host module's
 * config "database"
 *
 * @param handle Handle to the TRX Host module
 * @param id The unique identifier of the config within range [0, TRX_MAX_CONFIGURATIONS)
 * @return TRX_Host_Config*, NULL if the id is invalid/out of range
 */
TRX_Host_Config *resolveConfig(TRX_Host_Handle handle, uint8_t id);

/**
 * @brief Resolves a cmd based on its ID to an entry in the TRX Host module's
 * cmd "database"
 *
 * @param handle Handle to the TRX Host module
 * @param slot The slot of the cmd within range [0, TRX_MAX_NUM_COMMANDS)
 * @return TRX_Host_Cmd*, NULL if the slot is invalid/out of range
 */
TRX_Host_Cmd *resolveCmd(TRX_Host_Handle handle, uint8_t slot);

/**
 * @brief Register a stream in the TRX Host's stream "database"
 *
 * @param handle Handle to the TRX Host module
 * @param id The unique identifier of the stream within range [0, TRX_MAX_STREAMS)
 * @param pData Pointer to the applications data buffer
 * @param len The length of the data. For an RX stream this is the max size of
 *            data that can fit in the buffer
 * @param retention The retention policy for this stream on the TRX
 * @param type The type of the stream
 * @param callback The application provided callback for this stream
 * @param subscribedEvents The events this stream is subscribed to
 * @return TRX_Host_Stream*, NULL if the id is out of range
 */
TRX_Host_Stream *registerStream(TRX_Host_Handle handle, uint8_t id,
                                       uint8_t *pData, uint16_t len,
                                       TRX_Stream_Retention retention,
                                       TRX_Stream_Type type,
                                       TRX_Host_Callback callback,
                                       uint64_t subscribedEvents);

/**
 * @brief Register a config in the TRX Host's config "database"
 *
 * @param handle Handle to the TRX Host module
 * @param id The unique identifier of the config within range [0, TRX_MAX_CONFIGURATIONS)
 * @param pData Pointer to the applications data buffer
 * @param len The length of the data
 * @param reference The unique reference for this config on the TRX
 * @param persisted Boolean whether the config is persisted to NV on the TRX
 * @param callback The application provided callback for this config
 * @param subscribedEvents The events this config is subscribed to
 * @return TRX_Host_Config*, NULL if the id is out of range
 */
TRX_Host_Config *registerConfig(TRX_Host_Handle handle, uint8_t id,
                                       uint8_t *pData, uint16_t len,
                                       uint32_t reference, uint8_t persisted,
                                       TRX_Host_Callback callback,
                                       uint64_t subscribedEvents);

/**
 * @brief Register a cmd in the TRX Host's cmd "database"
 *
 * @param handle Handle to the TRX Host module
 * @param pCmdStore Pointer to application provided TRX_Request_CommandStore
 * @param callback The application provided callback for this cmd
 * @param subscribedEvents The events this cmd is subscribed to
 * @return TRX_Host_Cmd*, NULL if the pCmdStore->slot is out of range
 */
TRX_Host_Cmd *registerCommand(TRX_Host_Handle handle,
                                     TRX_Request_CommandStore *pCmdStore,
                                     TRX_Host_Callback callback,
                                     uint64_t subscribedEvents);

/**
 * @brief Prepare a SPI_Transport_Container for being sent to the TRX by
 * calculating a CRC over the data and other housekeeping operations before
 * finally sending it to the SPI Transport module.
 *
 * @param handle Handle to the TRX Host module
 * @param pSpiContainer Pointer to the SPI_Transport_Container to send
 */
void prepAndSendTransaction(TRX_Host_Handle handle, SPI_Transport_Container *pSpiContainer);

/**
 * @brief Builds and sends the requested amount of a stream to the TRX via a
 * TRX_Request_StreamStore
 *
 * @param handle Handle to the TRX Host module
 * @param pStream Pointer to the TRX_Host_Stream to build
 * @param lenToSend Length of the data to send
 * @param blockForContainer Whether to block when there are no available
 *                          SPI_Transport_Container
 * @return TRX_Host_Status of TRX_Host_Success when all the requested data was
 *         able to be built and sent, otherwise TRX_Host_No_Resources indicating
 *         there were no available SPI_Transport_Container
 */
TRX_Host_Status buildAndSendStream(TRX_Host_Handle handle, TRX_Host_Stream *pStream,
                                   uint16_t lenToSend, bool blockForContainer);

//*****************************************************************************
//
//! Close the Doxygen group.
//! @}
//
//*****************************************************************************

#ifdef __cplusplus
}
#endif

#endif // TRX_HOST_PRIVATE_H_

