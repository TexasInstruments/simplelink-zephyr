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

@file platform_spi.c

@brief Platform specific SPI driver implementation for Zephyr

*/

/********************************** Includes **********************************/
/* Standard C Libraries */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

/* Zephyr Headers */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>

/* Module Header Files */
#include "TRX.h"
#include "hal/platform.h"
#include "hal/platform_spi.h"
#include "hal/platform_util.h"
#include "transport/spi_transport.h"

/********************************** Defines ***********************************/

#define THIRTY_MS_IN_US    (30000U)
#define PLAT_SPI_TRX_READY_POLL_INTERVAL_US  (30U)
#define PLAT_SPI_POCI_WAIT_TIMEOUT_US        (10000U)

/* Maximum SPI transaction size for dummy TX buffer */
#define PLAT_SPI_MAX_DUMMY_TX_SIZE (256U)

/* Timing debug GPIO removed - was causing conflicts with reset pin on some boards */

#define PLAT_SPI_OPEN_MASK            (0b00000001)
#define PLAT_SPI_ACTIVE_MASK          (0b00000010)
#define PLAT_SPI_TRX_READY_MASK       (0b00000100)
#define PLAT_SPI_CS_ASSERTED_MASK     (0b00001000)
#define PLAT_SPI_IS_OPEN(handle)      (handle->status & PLAT_SPI_OPEN_MASK)
#define PLAT_SPI_IS_ACTIVE(handle)    (handle->status & PLAT_SPI_ACTIVE_MASK)
#define PLAT_SPI_IS_TRX_READY(handle) (handle->status & PLAT_SPI_TRX_READY_MASK)
#define PLAT_SPI_IS_CS_ASSERTED(handle)   (handle->status & PLAT_SPI_CS_ASSERTED_MASK)
#define PLAT_SPI_SET_OPEN(handle)     (handle->status |= PLAT_SPI_OPEN_MASK)
#define PLAT_SPI_SET_NOT_OPEN(handle) (handle->status &= ~(PLAT_SPI_OPEN_MASK))
#define PLAT_SPI_SET_ACTIVE(handle)   (handle->status |= PLAT_SPI_ACTIVE_MASK)
#define PLAT_SPI_SET_INACTIVE(handle) (handle->status &= ~(PLAT_SPI_ACTIVE_MASK))
#define PLAT_SPI_SET_TRX_READY(handle) (handle->status |= PLAT_SPI_TRX_READY_MASK)
#define PLAT_SPI_SET_TRX_NOT_READY(handle) (handle->status &= ~(PLAT_SPI_TRX_READY_MASK))
#define PLAT_SPI_SET_CS_ASSERTED(handle)  (handle->status |= PLAT_SPI_CS_ASSERTED_MASK)
#define PLAT_SPI_CLR_CS_ASSERTED(handle)  (handle->status &= ~(PLAT_SPI_CS_ASSERTED_MASK))

/* Get the TI CC140xP device from device tree */
#define TI_CC140XP_NODE DT_INST(0, ti_cc140xp_trx)


/********************************** Structs ***********************************/

/**
 *  @brief Object global to the SPI platform module
 */
typedef struct Plat_SPI_Object_t
{
    volatile uint8_t status;
    const struct device *spi_dev;
    struct spi_config spi_cfg;
    struct gpio_dt_spec cs_gpio;
    struct gpio_dt_spec reset_gpio;
    struct gpio_dt_spec int_gpio;
    struct gpio_dt_spec poci_gpio;
    struct gpio_callback int_callback;
    struct k_timer ready_timer;

    platSpiTransactionCompleteCallback pTransactionCompleteCb;
    platSpiIntAssertedCallback pIntAssertedCb;
    platSpiTrxReadyCallback pTrxReadyCb;

    uint8_t *pTransmitBuf;
    uint16_t transmitLen;
    uint8_t *pReceiveBuf;
    uint16_t receiveLen;

    /* Dummy TX buffer filled with 0x00 for RX-only transactions */
    uint8_t dummyTxBuf[PLAT_SPI_MAX_DUMMY_TX_SIZE];

    /* Persistent SPI buffer descriptors — must outlive the async callback.
     * Zephyr's spi_context keeps ctx->current_tx pointing into these structs
     * and re-reads them in spi_context_update_tx after the DMA ISR fires.
     * Stack-allocated descriptors are freed before the ISR runs, causing a
     * spurious second DMA that overwrites pReceiveBuf with 0xFF. */
    struct spi_buf     tx_spi_buf;
    struct spi_buf_set tx_spi_buf_set;
    struct spi_buf     rx_spi_buf;
    struct spi_buf_set rx_spi_buf_set;

    uintptr_t arg;
} Plat_SPI_Object;

