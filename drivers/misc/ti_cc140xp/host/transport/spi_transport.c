/******************************************************************************
 Group: CMCU LPRF
 Target Device: cc13xx_cc26xx

 ******************************************************************************
 
 Copyright (c) 2019-2026, Texas Instruments Incorporated
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

@file spi_transport.c

@brief Transport module contoller the SPI interface to the TRX

 */

/********************************** Includes **********************************/
/* Standard C Libraries */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

/* Hardware Abstraction */
#include "hal/platform.h"
#include "hal/platform_spi.h"
#include "hal/platform_util.h"

/* Module Header Files */
#include "utility/list/list.h"
#include "utility/utils.h"
#include "transport/spi_transport.h"
#include "utility/buffer_manager/buffer_manager.h"
#include "TRX.h"

/* Zephyr Logging */
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(ti_cc140xp, CONFIG_TI_CC140XP_LOG_LEVEL);

/********************************** Defines ***********************************/
#define TRX_INT_PROBE_LEN         (SPI_TRANSPORT_HEADER_LEN + SPI_TRANSPORT_CRC_SIZE + (sizeof(TRX_Request_LastStatus) * 2))

#define SPI_TRANSPORT_OPEN_MASK              (0b00000001)
#define SPI_TRANSPORT_ACTIVE_MASK            (0b00000010)
#define SPI_TRANSPORT_INT_ASSERTED_MASK      (0b00000100)
#define SPI_TRANSPORT_IS_OPEN(handle)      (handle->status & SPI_TRANSPORT_OPEN_MASK)
#define SPI_TRANSPORT_IS_ACTIVE(handle)    (handle->status & SPI_TRANSPORT_ACTIVE_MASK)
#define SPI_TRANSPORT_IS_INT_ASSERTED(handle)  (handle->status & SPI_TRANSPORT_INT_ASSERTED_MASK)
#define SPI_TRANSPORT_SET_OPEN(handle)     (handle->status |= SPI_TRANSPORT_OPEN_MASK)
#define SPI_TRANSPORT_SET_CLOSED(handle) (handle->status &= ~(SPI_TRANSPORT_OPEN_MASK))
#define SPI_TRANSPORT_SET_ACTIVE(handle)   (handle->status |= SPI_TRANSPORT_ACTIVE_MASK)
#define SPI_TRANSPORT_SET_INACTIVE(handle) (handle->status &= ~(SPI_TRANSPORT_ACTIVE_MASK))
#define SPI_TRANSPORT_SET_INT_ASSERTED(handle)   (handle->status |= SPI_TRANSPORT_INT_ASSERTED_MASK)
#define SPI_TRANSPORT_SET_INT_NOT_ASSERTED(handle) (handle->status &= ~(SPI_TRANSPORT_INT_ASSERTED_MASK))

/********************************** Structs ***********************************/
typedef struct SPI_Transport_Transaction_t
{
    SPI_Transport_Container *rxSPIBlock; //!< SPIBlock allocated from buffer manager
                                        //!< to hold data received from TRX
    uint8_t firstRxLen;  //!< Len of the rx the 1st time sclk is issued
    uint8_t secondRxLen; //!< Len of the rx the 2nd time sclk is issued
    SPI_Transport_Container *txSPIBlock; //!< SPIBlock passed into module for transmission to the TRX
    uint8_t txLen; //!< Total length of the data to TX on the SPI bus
} SPI_Transport_Transaction;

/*!
 *  @brief Object global to the SPI transport controller module used to
 *  organize important information regarding operation.
 */
typedef struct SPI_Transport_Object_t
{
    volatile uint8_t status; //!< Bitmask of the status of the SPI Transport
                             //!< module including MSB to LSB\:
                             //!< INT_ASSERTED | ACTIVE | OPEN
    Plat_SPI_Handle platSPIHandle; //!< Handle to the Platform SPI module
    SPI_Transport_Transaction currentTransaction; //!< Metadata regarding the
                                                  //!< current transaction on the SPI bus
    uintptr_t arg; //!< The argument to pass to the user callbacks from this module
    spiTransportCallback pSpiTransportCb; //!< Callback used after each transaction with the TRX
    spiTransportCSDeassertCallback pCSDeassertedCb; //!< Callback when the
                                                    //!< CS line is deasserted.
    spiTransportGetNextContainer pGetNextSpiContainer; //!< Callback provided
                                        //!< by the upper layer to retrieve
                                        //!< any pending SPI Containers intended
                                        //!< for transmission
} SPI_Transport_Object;

