/******************************************************************************

 @file  npi_export_osprey.h

 @brief This file contains the exported functions required for Osprey

 Group: WCS, LPC, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: BSD3 2015 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/
#ifndef NPI_EXPORT_OSPREY_H
#define NPI_EXPORT_OSPREY_H

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


// ****************************************************************************
// typedefs
// ****************************************************************************


//*****************************************************************************
// globals
//*****************************************************************************

//*****************************************************************************
// function prototypes
//*****************************************************************************

// -----------------------------------------------------------------------------
//! \brief      NPI Frame HCI UART handler for full header reception
//! \context    UART IRQ
//! \return     void
// -----------------------------------------------------------------------------
void NPIframeHCI_AutoUartFullHeaderIsrCB(void);

// -----------------------------------------------------------------------------
//! \brief      NPI Frame HCI UART handler for first byte only reception
//! \context    UART IRQ
//! \return     void
// -----------------------------------------------------------------------------
void NPIframeHCI_AutoUartFirstByteIsrCB(void);

// -----------------------------------------------------------------------------
//! \brief      NPI Frame HCI UART handler for partial header reception (missing first byte)
//! \context    UART IRQ
//! \return     void
// -----------------------------------------------------------------------------
void NPIframeHCI_AutoUartPartialHeaderIsrCB(void);

// -----------------------------------------------------------------------------
//! \brief      NPI Task function that sends one time message in order to host know when he can start send hci packets
//! \context    in order to host know when he can start send hci packets
//! \return     void
// -----------------------------------------------------------------------------
void NPITask_SendBleReadyMessagetoHost(void);

// -----------------------------------------------------------------------------
//! \brief      NPI Task UART RX Packet after wakeup set event
//! \context    UART IRQ
//! \return     void
// -----------------------------------------------------------------------------
void NPITask_UartRxPacketWakeupSetEvent();

// -----------------------------------------------------------------------------
//! \brief      NPI Task SDIO RX Packet notification set event
//! \context    SDIO IRQ
//! \return     void
// -----------------------------------------------------------------------------
void NPITask_SdioRxPacketNotifySetEvent();

#ifdef __cplusplus
}
#endif

#endif /* NPI_EXPORT_OSPREY_H */