/********************************* Prototypes *********************************/

static void trx_ready_timer_handler(struct k_timer *timer);
static void int_asserted_handler(const struct device *dev,
                                 struct gpio_callback *cb,
                                 uint32_t pins);
static void spi_async_callback(const struct device *dev, int result, void *userdata);

/*************************** Variable Declarations ****************************/

/* Timing debug function removed */

static Plat_SPI_Object Plat_SPI_object = {
    .status = 0,
    .spi_dev = NULL,
    .pTransactionCompleteCb = NULL,
    .pIntAssertedCb = NULL,
    .pTrxReadyCb = NULL,
    .pTransmitBuf = NULL,
    .transmitLen = 0U,
    .pReceiveBuf = NULL,
    .receiveLen = 0U,
    .dummyTxBuf = {0}, /* Initialize all bytes to 0x00 */
};

/**************************** Function Definitions ****************************/

static void trx_ready_timer_handler(struct k_timer *timer)
{
    Plat_SPI_Handle handle = CONTAINER_OF(timer, Plat_SPI_Object, ready_timer);

    if (!PLAT_SPI_IS_TRX_READY(handle)) {
        PLAT_SPI_SET_TRX_READY(handle);
        if (NULL != handle->pTrxReadyCb) {
            handle->pTrxReadyCb(handle->arg);
        }
    }
}

static void int_asserted_handler(const struct device *dev,
                                struct gpio_callback *cb,
                                uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(pins);

    Plat_SPI_Handle handle = CONTAINER_OF(cb, Plat_SPI_Object, int_callback);

    k_timer_stop(&handle->ready_timer);

    PLAT_SPI_SET_TRX_READY(handle);
    if (NULL != handle->pIntAssertedCb) {
        handle->pIntAssertedCb(handle->arg);
    }
}

void Plat_SPI_Params_init(Plat_SPI_Params *pParams)
{
    if (NULL != pParams) {
        pParams->bitRate = PLATFORM_DEFAULT_SPI_BITRATE;
        pParams->arg = (uintptr_t)NULL;
        pParams->pTransactionCompleteCb = NULL;
        pParams->pTrxReadyCallback = NULL;
        pParams->pIntAssertedCb = NULL;
    }
}

Plat_SPI_Status Plat_SPI_assertCs(Plat_SPI_Handle handle)
{
    unsigned int key;
    Plat_Util_startCriticalSection((uintptr_t)&key);
    Plat_SPI_Status status = Plat_SPI_Invalid_Handle;

    if (NULL != handle) {
        if (PLAT_SPI_IS_OPEN(handle)) {
            if (PLAT_SPI_IS_TRX_READY(handle)) {
                int ret = gpio_pin_set_dt(&handle->cs_gpio, 1);
                if (ret < 0) {
                    status = Plat_SPI_Failed;
                } else {
                    PLAT_SPI_SET_TRX_NOT_READY(handle);
                    /* Mark CS as freshly asserted so Plat_SPI_startTransaction
                     * checks POCI immediately before the first CLK edge, per
                     * the Plat_SPI_startTransaction API contract. */
                    PLAT_SPI_SET_CS_ASSERTED(handle);
                    status = Plat_SPI_Success;
                }
            } else {
                status = Plat_SPI_Cs_Not_Ready;
            }
        } else {
            status = Plat_SPI_Failed;
        }
    }
    Plat_Util_endCriticalSection((uintptr_t)&key);

    return status;
}

