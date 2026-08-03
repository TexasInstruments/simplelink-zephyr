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

@file trx_host.c

@brief Implements the TRX Host driver.

 */

/********************************** Includes **********************************/

/* Standard Header Files */
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Module Header Files */
#include "TRX.h"
#include "trx_host.h"
#include "private/trx_host_private.h"
#include "handlers/trx_host_handlers.h"
#include "utility/utils.h"
#include "utility/list/list.h"
#include "hal/platform_util.h"
#include "transport/spi_transport.h"
#include "utility/buffer_manager/buffer_manager.h"

/* Zephyr Logging */
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(ti_cc140xp, CONFIG_TI_CC140XP_LOG_LEVEL);


/********************************** Defines ***********************************/

#define THIRTY_MS_IN_US (30000U)

/*********************************** Macros ***********************************/

/*********************************** Enums ************************************/

/********************************** Structs ***********************************/

/********************************** Typedefs **********************************/

/********************************* Prototypes *********************************/

/**
 * @brief Callback that the SPI Transport layer issues when the CS pin is deasserted
 *
 * @param arg Optional argument user can specify to be passed to each execution
 *            of this callback
 */
static void csDeassertCb(uintptr_t arg);

/**
 * @brief Callback from the SPI Transport module to the TR Host module
 *
 * @note Calling context: HWI, SWI
 *
 * @param[in] pTxSpiContainer  Pointer to the transmitted SPI block that the TRX
 *                             Host module must free
 * @param[in] pRxSpiContainer  Pointer to the received SPI block that is freed
 *                             after the callback returns
 * @param[in] status           Status of the SPI transaction
 * @param[in] arg              Optional argument user can specify to be passed
 *                             to each execution of this callback
 */
static void transportCb(SPI_Transport_Container *pTxSpiContainer,
                        SPI_Transport_Container *pRxSpiContainer,
                        SPI_Transport_Status status, uintptr_t arg);

/** Callback to get the next TRX Operation from the queue.
 *  @return Pointer to the pending RF operation, NULL if empty.
 */
static SPI_Transport_Container *getNextSpiContainer(uintptr_t arg);

static TRX_Host_Status processStream(TRX_Host_Handle handle, TRX_Host_Stream *pStream,
                                    uint8_t *pData, uint16_t len);

/*************************** Variable Declarations ****************************/

/*!
 *  @brief Object global to the TRC Host module used to
 *  organize important information regarding operation.
 */
TRX_Host_Object trxHostObject;

/* Diagnostic counter defined in spi_transport.c. Decremented here when the
 * parse loop body is reached, so a non-zero value at hang time means a CCB
 * fired without a subsequent parse (Pattern B Hypothesis 1).
 * See stability-failure-insights.md.
 */
extern volatile uint8_t spiCcbNotParsed;

/************************* Static Function Definitions ************************/

static void csDeassertCb(uintptr_t arg)
{
    TRX_Host_Handle handle = (TRX_Host_Handle)arg;
    if(!TRX_HOST_IS_OPEN(handle))
    {
        // Driver might have been closed already
        goto csDeassertCbEnd;
    }

    if(NULL != handle->captureHostTimeCb)
    {
        handle->captureHostTimeCb(handle, (uintptr_t)NULL, NULL, TRX_EventCaptureHostTime, handle->arg);
    }

csDeassertCbEnd:
    return;
}

static void transportCb(SPI_Transport_Container *pTxSpiContainer,
                        SPI_Transport_Container *pRxSpiContainer,
                        SPI_Transport_Status status, uintptr_t arg)
{

    LOG_DBG("RF: Transport CB being executed with params %p, %p, %d, %p",
        (void *)pTxSpiContainer, (void *)pRxSpiContainer, status, (void *)arg);

    TRX_Host_Handle handle = (TRX_Host_Handle)arg;

    if(NULL != pTxSpiContainer)
    {
        freeSpiContainer(handle, pTxSpiContainer);
    } // RX Spi container is freed at the SPI_Transport level where it's allocated

    if(!TRX_HOST_IS_OPEN(handle))
    {

        LOG_INF("RF: Driver closed, just exiting");

        // Driver might have been closed already
        goto transportCbEnd;
    }

    // Prioritize sending out streams that are being flushed
    if(NULL != handle->streamBeingFlushed)
    {

        LOG_DBG("RF: Continuing to flush stream %d", handle->streamBeingFlushed->id);

        uint16_t lenToSend = handle->streamBeingFlushed->len - handle->streamBeingFlushed->index;
        TRX_Host_Status streamStatus = buildAndSendStream(handle, handle->streamBeingFlushed, lenToSend, false);
        if(TRX_Host_Success == streamStatus)
        {
            // Done streaming
            LOG_DBG("RF: Done flushing stream %d", handle->streamBeingFlushed->id);
            handle->streamBeingFlushed = NULL;
        }
    }

    if((NULL != pRxSpiContainer) && (SPI_Transport_Success == status))
    {
        /* Diagnostic: parse loop reached. See stability-failure-insights.md. */
        if (spiCcbNotParsed > 0U) { spiCcbNotParsed--; }

        // !!!!!!!!!! Could verify the CRC here but we don't !!!!!!!!!!!!
        // Would add delay in parsing and if an error occurs there's no retry mechanism

        LOG_INF("RF: Parsing requests in the received transaction");

        // Track the number of free spiblocks and tid's to ack
        handle->numFreeTrxSpiBlocks = pRxSpiContainer->trxSpiBlock.header.trx.free;
        if(0U != pRxSpiContainer->trxSpiBlock.header.trx.tid_tx)
        {
            handle->tid_ack = pRxSpiContainer->trxSpiBlock.header.trx.tid_tx;
        }

        uint8_t i = 0;
        uintptr_t cbArg = handle->arg;
        while(i < pRxSpiContainer->trxSpiBlock.header.trx.len)
        {
            TRX_Request *pReq = (TRX_Request *)&(pRxSpiContainer->trxSpiBlock.payload[i]);
            uint64_t observedEvents = TRX_EventUnassigned;
            uint64_t subscribedEvents = TRX_EventUnassigned;
            TRX_Host_Callback cb = NULL;
            uintptr_t pCmdStreamOrConfig = (uintptr_t)NULL;

            requestHandler handler = TRX_Host_Handlers_getHandler(pReq->header.request);
            if(NULL != handler)
            {
                // Call this request's parser
                handler(handle, pReq, &observedEvents, &subscribedEvents, &cb,
                        &pCmdStreamOrConfig);
            }

            // CMD_STATUS responses are only emitted for user-registered commands
            // (RX, TX, etc.) and must always produce an invokable user callback;
            // a silent drop here points to dispatch-layer corruption.
            if(Req_CMD_STATUS == pReq->header.request)
            {
                Assert_isTrue(NULL != cb, 23U);
            }

            if((NULL != cb) && (subscribedEvents & observedEvents))
            {
                LOG_DBG("RF: Calling application cb");
                cb(handle, pCmdStreamOrConfig, pReq, observedEvents, cbArg);
            }

            // Increment to the next req accounting for padding
            i += pReq->header.len + PADDING_REQUIRED(pReq->header.len);
        }
    }

    LOG_DBG("RF: Done handling Transport cb from SPI layer");

transportCbEnd:
    return;
}

