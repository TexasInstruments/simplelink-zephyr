/******************************************************************************

 @file  npi_tl_spi.h

 @brief NPI Transport Layer Module for SPI

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2015 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef NPI_TL_SPI_H
#define NPI_TL_SPI_H

#ifdef __cplusplus
extern "C"
{
#endif

// ****************************************************************************
// includes
// ****************************************************************************

// ****************************************************************************
// defines
// ****************************************************************************
// SPI Peripheral baud rate
#define SPI_PERIPHERAL_BAUD_RATE 8000000
#define NPI_SPI_HEADER_LEN 3


// ****************************************************************************
// typedefs
// ****************************************************************************

// -----------------------------------------------------------------------------
//! \brief      Typedef for call back function mechanism to notify NPI TL that
//!             an NPI transaction has occurred
//! \param[in]  uint16     number of bytes received
//! \param[in]  uint16     number of bytes transmitted
//!
//! \return     void
// -----------------------------------------------------------------------------
typedef void (*npiCB_t)(uint16 Rxlen, uint16 Txlen);

//*****************************************************************************
// globals
//*****************************************************************************

//*****************************************************************************
// function prototypes
//*****************************************************************************
// -----------------------------------------------------------------------------
//! \brief      This routine initializes the transport layer and opens the port
//!             of the device.
//!
//! \param[in]  tRxBuf - pointer to NPI TL Tx Buffer
//! \param[in]  tTxBuf - pointer to NPI TL Rx Buffer
//! \param[in]  npiCBack - NPI TL call back function to be invoked at the end of
//!             a SPI transaction
//!
//! \return     void
// -----------------------------------------------------------------------------
void NPITLSPI_initializeTransport(Char *tRxBuf, Char *tTxBuf, npiCB_t npiCBack);

// -----------------------------------------------------------------------------
//! \brief      This routine reads data from the transport layer
//!             and places it into the buffer.
//!
//! \return     void
// -----------------------------------------------------------------------------
void NPITLSPI_readTransport(void);

// -----------------------------------------------------------------------------
//! \brief      This routine initializes and begins a SPI transaction
//!
//! \param[in]  len - Number of bytes to write.
//!
//! \return     uint16 - number of bytes written to transport
// -----------------------------------------------------------------------------
uint16 NPITLSPI_writeTransport(uint16);

// -----------------------------------------------------------------------------
//! \brief      This routine stops any pending reads
//!
//! \return     void
// -----------------------------------------------------------------------------
void NPITLSPI_stopTransfer(void);

// -----------------------------------------------------------------------------
//! \brief      This routine is called from the application context when MRDY is
//!             de-asserted
//!
//! \return     void
// -----------------------------------------------------------------------------
void NPITLSPI_handleMrdyEvent(void);

#ifdef __cplusplus
}
#endif

#endif /* NPI_TL_SPI_H */
