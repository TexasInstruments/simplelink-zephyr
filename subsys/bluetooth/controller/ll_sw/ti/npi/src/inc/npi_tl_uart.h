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

#define NPI_UART_FC_THRESHOLD 48
#define NPI_UART_IDLE_TIMEOUT 6
#define NPI_UART_INT_ENABLE TRUE

#if !defined(NPI_UART_BR)
#define NPI_UART_BR 115200
#endif // !NPI_UART_BR

// UART ISR Buffer define
#ifndef UART_ISR_BUF_SIZE
#define UART_ISR_BUF_SIZE 32
#endif   /* UART_ISR_BUF_SIZE */
#define UART_ISR_BUF_CNT 2

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
//!             a UART transaction
//!
//! \return     void
// -----------------------------------------------------------------------------
void NPITLUART_initializeTransport(Char *tRxBuf, Char *tTxBuf, npiCB_t npiCBack);

// -----------------------------------------------------------------------------
//! \brief      This routine reads data from the UART
//!
//! \return     void
// -----------------------------------------------------------------------------
void NPITLUART_readTransport(void);

// -----------------------------------------------------------------------------
//! \brief      This routine writes copies buffer addr to the transport layer.
//!
//! \param[in]  len - Number of bytes to write.
//!
//! \return     uint16 - number of bytes written to transport
// -----------------------------------------------------------------------------
uint16 NPITLUART_writeTransport(uint16);

// -----------------------------------------------------------------------------
//! \brief      This routine stops any pending reads
//!
//! \return     void
// -----------------------------------------------------------------------------
void NPITLUART_stopTransfer(void);

// -----------------------------------------------------------------------------
//! \brief      This routine is called from the application context when MRDY is
//!             de-asserted
//!
//! \return     void
// -----------------------------------------------------------------------------
void NPITLUART_handleMrdyEvent(void);



#ifdef __cplusplus
}
#endif

#endif /* NPI_TL_UART_H */
