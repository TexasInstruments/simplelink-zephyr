/******************************************************************************

 @file  npi_frame_hci_osprey.c

 @brief This file contains the Network Processor Interface (NPI) data frame
        specific function implementations for the MT serial interface.

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
#include "npi_task.h"
#include "npi_ble.h"
#include "npi_frame.h"
#include "hci.h"
#include "hci_tl.h"
#include "osi.h"
#include "uart.h"
#include "sdio.h"
#include "npi_export_osprey.h"
#include "ble_autonomous_hif.h"

// ****************************************************************************
// defines
// ****************************************************************************


// ****************************************************************************
// typedefs
// ****************************************************************************


//*****************************************************************************
// globals
//*****************************************************************************
//NPI incoming frame callback
npiIncomingFrameCBack_t incomingFrameCBFunc = NULL;

//HCI serial collection globals
NPIFrameHCI_Params_t hciRxPkt;
uint16_t bytesToRead = NPIFRAMEHCI_CMD_PKT_HDR_LEN;

//RX Buffer Index
uint16_t RxBufIndex = 0;

//*****************************************************************************
// function prototypes
//*****************************************************************************
static void NPIframeHCI_ResetHCIRxPacket(void);
static void NPIframeHCI_UartThresholdIsrCB(void);
static void NPIframeHCI_UartReadIsrCB(void);
static void NPIframeHCI_SdioThresholdIsrCB(void);
static void NPIframeHCI_SdioDispachReadIsrCB(void);

//*****************************************************************************
// internal functions
//*****************************************************************************
// -----------------------------------------------------------------------------
//! \brief      NPI Frame HCI reset packet parameters
//! \context    UART IRQ
//! \return     void
// -----------------------------------------------------------------------------
static void NPIframeHCI_ResetHCIRxPacket(void)
{
    // Clear the RX HCI meta data
    hciRxPkt.state = NPIFRAMEHCI_STATE_PKT_TYPE;
    os_memset(&hciRxPkt.RxBuffer[0], 0x00, sizeof(NPIFRAMEHCI_RX_BUF_SIZE));
    RxBufIndex = 0;

    // Set the initial RX threshold and callback
    bytesToRead = NPIFRAMEHCI_CMD_PKT_HDR_LEN;
    if (Sdio_IsTrasportEnabled() == TRUE)
    {
        Sdio_SetThreshold(bytesToRead, NPIframeHCI_SdioThresholdIsrCB); // after reset set bytes to read to 4 (to read 4 bytes of header)
    }
    else
    {
        Uart_SetThreshold(bytesToRead, NPIframeHCI_UartThresholdIsrCB);
    }
}

// -----------------------------------------------------------------------------
//! \brief      NPI Frame HCI UART Write CB
//! \context    UART IRQ
//! \return     void
// -----------------------------------------------------------------------------
static void NPIframeHCI_UartThresholdIsrCB(void)
{
    GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG,"NPIframeHCI_UartThresholdIsrCB: Enter");

    //Set how much byte to read according to the packet state
    //Initially we wait for 4 bytes in order to read the packet type and length field from the HCI header
    //In case we identify the packet as extended or data we read additional byte to have the length
    //The length will later be used for the DMA read
    switch (hciRxPkt.state)
    {
        case NPIFRAMEHCI_STATE_PKT_TYPE:
            NPIframeHCI_ResetHCIRxPacket();
            break;

        case NPIFRAMEHCI_CMD_STATE_LENGTH1:
        case NPIFRAMEHCI_DATA_STATE_LENGTH1:
            bytesToRead = 1;
            break;

        default:
            //Invalid packet state
            GTRACE_NVIC(GRP_BLE_ERROR, "NPIframeHCI_UartThresholdIsrCB: Received invalid packet state");
            NPIframeHCI_ResetHCIRxPacket();
            break;
    }

    //Disable the UART threshold irq
    Uart_SetThreshold(0, NULL);

    //Read the bytes according to UART threshold
    Uart_ReadBytes(&hciRxPkt.RxBuffer[RxBufIndex], bytesToRead);

    //Call the packet/frame collector parser
    NPIFrame_uartCollectFrameData();
}

// -----------------------------------------------------------------------------
//! \brief      NPI Frame HCI UART Write CB
//! \context    UART DMA
//! \return     void
// -----------------------------------------------------------------------------
static void NPIframeHCI_UartReadIsrCB(void)
{
    GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG,"NPIframeHCI_UartReadIsrCB: Enter");

    switch (hciRxPkt.state)
    {
        case NPIFRAMEHCI_CMD_STATE_DATA:
        case NPIFRAMEHCI_DATA_STATE_DATA:
            incomingFrameCBFunc(&hciRxPkt);
            break;

        default:
            //Invalid packet state
            GTRACE_NVIC(GRP_BLE_ERROR, "NPIframeHCI_UartReadIsrCB: Received invalid packet state %d", hciRxPkt.state);
            NPIframeHCI_ResetHCIRxPacket();
            break;
    }
}

//*****************************************************************************
// function prototypes
//*****************************************************************************

// ----------------------------------------------------------------------------
//! \brief      Initialize Frame module with NPI callbacks.
//!
//! \param[in]  incomingFrameCB   Call back for complete inbound (from host)
//!                               messages
//!
//! \return     void
// ----------------------------------------------------------------------------
void NPIFrame_initialize(npiIncomingFrameCBack_t incomingFrameCB)
{
    if (incomingFrameCB)
    {
        incomingFrameCBFunc = incomingFrameCB;
        NPIframeHCI_ResetHCIRxPacket();
    }
    else
    {
        //Must have a callback
        GTRACE_NVIC(GRP_BLE_ERROR, "NPIFrame_initialize: No callback was set");
        ASSERT_GENERAL(0);
    }
}


// ----------------------------------------------------------------------------
//! \brief  HCI command packet format:
//!         Packet Type + Command opcode + length + command payload
//!         | 1 octet   |       2        |   1    |       n       |
//!
//!         HCI data packet format:
//!         Packet Type + Conn Handle+BC+PB + length + data payload
//!         | 1 octet   |       2           |   2    |     n      |
//!
//! \return     void
// ----------------------------------------------------------------------------
void NPIFrame_uartCollectFrameData(void)
{
    uint8_t ch;
    uint8_t i;
    OsiReturnVal_e uart_rc;

    //Disable the UART threshold irq
    Uart_SetThreshold(0, NULL);

    //run on all buffer
    for (i=0; i<bytesToRead; i++)
    {
        //Read single byte and increment the RX buffer index
        ch = hciRxPkt.RxBuffer[RxBufIndex];
        RxBufIndex++;

        switch (hciRxPkt.state)
        {
//*****************************************************************************
// HCI PACKET TYPE
//*****************************************************************************
            // Packet Type
            case NPIFRAMEHCI_STATE_PKT_TYPE:
                hciRxPkt.PKT_Token = ch;
                switch (ch)
                {
                    case HCI_CMD_PACKET:
                    case HCI_EXTENDED_CMD_PACKET:
                        hciRxPkt.state = NPIFRAMEHCI_CMD_STATE_OPCODE0;
                        break;
                    case HCI_ACL_DATA_PACKET:
                        hciRxPkt.state = NPIFRAMEHCI_DATA_STATE_HANDLE0;
                        break;
                    default:
                        GTRACE_NVIC(GRP_BLE_ERROR, "NPIFrame_uartCollectFrameData: Received unsupported packet type: %d", hciRxPkt.PKT_Token);
                        NPIframeHCI_ResetHCIRxPacket();
                }
                break;
//*****************************************************************************
// HCI COMMAND PACKET
//*****************************************************************************
            // Command Opcode Byte 0
            case NPIFRAMEHCI_CMD_STATE_OPCODE0:
                hciRxPkt.OPCODE_Token = ch;
                hciRxPkt.state = NPIFRAMEHCI_CMD_STATE_OPCODE1;
                break;

            // Command Opcode Byte 1
            case NPIFRAMEHCI_CMD_STATE_OPCODE1:
                hciRxPkt.OPCODE_Token |= ((uint16_t)ch << 8);
                if (hciRxPkt.PKT_Token == HCI_EXTENDED_CMD_PACKET)
                {
                    hciRxPkt.state = NPIFRAMEHCI_CMD_STATE_LENGTH0;
                }
                else
                {
                    hciRxPkt.state = NPIFRAMEHCI_CMD_STATE_LENGTH1;
                }
                break;

            // Extended Command Payload Length 0
            case NPIFRAMEHCI_CMD_STATE_LENGTH0:
                hciRxPkt.LEN_Token = ch;
                hciRxPkt.state = NPIFRAMEHCI_CMD_STATE_LENGTH1;

                //Need to read additional byte for the Extended Command Packet
                Uart_SetThreshold(1, NPIframeHCI_UartThresholdIsrCB);
                break;

            // Command Payload Length
            case NPIFRAMEHCI_CMD_STATE_LENGTH1:
                if (hciRxPkt.PKT_Token == HCI_EXTENDED_CMD_PACKET)
                {
                    hciRxPkt.HeaderLength = NPIFRAMEHCI_EXTENDED_CMD_PKT_HDR_LEN;
                    hciRxPkt.LEN_Token |= ((uint16_t)ch << 8);
                }
                else
                {
                    hciRxPkt.HeaderLength = NPIFRAMEHCI_CMD_PKT_HDR_LEN;
                    hciRxPkt.LEN_Token = (uint16_t)ch;
                }

                if (hciRxPkt.LEN_Token > NPIFRAMEHCI_RX_BUF_SIZE)
                {
                    // Data length to read is higher than the RX buffer supported length
                    GTRACE_NVIC(GRP_BLE_ERROR, "NPIFrame_uartCollectFrameData: Received size %d exceeds buffer size", hciRxPkt.LEN_Token);
                    NPIframeHCI_ResetHCIRxPacket();
                }
                else
                {
                    hciRxPkt.state = NPIFRAMEHCI_CMD_STATE_DATA;
                    if (hciRxPkt.LEN_Token == 0)
                    {
                        // No Payload to read so go ahead and send to Stack
                        incomingFrameCBFunc(&hciRxPkt);
                    }
                    else
                    {
                        // Read the Command Payload
                        uart_rc = Uart_ReadBytesDMA(&hciRxPkt.RxBuffer[RxBufIndex], hciRxPkt.LEN_Token, NPIframeHCI_UartReadIsrCB);
                        if (uart_rc != UART_HAL_SUCCESS)
                        {
                            GTRACE_NVIC(GRP_BLE_ERROR,"NPIFrame_uartCollectFrameData: Uart_ReadBytesDMA failed, uart_rc=%d", uart_rc);
                            ASSERT_GENERAL(0);
                        }
                    }
                }
                break;
//*****************************************************************************
// HCI DATA PACKET
//*****************************************************************************
            // Data Handle Byte 0
            case NPIFRAMEHCI_DATA_STATE_HANDLE0:
                hciRxPkt.HANDLE_Token = ch;
                hciRxPkt.state = NPIFRAMEHCI_DATA_STATE_HANDLE1;
                break;

            // Data Handle Byte 1
            case NPIFRAMEHCI_DATA_STATE_HANDLE1:
                hciRxPkt.HANDLE_Token |= ((uint16_t)ch << 8);
                hciRxPkt.state = NPIFRAMEHCI_DATA_STATE_LENGTH0;
                break;

            // Data Len Byte 0
            case NPIFRAMEHCI_DATA_STATE_LENGTH0:
                hciRxPkt.LEN_Token = ch;
                hciRxPkt.state = NPIFRAMEHCI_DATA_STATE_LENGTH1;

                //Need to read additional byte for the Data Packet
                Uart_SetThreshold(1, NPIframeHCI_UartThresholdIsrCB);
                break;

            // Data Len Byte 1
            case NPIFRAMEHCI_DATA_STATE_LENGTH1:
                hciRxPkt.LEN_Token |= ((uint16_t)ch << 8);
                hciRxPkt.HeaderLength = NPIFRAMEHCI_DATA_PKT_HDR_LEN;

                if (hciRxPkt.LEN_Token > NPIFRAMEHCI_RX_BUF_SIZE)
                {
                    // Data length to read is higher than the RX buffer supported length
                    GTRACE_NVIC(GRP_BLE_ERROR, "NPIFrame_uartCollectFrameData: Received size %d exceeds buffer size", hciRxPkt.LEN_Token);
                    NPIframeHCI_ResetHCIRxPacket();
                }
                else
                {
                    hciRxPkt.state = NPIFRAMEHCI_DATA_STATE_DATA;
                    if (hciRxPkt.LEN_Token == 0)
                    {
                        // No Payload to read so go ahead and send to Stack
                        incomingFrameCBFunc(&hciRxPkt);
                    }
                    else
                    {
                        // Read the data payload
                        uart_rc = Uart_ReadBytesDMA(&hciRxPkt.RxBuffer[RxBufIndex], hciRxPkt.LEN_Token, NPIframeHCI_UartReadIsrCB);
                        if (uart_rc != UART_HAL_SUCCESS)
                        {
                            GTRACE_NVIC(GRP_BLE_ERROR,"NPIFrame_uartCollectFrameData: Uart_ReadBytesDMA failed, uart_rc=%d", uart_rc);
                            ASSERT_GENERAL(0);
                        }
                    }
                }
                break;
            default:
                NPIframeHCI_ResetHCIRxPacket();
                hciRxPkt.state = NPIFRAMEHCI_STATE_PKT_TYPE;
                GTRACE_NVIC(GRP_BLE_ERROR, "NPIFrame_uartCollectFrameData: Received unsupported packet state: %d", hciRxPkt.state);
                break;
        }
    }
}

// -----------------------------------------------------------------------------
//! \brief      NPI Frame HCI SDIO Write CB
//! \context    SDIO IRQ
//! \return     void
// -----------------------------------------------------------------------------
static void NPIframeHCI_SdioThresholdIsrCB(void)
{
    // GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG,"NPIframeHCI_SdioThresholdIsrCB: Enter");

    // Disable the SDIO threshold irq
    Sdio_SetThreshold(0, NULL);

    // Call the packet/frame collector parser
    NPIFrame_sdioCollectFrameData();
}

// -----------------------------------------------------------------------------------------------------
//! \brief  NPIframeHCI_SdioDispachReadIsrCB- Handle HCI command packet format after SDIO length removed:
//! \context    SDIO DMA    //relevant  SDIO
//!         Packet Type + Command opcode + length + command payload
//!         | 1 octet   |      2         |   1   |      n        |
//!
//!         HCI data packet format:
//!         Packet Type +   Conn Handle  + length + data payload
//!         | 1 octet   |      2         |   2   |      n      |
//!
//! \return     void
// -----------------------------------------------------------------------------------------------------
static void NPIframeHCI_SdioDispachReadIsrCB(void)
{
    //: this is the function to be called when making Read from SDIO/UART using DMA
    uint8_t ch;
    uint32_t DuringDispachFlag;
    
    GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG, "NPIframeHCI_SdioDispachReadIsrCB: Enter");

    if (hciRxPkt.state == NPIFRAMEHCI_STATE_SDIO_HEADER_IDENIFIED)
    {
        GTRACE_NVIC(GRP_BLE_ERROR, " NPIframeHCI_SdioDispachReadIsrCB hciRxPkt.state  = NPIFRAMEHCI_STATE_SDIO_HEADER_IDENIFIED ");
        // GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG," NPIframeHCI_SdioDispachReadIsrCB state HEADER_IDENIFIED");
        if ((hciRxPkt.PKT_Token == HCI_CMD_PACKET) || (hciRxPkt.PKT_Token == HCI_EXTENDED_CMD_PACKET))
            hciRxPkt.state = NPIFRAMEHCI_CMD_STATE_OPCODE0;
        else if (hciRxPkt.PKT_Token == HCI_ACL_DATA_PACKET)
            hciRxPkt.state = NPIFRAMEHCI_DATA_STATE_HANDLE0;
        else // there are more states  -  check later if needed
        {
            hciRxPkt.state = NPIFRAMEHCI_STATE_PKT_TYPE;
            GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG, "call C Error  to NPIframeHCI_ResetHCIRxPacket ");
            NPIframeHCI_ResetHCIRxPacket(); // Sdio_SetThreshold(4, NPIframeHCI_SdioThresholdIsrCB);
        }

        // dont break;

        DuringDispachFlag = TRUE;
        GTRACE_NVIC(GRP_BLE_ERROR, "NPIframeHCI_SdioDispachReadIsrCB start dispatch");

        while (DuringDispachFlag)
        {
            switch (hciRxPkt.state)
            {

            // continue to parse packet
            // Command Opcode Byte 0
            case NPIFRAMEHCI_CMD_STATE_OPCODE0:
                // GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG, "NPIframeHCI_SdioDispachReadIsrCB state NPIFRAMEHCI_CMD_STATE_OPCODE0");
                ch = hciRxPkt.RxBuffer[RxBufIndex];
                hciRxPkt.OPCODE_Token = ch;
                hciRxPkt.state = NPIFRAMEHCI_CMD_STATE_OPCODE1;
                RxBufIndex++;
                break;

            // Command Opcode Byte 1
            case NPIFRAMEHCI_CMD_STATE_OPCODE1:
                // GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG, "NPIframeHCI_SdioDispachReadIsrCB state NPIFRAMEHCI_CMD_STATE_OPCODE1");
                ch = hciRxPkt.RxBuffer[RxBufIndex];
                hciRxPkt.OPCODE_Token |= ((uint16_t)ch << 8);
                RxBufIndex++;
                if (hciRxPkt.PKT_Token == HCI_EXTENDED_CMD_PACKET)
                {
                    hciRxPkt.state = NPIFRAMEHCI_CMD_STATE_LENGTH0;
                }
                else
                {
                    hciRxPkt.state = NPIFRAMEHCI_CMD_STATE_LENGTH1;
                }
                break;

            // Extended Command Payload Length 0
            case NPIFRAMEHCI_CMD_STATE_LENGTH0:

                //  GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG, "NPIframeHCI_SdioDispachReadIsrCB state NPIFRAMEHCI_CMD_STATE_LENGTH0");
                ch = hciRxPkt.RxBuffer[RxBufIndex];
                RxBufIndex++;
                hciRxPkt.LEN_Token = ch;
                hciRxPkt.state = NPIFRAMEHCI_CMD_STATE_LENGTH1;
                break;

            // Command Payload Length
            case NPIFRAMEHCI_CMD_STATE_LENGTH1:
            {
                //   GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG, "NPIframeHCI_SdioDispachReadIsrCB state NPIFRAMEHCI_CMD_STATE_LENGTH1");
                ch = hciRxPkt.RxBuffer[RxBufIndex];
                RxBufIndex++;

                if (hciRxPkt.PKT_Token == HCI_EXTENDED_CMD_PACKET)
                {
                    hciRxPkt.HeaderLength = NPIFRAMEHCI_EXTENDED_CMD_PKT_HDR_LEN;
                    hciRxPkt.LEN_Token |= ((uint16_t)ch << 8);
                }
                else
                {
                    hciRxPkt.HeaderLength = NPIFRAMEHCI_CMD_PKT_HDR_LEN;
                    hciRxPkt.LEN_Token = (uint16_t)ch;
                }

                // GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG, "NPIframeHCI_SdioDispachReadIsrCB state CMD_OPCODE = 0x%2x ,CMD_Length = 0x%2x",
                //             hciRxPkt.OPCODE_Token,hciRxPkt.LEN_Token);
                if (hciRxPkt.LEN_Token > NPIFRAMEHCI_RX_BUF_SIZE) /*|| (hciRxPkt.LEN_Token != sdio_buf_len-7)  )// if size does not match assert error*/
                {
                    GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG, "NPIframeHCI_SdioDispachReadIsrCB error LEN_Token > NPIFRAMEHCI_RX_BUF_SIZE ");
                    // Data length to read is higher than the RX buffer supported length
                    NPIframeHCI_ResetHCIRxPacket();
                    DuringDispachFlag = 0; // stop Parsing hci packet
                    ASSERT_GENERAL(0);     // TODO: Remove eventually
                }

                else
                {
                    hciRxPkt.state = NPIFRAMEHCI_CMD_STATE_DATA;
                    GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG, " NPIframeHCI_SdioDispachReadIsrCB CMD_STATE call incomingFrameCBFunc hciRxPkt.LEN_Token = %d ", hciRxPkt.LEN_Token);
                    incomingFrameCBFunc(&hciRxPkt);
                    DuringDispachFlag = 0; // stop Parsing hci packet
                    GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG, "added  26/10/2022 to reset state machine after packet handled ");
                    GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG, "call B to NPIframeHCI_ResetHCIRxPacket ");
                    // NPIframeHCI_ResetHCIRxPacket();//added  26/10/2022 to reset state machine after packet handled - removed elad  i will reset once after packethandle
                }
                break;
            }
            
            //*****************************************************************************
            // HCI DATA PACKET
            //*****************************************************************************
            // Data Handle Byte 0
            case NPIFRAMEHCI_DATA_STATE_HANDLE0:
                //  GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG," NPIframeHCI_SdioDispachReadIsrCB state NPIFRAMEHCI_DATA_STATE_HANDLE0");
                ch = hciRxPkt.RxBuffer[RxBufIndex];
                RxBufIndex++;
                hciRxPkt.HANDLE_Token = ch;
                hciRxPkt.state = NPIFRAMEHCI_DATA_STATE_HANDLE1;
                break;

            // Data Handle Byte 1
            case NPIFRAMEHCI_DATA_STATE_HANDLE1:
                //   GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG," NPIframeHCI_SdioDispachReadIsrCB state NPIFRAMEHCI_DATA_STATE_HANDLE1");
                ch = hciRxPkt.RxBuffer[RxBufIndex];
                RxBufIndex++;
                hciRxPkt.HANDLE_Token |= ((uint16_t)ch << 8);
                hciRxPkt.state = NPIFRAMEHCI_DATA_STATE_LENGTH0;
                break;
            // Data Len Byte 0
            case NPIFRAMEHCI_DATA_STATE_LENGTH0:
                //   GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG," NPIframeHCI_SdioDispachReadIsrCB state NPIFRAMEHCI_DATA_STATE_LENGTH0");
                ch = hciRxPkt.RxBuffer[RxBufIndex];
                RxBufIndex++;
                hciRxPkt.LEN_Token = ch;
                hciRxPkt.state = NPIFRAMEHCI_DATA_STATE_LENGTH1;
            // Data Len Byte 1
            case NPIFRAMEHCI_DATA_STATE_LENGTH1:
                //   GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG," NPIframeHCI_SdioDispachReadIsrCB state NPIFRAMEHCI_DATA_STATE_LENGTH1");
                ch = hciRxPkt.RxBuffer[RxBufIndex];
                RxBufIndex++;
                hciRxPkt.LEN_Token |= ((uint16_t)ch << 8);
                hciRxPkt.HeaderLength = NPIFRAMEHCI_DATA_PKT_HDR_LEN;

                if (hciRxPkt.LEN_Token > NPIFRAMEHCI_RX_BUF_SIZE) // if( (hciRxPkt.LEN_Token > NPIFRAMEHCI_RX_BUF_SIZE) || (hciRxPkt.LEN_Token != sdio_buf_len-8)  ) // if size does not match assert error
                {
                    // Data length to read is higher than the RX buffer supported length
                    NPIframeHCI_ResetHCIRxPacket();
                    DuringDispachFlag = 0; // stop Parsing hci packet
                    ASSERT_GENERAL(0);     // TODO: Remove eventually
                }
                else
                {
                    hciRxPkt.state = NPIFRAMEHCI_DATA_STATE_DATA;

                    GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG, "NPIframeHCI_SdioDispachReadIsrCB  DATA_STATE call incomingFrameCBFunc data len =%d ", hciRxPkt.LEN_Token);

                    incomingFrameCBFunc(&hciRxPkt); // in sdio dont need to make another dma read
                    DuringDispachFlag = 0;          // stop Parsing hci packet
                    GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG, "call A to NPIframeHCI_ResetHCIRxPacket ");
                    NPIframeHCI_ResetHCIRxPacket(); // added  26/10/2022 to reset state machine after packet handled
                }
                break;

            default:
                GTRACE_NVIC(GRP_BLE_ERROR, "Received unsupported packet type: %d", hciRxPkt.PKT_Token);
                NPIframeHCI_ResetHCIRxPacket();
                ASSERT_GENERAL(0); // TODO: Remove eventually
                break;
            }
        }
    }
}