static SPI_Transport_Container *getNextSpiContainer(uintptr_t arg)
{
    TRX_Host_Handle handle = (TRX_Host_Handle)arg;

    // This operation must happen atomically
    unsigned int key;
    Plat_Util_startCriticalSection((uintptr_t)&key);

    SPI_Transport_Container *nextTxSpiBlock = (SPI_Transport_Container *)Util_List_pop(&(handle->spiTxQueue));
    if(NULL != nextTxSpiBlock)
    {
        // MUST move req maps to the req map list here as it's guaranteed to be sent to the TRX in order
        const int8_t concatStatus = Util_List_concat(&(handle->lastStatusMapQueue), &(nextTxSpiBlock->lastStatusMaps));

        LOG_DBG( "RF: Moved last status maps for SPI_Transport_Container %p to queue with status %d", (void *)nextTxSpiBlock, concatStatus);

        Assert_isTrue((0U == concatStatus), 0U);
    }

    Plat_Util_endCriticalSection((uintptr_t)&key);
    LOG_DBG( "RF: Providing SPI_Transport_Container %p to SPI layer", (void *)nextTxSpiBlock);

    return(nextTxSpiBlock);
}

/**
 * @brief Perfrom the operations needed by ::TRX_Host_storeStream after 
 * resolving the streamStore a stream on the TRX via one or more
 *
 * @param handle Handle to the TRX Host driver
 * @param pStream Pointer to the stream being stored; must be resolved and checked for NULL by the caller
 * @param pData Pointer to a buffer containing the stream data
 * @param len The length of the data
 * @return TRX_Host_Status indicating success or failure of starting to send the
 *         ::TRX_Request_StreamStore requests
 */
static TRX_Host_Status processStream(TRX_Host_Handle handle, TRX_Host_Stream *pStream,
                                    uint8_t *pData, uint16_t len)    
{
    TRX_Host_Status status;
    uint16_t lenToSend ;
    uint8_t numTransactions;
    if (Stream_Retention_Flush_Streaming == pStream->retention)
    {
        /* Start by at most 2 SPI blocks */
        lenToSend = (len < (MAX_STREAM_DATA_CHUNK * 2)) ? len : (MAX_STREAM_DATA_CHUNK * 2);
    }
    else if (Stream_Retention_Flush_Streaming_Continuous == pStream->retention && pStream->type == TRX_Stream_Tx_Start)
    {
        /* Start by at most 3 SPI blocks */
        lenToSend = (len < (MAX_STREAM_DATA_CHUNK * 3)) ? len : (MAX_STREAM_DATA_CHUNK * 3);
    }
    else
    {
        // The whole stream must be put on the TRX
        lenToSend = len;
    }
    // Calculate the total number of transactions for lenToSend this stream will be split across
    // including room for the req header on each
    numTransactions = (lenToSend + MAX_STREAM_DATA_CHUNK - 1) / MAX_STREAM_DATA_CHUNK;
    // Determine if the stream will even fit on the TRX
    // TRX has max of 20 SPI Blocks but can only indicate a max of 15 due to bit width limitation (4bits)
    // So if TRX indicates 15 - it can potentially have more than that and hence, host can try sending the req
    // Due to lenToSend being updated according to the stream retention policy, this check now takes into account
    // the smaller number of SPI blocks needed when using flush streaming compared to flush never policy.
    if((handle->numFreeTrxSpiBlocks < MAX_TRX_SPIBLOCKS) && (handle->numFreeTrxSpiBlocks < numTransactions + 2))
    {
        // There isn't enough space on the TRX for this stream
        status = TRX_Host_No_Resources;
        goto endProcessStream;
    }
    status = buildAndSendStream(handle, pStream, lenToSend, true);

endProcessStream:
    return(status);
}

/************************* Extern Function Definitions ************************/