Plat_SPI_Status Plat_SPI_deassertCs(Plat_SPI_Handle handle)
{
    unsigned int key;
    Plat_Util_startCriticalSection((uintptr_t)&key);
    Plat_SPI_Status status = Plat_SPI_Invalid_Handle;

    if (NULL != handle) {
        if (PLAT_SPI_IS_OPEN(handle)) {
            int ret = gpio_pin_set_dt(&handle->cs_gpio, 0);
            if (ret < 0) {
                status = Plat_SPI_Failed;
            } else {
                status = Plat_SPI_Success;
                PLAT_SPI_SET_TRX_NOT_READY(handle);

                /* Start the ready timer */
                k_timer_start(&handle->ready_timer,
                            K_USEC(PLAT_SPI_TRX_READY_POLL_INTERVAL_US),
                            K_NO_WAIT);
            }
        } else {
            status = Plat_SPI_Failed;
        }
    }
    Plat_Util_endCriticalSection((uintptr_t)&key);

    return status;
}

bool Plat_SPI_isTrxReady(Plat_SPI_Handle handle)
{
    unsigned int key;
    Plat_Util_startCriticalSection((uintptr_t)&key);
    bool ready = false;

    if (NULL != handle) {
        if (PLAT_SPI_IS_OPEN(handle)) {
            ready = PLAT_SPI_IS_TRX_READY(handle);
        }
    }
    Plat_Util_endCriticalSection((uintptr_t)&key);

    return ready;
}

Plat_SPI_Handle Plat_SPI_open(const Plat_SPI_Params *pParams)
{
    Plat_SPI_Handle handle = &Plat_SPI_object;
    int ret;

    if (PLAT_SPI_IS_OPEN(handle) || (NULL == pParams)) {
        return NULL;
    }

    if ((NULL == pParams->pTrxReadyCallback) ||
        (NULL == pParams->pIntAssertedCb) ||
        (NULL == pParams->pTransactionCompleteCb)) {
        return NULL;
    }

    /* Get SPI device from device tree */
    handle->spi_dev = DEVICE_DT_GET(DT_BUS(TI_CC140XP_NODE));
    if (!device_is_ready(handle->spi_dev)) {
        return NULL;
    }

    /* Configure SPI - Mode 0 (CPOL=0, CPHA=0) */
    handle->spi_cfg.frequency = pParams->bitRate;
    handle->spi_cfg.operation = SPI_OP_MODE_MASTER |
                               SPI_WORD_SET(8) |
                               SPI_TRANSFER_MSB;
    handle->spi_cfg.slave = 0;  /* Slave address - usually 0 for SPI */
    /* CS controlled manually - zero initialize the struct */
    memset(&handle->spi_cfg.cs, 0, sizeof(handle->spi_cfg.cs));

    /* Initialize GPIO specs from device tree - use static initializers */
    /* CS GPIO comes from parent SPI node's cs-gpios property */
    const struct gpio_dt_spec cs_gpio_init = SPI_CS_GPIOS_DT_SPEC_GET(TI_CC140XP_NODE);
    const struct gpio_dt_spec reset_gpio_init = GPIO_DT_SPEC_GET(TI_CC140XP_NODE, reset_gpios);
    const struct gpio_dt_spec int_gpio_init = GPIO_DT_SPEC_GET(TI_CC140XP_NODE, trx_host_spi_int_gpios);
    const struct gpio_dt_spec poci_gpio_init = GPIO_DT_SPEC_GET(TI_CC140XP_NODE, poci_gpios);

    memcpy(&handle->cs_gpio, &cs_gpio_init, sizeof(handle->cs_gpio));
    memcpy(&handle->reset_gpio, &reset_gpio_init, sizeof(handle->reset_gpio));
    memcpy(&handle->int_gpio, &int_gpio_init, sizeof(handle->int_gpio));
    memcpy(&handle->poci_gpio, &poci_gpio_init, sizeof(handle->poci_gpio));

    /* Configure CS GPIO (manual control) - only if valid */
    if (handle->cs_gpio.port != NULL) {
        if (!gpio_is_ready_dt(&handle->cs_gpio)) {
            return NULL;
        }
        ret = gpio_pin_configure_dt(&handle->cs_gpio, GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            return NULL;
        }
    }

    /* Configure Reset GPIO as output, starting HIGH (out of reset) */
    if (!gpio_is_ready_dt(&handle->reset_gpio)) {
        return NULL;
    }
    ret = gpio_pin_configure_dt(&handle->reset_gpio, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return NULL;
    }

    /* Configure INT GPIO with pull-up (active-low signal needs pull-up) */
    if (!gpio_is_ready_dt(&handle->int_gpio)) {
        return NULL;
    }
    ret = gpio_pin_configure_dt(&handle->int_gpio, GPIO_INPUT | GPIO_PULL_UP);
    if (ret < 0) {
        return NULL;
    }

    /* Setup interrupt callback */
    ret = gpio_pin_interrupt_configure_dt(&handle->int_gpio, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        return NULL;
    }

    gpio_init_callback(&handle->int_callback, int_asserted_handler,
                      BIT(handle->int_gpio.pin));
    ret = gpio_add_callback(handle->int_gpio.port, &handle->int_callback);
    if (ret < 0) {
        return NULL;
    }

    /* Initialize ready timer */
    k_timer_init(&handle->ready_timer, trx_ready_timer_handler, NULL);

    /* Set callbacks */
    handle->pTransactionCompleteCb = pParams->pTransactionCompleteCb;
    handle->pIntAssertedCb = pParams->pIntAssertedCb;
    handle->pTrxReadyCb = pParams->pTrxReadyCallback;
    handle->arg = pParams->arg;

    PLAT_SPI_SET_INACTIVE(handle);
    PLAT_SPI_SET_OPEN(handle);
    PLAT_SPI_SET_TRX_NOT_READY(handle);

    /* Reset sequence: assert (active = physical LOW = in reset), release (inactive = HIGH = running) */
    gpio_pin_set_dt(&handle->reset_gpio, 1);  /* Assert reset (active-low: physical LOW = in reset) */
    k_busy_wait(1000);
    gpio_pin_set_dt(&handle->reset_gpio, 0);  /* Release reset (inactive: physical HIGH = running) */

    /* Start ready timer and wait for TRX to be ready */
    k_timer_start(&handle->ready_timer,
                 K_USEC(PLAT_SPI_TRX_READY_POLL_INTERVAL_US),
                 K_NO_WAIT);

    while (!PLAT_SPI_IS_TRX_READY(handle)) {
        k_yield();
    }

    /* Wait for TRX SPI slave stack to initialize after reset.
     * CC1407P requires this time before it will respond to CS with POCI. */
    Plat_Util_blockCPU(THIRTY_MS_IN_US);

    return handle;
}