// ----------------------------------------------------------------------------
//! \brief  SDIO+HCI  command packet format:
//!        Size +  Packet Type + Command opcode + length + command payload
//!        3 octet  | 1    |      2         |   1   |      n        |
//!
//!         SDIO+HCI HCI data packet format:
//!        Size +  Packet Type +   Conn Handle  + length + data payload
//!        3 octet  | 1    |      2         |   2   |      n      |
//!
//! \return     void
// ----------------------------------------------------------------------------
void NPIFrame_sdioCollectFrameData(void)
{
    uint32_t readBytes;
    OsiReturnVal_e sdio_rc;

    static uint32_t sdio_buf_len = 0;
    uint32_t read_size;
    uint32_t modulu;
    uint8_t sdio_type; // not really needed for parsing - might use for debug

    // Disable the SDIO threshold irq
    Sdio_SetThreshold(0, NULL);
    // GTRACE_NVIC(GRP_BLE_ERROR, "Entered NPIFrame_collectSDIOFrameData");

    switch (hciRxPkt.state)
    {
        //*****************************************************************************
        // HCI PACKET TYPE
        //*****************************************************************************
        // Packet Type
        case NPIFRAMEHCI_STATE_PKT_TYPE:
        {
            GTRACE_NVIC(GRP_BLE_ERROR, "NPIFrame_sdioCollectFrameData: first RxBufIndex  =%d", RxBufIndex);
            GTRACE_NVIC(GRP_BLE_ERROR, "NPIFrame_sdioCollectFrameData: Entered NPIFRAMEHCI_STATE_PKT_TYPE state");

            readBytes = Sdio_ReadBytes(&hciRxPkt.RxBuffer[RxBufIndex], 4);

            if (readBytes == 0)
            {
                GTRACE_NVIC(GRP_BLE_ERROR, " did not read any byte in Reset state ");

                hciRxPkt.state = NPIFRAMEHCI_STATE_PKT_TYPE;
                NPIframeHCI_ResetHCIRxPacket(); // Sdio_SetThreshold(4, NPIframeHCI_SdioThresholdIsrCB);
                break;
            }

            sdio_buf_len = hciRxPkt.RxBuffer[0];
            sdio_buf_len |= hciRxPkt.RxBuffer[1] << 8;
            sdio_buf_len |= hciRxPkt.RxBuffer[2] << 16;
            sdio_type = hciRxPkt.RxBuffer[3];

            if (sdio_buf_len > NPIFRAMEHCI_RX_BUF_SIZE)
            {
                GTRACE_NVIC(GRP_BLE_ERROR, "NPIFrame_sdioCollectFrameData: did not read any byte in Reset state");
                hciRxPkt.state = NPIFRAMEHCI_STATE_PKT_TYPE;
                NPIframeHCI_ResetHCIRxPacket(); // Sdio_SetThreshold(4, NPIframeHCI_SdioThresholdIsrCB);
                ASSERT_GENERAL(0);     // TODO: Remove eventually
                break;
            }

            //  GTRACE_NVIC(GRP_BLE_ERROR,"hciRxPkt.RxBuffer[0]= %x, hciRxPkt.RxBuffer[1]= %x, hciRxPkt.RxBuffer[2]= %x, hciRxPkt.RxBuffer[3]= %x",
            //              hciRxPkt.RxBuffer[0],hciRxPkt.RxBuffer[1],hciRxPkt.RxBuffer[2],hciRxPkt.RxBuffer[3]);
            //  GTRACE_NVIC(GRP_BLE_ERROR,"sdio_buf_len= %d,sdio_type= %d", sdio_buf_len, sdio_type);

            if ((sdio_type == HCI_CMD_PACKET) || (sdio_type == HCI_ACL_DATA_PACKET) || (sdio_type == HCI_SCO_DATA_PACKET) ||
                (sdio_type == HCI_EVENT_PACKET) || (sdio_type == HCI_EXTENDED_EVENT_PACKET) || (sdio_type == HCI_EXTENDED_CMD_PACKET))
            {
                hciRxPkt.state = NPIFRAMEHCI_STATE_SDIO_HEADER_IDENIFIED; // sdio_type;

                Sdio_SetThreshold(0, NULL); // disable isr until end of packet

                hciRxPkt.PKT_Token = sdio_type;
                hciRxPkt.RxBuffer[0] = hciRxPkt.PKT_Token;
                RxBufIndex = 1;
                GTRACE_NVIC(GRP_BLE_ERROR, "NPIFrame_sdioCollectFrameData: sdio_buf_len=%d, sdio_type=%d", sdio_buf_len, sdio_type);
                #ifdef SDIO_BUFFER_ALIGNED //aligned to 4 bytes
                    modulu = sdio_buf_len % 4;
                    if (modulu == 0)
                        read_size = sdio_buf_len - 4;
                    else
                        read_size = sdio_buf_len - modulu;
                #else//unaligned
                    read_size = sdio_buf_len - 4; //minus sdio header has been read before
                #endif
                GTRACE_NVIC(GRP_BLE_ERROR, "NPIFrame_sdioCollectFrameData: call Sdio_ReadBytesDMA  read_size=%d", read_size);
                autohif_sdioRxPacketStartNotify();
                sdio_rc = Sdio_ReadBytesDMA(&hciRxPkt.RxBuffer[RxBufIndex], read_size, NPIframeHCI_SdioDispachReadIsrCB);
                if (sdio_rc != SDIO_HAL_SUCCESS)
                {
                    GTRACE_NVIC(GRP_BLE_ERROR, "NPIFrame_sdioCollectFrameData: Sdio_ReadBytesDMA failed, sdio_rc=%d", sdio_rc);
                    ASSERT_GENERAL(0);
                }
            }
            else
            {
                GTRACE_NVIC(GRP_BLE_ERROR, "Reset State");
                hciRxPkt.state = NPIFRAMEHCI_STATE_PKT_TYPE;
                NPIframeHCI_ResetHCIRxPacket();
            }

            break;
        }

        default:
            GTRACE_NVIC(GRP_BLE_ERROR, "NPIFrame_sdioCollectFrameData: Received unsupported state  type: %d", hciRxPkt.state);
            ASSERT_GENERAL(0); // TODO: Remove eventually
            break;
        }
        // GTRACE_NVIC(GRP_BLE_ERROR, " Exit NPIFrame_collectSDIOFrameData state");
}