/********************************* Prototypes *********************************/

/*!
 * @brief Processes the next event that may generate a transaction on the SPI
 * bus. This is done by either servicing the txQueue, the INT asserted status,
 * or both. Either will start a new transaction on the SPI bus but only if it's
 * free.
 *
 * @note Calling context: Task, HWI, SWI
 *
 * @param[in] handle Handle to the SPI transport module
 */
static void processNextEvent(SPI_Transport_Handle handle);

/*!
 * @brief Used to determine if the SPI bus is free
 *
 * @note Calling context: Task, HWI, SWI
 *
 * @param[in] handle Handle to the SPI transport module
 *
 * @return true when SPI bus is free, otherwise false
 */
static bool isBusFree(SPI_Transport_Handle handle);

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
 *                            to each execution of this callback. In this case
 *                            the handle to the SPI_Transport module
 */
static void transactionCompleteCb(uint8_t *pTransmitBuf, uint16_t transmitBufLen,
                      uint8_t *pReceiveBuf, uint16_t receiveBufLen,
                      Plat_SPI_Status status, uintptr_t arg);

/*!
 * @brief Indicates that the TRX being communicated with over this SPI
 * interface has asserted its interrupt line implying there is data ready to
 * be sent on the POCI line.
 *
 * @note Calling context: HWI, SWI
 *
 * @param[in] arg Optional additional argument to passed to Plat_SPI_open()
 */
static void intAssertedCb(uintptr_t arg);

/*! @brief Callback used to indicate that it is OK to assert the CS signal again
 *
 * @note Calling context: HWI, SWI
 *
 * @param[in] arg Optional additional argument to passed to Plat_SPI_open()
 */
static void trxReadyCb(uintptr_t arg);

/*************************** Variable Declarations ****************************/
/*!
 * Object global to the SPI platform module used to organize important
 * information regarding operation.
 */
static SPI_Transport_Object SPI_Transport_object = {.status = 0};
volatile uint8_t spiTransportDebugFlags = 0U; /* bit0=open, bit1=active, bit2=trxReady */

/* Diagnostic counters for distinguishing Pattern B failure modes (see
 * stability-failure-insights.md). Both should be 0 at quiescent moments.
 *   spiTxStartedNotCcb  ++ at Plat_SPI_startTransaction success
 *                       -- at transactionCompleteCb entry
 *                       >0 at hang  ⇒  CCB never fired (Hypothesis 2)
 *   spiCcbNotParsed     ++ at transactionCompleteCb entry
 *                       -- at trx_host.c transportCb parse-loop entry
 *                       >0 at hang  ⇒  CCB ran but parse skipped (Hypothesis 1)
 */
volatile uint8_t spiTxStartedNotCcb = 0U;
volatile uint8_t spiCcbNotParsed    = 0U;

/**************************** Function Definitions ****************************/
/*
 * See prototype for more documentation
 */
static bool isBusFree(SPI_Transport_Handle handle)
{
    bool isActive = SPI_TRANSPORT_IS_ACTIVE(handle);
    bool isTrxReady = Plat_SPI_isTrxReady(handle->platSPIHandle);
    bool isOpen = SPI_TRANSPORT_IS_OPEN(handle);
    bool busIsFree = !isActive && isTrxReady && isOpen;
    spiTransportDebugFlags = (uint8_t)(isOpen ? 0x01U : 0U) |
                             (uint8_t)(isActive ? 0x02U : 0U) |
                             (uint8_t)(isTrxReady ? 0x04U : 0U);
	LOG_DBG( "PNE checking isBusFree: transportStatus=0x%x active=%d trxReady=%d open=%d",
			handle->status, isActive, isTrxReady, isOpen);

    return busIsFree;
}

/*
 * See prototype for more documentation
 */
static void trxReadyCb(uintptr_t arg)
{
    SPI_Transport_Handle handle = (SPI_Transport_Handle)arg;
    LOG_DBG( "SPI: TRX Ready CB from platform layer");
    processNextEvent(handle);
}