Plat_SPI_Status Plat_SPI_close(Plat_SPI_Handle handle)
{
    if (NULL == handle) {
        return Plat_SPI_Invalid_Handle;
    }

    if (PLAT_SPI_IS_OPEN(handle)) {
        /* Stop timer */
        k_timer_stop(&handle->ready_timer);

        /* Disable interrupt */
        gpio_pin_interrupt_configure_dt(&handle->int_gpio, GPIO_INT_DISABLE);
        gpio_remove_callback(handle->int_gpio.port, &handle->int_callback);

        PLAT_SPI_SET_NOT_OPEN(handle);
        return Plat_SPI_Success;
    }

    return Plat_SPI_Success;
}

static void spi_async_callback(const struct device *dev, int result, void *userdata)
{
    Plat_SPI_Object *handle = (Plat_SPI_Object *)userdata;
    Plat_SPI_Status status;

    PLAT_SPI_SET_INACTIVE(handle);

    if (result < 0) {
        status = Plat_SPI_Failed;
    } else {
        status = Plat_SPI_Success;
    }

    if (NULL != handle->pTransactionCompleteCb) {
        handle->pTransactionCompleteCb(handle->pTransmitBuf, handle->transmitLen,
                                      handle->pReceiveBuf, handle->receiveLen,
                                      status, handle->arg);
    }
}