TRX_Host_Handle TRX_Host_open(TRX_Host_Params *pParams)
{
    TRX_Host_Handle handle = (TRX_Host_Handle)&trxHostObject;
    if((TRX_HOST_IS_OPEN(handle)) || (NULL == pParams->generalCb))
    {
        handle = NULL;
        goto hostOpenEnd;
    }

    handle->tid_ack = 1U;
    handle->tid_tx  = 1U;
    handle->numFreeHostSpiContainers = ((BUFFER_MANAGER_NBUFFERS/2U) < MAX_HOST_TX_SPIBLOCKS) ? (BUFFER_MANAGER_NBUFFERS/2U) : MAX_HOST_TX_SPIBLOCKS;
    handle->numFreeTrxSpiBlocks = MAX_TRX_SPIBLOCKS;
    handle->streamBeingFlushed = NULL;
    handle->arg = pParams->arg;
    handle->generalCb = pParams->generalCb;
    handle->captureHostTimeCb = NULL;
    handle->lastStatusMapIndex = 0U;
    Util_List_clear(&(handle->lastStatusMapQueue));
    Util_List_setMaxEntries(&(handle->lastStatusMapQueue), sizeof(handle->lastStatusMap)/sizeof(SPI_Transport_Last_Status_Map) + 1);
    Util_List_clear(&(handle->spiTxQueue));
    Util_List_setMaxEntries(&(handle->spiTxQueue), BUFFER_MANAGER_NBUFFERS);

    const SPI_Transport_Params transportParams = {
        .bitRate = pParams->bitRate,
        .arg = (uintptr_t)handle,
        .pCSDeassertCb = csDeassertCb,
        .pSpiTransportCb = transportCb,
        .pGetNextSpiContainer = getNextSpiContainer
    };

    Buffer_Manager_init();
    handle->transportHandle = SPI_Transport_open(&transportParams);
    if(NULL == handle->transportHandle)
    {
        handle = NULL;
        goto hostOpenEnd;
    }

    // Initialize the config, stream, and cmd "databases"
    uint8_t i = 0;
    for(i = 0; i < (sizeof(handle->registeredCmds)/sizeof(TRX_Host_Cmd)); i++)
    {
        handle->registeredCmds[i].slot = TRX_RESERVED_ID;
    }
    for(i = 0; i < (sizeof(handle->registeredConfigs)/sizeof(TRX_Host_Config)); i++)
    {
        handle->registeredConfigs[i].id = TRX_RESERVED_ID;
    }
    for(i = 0; i < (sizeof(handle->registeredStreams)/sizeof(TRX_Host_Stream)); i++)
    {
        handle->registeredStreams[i].id = TRX_RESERVED_ID;
    }

    TRX_HOST_SET_OPEN(handle);

    SPI_Transport_Container *pSpiContainer = getSpiContainer(handle, SPI_TRANSPORT_MAX_TLEN);
    if(NULL == pSpiContainer)
    {
        handle = NULL;
        goto hostOpenEnd;
    }

    TRX_Request_ConfigList *pConfigListSpiBlock = (TRX_Request_ConfigList *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_ConfigList));
    pConfigListSpiBlock->header.request = Req_CONFIG_LIST;
    pConfigListSpiBlock->header.len = sizeof(TRX_Request_ConfigList);

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        freeSpiContainer(handle, pSpiContainer);
        handle = NULL;
        goto hostOpenEnd;
    }

    pMap->cb = (uintptr_t)NULL;
    pMap->id = TRX_RESERVED_ID;
    pMap->md = 0U;
    pMap->req = Req_CONFIG_LIST;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    TRX_HOST_SET_PENDING(handle);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

    // Pend on response from TRX
    while(TRX_HOST_IS_PENDING(handle));

hostOpenEnd:
    return(handle);
}

void TRX_Host_close(TRX_Host_Handle handle)
{
    if(!TRX_HOST_IS_OPEN(handle))
    {
        // Already closed
        goto hostCloseEnd;
    }

    TRX_HOST_SET_CLOSED(handle);

    // Free any dangling last status maps
    SPI_Transport_Last_Status_Map *pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));
    while(NULL != pMap)
    {
        freeLastStatusMap(pMap);
        pMap = (SPI_Transport_Last_Status_Map *)Util_List_pop(&(handle->lastStatusMapQueue));
    }

    // Any TX SPI_Transport_Container will be freed in transportCb
    (void)SPI_Transport_close(handle->transportHandle);


hostCloseEnd:
    return;
}

TRX_Host_Status TRX_Host_storeConfig(TRX_Host_Handle handle, uint8_t id,
                                     uint8_t *pData, uint16_t len,
                                     uint32_t reference, bool force,
                                     TRX_Host_Callback callback,
                                     uint64_t subscribedEvents)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;
    TRX_Host_Config *pConfig = resolveConfig(handle, id);

    if(NULL != pConfig)
    {
        if((reference == pConfig->reference) && (!force))
        {
            // There's already a matching entry
            status = TRX_Host_Config_Exists;
            goto endStoreConfig;
        }
    }

    pConfig = registerConfig(handle, id, pData, len, reference, 0U, callback,
                             subscribedEvents);
    if(NULL == pConfig)
    {
        status = TRX_Host_Invalid_Id;
        goto endStoreConfig;
    }

    // Calculate the total number of transactions this stream will be split across
    // including room for the req header on each
    // Integer division with round-up: https://stackoverflow.com/questions/2745074/fast-ceiling-of-an-integer-division-in-c-c
    const uint16_t chunkSize = (len < MAX_CONFIG_DATA_CHUNK) ? len : MAX_CONFIG_DATA_CHUNK;
    int16_t temp = -1;
    if(chunkSize != len)
    {
        temp = (((int16_t)((len - chunkSize) - 1U))/((int16_t)MAX_CONFIG_DATA_CHUNK));
    }

    const uint8_t numTransactions = 1U + 1U + temp;

    // Determine if the config will even fit on the TRX
    // TRX has max of 20 SPI Blocks but can only indicate a max of 15 due to bit width limitation (4bits)
    // So if TRX indicates 15 - it can potentially have more than that and hence, host can try sending the req
    if((handle->numFreeTrxSpiBlocks < MAX_TRX_SPIBLOCKS) && (handle->numFreeTrxSpiBlocks <= numTransactions))
    {
        // There isn't enough space on the TRX for this stream
        status = TRX_Host_No_Resources;
        goto endStoreConfig;
    }

    LOG_INF( "RF: Starting to build TRX_Request_ConfigStore's for config with id: %d", id);

    uint16_t i = 0;
    while (i < len)
    {
        uint8_t lenToCopy = ((len - i) <= MAX_CONFIG_DATA_CHUNK) ? (len - i) : MAX_CONFIG_DATA_CHUNK;
        while(NULL == pSpiContainer)
        {
            // Block until a buffer is freed in the transport cb
            const uint8_t requiredSpace = sizeof(TRX_Request_ConfigStore) + lenToCopy;
            pSpiContainer = getSpiContainer(handle, requiredSpace);
        }

        TRX_Request_ConfigStore *pConfigStoreSpiBlock = (TRX_Request_ConfigStore *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_ConfigStore));
        pSpiContainer->index += lenToCopy;
        (void)memcpy(pConfigStoreSpiBlock->data, &(pData[i]), lenToCopy);
        pConfigStoreSpiBlock->header.request = Req_CONFIG_STORE;
        pConfigStoreSpiBlock->header.len = sizeof(TRX_Request_ConfigStore) + lenToCopy;
        pConfigStoreSpiBlock->start = (i == 0);
        pConfigStoreSpiBlock->md = (len != (i + lenToCopy));
        pConfigStoreSpiBlock->config_id = id;
        pConfigStoreSpiBlock->reference = reference;

        SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
        if(NULL == pMap)
        {
            // Too many reqs out at once
            status = TRX_Host_No_Resources;
            goto endStoreConfig;
        }

        pMap->cb = (uintptr_t)callback;
        pMap->id = id;
        pMap->md = pConfigStoreSpiBlock->md;
        pMap->req = Req_CONFIG_STORE;
        (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

        prepAndSendTransaction(handle, pSpiContainer);
        pSpiContainer = NULL;

        i += lenToCopy;
    }

    LOG_INF( "RF: Done to build TRX_Request_ConfigStore's for config with id: %d", id);