/*
 * See prototype for more documentation
 */
static void intAssertedCb(uintptr_t arg)
{
    LOG_DBG( "SPI: INT asserted CB calling PNE");
    SPI_Transport_Handle handle = (SPI_Transport_Handle)arg;
    SPI_TRANSPORT_SET_INT_ASSERTED(handle);	
	LOG_DBG( "SPI: intAssertedCb - status after SET_INT_ASSERTED=0x%x", handle->status);
    processNextEvent(handle);
}

/*
 * See prototype for more documentation
 */
static void transactionCompleteCb(uint8_t *pTransmitBuf, uint16_t transmitBufLen,
                      uint8_t *pReceiveBuf, uint16_t receiveBufLen,
                      Plat_SPI_Status status, uintptr_t arg)
{
    LOG_DBG( "SPI: Handling SPI transaction CB from lower layer status=%d receiveBufLen=%d", status, receiveBufLen);

    /* Diagnostic: a started transaction has now reported back. The parse-side
     * counter (spiCcbNotParsed) is incremented only inside the endTransaction
     * branch where a parse is actually expected, so multi-stage CCBs do not
     * leak the counter. See stability-failure-insights.md. */
    if (spiTxStartedNotCcb > 0U) { spiTxStartedNotCcb--; }

    SPI_Transport_Handle handle = (SPI_Transport_Handle)arg;
    SPI_Transport_Transaction *currentTransaction = &(handle->currentTransaction);
    SPI_Transport_Status spiTransportStatus = SPI_Transport_Success;
    bool endTransaction = false;
    bool invalidRx = false;

    if(Plat_SPI_Success == status)
    {
        // The tlen is the only thing this layer interacts with
        TRX_TransactionHeader *receivedHeader = (TRX_TransactionHeader *)&(currentTransaction->rxSPIBlock->trxSpiBlock);
        uint8_t rxTlen = receivedHeader->trx.len;

        // Received tlen determines what actions need to be taken next
        if(SPI_TRANSPORT_MAX_TLEN >= rxTlen)
        {
            // Check if all rx data was received, if not start another transaction
            if((rxTlen + SPI_TRANSPORT_HEADER_LEN + SPI_TRANSPORT_CRC_SIZE) <= (currentTransaction->firstRxLen + currentTransaction->secondRxLen))
            {
                // All data received
                endTransaction = true;
            }
            else
            {
                // Need to start another transaction
                currentTransaction->secondRxLen = (SPI_TRANSPORT_HEADER_LEN + rxTlen + SPI_TRANSPORT_CRC_SIZE) - currentTransaction->firstRxLen;
                endTransaction = false;

                LOG_DBG( "SPI: Need to RX more data, starting another SPI transaction with params %p, %d, %p, %d",
                    NULL, 0,
                    &(((uint8_t *)&(currentTransaction->rxSPIBlock->trxSpiBlock))[currentTransaction->firstRxLen]),
                    currentTransaction->secondRxLen);


                Plat_SPI_Status spiStatus = Plat_SPI_startTransaction(handle->platSPIHandle,
                    NULL, 0,
                    &(((uint8_t *)&(currentTransaction->rxSPIBlock->trxSpiBlock))[currentTransaction->firstRxLen]),
                    currentTransaction->secondRxLen
                );
                Assert_isTrue(Plat_SPI_Success == spiStatus, 9U);

                /* Continuation transaction is now in flight; track it so a
                 * stage-2 CCB miss is visible in spiTxStartedNotCcb. */
                if (spiTxStartedNotCcb < 0xFFU) { spiTxStartedNotCcb++; }
            }
        }
        else
        {
            // Invalid rx data because TLEN is outside legal bounds: the TLEN
            // byte read from the probe header is corrupt. Halt with tag 25U so
            // the failing run is captured rather than silently dropping the RX.
            LOG_ERR( "SPI: assert 25U: invalidRx rxTlen=%d > MAX_TLEN, status=0x%x firstRxLen=%d secondRxLen=%d",
                rxTlen, handle->status, currentTransaction->firstRxLen, currentTransaction->secondRxLen);
            Assert_isTrue(false, 25U);
            invalidRx = true;
            endTransaction = true;
        }
    }
    else
    {
        // Transaction was either cancelled or other unexpected failure occurred.
        // Plat_SPI_Canceled is the legitimate signal for SPI_Transport_close
        // forcing a callback, so it is allowed through. Any other non-Success
        // status is a real transport failure: halt with tag 26U.
        spiTransportStatus = (Plat_SPI_Canceled == status) ? SPI_Transport_Canceled: SPI_Transport_Failed;
        endTransaction = true;
        if (Plat_SPI_Canceled != status)
        {
            LOG_ERR( "SPI: assert 26U: Plat status=%d transportStatus=%d", status, spiTransportStatus);
            Assert_isTrue(false, 26U);
        }
        else
        {
            LOG_INF( "SPI: Transaction canceled (close path): platStatus=%d", status);
        }
    }

    if(endTransaction)
    {
        LOG_INF( "SPI: Transaction complete. Buffer pool: %d/%d allocated",
            Buffer_Manager_getNumAllocated(), BUFFER_MANAGER_NBUFFERS);

        // Save transaction context before processing event which may overwrite it
        SPI_Transport_Container *oldTxSPIBlock = currentTransaction->txSPIBlock;
        SPI_Transport_Container *oldRxSPIBlock = currentTransaction->rxSPIBlock;

        // De-assert CS
        (void)Plat_SPI_deassertCs(handle->platSPIHandle);

        // Set module as inactive
        SPI_TRANSPORT_SET_INACTIVE(handle);
        LOG_DBG( "SPI: transactionCompleteCb - status after SET_INACTIVE=0x%x", handle->status);

        if(NULL != handle->pCSDeassertedCb)
        {
            LOG_DBG( "SPI: Calling upper layer CS deassert cb");

            // Issue CS de-assert callback
            handle->pCSDeassertedCb((uintptr_t)handle->arg);
        }

        // Processing an event may start a new SPI transaction so it's done
        // before the cb's to take advantage of the time when the SPI bus is busy
        LOG_DBG( "SPI: Transaction complete calling PNE");
        processNextEvent(handle);

        // Call user cb
        if(NULL != handle->pSpiTransportCb)
        {

            LOG_DBG( "SPI: Calling upper layer transaction complete cb");

            SPI_Transport_Container *pRxContainer = invalidRx ? NULL : oldRxSPIBlock;

            /* Diagnostic: increment only when the upper-layer parse loop is
             * expected to run (matches the gate at trx_host.c transportCb).
             * Decrement happens at parse-loop entry; multi-stage CCBs and
             * error-path CCBs (caught by tags 25U/26U above) bypass this. */
            if ((NULL != pRxContainer) && (SPI_Transport_Success == spiTransportStatus))
            {
                if (spiCcbNotParsed < 0xFFU) { spiCcbNotParsed++; }
            }

            handle->pSpiTransportCb(oldTxSPIBlock, pRxContainer, spiTransportStatus, handle->arg);
        }

        // Free the RX Block
        Buffer_Manager_Status bufferStatus = Buffer_Manager_freeBuffer((uint8_t *)oldRxSPIBlock);

        LOG_DBG( "SPI: Freed RX buffer with status %d", bufferStatus);

        Assert_isTrue((Buffer_Manager_Success == bufferStatus), 10U);
    }

    LOG_DBG( "SPI: Done handling SPI transaction CB from lower layer");
}