//*****************************************************************************
// exported function
//*****************************************************************************

// -----------------------------------------------------------------------------
//! \brief      NPI Frame HCI UART handler for full header reception
//! \context    UART IRQ
//! \return     void
// -----------------------------------------------------------------------------
void NPIframeHCI_AutoUartFullHeaderIsrCB(void)
{
    GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG,"NPIframeHCI_AutoUartFullHeaderIsrCB: Enter");

    //Call directly to full header CB
    NPIframeHCI_UartThresholdIsrCB();
}

// -----------------------------------------------------------------------------
//! \brief      NPI Frame HCI UART handler for first byte only reception
//! \context    UART IRQ
//! \return     void
// -----------------------------------------------------------------------------
void NPIframeHCI_AutoUartFirstByteIsrCB(void)
{
    GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG,"NPIframeHCI_AutoUartFirstByteIsrCB: Enter");

    //Signal RX started event to autonomous UART
    autohif_RxStarted_Event();

    //We received the first byte properly as we didn't go to sleep yet
    //Set the UART threshold irq to receive the interrupt for the total of 4 bytes (1 we got earlier + 3 remaining bytes)
    Uart_SetThreshold(NPIFRAMEHCI_CMD_PKT_HDR_LEN, NPIframeHCI_UartThresholdIsrCB);
}