endStoreConfig:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }
    return(status);
}

TRX_Host_Status TRX_Host_registerRxStream(TRX_Host_Handle handle, uint8_t id,
                                          uint8_t *pData, uint16_t len,
                                          TRX_Host_Callback callback,
                                          uint64_t subscribedEvents)
{
    LOG_INF( "RF: Registering RX stream with id: %d", id);
    TRX_Host_Status status = TRX_Host_Success;

    // Remains a registered stream until a new rx stream with the same ID is registered
    TRX_Host_Stream *pStream = registerStream(handle, id, pData, len,
                                              Stream_Retention_Flush_Never,
                                              TRX_Stream_Rx, callback,
                                              subscribedEvents);
    if(NULL == pStream)
    {
        status = TRX_Host_Invalid_Id;
    }
    return(status);
}

TRX_Host_Status TRX_Host_storeStream(TRX_Host_Handle handle, uint8_t id,
                                     uint8_t *pData, uint16_t len,
                                     TRX_Stream_Retention retention,
                                     TRX_Host_Callback callback,
                                     uint64_t subscribedEvents)
{
    TRX_Host_Status status = TRX_Host_Success;
    TRX_Stream_Type streamType = TRX_Stream_Tx;
    if (retention == Stream_Retention_Flush_Streaming_Continuous)
    {
        streamType = TRX_Stream_Tx_Start;
    }
    TRX_Host_Stream *pStream = registerStream(handle, id, pData, len, retention,
                                              streamType, callback,
                                              subscribedEvents);
    if(NULL == pStream)
    {
        status = TRX_Host_Invalid_Id;
        goto endStoreStream;
    }
    status = processStream(handle, pStream, pData, len);

endStoreStream:
    return(status);

}

TRX_Host_Status TRX_Host_continueStream(TRX_Host_Handle handle, uint8_t id,
                                        uint8_t *pData, uint16_t len, bool endStream)
{
    TRX_Host_Status status = TRX_Host_Success;
    TRX_Host_Stream *pStream = resolveStream(handle, id);
    if (NULL == pStream || pStream->type != TRX_Stream_Tx_Cont)
    {
        status = TRX_Host_Invalid_Id;
        goto endContinueStream;
    }
    pStream->data = pData;
    pStream->len = len;
    pStream->index = 0;
    if (endStream)
    {
        pStream->type = TRX_Stream_Tx_End;
    }

    status = processStream(handle, pStream, pData, len);
endContinueStream:
    return(status);
}


