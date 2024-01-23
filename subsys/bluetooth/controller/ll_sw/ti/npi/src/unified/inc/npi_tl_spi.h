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

#define NPI_SPI_HEADER_LEN 3

// ****************************************************************************
// typedefs
// ****************************************************************************

// -----------------------------------------------------------------------------
//! \brief      Typedef for call back function mechanism to notify NPI TL that
//!             an NPI transaction has occurred
//! \param[in]  rxLen	number of bytes received
//! \param[in]  txLen	number of bytes transmitted
//!
//! \return     void
// -----------------------------------------------------------------------------
typedef void (*npiCB_t)(uint16_t rxLen, uint16_t txLen);

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
//! \param[in]  portID		ID value for board specific SPI port
//! \param[in]  portParams	Parameters used to initialize SPI port
//! \param[in]  npiCBack	Transport Layer call back function
//!
//! \return     void
// -----------------------------------------------------------------------------
extern void NPITLSPI_openTransport(uint8_t portID, SPI_Params *portParams,
                                   npiCB_t npiCBack);

// -----------------------------------------------------------------------------
//! \brief      This routine closes Transport Layer port
//!
//! \return     void
// -----------------------------------------------------------------------------
extern void NPITLSPI_closeTransport(void);

// -----------------------------------------------------------------------------
//! \brief      This routine reads data from the transport layer
//!             and places it into the buffer.
//!
//! \return     void
// -----------------------------------------------------------------------------
extern void NPITLSPI_readTransport(void);

// -----------------------------------------------------------------------------
//! \brief      This routine initializes and begins a SPI transaction
//!
//! \param[in]  len - Number of bytes to write.
//!
//! \return     uint16_t - number of bytes written to transport
// -----------------------------------------------------------------------------
extern uint16_t NPITLSPI_writeTransport(uint16_t len);

// -----------------------------------------------------------------------------
//! \brief      This routine stops any pending reads
//!
//! \return     void
// -----------------------------------------------------------------------------
extern void NPITLSPI_stopTransfer(void);

// -----------------------------------------------------------------------------
//! \brief      This routine is called from the application context when REM RDY
//!             is de-asserted
//!
//! \return     void
// -----------------------------------------------------------------------------
extern void NPITLSPI_handleRemRdyEvent(void);

#ifdef __cplusplus
}
#endif
#endif /* NPI_TL_SPI_H */