/*
 * See prototype for more documentation
 */
static void processNextEvent(SPI_Transport_Handle handle)
{
    LOG_DBG( "SPI: Processing next event");

    // Enter critical section so async events don't start another transaction
    unsigned int key;
    Plat_Util_startCriticalSection((uintptr_t)&key);
    if(isBusFree(handle))
    {
        SPI_Transport_Transaction *pTransaction = &(handle->currentTransaction);
        (void)memset((uint8_t *)pTransaction, 0U, sizeof(SPI_Transport_Transaction));

        // SPI bus is free let's start sending data
        SPI_Transport_Container *nextTxSpiBlock = handle->pGetNextSpiContainer(handle->arg);
        if(NULL != nextTxSpiBlock)
        {
            LOG_DBG( "SPI: Handling SPI block from upper layer");

            pTransaction->txSPIBlock = nextTxSpiBlock;
            pTransaction->txLen = SPI_TRANSPORT_HEADER_LEN + nextTxSpiBlock->trxSpiBlock.header.host.len + SPI_TRANSPORT_CRC_SIZE;
            pTransaction->firstRxLen = pTransaction->txLen;
        }

        if(SPI_TRANSPORT_IS_INT_ASSERTED(handle))
        {
            LOG_DBG( "SPI: Handling int assertion");

            pTransaction->firstRxLen = (TRX_INT_PROBE_LEN < pTransaction->firstRxLen) ? pTransaction->firstRxLen : TRX_INT_PROBE_LEN;
        }

        if(0U < pTransaction->firstRxLen)
        {
            pTransaction->rxSPIBlock = (SPI_Transport_Container *)Buffer_Manager_getBuffer();

            LOG_DBG( "SPI: Allocated RX SPI Block %p", pTransaction->rxSPIBlock);

            if(NULL == pTransaction->rxSPIBlock)
            {
                LOG_ERR( "SPI: PNE assert 11U: Buffer_Manager pool empty, RX alloc failed");
            }
            Assert_isTrue(NULL != pTransaction->rxSPIBlock, 11U);

            uint8_t *trxTxSpiBlock = (0U != pTransaction->txLen) ? (uint8_t *)&(pTransaction->txSPIBlock->trxSpiBlock) : NULL;

            // Asert the CS line before starting a brand new transaction
            Plat_SPI_Status spiStatus = Plat_SPI_assertCs(handle->platSPIHandle);

            LOG_DBG( "SPI: Asserted CS with status %d", spiStatus);

            if (Plat_SPI_Success != spiStatus)
            {
                LOG_ERR( "SPI: PNE assert 12U: Plat_SPI_assertCs FAILED status=%d", spiStatus);
            }

            Assert_isTrue(Plat_SPI_Success == spiStatus, 12U);

            SPI_TRANSPORT_SET_INT_NOT_ASSERTED(handle);
            SPI_TRANSPORT_SET_ACTIVE(handle);
            LOG_DBG( "SPI: Starting SPI transaction with params %p, %d, %p, %d",
                trxTxSpiBlock, handle->currentTransaction.txLen,
                (uint8_t *)&(handle->currentTransaction.rxSPIBlock->trxSpiBlock),
                handle->currentTransaction.firstRxLen);
            // Start the transaction
            spiStatus = Plat_SPI_startTransaction(handle->platSPIHandle,
                trxTxSpiBlock,
                handle->currentTransaction.txLen,
                (uint8_t *)&(handle->currentTransaction.rxSPIBlock->trxSpiBlock),
                handle->currentTransaction.firstRxLen
            );

            LOG_DBG( "SPI: Transaction start returned status %d", spiStatus);

            if (Plat_SPI_Success != spiStatus)
            {
                LOG_ERR( "SPI: PNE assert 13U: Plat_SPI_startTransaction FAILED status=%d, ACTIVE flag is now stuck", spiStatus);
            }

            Assert_isTrue(Plat_SPI_Success == spiStatus, 13U);

            /* Diagnostic: tx started, awaiting CCB. See stability-failure-insights.md. */
            if (spiTxStartedNotCcb < 0xFFU) { spiTxStartedNotCcb++; }
        }
        else
        {
            LOG_DBG( "PNE noop: sts=0x%x cont=%d", handle->status, (nextTxSpiBlock != NULL));
        }
    }
    Plat_Util_endCriticalSection((uintptr_t)&key);

    LOG_DBG( "SPI: Done processing event");

}