TRX_Host_Status TRX_Host_storeCmds(TRX_Host_Handle handle,
                                   TRX_Request_CommandStore *pCmdStores,
                                   uint8_t numCmds,
                                   TRX_Request_CommandStore *pCmdToSubmit,
                                   TRX_Host_Callback callback,
                                   uint64_t subscribedEvents)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;

    uint8_t i = 0;
    if (numCmds <= 0)
    {
        status = TRX_Host_Invalid_Param;
        goto endStoreCmds;
    }
    while(i < numCmds)
    {
        while(NULL == pSpiContainer)
        {
            pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_CommandStore));
        }

        TRX_Request_CommandStore *pCmdStore = &(pCmdStores[i]);
        TRX_Request_CommandStore *pCmdStoreSpiBlock = (TRX_Request_CommandStore *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_CommandStore));
        if(NULL == pCmdStoreSpiBlock)
        {
            // Out of space in the current SPI block. Send it to  the TRX
            prepAndSendTransaction(handle, pSpiContainer);
            pSpiContainer = NULL;
        }
        else
        {
            LOG_INF( "RF: Creating TRX_Request_CommandStore for cmd in slot: %d", pCmdStore->slot);
            LOG_WRN("RF: with callback: %p", (void *)callback);

            TRX_Host_Cmd *pCmd = registerCommand(handle, pCmdStore, callback, subscribedEvents);
            if(NULL == pCmd)
            {
                status = TRX_Host_No_Resources;
                goto endStoreCmds;
            }

            // Move the cmd into the spiblock
            (void)memcpy(pCmdStoreSpiBlock, pCmdStore, sizeof(TRX_Request_CommandStore));
            pCmdStoreSpiBlock->header.len = sizeof(TRX_Request_CommandStore);
            pCmdStoreSpiBlock->header.request = Req_CMD_STORE;

            SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
            if(NULL == pMap)
            {
                // Too many reqs out at once
                status = TRX_Host_No_Resources;
                goto endStoreCmds;
            }

            pMap->cb = (uintptr_t)callback;
            pMap->id = pCmdStore->slot;
            pMap->md = 0;
            pMap->req = Req_CMD_STORE;
            (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);
            i += 1;
        }
    }

    if(NULL != pCmdToSubmit)
    {
        LOG_INF( "RF: Creating TRX_Request_CommandSubmit for cmd in slot: %d", pCmdToSubmit->slot);

        TRX_Request_CommandSubmit *cmdSubmitSpiBlock = (TRX_Request_CommandSubmit *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_CommandSubmit));
        if(NULL == cmdSubmitSpiBlock)
        {
            LOG_INF( "RF: Out of space in the current SPI block, sending to TRX and getting a new block.");

            // Out of space in the current SPI block. Send it to  the TRX
            prepAndSendTransaction(handle, pSpiContainer);

            // Get a new SPIblock
            pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_CommandSubmit));
            if(NULL == pSpiContainer)
            {
                LOG_INF( "RF: Failed to get a new SPI block, out of resources!");

                status = TRX_Host_No_Resources;
                goto endStoreCmds;
            }

            cmdSubmitSpiBlock = (TRX_Request_CommandSubmit *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_CommandSubmit));
        }

        cmdSubmitSpiBlock->header.len = sizeof(TRX_Request_CommandSubmit);
        cmdSubmitSpiBlock->header.request = Req_CMD_SUBMIT;
        cmdSubmitSpiBlock->slot = pCmdToSubmit->slot;

        SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
        if(NULL == pMap)
        {
            // Too many reqs out at once
            status = TRX_Host_No_Resources;
            goto endStoreCmds;
        }

        pMap->cb = (uintptr_t)callback;
        pMap->id = pCmdToSubmit->slot;
        pMap->md = 0;
        pMap->req = Req_CMD_SUBMIT;
        (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);
    }

    if(NULL != pSpiContainer)
    {
        // There is remaining stuff to send
        prepAndSendTransaction(handle, pSpiContainer);
        pSpiContainer = NULL;
    }

endStoreCmds:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }
    return(status);
}

TRX_Host_Status TRX_Host_submitCmd(TRX_Host_Handle handle, uint8_t slot)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;
    TRX_Host_Cmd *pCmd = resolveCmd(handle, slot);
    if(NULL == pCmd)
    {
        status = TRX_Host_Invalid_Id;
        goto endSubmitCmd;
    }

    // Not sure on best fix here -- should this all be a critical section so the SPI container doesn't run out of space for requests,
    // or if we're out of requests should we just request a new spi container? We might lock up here waiting for getSpiContainer
    while(NULL == pSpiContainer)
    {
        // Block until a buffer is freed in the transport cb
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_CommandSubmit));
    }

    LOG_INF( "RF: Creating TRX_Request_CommandSubmit for cmd in slot: %d", slot);

    //need to check if this is null, and handle properly!
    TRX_Request_CommandSubmit *cmdSubmitSpiBlock = (TRX_Request_CommandSubmit *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_CommandSubmit));
    if(NULL == cmdSubmitSpiBlock)
    {
        LOG_WRN( "RF: NextEmptyRequest returned null, out of resources!");
        status = TRX_Host_No_Resources;
        goto endSubmitCmd;
    }

    cmdSubmitSpiBlock->header.len = sizeof(TRX_Request_CommandSubmit);
    cmdSubmitSpiBlock->header.request = Req_CMD_SUBMIT;
    cmdSubmitSpiBlock->slot = pCmd->slot;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endSubmitCmd;
    }
    pMap->cb = (uintptr_t)pCmd->cb;
    pMap->id = pCmd->slot;
    pMap->md = 0;
    pMap->req = Req_CMD_SUBMIT;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endSubmitCmd:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }

    return(status);
}

TRX_Host_Status TRX_Host_stopCmd(TRX_Host_Handle handle, uint8_t slot, TRX_Command_StopType stopType)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;
    TRX_Host_Cmd *pCmd = resolveCmd(handle, slot);
    if(NULL == pCmd)
    {
        status = TRX_Host_Invalid_Id;
        goto endStopCmd;
    }

    while(NULL == pSpiContainer)
    {
        // Block until a buffer is freed in the transport cb
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_CommandStop));
    }

    LOG_INF( "RF: Creating TRX_Request_CommandStop for cmd in slot: %d", slot);

    TRX_Request_CommandStop *cmdStopSpiBlock = (TRX_Request_CommandStop *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_CommandStop));
    cmdStopSpiBlock->header.len = sizeof(TRX_Request_CommandStop);
    cmdStopSpiBlock->header.request = Req_CMD_STOP;
    cmdStopSpiBlock->slot = pCmd->slot;
    cmdStopSpiBlock->stop_type = stopType;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endStopCmd;
    }
    pMap->cb = (uintptr_t)pCmd->cb;
    pMap->id = pCmd->slot;
    pMap->md = 0;
    pMap->req = Req_CMD_STOP;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endStopCmd:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }

    return(status);
}

TRX_Host_Status TRX_Host_ping(TRX_Host_Handle handle, uint8_t pingData, TRX_Host_Callback callback)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;

    while(NULL == pSpiContainer)
    {
        // Block until a buffer is freed in the transport cb
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_Ping));
    }

    LOG_INF( "RF: Creating TRX_Request_Ping");

    TRX_Request_Ping *pingSpiBlock = (TRX_Request_Ping *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_Ping));
    pingSpiBlock->header.len = sizeof(TRX_Request_Ping);
    pingSpiBlock->header.request = Req_PING;
    pingSpiBlock->pingData = pingData;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endPing;
    }
    pMap->cb = (uintptr_t)callback;
    pMap->id = TRX_RESERVED_ID;
    pMap->md = 0;
    pMap->req = Req_PING;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endPing:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }

    return(status);
}