// -----------------------------------------------------------------------------
//! \brief      NPI Frame HCI UART handler for partial header reception (missing first byte)
//! \context    UART IRQ
//! \return     void
// -----------------------------------------------------------------------------
void NPIframeHCI_AutoUartPartialHeaderIsrCB(void)
{
    uint16_t nextBytes;
    uint8_t restoredByte;

    GTRACE_NVIC(GRP_BLE_NPI_RX_PKT_DBG,"NPIframeHCI_AutoUartPartialHeaderIsrCB: Enter");

    //Reset the HCI RX Packet
    NPIframeHCI_ResetHCIRxPacket();

    //Disable the UART threshold irq
    Uart_SetThreshold(0, NULL);

    //We lost the first byte as we just woke up from sleep
    //Read the remaining 3 bytes to their corresponding place in the internal buffer
    Uart_ReadBytes(&hciRxPkt.RxBuffer[1], (bytesToRead-1));

    GTRACE_NVIC(GRP_BLE_AUTO_HIF_DBG,"NPIframeHCI_AutoUartPartialHeaderIsrCB: UART buffer read 0x%x 0x%x 0x%x 0x%x",
                hciRxPkt.RxBuffer[0],
                hciRxPkt.RxBuffer[1],
                hciRxPkt.RxBuffer[2],
                hciRxPkt.RxBuffer[3]);

    //Restore the missing first byte (based on the 2nd and 3rd bytes of the packet header) and set it to its corresponding place in the internal buffer
    nextBytes = hciRxPkt.RxBuffer[1];
    nextBytes |= ((uint16_t)hciRxPkt.RxBuffer[2] << 8);
    restoredByte = autohif_uartRestoreFisrtByte(nextBytes);
    hciRxPkt.RxBuffer[0] = restoredByte;

    //Call the packet/frame collector parser
    NPIFrame_uartCollectFrameData();
}
