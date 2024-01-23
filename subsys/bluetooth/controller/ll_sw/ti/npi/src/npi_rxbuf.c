/******************************************************************************

 @file  npi_rxbuf.c

 @brief NPI RX Buffer and utilities

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2015 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

// ****************************************************************************
// includes
// ****************************************************************************
#include <string.h>
#ifndef FREERTOS
#include <xdc/std.h>
#endif

#include <ti_drivers_config.h>
#include "hal_types.h"
#include "inc/npi_config.h"
#include "inc/npi_tl.h"

// ****************************************************************************
// defines
// ****************************************************************************
#define NPIRXBUF_RXHEAD_INC(x)   RxBufHead += x;               \
    RxBufHead %= NPI_TL_BUF_SIZE;

#define NPIRXBUF_RXTAIL_INC(x)   RxBufTail += x;               \
    RxBufTail %= NPI_TL_BUF_SIZE;

// ****************************************************************************
// typedefs
// ****************************************************************************

//*****************************************************************************
// globals
//*****************************************************************************

//Receive Buffer for all NPI messages
static uint8 RxBuf[NPI_TL_BUF_SIZE];
static uint16 RxBufHead = 0;
static uint16 RxBufTail = 0;

//*****************************************************************************
// function prototypes
//*****************************************************************************

// -----------------------------------------------------------------------------
//! \brief      NPIRxBuf_Read
//!
//! \param[in]  len -
//!
//! \return     uint16 -
// -----------------------------------------------------------------------------
uint16 NPIRxBuf_Read(uint16 len)
{
    uint16 partialLen = 0;

    // Need to make two reads due to wrap around of circular buffer
    if ((len + RxBufTail) > NPI_TL_BUF_SIZE)
    {
        partialLen = NPI_TL_BUF_SIZE - RxBufTail;
        NPITL_readTL(&RxBuf[RxBufTail],partialLen);
        len -= partialLen;
        RxBufTail = 0;
    }

    // Read remainder of data from Transport Layer
    NPITL_readTL(&RxBuf[RxBufTail],len);
    NPIRXBUF_RXTAIL_INC(len);

    // Return len to original size
    len += partialLen;

    return len;
}

// -----------------------------------------------------------------------------
//! \brief      Returns number of bytes that are unparsed in RxBuf
//!
//! \return     uint16 -
// -----------------------------------------------------------------------------
uint16 NPIRxBuf_GetRxBufCount(void)
{
    return ((RxBufTail - RxBufHead) + NPI_TL_BUF_SIZE) % NPI_TL_BUF_SIZE;
}

// -----------------------------------------------------------------------------
//! \brief      Returns number of bytes that are available in RxBuf
//!
//! \return     uint16 -
// -----------------------------------------------------------------------------
uint16 NPIRxBuf_GetRxBufAvail(void)
{
    return (NPI_TL_BUF_SIZE - NPIRxBuf_GetRxBufCount());
}

// -----------------------------------------------------------------------------
//! \brief      NPIRxBuf_ReadFromRxBuf
//!
//! \return     uint16 -
// -----------------------------------------------------------------------------
uint16 NPIRxBuf_ReadFromRxBuf(uint8_t *buf, uint16 len)
{
	uint16_t idx;
    for (idx = 0; idx < len; idx++)
    {
        *buf++ = RxBuf[RxBufHead];
        NPIRXBUF_RXHEAD_INC(1)
    }

    return len;
}