TRX_Host_Status TRX_Host_persistConfig(TRX_Host_Handle handle, uint8_t id)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;
    TRX_Host_Config *pConfig = resolveConfig(handle, id);
    if(NULL == pConfig)
    {
        status = TRX_Host_Invalid_Id;
        goto endPersistConfig;
    }

    while(NULL == pSpiContainer)
    {
        // Block until a buffer is freed in the transport cb
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_ConfigPersist));
    }

    LOG_INF( "RF: Creating TRX_Request_ConfigPersist for config with id: %d", id);

    TRX_Request_ConfigPersist *configSpiBlock = (TRX_Request_ConfigPersist *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_ConfigPersist));
    configSpiBlock->header.len = sizeof(TRX_Request_ConfigPersist);
    configSpiBlock->header.request = Req_CONFIG_PERSIST;
    configSpiBlock->config_id = id;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endPersistConfig;
    }
    pMap->cb = (uintptr_t)pConfig->cb;
    pMap->id = pConfig->id;
    pMap->md = 0;
    pMap->req = Req_CONFIG_PERSIST;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endPersistConfig:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }

    return(status);
}

TRX_Host_Status TRX_Host_startTimeSync(TRX_Host_Handle handle, TRX_Host_Callback callback)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;
    handle->captureHostTimeCb = callback;

    while(NULL == pSpiContainer)
    {
        // Block until a buffer is freed in the transport cb
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_UtilGetTime));
    }

    LOG_INF( "RF: Creating TRX_Request_UtilGetTime");

    TRX_Request_UtilGetTime *getTimeSpiBlock = (TRX_Request_UtilGetTime *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_UtilGetTime));
    getTimeSpiBlock->header.len = sizeof(TRX_Request_UtilGetTime);
    getTimeSpiBlock->header.request = Req_UTIL_TIME;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endStartTimeSync;
    }
    pMap->cb = (uintptr_t)callback;
    pMap->id = TRX_RESERVED_ID;
    pMap->md = 0;
    pMap->req = Req_UTIL_TIME;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endStartTimeSync:
    if(NULL != pSpiContainer)
    {
        handle->captureHostTimeCb = NULL;
        freeSpiContainer(handle, pSpiContainer);
    }

    return(status);
}

TRX_Host_Status TRX_Host_utilGetRssi(TRX_Host_Handle handle, TRX_Host_Callback callback)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;

    while(NULL == pSpiContainer)
    {
        // Block until a buffer is freed in the transport cb
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_UtilGetRssi));
    }

    LOG_INF( "RF: Creating TRX_Request_UtilGetRssi");

    TRX_Request_UtilGetRssi *getRssiSpiBlock = (TRX_Request_UtilGetRssi *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_UtilGetRssi));
    getRssiSpiBlock->header.len = sizeof(TRX_Request_UtilGetRssi);
    getRssiSpiBlock->header.request = Req_UTIL_RSSI;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endGetRssi;
    }
    pMap->cb = (uintptr_t)callback;
    pMap->id = TRX_RESERVED_ID;
    pMap->md = 0;
    pMap->req = Req_UTIL_RSSI;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endGetRssi:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }

    return(status);
}

TRX_Host_Status TRX_Host_utilGetVersion(TRX_Host_Handle handle, TRX_Host_Callback callback)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;

    while(NULL == pSpiContainer)
    {
        // Block until a buffer is freed in the transport cb
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_UtilInfoGetVersion));
    }

    LOG_INF( "RF: Creating TRX_Request_UtilInfoGetVersion");

    TRX_Request_UtilInfoGetVersion *getVersionSpiBlock = (TRX_Request_UtilInfoGetVersion *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_UtilInfoGetVersion));
    getVersionSpiBlock->header.len = sizeof(TRX_Request_UtilInfoGetVersion);
    getVersionSpiBlock->header.request = Req_UTIL_INFO_VERSION;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endGetVersion;
    }
    pMap->cb = (uintptr_t)callback;
    pMap->id = TRX_RESERVED_ID;
    pMap->md = 0;
    pMap->req = Req_UTIL_INFO_VERSION;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endGetVersion:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }

    return(status);
}

TRX_Host_Status TRX_Host_utilGetMacUuid(TRX_Host_Handle handle, TRX_Host_Callback callback)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;

    while(NULL == pSpiContainer)
    {
        // Block until a buffer is freed in the transport cb
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_UtilInfoGetMacUuid));
    }

    LOG_INF( "RF: Creating TRX_Request_UtilInfoGetMacUuid");

    TRX_Request_UtilInfoGetMacUuid *getVersionSpiBlock = (TRX_Request_UtilInfoGetMacUuid *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_UtilInfoGetMacUuid));
    getVersionSpiBlock->header.len = sizeof(TRX_Request_UtilInfoGetMacUuid);
    getVersionSpiBlock->header.request = Req_UTIL_INFO_MAC_UUID;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endGetMacUuid;
    }
    pMap->cb = (uintptr_t)callback;
    pMap->id = TRX_RESERVED_ID;
    pMap->md = 0;
    pMap->req = Req_UTIL_INFO_MAC_UUID;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endGetMacUuid:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }

    return(status);
}

