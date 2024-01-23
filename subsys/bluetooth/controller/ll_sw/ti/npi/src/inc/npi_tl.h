/******************************************************************************

 @file  npi_tl.h

 @brief NPI Transport Layer API

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2015 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef NPI_TL_H
#define NPI_TL_H

#ifdef __cplusplus
extern "C"
{
#endif

// ****************************************************************************
// includes
// ****************************************************************************
#include "hal_types.h"
#include "inc/npi_config.h"

// ****************************************************************************
// defines
// ****************************************************************************


#if defined(NPI_USE_UART)
#define transportInit NPITLUART_initializeTransport
#define transportRead NPITLUART_readTransport
#define transportWrite NPITLUART_writeTransport
#define transportStopTransfer NPITLUART_stopTransfer
#define transportMrdyEvent NPITLUART_handleMrdyEvent
#elif defined(NPI_USE_SPI)
#define transportInit NPITLSPI_initializeTransport
#define transportRead NPITLSPI_readTransport
#define transportWrite NPITLSPI_writeTransport
#define transportStopTransfer NPITLSPI_stopTransfer
#define transportMrdyEvent NPITLSPI_handleMrdyEvent
#endif

// ****************************************************************************
// typedefs
// ****************************************************************************

// -----------------------------------------------------------------------------
//! \brief      Typedef for call back function mechanism to notify NPI Task that
//!             an NPI transaction has occurred
//! \param[in]  int     number of bytes either received or transmitted
//!
//! \return     void
// -----------------------------------------------------------------------------
typedef void (*npiRtosCB_t)(int size);

// -----------------------------------------------------------------------------
//! \brief      Typedef for call back function mechanism to notify NPI Task that
//!             an MRDY edge has occurred
//! \param[in]  void
//! \return     void
// -----------------------------------------------------------------------------
typedef void (*npiMrdyRtosCB_t)();

//*****************************************************************************
// globals
//*****************************************************************************

//*****************************************************************************
// function prototypes
//*****************************************************************************

// -----------------------------------------------------------------------------
//! \brief      This routine initializes the transport layer and opens the port
//!             of the device. Note that based on project defines, either the
//!             UART, USB (CDC), or SPI driver can be used.
//!
//! \param[in]  npiCBTx - Call back function for TX complete event
//! \param[in]  npiCBRx - Call back function for RX event
//! \param[in]  npiCBMrdy - Call back function for MRDY event
//!
//! \return     void
// -----------------------------------------------------------------------------
void NPITL_initTL(npiRtosCB_t npiCBTx, npiRtosCB_t npiCBRx, npiRtosCB_t npiCBMrdy);

// -----------------------------------------------------------------------------
//! \brief      This routine reads data from the transport layer based on len,
//!             and places it into the buffer.
//!
//! \param[in]  buf - Pointer to buffer to place read data.
//! \param[in]  len - Number of bytes to read.
//!
//! \return     uint16 - the number of bytes read from transport
// -----------------------------------------------------------------------------
uint16 NPITL_readTL(uint8 *buf, uint16 len);

// -----------------------------------------------------------------------------
//! \brief      This routine writes data from the buffer to the transport layer.
//!
//! \param[in]  buf - Pointer to buffer to write data from.
//! \param[in]  len - Number of bytes to write.
//!
//! \return     uint16 - the number of bytes written to transport
// -----------------------------------------------------------------------------
uint16 NPITL_writeTL(uint8 *buf, uint16 len);

// -----------------------------------------------------------------------------
//! \brief      This routine is used to handle an MRDY edge from the application
//!             context. Certain operations such as UART_read() cannot be
//!             performed from the actual MRDY hwi handler
//!
//! \return     void
// -----------------------------------------------------------------------------
void NPITL_handleMrdyEvent(void);

// -----------------------------------------------------------------------------
//! \brief      This routine returns the max size receive buffer.
//!
//! \return     uint16 - max size of the receive buffer
// -----------------------------------------------------------------------------
uint16 NPITL_getMaxRxBufSize(void);

// -----------------------------------------------------------------------------
//! \brief      This routine returns the max size transmit buffer.
//!
//! \return     uint16 - max size of the transmit buffer
// -----------------------------------------------------------------------------
uint16 NPITL_getMaxTxBufSize(void);

// -----------------------------------------------------------------------------
//! \brief      Returns number of bytes that are unread in RxBuf
//!
//! \return     uint16 - number of unread bytes
// -----------------------------------------------------------------------------
uint16 NPITL_getRxBufLen(void);

// -----------------------------------------------------------------------------
//! \brief      This routine returns the state of transmission on NPI
//!
//! \return     bool - state of NPI transmission - 1 - active, 0 - not active
// -----------------------------------------------------------------------------
bool NPITL_checkNpiBusy(void);

/*******************************************************************************
 */

#ifdef __cplusplus
}
#endif

#endif /* NPI_TL_H */