/*
 * See prototype for more documentation
 */
void SPI_Transport_Params_init(SPI_Transport_Params *pParams)
{
    if(NULL != pParams)
    {
        pParams->bitRate = PLATFORM_DEFAULT_SPI_BITRATE;
        pParams->arg = (uintptr_t)NULL;
        pParams->pCSDeassertCb = NULL;
        pParams->pSpiTransportCb = NULL;
    }
}

/*
 * See prototype for more documentation
 */
SPI_Transport_Handle SPI_Transport_open(const SPI_Transport_Params *pParams)
{
    LOG_DBG( "SPI: Opening SPI Transport Module");

    SPI_Transport_Handle handle = &SPI_Transport_object;

    if(SPI_TRANSPORT_IS_OPEN(handle) || (NULL == pParams))
    {
        handle = NULL;
        goto endSpiTransportOpen;
    }
    else if((NULL == pParams->pCSDeassertCb) ||
            (NULL == pParams->pSpiTransportCb) ||
            (NULL == pParams->pGetNextSpiContainer))
    {
        handle = NULL;
        goto endSpiTransportOpen;
    }

    handle->arg = pParams->arg;
    handle->currentTransaction.firstRxLen = 0U;
    handle->currentTransaction.rxSPIBlock = NULL;
    handle->currentTransaction.secondRxLen = 0U;
    handle->currentTransaction.txLen = 0U;
    handle->currentTransaction.txSPIBlock = NULL;
    handle->pCSDeassertedCb = pParams->pCSDeassertCb;
    handle->pSpiTransportCb = pParams->pSpiTransportCb;
    handle->pGetNextSpiContainer = pParams->pGetNextSpiContainer;
    handle->status = 0U;
    SPI_TRANSPORT_SET_INACTIVE(handle);
    SPI_TRANSPORT_SET_INT_NOT_ASSERTED(handle);
    SPI_TRANSPORT_SET_OPEN(handle);

    const Plat_SPI_Params spiParams = {
        .bitRate = pParams->bitRate,
        .arg = (uintptr_t)handle,
        .pTrxReadyCallback = trxReadyCb,
        .pIntAssertedCb = intAssertedCb,
        .pTransactionCompleteCb = transactionCompleteCb,
    };
    handle->platSPIHandle = Plat_SPI_open(&spiParams);
    if(NULL == handle->platSPIHandle)
    {
        // De-init SPI transport module
        handle->status = 0U;
        handle = NULL;
        goto endSpiTransportOpen;
    }

endSpiTransportOpen:
    return(handle);
}