TRX_Host_Status TRX_Host_utilGetDeviceInfo(TRX_Host_Handle handle, TRX_Host_Callback callback)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;

    while(NULL == pSpiContainer)
    {
        // Block until a buffer is freed in the transport cb
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_UtilInfoGetDevice));
    }

    LOG_INF( "RF: Creating TRX_Request_UtilInfoGetDevice");

    TRX_Request_UtilInfoGetDevice *getDeviceSpiBlock = (TRX_Request_UtilInfoGetDevice *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_UtilInfoGetDevice));
    getDeviceSpiBlock->header.len = sizeof(TRX_Request_UtilInfoGetDevice);
    getDeviceSpiBlock->header.request = Req_UTIL_INFO_DEVICE;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endGetDevice;
    }
    pMap->cb = (uintptr_t)callback;
    pMap->id = TRX_RESERVED_ID;
    pMap->md = 0;
    pMap->req = Req_UTIL_INFO_DEVICE;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endGetDevice:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }

    return(status);
}

TRX_Host_Status TRX_Host_utilEnterSsbl(TRX_Host_Handle handle, TRX_Host_Callback callback)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;

    while(NULL == pSpiContainer)
    {
        // Block until a buffer is freed in the transport cb
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_UtilEnterSsbl));
    }

    LOG_INF( "RF: Creating TRX_Request_UtilEnterSsbl");

    TRX_Request_UtilEnterSsbl *enterSsblSpiBlock = (TRX_Request_UtilEnterSsbl *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_UtilEnterSsbl));
    enterSsblSpiBlock->header.len = sizeof(TRX_Request_UtilEnterSsbl);
    enterSsblSpiBlock->header.request = Req_UTIL_ENTER_SSBL;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endEnterSsbl;
    }
    pMap->cb = (uintptr_t)callback;
    pMap->id = TRX_RESERVED_ID;
    pMap->md = 0;
    pMap->req = Req_UTIL_ENTER_SSBL;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endEnterSsbl:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }

    return(status);
}

TRX_Host_Status TRX_Host_utilWakeTrx(TRX_Host_Handle handle)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;

    while(NULL == pSpiContainer)
    {
        // Block until a buffer is freed in the transport cb
        pSpiContainer = getSpiContainer(handle, 0U);
    }

    // Send an empty transaction to wake up the TRX
    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

    // Wait until TRX is ready
    Plat_Util_blockCPU(THIRTY_MS_IN_US);

    handle->tid_ack = 1U;
    handle->tid_tx  = 1U;
    handle->numFreeTrxSpiBlocks = MAX_TRX_SPIBLOCKS;

    return(status);
}

TRX_Host_Status TRX_Host_utilSetPowerMode(TRX_Host_Handle handle, TRX_PowerMode mode,
                                          TRX_Host_Callback callback)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;

    while(NULL == pSpiContainer)
    {
        // Block until a buffer is freed in the transport cb
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_UtilPowerMode));
    }

    LOG_INF( "RF: Creating TRX_Request_UtilPowerMode");

    TRX_Request_UtilPowerMode *setPowerModeSpiBlock = (TRX_Request_UtilPowerMode *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_UtilPowerMode));
    setPowerModeSpiBlock->header.len = sizeof(TRX_Request_UtilPowerMode);
    setPowerModeSpiBlock->header.request = Req_UTIL_POWER_MODE;
    setPowerModeSpiBlock->mode = mode;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endSetPowerMode;
    }
    pMap->cb = (uintptr_t)callback;
    pMap->id = TRX_RESERVED_ID;
    pMap->md = 0;
    pMap->req = Req_UTIL_POWER_MODE;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endSetPowerMode:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }

    return(status);
}

TRX_Host_Status TRX_Host_eraseNv(TRX_Host_Handle handle, TRX_Host_Callback callback)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;

    while(NULL == pSpiContainer)
    {
        // Block until a buffer is freed in the transport cb
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_NvErase));
    }

    LOG_INF( "RF: Creating TRX_Request_NvErase");

    TRX_Request_NvErase *nvEraseSpiBlock = (TRX_Request_NvErase *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_NvErase));
    nvEraseSpiBlock->header.len = sizeof(TRX_Request_NvErase);
    nvEraseSpiBlock->header.request = Req_NV_ERASE;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endNvErase;
    }
    pMap->cb = (uintptr_t)callback;
    pMap->id = TRX_RESERVED_ID;
    pMap->md = 0;
    pMap->req = Req_NV_ERASE;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endNvErase:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }

    return(status);
}


TRX_Host_Status TRX_Host_utilSetRfMode(TRX_Host_Handle handle, TRX_RfMode rfMode, TRX_Host_Callback callback)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;

    while(NULL == pSpiContainer)
    {
        // Block until a buffer is freed in the transport cb
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_UtilRfMode));
    }

    LOG_INF( "RF: Creating TRX_Request_UtilRfMode");

    TRX_Request_UtilRfMode *setRfModeSpiBlock = (TRX_Request_UtilRfMode *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_UtilRfMode));
    setRfModeSpiBlock->header.len = sizeof(TRX_Request_UtilRfMode);
    setRfModeSpiBlock->header.request = Req_UTIL_RF_MODE;
    setRfModeSpiBlock->mode = rfMode;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endSetRfMode;
    }
    pMap->cb = (uintptr_t)callback;
    pMap->id = TRX_RESERVED_ID;
    pMap->md = 0;
    pMap->req = Req_UTIL_RF_MODE;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endSetRfMode:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }

    return(status);
}

TRX_Host_Status TRX_Host_utilDioSetup(TRX_Host_Handle handle, TRX_DioConfig dioConfig, TRX_Host_Callback callback)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;

    while(NULL == pSpiContainer)
    {
        // Block until a buffer is freed in the transport cb
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_DioSetup));
    }

    LOG_INF( "RF: Creating TRX_Request_DioSetup");

    TRX_Request_DioSetup *dioSetupSpiBlock = (TRX_Request_DioSetup *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_DioSetup));
    dioSetupSpiBlock->header.len = sizeof(TRX_Request_DioSetup);
    dioSetupSpiBlock->header.request = Req_DIO_SETUP;
    dioSetupSpiBlock->dioConfig = dioConfig;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endDioSetup;
    }
    pMap->cb = (uintptr_t)callback;
    pMap->id = TRX_RESERVED_ID;
    pMap->md = 0;
    pMap->req = Req_DIO_SETUP;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endDioSetup:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }

    return(status);
}