Plat_SPI_Status Plat_SPI_startTransaction(Plat_SPI_Handle handle,
                                          uint8_t *pTransmitBuf,
                                          uint16_t transmitBufLen,
                                          uint8_t *pReceiveBuf,
                                          uint16_t receiveBufLen)
{
    Plat_SPI_Status status = Plat_SPI_Success;
    int ret;

    if (NULL == handle) {
        return Plat_SPI_Invalid_Handle;
    }

    if (PLAT_SPI_IS_ACTIVE(handle)) {
        return Plat_SPI_Busy;
    }

    if (!PLAT_SPI_IS_OPEN(handle)) {
        return Plat_SPI_Failed;
    }

    if (((0 < transmitBufLen) && (NULL == pTransmitBuf)) ||
        ((0 < receiveBufLen) && (NULL == pReceiveBuf))) {
        return Plat_SPI_Failed;
    }

    if ((0 == transmitBufLen) && (0 == receiveBufLen)) {
        return Plat_SPI_Failed;
    }

    /* Save transaction parameters */
    handle->pTransmitBuf = pTransmitBuf;
    handle->transmitLen = transmitBufLen;
    handle->pReceiveBuf = pReceiveBuf;
    handle->receiveLen = receiveBufLen;

    PLAT_SPI_SET_ACTIVE(handle);

    /* Prepare SPI buffer sets.
     * Use persistent struct fields — NOT stack variables — because Zephyr's
     * spi_context stores ctx->current_tx pointing into these structs and
     * re-reads them from spi_context_update_tx inside the DMA ISR, which runs
     * after this function has returned and its stack frame is gone. */
    uint8_t *actualTxBuf = pTransmitBuf;
    uint16_t actualTxLen = transmitBufLen;

    if ((NULL == pTransmitBuf) && (receiveBufLen > 0)) {
        /* RX-only: use dummy buffer filled with 0x00 */
        if (receiveBufLen <= PLAT_SPI_MAX_DUMMY_TX_SIZE) {
            actualTxBuf = handle->dummyTxBuf;
            actualTxLen = receiveBufLen;
        } else {
            /* Buffer too large for dummy TX */
            PLAT_SPI_SET_INACTIVE(handle);
            return Plat_SPI_Failed;
        }
    }

    handle->tx_spi_buf.buf = actualTxBuf;
    handle->tx_spi_buf.len = actualTxLen;
    handle->tx_spi_buf_set.buffers = &handle->tx_spi_buf;
    handle->tx_spi_buf_set.count = 1U;

    handle->rx_spi_buf.buf = pReceiveBuf;
    handle->rx_spi_buf.len = receiveBufLen;
    handle->rx_spi_buf_set.buffers = pReceiveBuf ? &handle->rx_spi_buf : NULL;
    handle->rx_spi_buf_set.count = pReceiveBuf ? 1U : 0U;

    /* Wait for CC1407P to drive POCI low before first CLK.
     * Only applies to first-stage transactions (CS just asserted);
     * continuations skip this since CS was never deasserted.
     *
     * CC1407P drives POCI LOW ~124µs after CS assertion to signal ready.
     * Read via gpio_pin_get_raw directly — do NOT call gpio_pin_configure_dt
     * first, as that ORs in GPIO_ACTIVE_LOW from the DT spec which sets
     * INV_INPT in the CC27xx IOC and inverts the read.  The GPIO input
     * register reflects the physical pad voltage regardless of pin mux mode,
     * so reading without reconfiguring is both correct and safe. */
    if (PLAT_SPI_IS_CS_ASSERTED(handle)) {
        PLAT_SPI_CLR_CS_ASSERTED(handle);
        uint32_t elapsed = 0U;
        while (gpio_pin_get_raw(handle->poci_gpio.port,
                                handle->poci_gpio.pin) != 0) {
            k_busy_wait(1U);
            if (++elapsed >= PLAT_SPI_POCI_WAIT_TIMEOUT_US) {
                PLAT_SPI_SET_INACTIVE(handle);
                gpio_pin_set_dt(&handle->cs_gpio, 0);
                k_timer_start(&handle->ready_timer,
                              K_USEC(PLAT_SPI_TRX_READY_POLL_INTERVAL_US),
                              K_NO_WAIT);
                return Plat_SPI_Failed;
            }
        }
    }

    ret = spi_transceive_cb(handle->spi_dev, &handle->spi_cfg,
                            &handle->tx_spi_buf_set,
                            pReceiveBuf ? &handle->rx_spi_buf_set : NULL,
                            spi_async_callback, (void *)handle);

    if (ret < 0) {
        status = Plat_SPI_Failed;
        PLAT_SPI_SET_INACTIVE(handle);
        if (NULL != handle->pTransactionCompleteCb) {
            handle->pTransactionCompleteCb(pTransmitBuf, transmitBufLen,
                                          pReceiveBuf, receiveBufLen,
                                          status, handle->arg);
        }
    } else {
        status = Plat_SPI_Success;
    }

    return status;
}
