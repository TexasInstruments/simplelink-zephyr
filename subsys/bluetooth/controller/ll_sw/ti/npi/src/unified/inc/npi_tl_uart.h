/******************************************************************************

 @file  npi_tl_uart.h

 @brief NPI Transport Layer Module for UART

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2015 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef NPI_TL_UART_H
#define NPI_TL_UART_H

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

// UART ISR Buffer define
#define UART_ISR_BUF_SIZE 128

// ****************************************************************************
// typedefs
// ****************************************************************************
// -----------------------------------------------------------------------------
//! \brief      Typedef for call back function mechanism to notify NPI TL that
//!             an NPI transaction has occurred
//! \param[in]  rxLen     number of bytes received
//! \param[in]  txLen     number of bytes transmitted
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
//! \param[in]  portID		ID value for board specific UART port
//! \param[in]  portParams	Parameters used to initialize UART port
//! \param[in]  npiCBack	Transport Layer call back function
//!
//! \return     void
// -----------------------------------------------------------------------------
extern void NPITLUART_openTransport(uint8_t portID, UART2_Params *portParams,
                                    npiCB_t npiCBack);

// -----------------------------------------------------------------------------
//! \brief      This routine closes Transport Layer port
//!
//! \return     void
// -----------------------------------------------------------------------------
extern void NPITLUART_closeTransport(void);

// -----------------------------------------------------------------------------
//! \brief      This routine reads data from the UART
//!
//! \return     void
// -----------------------------------------------------------------------------
extern void NPITLUART_readTransport(void);

// -----------------------------------------------------------------------------
//! \brief      This routine initializes and begins a UART transfer
//!
//! \param[in]  len - Number of bytes to write.
//!
//! \return     uint16_t - number of bytes written to transport
// -----------------------------------------------------------------------------
extern uint16_t NPITLUART_writeTransport(uint16_t len);

// -----------------------------------------------------------------------------
//! \brief      This routine stops any pending reads
//!
//! \return     void
// -----------------------------------------------------------------------------
extern void NPITLUART_stopTransfer(void);

// -----------------------------------------------------------------------------
//! \brief      This routine is called from the application context when REM RDY
//!             is de-asserted
//!
//! \return     void
// -----------------------------------------------------------------------------
extern void NPITLUART_handleRemRdyEvent(void);

#ifdef __cplusplus
}
#endif

#endif /* NPI_TL_UART_H */