TRX_Host_Status TRX_Host_storeDeviceConfig(TRX_Host_Handle handle,
                                           TRX_DeviceConfigData data,
                                           TRX_Host_Callback callback)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;

    while(NULL == pSpiContainer)
    {
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_DeviceConfigStore));
    }

    LOG_INF("RF: Creating TRX_Request_DeviceConfigStore");

    TRX_Request_DeviceConfigStore *storeSpiBlock = (TRX_Request_DeviceConfigStore *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_DeviceConfigStore));
    storeSpiBlock->header.len = sizeof(TRX_Request_DeviceConfigStore);
    storeSpiBlock->header.request = Req_DEVICE_CONFIG_STORE;
    storeSpiBlock->data = data;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endStoreDeviceConfig;
    }
    pMap->cb = (uintptr_t)callback;
    pMap->id = TRX_RESERVED_ID;
    pMap->md = 0;
    pMap->req = Req_DEVICE_CONFIG_STORE;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endStoreDeviceConfig:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }
    return(status);
}

TRX_Host_Status TRX_Host_persistDeviceConfig(TRX_Host_Handle handle,
                                             bool allowReplace,
                                             TRX_Host_Callback callback)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;

    while(NULL == pSpiContainer)
    {
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_DeviceConfigPersist));
    }

    LOG_INF("RF: Creating TRX_Request_DeviceConfigPersist");

    TRX_Request_DeviceConfigPersist *persistSpiBlock = (TRX_Request_DeviceConfigPersist *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_DeviceConfigPersist));
    persistSpiBlock->header.len = sizeof(TRX_Request_DeviceConfigPersist);
    persistSpiBlock->header.request = Req_DEVICE_CONFIG_PERSIST;
    persistSpiBlock->allowReplace = allowReplace ? 1U : 0U;
    persistSpiBlock->reserved = 0U;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endPersistDeviceConfig;
    }
    pMap->cb = (uintptr_t)callback;
    pMap->id = TRX_RESERVED_ID;
    pMap->md = 0;
    pMap->req = Req_DEVICE_CONFIG_PERSIST;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endPersistDeviceConfig:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }
    return(status);
}

TRX_Host_Status TRX_Host_getDeviceConfig(TRX_Host_Handle handle,
                                         TRX_Host_Callback callback)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;

    while(NULL == pSpiContainer)
    {
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_DeviceConfigGet));
    }

    LOG_INF("RF: Creating TRX_Request_DeviceConfigGet");

    TRX_Request_DeviceConfigGet *getSpiBlock = (TRX_Request_DeviceConfigGet *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_DeviceConfigGet));
    getSpiBlock->header.len = sizeof(TRX_Request_DeviceConfigGet);
    getSpiBlock->header.request = Req_DEVICE_CONFIG_GET;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endGetDeviceConfig;
    }
    pMap->cb = (uintptr_t)callback;
    pMap->id = TRX_RESERVED_ID;
    pMap->md = 0;
    pMap->req = Req_DEVICE_CONFIG_GET;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endGetDeviceConfig:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }
    return(status);
}

TRX_Host_Status TRX_Host_utilShutdown(TRX_Host_Handle handle, TRX_Host_Callback callback)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;

    while(NULL == pSpiContainer)
    {
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_UtilShutdown));
    }

    LOG_INF("RF: Creating TRX_Request_UtilShutdown");

    TRX_Request_UtilShutdown *shutdownSpiBlock = (TRX_Request_UtilShutdown *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_UtilShutdown));
    shutdownSpiBlock->header.len = sizeof(TRX_Request_UtilShutdown);
    shutdownSpiBlock->header.request = Req_UTIL_SHUTDOWN;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endShutdown;
    }
    pMap->cb = (uintptr_t)callback;
    pMap->id = TRX_RESERVED_ID;
    pMap->md = 0;
    pMap->req = Req_UTIL_SHUTDOWN;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endShutdown:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }
    return(status);
}

TRX_Host_Status TRX_Host_flushStream(TRX_Host_Handle handle, uint8_t id, TRX_Host_Callback callback)
{
    TRX_Host_Status status = TRX_Host_Success;
    SPI_Transport_Container *pSpiContainer = NULL;

    TRX_Host_Stream *pStream = resolveStream(handle, id);
    if(NULL == pStream)
    {
        status = TRX_Host_Invalid_Id;
        goto endFlushStream;
    }

    while(NULL == pSpiContainer)
    {
        pSpiContainer = getSpiContainer(handle, sizeof(TRX_Request_StreamFlush));
    }

    LOG_INF("RF: Creating TRX_Request_StreamFlush");

    TRX_Request_StreamFlush *flushSpiBlock = (TRX_Request_StreamFlush *)getNextEmptyRequest(pSpiContainer, sizeof(TRX_Request_StreamFlush));
    flushSpiBlock->header.len = sizeof(TRX_Request_StreamFlush);
    flushSpiBlock->header.request = Req_STREAM_FLUSH;
    flushSpiBlock->stream_id = id;
    flushSpiBlock->reserved = 0;

    SPI_Transport_Last_Status_Map *pMap = getEmptyLastStatusMap(handle);
    if(NULL == pMap)
    {
        status = TRX_Host_No_Resources;
        goto endFlushStream;
    }
    pMap->cb = (uintptr_t)callback;
    pMap->id = id;
    pMap->md = 0;
    pMap->req = Req_STREAM_FLUSH;
    (void)Util_List_push(&(pSpiContainer->lastStatusMaps), (Util_List_Entry *)pMap);

    prepAndSendTransaction(handle, pSpiContainer);
    pSpiContainer = NULL;

endFlushStream:
    if(NULL != pSpiContainer)
    {
        freeSpiContainer(handle, pSpiContainer);
    }
    return(status);
}