/*
 * See prototype for more documentation
 */
SPI_Transport_Status SPI_Transport_close(SPI_Transport_Handle handle)
{
    LOG_DBG( "SPI: Closing SPI Transport Module");

    SPI_Transport_Status status = SPI_Transport_Invalid_Handle;

    if(NULL != handle)
    {
        if(SPI_TRANSPORT_IS_OPEN(handle))
        {
            while(SPI_TRANSPORT_IS_ACTIVE(handle))
            {
                // pend until module isn't actively txing/rxing anymore
            }

            // Enter critical section so async events don't start another transaction
            unsigned int key;
            Plat_Util_startCriticalSection((uintptr_t)&key);

            // If the module is active, this will force a callback
            Plat_SPI_Status platSPIStatus = Plat_SPI_close(handle->platSPIHandle);
            if(Plat_SPI_Success != platSPIStatus)
            {

                LOG_DBG( "SPI: Failed closing due to status returned from Plat_SPI_close(): %d", platSPIStatus);
                status = SPI_Transport_Failed;
            }
            else
            {
                LOG_DBG( "SPI: Successfully closed SPI");

                SPI_TRANSPORT_SET_CLOSED(handle);
                status = SPI_Transport_Success;
            }

            Plat_Util_endCriticalSection((uintptr_t)&key);
        }
        else
        {
            // Module not open
            LOG_DBG( "SPI: Module not open");

            status = SPI_Transport_Failed;
        }
    }

    return(status);
}

/*
 * See prototype for more documentation
 */
SPI_Transport_Status SPI_Transport_transmit(SPI_Transport_Handle handle)
{

    LOG_DBG( "SPI: SPI_Transport_transmit called");

    SPI_Transport_Status status = SPI_Transport_Success;

    // Verify the handle is non-NULL before dereferencing it
    if(NULL == handle)
    {
        // Invalid handle
        LOG_DBG( "SPI: Failed transmit due to NULL handle");

        status = SPI_Transport_Invalid_Handle;
    }
    else if(!SPI_TRANSPORT_IS_OPEN(handle))
    {
        // Module hasn't been opened or invalid input args
        LOG_DBG( "SPI: Failed transmit because module not open");

        status = SPI_Transport_Failed;
    }
    else
    {
        LOG_DBG( "API/Stream calls PNE: sts=0x%x rdy=%d", handle->status, Plat_SPI_isTrxReady(handle->platSPIHandle));
        // Process next event on the queue
        processNextEvent(handle);
    }

    return(status);
}
